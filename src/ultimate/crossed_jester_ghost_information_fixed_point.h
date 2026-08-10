/* Ultimate Fish exact crossed Jester/Ghost fixed point. GPLv3+. */

#ifndef ULTIMATE_CROSSED_JESTER_GHOST_INFORMATION_FIXED_POINT_H_INCLUDED
#define ULTIMATE_CROSSED_JESTER_GHOST_INFORMATION_FIXED_POINT_H_INCLUDED

#include "crossed_jester_ghost_information_solver.h"
#include "information_solver.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Stockfish::Ultimate::CrossedJesterGhostSolver {

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

}  // namespace Stockfish::Ultimate::CrossedJesterGhostSolver

#endif
