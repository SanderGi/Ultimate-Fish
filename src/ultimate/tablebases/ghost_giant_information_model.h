/*
  Ultimate Fish - exact one-Ghost plus Giant information model
  Copyright (C) 2026 Ultimate Fish contributors
  GPLv3 or later.
*/

#ifndef ULTIMATE_GHOST_GIANT_INFORMATION_MODEL_H_INCLUDED
#define ULTIMATE_GHOST_GIANT_INFORMATION_MODEL_H_INCLUDED

#include "ghost_public_extra_model.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace Stockfish::Ultimate::GhostGiant {

inline constexpr std::uint32_t PlacementCount =
  GhostPublicExtra::PlacementCount;
inline constexpr std::uint32_t StateCount = PlacementCount * 2;
inline constexpr std::uint32_t GiantAnchorCount =
  (Position::BoardFiles - 1) * (Position::BoardRanks - 1);

enum class Orientation : std::uint8_t { Same, Opposing };
enum class RectangleTransform : std::uint8_t {
    Identity = 0, Horizontal = 1, Vertical = 2, Both = 3,
};

// The shipping source order for both Giant/Ghost rows is Ghost then Giant.
// The dense codec deliberately retains invalid anchors and footprint overlaps;
// admission and the transition compiler reject those records independently.
struct SourceState {
    Color side = Color::White;
    std::uint8_t whiteKing = 0;
    std::uint8_t blackKing = 0;
    std::uint8_t ghost = 0;
    std::uint8_t giant = 0;  // lower-left 2x2 anchor
    bool ghostVisible = false;
};

// The exact force kernel always normalizes the public Giant to White. In the
// opposing row this swaps colors/King identities while preserving coordinates.
struct NormalizedState {
    Color side = Color::White;
    std::uint8_t whiteKing = 0;
    std::uint8_t blackKing = 0;
    std::uint8_t giant = 0;
    std::uint8_t ghost = 0;
    bool ghostVisible = false;
};

struct PublicGeometry {
    std::uint8_t side = 0;
    std::uint8_t whiteKing = 0;
    std::uint8_t blackKing = 0;
    std::uint8_t giant = 0;
    std::uint8_t visible = 0;

    friend bool operator==(const PublicGeometry& lhs,
                           const PublicGeometry& rhs) {
        return lhs.side == rhs.side && lhs.whiteKing == rhs.whiteKing &&
               lhs.blackKing == rhs.blackKing && lhs.giant == rhs.giant &&
               lhs.visible == rhs.visible;
    }
    friend bool operator<(const PublicGeometry& lhs, const PublicGeometry& rhs);
};

struct CanonicalGeometry {
    PublicGeometry geometry;
    RectangleTransform transform = RectangleTransform::Identity;
};

[[nodiscard]] bool valid_giant_anchor(std::uint8_t anchor);
[[nodiscard]] Bitboard giant_footprint(std::uint8_t anchor);
[[nodiscard]] bool valid_physical_geometry(const NormalizedState& state);

[[nodiscard]] std::uint8_t transform_square(
  std::uint8_t square, RectangleTransform transform);
[[nodiscard]] std::uint8_t transform_giant_anchor(
  std::uint8_t anchor, RectangleTransform transform);
[[nodiscard]] SourceState transform_source(
  SourceState state, RectangleTransform transform);
[[nodiscard]] NormalizedState transform_normalized(
  NormalizedState state, RectangleTransform transform);
[[nodiscard]] PublicGeometry transform_geometry(
  PublicGeometry geometry, RectangleTransform transform);
[[nodiscard]] CanonicalGeometry canonical_geometry(
  const PublicGeometry& geometry);

[[nodiscard]] std::uint32_t encode_source(const SourceState& state);
[[nodiscard]] SourceState decode_source(std::uint32_t index);
[[nodiscard]] NormalizedState normalize(const SourceState& state,
                                        Orientation orientation);
[[nodiscard]] SourceState denormalize(const NormalizedState& state,
                                      Orientation orientation);

[[nodiscard]] Position make_position(const NormalizedState& state,
                                     Orientation orientation);
[[nodiscard]] std::optional<NormalizedState> scan_same_class(
  const Position& position, Orientation orientation);

using ActionKey = GhostPublicExtra::ActionKey;
using DecisionMarker = GhostPublicExtra::DecisionMarker;
[[nodiscard]] ActionKey action_key(const Move& move);
[[nodiscard]] ActionKey transform_action(const Position& before,
                                         const Move& move,
                                         RectangleTransform transform);
[[nodiscard]] std::vector<ActionKey> legal_action_keys(
  const Position& position);
[[nodiscard]] std::vector<DecisionMarker> decision_markers(
  const Position& position);
[[nodiscard]] std::vector<DecisionMarker> transform_markers(
  const Position& before, const std::vector<Move>& moves,
  RectangleTransform transform);

[[nodiscard]] bool fresh_root_admitted(const NormalizedState& state,
                                       Orientation orientation);

// Exhaustive dense-codec and physical D2 contracts. This is intentionally
// standalone so a Giant source can never be accepted by the point adapter.
void exact_model_self_test();

}  // namespace Stockfish::Ultimate::GhostGiant

#endif
