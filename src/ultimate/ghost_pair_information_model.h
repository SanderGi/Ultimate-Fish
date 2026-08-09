/*
  Ultimate Fish - exact K+Ghost+Ghost versus K information model
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#ifndef ULTIMATE_GHOST_PAIR_INFORMATION_MODEL_H_INCLUDED
#define ULTIMATE_GHOST_PAIR_INFORMATION_MODEL_H_INCLUDED

#include "position.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Stockfish::Ultimate::GhostPairInformation {

// kghostghostk uses the identical-four-model codec: horizontal reflection is
// folded by the White King, and the two Ghost square ranks are unordered.
// Four visibility substates remain (hidden/visible for each sorted square).
inline constexpr std::uint32_t PlacementCount =
  (2 * (Position::BoardSquares / 2) * (Position::BoardSquares - 1) *
   (Position::BoardSquares - 2) * (Position::BoardSquares - 3)) / 2;
inline constexpr std::uint32_t StateCount = PlacementCount * 4;
inline constexpr std::uint32_t LowerGhostStateCount =
  2 * Position::BoardSquares * (Position::BoardSquares - 1) *
  (Position::BoardSquares - 2) * 2;
inline constexpr unsigned MaximumWorlds =
  ((Position::BoardSquares - 2) * (Position::BoardSquares - 3)) / 2;
inline constexpr unsigned MaskWords = (MaximumWorlds + 63) / 64;

// The two concrete fields may arrive in either model order. encode_source()
// canonicalizes their physical squares and carries each visibility bit with
// its square, so swapping complete Ghost records cannot change the index.
struct ConcreteState {
    Color side = Color::White;
    std::uint8_t whiteKing = 0;
    std::uint8_t blackKing = 0;
    std::uint8_t firstGhost = 0;
    std::uint8_t secondGhost = 0;
    bool firstVisible = false;
    bool secondVisible = false;
};

[[nodiscard]] std::uint32_t encode_source(const ConcreteState& state);
[[nodiscard]] ConcreteState decode_source(std::uint32_t index);

enum class VisibilityClass : std::uint8_t {
    HiddenHidden,
    HiddenVisible,
    VisibleVisible,
};

// visibleGhosts is sorted. A hidden-hidden frame contains no public Ghost
// square; a mixed frame contains exactly one; visible-visible contains two.
struct PublicFrame {
    Color side = Color::White;
    std::uint8_t whiteKing = 0;
    std::uint8_t blackKing = 0;
    std::array<std::uint8_t, 2> visibleGhosts{};
    std::uint8_t visibleCount = 0;

    [[nodiscard]] VisibilityClass visibility_class() const;
    friend bool operator==(const PublicFrame& lhs, const PublicFrame& rhs) {
        return lhs.side == rhs.side && lhs.whiteKing == rhs.whiteKing &&
               lhs.blackKing == rhs.blackKing &&
               lhs.visibleGhosts == rhs.visibleGhosts &&
               lhs.visibleCount == rhs.visibleCount;
    }
};

// A world is an unordered, noncolliding physical square pair. Visibility is
// inferred from membership in PublicFrame::visibleGhosts. This representation
// has no latent Ghost identity and therefore cannot leak identity through a
// codec, action, or symmetry operation.
struct PairWorld {
    std::uint8_t first = 0;
    std::uint8_t second = 1;

    friend bool operator==(const PairWorld& lhs, const PairWorld& rhs) {
        return lhs.first == rhs.first && lhs.second == rhs.second;
    }
};

struct FramedWorld {
    PublicFrame frame;
    PairWorld world;
};

[[nodiscard]] FramedWorld source_to_product(const ConcreteState& state);
[[nodiscard]] ConcreteState product_to_source(const PublicFrame& frame,
                                              const PairWorld& world);
[[nodiscard]] Position make_position(const PublicFrame& frame,
                                     const PairWorld& world);

// Hidden-hidden uses all C(78,2)=3003 pair variables. Mixed visibility uses
// one variable for each of 77 possible hidden companions. Visible-visible has
// one variable. A single exact bitset is used in all three cases so arbitrary
// correlations between the two hidden coordinates are preserved.
struct PairMask {
    std::array<std::uint64_t, MaskWords> words{};

    void set(unsigned variable);
    [[nodiscard]] bool test(unsigned variable) const;
    [[nodiscard]] unsigned count() const;
    friend bool operator==(const PairMask& lhs, const PairMask& rhs) {
        return lhs.words == rhs.words;
    }
};

[[nodiscard]] unsigned variable_count(const PublicFrame& frame);
[[nodiscard]] unsigned pair_variable(const PublicFrame& frame,
                                     const PairWorld& world);
[[nodiscard]] PairWorld decode_pair_variable(const PublicFrame& frame,
                                             unsigned variable);
[[nodiscard]] std::vector<PairWorld> geometric_worlds(
  const PublicFrame& frame);
[[nodiscard]] std::vector<PairWorld> decode_pair_mask(
  const PublicFrame& frame, const PairMask& mask);

enum class RectangleTransform : std::uint8_t {
    Identity = 0,
    Horizontal = 1,
    Vertical = 2,
    Both = 3,
};

[[nodiscard]] std::uint8_t transform_square(
  std::uint8_t square, RectangleTransform transform);
[[nodiscard]] FramedWorld transform_world(
  const PublicFrame& frame, const PairWorld& world,
  RectangleTransform transform);

struct PairSet {
    PublicFrame frame;
    PairMask worlds;

    friend bool operator==(const PairSet& lhs, const PairSet& rhs) {
        return lhs.frame == rhs.frame && lhs.worlds == rhs.worlds;
    }
};

[[nodiscard]] PairSet transform_set(
  const PublicFrame& frame, const PairMask& worlds,
  RectangleTransform transform);

struct CanonicalSet {
    PairSet value;
    RectangleTransform transform = RectangleTransform::Identity;
};

[[nodiscard]] CanonicalSet canonicalize_set(
  const PublicFrame& frame, const PairMask& worlds);

// Complete engine action identity. The mover-private UI observation remains
// the shared decision_observation_key(), which deliberately collapses engine
// actions that render the same source/destination dot.
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

[[nodiscard]] ActionKey action_key(const Move& move);
[[nodiscard]] ActionKey transform_action(
  ActionKey action, RectangleTransform transform);
[[nodiscard]] std::vector<ActionKey> legal_actions(const Position& position);

struct DecisionBucket {
    std::string observation;
    PairMask worlds;
};

// White owns both Ghosts and therefore retains singleton actual worlds.
// Black's mover-private cells are the exact legal-dot observation classes.
[[nodiscard]] std::vector<DecisionBucket> decision_partition(
  const PublicFrame& frame, const std::vector<PairWorld>& worlds);

enum class Role : std::uint8_t {
    GhostOwner,
    Observer,
};

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
    ExactTerminal,
    Invalid,
};

struct ClassifiedChild {
    ChildDomain domain = ChildDomain::Invalid;
    std::uint32_t index = 0;
    std::optional<Color> winner;
};

[[nodiscard]] ClassifiedChild classify_child(const Position& position);
[[nodiscard]] std::optional<FramedWorld> same_class_product(
  const Position& position);

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

// Project one capture-observation bucket into the exact arbitrary K+Ghost
// belief accepted by UFGM. It deduplicates convergent children but never
// replaces the image with a fresh maximal square mask.
[[nodiscard]] LowerGhostImage inherited_lower_ghost_image(
  const std::vector<ClassifiedChild>& children);

struct TransitionWorld {
    PairWorld source;
    std::string observation;
    ClassifiedChild child;
    std::optional<FramedWorld> sameClassProduct;
};

[[nodiscard]] std::vector<std::vector<TransitionWorld>> transition_partition(
  const PublicFrame& frame, const std::vector<PairWorld>& worlds,
  const ActionKey& action, Color observer);

enum class AdmissionVerdict : std::uint8_t {
    Admit,
    Reject,
};

// Exact fresh-snapshot admission for this no-relocator material. Native
// predecessor safety is required, and every hidden Ghost adjacent to the
// enemy Black King is causally impossible because either mover reveals it.
[[nodiscard]] AdmissionVerdict fresh_world_admission(
  const PublicFrame& frame, const PairWorld& world);
[[nodiscard]] std::vector<PairWorld> admitted_fresh_worlds(
  const PublicFrame& frame);

}  // namespace Stockfish::Ultimate::GhostPairInformation

#endif
