/*
  Ultimate Fish - exact crossed K+Jester versus K+Ghost information model
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#ifndef ULTIMATE_CROSSED_JESTER_GHOST_INFORMATION_MODEL_H_INCLUDED
#define ULTIMATE_CROSSED_JESTER_GHOST_INFORMATION_MODEL_H_INCLUDED

#include "position.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Stockfish::Ultimate::CrossedJesterGhostInformation {

inline constexpr std::uint32_t PlacementCount =
  2 * (Position::BoardSquares / 2) * (Position::BoardSquares - 1) *
  (Position::BoardSquares - 2) * (Position::BoardSquares - 3);
inline constexpr std::uint32_t StateCount = PlacementCount * 2;
inline constexpr std::uint32_t LowerJesterStateCount =
  2 * Position::BoardSquares * (Position::BoardSquares - 1) *
  (Position::BoardSquares - 2);
inline constexpr std::uint32_t LowerGhostStateCount =
  LowerJesterStateCount * 2;
inline constexpr unsigned ProductVariables = 2 * Position::BoardSquares;

// Physical kjesterkghost source state. Jester and Ghost retain fixed material
// roles through the source table's White-King horizontal fold.
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

// White's two royal models are public silhouettes. A hidden Black Ghost has
// no public coordinate; a visible Ghost has exactly visibleGhost.
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

// White privately knows kingAtFirst. Black privately knows ghost. Neither
// coordinate is encoded as an independent marginal in an information state.
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

struct WorldMask {
    std::array<std::uint64_t, 3> words{};

    void set(unsigned variable);
    [[nodiscard]] bool test(unsigned variable) const;
    [[nodiscard]] unsigned count() const;
    friend bool operator==(const WorldMask& lhs, const WorldMask& rhs) {
        return lhs.words == rhs.words;
    }
    friend bool operator<(const WorldMask& lhs, const WorldMask& rhs) {
        return lhs.words < rhs.words;
    }
};

[[nodiscard]] unsigned world_variable(const PublicFrame& frame,
                                      const ProductWorld& world);
[[nodiscard]] ProductWorld decode_world_variable(
  const PublicFrame& frame, unsigned variable);
[[nodiscard]] std::vector<ProductWorld> geometric_worlds(
  const PublicFrame& frame);

// A history atom is a current concrete world reached through one private
// history. Multiple atoms may have the same ProductWorld after non-signalling
// hidden moves converge; keeping both is required for perfect recall.
struct HistoryAtom {
    ProductWorld world;

    friend bool operator==(const HistoryAtom& lhs, const HistoryAtom& rhs) {
        return lhs.world == rhs.world;
    }
};

using KnowledgeCell = std::vector<std::uint32_t>;

struct KnowledgePartition {
    std::vector<KnowledgeCell> cells;

    friend bool operator==(const KnowledgePartition& lhs,
                           const KnowledgePartition& rhs) {
        return lhs.cells == rhs.cells;
    }
};

// atoms are histories; worlds is their deduplicated physical projection.
// Both partitions cover every atom exactly once. Thus two public histories
// with the same physical world mask but different private memories remain
// different states.
struct KnowledgeState {
    PublicFrame frame;
    std::vector<HistoryAtom> atoms;
    WorldMask worlds;
    KnowledgePartition white;
    KnowledgePartition black;

    friend bool operator==(const KnowledgeState& lhs,
                           const KnowledgeState& rhs) {
        return lhs.frame == rhs.frame && lhs.atoms == rhs.atoms &&
               lhs.worlds == rhs.worlds && lhs.white == rhs.white &&
               lhs.black == rhs.black;
    }
};

// Construct the exact fresh maximal public state after native/causal
// admission. White cells group by royal assignment; Black cells group by its
// exact Ghost square. No legal-dot observation is applied until requested.
[[nodiscard]] KnowledgeState fresh_state(const PublicFrame& frame);
void validate_knowledge_state(const KnowledgeState& state);
[[nodiscard]] const KnowledgePartition& partition_for(
  const KnowledgeState& state, Color player);
[[nodiscard]] std::uint32_t cell_for_atom(
  const KnowledgeState& state, Color player, std::uint32_t atom);

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
[[nodiscard]] KnowledgeState transform_state(
  const KnowledgeState& state, RectangleTransform transform);

struct CanonicalState {
    KnowledgeState value;
    RectangleTransform transform = RectangleTransform::Identity;
};

[[nodiscard]] CanonicalState canonicalize_state(
  const KnowledgeState& state);

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

// Split only the side-to-move partition, and only inside its existing cells,
// using the private inspectable legal-dot observation. The nonmover's perfect-
// recall partition is byte-for-byte preserved.
[[nodiscard]] KnowledgeState refine_mover_decisions(
  const KnowledgeState& state);

// Exact complete actions legal in every history of one already-refined mover
// cell. A strategy choice must select one of these uniform actions.
[[nodiscard]] std::vector<ActionKey> uniform_actions(
  const KnowledgeState& state, std::uint32_t moverCell);

struct CellAction {
    std::uint32_t cell = 0;
    ActionKey action;
};

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

struct TransitionAtom {
    std::uint32_t sourceAtom = 0;
    ProductWorld source;
    ClassifiedChild child;
    std::optional<FramedWorld> sameClassProduct;
    std::string whiteObservation;
    std::string blackObservation;
};

struct SuccessorBucket {
    std::string publicObservation;
    ChildDomain domain = ChildDomain::Invalid;
    std::vector<TransitionAtom> atoms;
    KnowledgePartition white;
    KnowledgePartition black;
    std::optional<KnowledgeState> sameClass;
};

struct AtomOutcome {
    std::uint32_t sourceAtom = 0;
    std::uint32_t bucket = 0;
    std::uint32_t childAtom = 0;
    ClassifiedChild child;
};

struct CellActionOutcomes {
    CellAction choice;
    std::vector<AtomOutcome> outcomes;
};

struct CompleteTransitions {
    KnowledgeState decisionState;
    std::vector<SuccessorBucket> buckets;
    std::vector<CellActionOutcomes> actions;
};

// Enumerate every full action available in every mover-private decision cell.
// Successor buckets are built from all cell/action candidates sharing one
// public observation, so the nonmover cannot infer an undisclosed private cell
// or hidden action.  AtomOutcome identifies the exact successor history for
// each source atom.  This is the solver-facing Bellman transition system; it
// does not sample policies or Cartesian-product their private choices.
[[nodiscard]] CompleteTransitions enumerate_complete_transitions(
  const KnowledgeState& state);

// Apply a complete policy containing exactly one uniform full ActionKey for
// every mover cell. Results are partitioned by the common/public transition
// observation. Both successor partitions refine their own prior cell by that
// player's private transition observation; duplicate physical worlds remain
// separate history atoms when perfect recall distinguishes their paths.
[[nodiscard]] std::vector<SuccessorBucket> apply_uniform_policy(
  const KnowledgeState& decisionState,
  const std::vector<CellAction>& policy);

enum class AdmissionVerdict : std::uint8_t {
    Admit,
    Reject,
};

// A hidden Black Ghost adjacent to either White royal silhouette is causally
// impossible: both King and Jester trigger the native reveal hook.
[[nodiscard]] AdmissionVerdict fresh_world_admission(
  const PublicFrame& frame, const ProductWorld& world);

}  // namespace Stockfish::Ultimate::CrossedJesterGhostInformation

#endif
