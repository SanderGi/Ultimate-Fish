/*
  Ultimate Fish - exact monotone information-set fixed point
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#ifndef ULTIMATE_INFORMATION_SOLVER_H_INCLUDED
#define ULTIMATE_INFORMATION_SOLVER_H_INCLUDED

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Stockfish::Ultimate {

using InformationToken = std::uint32_t;

// Tokens below InformationTrue name variables.  The two highest tokens are
// constants, so equations can cross-probe already solved information strata
// without allocating synthetic variables.
inline constexpr InformationToken InformationFalse =
    static_cast<InformationToken>(-1);
inline constexpr InformationToken InformationTrue = InformationFalse - 1;
inline constexpr std::uint32_t InformationNoWitness =
    static_cast<std::uint32_t>(-1);

struct InformationPair {
  InformationToken first = InformationFalse;
  InformationToken second = InformationFalse;
};

enum class InformationEquationKind : std::uint8_t {
  Or,
  And,
  OrOfPairs,
};

struct InformationSolveSummary {
  std::uint64_t variables = 0;
  std::uint64_t reverseEdges = 0;
  std::uint64_t activated = 0;
  std::uint64_t bellmanResidual = 0;
  std::uint64_t rankResidual = 0;
};

// Solve a finite monotone Boolean equation system by its exact least fixed
// point.  Definitions may refer to variables which are defined later.  Every
// variable must receive exactly one equation before solve().
//
// Scratch arrays, the reverse CSR, and the activation queue are anonymous
// mmap-backed files created beneath scratchDirectory.  Their directory entries
// are unlinked immediately, so normal destruction and abnormal process exit do
// not leave multi-gigabyte artifacts behind.  No equation, edge, or activation
// is sampled or capped.
class InformationFixedPoint {
public:
  InformationFixedPoint(std::uint32_t variableCount,
                        std::string scratchDirectory);
  ~InformationFixedPoint();

  InformationFixedPoint(const InformationFixedPoint &) = delete;
  InformationFixedPoint &operator=(const InformationFixedPoint &) = delete;
  InformationFixedPoint(InformationFixedPoint &&) noexcept;
  InformationFixedPoint &operator=(InformationFixedPoint &&) noexcept;

  void define_or(std::uint32_t parent, const InformationToken *children,
                 std::size_t count);
  void define_and(std::uint32_t parent, const InformationToken *children,
                  std::size_t count);
  void define_or_of_pairs(std::uint32_t parent, const InformationPair *pairs,
                          std::size_t count);

  void define_or(std::uint32_t parent,
                 const std::vector<InformationToken> &children) {
    define_or(parent, children.data(), children.size());
  }
  void define_and(std::uint32_t parent,
                  const std::vector<InformationToken> &children) {
    define_and(parent, children.data(), children.size());
  }
  void define_or_of_pairs(std::uint32_t parent,
                          const std::vector<InformationPair> &pairs) {
    define_or_of_pairs(parent, pairs.data(), pairs.size());
  }

  // Builds the reverse CSR, propagates the least fixed point, and performs a
  // full Bellman/rank verification.  A nonzero residual is an error.
  [[nodiscard]] InformationSolveSummary solve();
  [[nodiscard]] InformationSolveSummary verify() const;

  [[nodiscard]] bool value(InformationToken token) const;
  [[nodiscard]] std::uint32_t activation_rank(std::uint32_t variable) const;

  // OR: index of a minimum-rank true child.
  // AND: index of the last (maximum-rank) required child.
  // OR-of-pairs: index of a minimum-rank satisfied pair/action.
  // Empty AND has no witness.
  [[nodiscard]] std::uint32_t witness_index(std::uint32_t variable) const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace Stockfish::Ultimate

#endif
