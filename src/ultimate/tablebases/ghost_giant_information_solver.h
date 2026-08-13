/* Ultimate Fish exact Giant/Ghost public-information solver. GPLv3+. */

#ifndef ULTIMATE_GHOST_GIANT_INFORMATION_SOLVER_H_INCLUDED
#define ULTIMATE_GHOST_GIANT_INFORMATION_SOLVER_H_INCLUDED

#include "ghost_giant_information_model.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Stockfish::Ultimate::GhostGiantExact {

enum class Orientation : std::uint8_t { Same, Opposing };

struct TransitionOptions {
    Orientation orientation = Orientation::Same;
    std::string prefix;
    std::string lowerGiantTable;
    std::string lowerGiantSha256;
    std::string lowerGiantSourceSha256;
    std::string lowerGiantModelSha256;
    std::string modelSha256;
    std::string observationSha256;
    std::uint32_t geometryBegin = 0;
    std::uint32_t geometryCount = 0;
};

struct SolveOptions {
    Orientation orientation = Orientation::Same;
    std::string transitionPrefix;
    std::string sourceTable;
    std::string lowerGhostSidecar;
    std::string scratchPrefix;
    std::string outputOverlay;
    std::string outputArbitrary;
    std::string sourceSha256;
    std::string modelSha256;
    // The transition graph may be reused when a solver-only proof change
    // leaves compilation semantics byte-for-byte unchanged.
    std::string transitionModelSha256;
    std::string observationSha256;
    std::string lowerGhostSourceSha256;
    std::string lowerGhostModelSha256;
    std::string lowerGhostObservationSha256;
    std::string lowerGhostSidecarSha256;
    std::string lowerGiantTable;
    std::string lowerGiantFullSha256;
    std::string lowerGiantSourceSha256;
    std::string lowerGiantModelSha256;
    std::uint32_t maxNodes = 500'000'000;
    std::uint64_t uniqueSlots = std::uint64_t{1} << 30;
    std::uint32_t compactEvery = 1;
    std::uint32_t measureIterations = 0;
};

struct ProbeBindings {
    Orientation orientation = Orientation::Same;
    std::string sourceSha256;
    std::string modelSha256;
    std::string observationSha256;
    std::string lowerGhostSidecarSha256;
    std::string lowerGiantFullSha256;
    std::string lowerGiantSourceSha256;
    std::string lowerGiantModelSha256;
    std::string arbitrarySidecarSha256;
};

struct ResourceEstimate {
    std::uint64_t geometries = 359'100;
    std::uint64_t concreteWorlds = 26'573'400;
    std::uint64_t ownerRoots = 28'728'000;
    std::uint64_t estimatedTransitionBytes = 13'000'000'000ULL;
    std::uint64_t peakScratchBytes = 32ULL << 30;
    std::uint64_t peakResidentBytes = 18ULL << 30;
};

struct SolveCertificate {
    std::uint64_t dualForceResidual = 0;
    std::uint64_t structuralResidual = 0;
    std::uint64_t singletonResidual = 0;
    std::uint64_t sourceRemapResidual = 0;
    std::string normalizedSourceSha256;
    std::string transitionPayloadSha256;
    std::string arbitrarySha256;
};

void compile_transitions(const TransitionOptions& options);
void merge_transitions(const TransitionOptions& output,
                       const std::vector<std::string>& shards,
                       std::uint32_t expectedGeometries);
void verify_transitions(const TransitionOptions& options);
[[nodiscard]] ResourceEstimate resource_estimate();
[[nodiscard]] SolveCertificate solve_exact(const SolveOptions& options);
void exact_self_test(const std::string& scratchPrefix);
[[nodiscard]] std::string verify_source_normalization(
  const std::string& sourceTable, Orientation orientation,
  const std::string& expectedSourceSha256, const std::string& scratchPrefix);
[[nodiscard]] std::uint32_t lower_capture_witness_geometry(
  Orientation orientation);
[[nodiscard]] std::uint32_t decision_witness_geometry(
  Orientation orientation);

class ArbitrarySidecarProbe {
  public:
    ArbitrarySidecarProbe(const std::string& path,
                          const ProbeBindings& bindings);
    ArbitrarySidecarProbe(const std::string& path,
                          const SolveOptions& bindings);
    ~ArbitrarySidecarProbe();
    ArbitrarySidecarProbe(ArbitrarySidecarProbe&&) noexcept;
    ArbitrarySidecarProbe& operator=(ArbitrarySidecarProbe&&) noexcept;
    ArbitrarySidecarProbe(const ArbitrarySidecarProbe&) = delete;
    ArbitrarySidecarProbe& operator=(const ArbitrarySidecarProbe&) = delete;

    [[nodiscard]] bool owner_forces(
      const GhostGiant::SourceState& actual,
      const GhostPublicExtra::GhostMask& belief) const;
    [[nodiscard]] bool observer_forces(
      const GhostGiant::SourceState& actual,
      const GhostPublicExtra::GhostMask& belief) const;

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace Stockfish::Ultimate::GhostGiantExact

#endif
