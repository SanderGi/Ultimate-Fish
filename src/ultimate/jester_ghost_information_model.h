/*
  Ultimate Fish - exact K+Jester+Ghost versus K information model
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#ifndef ULTIMATE_JESTER_GHOST_INFORMATION_MODEL_H_INCLUDED
#define ULTIMATE_JESTER_GHOST_INFORMATION_MODEL_H_INCLUDED

#include "position.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Stockfish::Ultimate::JesterGhostInformation {

inline constexpr std::uint32_t PlacementCount =
  2 * (Position::BoardSquares / 2) * (Position::BoardSquares - 1) *
  (Position::BoardSquares - 2) * (Position::BoardSquares - 3);
inline constexpr std::uint32_t StateCount = PlacementCount * 2;
inline constexpr std::uint32_t LowerJesterStateCount =
  2 * Position::BoardSquares * (Position::BoardSquares - 1) *
  (Position::BoardSquares - 2);
inline constexpr std::uint32_t LowerGhostStateCount =
  LowerJesterStateCount * 2;
inline constexpr unsigned ProductVariables =
  2 * Position::BoardSquares;

// Concrete kjesterghostk source-table state. The dense source folds
// horizontally according to the real White King's square, so the alternative
// royal assignment may decode in a different physical reflection. Product
// frames below remove that accidental concrete-codec distinction.
struct ConcreteState {
    Color side = Color::White;
    std::uint8_t whiteKing = 0;
    std::uint8_t blackKing = 0;
    std::uint8_t jester = 0;
    std::uint8_t ghost = 0;
    bool ghostVisible = false;
};

[[nodiscard]] std::uint32_t encode_source(const ConcreteState& state);
[[nodiscard]] ConcreteState decode_source(std::uint32_t index);

// Public frame before the private legal-dot observation. royalFirst and
// royalSecond are sorted physical silhouette squares. A hidden Ghost has no
// public coordinate. A visible Ghost has exactly visibleGhost.
struct PublicFrame {
    Color side = Color::White;
    std::uint8_t blackKing = 0;
    std::uint8_t royalFirst = 0;
    std::uint8_t royalSecond = 0;
    std::optional<std::uint8_t> visibleGhost;

    friend bool operator==(const PublicFrame& lhs, const PublicFrame& rhs) {
        return lhs.side == rhs.side && lhs.blackKing == rhs.blackKing &&
               lhs.royalFirst == rhs.royalFirst &&
               lhs.royalSecond == rhs.royalSecond &&
               lhs.visibleGhost == rhs.visibleGhost;
    }
};

// White owns the Jester and Ghost and knows this complete world. Black sees
// neither the real royal identity nor a hidden Ghost coordinate.
struct ProductWorld {
    bool kingAtFirst = true;
    std::uint8_t ghost = 0;

    friend bool operator==(const ProductWorld& lhs,
                           const ProductWorld& rhs) {
        return lhs.kingAtFirst == rhs.kingAtFirst &&
               lhs.ghost == rhs.ghost;
    }
};

struct FramedWorld {
    PublicFrame frame;
    ProductWorld world;
};

[[nodiscard]] FramedWorld source_to_product(const ConcreteState& state);
[[nodiscard]] ConcreteState product_to_source(const PublicFrame& frame,
                                              const ProductWorld& world);
[[nodiscard]] Position make_position(const PublicFrame& frame,
                                     const ProductWorld& world);

// The exact product universe uses one collision-free membership bit for every
// assignment/square pair. Three machine words cover 160 variables. Occupied
// royal/King squares are invalid variables, leaving at most 2*77=154 worlds.
struct ProductMask {
    std::array<std::uint64_t, 3> words{};

    void set(unsigned variable);
    [[nodiscard]] bool test(unsigned variable) const;
    [[nodiscard]] unsigned count() const;
    friend bool operator==(const ProductMask& lhs, const ProductMask& rhs) {
        return lhs.words == rhs.words;
    }
};

// A belief is encoded losslessly as one public physical frame plus the exact
// 160-variable product mask.  D2 canonicalization transforms the whole set;
// it never chooses a reflection from one actual/private world.
struct ProductSet {
    PublicFrame frame;
    ProductMask worlds;

    friend bool operator==(const ProductSet& lhs, const ProductSet& rhs) {
        return lhs.frame == rhs.frame && lhs.worlds == rhs.worlds;
    }
};

[[nodiscard]] unsigned product_variable(const PublicFrame& frame,
                                        const ProductWorld& world);
[[nodiscard]] ProductWorld decode_product_variable(
  const PublicFrame& frame, unsigned variable);
[[nodiscard]] std::vector<ProductWorld> geometric_worlds(
  const PublicFrame& frame);
[[nodiscard]] std::vector<ProductWorld> decode_product_mask(
  const PublicFrame& frame, const ProductMask& mask);

enum class RectangleTransform : std::uint8_t {
    Identity = 0,
    Horizontal = 1,
    Vertical = 2,
    Both = 3,
};

[[nodiscard]] std::uint8_t transform_square(
  std::uint8_t square, RectangleTransform transform);
[[nodiscard]] FramedWorld transform_world(
  const PublicFrame& frame, const ProductWorld& world,
  RectangleTransform transform);
[[nodiscard]] ProductSet transform_set(
  const PublicFrame& frame, const ProductMask& worlds,
  RectangleTransform transform);

struct CanonicalWorld {
    PublicFrame frame;
    ProductWorld world;
    RectangleTransform transform = RectangleTransform::Identity;
};

[[nodiscard]] CanonicalWorld canonicalize_world(
  const PublicFrame& frame, const ProductWorld& world);

struct CanonicalSet {
    ProductSet value;
    RectangleTransform transform = RectangleTransform::Identity;
};

[[nodiscard]] CanonicalSet canonicalize_set(
  const PublicFrame& frame, const ProductMask& worlds);

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
    // The owner's information cells are singleton even when two worlds happen
    // to display the same dots. Black's cells group by this exact key.
    std::string observation;
    ProductMask worlds;
};

// Partition one exact current belief before its mover acts. No world is
// dropped. White-owner cells remain singleton; Black-observer cells group by
// the mover-private legal-dot key, which is not exposed to White.
[[nodiscard]] std::vector<DecisionBucket> decision_partition(
  const PublicFrame& frame, const std::vector<ProductWorld>& worlds);

struct LowerJesterState {
    Color side = Color::White;
    std::uint8_t whiteKing = 0;
    std::uint8_t blackKing = 0;
    std::uint8_t jester = 0;
};

[[nodiscard]] std::uint32_t encode_lower_jester(
  const LowerJesterState& state);
[[nodiscard]] LowerJesterState decode_lower_jester(std::uint32_t index);

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
    LowerJester,
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

// Recover the physical product coordinate of a same-class Position without
// passing through the horizontally folded source index. This distinction is
// essential: the two royal assignments can select opposite source-table
// reflections even though they occupy one common public physical frame.
[[nodiscard]] std::optional<FramedWorld> same_class_product(
  const Position& position);

struct LowerJesterSet {
    std::uint8_t cardinality = 0;
    Color side = Color::White;
    std::uint8_t blackKing = 0;
    std::uint8_t royalFirst = 0;
    std::uint8_t royalSecond = 0;
    std::array<std::uint32_t, 2> concrete{};
};

// The K+Jester lower world is exactly a concrete singleton or the two swapped
// royal assignments in one physical silhouette frame. No fresh remaximizing
// is permitted after history has selected only one assignment.
[[nodiscard]] LowerJesterSet inherited_lower_jester_set(
  const std::vector<ClassifiedChild>& children);

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

[[nodiscard]] LowerGhostImage inherited_lower_ghost_image(
  const std::vector<ClassifiedChild>& children);

struct TransitionWorld {
    ProductWorld source;
    std::string observation;
    ClassifiedChild child;
    // Populated exactly for SameClass. The folded child.index remains the
    // concrete-table probe coordinate; this physical value is the epistemic
    // successor coordinate and must be used when forming its product mask.
    std::optional<FramedWorld> sameClassProduct;
};

// Apply one complete action in every world where it is legal and partition by
// the chosen observer's exact public transition key. Worlds where the action
// is illegal are absent because observing the action eliminates them. The
// caller must not eliminate a legal world based on policy preference.
[[nodiscard]] std::vector<std::vector<TransitionWorld>> transition_partition(
  const PublicFrame& frame, const std::vector<ProductWorld>& worlds,
  const ActionKey& action, Color observer);

enum class AdmissionVerdict : std::uint8_t {
    Admit,
    Reject,
};

// Exact fresh-snapshot admission for this closed material: native necessary
// predecessor safety plus the causal rule that a hidden White Ghost cannot
// remain adjacent to the observing Black King. There is no forced relocator
// in K+Jester+Ghost-v-K, so no unresolved hook is required.
[[nodiscard]] AdmissionVerdict fresh_world_admission(
  const PublicFrame& frame, const ProductWorld& world);
[[nodiscard]] std::vector<ProductWorld> admitted_fresh_worlds(
  const PublicFrame& frame);

}  // namespace Stockfish::Ultimate::JesterGhostInformation

#endif
