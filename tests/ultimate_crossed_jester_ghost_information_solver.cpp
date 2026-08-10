/* Exact crossed Jester/Ghost solver-core regression. GPLv3+. */

#include "crossed_jester_ghost_information_solver.h"

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
    std::cout << "crossed_solver_arena nodes " << arena.size()
              << " actions " << first.certificate.actions
              << " observations " << first.certificate.observations
              << " outcomes " << first.certificate.outcomes
              << " replay_new_nodes 0 residual 0\n";
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
