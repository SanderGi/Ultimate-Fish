/*
  Ultimate Fish - exact crossed Jester/Ghost information solver core
  Copyright (C) 2026 Ultimate Fish contributors
  GPLv3 or later.
*/

#ifndef ULTIMATE_CROSSED_JESTER_GHOST_INFORMATION_SOLVER_H_INCLUDED
#define ULTIMATE_CROSSED_JESTER_GHOST_INFORMATION_SOLVER_H_INCLUDED

#include "crossed_jester_ghost_information_model.h"

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace Stockfish::Ultimate::CrossedJesterGhostSolver {

using NodeId = std::uint32_t;

struct ExpansionCertificate {
    NodeId node = 0;
    std::uint64_t actions = 0;
    std::uint64_t observations = 0;
    std::uint64_t outcomes = 0;
    std::uint64_t sameClassObservations = 0;
    std::uint64_t externalObservations = 0;
    std::uint64_t newlyInterned = 0;
    std::uint64_t keyRoundtripResidual = 0;
};

struct NodeExpansion {
    CrossedJesterGhostInformation::CompleteTransitions transitions;
    // SameClass buckets name their exact canonical child. External buckets
    // remain null and are resolved by the authenticated lower-table oracles in
    // the Bellman layer.
    std::vector<std::optional<NodeId>> sameClassChildren;
    ExpansionCertificate certificate;
};

enum class EquationKind : std::uint8_t {
    Or,
    And,
};

// A Bellman child is either a variable in the crossed graph or an exact
// query against one of the authenticated lower domains. External children
// deliberately retain the complete classified child and successor-history
// coordinates: equation emission must resolve them, and cannot reinterpret an
// absent lower answer as a draw.
struct ChildReference {
    CrossedJesterGhostInformation::ChildDomain domain =
      CrossedJesterGhostInformation::ChildDomain::Invalid;
    std::optional<NodeId> node;
    std::uint32_t bucket = 0;
    std::uint32_t childAtom = 0;
    CrossedJesterGhostInformation::ClassifiedChild child;

    friend bool operator==(const ChildReference& lhs,
                           const ChildReference& rhs) {
        return lhs.domain == rhs.domain && lhs.node == rhs.node &&
               lhs.bucket == rhs.bucket && lhs.childAtom == rhs.childAtom &&
               lhs.child.domain == rhs.child.domain &&
               lhs.child.index == rhs.child.index &&
               lhs.child.winner == rhs.child.winner;
    }
};

struct ActionGatePlan {
    std::uint32_t action = 0;
    std::uint32_t moverCell = 0;
    std::vector<ChildReference> children;
};

struct AtomEquationPlan {
    std::uint32_t atom = 0;
    EquationKind kind = EquationKind::Or;
    // Mover equations OR action gates available throughout the mover's exact
    // private cell. Nonmover equations leave this empty and AND children.
    std::vector<std::uint32_t> gates;
    std::vector<ChildReference> children;
};

struct BellmanCertificate {
    std::uint64_t atoms = 0;
    std::uint64_t gates = 0;
    std::uint64_t childReferences = 0;
    std::uint64_t internalReferences = 0;
    std::uint64_t externalReferences = 0;
    std::uint64_t sourceCoverageResidual = 0;
    std::uint64_t cellUniformityResidual = 0;
};

struct TargetBellmanPlan {
    Color target = Color::White;
    Color mover = Color::White;
    std::vector<ActionGatePlan> gates;
    std::vector<AtomEquationPlan> atoms;
    BellmanCertificate certificate;
};

// Collision-free in-memory reference arena. Production graph capture will
// stream the same portable keys to named scratch; this implementation is the
// deterministic oracle used by focused tests and restore verification.
class Arena {
   public:
    [[nodiscard]] NodeId intern(
      const CrossedJesterGhostInformation::KnowledgeState& state);
    [[nodiscard]] std::optional<NodeId> find(
      const CrossedJesterGhostInformation::KnowledgeState& state) const;
    [[nodiscard]] NodeExpansion regenerate(NodeId node, bool allowNew);
    [[nodiscard]] TargetBellmanPlan bellman_plan(
      NodeId node, Color target, bool allowNew);

    [[nodiscard]] const CrossedJesterGhostInformation::KnowledgeState& node(
      NodeId id) const;
    [[nodiscard]] std::size_t size() const;

   private:
    [[nodiscard]] static std::vector<std::uint8_t> key(
      const CrossedJesterGhostInformation::KnowledgeState& state);

    std::vector<CrossedJesterGhostInformation::KnowledgeState> nodes_;
    std::map<std::vector<std::uint8_t>, NodeId> interner_;
};

}  // namespace Stockfish::Ultimate::CrossedJesterGhostSolver

#endif
