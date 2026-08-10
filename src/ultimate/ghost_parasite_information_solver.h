/* Ultimate Fish exact Parasite/Ghost public-information solver. GPLv3+. */

#ifndef ULTIMATE_GHOST_PARASITE_INFORMATION_SOLVER_H_INCLUDED
#define ULTIMATE_GHOST_PARASITE_INFORMATION_SOLVER_H_INCLUDED

#include "ghost_public_extra_model.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Stockfish::Ultimate::GhostParasiteExact {

enum class Orientation : std::uint8_t { Same, Opposing };

struct TransitionOptions {
    Orientation orientation = Orientation::Same;
    std::string prefix;
    std::string lowerParasiteTable;
    std::string lowerParasiteSha256;
    std::string lowerParasiteSourceSha256;
    std::string lowerParasiteModelSha256;
    // Opposing Parasite/Ghost captures possess the surviving Ghost.  The
    // resulting visible singleton belongs to the former observer, so it is
    // an exact K+Ghost-v-K child with the two information roles exchanged.
    std::string lowerGhostSidecar;
    std::string lowerGhostSidecarSha256;
    std::string lowerGhostSourceSha256;
    std::string lowerGhostModelSha256;
    std::string lowerGhostObservationSha256;
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
    std::string observationSha256;
    std::string lowerGhostSourceSha256;
    std::string lowerGhostModelSha256;
    std::string lowerGhostObservationSha256;
    std::string lowerGhostSidecarSha256;
    std::string lowerParasiteTable;
    std::string lowerParasiteFullSha256;
    std::string lowerParasiteSourceSha256;
    std::string lowerParasiteModelSha256;
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
    std::string lowerParasiteFullSha256;
    std::string lowerParasiteSourceSha256;
    std::string lowerParasiteModelSha256;
    std::string arbitrarySidecarSha256;
};

struct ResourceEstimate {
    std::uint64_t geometries = 492'960;
    std::uint64_t concreteWorlds = 37'957'920;
    std::uint64_t ownerRoots = 39'436'800;
    std::uint64_t estimatedTransitionBytes = 16'000'000'000ULL;
    std::uint64_t peakScratchBytes = 36ULL << 30;
    std::uint64_t peakResidentBytes = 20ULL << 30;
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
[[nodiscard]] std::uint32_t possession_witness_geometry();
[[nodiscard]] std::uint32_t next_decision_witness_geometry(
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
      const GhostPublicExtra::ConcreteState& actual,
      const GhostPublicExtra::GhostMask& belief) const;
    [[nodiscard]] bool observer_forces(
      const GhostPublicExtra::ConcreteState& actual,
      const GhostPublicExtra::GhostMask& belief) const;

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace Stockfish::Ultimate::GhostParasiteExact

#endif
