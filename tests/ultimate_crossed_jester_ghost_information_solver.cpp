/* Exact crossed Jester/Ghost solver-core regression. GPLv3+. */

#include "crossed_jester_ghost_information_solver.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

namespace Stockfish::Ultimate {
namespace {

namespace Model = CrossedJesterGhostInformation;
namespace Solver = CrossedJesterGhostSolver;

[[nodiscard]] std::uint8_t square(const char* name) {
    const int result = Position::square_from_name(name);
    if (result == Position::NoSquare)
        throw std::runtime_error("invalid crossed solver test square");
    return static_cast<std::uint8_t>(result);
}

void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

Model::KnowledgeState fixture() {
    Model::KnowledgeState state;
    state.frame = {Color::Black, square("h10"), square("a1"),
                   square("c1"), {}};
    state.atoms = {{{true, square("a5")}},
                   {{false, square("a5")}},
                   {{true, square("c5")}},
                   {{false, square("c5")}}};
    for (const Model::HistoryAtom& atom : state.atoms)
        state.worlds.set(Model::world_variable(state.frame, atom.world));
    state.white.cells = {{0, 2}, {1, 3}};
    state.black.cells = {{0, 1}, {2, 3}};
    Model::validate_knowledge_state(state);
    return state;
}

Model::KnowledgeState external_fixture() {
    Model::KnowledgeState state;
    state.frame = {Color::Black, square("e5"), square("a1"),
                   square("e4"), {}};
    const std::array<const char*, 8> ghosts{
      "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8"};
    for (const char* name : ghosts) {
        state.atoms.push_back({{true, square(name)}});
        state.atoms.push_back({{false, square(name)}});
    }
    for (const Model::HistoryAtom& atom : state.atoms)
        state.worlds.set(Model::world_variable(state.frame, atom.world));
    Model::KnowledgeCell first;
    Model::KnowledgeCell second;
    for (std::uint32_t atom = 0; atom < state.atoms.size(); ++atom)
        (atom % 2 ? second : first).push_back(atom);
    state.white.cells = {first, second};
    for (std::uint32_t atom = 0; atom < state.atoms.size(); atom += 2)
        state.black.cells.push_back({atom, atom + 1});
    Model::validate_knowledge_state(state);
    return state;
}

void solver_arena_test() {
    Solver::Arena arena;
    const Model::KnowledgeState root = fixture();
    const Solver::NodeId rootId = arena.intern(root);
    require(rootId == 0 && arena.intern(root) == rootId && arena.size() == 1,
            "crossed solver interner is not collision-free/idempotent");
    const Model::KnowledgeState reflected = Model::transform_state(
      root, Model::RectangleTransform::Both);
    require(arena.intern(reflected) == rootId,
            "crossed solver did not D2-canonicalize a root");

    const Solver::NodeExpansion first = arena.regenerate(rootId, true);
    require(first.certificate.actions == 19 &&
             first.certificate.observations == 4 &&
             first.certificate.outcomes == 38 &&
             first.certificate.sameClassObservations > 0 &&
             first.certificate.newlyInterned > 0 &&
             first.certificate.keyRoundtripResidual == 0,
            "crossed solver first expansion certificate has a residual");
    const std::size_t discovered = arena.size();
    const Solver::NodeExpansion replay = arena.regenerate(rootId, false);
    require(arena.size() == discovered &&
             replay.certificate.actions == first.certificate.actions &&
             replay.certificate.observations ==
               first.certificate.observations &&
             replay.certificate.outcomes == first.certificate.outcomes &&
             replay.certificate.newlyInterned == 0 &&
             replay.sameClassChildren == first.sameClassChildren,
            "crossed solver closed replay differs from discovery");
    for (std::size_t bucket = 0;
         bucket < first.transitions.buckets.size(); ++bucket) {
        const auto& transition = first.transitions.buckets[bucket];
        require((transition.domain == Model::ChildDomain::SameClass) ==
                  first.sameClassChildren[bucket].has_value(),
                "crossed solver bucket/child domain mapping is incomplete");
    }
    const Solver::TargetBellmanPlan black = arena.bellman_plan(
      rootId, Color::Black, false);
    require(black.mover == Color::Black &&
             black.certificate.atoms == root.atoms.size() &&
             black.certificate.gates == first.certificate.actions &&
             black.certificate.childReferences ==
               first.certificate.outcomes &&
             black.certificate.internalReferences +
                 black.certificate.externalReferences ==
               black.certificate.childReferences &&
             black.certificate.sourceCoverageResidual == 0 &&
             black.certificate.cellUniformityResidual == 0,
            "crossed informed-mover Bellman plan has a residual");
    for (const Solver::AtomEquationPlan& equation : black.atoms)
        require(equation.kind == Solver::EquationKind::Or &&
                  !equation.gates.empty() && equation.children.empty(),
                "crossed informed atom is not an OR of action gates");
    for (const Solver::ActionGatePlan& gate : black.gates)
        require(!gate.children.empty(),
                "crossed informed action gate lacks cell outcomes");

    const Solver::TargetBellmanPlan white = arena.bellman_plan(
      rootId, Color::White, false);
    require(white.mover == Color::Black && white.gates.empty() &&
             white.certificate.atoms == root.atoms.size() &&
             white.certificate.gates == 0 &&
             white.certificate.childReferences ==
               2 * first.certificate.outcomes &&
             white.certificate.internalReferences +
                 white.certificate.externalReferences ==
               white.certificate.childReferences &&
             white.certificate.sourceCoverageResidual == 0 &&
             white.certificate.cellUniformityResidual == 0,
            "crossed uninformed-target Bellman plan has a residual");
    for (const Solver::AtomEquationPlan& equation : white.atoms)
        require(equation.kind == Solver::EquationKind::And &&
                  equation.gates.empty() && !equation.children.empty(),
                "crossed opponent atom is not an AND of compatible outcomes");
    std::cout << "crossed_solver_arena nodes " << arena.size()
              << " actions " << first.certificate.actions
              << " observations " << first.certificate.observations
              << " outcomes " << first.certificate.outcomes
              << " black_gates " << black.certificate.gates
              << " white_children " << white.certificate.childReferences
              << " replay_new_nodes 0 bellman_residual 0\n";

    Solver::Arena externalArena;
    const Solver::NodeId externalId = externalArena.intern(
      external_fixture());
    const Solver::NodeExpansion externalExpansion = externalArena.regenerate(
      externalId, true);
    const Solver::TargetBellmanPlan external = externalArena.bellman_plan(
      externalId, Color::Black, false);
    require(external.certificate.externalReferences > 0 &&
             external.certificate.internalReferences > 0,
            "crossed external fixture did not retain mixed child domains");
    std::uint64_t lowerGhost = 0;
    std::uint64_t exactTerminal = 0;
    std::uint64_t blackTerminalForces = 0;
    for (const Solver::ActionGatePlan& gate : external.gates)
        for (const Solver::ChildReference& child : gate.children) {
            if (child.domain == Model::ChildDomain::SameClass)
                require(child.node.has_value(),
                        "crossed internal reference lost its node");
            else {
                require(!child.node.has_value() &&
                          child.domain != Model::ChildDomain::Invalid,
                        "crossed external reference was interned or invalid");
                lowerGhost += child.domain == Model::ChildDomain::LowerGhost;
                exactTerminal +=
                  child.domain == Model::ChildDomain::ExactTerminal;
                if (child.domain == Model::ChildDomain::ExactTerminal)
                    require(child.child.winner.has_value(),
                            "crossed terminal reference lost its winner");
                const Solver::ExternalForceQuery query =
                  Solver::external_force_query(
                    externalExpansion, Color::Black, child);
                require(!query.belief.empty() &&
                          query.domain == child.domain &&
                          query.actual.domain == child.domain,
                        "crossed external force query lost its inherited cell");
                if (child.domain == Model::ChildDomain::ExactTerminal)
                    blackTerminalForces +=
                      Solver::exact_terminal_force(query);
            }
        }
    require(lowerGhost > 0 && exactTerminal > 0,
            "crossed Bellman plan lost lower-Ghost or terminal references");
    require(blackTerminalForces == exactTerminal,
            "crossed Black terminal force did not reproduce the public winner");
    const Solver::TargetBellmanPlan externalWhite =
      externalArena.bellman_plan(externalId, Color::White, false);
    std::size_t largestWhiteLowerBelief = 0;
    for (const Solver::AtomEquationPlan& equation : externalWhite.atoms)
        for (const Solver::ChildReference& child : equation.children)
            if (child.domain == Model::ChildDomain::LowerGhost) {
                const Solver::ExternalForceQuery query =
                  Solver::external_force_query(
                    externalExpansion, Color::White, child);
                largestWhiteLowerBelief = std::max(
                  largestWhiteLowerBelief, query.belief.size());
            }
            else if (child.domain == Model::ChildDomain::ExactTerminal) {
                const Solver::ExternalForceQuery query =
                  Solver::external_force_query(
                    externalExpansion, Color::White, child);
                require(!Solver::exact_terminal_force(query),
                        "crossed White force accepted a Black terminal win");
            }
    require(largestWhiteLowerBelief == 8,
            "crossed lower-Ghost query freshened or singletonized the White belief");
    std::cout << "crossed_solver_external lower_ghost " << lowerGhost
              << " exact_terminal " << exactTerminal
              << " inherited_white_belief " << largestWhiteLowerBelief
              << " unresolved_as_draw 0 residual 0\n";
}

}  // namespace
}  // namespace Stockfish::Ultimate

int main() {
    try {
        Stockfish::Ultimate::solver_arena_test();
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Crossed Jester/Ghost solver test failed: "
                  << error.what() << '\n';
        return 1;
    }
}
