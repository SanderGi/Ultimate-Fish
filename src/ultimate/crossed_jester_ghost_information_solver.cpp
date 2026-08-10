/*
  Ultimate Fish - exact crossed Jester/Ghost information solver core
  Copyright (C) 2026 Ultimate Fish contributors
  GPLv3 or later.
*/

#include "crossed_jester_ghost_information_solver.h"

#include <limits>
#include <stdexcept>

namespace Stockfish::Ultimate::CrossedJesterGhostSolver {
namespace Model = CrossedJesterGhostInformation;

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

const Model::KnowledgeState& Arena::node(NodeId id) const {
    if (id >= nodes_.size())
        throw std::out_of_range("crossed solver node is outside the arena");
    return nodes_[id];
}

std::size_t Arena::size() const {
    return nodes_.size();
}

}  // namespace Stockfish::Ultimate::CrossedJesterGhostSolver
