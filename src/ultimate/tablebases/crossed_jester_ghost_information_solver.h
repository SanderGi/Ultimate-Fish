/*
  Ultimate Fish - exact crossed Jester/Ghost information solver core
  Copyright (C) 2026 Ultimate Fish contributors
  GPLv3 or later.
*/

#ifndef ULTIMATE_CROSSED_JESTER_GHOST_INFORMATION_SOLVER_H_INCLUDED
#define ULTIMATE_CROSSED_JESTER_GHOST_INFORMATION_SOLVER_H_INCLUDED

#include "crossed_jester_ghost_information_model.h"

#include <cstdint>
#include <iosfwd>
#include <limits>
#include <map>
#include <optional>
#include <utility>
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

struct GraphArchiveCertificate {
    std::uint64_t nodes = 0;
    std::uint64_t payloadBytes = 0;
    std::uint64_t keyRoundtripResidual = 0;
    std::uint64_t canonicalResidual = 0;
    std::uint64_t duplicateResidual = 0;
};

struct FreshSeedCertificate {
    std::uint32_t rawBegin = 0;
    std::uint32_t rawCount = 0;
    std::uint64_t admitted = 0;
    std::uint64_t empty = 0;
    std::uint64_t unique = 0;
    std::uint64_t duplicate = 0;
    std::uint64_t codecResidual = 0;
};

struct FreshSeedResult {
    std::vector<std::optional<NodeId>> roots;
    FreshSeedCertificate certificate;
};

struct DiscoveryCertificate {
    std::uint64_t expanded = 0;
    std::uint64_t nodesBefore = 0;
    std::uint64_t nodesAfter = 0;
    std::uint64_t actions = 0;
    std::uint64_t observations = 0;
    std::uint64_t outcomes = 0;
    std::uint64_t newlyInterned = 0;
    std::uint64_t residual = 0;
    bool closed = false;
};

struct DiscoveryArchiveCertificate {
    std::uint64_t roots = 0;
    std::uint64_t emptyRoots = 0;
    std::uint64_t expanded = 0;
    GraphArchiveCertificate graph;
    std::uint64_t rootBoundsResidual = 0;
    std::uint64_t cursorResidual = 0;
    std::uint64_t extentResidual = 0;
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

struct ExternalForceQuery {
    Color target = Color::White;
    CrossedJesterGhostInformation::ChildDomain domain =
      CrossedJesterGhostInformation::ChildDomain::Invalid;
    std::uint32_t bucket = 0;
    std::uint32_t childAtom = 0;
    CrossedJesterGhostInformation::ClassifiedChild actual;
    // Exact successor histories in the target player's inherited private
    // knowledge cell. Duplicates are preserved here; a lower-domain adapter
    // may project/deduplicate physical coordinates only after authenticating
    // the complete history cell.
    std::vector<CrossedJesterGhostInformation::ClassifiedChild> belief;
};

[[nodiscard]] ExternalForceQuery external_force_query(
  const NodeExpansion& expansion, Color target,
  const ChildReference& reference);

// ExactTerminal is the only external domain resolved without a dependency.
// The public observation must make its winner (or draw) uniform throughout
// the inherited target cell; mixed outcomes are a certificate failure.
[[nodiscard]] bool exact_terminal_force(const ExternalForceQuery& query);

struct LowerJesterForceQuery {
    Color target = Color::White;
    bool targetOwnsJester = false;
    std::uint32_t actual = 0;
    CrossedJesterGhostInformation::LowerJesterSet belief;
};

struct LowerGhostForceQuery {
    Color target = Color::White;
    CrossedJesterGhostInformation::Role targetRole =
      CrossedJesterGhostInformation::Role::Observer;
    CrossedJesterGhostInformation::LowerGhostState actual;
    CrossedJesterGhostInformation::LowerGhostImage belief;
};

[[nodiscard]] LowerJesterForceQuery lower_jester_force_query(
  const ExternalForceQuery& query);
[[nodiscard]] LowerGhostForceQuery lower_ghost_force_query(
  const ExternalForceQuery& query);

class LowerForceOracle {
   public:
    virtual ~LowerForceOracle() = default;
    [[nodiscard]] virtual bool force(
      const LowerJesterForceQuery& query) const = 0;
    [[nodiscard]] virtual bool force(
      const LowerGhostForceQuery& query) const = 0;
};

// There is intentionally no fallback branch. A lower material edge must pass
// through the supplied authenticated oracle; unsupported domains fail closed.
[[nodiscard]] bool resolve_external_force(
  const ExternalForceQuery& query, const LowerForceOracle& oracle);

// Collision-free in-memory reference arena. Production graph capture will
// stream the same portable keys to named scratch; this implementation is the
// deterministic oracle used by focused tests and restore verification.
class Arena {
   public:
    Arena() = default;
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;
    Arena(Arena&&) = default;
    Arena& operator=(Arena&&) = default;

    [[nodiscard]] FreshSeedResult seed_fresh_range(
      std::uint32_t rawBegin, std::uint32_t rawCount);
    [[nodiscard]] NodeId intern(
      const CrossedJesterGhostInformation::KnowledgeState& state);
    [[nodiscard]] std::optional<NodeId> find(
      const CrossedJesterGhostInformation::KnowledgeState& state) const;
    [[nodiscard]] NodeExpansion regenerate(NodeId node, bool allowNew);
    [[nodiscard]] TargetBellmanPlan bellman_plan(
      NodeId node, Color target, bool allowNew);
    [[nodiscard]] GraphArchiveCertificate write(std::ostream& output) const;
    [[nodiscard]] static std::pair<Arena, GraphArchiveCertificate> read(
      std::istream& input);

    [[nodiscard]] CrossedJesterGhostInformation::KnowledgeState node(
      NodeId id) const;
    [[nodiscard]] const std::vector<std::uint8_t>& encoded_key(
      NodeId id) const;
    [[nodiscard]] std::size_t size() const;

   private:
    [[nodiscard]] static std::vector<std::uint8_t> key(
      const CrossedJesterGhostInformation::KnowledgeState& state);

    // The map owns the one canonical byte string for each node. Stable key
    // addresses provide the ID-indexed arena without retaining a second,
    // heap-heavy decoded KnowledgeState for every discovered node.
    std::vector<const std::vector<std::uint8_t>*> nodes_;
    std::map<std::vector<std::uint8_t>, NodeId> interner_;
};

// Resumable full-closure driver. A checkpoint contains the exact raw-root map,
// expansion cursor, and portable graph arena; restoring it never depends on a
// process heap or anonymous scratch file.
class GraphDiscovery {
   public:
    GraphDiscovery() = default;
    GraphDiscovery(const GraphDiscovery&) = delete;
    GraphDiscovery& operator=(const GraphDiscovery&) = delete;
    GraphDiscovery(GraphDiscovery&&) = default;
    GraphDiscovery& operator=(GraphDiscovery&&) = default;

    [[nodiscard]] static GraphDiscovery seed(std::uint32_t rawBegin,
                                             std::uint32_t rawCount);
    [[nodiscard]] FreshSeedCertificate append_seed(
      std::uint32_t rawCount);
    [[nodiscard]] DiscoveryCertificate advance(
      std::uint64_t maximumExpansions);
    [[nodiscard]] DiscoveryArchiveCertificate write(
      std::ostream& output) const;
    [[nodiscard]] static std::pair<GraphDiscovery,
                                   DiscoveryArchiveCertificate>
      read(std::istream& input);

    [[nodiscard]] std::uint32_t raw_begin() const;
    [[nodiscard]] const std::vector<NodeId>& roots() const;
    [[nodiscard]] std::uint64_t expanded() const;
    [[nodiscard]] bool closed() const;
    [[nodiscard]] Arena& arena();
    [[nodiscard]] const Arena& arena() const;

   private:
    static constexpr NodeId EmptyRoot = std::numeric_limits<NodeId>::max();

    Arena arena_;
    std::uint32_t rawBegin_ = 0;
    std::vector<NodeId> roots_;
    std::uint64_t expanded_ = 0;
};

}  // namespace Stockfish::Ultimate::CrossedJesterGhostSolver

#endif
