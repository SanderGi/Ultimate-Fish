/*
  Ultimate Fish - exact one-Ghost plus one-public-extra information model
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#ifndef ULTIMATE_GHOST_PUBLIC_EXTRA_MODEL_H_INCLUDED
#define ULTIMATE_GHOST_PUBLIC_EXTRA_MODEL_H_INCLUDED

#include "position.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Stockfish::Ultimate::GhostPublicExtra {

// The dense K+K+2 source tables fold only the ordinary horizontal reflection.
// Ghost visibility is the sole substate in this model.  These constants are
// part of the adapter contract and deliberately do not depend on a tablebase
// file being open.
#ifdef ULTIMATE_GHOST_EXTRA_IS_COPYCAT
inline constexpr std::uint32_t PlacementCount =
  2 * Position::BoardSquares * (Position::BoardSquares - 1) *
  (Position::BoardSquares - 2) * (Position::BoardSquares - 3);
#else
inline constexpr std::uint32_t PlacementCount =
  2 * (Position::BoardSquares / 2) * (Position::BoardSquares - 1) *
  (Position::BoardSquares - 2) * (Position::BoardSquares - 3);
#endif
#ifdef ULTIMATE_GHOST_EXTRA_SUBSTATES
inline constexpr std::uint32_t ExtraSubstateCount =
  ULTIMATE_GHOST_EXTRA_SUBSTATES;
#else
inline constexpr std::uint32_t ExtraSubstateCount = 1;
#endif
inline constexpr std::uint32_t StateCount =
  PlacementCount * 2 * ExtraSubstateCount;
inline constexpr std::uint32_t LowerGhostStateCount =
  2 * Position::BoardSquares * (Position::BoardSquares - 1) *
  (Position::BoardSquares - 2) * 2;

enum class SourceOrder : std::uint8_t {
    ExtraPrimary,
    GhostPrimary,
};

// A hidden Ghost next to an enemy royal is ordinarily impossible: a royal
// move reveals it and an ordinary Ghost move beside the royal reveals itself.
// Mage Swap and Fisherman Pull can instead create that geometry by forced
// displacement without running either reveal hook.  Their fresh-root solver
// must supply a material-specific exact causal audit; this core returns a
// fail-closed NeedsExactCausalAudit verdict rather than admitting every dense
// record or incorrectly applying the Bishop rule.
enum class HiddenAdjacentPolicy : std::uint8_t {
    ImpossibleWithoutForcedRelocation,
    NeedsExactCausalAudit,
};

struct MaterialSpec {
    PieceType extraType = PieceType::Bishop;
    Color extraColor = Color::White;
    Color ghostColor = Color::White;
    SourceOrder sourceOrder = SourceOrder::ExtraPrimary;
    HiddenAdjacentPolicy hiddenAdjacent =
      HiddenAdjacentPolicy::ImpossibleWithoutForcedRelocation;
    const char* name = "kbishopghostk";

    [[nodiscard]] Color observer_color() const { return ~ghostColor; }
    [[nodiscard]] bool extra_owned_by_ghost_owner() const {
        return extraColor == ghostColor;
    }
};

[[nodiscard]] MaterialSpec bishop_same();
[[nodiscard]] MaterialSpec bishop_reciprocal();
[[nodiscard]] MaterialSpec mage_same_contract();
[[nodiscard]] MaterialSpec mage_reciprocal_contract();
[[nodiscard]] MaterialSpec fisherman_same_contract();
[[nodiscard]] MaterialSpec fisherman_reciprocal_contract();

// Physical source-table coordinates. first/second preserve the table header's
// primary/secondary order; extra/ghost accessors apply the material adapter.
struct ConcreteState {
    Color side = Color::White;
    std::uint8_t whiteKing = 0;
    std::uint8_t blackKing = 0;
    std::uint8_t first = 0;
    std::uint8_t second = 0;
    std::uint8_t extraSubstate = 0;
    bool ghostVisible = false;
};

enum class Role : std::uint8_t {
    GhostOwner,
    Observer,
};

// Color-independent role frame used by the exact force kernel. Coordinates
// remain in the physical board frame; only ownership and side-to-move are
// normalized. This avoids silently reversing geometry in reciprocal rows.
struct RoleState {
    Role side = Role::GhostOwner;
    std::uint8_t ownerKing = 0;
    std::uint8_t observerKing = 0;
    std::uint8_t extra = 0;
    std::uint8_t ghost = 0;
    std::uint8_t extraSubstate = 0;
    bool ghostVisible = false;
    bool extraOwnedByOwner = true;
};

[[nodiscard]] std::uint8_t extra_square(const ConcreteState& state,
                                        const MaterialSpec& material);
[[nodiscard]] std::uint8_t ghost_square(const ConcreteState& state,
                                        const MaterialSpec& material);
[[nodiscard]] ConcreteState with_piece_squares(
  ConcreteState state, const MaterialSpec& material,
  std::uint8_t extra, std::uint8_t ghost);

[[nodiscard]] std::uint32_t encode_index(const ConcreteState& state,
                                         const MaterialSpec& material);
[[nodiscard]] ConcreteState decode_index(std::uint32_t index,
                                         const MaterialSpec& material);
[[nodiscard]] RoleState normalize_roles(const ConcreteState& state,
                                        const MaterialSpec& material);
[[nodiscard]] ConcreteState denormalize_roles(const RoleState& state,
                                              const MaterialSpec& material);

// Construct a physical world without applying the source table's horizontal
// fold. Every represented model is marked moved, matching the closed concrete
// K+K+2 tables and excluding unrepresented castling state.
[[nodiscard]] Position make_position(const ConcreteState& state,
                                     const MaterialSpec& material);
[[nodiscard]] Position make_position(std::uint32_t index,
                                     const MaterialSpec& material);

// Compound Copycat source codecs deliberately retain dense sentinel records
// whose derived mirror clone overlaps another model.  They remain part of the
// source index space but are never legal concrete worlds or fresh roots.
[[nodiscard]] bool valid_concrete_world(const ConcreteState& state,
                                        const MaterialSpec& material);
[[nodiscard]] bool tablebase_substate_geometrically_valid(
  PieceType type, std::uint8_t whiteKing, std::uint8_t blackKing,
  std::uint8_t extra, std::uint8_t other, std::uint32_t substate);

enum class RectangleTransform : std::uint8_t {
    Identity = 0,
    Horizontal = 1,
    Vertical = 2,
    Both = 3,
};

[[nodiscard]] std::uint8_t transform_square(
  std::uint8_t square, RectangleTransform transform);
[[nodiscard]] ConcreteState transform_state(
  ConcreteState state, RectangleTransform transform);

// Full collision-free engine action identity. This is intentionally distinct
// from a mover-private UI marker: decision dots collapse to (from,to), while
// an exact strategy edge retains kind, promotion, and semantic auxiliary.
struct ActionKey {
    std::uint8_t from = 0;
    std::uint8_t to = 0;
    std::uint8_t auxiliary = 0;
    MoveKind kind = MoveKind::Normal;
    PieceType promotion = PieceType::Count;

    friend bool operator==(const ActionKey& lhs, const ActionKey& rhs) {
        return lhs.from == rhs.from && lhs.to == rhs.to &&
               lhs.auxiliary == rhs.auxiliary && lhs.kind == rhs.kind &&
               lhs.promotion == rhs.promotion;
    }
    friend bool operator<(const ActionKey& lhs, const ActionKey& rhs);
};

struct DecisionMarker {
    std::int16_t from = -1;
    std::int16_t to = -1;

    friend bool operator==(const DecisionMarker& lhs,
                           const DecisionMarker& rhs) {
        return lhs.from == rhs.from && lhs.to == rhs.to;
    }
    friend bool operator<(const DecisionMarker& lhs,
                          const DecisionMarker& rhs) {
        return lhs.from < rhs.from ||
               (lhs.from == rhs.from && lhs.to < rhs.to);
    }
};

[[nodiscard]] ActionKey action_key(const Move& move);
[[nodiscard]] ActionKey transform_action(
  const ActionKey& action, RectangleTransform transform);
[[nodiscard]] std::vector<ActionKey> legal_action_keys(
  const Position& position);
[[nodiscard]] std::vector<DecisionMarker> decision_markers(
  const Position& position);
[[nodiscard]] std::vector<DecisionMarker> transform_markers(
  std::vector<DecisionMarker> markers, RectangleTransform transform);

struct LowerGhostState {
    Role side = Role::GhostOwner;
    std::uint8_t ownerKing = 0;
    std::uint8_t observerKing = 0;
    std::uint8_t ghost = 0;
    bool visible = false;
};

[[nodiscard]] std::uint32_t encode_lower_ghost(
  const LowerGhostState& state);
[[nodiscard]] LowerGhostState decode_lower_ghost(std::uint32_t index);

enum class ChildDomain : std::uint8_t {
    SameClass,
    LowerGhost,
    InsufficientExtra,
    LowerExtraConcrete,
    ExactTerminal,
    Invalid,
};

struct ClassifiedChild {
    ChildDomain domain = ChildDomain::Invalid;
    std::uint32_t index = 0;
    std::optional<Color> winner;
};

[[nodiscard]] ClassifiedChild classify_child(
  const Position& position, const MaterialSpec& material);

// Exact 80-square membership mask consumed by the certified K+Ghost UFGM
// lower probe. It represents an arbitrary history-refined image, not a fresh
// maximal root, and has no count limit.
struct GhostMask {
    std::uint64_t low = 0;
    std::uint16_t high = 0;

    void set(unsigned square);
    [[nodiscard]] bool test(unsigned square) const;
    [[nodiscard]] unsigned count() const;
};

struct LowerGhostImage {
    Role side = Role::GhostOwner;
    std::uint8_t ownerKing = 0;
    std::uint8_t observerKing = 0;
    bool visible = false;
    GhostMask locations;
};

// Combine every concrete lower child in one public observation bucket. The
// call rejects mixed public geometries or non-K+Ghost children instead of
// resetting the belief to a dense root.
[[nodiscard]] LowerGhostImage inherited_lower_ghost_image(
  const std::vector<ClassifiedChild>& children);

enum class FreshAdmissionVerdict : std::uint8_t {
    Admit,
    Reject,
    NeedsExactCausalAudit,
};

[[nodiscard]] FreshAdmissionVerdict classify_fresh_root_admission(
  const Position& position, const MaterialSpec& material);

}  // namespace Stockfish::Ultimate::GhostPublicExtra

#endif
