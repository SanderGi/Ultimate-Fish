/*
  Ultimate Fish - exact K+Ghost+Ghost versus K information solver
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#ifndef ULTIMATE_GHOST_PAIR_INFORMATION_SOLVER_H_INCLUDED
#define ULTIMATE_GHOST_PAIR_INFORMATION_SOLVER_H_INCLUDED

#include "ghost_pair_information_model.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Stockfish::Ultimate::GhostPairInformation {

// Exact external-memory ROBDD over the complete unordered-pair universe.
// Variables are pair worlds, not square marginals.  In particular, a formula
// over variables {b2,c3} and {e5,f6} cannot accidentally admit either cross
// product.  uint16_t node variables cover all C(78,2)=3,003 worlds.
class PairRobdd {
  public:
    using Id = std::uint32_t;
    static constexpr Id False = 0;
    static constexpr Id True = 1;
    static constexpr Id Invalid = ~Id{0};

    struct Limits {
        std::uint32_t variables = MaximumWorlds;
        std::uint32_t maxNodes = 500'000'000;
        std::uint64_t uniqueSlots = std::uint64_t{1} << 30;
        std::uint64_t applyCacheEntries = 1'000'000;
        std::uint64_t unaryCacheEntries = 500'000;
        std::uint64_t budgetBytes = 128ULL << 30;
    };

    struct CompactionCertificate {
        std::uint32_t oldNodes = 0;
        std::uint32_t newNodes = 0;
        std::uint64_t roots = 0;
        std::uint64_t structuralResidual = 0;
        std::uint64_t rootResidual = 0;
    };

    struct NodeRecord {
        std::uint16_t variable = MaximumWorlds;
        Id low = False;
        Id high = False;
    };

    PairRobdd(const std::string& prefix, Limits limits, bool create);
    ~PairRobdd();
    PairRobdd(PairRobdd&&) noexcept;
    PairRobdd& operator=(PairRobdd&&) noexcept;
    PairRobdd(const PairRobdd&) = delete;
    PairRobdd& operator=(const PairRobdd&) = delete;

    [[nodiscard]] Id variable(unsigned variable);
    [[nodiscard]] Id logical_not(Id root);
    [[nodiscard]] Id logical_and(Id lhs, Id rhs);
    [[nodiscard]] Id logical_or(Id lhs, Id rhs);
    [[nodiscard]] Id ite(Id condition, Id whenTrue, Id whenFalse);
    [[nodiscard]] Id any(const PairMask& variables);
    [[nodiscard]] Id subset_of(const PairMask& variables,
                               unsigned variableCount);
    [[nodiscard]] Id compose(Id root, const std::vector<Id>& image,
                             std::uint64_t relationId);
    [[nodiscard]] bool evaluate(Id root, const PairMask& assignment) const;
    [[nodiscard]] bool is_upward_closed(Id root, const PairMask& allowed);
    [[nodiscard]] bool is_downward_closed(Id root);
    [[nodiscard]] std::uint32_t node_count() const;
    [[nodiscard]] NodeRecord node_record(Id id) const;

    [[nodiscard]] std::pair<PairRobdd, CompactionCertificate> compact(
      const std::string& replacementPrefix, const std::string& remapPath,
      std::vector<Id>& roots);
    [[nodiscard]] static std::uint64_t required_bytes(const Limits& limits);

  private:
    class Impl;
    Impl* impl_ = nullptr;
};

enum class CompiledChildDomain : std::uint8_t {
    SameClass,
    LowerGhost,
    ExactTerminal,
};

struct CompiledEdge {
    std::uint32_t relation = 0;
    std::uint32_t action = 0;
    std::uint32_t childGeometry = 0;
    std::uint32_t childConcrete = 0;
    std::uint16_t childActual = 0;
    CompiledChildDomain domain = CompiledChildDomain::ExactTerminal;
    std::uint8_t terminalForces = 0;
};

struct TransitionCompileOptions {
    std::string prefix;
    std::string sourceSha256;
    std::string modelSha256;
    std::string observationSha256;
    std::uint32_t rawGeometryBegin = 0;
    std::uint32_t rawGeometryCount = 0;
    bool exhaustiveSymmetryCertificate = true;
};

struct TransitionCertificate {
    std::uint64_t rawGeometries = 0;
    std::uint64_t canonicalGeometries = 0;
    std::uint64_t worlds = 0;
    std::uint64_t liveWorlds = 0;
    std::uint64_t admittedFreshWorlds = 0;
    std::uint64_t terminalWorlds = 0;
    std::uint64_t actions = 0;
    std::uint64_t observations = 0;
    std::uint64_t edges = 0;
    std::uint64_t sameClass = 0;
    std::uint64_t lowerGhost = 0;
    std::uint64_t exactTerminal = 0;
    std::uint64_t codecResidual = 0;
    std::uint64_t actionResidual = 0;
    std::uint64_t decisionResidual = 0;
    std::uint64_t transitionResidual = 0;
    std::uint64_t symmetryResidual = 0;
    std::string payloadSha256;
};

[[nodiscard]] TransitionCertificate compile_transition_database(
  const TransitionCompileOptions& options);
[[nodiscard]] TransitionCertificate verify_transition_database(
  const std::string& prefix, const std::string& sourceSha256,
  const std::string& modelSha256, const std::string& observationSha256,
  bool requireComplete);
[[nodiscard]] TransitionCertificate merge_transition_databases(
  const std::vector<std::string>& shardPrefixes,
  const std::string& outputPrefix, const std::string& sourceSha256,
  const std::string& modelSha256, const std::string& observationSha256,
  bool requireComplete = true);

struct ResourceLimits {
    std::uint64_t maxDiskBytes = 0;
    std::uint64_t maxResidentBytes = 0;
    std::uint64_t maxBddNodes = 0;
    std::uint64_t minFreeDiskBytes = 0;
};

struct ResourceEstimate {
    std::uint64_t canonicalGeometries = 0;
    std::uint64_t correlatedWorlds = 0;
    std::uint64_t transitionBytes = 0;
    std::uint64_t rootBytes = 0;
    std::uint64_t rootCatalogBytes = 0;
    std::uint64_t bddBytes = 0;
    std::uint64_t compactionBytes = 0;
    std::uint64_t peakDiskBytes = 0;
    std::uint64_t peakResidentBytes = 0;
    bool admitted = false;
};

[[nodiscard]] ResourceEstimate sampled_resource_preflight(
  const TransitionCertificate& sample, std::uint64_t sampledRawGeometries,
  const PairRobdd::Limits& bdd, const ResourceLimits& limits);
[[nodiscard]] ResourceEstimate full_domain_preflight(
  const std::string& transitionPrefix, const PairRobdd::Limits& bdd,
  const ResourceLimits& limits, const std::string& scratchPrefix);

struct SolveOptions {
    std::string transitionPrefix;
    std::string sourceTable;
    std::string lowerGhostSidecar;
    std::string scratchPrefix;
    std::string outputOverlay;
    // Compact S3/probe artifact containing every arbitrary-belief force root.
    // Construction-only unique tables and transition edges are excluded.
    std::string outputArbitrarySidecar;
    std::string sourceSha256;
    std::string modelSha256;
    std::string observationSha256;
    std::string lowerGhostSourceSha256;
    std::string lowerGhostModelSha256;
    std::string lowerGhostObservationSha256;
    std::string lowerGhostSidecarSha256;
    PairRobdd::Limits bdd;
    ResourceLimits resources;
    std::uint32_t compactEvery = 4;
    std::uint32_t measureIterations = 0;
};

struct SolveCertificate {
    std::uint64_t iterations = 0;
    std::uint64_t bddNodes = 0;
    std::uint64_t domainRoots = 0;
    std::uint64_t domainCacheHits = 0;
    std::uint64_t domainCacheMisses = 0;
    std::uint64_t domainCacheEntries = 0;
    std::uint64_t lowerGhostMaskProbes = 0;
    std::uint64_t compactions = 0;
    std::uint64_t bellmanResidual = 0;
    std::uint64_t rankResidual = 0;
    std::uint64_t singletonResidual = 0;
    std::uint64_t conservationResidual = 0;
    std::uint64_t dualWinResidual = 0;
    std::uint64_t partitionResidual = 0;
    std::uint64_t observationResidual = 0;
    std::array<std::uint64_t, 2> informationSets{};
    std::array<std::uint64_t, 2> legalRealizations{};
    std::array<std::uint64_t, 2> unreachableRealizations{};
    std::array<std::uint64_t, 2> unresolvedInformationSets{};
    std::array<std::array<std::uint64_t, 4>, 2> totals{};
    std::array<std::array<std::uint64_t, 4>, 2> unreachable{};
    std::string overlaySha256;
    std::string transitionPayloadSha256;
    std::string transitionHeaderSha256;
    std::string transitionMarkerSha256;
    std::string lowerGhostSidecarSha256;
    std::string arbitrarySidecarSha256;
};

[[nodiscard]] SolveCertificate solve_exact(const SolveOptions& options);
[[nodiscard]] SolveCertificate verify_exact_overlay(
  const SolveOptions& options);

struct ArbitrarySidecarCertificate {
    std::uint64_t nodes = 0;
    std::uint64_t geometries = 0;
    std::uint64_t strata = 0;
    std::uint64_t ownerRoots = 0;
    std::uint64_t bytes = 0;
    std::string payloadSha256;
    std::string fileSha256;
    std::string transitionPayloadSha256;
    std::string transitionHeaderSha256;
    std::string transitionMarkerSha256;
    std::string lowerGhostSidecarSha256;
};

// UFGG1 is the permanent compact all-beliefs probe artifact.  Its payload is:
// the reduced ordered node arena, 40-byte canonical probe-geometry records,
// exact correlated decision strata, dense-actual-to-stratum reverse map,
// informed
// owner roots (ranked by live actuals in the reverse map), and observer roots
// (one per stratum).  Thus terminal/admission masks, transition edges, caches,
// and construction unique tables can be discarded after this file and the
// proof certificate are
// archived.  The header binds all offsets, dimensions, dependency digests,
// transition regeneration marker, and payload digest.
//
// Structurally authenticates that file against the exact dependency tuple in
// options.  transitionPrefix may be empty for a standalone S3/download check;
// when supplied, its header, payload, and exhaustive-regeneration marker are
// rehashed as well.  It never regenerates or samples a belief; Bellman/rank
// certificates are produced before the writer is called.
// Archive and compare `fileSha256` from the solve manifest before probing an
// S3 download; the file cannot recursively embed its own full-file digest.
[[nodiscard]] ArbitrarySidecarCertificate verify_arbitrary_sidecar(
  const std::string& path, const SolveOptions& options);

// Read-only exact query interface for the permanent UFGG1 artifact.  `worlds`
// is the complete correlated information set in `frame`, and `actual` must be
// one member.  The implementation D2-canonicalizes the whole set, rejects a
// set spanning private legal-dot decision cells, and evaluates the archived
// owner/observer force function without consulting transition scratch.
class ArbitrarySidecarProbe {
  public:
    ArbitrarySidecarProbe(const std::string& path,
                          const SolveOptions& options);
    ~ArbitrarySidecarProbe();
    ArbitrarySidecarProbe(ArbitrarySidecarProbe&&) noexcept;
    ArbitrarySidecarProbe& operator=(ArbitrarySidecarProbe&&) noexcept;
    ArbitrarySidecarProbe(const ArbitrarySidecarProbe&) = delete;
    ArbitrarySidecarProbe& operator=(const ArbitrarySidecarProbe&) = delete;

    [[nodiscard]] bool owner_forces(const PublicFrame& frame,
                                    const PairWorld& actual,
                                    const PairMask& worlds) const;
    [[nodiscard]] bool observer_forces(const PublicFrame& frame,
                                       const PairWorld& actual,
                                       const PairMask& worlds) const;
    [[nodiscard]] const ArbitrarySidecarCertificate& certificate() const;

  private:
    class Impl;
    Impl* impl_ = nullptr;
};

// Exhaustive truth-table and structural tests over a deliberately small exact
// universe.  This is a test hook, not a sampled production solve.
void exact_small_domain_self_test(const std::string& scratchPrefix);

// Raw public geometries: side, ordered Kings, and exactly one of HH, HV, VV.
inline constexpr std::uint32_t VisibilityGeometryCount =
  1 + (Position::BoardSquares - 2) + MaximumWorlds;
inline constexpr std::uint32_t RawGeometryCount =
  2 * Position::BoardSquares * (Position::BoardSquares - 1) *
  VisibilityGeometryCount;

}  // namespace Stockfish::Ultimate::GhostPairInformation

#endif  // ULTIMATE_GHOST_PAIR_INFORMATION_SOLVER_H_INCLUDED
