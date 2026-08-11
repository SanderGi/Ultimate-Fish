/*
  Standalone exact-contract tests for crossed_jester_ghost_information_model.

  Compile directly while proof-bound build sources remain frozen:

    c++ -std=c++17 -O3 -Isrc/ultimate \
      src/ultimate/position.cpp src/ultimate/tablebases/information.cpp \
      src/ultimate/nnue.cpp \
      src/ultimate/tablebases/crossed_jester_ghost_information_model.cpp \
      tests/tablebases/ultimate_crossed_jester_ghost_information_model.cpp \
      -o /tmp/ultimate_crossed_jester_ghost_information_model_test
*/

#include "crossed_jester_ghost_information_model.h"
#include "information.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace Stockfish::Ultimate {
namespace {

namespace Model = CrossedJesterGhostInformation;

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error(message);
}

void expect(bool condition, const std::string& message) {
    if (!condition)
        fail(message);
}

[[nodiscard]] std::uint8_t square(const char* name) {
    const int result = Position::square_from_name(name);
    if (result == Position::NoSquare)
        fail(std::string("invalid test square: ") + name);
    return static_cast<std::uint8_t>(result);
}

[[nodiscard]] Move require_move(const Position& position,
                                std::uint8_t from, std::uint8_t to,
                                MoveKind kind = MoveKind::Normal) {
    for (const Move& move : position.legal_moves())
        if (move.from == from && move.to == to && move.kind == kind)
            return move;
    fail("required crossed-information move is absent");
}

[[nodiscard]] bool same_source(const Model::ConcreteState& lhs,
                               const Model::ConcreteState& rhs) {
    return lhs.side == rhs.side && lhs.whiteKing == rhs.whiteKing &&
           lhs.blackKing == rhs.blackKing && lhs.jester == rhs.jester &&
           lhs.ghost == rhs.ghost &&
           lhs.ghostVisible == rhs.ghostVisible;
}

void exhaustive_codec_test() {
    for (std::uint32_t index = 0; index < Model::StateCount; ++index) {
        const Model::ConcreteState decoded = Model::decode_source(index);
        if (Model::encode_source(decoded) != index)
            fail("crossed dense codec residual at " + std::to_string(index));
        const Model::FramedWorld product =
          Model::source_to_product(decoded);
        const unsigned variable = Model::world_variable(
          product.frame, product.world);
        if (!(Model::decode_world_variable(product.frame, variable) ==
              product.world))
            fail("crossed product codec residual at " +
                 std::to_string(index));
        if (!same_source(decoded, Model::product_to_source(
                                   product.frame, product.world)))
            fail("crossed source/product residual at " +
                 std::to_string(index));
    }
    for (std::uint32_t index = 0;
         index < Model::LowerJesterStateCount; ++index)
        if (Model::encode_lower_jester(
              Model::decode_lower_jester(index)) != index)
            fail("lower Jester codec residual at " + std::to_string(index));
    for (std::uint32_t index = 0;
         index < Model::LowerGhostStateCount; ++index)
        if (Model::encode_lower_ghost(
              Model::decode_lower_ghost(index)) != index)
            fail("lower Ghost codec residual at " + std::to_string(index));
    std::cout << "crossed_codec source " << Model::StateCount
              << " lower_jester " << Model::LowerJesterStateCount
              << " lower_ghost " << Model::LowerGhostStateCount
              << " residual 0\n";
}

Model::KnowledgeState four_world_state(Color side) {
    Model::KnowledgeState state;
    state.frame = {side, square("h10"), square("a1"), square("c1"), {}};
    state.atoms = {
      {{true, square("a5")}},
      {{false, square("a5")}},
      {{true, square("c5")}},
      {{false, square("c5")}},
    };
    for (const Model::HistoryAtom& atom : state.atoms)
        state.worlds.set(Model::world_variable(state.frame, atom.world));
    state.white.cells = {{0, 2}, {1, 3}};
    state.black.cells = {{0, 1}, {2, 3}};
    Model::validate_knowledge_state(state);
    return state;
}

void fresh_partition_test() {
    const Model::PublicFrame frame{
      Color::Black, square("h10"), square("a1"), square("c1"), {}};
    const Model::KnowledgeState state = Model::fresh_state(frame);
    expect(Model::geometric_worlds(frame).size() == 154,
           "crossed hidden product is not 2*77");
    expect(state.atoms.size() == 142 && state.worlds.count() == 142,
           "fresh crossed causal admission count is not exact");
    expect(state.white.cells.size() == 2 &&
             state.black.cells.size() == 71 &&
             std::all_of(state.white.cells.begin(), state.white.cells.end(),
               [](const auto& cell) { return cell.size() == 71; }) &&
             std::all_of(state.black.cells.begin(), state.black.cells.end(),
               [](const auto& cell) { return cell.size() == 2; }),
           "fresh crossed ownership partitions are not exact");

    Model::KnowledgeState different = four_world_state(Color::Black);
    Model::KnowledgeState split = different;
    split.black.cells = {{0}, {1}, {2, 3}};
    Model::validate_knowledge_state(split);
    expect(different.worlds == split.worlds && !(different == split),
           "same world set with different perfect recall was merged");
    std::cout << "crossed_fresh geometric 154 admitted 142"
              << " white_cells 2 black_cells 71"
              << " partition_identity 1 residual 0\n";
}

void decision_refinement_test() {
    Model::KnowledgeState state;
    state.frame = {Color::Black, square("e7"), square("e5"),
                   square("e6"), {}};
    state.atoms = {{{true, square("h10")}},
                   {{false, square("h10")}}};
    for (const Model::HistoryAtom& atom : state.atoms)
        state.worlds.set(Model::world_variable(state.frame, atom.world));
    state.white.cells = {{0}, {1}};
    state.black.cells = {{0, 1}};
    Model::validate_knowledge_state(state);
    expect(view_key(Model::make_position(state.frame, state.atoms[0].world),
                    {Color::Black, false}) ==
             view_key(Model::make_position(state.frame, state.atoms[1].world),
                      {Color::Black, false}),
           "royal witness differs in ordinary Black view");
    bool rejectedUnrefined = false;
    try {
        (void)Model::uniform_actions(state, 0);
    }
    catch (const std::invalid_argument&) {
        rejectedUnrefined = true;
    }
    expect(rejectedUnrefined,
           "uniform-action API accepted an unrefined legal-dot cell");
    const Model::KnowledgePartition whiteBefore = state.white;
    const Model::KnowledgeState refined =
      Model::refine_mover_decisions(state);
    expect(refined.black.cells.size() == 2 &&
             refined.white == whiteBefore,
           "Black legal dots did not split only Black's partition");

    Model::KnowledgeState white = four_world_state(Color::White);
    const Model::KnowledgePartition blackBefore = white.black;
    const Model::KnowledgeState whiteRefined =
      Model::refine_mover_decisions(white);
    expect(whiteRefined.black == blackBefore,
           "White decision refinement changed Black's memory");
    std::cout << "crossed_decision black_pair_to_singletons 2"
              << " nonmover_partition_changes 0 residual 0\n";
}

void perfect_recall_convergence_test() {
    Model::KnowledgeState state = four_world_state(Color::Black);
    state = Model::refine_mover_decisions(state);
    expect(state.black.cells.size() == 2,
           "far royal assignment unexpectedly split Black Ghost cells");
    std::vector<Model::CellAction> policy;
    for (std::uint32_t cell = 0; cell < state.black.cells.size(); ++cell) {
        const Model::ProductWorld world =
          state.atoms[state.black.cells[cell].front()].world;
        const std::uint8_t destination = square("b5");
        const Position position = Model::make_position(state.frame, world);
        policy.push_back({cell, Model::action_key(require_move(
          position, world.ghost, destination))});
    }
    const std::vector<Model::SuccessorBucket> successors =
      Model::apply_uniform_policy(state, policy);
    expect(successors.size() == 1 && successors.front().sameClass,
           "two quiet hidden policies did not share one public observation");
    const Model::KnowledgeState& child = *successors.front().sameClass;
    expect(child.atoms.size() == 4 && child.worlds.count() == 2,
           "convergent histories were collapsed into physical worlds");
    expect(child.white.cells.size() == 2 && child.black.cells.size() == 2 &&
             std::all_of(child.white.cells.begin(), child.white.cells.end(),
               [](const auto& cell) { return cell.size() == 2; }) &&
             std::all_of(child.black.cells.begin(), child.black.cells.end(),
               [](const auto& cell) { return cell.size() == 2; }),
           "successor private observations lost perfect recall partitions");
    expect(successors.front().white == child.white &&
             successors.front().black == child.black,
           "same-class bucket and child partitions disagree");
    for (const Model::KnowledgeCell& cell : child.black.cells) {
        const std::uint32_t first = cell.front();
        const std::uint32_t second = cell.back();
        expect(child.atoms[first].world.ghost == square("b5") &&
                 child.atoms[second].world.ghost == square("b5"),
               "Black history cells do not share the converged physical Ghost");
    }
    const Model::KnowledgePartition blackBefore = child.black;
    const Model::KnowledgeState decision =
      Model::refine_mover_decisions(child);
    expect(decision.black == blackBefore,
           "next White legal dots leaked into Black's partition");
    const std::array<Model::RectangleTransform, 4> transforms{
      Model::RectangleTransform::Identity,
      Model::RectangleTransform::Horizontal,
      Model::RectangleTransform::Vertical,
      Model::RectangleTransform::Both};
    for (const auto transform : transforms) {
        const Model::KnowledgeState mappedState =
          Model::transform_state(state, transform);
        std::vector<Model::CellAction> mappedPolicy;
        for (const Model::CellAction& choice : policy)
            mappedPolicy.push_back({choice.cell,
              Model::transform_action(choice.action, transform)});
        const auto mappedSuccessors = Model::apply_uniform_policy(
          mappedState, mappedPolicy);
        expect(mappedSuccessors.size() == 1 &&
                 mappedSuccessors.front().sameClass &&
                 *mappedSuccessors.front().sameClass ==
                   Model::transform_state(child, transform),
               "crossed policy/observation update changed under D2");
    }
    std::cout << "crossed_perfect_recall source_atoms 4 child_atoms 4"
              << " physical_worlds 2 white_cells 2 black_cells 2 residual 0\n";
}

void complete_transition_enumeration_test() {
    const Model::KnowledgeState source = four_world_state(Color::Black);
    const Model::CompleteTransitions generated =
      Model::enumerate_complete_transitions(source);
    const Model::KnowledgePartition& mover = generated.decisionState.black;
    expect(!generated.actions.empty() && !generated.buckets.empty(),
           "complete crossed transition enumeration is empty");
    std::uint64_t expectedOutcomes = 0;
    std::uint64_t actualOutcomes = 0;
    for (const Model::CellActionOutcomes& action : generated.actions) {
        expect(action.choice.cell < mover.cells.size(),
               "complete crossed action references an invalid private cell");
        expectedOutcomes += mover.cells[action.choice.cell].size();
        actualOutcomes += action.outcomes.size();
        std::vector<std::uint32_t> sources;
        for (const Model::AtomOutcome& outcome : action.outcomes) {
            expect(outcome.bucket < generated.buckets.size() &&
                     outcome.childAtom <
                       generated.buckets[outcome.bucket].atoms.size(),
                   "complete crossed outcome references an invalid child");
            expect(generated.buckets[outcome.bucket]
                     .atoms[outcome.childAtom].sourceAtom == outcome.sourceAtom,
                   "complete crossed outcome/child mapping has a residual");
            sources.push_back(outcome.sourceAtom);
        }
        std::sort(sources.begin(), sources.end());
        expect(sources == mover.cells[action.choice.cell],
               "complete crossed action does not cover its exact cell");
    }
    expect(expectedOutcomes == actualOutcomes,
           "complete crossed transition outcome conservation residual");
    for (const Model::SuccessorBucket& bucket : generated.buckets) {
        expect(!bucket.atoms.empty(),
               "complete crossed transition produced an empty bucket");
        if (bucket.domain == Model::ChildDomain::SameClass) {
            expect(bucket.sameClass.has_value(),
                   "same-class complete bucket has no knowledge state");
            Model::validate_knowledge_state(*bucket.sameClass);
        }
    }
    std::cout << "crossed_complete_transitions actions "
              << generated.actions.size() << " observations "
              << generated.buckets.size() << " outcomes " << actualOutcomes
              << " residual 0\n";
}

void portable_state_codec_test() {
    const Model::KnowledgeState state = four_world_state(Color::Black);
    const std::vector<std::uint8_t> encoded = Model::serialize_state(state);
    expect(Model::deserialize_state(encoded) == state,
           "portable crossed-state codec does not round trip");
    expect(Model::serialize_state(Model::deserialize_state(encoded)) == encoded,
           "portable crossed-state encoding is not canonical for one state");
    std::vector<std::uint8_t> truncated = encoded;
    truncated.pop_back();
    bool rejected = false;
    try {
        (void)Model::deserialize_state(truncated);
    }
    catch (const std::invalid_argument&) {
        rejected = true;
    }
    expect(rejected, "portable crossed-state codec accepted truncation");
    std::vector<std::uint8_t> trailing = encoded;
    trailing.push_back(0);
    rejected = false;
    try {
        (void)Model::deserialize_state(trailing);
    }
    catch (const std::invalid_argument&) {
        rejected = true;
    }
    expect(rejected, "portable crossed-state codec accepted trailing bytes");
    std::cout << "crossed_state_codec bytes " << encoded.size()
              << " truncation_residual 0 trailing_residual 0\n";
}

void folded_physical_successor_test() {
    Model::KnowledgeState state;
    state.frame = {Color::Black, square("d10"), square("b2"),
                   square("g2"), {}};
    state.atoms = {{{true, square("d5")}},
                   {{false, square("d5")}}};
    for (const Model::HistoryAtom& atom : state.atoms)
        state.worlds.set(Model::world_variable(state.frame, atom.world));
    state.white.cells = {{0}, {1}};
    state.black.cells = {{0, 1}};
    Model::validate_knowledge_state(state);
    state = Model::refine_mover_decisions(state);
    expect(state.black.cells.size() == 1,
           "folded physical witness unexpectedly split legal dots");
    const Position position = Model::make_position(
      state.frame, state.atoms.front().world);
    const Model::ActionKey action = Model::action_key(require_move(
      position, square("d10"), square("d9")));
    const auto successors = Model::apply_uniform_policy(
      state, {{0, action}});
    expect(successors.size() == 1 && successors.front().sameClass &&
             successors.front().sameClass->frame.blackKing == square("d9"),
           "same-class successor lost its physical public frame");
    const Model::FramedWorld foldedFirst = Model::source_to_product(
      Model::decode_source(successors.front().atoms[0].child.index));
    const Model::FramedWorld foldedSecond = Model::source_to_product(
      Model::decode_source(successors.front().atoms[1].child.index));
    expect(!(foldedFirst.frame == foldedSecond.frame),
           "folded witness no longer selects opposite concrete reflections");
    std::cout << "crossed_physical_child folded_frames 2 public_frames 1"
              << " residual 0\n";
}

void symmetry_and_action_test() {
    const std::array<Model::RectangleTransform, 4> transforms{
      Model::RectangleTransform::Identity,
      Model::RectangleTransform::Horizontal,
      Model::RectangleTransform::Vertical,
      Model::RectangleTransform::Both};
    for (std::uint8_t source = 0; source < Position::BoardSquares; ++source)
        for (const auto transform : transforms)
            expect(Model::transform_square(
                     Model::transform_square(source, transform), transform) ==
                     source,
                   "crossed square transform is not an involution");
    const std::vector<Model::ActionKey> actions{
      {square("b2"), square("c3"), 0, MoveKind::Normal,
       PieceType::Count},
      {square("b2"), square("c3"), 0, MoveKind::Normal,
       PieceType::Queen},
      {square("b2"), square("d2"), 3, MoveKind::Castle,
       PieceType::Count},
      {square("b2"), square("f6"), 3, MoveKind::Swap,
       PieceType::Count},
      {square("b2"), square("e5"), 0, MoveKind::Spawn,
       PieceType::Count},
      {square("b2"), square("e5"), 3, MoveKind::Shoot,
       PieceType::Count},
      {square("b2"), square("e5"), square("c3"), MoveKind::Pull,
       PieceType::Count},
      {square("b2"), square("e5"), 3, MoveKind::Link,
       PieceType::Count},
      {0, 0, 0, MoveKind::Pass, PieceType::Count},
    };
    expect(std::set<Model::ActionKey>(actions.begin(), actions.end()).size() ==
             actions.size(),
           "complete crossed action key merges identities");
    for (const Model::ActionKey& action : actions)
        for (const auto transform : transforms)
            expect(Model::transform_action(
                     Model::transform_action(action, transform), transform) ==
                     action,
                   "crossed complete action transform is not an involution");

    const Model::KnowledgeState source = four_world_state(Color::Black);
    for (const auto transform : transforms) {
        const Model::KnowledgeState mapped = Model::transform_state(
          source, transform);
        const Model::KnowledgeState restored = Model::transform_state(
          mapped, transform);
        expect(restored == source,
               "whole crossed partition state transform is not an involution");
        expect(mapped.white == source.white && mapped.black == source.black,
               "D2 transform changed perfect recall partitions");
    }
    const Model::CanonicalState canonical =
      Model::canonicalize_state(source);
    expect(canonical.value.atoms.size() == source.atoms.size() &&
             canonical.value.worlds.count() == source.worlds.count(),
           "crossed canonicalization lost histories or worlds");
    std::cout << "crossed_symmetry actions " << actions.size()
              << " transforms " << transforms.size()
              << " history_atoms " << source.atoms.size()
              << " residual 0\n";
}

void lower_projection_and_terminal_test() {
    const Model::LowerJesterState first{
      Color::Black, square("a1"), square("h10"), square("c1")};
    const Model::LowerJesterState second{
      Color::Black, square("c1"), square("h10"), square("a1")};
    const Model::ClassifiedChild firstChild{
      Model::ChildDomain::LowerJester,
      Model::encode_lower_jester(first), {}};
    const Model::ClassifiedChild secondChild{
      Model::ChildDomain::LowerJester,
      Model::encode_lower_jester(second), {}};
    expect(Model::inherited_lower_jester_set({firstChild}).cardinality == 1 &&
             Model::inherited_lower_jester_set(
               {firstChild, secondChild}).cardinality == 2,
           "lower Jester pair/singleton was fresh-remaximized");

    const std::array<const char*, 8> ghosts{
      "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8"};
    std::vector<Model::ClassifiedChild> lowerGhosts;
    for (const char* name : ghosts) {
        const Model::PublicFrame frame{
          Color::Black, square("e5"), square("a1"), square("e4"), {}};
        const Model::ProductWorld world{true, square(name)};
        const Position sourcePosition = Model::make_position(frame, world);
        const Move capture = require_move(
          sourcePosition, square("e5"), square("e4"));
        Position child = sourcePosition;
        Undo undo;
        expect(child.make_move(capture, undo),
               "Black King Jester capture failed");
        const Model::ClassifiedChild classified =
          Model::classify_child(child);
        expect(classified.domain == Model::ChildDomain::LowerGhost,
               "Jester capture did not lower to K+Ghost-v-K");
        lowerGhosts.push_back(classified);
    }
    const Model::LowerGhostImage image =
      Model::inherited_lower_ghost_image(lowerGhosts);
    expect(image.locations.count() == ghosts.size() &&
             image.side == Model::Role::Observer &&
             image.ownerKing == square("e4") &&
             image.observerKing == square("a1"),
           "crossed Jester capture produced the wrong UFGM role image");

    Model::KnowledgeState crossed;
    crossed.frame = {Color::Black, square("e5"), square("a1"),
                     square("e4"), {}};
    for (const char* name : ghosts) {
        crossed.atoms.push_back({{true, square(name)}});
        crossed.atoms.push_back({{false, square(name)}});
    }
    for (const Model::HistoryAtom& atom : crossed.atoms)
        crossed.worlds.set(Model::world_variable(crossed.frame, atom.world));
    Model::KnowledgeCell firstAssignment;
    Model::KnowledgeCell secondAssignment;
    for (std::uint32_t atom = 0; atom < crossed.atoms.size(); ++atom)
        (atom % 2 ? secondAssignment : firstAssignment).push_back(atom);
    crossed.white.cells = {firstAssignment, secondAssignment};
    for (std::uint32_t atom = 0; atom < crossed.atoms.size(); atom += 2)
        crossed.black.cells.push_back({atom, atom + 1});
    Model::validate_knowledge_state(crossed);
    crossed = Model::refine_mover_decisions(crossed);
    std::vector<Model::CellAction> policy;
    for (std::uint32_t cell = 0; cell < crossed.black.cells.size(); ++cell) {
        const Position position = Model::make_position(
          crossed.frame,
          crossed.atoms[crossed.black.cells[cell].front()].world);
        policy.push_back({cell, Model::action_key(require_move(
          position, square("e5"), square("e4")))});
    }
    const auto buckets = Model::apply_uniform_policy(crossed, policy);
    expect(buckets.size() == 2,
           "public transition did not separate Jester and real-King capture");
    const auto lowerBucket = std::find_if(
      buckets.begin(), buckets.end(), [](const Model::SuccessorBucket& bucket) {
          return bucket.domain == Model::ChildDomain::LowerGhost;
      });
    const auto terminalBucket = std::find_if(
      buckets.begin(), buckets.end(), [](const Model::SuccessorBucket& bucket) {
          return bucket.domain == Model::ChildDomain::ExactTerminal;
      });
    expect(lowerBucket != buckets.end() && terminalBucket != buckets.end() &&
             lowerBucket->atoms.size() == ghosts.size() &&
             lowerBucket->white.cells.size() == 1 &&
             lowerBucket->black.cells.size() == ghosts.size() &&
             terminalBucket->white.cells.size() == 1 &&
             terminalBucket->black.cells.size() == ghosts.size(),
           "external/terminal successor partitions lost crossed private memory");
    std::vector<Model::ClassifiedChild> partitionedLower;
    for (const Model::TransitionAtom& atom : lowerBucket->atoms)
        partitionedLower.push_back(atom.child);
    expect(Model::inherited_lower_ghost_image(
             partitionedLower).locations.count() == ghosts.size(),
           "partitioned lower bucket changed its arbitrary UFGM mask");

    const Model::PublicFrame terminalFrame{
      Color::Black, square("e5"), square("a1"), square("e4"), {}};
    const Model::ProductWorld kingTarget{false, square("h8")};
    const Position sourcePosition = Model::make_position(
      terminalFrame, kingTarget);
    const Move capture = require_move(
      sourcePosition, square("e5"), square("e4"));
    Position terminal = sourcePosition;
    Undo undo;
    expect(terminal.make_move(capture, undo),
           "crossed real-King capture failed");
    const Model::ClassifiedChild classified =
      Model::classify_child(terminal);
    expect(classified.domain == Model::ChildDomain::ExactTerminal &&
             classified.winner == Color::Black,
           "crossed real-King capture is not exact terminal");
    std::cout << "crossed_lower jester_pair 1 jester_singleton 1"
              << " ghost_mask " << image.locations.count()
              << " terminal 1 external_partitions 2 residual 0\n";
}

}  // namespace
}  // namespace Stockfish::Ultimate

int main() {
    using namespace Stockfish::Ultimate;
    try {
        fresh_partition_test();
        decision_refinement_test();
        perfect_recall_convergence_test();
        complete_transition_enumeration_test();
        portable_state_codec_test();
        folded_physical_successor_test();
        symmetry_and_action_test();
        lower_projection_and_terminal_test();
        exhaustive_codec_test();
        std::cout << "ultimate crossed Jester/Ghost information tests passed\n";
    }
    catch (const std::exception& error) {
        std::cerr << "ultimate crossed Jester/Ghost information test failed: "
                  << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
