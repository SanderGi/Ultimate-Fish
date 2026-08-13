/*
  Standalone exact-contract tests for ghost_public_extra_model.

  Compile directly while the AWS-bound build graph remains frozen:

    c++ -std=c++17 -O3 -Isrc/ultimate \
      src/ultimate/position.cpp src/ultimate/tablebases/information.cpp \
      src/ultimate/nnue.cpp \
      src/ultimate/tablebases/ghost_public_extra_model.cpp \
      tests/tablebases/ultimate_ghost_public_extra_model.cpp \
      -o /tmp/ultimate_ghost_public_extra_model_test
*/

#include "ghost_public_extra_model.h"
#include "information.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace Stockfish::Ultimate {
namespace {

namespace Model = GhostPublicExtra;

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

[[nodiscard]] bool same_state(const Model::ConcreteState& lhs,
                              const Model::ConcreteState& rhs) {
    return lhs.side == rhs.side && lhs.whiteKing == rhs.whiteKing &&
           lhs.blackKing == rhs.blackKing && lhs.first == rhs.first &&
           lhs.second == rhs.second && lhs.extraSubstate == rhs.extraSubstate &&
           lhs.ghostVisible == rhs.ghostVisible;
}

void stateful_substate_smoke_test() {
    auto fixture = [](PieceType represented, PieceType logical,
                      std::uint32_t substate) {
        Position position;
        position.clear();
        const int whiteKing = position.add_piece(
          PieceType::King, Color::White, square("a1"));
        const int blackKing = position.add_piece(
          PieceType::King, Color::Black, square("h10"));
        const int extra = position.add_piece(
          represented, Color::White, square("d4"));
        expect(whiteKing != Position::NoPiece &&
                 blackKing != Position::NoPiece && extra != Position::NoPiece,
               "stateful fixture construction failed");
        expect(position.apply_tablebase_substate(extra, logical, substate),
               "stateful fixture substate application failed");
        const auto restored = position.tablebase_substate(extra, logical);
        expect(restored && *restored == substate,
               "stateful fixture substate did not round trip");
        return position;
    };

    (void)fixture(PieceType::Pawn, PieceType::Pawn, 1);
    (void)fixture(PieceType::Berserker, PieceType::Berserker, 9);
    (void)fixture(PieceType::Sniper, PieceType::Sniper, 3);
    const Position prince = fixture(PieceType::Prince, PieceType::Prince, 1);
    expect(prince.continuation() == Continuation::PrinceSecondMove,
           "Prince continuation was not reconstructed");
    const Position checker = fixture(
      PieceType::CheckerKing, PieceType::Checker, 3);
    expect(checker.continuation() == Continuation::CheckerJump,
           "Checker continuation was not reconstructed");
    Position penguin;
    penguin.clear();
    const int penguinWhite = penguin.add_piece(
      PieceType::King, Color::White, square("c3"));
    const int penguinBlack = penguin.add_piece(
      PieceType::King, Color::Black, square("h10"));
    const int penguinId = penguin.add_piece(
      PieceType::Penguin, Color::White, square("d4"));
    const int penguinGhost = penguin.add_piece(
      PieceType::Ghost, Color::White, square("e5"));
    expect(penguinWhite != Position::NoPiece &&
             penguinBlack != Position::NoPiece &&
             penguinId != Position::NoPiece &&
             penguinGhost != Position::NoPiece &&
             penguin.apply_tablebase_substate(
               penguinId, PieceType::Penguin, 5),
           "Penguin causal freeze fixture was rejected");
    expect(penguin.piece(penguinWhite).freezeCount == 1 &&
             penguin.piece(penguinGhost).freezeCount == 1 &&
             penguin.piece(penguinBlack).freezeCount == 0 &&
             penguin.tablebase_substate(
               penguinId, PieceType::Penguin) == 5u,
           "Penguin causal freeze layers did not round trip");
    expect(Model::tablebase_substate_geometrically_valid(
             PieceType::Penguin, square("c3"), square("h10"),
             square("d4"), square("e5"), 5) &&
             !Model::tablebase_substate_geometrically_valid(
               PieceType::Penguin, square("c3"), square("h10"),
               square("d4"), square("e5"), 2) &&
             !Model::tablebase_substate_geometrically_valid(
               PieceType::Penguin, square("c3"), square("h10"),
               square("d4"), square("e5"), 8),
           "Penguin dense-state geometry admitted an impossible freeze aura");

#if ULTIMATE_GHOST_EXTRA_SUBSTATES == 4
    Model::MaterialSpec material{
      PieceType::Checker, Color::White, Color::White,
      Model::SourceOrder::ExtraPrimary,
      Model::HiddenAdjacentPolicy::ImpossibleWithoutForcedRelocation,
      "kcheckerghostk"};
    Model::ConcreteState state{
      Color::White, square("a1"), square("h10"), square("d4"),
      square("f6"), 3, false};
    const std::uint32_t index = Model::encode_index(state, material);
    const Model::ConcreteState decoded = Model::decode_index(index, material);
    expect(same_state(state, decoded),
           "stateful public-extra dense codec lost Checker substate");
    expect(same_state(
             state, Model::denormalize_roles(
                      Model::normalize_roles(state, material), material)),
           "stateful role normalization lost Checker substate");
    const Position world = Model::make_position(decoded, material);
    int checkerId = Position::NoPiece;
    for (int id = 0; id < world.piece_count(); ++id)
        if (world.piece(id).type == PieceType::CheckerKing)
            checkerId = id;
    expect(checkerId != Position::NoPiece &&
             world.tablebase_substate(checkerId, PieceType::Checker) == 3u,
           "stateful world reconstruction lost CheckerKing/jump state");
    const Model::ConcreteState reflected = Model::transform_state(
      state, Model::RectangleTransform::Both);
    expect(reflected.extraSubstate == state.extraSubstate,
           "rectangle symmetry changed a public extra substate");
#endif
    std::cout << "ghost_public_extra_stateful_substates residual 0\n";
}

[[nodiscard]] Move require_move(const Position& position,
                                std::uint8_t from, std::uint8_t to,
                                MoveKind kind = MoveKind::Normal) {
    for (const Move& move : position.legal_moves())
        if (move.from == from && move.to == to && move.kind == kind)
            return move;
    fail("required native move is absent");
}

void mark_all_moved(Position& position) {
    for (int id = 0; id < position.piece_count(); ++id)
        if (position.piece(id).alive)
            position.piece(id).moved = true;
}

void exhaustive_codec_test() {
    const std::array<Model::MaterialSpec, 2> materials{
      Model::bishop_same(), Model::bishop_reciprocal()};
    for (const Model::MaterialSpec& material : materials) {
        for (std::uint32_t index = 0; index < Model::StateCount; ++index) {
            const Model::ConcreteState decoded =
              Model::decode_index(index, material);
            if (Model::encode_index(decoded, material) != index)
                fail(std::string(material.name) +
                     " concrete codec is not an involution at " +
                     std::to_string(index));
            const Model::RoleState roles =
              Model::normalize_roles(decoded, material);
            const Model::ConcreteState restored =
              Model::denormalize_roles(roles, material);
            if (!same_state(decoded, restored))
                fail(std::string(material.name) +
                     " owner/observer normalization is not lossless at " +
                     std::to_string(index));
            if ((material.ghostColor == Color::White) !=
                (roles.ownerKing == decoded.whiteKing))
                fail(std::string(material.name) +
                     " normalized the wrong royal as Ghost owner");
        }
    }

    for (std::uint32_t index = 0; index < Model::LowerGhostStateCount;
         ++index) {
        const Model::LowerGhostState state =
          Model::decode_lower_ghost(index);
        if (Model::encode_lower_ghost(state) != index)
            fail("lower K+Ghost codec is not an involution at " +
                 std::to_string(index));
    }
    std::cout << "ghost_public_extra_codec states "
              << std::uint64_t(Model::StateCount) * materials.size()
              << " lower_states " << Model::LowerGhostStateCount
              << " residual 0\n";
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

    std::vector<Model::ActionKey> actions{
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
    std::set<Model::ActionKey> unique(actions.begin(), actions.end());
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

    Model::ConcreteState materialFixture{
      Color::White, square("a1"), square("h10"), square("b2"),
      square("c3"), false};
    bool rejectedWrongPrimaryColor = false;
    try {
        Model::MaterialSpec invalid = Model::bishop_same();
        invalid.extraColor = Color::Black;
        (void)Model::encode_index(materialFixture, invalid);
    }
    catch (const std::invalid_argument&) {
        rejectedWrongPrimaryColor = true;
    }
    expect(rejectedWrongPrimaryColor,
           "source codec accepted a non-White primary piece");
    bool rejectedPointFoldedGiant = false;
    try {
        Model::MaterialSpec invalid = Model::bishop_same();
        invalid.extraType = PieceType::Giant;
        (void)Model::encode_index(materialFixture, invalid);
    }
    catch (const std::invalid_argument&) {
        rejectedPointFoldedGiant = true;
    }
    expect(rejectedPointFoldedGiant,
           "point-piece codec accepted a Giant anchor");
    std::cout << "ghost_public_extra_action_codec identities "
              << actions.size() << " transforms " << transforms.size()
              << " residual 0\n";
}

struct SymmetryWorld {
    Position source;
    Position transformed;
    std::vector<Move> sourceMoves;
    std::vector<Move> transformedMoves;
    std::vector<Model::ActionKey> sourceActions;
    std::vector<Model::ActionKey> transformedActions;
    std::vector<Model::DecisionMarker> sourceMarkers;
    std::vector<Model::DecisionMarker> transformedMarkers;
    std::string decisionKey;
    std::string transformedDecisionKey;
};

[[nodiscard]] bool same_optional_observation(
  const std::optional<std::string>& first,
  const std::optional<std::string>& second) {
    return first.has_value() == second.has_value() &&
           (!first || *first == *second);
}

void public_frame_symmetry_test(const Model::MaterialSpec& material,
                                Model::RectangleTransform transform,
                                Color side, bool visible) {
    Model::ConcreteState frame;
    frame.side = side;
    frame.whiteKing = square("b2");
    frame.blackKing = square("g9");
    frame = Model::with_piece_squares(
      frame, material, square("d4"), square("a1"));
    frame.ghostVisible = visible;

    const std::array<std::uint8_t, 3> occupied{
      frame.whiteKing, frame.blackKing, Model::extra_square(frame, material)};
    std::vector<SymmetryWorld> worlds;
    for (std::uint8_t ghost = 0; ghost < Position::BoardSquares; ++ghost) {
        if (std::find(occupied.begin(), occupied.end(), ghost) !=
            occupied.end())
            continue;
        Model::ConcreteState state = Model::with_piece_squares(
          frame, material, Model::extra_square(frame, material), ghost);
        const Model::ConcreteState transformedState =
          Model::transform_state(state, transform);
        SymmetryWorld world;
        world.source = Model::make_position(state, material);
        world.transformed = Model::make_position(transformedState, material);
        world.sourceMoves = world.source.legal_moves();
        world.transformedMoves = world.transformed.legal_moves();
        world.sourceActions = Model::legal_action_keys(world.source);
        world.transformedActions = Model::legal_action_keys(world.transformed);
        std::vector<Model::ActionKey> mapped;
        for (const Model::ActionKey& action : world.sourceActions)
            mapped.push_back(Model::transform_action(action, transform));
        std::sort(mapped.begin(), mapped.end());
        expect(mapped == world.transformedActions,
               std::string(material.name) +
               " complete legal actions are not rectangle symmetric");

        world.sourceMarkers = Model::decision_markers(world.source);
        world.transformedMarkers = Model::decision_markers(world.transformed);
        expect(Model::transform_markers(world.sourceMarkers, transform) ==
                 world.transformedMarkers,
               std::string(material.name) +
               " rendered decision markers are not rectangle symmetric");
        world.decisionKey = decision_observation_key(
          world.source, {frame.side, false});
        world.transformedDecisionKey = decision_observation_key(
          world.transformed, {frame.side, false});
        worlds.push_back(std::move(world));
    }
    expect(worlds.size() == Position::BoardSquares - occupied.size(),
           "public frame omitted a concrete Ghost world");

    // Coordinate strings differ after a reflection. The exact invariant is
    // preservation of the observation partition, not byte equality between
    // differently labelled boards.
    for (std::size_t first = 0; first < worlds.size(); ++first)
        for (std::size_t second = 0; second < worlds.size(); ++second)
            expect((worlds[first].decisionKey == worlds[second].decisionKey) ==
                     (worlds[first].transformedDecisionKey ==
                      worlds[second].transformedDecisionKey),
                   std::string(material.name) +
                   " private legal-dot partition changed under symmetry");

    std::set<Model::ActionKey> actionUniverse;
    for (const SymmetryWorld& world : worlds)
        actionUniverse.insert(world.sourceActions.begin(),
                              world.sourceActions.end());
    const std::array<Color, 2> observers{
      material.ghostColor, material.observer_color()};
    std::uint64_t transitions = 0;
    for (const Model::ActionKey& action : actionUniverse) {
        const Model::ActionKey mapped =
          Model::transform_action(action, transform);
        for (const Color observer : observers) {
            std::vector<std::optional<std::string>> sourceObservations(
              worlds.size());
            std::vector<std::optional<std::string>> transformedObservations(
              worlds.size());
            for (std::size_t index = 0; index < worlds.size(); ++index) {
                SymmetryWorld& world = worlds[index];
                const auto source = std::find_if(
                  world.sourceMoves.begin(), world.sourceMoves.end(),
                  [&](const Move& move) {
                      return Model::action_key(move) == action;
                  });
                const auto target = std::find_if(
                  world.transformedMoves.begin(), world.transformedMoves.end(),
                  [&](const Move& move) {
                      return Model::action_key(move) == mapped;
                  });
                expect((source == world.sourceMoves.end()) ==
                         (target == world.transformedMoves.end()),
                       std::string(material.name) +
                       " action legality changed under symmetry");
                if (source == world.sourceMoves.end())
                    continue;
                Position child = world.source;
                Position transformedChild = world.transformed;
                Undo undo;
                Undo transformedUndo;
                expect(child.make_move(*source, undo) &&
                         transformedChild.make_move(*target, transformedUndo),
                       "symmetric legal transition failed to apply");
                sourceObservations[index] = transition_observation_key(
                  world.source, *source, child, {observer, false});
                transformedObservations[index] = transition_observation_key(
                  world.transformed, *target, transformedChild,
                  {observer, false});
                ++transitions;
            }
            for (std::size_t first = 0; first < worlds.size(); ++first)
                for (std::size_t second = 0; second < worlds.size(); ++second) {
                    const bool sourceEqual = same_optional_observation(
                      sourceObservations[first], sourceObservations[second]);
                    const bool transformedEqual = same_optional_observation(
                      transformedObservations[first],
                      transformedObservations[second]);
                    expect(sourceEqual == transformedEqual,
                           std::string(material.name) +
                           " transition-observation partition changed under symmetry");
                }
        }
    }
    std::cout << "ghost_public_extra_symmetry material " << material.name
              << " transform " << static_cast<int>(transform)
              << " side " << static_cast<int>(side)
              << " visible " << visible
              << " worlds " << worlds.size()
              << " actions " << actionUniverse.size()
              << " transition_observations " << transitions
              << " action_residual 0 decision_residual 0"
              << " transition_residual 0\n";
}

void complete_symmetry_test() {
    const std::array<Model::MaterialSpec, 2> materials{
      Model::bishop_same(), Model::bishop_reciprocal()};
    const std::array<Model::RectangleTransform, 4> transforms{
      Model::RectangleTransform::Identity,
      Model::RectangleTransform::Horizontal,
      Model::RectangleTransform::Vertical,
      Model::RectangleTransform::Both};
    for (const Model::MaterialSpec& material : materials)
        for (const auto transform : transforms)
            for (const Color side : {Color::White, Color::Black})
                for (const bool visible : {false, true})
                    public_frame_symmetry_test(
                      material, transform, side, visible);
}

void inherited_lower_mask_test(const Model::MaterialSpec& material) {
    const std::array<const char*, 8> ghostSquares{
      "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8"};
    std::vector<Model::ClassifiedChild> children;
    std::optional<std::string> publicObservation;
    for (const char* ghostName : ghostSquares) {
        Model::ConcreteState state;
        state.side = Color::Black;
        state.whiteKing = square("a1");
        state.blackKing = square("e5");
        state = Model::with_piece_squares(
          state, material, square("e4"), square(ghostName));
        state.ghostVisible = false;
        Position position = Model::make_position(state, material);
        const Move capture = require_move(
          position, square("e5"), square("e4"));
        Position child = position;
        Undo undo;
        expect(child.make_move(capture, undo),
               "Bishop-capture lower fixture failed to apply");
        const std::string observation = transition_observation_key(
          position, capture, child, {material.observer_color(), false});
        if (!publicObservation)
            publicObservation = observation;
        else
            expect(*publicObservation == observation,
                   "one public Bishop-capture observation split hidden Ghost worlds");
        const Model::ClassifiedChild classified =
          Model::classify_child(child, material);
        expect(classified.domain == Model::ChildDomain::LowerGhost,
               "Bishop capture did not classify as K+Ghost lower material");
        children.push_back(classified);
    }
    const Model::LowerGhostImage image =
      Model::inherited_lower_ghost_image(children);
    expect(image.locations.count() == ghostSquares.size(),
           "history-preserving lower Ghost image lost a world");
    for (const char* ghostName : ghostSquares)
        expect(image.locations.test(square(ghostName)),
               "history-preserving lower Ghost image changed a square");
    expect(image.locations.count() > 1,
           "lower Ghost regression did not exercise an arbitrary mask");

    std::vector<Model::ClassifiedChild> mixed = children;
    Model::LowerGhostState different =
      Model::decode_lower_ghost(mixed.back().index);
    different.visible = !different.visible;
    mixed.back().index = Model::encode_lower_ghost(different);
    bool rejectedMixedGeometry = false;
    try {
        (void)Model::inherited_lower_ghost_image(mixed);
    }
    catch (const std::invalid_argument&) {
        rejectedMixedGeometry = true;
    }
    expect(rejectedMixedGeometry,
           "lower Ghost image accepted mixed public geometry");
    std::cout << "ghost_public_extra_lower_image material " << material.name
              << " memberships " << image.locations.count()
              << " fresh_remaximized 0 mixed_geometry_residual 0\n";
}

void bishop_admission_test() {
    {
        Model::ConcreteState state;
        state.side = Color::White;
        state.whiteKing = square("h1");
        state.blackKing = square("b2");
        state = Model::with_piece_squares(
          state, Model::bishop_same(), square("d4"), square("a2"));
        Position position = Model::make_position(state, Model::bishop_same());
        expect(Model::classify_fresh_root_admission(
                 position, Model::bishop_same()) ==
                 Model::FreshAdmissionVerdict::Reject,
               "same-side Bishop admitted an impossible hidden-adjacent Ghost");
    }
    {
        Model::ConcreteState state;
        state.side = Color::Black;
        state.whiteKing = square("a2");
        state.blackKing = square("h10");
        state = Model::with_piece_squares(
          state, Model::bishop_reciprocal(), square("d4"), square("b2"));
        Position position = Model::make_position(
          state, Model::bishop_reciprocal());
        expect(Model::classify_fresh_root_admission(
                 position, Model::bishop_reciprocal()) ==
                 Model::FreshAdmissionVerdict::Reject,
               "reciprocal Bishop tested adjacency against the wrong royal");
    }
    std::cout << "ghost_public_extra_bishop_admission same 1 reciprocal 1"
              << " residual 0\n";
}

Position mage_same_reachable_target() {
    Position position;
    position.add_piece(PieceType::King, Color::White, square("h1"));
    position.add_piece(PieceType::King, Color::Black, square("b2"));
    position.add_piece(PieceType::Mage, Color::White, square("a2"));
    const int ghost = position.add_piece(
      PieceType::Ghost, Color::White, square("d3"));
    position.piece(ghost).visible = false;
    mark_all_moved(position);
    position.set_side_to_move(Color::White);
    const Move swap = require_move(
      position, square("a2"), square("d3"), MoveKind::Swap);
    Undo undo;
    expect(position.make_move(swap, undo),
           "same-side Mage forced-relocation witness failed");
    return position;
}

Position mage_reciprocal_reachable_target() {
    Position position;
    position.add_piece(PieceType::King, Color::White, square("h10"));
    position.add_piece(PieceType::King, Color::Black, square("d3"));
    position.add_piece(PieceType::Mage, Color::Black, square("a2"));
    const int ghost = position.add_piece(
      PieceType::Ghost, Color::White, square("b2"));
    position.piece(ghost).visible = false;
    mark_all_moved(position);
    position.set_side_to_move(Color::Black);
    const Move swap = require_move(
      position, square("a2"), square("d3"), MoveKind::Swap);
    Undo undo;
    expect(position.make_move(swap, undo),
           "reciprocal Mage forced-royal witness failed");
    return position;
}

Position fisherman_reachable_target(bool sameSide) {
    Position position;
    position.add_piece(PieceType::King, Color::White, square("h1"));
    position.add_piece(PieceType::King, Color::Black, square("d4"));
    const Color fishermanColor = sameSide ? Color::White : Color::Black;
    position.add_piece(
      PieceType::Fisherman, fishermanColor, square("d1"));
    const int ghost = position.add_piece(
      PieceType::Ghost, Color::White, square("c2"));
    position.piece(ghost).visible = false;
    mark_all_moved(position);
    position.set_side_to_move(fishermanColor);
    const Move pull = require_move(
      position, square("d1"), square("d4"), MoveKind::Pull);
    Undo undo;
    expect(position.make_move(pull, undo),
           "Fisherman forced-royal witness failed");
    return position;
}

void forced_relocation_admission_contract_test() {
    const std::array<std::pair<Model::MaterialSpec, Position>, 4> fixtures{
      std::pair{Model::mage_same_contract(), mage_same_reachable_target()},
      std::pair{Model::mage_reciprocal_contract(),
                mage_reciprocal_reachable_target()},
      std::pair{Model::fisherman_same_contract(),
                fisherman_reachable_target(true)},
      std::pair{Model::fisherman_reciprocal_contract(),
                fisherman_reachable_target(false)}};
    for (const auto& [material, position] : fixtures) {
        const auto child = Model::classify_child(position, material);
        expect(child.domain == Model::ChildDomain::SameClass,
               std::string(material.name) +
               " reachable forced-relocation target left its material class");
        expect(Model::classify_fresh_root_admission(position, material) ==
                 Model::FreshAdmissionVerdict::NeedsExactCausalAudit,
               std::string(material.name) +
               " did not fail closed into its exact causal admission hook");
    }
    std::cout << "ghost_public_extra_forced_relocation_contracts mage 2"
              << " fisherman 2 reachable_witnesses 4 blanket_admission 0"
              << " residual 0\n";
}

}  // namespace
}  // namespace Stockfish::Ultimate

int main(int argc, char** argv) {
    using namespace Stockfish::Ultimate;
    try {
        if (argc == 2 && std::string(argv[1]) == "--stateful-smoke") {
            stateful_substate_smoke_test();
            return EXIT_SUCCESS;
        }
        if (argc != 1)
            throw std::invalid_argument("unknown test argument");
        exhaustive_codec_test();
        stateful_substate_smoke_test();
        action_codec_test();
        complete_symmetry_test();
        inherited_lower_mask_test(
          GhostPublicExtra::bishop_same());
        inherited_lower_mask_test(
          GhostPublicExtra::bishop_reciprocal());
        bishop_admission_test();
        forced_relocation_admission_contract_test();
        std::cout << "ultimate ghost public-extra model tests passed\n";
    }
    catch (const std::exception& error) {
        std::cerr << "ultimate ghost public-extra model test failed: "
                  << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
