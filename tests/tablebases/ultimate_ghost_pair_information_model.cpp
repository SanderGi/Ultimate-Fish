/*
  Standalone exact-contract tests for ghost_pair_information_model.

  Compile directly while the proof-bound build graph remains frozen:

    c++ -std=c++17 -O3 -Isrc/ultimate \
      src/ultimate/position.cpp src/ultimate/tablebases/information.cpp \
      src/ultimate/nnue.cpp \
      src/ultimate/tablebases/ghost_pair_information_model.cpp \
      tests/tablebases/ultimate_ghost_pair_information_model.cpp \
      -o /tmp/ultimate_ghost_pair_information_model_test
*/

#include "ghost_pair_information_model.h"
#include "information.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace Stockfish::Ultimate {
namespace {

namespace Model = GhostPairInformation;

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
    fail("required native Ghost-pair move is absent");
}

[[nodiscard]] bool same_source(const Model::ConcreteState& lhs,
                               const Model::ConcreteState& rhs) {
    return lhs.side == rhs.side && lhs.whiteKing == rhs.whiteKing &&
           lhs.blackKing == rhs.blackKing &&
           lhs.firstGhost == rhs.firstGhost &&
           lhs.secondGhost == rhs.secondGhost &&
           lhs.firstVisible == rhs.firstVisible &&
           lhs.secondVisible == rhs.secondVisible;
}

void exhaustive_codec_and_swap_test() {
    for (std::uint32_t index = 0; index < Model::StateCount; ++index) {
        const Model::ConcreteState decoded = Model::decode_source(index);
        if (Model::encode_source(decoded) != index)
            fail("Ghost-pair dense codec residual at " +
                 std::to_string(index));
        Model::ConcreteState swapped = decoded;
        std::swap(swapped.firstGhost, swapped.secondGhost);
        std::swap(swapped.firstVisible, swapped.secondVisible);
        if (Model::encode_source(swapped) != index)
            fail("same-team Ghost swap changed dense index at " +
                 std::to_string(index));
        const Model::FramedWorld product =
          Model::source_to_product(decoded);
        const unsigned variable =
          Model::pair_variable(product.frame, product.world);
        if (!(Model::decode_pair_variable(product.frame, variable) ==
              product.world))
            fail("Ghost-pair product codec residual at " +
                 std::to_string(index));
        if (!same_source(decoded, Model::product_to_source(
                                   product.frame, product.world)))
            fail("source/product Ghost-pair residual at " +
                 std::to_string(index));
        const Model::FramedWorld swappedProduct =
          Model::source_to_product(swapped);
        if (!(swappedProduct.frame == product.frame) ||
            !(swappedProduct.world == product.world))
            fail("same-team Ghost identity leaked into product frame at " +
                 std::to_string(index));
    }
    for (std::uint32_t index = 0;
         index < Model::LowerGhostStateCount; ++index)
        if (Model::encode_lower_ghost(
              Model::decode_lower_ghost(index)) != index)
            fail("lower Ghost codec residual at " + std::to_string(index));
    std::cout << "ghost_pair_codec source_states " << Model::StateCount
              << " swap_invariant " << Model::StateCount
              << " lower_ghost " << Model::LowerGhostStateCount
              << " residual 0\n";
}

[[nodiscard]] Model::PairMask complete_mask(
  const Model::PublicFrame& frame) {
    Model::PairMask result;
    for (const Model::PairWorld& world : Model::geometric_worlds(frame))
        result.set(Model::pair_variable(frame, world));
    return result;
}

void correlated_product_test() {
    const Model::PublicFrame hidden{
      Color::Black, square("a1"), square("h10"), {}, 0};
    Model::PublicFrame mixed = hidden;
    mixed.visibleGhosts[0] = square("d4");
    mixed.visibleCount = 1;
    Model::PublicFrame visible = hidden;
    visible.visibleGhosts = {square("d4"), square("f6")};
    visible.visibleCount = 2;
    expect(Model::variable_count(hidden) == 3003 &&
             Model::geometric_worlds(hidden).size() == 3003,
           "hidden-hidden product is not C(78,2)");
    expect(Model::variable_count(mixed) == 77 &&
             Model::geometric_worlds(mixed).size() == 77,
           "hidden-visible product does not have 77 companions");
    expect(Model::variable_count(visible) == 1 &&
             Model::geometric_worlds(visible).size() == 1,
           "visible-visible product is not a singleton");

    Model::PairMask correlated;
    const Model::PairWorld first{square("b2"), square("c3")};
    const Model::PairWorld second{square("e5"), square("f6")};
    correlated.set(Model::pair_variable(hidden, first));
    correlated.set(Model::pair_variable(hidden, second));
    const std::vector<Model::PairWorld> decoded =
      Model::decode_pair_mask(hidden, correlated);
    expect(decoded.size() == 2 &&
             std::find(decoded.begin(), decoded.end(), first) != decoded.end() &&
             std::find(decoded.begin(), decoded.end(), second) != decoded.end(),
           "correlated pair mask lost an admitted pair");
    expect(!correlated.test(Model::pair_variable(
             hidden, {square("b2"), square("f6")})) &&
             !correlated.test(Model::pair_variable(
             hidden, {square("c3"), square("e5")})),
           "correlated pair mask expanded into independent marginals");
    Model::PairMask invalid = correlated;
    invalid.words.back() |= std::uint64_t(1) <<
                            (Model::MaximumWorlds % 64);
    bool rejectedUndefinedBit = false;
    try {
        (void)Model::decode_pair_mask(hidden, invalid);
    }
    catch (const std::invalid_argument&) {
        rejectedUndefinedBit = true;
    }
    expect(rejectedUndefinedBit,
           "pair-mask codec accepted an undefined padding bit");

    const std::array<Model::RectangleTransform, 4> transforms{
      Model::RectangleTransform::Identity,
      Model::RectangleTransform::Horizontal,
      Model::RectangleTransform::Vertical,
      Model::RectangleTransform::Both};
    for (const Model::PublicFrame& frame : {hidden, mixed, visible}) {
        const Model::PairMask complete = complete_mask(frame);
        for (const auto transform : transforms) {
            const Model::PairSet mapped = Model::transform_set(
              frame, complete, transform);
            const Model::PairSet restored = Model::transform_set(
              mapped.frame, mapped.worlds, transform);
            expect(restored == Model::PairSet{frame, complete},
                   "whole correlated belief transform is not an involution");
        }
        const Model::CanonicalSet canonical =
          Model::canonicalize_set(frame, complete);
        expect(canonical.value.worlds.count() == complete.count(),
               "D2 canonicalization did not conserve pair correlation");
    }
    std::cout << "ghost_pair_product hidden_hidden 3003 hidden_visible 77"
              << " visible_visible 1 correlated_cross_products 0 residual 0\n";
}

void action_codec_test() {
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
                   "rectangle square transform is not an involution");
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
           "complete action key merged distinct engine actions");
    for (const Model::ActionKey& action : actions)
        for (const auto transform : transforms)
            expect(Model::transform_action(
                     Model::transform_action(action, transform), transform) ==
                     action,
                   "complete action transform is not an involution");
    std::cout << "ghost_pair_action_codec identities " << actions.size()
              << " transforms " << transforms.size() << " residual 0\n";
}

[[nodiscard]] bool extend_bijection(
  std::map<std::string, std::string>& forward,
  std::map<std::string, std::string>& reverse,
  const std::string& source, const std::string& target) {
    const auto [first, insertedFirst] = forward.emplace(source, target);
    if (!insertedFirst && first->second != target)
        return false;
    const auto [second, insertedSecond] = reverse.emplace(target, source);
    return insertedSecond || second->second == source;
}

void complete_frame_symmetry_test(Model::PublicFrame frame,
                                  Model::RectangleTransform transform) {
    const std::vector<Model::PairWorld> worlds =
      Model::geometric_worlds(frame);
    std::array<std::map<std::string, std::string>, 2> decisionForward;
    std::array<std::map<std::string, std::string>, 2> decisionReverse;
    std::array<std::map<std::string, std::string>, 2> transitionForward;
    std::array<std::map<std::string, std::string>, 2> transitionReverse;
    std::uint64_t actions = 0;
    std::uint64_t transitions = 0;
    for (const Model::PairWorld& world : worlds) {
        const Model::FramedWorld mapped =
          Model::transform_world(frame, world, transform);
        const Position source = Model::make_position(frame, world);
        const Position target = Model::make_position(
          mapped.frame, mapped.world);
        const std::vector<Model::ActionKey> sourceActions =
          Model::legal_actions(source);
        std::vector<Model::ActionKey> mappedActions;
        for (const Model::ActionKey& action : sourceActions)
            mappedActions.push_back(
              Model::transform_action(action, transform));
        std::sort(mappedActions.begin(), mappedActions.end());
        expect(mappedActions == Model::legal_actions(target),
               "complete Ghost-pair action frontier changed under D2");
        const std::string sourceDecision = decision_observation_key(
          source, {frame.side, false});
        const std::string targetDecision = decision_observation_key(
          target, {mapped.frame.side, false});
        expect(extend_bijection(
                 decisionForward[static_cast<std::size_t>(frame.side)],
                 decisionReverse[static_cast<std::size_t>(frame.side)],
                 sourceDecision, targetDecision),
               "mover-private dot partition changed under D2");

        const std::vector<Move> moves = source.legal_moves();
        const std::vector<Move> targetMoves = target.legal_moves();
        for (const Move& move : moves) {
            const Model::ActionKey mappedAction = Model::transform_action(
              Model::action_key(move), transform);
            const auto found = std::find_if(
              targetMoves.begin(), targetMoves.end(),
              [&](const Move& candidate) {
                  return Model::action_key(candidate) == mappedAction;
              });
            expect(found != targetMoves.end(),
                   "symmetric complete action has no target move");
            Position sourceChild = source;
            Position targetChild = target;
            Undo sourceUndo;
            Undo targetUndo;
            expect(sourceChild.make_move(move, sourceUndo) &&
                     targetChild.make_move(*found, targetUndo),
                   "symmetric Ghost-pair transition failed to apply");
            expect(Model::classify_child(sourceChild).domain ==
                     Model::classify_child(targetChild).domain,
                   "child material domain changed under D2");
            for (const Color observer : {Color::White, Color::Black}) {
                const std::string sourceObservation =
                  transition_observation_key(
                    source, move, sourceChild, {observer, false});
                const std::string targetObservation =
                  transition_observation_key(
                    target, *found, targetChild, {observer, false});
                const std::size_t side = static_cast<std::size_t>(observer);
                expect(extend_bijection(
                         transitionForward[side], transitionReverse[side],
                         sourceObservation, targetObservation),
                       "transition-observation partition changed under D2");
                ++transitions;
            }
            ++actions;
        }
    }
    std::cout << "ghost_pair_symmetry side " << int(frame.side)
              << " visible " << int(frame.visibleCount)
              << " transform " << int(transform)
              << " worlds " << worlds.size()
              << " actions " << actions
              << " observer_transitions " << transitions
              << " residual 0\n";
}

void complete_symmetry_test() {
    const std::array<Model::RectangleTransform, 4> transforms{
      Model::RectangleTransform::Identity,
      Model::RectangleTransform::Horizontal,
      Model::RectangleTransform::Vertical,
      Model::RectangleTransform::Both};
    for (const Color side : {Color::White, Color::Black}) {
        Model::PublicFrame hidden{
          side, square("b2"), square("g9"), {}, 0};
        Model::PublicFrame mixed = hidden;
        mixed.visibleGhosts[0] = square("d4");
        mixed.visibleCount = 1;
        Model::PublicFrame visible = hidden;
        visible.visibleGhosts = {square("d4"), square("f6")};
        visible.visibleCount = 2;
        for (const Model::PublicFrame& frame : {hidden, mixed, visible})
            for (const auto transform : transforms)
                complete_frame_symmetry_test(frame, transform);
    }
}

void decision_and_hidden_transition_test() {
    Model::PublicFrame observerFrame{
      Color::Black, square("a1"), square("h10"), {}, 0};
    const std::vector<Model::PairWorld> observerWorlds =
      Model::geometric_worlds(observerFrame);
    const std::vector<Model::DecisionBucket> observer =
      Model::decision_partition(observerFrame, observerWorlds);
    unsigned observerMembers = 0;
    for (const Model::DecisionBucket& bucket : observer)
        observerMembers += bucket.worlds.count();
    expect(observerMembers == 3003,
           "Black legal-dot partition lost a hidden pair");

    Model::PublicFrame ownerFrame = observerFrame;
    ownerFrame.side = Color::White;
    const std::vector<Model::PairWorld> ownerWorlds =
      Model::geometric_worlds(ownerFrame);
    const std::vector<Model::DecisionBucket> owner =
      Model::decision_partition(ownerFrame, ownerWorlds);
    expect(owner.size() == 3003 &&
             std::all_of(owner.begin(), owner.end(),
               [](const Model::DecisionBucket& bucket) {
                   return bucket.worlds.count() == 1;
               }),
           "Ghost owner decision cells are not exact singleton pairs");

    const std::array<Model::PairWorld, 2> worlds{
      Model::PairWorld{square("a5"), square("c7")},
      Model::PairWorld{square("h5"), square("c7")}};
    const std::array<std::pair<std::uint8_t, std::uint8_t>, 2> endpoints{
      std::pair{square("a5"), square("a6")},
      std::pair{square("h5"), square("h6")}};
    std::array<std::string, 2> blackObservation;
    std::array<std::string, 2> whiteObservation;
    for (std::size_t index = 0; index < worlds.size(); ++index) {
        const Position source = Model::make_position(ownerFrame, worlds[index]);
        const Move move = require_move(
          source, endpoints[index].first, endpoints[index].second);
        Position child = source;
        Undo undo;
        expect(child.make_move(move, undo),
               "quiet hidden Ghost transition failed");
        blackObservation[index] = transition_observation_key(
          source, move, child, {Color::Black, false});
        whiteObservation[index] = transition_observation_key(
          source, move, child, {Color::White, false});
    }
    expect(blackObservation[0] == blackObservation[1],
           "quiet hidden Ghost pair leaked endpoints to Black");
    expect(whiteObservation[0] != whiteObservation[1],
           "Ghost owner lost exact pair correlation after a quiet move");
    std::cout << "ghost_pair_decision black_members " << observerMembers
              << " owner_singletons " << owner.size()
              << " hidden_transition_black_buckets 1 residual 0\n";
}

void lower_capture_projection_test() {
    Model::PublicFrame frame{
      Color::Black, square("a1"), square("e5"), {}, 1};
    frame.visibleGhosts[0] = square("e4");
    const std::array<const char*, 8> hiddenSquares{
      "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8"};
    std::vector<Model::PairWorld> worlds;
    for (const char* name : hiddenSquares)
        worlds.push_back({std::min(square("e4"), square(name)),
                          std::max(square("e4"), square(name))});
    const Position first = Model::make_position(frame, worlds.front());
    const Model::ActionKey capture = Model::action_key(require_move(
      first, square("e5"), square("e4")));
    const auto observerBuckets = Model::transition_partition(
      frame, worlds, capture, Color::Black);
    const auto ownerBuckets = Model::transition_partition(
      frame, worlds, capture, Color::White);
    expect(observerBuckets.size() == 1 &&
             observerBuckets.front().size() == hiddenSquares.size(),
           "visible Ghost capture lost correlated hidden companions");
    expect(ownerBuckets.size() == hiddenSquares.size() &&
             std::all_of(ownerBuckets.begin(), ownerBuckets.end(),
               [](const auto& bucket) { return bucket.size() == 1; }),
           "visible Ghost capture merged owner-known companions");
    std::vector<Model::ClassifiedChild> children;
    for (const Model::TransitionWorld& transition : observerBuckets.front()) {
        expect(transition.child.domain == Model::ChildDomain::LowerGhost,
               "one-Ghost capture did not lower to K+Ghost-v-K");
        children.push_back(transition.child);
    }
    const Model::LowerGhostImage image =
      Model::inherited_lower_ghost_image(children);
    expect(image.locations.count() == hiddenSquares.size(),
           "UFGM child projection lost an arbitrary companion square");
    for (const char* name : hiddenSquares)
        expect(image.locations.test(square(name)),
               "UFGM child projection changed a companion square");

    Model::PublicFrame terminalFrame = frame;
    terminalFrame.side = Color::White;
    const Model::PairWorld terminalWorld{
      std::min(square("e4"), square("h8")),
      std::max(square("e4"), square("h8"))};
    const Position source = Model::make_position(
      terminalFrame, terminalWorld);
    const Move kingCapture = require_move(
      source, square("e4"), square("e5"));
    Position terminal = source;
    Undo undo;
    expect(terminal.make_move(kingCapture, undo),
           "Ghost real-King capture failed");
    const Model::ClassifiedChild classified =
      Model::classify_child(terminal);
    expect(classified.domain == Model::ChildDomain::ExactTerminal &&
             classified.winner == Color::White,
           "real-King capture did not classify as exact terminal");
    std::cout << "ghost_pair_lower_projection mask "
              << image.locations.count()
              << " terminal_king_capture 1 residual 0\n";
}

void physical_fold_and_admission_test() {
    const Model::PublicFrame frame{
      Color::Black, square("g2"), square("d10"), {}, 0};
    const Model::PairWorld world{square("a5"), square("h5")};
    const Position source = Model::make_position(frame, world);
    const Model::ActionKey action = Model::action_key(require_move(
      source, square("d10"), square("d9")));
    const auto buckets = Model::transition_partition(
      frame, {world}, action, Color::Black);
    expect(buckets.size() == 1 && buckets.front().size() == 1 &&
             buckets.front().front().sameClassProduct.has_value(),
           "same-class transition omitted its physical pair coordinate");
    const Model::PublicFrame physical =
      buckets.front().front().sameClassProduct->frame;
    const Model::PublicFrame folded = Model::source_to_product(
      Model::decode_source(buckets.front().front().child.index)).frame;
    expect(!(physical == folded),
           "folded-source fixture did not exercise physical-frame separation");

    const Model::PublicFrame admissionFrame{
      Color::White, square("a1"), square("h10"), {}, 0};
    const std::vector<Model::PairWorld> geometric =
      Model::geometric_worlds(admissionFrame);
    const std::vector<Model::PairWorld> admitted =
      Model::admitted_fresh_worlds(admissionFrame);
    expect(admitted.size() == 2775 && geometric.size() == 3003,
           "hidden-hidden causal admission count is not exact");
    for (const Model::PairWorld& admittedWorld : admitted)
        expect(Model::fresh_world_admission(admissionFrame, admittedWorld) ==
                 Model::AdmissionVerdict::Admit,
               "admission enumeration retained a rejected pair");

    Model::PublicFrame mixed = admissionFrame;
    mixed.visibleGhosts[0] = square("d4");
    mixed.visibleCount = 1;
    expect(Model::admitted_fresh_worlds(mixed).size() == 74,
           "mixed-visibility causal admission tested the wrong companion");
    Model::PublicFrame visible = admissionFrame;
    visible.visibleGhosts = {square("d4"), square("g9")};
    visible.visibleCount = 2;
    const Model::PairWorld visibleWorld{square("d4"), square("g9")};
    expect(Model::fresh_world_admission(visible, visibleWorld) ==
             Model::AdmissionVerdict::Admit,
           "visible Ghost adjacency was rejected as hidden causality");
    std::cout << "ghost_pair_admission geometric " << geometric.size()
              << " admitted " << admitted.size()
              << " rejected " << geometric.size() - admitted.size()
              << " physical_fold_separate 1 residual 0\n";
}

}  // namespace
}  // namespace Stockfish::Ultimate

int main() {
    using namespace Stockfish::Ultimate;
    try {
        correlated_product_test();
        action_codec_test();
        complete_symmetry_test();
        decision_and_hidden_transition_test();
        lower_capture_projection_test();
        physical_fold_and_admission_test();
        exhaustive_codec_and_swap_test();
        std::cout << "ultimate Ghost-pair information model tests passed\n";
    }
    catch (const std::exception& error) {
        std::cerr << "ultimate Ghost-pair information model test failed: "
                  << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
