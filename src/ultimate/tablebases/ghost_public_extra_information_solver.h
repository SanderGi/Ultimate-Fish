/*
  Ultimate Fish - exact reciprocal public-extra/Ghost information solver
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#ifndef ULTIMATE_GHOST_PUBLIC_EXTRA_INFORMATION_SOLVER_H_INCLUDED
#define ULTIMATE_GHOST_PUBLIC_EXTRA_INFORMATION_SOLVER_H_INCLUDED

#include "ghost_public_extra_model.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Stockfish::Ultimate::GhostPublicExtraExact {

inline constexpr const char* DomainName = "bishop-reciprocal-ghost";

struct TransitionOptions {
    std::string prefix;
    std::uint32_t geometryBegin = 0;
    std::uint32_t geometryCount = 0;
};

struct SolveOptions {
    std::string transitionPrefix;
    std::string sourceTable;
    std::string lowerGhostSidecar;
    std::string scratchPrefix;
    std::string outputOverlay;
    std::string outputArbitrary;
    std::string sourceSha256;
    std::string modelSha256;
    std::string observationSha256;
    std::string lowerGhostSourceSha256;
    std::string lowerGhostModelSha256;
    std::string lowerGhostObservationSha256;
    std::string lowerGhostSidecarSha256;
    std::uint32_t maxNodes = 500'000'000;
    std::uint64_t uniqueSlots = std::uint64_t{1} << 30;
    std::uint64_t applyCacheEntries = 1'000'000;
    std::uint64_t unaryCacheEntries = 500'000;
    std::uint64_t composeCacheEntries = 500'000;
    std::uint64_t bddBudgetBytes = 220ULL << 30;
    std::uint32_t compactEvery = 1;
    std::uint32_t measureIterations = 0;
    bool resumeFixedPoint = false;
    bool resumeConverged = false;
    bool resumeCurrentInNextSlot = false;
    char resumeBddSlot = 'a';
    std::uint64_t resumeIteration = 0;
};

struct ResourceEstimate {
    std::uint64_t geometries = 0;
    std::uint64_t variables = 0;
    std::uint64_t ownerRoots = 0;
    std::uint64_t estimatedTransitionBytes = 0;
    std::uint64_t rootBytes = 0;
    std::uint64_t bddBytes = 0;
    std::uint64_t peakScratchBytes = 0;
    std::uint64_t peakResidentBytes = 0;
};

struct SolveCertificate {
    std::uint64_t arbitraryDualForceResidual = 0;
    std::uint64_t arbitraryStructuralResidual = 0;
    std::uint64_t arbitrarySingletonResidual = 0;
    std::string transitionHeaderSha256;
    std::string transitionPayloadSha256;
    std::string transitionVerifiedSha256;
    std::string arbitrarySha256;
};

// Engine probes authenticate the complete immutable UFGX2 digest and its
// embedded semantic bindings, but deliberately do not require the multi-GB
// transition database or source .uftb to remain installed. Strict restoration
// after a solve uses SolveOptions and additionally rehashes every dependency.
struct ProbeBindings {
    std::string sourceSha256;
    std::string modelSha256;
    std::string observationSha256;
    std::string lowerGhostSidecarSha256;
    std::string arbitrarySidecarSha256;
};

void compile_transitions(const TransitionOptions& options);
void merge_transitions(const std::string& outputPrefix,
                       const std::vector<std::string>& shards,
                       std::uint32_t expectedGeometries);
void verify_transitions(const std::string& prefix);
[[nodiscard]] ResourceEstimate resource_estimate();
[[nodiscard]] SolveCertificate solve_exact(const SolveOptions& options);
void exact_self_test(const std::string& scratchPrefix);

// UFGX2 is a self-contained, little-endian mmap artifact.  It stores the
// collision-free 80-variable ROBDD, canonical physical geometry catalog,
// exact decision strata, owner roots for every actual Ghost square, observer
// roots for every history-refined legal-dot cell, and visible singleton
// outcomes.  The source/model/observation/lower-UFGM and all six transition
// component digests are authenticated before any query.
class ArbitrarySidecarProbe {
  public:
    ArbitrarySidecarProbe(const std::string& path,
                          const SolveOptions& bindings);
    ArbitrarySidecarProbe(const std::string& path,
                          const ProbeBindings& bindings);
    ~ArbitrarySidecarProbe();
    ArbitrarySidecarProbe(ArbitrarySidecarProbe&&) noexcept;
    ArbitrarySidecarProbe& operator=(ArbitrarySidecarProbe&&) noexcept;
    ArbitrarySidecarProbe(const ArbitrarySidecarProbe&) = delete;
    ArbitrarySidecarProbe& operator=(const ArbitrarySidecarProbe&) = delete;

    [[nodiscard]] bool owner_forces(
      const GhostPublicExtra::ConcreteState& actual,
      const GhostPublicExtra::GhostMask& belief) const;
    [[nodiscard]] bool observer_forces(
      const GhostPublicExtra::ConcreteState& actual,
      const GhostPublicExtra::GhostMask& belief) const;
    [[nodiscard]] const SolveCertificate& certificate() const;

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace Stockfish::Ultimate::GhostPublicExtraExact

#endif
