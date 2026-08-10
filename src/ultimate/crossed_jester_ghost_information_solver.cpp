/*
  Ultimate Fish - exact crossed Jester/Ghost information solver core
  Copyright (C) 2026 Ultimate Fish contributors
  GPLv3 or later.
*/

#include "crossed_jester_ghost_information_solver.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <istream>
#include <limits>
#include <ostream>
#include <stdexcept>

namespace Stockfish::Ultimate::CrossedJesterGhostSolver {
namespace Model = CrossedJesterGhostInformation;

namespace {

constexpr std::array<char, 12> GraphMagic{{
  'U', 'F', 'C', 'R', 'O', 'S', 'S', 'G', 'R', 'F', '1', '\0'}};

void write_u32(std::ostream& output, std::uint32_t value) {
    std::array<char, 4> bytes{};
    for (unsigned index = 0; index < bytes.size(); ++index)
        bytes[index] = static_cast<char>(value >> (8 * index));
    output.write(bytes.data(), bytes.size());
    if (!output)
        throw std::runtime_error("cannot write crossed graph archive");
}

std::uint32_t read_u32(std::istream& input) {
    std::array<std::uint8_t, 4> bytes{};
    input.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    if (!input)
        throw std::runtime_error("truncated crossed graph word");
    std::uint32_t value = 0;
    for (unsigned index = 0; index < bytes.size(); ++index)
        value |= static_cast<std::uint32_t>(bytes[index]) << (8 * index);
    return value;
}

}  // namespace

std::vector<std::uint8_t> Arena::key(const Model::KnowledgeState& state) {
    return Model::serialize_state(Model::canonicalize_state(state).value);
}

NodeId Arena::intern(const Model::KnowledgeState& state) {
    Model::KnowledgeState canonical = Model::canonicalize_state(state).value;
    std::vector<std::uint8_t> encoded = Model::serialize_state(canonical);
    if (!(Model::deserialize_state(encoded) == canonical))
        throw std::runtime_error(
          "crossed solver state-key roundtrip residual");
    const auto found = interner_.find(encoded);
    if (found != interner_.end())
        return found->second;
    if (nodes_.size() >= std::numeric_limits<NodeId>::max())
        throw std::overflow_error("crossed solver node IDs exceed uint32");
    const NodeId id = static_cast<NodeId>(nodes_.size());
    nodes_.push_back(std::move(canonical));
    interner_.emplace(std::move(encoded), id);
    return id;
}

std::optional<NodeId> Arena::find(const Model::KnowledgeState& state) const {
    const auto found = interner_.find(key(state));
    return found == interner_.end() ? std::nullopt
                                    : std::optional<NodeId>(found->second);
}

NodeExpansion Arena::regenerate(NodeId id, bool allowNew) {
    if (id >= nodes_.size())
        throw std::out_of_range("crossed solver node is outside the arena");
    // Interning children may reallocate nodes_; keep the source independent.
    const Model::KnowledgeState source = nodes_[id];
    NodeExpansion result;
    result.transitions = Model::enumerate_complete_transitions(source);
    result.sameClassChildren.resize(result.transitions.buckets.size());
    result.certificate.node = id;
    result.certificate.actions = result.transitions.actions.size();
    result.certificate.observations = result.transitions.buckets.size();
    for (const Model::CellActionOutcomes& action : result.transitions.actions)
        result.certificate.outcomes += action.outcomes.size();

    for (std::size_t bucketIndex = 0;
         bucketIndex < result.transitions.buckets.size(); ++bucketIndex) {
        const Model::SuccessorBucket& bucket =
          result.transitions.buckets[bucketIndex];
        if (bucket.domain != Model::ChildDomain::SameClass) {
            ++result.certificate.externalObservations;
            continue;
        }
        ++result.certificate.sameClassObservations;
        if (!bucket.sameClass)
            throw std::runtime_error(
              "crossed solver same-class bucket has no child state");
        const std::optional<NodeId> existing = find(*bucket.sameClass);
        if (existing)
            result.sameClassChildren[bucketIndex] = *existing;
        else if (allowNew) {
            result.sameClassChildren[bucketIndex] = intern(*bucket.sameClass);
            ++result.certificate.newlyInterned;
        }
        else
            throw std::runtime_error(
              "crossed solver closed regeneration found a new child");
    }
    const std::vector<std::uint8_t> encoded =
      Model::serialize_state(source);
    result.certificate.keyRoundtripResidual =
      Model::deserialize_state(encoded) == source ? 0 : 1;
    if (result.certificate.keyRoundtripResidual)
        throw std::runtime_error("crossed solver key residual");
    return result;
}

TargetBellmanPlan Arena::bellman_plan(NodeId id, Color target,
                                      bool allowNew) {
    const NodeExpansion expansion = regenerate(id, allowNew);
    const Model::KnowledgeState& decision = expansion.transitions.decisionState;
    TargetBellmanPlan plan;
    plan.target = target;
    plan.mover = decision.frame.side;
    plan.atoms.resize(decision.atoms.size());
    plan.certificate.atoms = decision.atoms.size();

    const auto child_reference = [&](const Model::AtomOutcome& outcome) {
        if (outcome.bucket >= expansion.transitions.buckets.size())
            throw std::runtime_error(
              "crossed Bellman outcome bucket is out of range");
        const Model::SuccessorBucket& bucket =
          expansion.transitions.buckets[outcome.bucket];
        if (outcome.childAtom >= bucket.atoms.size() ||
            bucket.atoms[outcome.childAtom].sourceAtom != outcome.sourceAtom ||
            bucket.atoms[outcome.childAtom].child.domain !=
              outcome.child.domain ||
            bucket.atoms[outcome.childAtom].child.index != outcome.child.index ||
            bucket.atoms[outcome.childAtom].child.winner != outcome.child.winner)
            throw std::runtime_error(
              "crossed Bellman outcome/bucket mapping has a residual");
        ChildReference reference;
        reference.domain = bucket.domain;
        reference.bucket = outcome.bucket;
        reference.childAtom = outcome.childAtom;
        reference.child = outcome.child;
        if (bucket.domain == Model::ChildDomain::SameClass) {
            if (outcome.bucket >= expansion.sameClassChildren.size() ||
                !expansion.sameClassChildren[outcome.bucket])
                throw std::runtime_error(
                  "crossed Bellman internal child is not interned");
            reference.node = expansion.sameClassChildren[outcome.bucket];
            ++plan.certificate.internalReferences;
        }
        else {
            if (bucket.domain == Model::ChildDomain::Invalid)
                throw std::runtime_error(
                  "crossed Bellman child escaped the certified domains");
            ++plan.certificate.externalReferences;
        }
        ++plan.certificate.childReferences;
        return reference;
    };

    for (std::uint32_t atom = 0; atom < plan.atoms.size(); ++atom) {
        plan.atoms[atom].atom = atom;
        plan.atoms[atom].kind = plan.mover == target
                              ? EquationKind::Or : EquationKind::And;
    }

    if (plan.mover == target) {
        plan.gates.reserve(expansion.transitions.actions.size());
        for (std::uint32_t actionIndex = 0;
             actionIndex < expansion.transitions.actions.size();
             ++actionIndex) {
            const Model::CellActionOutcomes& action =
              expansion.transitions.actions[actionIndex];
            ActionGatePlan gate;
            gate.action = actionIndex;
            gate.moverCell = action.choice.cell;
            gate.children.reserve(action.outcomes.size());
            for (const Model::AtomOutcome& outcome : action.outcomes)
                gate.children.push_back(child_reference(outcome));
            plan.gates.push_back(std::move(gate));
        }
        plan.certificate.gates = plan.gates.size();

        const Model::KnowledgePartition& cells = Model::partition_for(
          decision, target);
        for (const ActionGatePlan& gate : plan.gates) {
            if (gate.moverCell >= cells.cells.size())
                ++plan.certificate.sourceCoverageResidual;
            else {
                std::vector<std::uint32_t> sources;
                sources.reserve(gate.children.size());
                for (const ChildReference& child : gate.children)
                    sources.push_back(
                      expansion.transitions.buckets[child.bucket]
                        .atoms[child.childAtom].sourceAtom);
                std::sort(sources.begin(), sources.end());
                if (sources != cells.cells[gate.moverCell])
                    ++plan.certificate.sourceCoverageResidual;
            }
        }
        for (std::uint32_t cell = 0; cell < cells.cells.size(); ++cell) {
            std::vector<std::uint32_t> choices;
            for (const ActionGatePlan& gate : plan.gates)
                if (gate.moverCell == cell)
                    choices.push_back(gate.action);
            for (const std::uint32_t atom : cells.cells[cell]) {
                if (atom >= plan.atoms.size()) {
                    ++plan.certificate.sourceCoverageResidual;
                    continue;
                }
                plan.atoms[atom].gates = choices;
            }
        }
        for (const Model::KnowledgeCell& cell : cells.cells) {
            if (cell.empty()) {
                ++plan.certificate.sourceCoverageResidual;
                continue;
            }
            for (const std::uint32_t atom : cell)
                if (plan.atoms[atom].gates !=
                    plan.atoms[cell.front()].gates)
                    ++plan.certificate.cellUniformityResidual;
        }
    }
    else {
        const Model::KnowledgePartition& targetCells = Model::partition_for(
          decision, target);
        for (const Model::CellActionOutcomes& action :
             expansion.transitions.actions) {
            for (const Model::AtomOutcome& outcome : action.outcomes) {
                const std::uint32_t cell = Model::cell_for_atom(
                  decision, target, outcome.sourceAtom);
                if (cell >= targetCells.cells.size()) {
                    ++plan.certificate.sourceCoverageResidual;
                    continue;
                }
                for (const std::uint32_t atom : targetCells.cells[cell])
                    plan.atoms[atom].children.push_back(
                      child_reference(outcome));
            }
        }
        for (const Model::KnowledgeCell& cell : targetCells.cells) {
            if (cell.empty()) {
                ++plan.certificate.sourceCoverageResidual;
                continue;
            }
            for (const std::uint32_t atom : cell)
                if (plan.atoms[atom].children !=
                    plan.atoms[cell.front()].children)
                    ++plan.certificate.cellUniformityResidual;
        }
    }

    if (plan.certificate.sourceCoverageResidual ||
        plan.certificate.cellUniformityResidual)
        throw std::runtime_error("crossed Bellman plan has a residual");
    return plan;
}

GraphArchiveCertificate Arena::write(std::ostream& output) const {
    if (nodes_.size() >= std::numeric_limits<NodeId>::max())
        throw std::overflow_error(
          "crossed graph archive exceeds the uint32 token domain");
    output.write(GraphMagic.data(), GraphMagic.size());
    write_u32(output, 1);
    write_u32(output, static_cast<std::uint32_t>(nodes_.size()));
    GraphArchiveCertificate certificate;
    certificate.nodes = nodes_.size();
    for (const Model::KnowledgeState& state : nodes_) {
        const std::vector<std::uint8_t> encoded = Model::serialize_state(state);
        if (encoded.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::overflow_error("crossed graph state key exceeds uint32");
        write_u32(output, static_cast<std::uint32_t>(encoded.size()));
        output.write(reinterpret_cast<const char*>(encoded.data()),
                     static_cast<std::streamsize>(encoded.size()));
        if (!output)
            throw std::runtime_error("cannot write crossed graph state key");
        certificate.payloadBytes += 4 + encoded.size();
        certificate.keyRoundtripResidual +=
          Model::deserialize_state(encoded) == state ? 0 : 1;
        certificate.canonicalResidual +=
          Model::canonicalize_state(state).value == state ? 0 : 1;
    }
    if (certificate.keyRoundtripResidual || certificate.canonicalResidual)
        throw std::runtime_error("crossed graph write certificate has a residual");
    return certificate;
}

std::pair<Arena, GraphArchiveCertificate> Arena::read(std::istream& input) {
    std::array<char, GraphMagic.size()> magic{};
    input.read(magic.data(), magic.size());
    if (!input || magic != GraphMagic || read_u32(input) != 1)
        throw std::runtime_error("invalid crossed graph archive header");
    const std::uint32_t count = read_u32(input);
    Arena arena;
    GraphArchiveCertificate certificate;
    certificate.nodes = count;
    for (std::uint32_t expected = 0; expected < count; ++expected) {
        const std::uint32_t size = read_u32(input);
        if (!size)
            throw std::runtime_error("crossed graph archive has an empty key");
        std::vector<std::uint8_t> encoded(size);
        input.read(reinterpret_cast<char*>(encoded.data()),
                   static_cast<std::streamsize>(encoded.size()));
        if (!input)
            throw std::runtime_error("truncated crossed graph state key");
        certificate.payloadBytes += 4 + encoded.size();
        const Model::KnowledgeState state = Model::deserialize_state(encoded);
        certificate.keyRoundtripResidual +=
          Model::serialize_state(state) == encoded ? 0 : 1;
        certificate.canonicalResidual +=
          Model::canonicalize_state(state).value == state ? 0 : 1;
        const NodeId restored = arena.intern(state);
        if (restored != expected)
            ++certificate.duplicateResidual;
    }
    if (input.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("crossed graph archive has trailing bytes");
    if (certificate.keyRoundtripResidual || certificate.canonicalResidual ||
        certificate.duplicateResidual || arena.size() != count)
        throw std::runtime_error("crossed graph restore certificate has a residual");
    return {std::move(arena), certificate};
}

ExternalForceQuery external_force_query(const NodeExpansion& expansion,
                                         Color target,
                                         const ChildReference& reference) {
    if (reference.domain == Model::ChildDomain::SameClass || reference.node ||
        reference.domain == Model::ChildDomain::Invalid)
        throw std::invalid_argument(
          "crossed external query requires an unresolved certified child");
    if (reference.bucket >= expansion.transitions.buckets.size())
        throw std::out_of_range(
          "crossed external query bucket is outside the expansion");
    const Model::SuccessorBucket& bucket =
      expansion.transitions.buckets[reference.bucket];
    if (bucket.domain != reference.domain ||
        reference.childAtom >= bucket.atoms.size() ||
        bucket.atoms[reference.childAtom].child.domain != reference.domain ||
        bucket.atoms[reference.childAtom].child.index != reference.child.index ||
        bucket.atoms[reference.childAtom].child.winner !=
          reference.child.winner)
        throw std::runtime_error(
          "crossed external query reference has a residual");

    const Model::KnowledgePartition& partition = target == Color::White
                                               ? bucket.white : bucket.black;
    const Model::KnowledgeCell* knowledge = nullptr;
    for (const Model::KnowledgeCell& cell : partition.cells)
        if (std::binary_search(cell.begin(), cell.end(), reference.childAtom)) {
            if (knowledge)
                throw std::runtime_error(
                  "crossed external atom appears in multiple knowledge cells");
            knowledge = &cell;
        }
    if (!knowledge || knowledge->empty())
        throw std::runtime_error(
          "crossed external atom is absent from the target knowledge partition");

    ExternalForceQuery query;
    query.target = target;
    query.domain = reference.domain;
    query.bucket = reference.bucket;
    query.childAtom = reference.childAtom;
    query.actual = reference.child;
    query.belief.reserve(knowledge->size());
    for (const std::uint32_t childAtom : *knowledge) {
        if (childAtom >= bucket.atoms.size() ||
            bucket.atoms[childAtom].child.domain != reference.domain)
            throw std::runtime_error(
              "crossed external knowledge cell mixes child domains");
        query.belief.push_back(bucket.atoms[childAtom].child);
    }
    if (std::none_of(query.belief.begin(), query.belief.end(),
          [&](const Model::ClassifiedChild& child) {
              return child.domain == query.actual.domain &&
                     child.index == query.actual.index &&
                     child.winner == query.actual.winner;
          }))
        throw std::runtime_error(
          "crossed external actual is absent from its inherited belief");
    return query;
}

bool exact_terminal_force(const ExternalForceQuery& query) {
    if (query.domain != Model::ChildDomain::ExactTerminal ||
        query.actual.domain != Model::ChildDomain::ExactTerminal ||
        query.belief.empty())
        throw std::invalid_argument(
          "crossed terminal resolver requires a nonempty terminal query");
    const std::optional<Color> winner = query.belief.front().winner;
    for (const Model::ClassifiedChild& child : query.belief)
        if (child.domain != Model::ChildDomain::ExactTerminal ||
            child.winner != winner)
            throw std::runtime_error(
              "crossed terminal knowledge cell mixes public outcomes");
    if (query.actual.winner != winner)
        throw std::runtime_error(
          "crossed terminal actual disagrees with its public outcome");
    return winner && *winner == query.target;
}

LowerJesterForceQuery lower_jester_force_query(
  const ExternalForceQuery& query) {
    if (query.domain != Model::ChildDomain::LowerJester ||
        query.actual.domain != Model::ChildDomain::LowerJester ||
        query.belief.empty())
        throw std::invalid_argument(
          "crossed lower-Jester adapter requires a nonempty Jester query");
    LowerJesterForceQuery result;
    result.target = query.target;
    result.targetOwnsJester = query.target == Color::White;
    result.actual = query.actual.index;
    result.belief = Model::inherited_lower_jester_set(query.belief);
    bool actualPresent = false;
    for (std::uint8_t index = 0; index < result.belief.cardinality; ++index)
        actualPresent |= result.belief.concrete[index] == result.actual;
    if (!actualPresent)
        throw std::runtime_error(
          "crossed lower-Jester actual is absent from projected belief");
    return result;
}

LowerGhostForceQuery lower_ghost_force_query(
  const ExternalForceQuery& query) {
    if (query.domain != Model::ChildDomain::LowerGhost ||
        query.actual.domain != Model::ChildDomain::LowerGhost ||
        query.belief.empty())
        throw std::invalid_argument(
          "crossed lower-Ghost adapter requires a nonempty Ghost query");
    LowerGhostForceQuery result;
    result.target = query.target;
    // In crossed K+Jester-v-K+Ghost, Black is always the Ghost owner and
    // White the observer, even though the lower UFGM normalizes those roles.
    result.targetRole = query.target == Color::Black
                      ? Model::Role::GhostOwner : Model::Role::Observer;
    result.actual = Model::decode_lower_ghost(query.actual.index);
    result.belief = Model::inherited_lower_ghost_image(query.belief);
    if (result.actual.side != result.belief.side ||
        result.actual.ownerKing != result.belief.ownerKing ||
        result.actual.observerKing != result.belief.observerKing ||
        result.actual.visible != result.belief.visible ||
        !result.belief.locations.test(result.actual.ghost))
        throw std::runtime_error(
          "crossed lower-Ghost actual is absent from projected belief");
    return result;
}

bool resolve_external_force(const ExternalForceQuery& query,
                            const LowerForceOracle& oracle) {
    switch (query.domain) {
      case Model::ChildDomain::LowerJester:
        return oracle.force(lower_jester_force_query(query));
      case Model::ChildDomain::LowerGhost:
        return oracle.force(lower_ghost_force_query(query));
      case Model::ChildDomain::ExactTerminal:
        return exact_terminal_force(query);
      case Model::ChildDomain::SameClass:
      case Model::ChildDomain::Invalid:
        throw std::invalid_argument(
          "crossed external resolver received a non-external domain");
    }
    throw std::invalid_argument("unknown crossed external child domain");
}

const Model::KnowledgeState& Arena::node(NodeId id) const {
    if (id >= nodes_.size())
        throw std::out_of_range("crossed solver node is outside the arena");
    return nodes_[id];
}

std::size_t Arena::size() const {
    return nodes_.size();
}

}  // namespace Stockfish::Ultimate::CrossedJesterGhostSolver
