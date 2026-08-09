/*
  Standalone exact-contract tests for jester_ghost_information_model.

  Compile directly while the AWS-bound build graph remains frozen:

    c++ -std=c++17 -O3 -Isrc/ultimate \
      src/ultimate/position.cpp src/ultimate/information.cpp \
      src/ultimate/nnue.cpp \
      src/ultimate/jester_ghost_information_model.cpp \
      tests/ultimate_jester_ghost_information_model.cpp \
      -o /tmp/ultimate_jester_ghost_information_model_test
*/

#include "information.h"
#include "jester_ghost_information_model.h"

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

namespace Model = JesterGhostInformation;

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
    fail("required native Jester/Ghost move is absent");
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
            fail("Jester/Ghost source codec residual at " +
                 std::to_string(index));
        const Model::FramedWorld product =
          Model::source_to_product(decoded);
        const unsigned variable =
          Model::product_variable(product.frame, product.world);
        if (!(Model::decode_product_variable(product.frame, variable) ==
              product.world))
            fail("Jester/Ghost product variable residual at " +
                 std::to_string(index));
        const Model::ConcreteState restored =
          Model::product_to_source(product.frame, product.world);
        if (!same_source(decoded, restored) ||
            Model::encode_source(restored) != index)
            fail("source/product physical-frame residual at " +
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
    std::cout << "jester_ghost_codec source_states " << Model::StateCount
              << " product_round_trips " << Model::StateCount
              << " lower_jester " << Model::LowerJesterStateCount
              << " lower_ghost " << Model::LowerGhostStateCount
              << " residual 0\n";
}

void complete_product_transform_test() {
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

    std::uint64_t assignmentCases = 0;
    for (std::uint8_t first = 0; first < Position::BoardSquares; ++first)
        for (std::uint8_t second = static_cast<std::uint8_t>(first + 1);
             second < Position::BoardSquares; ++second)
            for (const bool kingAtFirst : {true, false})
                for (const auto transform : transforms) {
                    Model::PublicFrame frame{
                      Color::White,
                      static_cast<std::uint8_t>(
                        first == 0 || second == 0 ? 1 : 0),
                      first, second, {}};
                    if (frame.blackKing == first ||
                        frame.blackKing == second)
                        frame.blackKing = static_cast<std::uint8_t>(
                          second == 2 || first == 2 ? 3 : 2);
                    std::uint8_t ghost = 0;
                    while (ghost == frame.blackKing || ghost == first ||
                           ghost == second)
                        ++ghost;
                    const Model::ProductWorld world{kingAtFirst, ghost};
                    const Model::FramedWorld mapped =
                      Model::transform_world(frame, world, transform);
                    const Model::FramedWorld restored =
                      Model::transform_world(
                        mapped.frame, mapped.world, transform);
                    expect(restored.frame == frame &&
                             restored.world == world,
                           "product-frame rectangle transform is not an involution");
                    const Model::CanonicalWorld ownerFirst =
                      Model::canonicalize_world(frame, world);
                    const Model::CanonicalWorld ownerSecond =
                      Model::canonicalize_world(
                        frame, {!kingAtFirst, ghost});
                    expect(ownerFirst.frame == ownerSecond.frame &&
                             ownerFirst.transform == ownerSecond.transform,
                           "canonical public transform depends on royal identity");
                    ++assignmentCases;
                }
    std::cout << "jester_ghost_product_transform royal_assignment_cases "
              << assignmentCases << " square_involutions "
              << Position::BoardSquares * transforms.size()
              << " residual 0\n";
}

void product_cardinality_test() {
    const Model::PublicFrame hidden{
      Color::Black, square("h10"), square("a1"), square("c1"), {}};
    const std::vector<Model::ProductWorld> hiddenWorlds =
      Model::geometric_worlds(hidden);
    expect(hiddenWorlds.size() == 154,
           "hidden Jester/Ghost product is not exactly 2*77");
    Model::ProductMask complete;
    for (const Model::ProductWorld& world : hiddenWorlds)
        complete.set(Model::product_variable(hidden, world));
    expect(complete.count() == 154,
           "160-bit product mask lost a valid hidden world");
    expect(Model::decode_product_mask(hidden, complete).size() == 154,
           "product-mask codec lost a valid hidden world");

    const std::array<Model::RectangleTransform, 4> transforms{
      Model::RectangleTransform::Identity,
      Model::RectangleTransform::Horizontal,
      Model::RectangleTransform::Vertical,
      Model::RectangleTransform::Both};
    Model::ProductMask sparse;
    for (const Model::ProductWorld& world : hiddenWorlds)
        if ((world.ghost + unsigned(world.kingAtFirst)) % 5 == 0)
            sparse.set(Model::product_variable(hidden, world));
    for (const auto transform : transforms) {
        const Model::ProductSet mapped =
          Model::transform_set(hidden, sparse, transform);
        const Model::ProductSet restored = Model::transform_set(
          mapped.frame, mapped.worlds, transform);
        expect(restored == Model::ProductSet{hidden, sparse},
               "exact product-set transform is not an involution");
    }
    const Model::CanonicalSet canonical =
      Model::canonicalize_set(hidden, sparse);
    expect(canonical.value.worlds.count() == sparse.count(),
           "product-set canonicalization did not conserve worlds");

    Model::PublicFrame visible = hidden;
    visible.visibleGhost = square("f6");
    const std::vector<Model::ProductWorld> visibleWorlds =
      Model::geometric_worlds(visible);
    expect(visibleWorlds.size() == 2,
           "visible Ghost frame does not contain exactly two royal worlds");
    std::cout << "jester_ghost_product_cardinality hidden 154 visible 2"
              << " mask_variables " << Model::ProductVariables
              << " residual 0\n";
}

void action_codec_test() {
    const std::array<Model::RectangleTransform, 4> transforms{
      Model::RectangleTransform::Identity,
      Model::RectangleTransform::Horizontal,
      Model::RectangleTransform::Vertical,
      Model::RectangleTransform::Both};
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
    const std::set<Model::ActionKey> unique(actions.begin(), actions.end());
    expect(unique.size() == actions.size(),
           "complete action key merges distinct engine actions");
    for (const Model::ActionKey& action : actions)
        for (const auto transform : transforms)
            expect(Model::transform_action(
                     Model::transform_action(action, transform), transform) ==
                     action,
                   "complete action transform is not an involution");

    bool rejectedAmbiguousAuxiliary = false;
    try {
        (void)Model::transform_action(
          {square("b2"), square("c3"), square("b3"), MoveKind::Normal,
           PieceType::Count}, Model::RectangleTransform::Horizontal);
    }
    catch (const std::invalid_argument&) {
        rejectedAmbiguousAuxiliary = true;
    }
    expect(rejectedAmbiguousAuxiliary,
           "unclassified ordinary auxiliary did not fail closed");
    std::cout << "jester_ghost_action_codec identities " << actions.size()
              << " transforms " << transforms.size() << " residual 0\n";
}

[[nodiscard]] bool equivalent_partition(
  const std::vector<std::optional<std::string>>& source,
  const std::vector<std::optional<std::string>>& target) {
    if (source.size() != target.size())
        return false;
    std::map<std::string, std::string> forward;
    std::map<std::string, std::string> reverse;
    for (std::size_t index = 0; index < source.size(); ++index) {
        const std::string sourceKey = source[index]
          ? "value:" + *source[index] : "absent";
        const std::string targetKey = target[index]
          ? "value:" + *target[index] : "absent";
        const auto [first, insertedFirst] =
          forward.emplace(sourceKey, targetKey);
        if (!insertedFirst && first->second != targetKey)
            return false;
        const auto [second, insertedSecond] =
          reverse.emplace(targetKey, sourceKey);
        if (!insertedSecond && second->second != sourceKey)
            return false;
    }
    return true;
}

void private_decision_witness_test() {
    const Model::PublicFrame frame{
      Color::Black, square("e7"), square("e5"), square("e6"), {}};
    const std::uint8_t ghost = square("h10");
    const Model::ProductWorld kingE5{true, ghost};
    const Model::ProductWorld kingE6{false, ghost};
    const Position first = Model::make_position(frame, kingE5);
    const Position second = Model::make_position(frame, kingE6);
    expect(view_key(first, {Color::Black, false}) ==
             view_key(second, {Color::Black, false}),
           "royal assignments do not share the ordinary public view");
    expect(decision_observation_key(first, {Color::Black, false}) !=
             decision_observation_key(second, {Color::Black, false}),
           "known legal-dot royal witness failed to refine the observer");
    expect(!first.move_from_string("e7-e6").has_value() &&
             second.move_from_string("e7-e6").has_value(),
           "known e7-e6 legal-dot witness has reversed identities");
    const std::vector<Model::DecisionBucket> split =
      Model::decision_partition(frame, {kingE5, kingE6});
    expect(split.size() == 2 && split[0].worlds.count() == 1 &&
             split[1].worlds.count() == 1,
           "observer decision partition did not split dot-distinguishable worlds");

    Model::PublicFrame ownerFrame = frame;
    ownerFrame.side = Color::White;
    const std::vector<Model::ProductWorld> ownerWorlds =
      Model::geometric_worlds(ownerFrame);
    const std::vector<Model::DecisionBucket> owner =
      Model::decision_partition(ownerFrame, ownerWorlds);
    expect(owner.size() == ownerWorlds.size() &&
             std::all_of(owner.begin(), owner.end(),
               [](const Model::DecisionBucket& bucket) {
                   return bucket.worlds.count() == 1;
               }),
           "fully informed owner decision cells are not singleton");
    std::cout << "jester_ghost_private_decision witness_split 2"
              << " owner_singletons " << owner.size()
              << " nonmover_disclosure 0 residual 0\n";
}

void folded_source_physical_child_test() {
    const Model::PublicFrame frame{
      Color::Black, square("d10"), square("b2"), square("g2"), {}};
    const std::vector<Model::ProductWorld> worlds{
      {true, square("d5")}, {false, square("d5")}};
    const Position first = Model::make_position(frame, worlds.front());
    const Model::ActionKey action = Model::action_key(require_move(
      first, square("d10"), square("d9")));
    const auto buckets = Model::transition_partition(
      frame, worlds, action, Color::Black);
    expect(buckets.size() == 1 && buckets.front().size() == 2,
           "common quiet action unexpectedly disclosed a royal assignment");
    for (const Model::TransitionWorld& child : buckets.front())
        expect(child.child.domain == Model::ChildDomain::SameClass &&
                 child.sameClassProduct.has_value(),
               "same-class transition omitted its physical product child");
    expect(buckets.front()[0].sameClassProduct->frame ==
             buckets.front()[1].sameClassProduct->frame,
           "one public successor was split into different physical frames");

    const Model::FramedWorld foldedFirst = Model::source_to_product(
      Model::decode_source(buckets.front()[0].child.index));
    const Model::FramedWorld foldedSecond = Model::source_to_product(
      Model::decode_source(buckets.front()[1].child.index));
    expect(!(foldedFirst.frame == foldedSecond.frame),
           "folded-source regression no longer exercises opposite reflections");
    std::cout << "jester_ghost_physical_child folded_frames 2"
              << " public_frames 1 residual 0\n";
}

void hidden_ghost_nonsignaling_test() {
    const Model::PublicFrame frame{
      Color::White, square("d10"), square("b2"), square("g2"), {}};
    const std::array<Model::ProductWorld, 2> worlds{
      Model::ProductWorld{true, square("a5")},
      Model::ProductWorld{true, square("h5")}};
    const std::array<std::pair<std::uint8_t, std::uint8_t>, 2> endpoints{
      std::pair{square("a5"), square("a6")},
      std::pair{square("h5"), square("h6")}};
    std::array<std::string, 2> observer;
    std::array<std::string, 2> owner;
    for (std::size_t index = 0; index < worlds.size(); ++index) {
        const Position source = Model::make_position(frame, worlds[index]);
        const Move move = require_move(
          source, endpoints[index].first, endpoints[index].second);
        Position child = source;
        Undo undo;
        expect(child.make_move(move, undo),
               "quiet hidden Ghost witness failed to move");
        observer[index] = transition_observation_key(
          source, move, child, {Color::Black, false});
        owner[index] = transition_observation_key(
          source, move, child, {Color::White, false});
    }
    expect(observer[0] == observer[1],
           "quiet invisible Ghost endpoints leaked to the observer");
    expect(owner[0] != owner[1],
           "Ghost owner lost the exact private transition coordinate");
    std::cout << "jester_ghost_hidden_transition observer_buckets 1"
              << " owner_buckets 2 residual 0\n";
}

[[nodiscard]] std::vector<std::optional<std::string>> transition_labels(
  const Model::PublicFrame& frame,
  const std::vector<Model::ProductWorld>& worlds,
  const Model::ActionKey& action, Color observer) {
    std::vector<std::optional<std::string>> result(worlds.size());
    std::map<unsigned, std::size_t> byVariable;
    for (std::size_t index = 0; index < worlds.size(); ++index)
        byVariable.emplace(Model::product_variable(frame, worlds[index]), index);
    const auto buckets = Model::transition_partition(
      frame, worlds, action, observer);
    for (std::size_t bucket = 0; bucket < buckets.size(); ++bucket)
        for (const Model::TransitionWorld& transition : buckets[bucket]) {
            const auto found = byVariable.find(
              Model::product_variable(frame, transition.source));
            expect(found != byVariable.end(),
                   "transition returned a world outside its source belief");
            result[found->second] = "bucket:" + std::to_string(bucket);
        }
    return result;
}

void symmetry_frame_test(Model::PublicFrame frame,
                         Model::RectangleTransform transform) {
    const std::vector<Model::ProductWorld> worlds =
      Model::geometric_worlds(frame);
    std::vector<Model::FramedWorld> mappedWorlds;
    mappedWorlds.reserve(worlds.size());
    for (const Model::ProductWorld& world : worlds)
        mappedWorlds.push_back(
          Model::transform_world(frame, world, transform));
    const Model::PublicFrame targetFrame = mappedWorlds.front().frame;
    expect(std::all_of(mappedWorlds.begin(), mappedWorlds.end(),
             [&](const Model::FramedWorld& world) {
                 return world.frame == targetFrame;
             }),
           "one public frame transformed into multiple public frames");

    std::set<Model::ActionKey> actionUniverse;
    std::vector<std::optional<std::string>> decisions;
    std::vector<std::optional<std::string>> targetDecisions;
    decisions.reserve(worlds.size());
    targetDecisions.reserve(worlds.size());
    for (std::size_t index = 0; index < worlds.size(); ++index) {
        const Position source = Model::make_position(frame, worlds[index]);
        const Position target = Model::make_position(
          targetFrame, mappedWorlds[index].world);
        const std::vector<Model::ActionKey> sourceActions =
          Model::legal_actions(source);
        std::vector<Model::ActionKey> mappedActions;
        for (const Model::ActionKey& action : sourceActions) {
            actionUniverse.insert(action);
            mappedActions.push_back(
              Model::transform_action(action, transform));
        }
        std::sort(mappedActions.begin(), mappedActions.end());
        expect(mappedActions == Model::legal_actions(target),
               "complete action frontier changed under product symmetry");
        decisions.push_back(decision_observation_key(
          source, {frame.side, false}));
        targetDecisions.push_back(decision_observation_key(
          target, {targetFrame.side, false}));
    }
    expect(equivalent_partition(decisions, targetDecisions),
           "mover-private decision partition changed under product symmetry");

    std::uint64_t transitionMemberships = 0;
    for (const Model::ActionKey& action : actionUniverse) {
        const Model::ActionKey mapped =
          Model::transform_action(action, transform);
        for (const Color observer : {Color::White, Color::Black}) {
            const auto sourceLabels = transition_labels(
              frame, worlds, action, observer);
            const auto targetLabels = transition_labels(
              targetFrame,
              [&] {
                  std::vector<Model::ProductWorld> result;
                  for (const Model::FramedWorld& world : mappedWorlds)
                      result.push_back(world.world);
                  return result;
              }(), mapped, observer);
            expect(equivalent_partition(sourceLabels, targetLabels),
                   "transition-observation partition changed under product symmetry");
            transitionMemberships += static_cast<std::uint64_t>(
              std::count_if(sourceLabels.begin(), sourceLabels.end(),
                [](const auto& value) { return value.has_value(); }));
        }
    }
    std::cout << "jester_ghost_symmetry transform "
              << static_cast<int>(transform)
              << " side " << static_cast<int>(frame.side)
              << " visible " << frame.visibleGhost.has_value()
              << " worlds " << worlds.size()
              << " actions " << actionUniverse.size()
              << " transition_memberships " << transitionMemberships
              << " action_residual 0 decision_residual 0"
              << " transition_residual 0\n";
}

void complete_symmetry_test() {
    const std::array<Model::RectangleTransform, 4> transforms{
      Model::RectangleTransform::Identity,
      Model::RectangleTransform::Horizontal,
      Model::RectangleTransform::Vertical,
      Model::RectangleTransform::Both};
    for (const Color side : {Color::White, Color::Black})
        for (const bool visible : {false, true}) {
            Model::PublicFrame frame{
              side, square("g9"), square("b2"), square("d4"), {}};
            if (visible)
                frame.visibleGhost = square("f6");
            for (const auto transform : transforms)
                symmetry_frame_test(frame, transform);
        }
}

void lower_jester_pair_test() {
    const Model::PublicFrame frame{
      Color::Black, square("e5"), square("a1"), square("c1"),
      square("e4")};
    std::vector<Model::ClassifiedChild> children;
    std::vector<Model::ProductWorld> worlds;
    std::optional<std::string> publicObservation;
    for (const bool kingAtFirst : {true, false}) {
        const Model::ProductWorld world{kingAtFirst, square("e4")};
        worlds.push_back(world);
        const Position source = Model::make_position(frame, world);
        const Move capture = require_move(
          source, square("e5"), square("e4"));
        Position child = source;
        Undo undo;
        expect(child.make_move(capture, undo),
               "visible Ghost capture failed");
        const std::string observation = transition_observation_key(
          source, capture, child, {Color::Black, false});
        if (!publicObservation)
            publicObservation = observation;
        else
            expect(*publicObservation == observation,
                   "Ghost capture disclosed a hidden royal assignment");
        const Model::ClassifiedChild classified =
          Model::classify_child(child);
        expect(classified.domain == Model::ChildDomain::LowerJester,
               "Ghost capture did not lower to K+Jester-v-K");
        children.push_back(classified);
    }
    const Model::ActionKey captureAction = Model::action_key(require_move(
      Model::make_position(frame, worlds.front()), square("e5"), square("e4")));
    const auto observerBuckets = Model::transition_partition(
      frame, worlds, captureAction, Color::Black);
    const auto ownerBuckets = Model::transition_partition(
      frame, worlds, captureAction, Color::White);
    expect(observerBuckets.size() == 1 && observerBuckets.front().size() == 2,
           "Ghost capture did not preserve the observer's royal pair");
    expect(ownerBuckets.size() == 2 && ownerBuckets[0].size() == 1 &&
             ownerBuckets[1].size() == 1,
           "Ghost capture merged the owner's concrete royal identities");
    const Model::LowerJesterSet pair =
      Model::inherited_lower_jester_set(children);
    const Model::LowerJesterSet singleton =
      Model::inherited_lower_jester_set({children.front()});
    expect(pair.cardinality == 2 && singleton.cardinality == 1,
           "lower Jester pair/singleton cardinality was remaximized");
    expect(pair.concrete[0] != pair.concrete[1],
           "lower Jester pair duplicates one concrete assignment");
    std::cout << "jester_ghost_lower_jester pair 1 singleton 1"
              << " fresh_remaximized 0 residual 0\n";
}

void lower_ghost_and_terminal_test() {
    const std::array<const char*, 8> ghostSquares{
      "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8"};
    std::vector<Model::ClassifiedChild> lower;
    std::vector<Model::ProductWorld> worlds;
    const Model::PublicFrame frame{
      Color::Black, square("e5"), square("a1"), square("e4"), {}};
    std::optional<std::string> publicObservation;
    for (const char* ghostName : ghostSquares) {
        const Model::ProductWorld world{true, square(ghostName)};
        worlds.push_back(world);
        const Position source = Model::make_position(frame, world);
        const Move capture = require_move(
          source, square("e5"), square("e4"));
        Position child = source;
        Undo undo;
        expect(child.make_move(capture, undo),
               "Jester capture failed");
        const std::string observation = transition_observation_key(
          source, capture, child, {Color::Black, false});
        if (!publicObservation)
            publicObservation = observation;
        else
            expect(*publicObservation == observation,
                   "Jester capture disclosed a hidden Ghost square");
        const Model::ClassifiedChild classified =
          Model::classify_child(child);
        expect(classified.domain == Model::ChildDomain::LowerGhost,
               "Jester capture did not lower to K+Ghost-v-K");
        lower.push_back(classified);
    }
    const Model::ActionKey captureAction = Model::action_key(require_move(
      Model::make_position(frame, worlds.front()), square("e5"), square("e4")));
    const auto observerBuckets = Model::transition_partition(
      frame, worlds, captureAction, Color::Black);
    const auto ownerBuckets = Model::transition_partition(
      frame, worlds, captureAction, Color::White);
    expect(observerBuckets.size() == 1 &&
             observerBuckets.front().size() == ghostSquares.size(),
           "Jester capture did not preserve the arbitrary hidden Ghost image");
    expect(ownerBuckets.size() == ghostSquares.size() &&
             std::all_of(ownerBuckets.begin(), ownerBuckets.end(),
               [](const auto& bucket) { return bucket.size() == 1; }),
           "Jester capture merged the owner's concrete Ghost locations");
    const Model::LowerGhostImage image =
      Model::inherited_lower_ghost_image(lower);
    expect(image.locations.count() == ghostSquares.size(),
           "Jester capture lost a history-refined Ghost world");
    for (const char* ghostName : ghostSquares)
        expect(image.locations.test(square(ghostName)),
               "Jester capture changed a lower Ghost coordinate");

    const Model::ProductWorld kingOnTarget{false, square("h8")};
    const Position source = Model::make_position(
      frame, kingOnTarget);
    const Move capture = require_move(
      source, square("e5"), square("e4"));
    Position terminal = source;
    Undo undo;
    expect(terminal.make_move(capture, undo),
           "real-King capture failed");
    const Model::ClassifiedChild classified =
      Model::classify_child(terminal);
    expect(classified.domain == Model::ChildDomain::ExactTerminal &&
             classified.winner == Color::Black,
           "real-King capture did not classify as an exact Black win");
    const auto terminalBuckets = Model::transition_partition(
      frame, {worlds.back(), kingOnTarget}, captureAction, Color::Black);
    expect(terminalBuckets.size() == 2,
           "terminal royal capture was merged with a live Jester capture");
    std::cout << "jester_ghost_lower_ghost memberships "
              << image.locations.count()
              << " king_capture_terminal 1 residual 0\n";
}

void admission_test() {
    const Model::PublicFrame frame{
      Color::White, square("h10"), square("a1"), square("c1"), {}};
    const std::vector<Model::ProductWorld> geometric =
      Model::geometric_worlds(frame);
    const std::vector<Model::ProductWorld> admitted =
      Model::admitted_fresh_worlds(frame);
    expect(!admitted.empty() && admitted.size() <= geometric.size(),
           "fresh Jester/Ghost admission returned an invalid cardinality");
    std::set<unsigned> admittedVariables;
    for (const Model::ProductWorld& world : admitted) {
        expect(Model::fresh_world_admission(frame, world) ==
                 Model::AdmissionVerdict::Admit,
               "admitted fresh world fails its admission predicate");
        expect(std::abs(int(world.ghost % Position::BoardFiles) -
                        int(frame.blackKing % Position::BoardFiles)) > 1 ||
                 std::abs(int(world.ghost / Position::BoardFiles) -
                          int(frame.blackKing / Position::BoardFiles)) > 1,
               "hidden Ghost remained adjacent to the observing King");
        admittedVariables.insert(Model::product_variable(frame, world));
    }
    unsigned classified = 0;
    for (const Model::ProductWorld& world : geometric) {
        const bool isAdmitted = Model::fresh_world_admission(frame, world) ==
                                Model::AdmissionVerdict::Admit;
        expect(isAdmitted == admittedVariables.count(
                 Model::product_variable(frame, world)),
               "fresh admission enumeration and predicate disagree");
        ++classified;
    }
    expect(classified == 154,
           "fresh admission did not classify the complete product frame");
    std::cout << "jester_ghost_admission geometric " << geometric.size()
              << " admitted " << admitted.size()
              << " classified " << classified
              << " unresolved 0 residual 0\n";
}

}  // namespace
}  // namespace Stockfish::Ultimate

int main() {
    using namespace Stockfish::Ultimate;
    try {
        exhaustive_codec_test();
        complete_product_transform_test();
        product_cardinality_test();
        action_codec_test();
        private_decision_witness_test();
        folded_source_physical_child_test();
        hidden_ghost_nonsignaling_test();
        complete_symmetry_test();
        lower_jester_pair_test();
        lower_ghost_and_terminal_test();
        admission_test();
        std::cout << "ultimate Jester/Ghost information model tests passed\n";
    }
    catch (const std::exception& error) {
        std::cerr << "ultimate Jester/Ghost information model test failed: "
                  << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
