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
