/* Ultimate Fish exact opposed Ghost/Ghost fixed point. GPLv3+. */

#ifndef ULTIMATE_OPPOSED_GHOST_PAIR_INFORMATION_FIXED_POINT_H_INCLUDED
#define ULTIMATE_OPPOSED_GHOST_PAIR_INFORMATION_FIXED_POINT_H_INCLUDED

#include "opposed_ghost_pair_information_solver.h"
#include "information_solver.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Stockfish::Ultimate::OpposedGhostPairSolver {

struct FixedPointCertificate {
    Color target = Color::White;
    std::uint64_t nodes = 0;
    std::uint64_t atomVariables = 0;
    std::uint64_t gateVariables = 0;
    std::uint64_t totalVariables = 0;
    std::uint64_t internalTokens = 0;
    std::uint64_t externalTrue = 0;
    std::uint64_t externalFalse = 0;
    std::uint64_t equationResidual = 0;
    InformationSolveSummary solve;
};

struct PackedForcePlane {
    FixedPointCertificate certificate;
    std::vector<std::uint32_t> atomBase;
    std::vector<std::uint8_t> values;

    [[nodiscard]] bool value(NodeId node, std::uint32_t atom) const;
};

struct FixedPointPreflight {
    Color target = Color::White;
    std::uint64_t nodes = 0;
    std::uint64_t atomVariables = 0;
    std::uint64_t gateVariables = 0;
    std::uint64_t totalVariables = 0;
    std::uint64_t tokenReferences = 0;
    std::uint64_t reverseEdges = 0;
    std::uint64_t externalConstants = 0;
    std::uint64_t peakScratchBytes = 0;
    std::uint64_t residual = 0;
};

class FixedPointSolution {
   public:
    FixedPointSolution(FixedPointSolution&&) noexcept = default;
    FixedPointSolution& operator=(FixedPointSolution&&) noexcept = default;
    FixedPointSolution(const FixedPointSolution&) = delete;
    FixedPointSolution& operator=(const FixedPointSolution&) = delete;

    [[nodiscard]] bool value(NodeId node, std::uint32_t atom) const;
    [[nodiscard]] std::uint32_t activation_rank(
      NodeId node, std::uint32_t atom) const;
    [[nodiscard]] std::uint32_t witness_index(
      NodeId node, std::uint32_t atom) const;
    [[nodiscard]] const FixedPointCertificate& certificate() const;
    // Retains only the solved force bitplane and node-to-atom offsets. This
    // lets production solve White and Black sequentially instead of keeping
    // two multi-gigabyte equation/reverse-CSR arenas resident at once.
    [[nodiscard]] PackedForcePlane pack() const;

   private:
    friend FixedPointSolution solve_closed_graph(
      Arena&, Color, const LowerForceOracle&, const std::string&);

    FixedPointSolution(std::unique_ptr<InformationFixedPoint> solver,
                       std::vector<std::uint32_t> atomBase,
                       FixedPointCertificate certificate);
    [[nodiscard]] InformationToken token(NodeId node,
                                         std::uint32_t atom) const;

    std::unique_ptr<InformationFixedPoint> solver_;
    std::vector<std::uint32_t> atomBase_;
    FixedPointCertificate certificate_;
};

// Regenerates every closed graph node twice: first to establish the exact
// variable layout, then to define every equation. Any newly discovered child,
// unresolved lower query, token overflow, or Bellman residual is fatal.
[[nodiscard]] FixedPointSolution solve_closed_graph(
  Arena& arena, Color target, const LowerForceOracle& oracle,
  const std::string& scratchDirectory);

// Regenerates every equation without allocating the disk-backed solver.  The
// returned scratch extent is the exact maximum of the fixed-point file layouts
// with every reverse edge counted, and is therefore a hard pre-launch gate.
[[nodiscard]] FixedPointPreflight preflight_closed_graph(
  Arena& arena, Color target);

}  // namespace Stockfish::Ultimate::OpposedGhostPairSolver

#endif

