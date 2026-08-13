/* Exact-contract tests for opposed_ghost_pair_information_model. */

#include "opposed_ghost_pair_information_model.h"
#include "opposed_ghost_pair_information_solver.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace Stockfish::Ultimate {
namespace {

namespace Model = OpposedGhostPairInformation;
namespace Solver = OpposedGhostPairSolver;

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error(message);
}

void expect(bool condition, const std::string& message) {
    if (!condition)
        fail(message);
}

std::uint8_t square(const char* name) {
    const int result = Position::square_from_name(name);
    if (result == Position::NoSquare)
        fail(std::string("bad square ") + name);
    return static_cast<std::uint8_t>(result);
}

bool same_source(const Model::ConcreteState& lhs,
                 const Model::ConcreteState& rhs) {
    return lhs.side == rhs.side && lhs.whiteKing == rhs.whiteKing &&
           lhs.blackKing == rhs.blackKing &&
           lhs.whiteGhost == rhs.whiteGhost &&
           lhs.blackGhost == rhs.blackGhost &&
           lhs.whiteGhostVisible == rhs.whiteGhostVisible &&
           lhs.blackGhostVisible == rhs.blackGhostVisible;
}

void source_codec_test(bool exhaustive) {
    const std::uint32_t stride = exhaustive ? 1u : 7919u;
    for (std::uint32_t index = 0; index < Model::StateCount; index += stride) {
        const Model::ConcreteState decoded = Model::decode_source(index);
        expect(Model::encode_source(decoded) == index,
               "opposed Ghost source codec residual");
        const Model::FramedWorld product = Model::source_to_product(decoded);
        expect(Model::decode_world_variable(product.frame,
                 Model::world_variable(product.frame, product.world)) ==
                 product.world,
               "opposed Ghost product codec residual");
        expect(same_source(decoded, Model::product_to_source(
                 product.frame, product.world)),
               "opposed Ghost source/product residual");
    }
    const auto last = Model::decode_source(Model::StateCount - 1);
    expect(Model::encode_source(last) == Model::StateCount - 1,
           "opposed Ghost final source codec residual");
    std::cout << "opposed_ghost_source_codec states " << Model::StateCount
              << " exhaustive " << exhaustive << " residual 0\n";
}

void public_codec_test(bool exhaustive) {
    const std::uint32_t stride = exhaustive ? 1u : 4001u;
    for (std::uint32_t index = 0;
         index < Model::RawPublicFrameCount; index += stride) {
        const auto frame = Model::decode_public_frame(index);
        expect(Model::encode_public_frame(frame) == index,
               "opposed Ghost public codec residual");
    }
    const auto last = Model::decode_public_frame(Model::RawPublicFrameCount - 1);
    expect(Model::encode_public_frame(last) == Model::RawPublicFrameCount - 1,
           "opposed Ghost final public codec residual");
    std::cout << "opposed_ghost_public_codec frames "
              << Model::RawPublicFrameCount << " exhaustive " << exhaustive
              << " residual 0\n";
}

void fresh_partition_test() {
    const Model::PublicFrame frame{
      Color::White, square("a1"), square("h10"), {}, {}};
    expect(Model::geometric_worlds(frame).size() == 6006,
           "opposed Ghost hidden product is not 78*77");
    const Model::KnowledgeState state = Model::fresh_state(frame);
    expect(state.atoms.size() == 5553 && state.worlds.count() == 5553,
           "opposed Ghost causal admission count changed");
    expect(state.white.cells.size() == 75 && state.black.cells.size() == 75,
           "opposed Ghost owner partitions are not coordinate-exact");
    for (const auto& cell : state.white.cells)
        expect(cell.size() == 74 || cell.size() == 75,
               "White owner cell has impossible size");
    for (const auto& cell : state.black.cells)
        expect(cell.size() == 74 || cell.size() == 75,
               "Black owner cell has impossible size");

    const auto bytes = Model::serialize_state(state);
    expect(Model::deserialize_state(bytes) == state,
           "opposed Ghost perfect-recall serialization residual");
    std::cout << "opposed_ghost_fresh geometric 6006 admitted 5553"
              << " white_cells 75 black_cells 75 residual 0\n";
}

void private_partition_test() {
    Model::KnowledgeState state;
    state.frame = {Color::White, square("a1"), square("h10"), {}, {}};
    state.atoms = {
      {{square("c3"), square("e5")}},
      {{square("c3"), square("g5")}},
    };
    for (const auto& atom : state.atoms)
        state.worlds.set(Model::world_variable(state.frame, atom.world));
    state.white.cells = {{0, 1}};
    state.black.cells = {{0}, {1}};
    Model::validate_knowledge_state(state);

    const Model::KnowledgeState refined = Model::refine_mover_decisions(state);
    expect(refined.black == state.black,
           "nonmover partition changed during legal-dot inspection");
    for (const auto& cell : refined.white.cells) {
        const auto actions = Model::uniform_actions(refined,
          static_cast<std::uint32_t>(&cell - refined.white.cells.data()));
        expect(!actions.empty(), "refined mover cell has no uniform action");
    }

    const auto horizontal = Model::transform_state(
      state, Model::RectangleTransform::Horizontal);
    const auto restored = Model::transform_state(
      horizontal, Model::RectangleTransform::Horizontal);
    expect(restored == state, "opposed Ghost D2 transform residual");
    std::cout << "opposed_ghost_private mover_cells "
              << refined.white.cells.size()
              << " nonmover_preserved 1 symmetry_residual 0\n";
}

void solver_specialization_test() {
    const Model::PublicFrame frame{
      Color::White, square("a1"), square("h10"),
      square("c3"), square("f8")};
    const Model::KnowledgeState state = Model::fresh_state(frame);
    expect(state.atoms.size() == 1,
           "visible opposed Ghost root is not singleton");
    Solver::Arena arena;
    const Solver::NodeId root = arena.intern(state);
    const Solver::NodeExpansion expansion = arena.regenerate(root, true);
    expect(expansion.certificate.actions != 0 &&
           expansion.certificate.outcomes != 0 &&
           expansion.certificate.keyRoundtripResidual == 0,
           "opposed Ghost graph specialization did not expand exactly");
    std::cout << "opposed_ghost_solver actions "
              << expansion.certificate.actions << " observations "
              << expansion.certificate.observations << " outcomes "
              << expansion.certificate.outcomes << " residual 0\n";
}

void capture_lowering_test() {
    const Model::PublicFrame frame{
      Color::White, square("a1"), square("h10"),
      square("d5"), square("e5")};
    const Model::ProductWorld world{square("d5"), square("e5")};
    Position position = Model::make_position(frame, world);
    Move capture;
    bool found = false;
    for (const Move& move : position.legal_moves())
        if (move.from == world.whiteGhost && move.to == world.blackGhost &&
            position.is_capture(move)) {
            capture = move;
            found = true;
            break;
        }
    expect(found, "opposed Ghost capture fixture has no capture");
    Undo undo;
    expect(position.make_move(capture, undo),
           "opposed Ghost capture did not apply");
    const Model::ClassifiedChild child = Model::classify_child(position);
    expect(child.domain == Model::ChildDomain::LowerGhost,
           "Ghost capture did not lower to authenticated K+Ghost");
    const Model::LowerGhostState lower = Model::decode_lower_ghost(child.index);
    expect(lower.side == Model::Role::Observer &&
           lower.ghost == world.blackGhost,
           "lower K+Ghost roles or coordinate are incorrect");
    std::cout << "opposed_ghost_capture lower_ghost 1 residual 0\n";
}

}  // namespace
}  // namespace Stockfish::Ultimate

int main(int argc, char** argv) {
    try {
        const bool exhaustive = argc == 2 &&
          std::string(argv[1]) == "--exhaustive";
        Stockfish::Ultimate::source_codec_test(exhaustive);
        Stockfish::Ultimate::public_codec_test(exhaustive);
        Stockfish::Ultimate::fresh_partition_test();
        Stockfish::Ultimate::private_partition_test();
        Stockfish::Ultimate::solver_specialization_test();
        Stockfish::Ultimate::capture_lowering_test();
        std::cout << "opposed_ghost_pair_information_model_ok\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error) {
        std::cerr << "Opposed Ghost-pair model test failed: "
                  << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
