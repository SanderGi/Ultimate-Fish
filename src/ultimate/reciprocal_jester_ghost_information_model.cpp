/*
  Ultimate Fish - exact K+Jester versus K+Ghost information model
  Copyright (C) 2026 Ultimate Fish contributors
  GPLv3 or later.
*/

#include "reciprocal_jester_ghost_information_model.h"

#include "information.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

namespace Stockfish::Ultimate::ReciprocalJesterGhostInformation {
namespace {

struct LiveMaterial {
    int whiteKing = Position::NoSquare;
    int blackKing = Position::NoSquare;
    int jester = Position::NoSquare;
    int ghost = Position::NoSquare;
    bool visible = false;
    int live = 0;
    bool unexpected = false;
};

[[nodiscard]] LiveMaterial scan_live(const Position& position) {
    LiveMaterial result;
    for (int id = 0; id < position.piece_count(); ++id) {
        const PieceState& piece = position.piece(id);
        if (!piece.alive || !piece.onBoard)
            continue;
        ++result.live;
        if (piece.type == PieceType::King && piece.color == Color::White &&
            result.whiteKing == Position::NoSquare)
            result.whiteKing = piece.square;
        else if (piece.type == PieceType::King &&
                 piece.color == Color::Black &&
                 result.blackKing == Position::NoSquare)
            result.blackKing = piece.square;
        else if (piece.type == PieceType::Jester &&
                 piece.color == Color::White &&
                 result.jester == Position::NoSquare)
            result.jester = piece.square;
        else if (piece.type == PieceType::Ghost &&
                 piece.color == Color::Black &&
                 result.ghost == Position::NoSquare) {
            result.ghost = piece.square;
            result.visible = piece.visible;
        }
        else
            result.unexpected = true;
    }
    return result;
}

[[nodiscard]] bool adjacent(std::uint8_t first, std::uint8_t second) {
    return std::abs(int(first % Position::BoardFiles) -
                    int(second % Position::BoardFiles)) <= 1 &&
           std::abs(int(first / Position::BoardFiles) -
                    int(second / Position::BoardFiles)) <= 1;
}

[[nodiscard]] std::optional<Move> locate_action(
  const Position& position, const ActionKey& action) {
    std::optional<Move> result;
    for (const Move& move : position.legal_moves())
        if (action_key(move) == action) {
            if (result)
                throw std::runtime_error(
                  "one reciprocal action maps to multiple legal moves");
            result = move;
        }
    return result;
}

[[nodiscard]] std::string private_fact(Color observer,
                                       const ProductWorld& world) {
    if (observer == Color::White)
        return std::string("royal=") + (world.kingAtFirst ? '0' : '1');
    return "ghost=" + std::to_string(world.ghost);
}

[[nodiscard]] std::string owned_observation(
  Color observer, const ProductWorld& world, const std::string& observation) {
    const std::string fact = private_fact(observer, world);
    return std::to_string(fact.size()) + ':' + fact + '|' + observation;
}

}  // namespace

Position make_position(const PublicFrame& frame, const ProductWorld& world) {
    // product_variable performs the shared collision/frame validation without
    // importing the same-side material ownership assumptions.
    (void)product_variable(frame, world);
    const ConcreteState state = JesterGhostInformation::product_to_source(
      frame, world);
    Position position;
    position.clear();
    const std::array<int, 4> pieces{{
      position.add_piece(PieceType::King, Color::White, state.whiteKing),
      position.add_piece(PieceType::King, Color::Black, state.blackKing),
      position.add_piece(PieceType::Jester, Color::White, state.jester),
      position.add_piece(PieceType::Ghost, Color::Black, state.ghost)}};
    if (std::any_of(pieces.begin(), pieces.end(), [](int id) {
            return id == Position::NoPiece;
        }))
        throw std::runtime_error(
          "cannot construct reciprocal Jester/Ghost product world");
    for (const int id : pieces)
        position.piece(id).moved = true;
    position.piece(pieces.back()).visible = state.ghostVisible;
    position.set_side_to_move(state.side);
    return position;
}

std::vector<DecisionBucket> decision_partition(
  const PublicFrame& frame, const std::vector<ProductWorld>& worlds) {
    std::map<std::string, ProductMask> grouped;
    std::set<unsigned> seen;
    for (const ProductWorld& world : worlds) {
        const unsigned variable = product_variable(frame, world);
        if (!seen.insert(variable).second)
            throw std::invalid_argument(
              "reciprocal decision partition contains a duplicate world");
        const Position position =
          ReciprocalJesterGhostInformation::make_position(frame, world);
        const std::string decision = decision_observation_key(
          position, {frame.side, false});
        grouped[owned_observation(frame.side, world, decision)].set(variable);
    }
    std::vector<DecisionBucket> result;
    result.reserve(grouped.size());
    unsigned conserved = 0;
    for (auto& [observation, mask] : grouped) {
        conserved += mask.count();
        result.push_back({std::move(observation), mask});
    }
    if (conserved != worlds.size())
        throw std::runtime_error(
          "reciprocal mover-private partition does not conserve worlds");
    return result;
}

ClassifiedChild classify_child(const Position& position) {
    const LiveMaterial live = scan_live(position);
    if (!live.unexpected && live.live == 4 &&
        live.whiteKing != Position::NoSquare &&
        live.blackKing != Position::NoSquare &&
        live.jester != Position::NoSquare &&
        live.ghost != Position::NoSquare) {
        const ConcreteState state{
          position.side_to_move(),
          static_cast<std::uint8_t>(live.whiteKing),
          static_cast<std::uint8_t>(live.blackKing),
          static_cast<std::uint8_t>(live.jester),
          static_cast<std::uint8_t>(live.ghost), live.visible};
        return {ChildDomain::SameClass, encode_source(state), {}};
    }
    if (!live.unexpected && live.live == 3 &&
        live.whiteKing != Position::NoSquare &&
        live.blackKing != Position::NoSquare &&
        live.jester != Position::NoSquare &&
        live.ghost == Position::NoSquare) {
        const JesterGhostInformation::LowerJesterState state{
          position.side_to_move(),
          static_cast<std::uint8_t>(live.whiteKing),
          static_cast<std::uint8_t>(live.blackKing),
          static_cast<std::uint8_t>(live.jester)};
        return {ChildDomain::LowerJester,
                JesterGhostInformation::encode_lower_jester(state), {}};
    }
    if (!live.unexpected && live.live == 3 &&
        live.whiteKing != Position::NoSquare &&
        live.blackKing != Position::NoSquare &&
        live.jester == Position::NoSquare &&
        live.ghost != Position::NoSquare) {
        const LowerGhostState state{
          position.side_to_move() == Color::Black
            ? Role::GhostOwner : Role::Observer,
          static_cast<std::uint8_t>(live.blackKing),
          static_cast<std::uint8_t>(live.whiteKing),
          static_cast<std::uint8_t>(live.ghost), live.visible};
        return {ChildDomain::LowerGhost, encode_lower_ghost(state), {}};
    }
    if (position.game_over())
        return {ChildDomain::ExactTerminal, 0, position.winner()};
    return {ChildDomain::Invalid, 0, {}};
}

std::optional<FramedWorld> same_class_product(const Position& position) {
    const LiveMaterial live = scan_live(position);
    if (live.unexpected || live.live != 4 ||
        live.whiteKing == Position::NoSquare ||
        live.blackKing == Position::NoSquare ||
        live.jester == Position::NoSquare ||
        live.ghost == Position::NoSquare)
        return std::nullopt;
    return source_to_product({
      position.side_to_move(),
      static_cast<std::uint8_t>(live.whiteKing),
      static_cast<std::uint8_t>(live.blackKing),
      static_cast<std::uint8_t>(live.jester),
      static_cast<std::uint8_t>(live.ghost), live.visible});
}

std::vector<std::vector<TransitionWorld>> transition_partition(
  const PublicFrame& frame, const std::vector<ProductWorld>& worlds,
  const ActionKey& action, Color observer) {
    std::map<std::string, std::vector<TransitionWorld>> grouped;
    std::set<unsigned> seen;
    for (const ProductWorld& world : worlds) {
        const unsigned variable = product_variable(frame, world);
        if (!seen.insert(variable).second)
            throw std::invalid_argument(
              "reciprocal transition partition contains a duplicate world");
        const Position position =
          ReciprocalJesterGhostInformation::make_position(frame, world);
        const std::optional<Move> move = locate_action(position, action);
        if (!move)
            continue;
        Position child = position;
        Undo undo;
        if (!child.make_move(*move, undo))
            throw std::runtime_error(
              "complete reciprocal Jester/Ghost action failed to apply");
        const std::string publicObservation = transition_observation_key(
          position, *move, child, {observer, false});
        const std::string observation = owned_observation(
          observer, world, publicObservation);
        const ClassifiedChild classified = classify_child(child);
        std::optional<FramedWorld> physical;
        if (classified.domain == ChildDomain::SameClass) {
            physical = same_class_product(child);
            if (!physical)
                throw std::runtime_error(
                  "same-class reciprocal child lost its product coordinate");
        }
        grouped[observation].push_back(
          {world, observation, classified, physical});
    }
    std::vector<std::vector<TransitionWorld>> result;
    result.reserve(grouped.size());
    for (auto& [observation, bucket] : grouped) {
        (void)observation;
        result.push_back(std::move(bucket));
    }
    return result;
}

AdmissionVerdict fresh_world_admission(const PublicFrame& frame,
                                       const ProductWorld& world) {
    const Position position =
      ReciprocalJesterGhostInformation::make_position(frame, world);
    if (position.has_forced_action() ||
        !position.ordinary_predecessor_king_safe())
        return AdmissionVerdict::Reject;
    const std::uint8_t whiteKing = world.kingAtFirst
                                ? frame.royalFirst : frame.royalSecond;
    if (!frame.visibleGhost && adjacent(world.ghost, whiteKing))
        return AdmissionVerdict::Reject;
    return AdmissionVerdict::Admit;
}

std::vector<ProductWorld> admitted_fresh_worlds(const PublicFrame& frame) {
    std::vector<ProductWorld> result;
    for (const ProductWorld& world : geometric_worlds(frame))
        if (ReciprocalJesterGhostInformation::fresh_world_admission(
              frame, world) == AdmissionVerdict::Admit)
            result.push_back(world);
    return result;
}

}  // namespace Stockfish::Ultimate::ReciprocalJesterGhostInformation
