/*
  Ultimate Fish - exact K+Jester+Ghost versus K information solver
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#ifndef ULTIMATE_JESTER_GHOST_INFORMATION_SOLVER_H_INCLUDED
#define ULTIMATE_JESTER_GHOST_INFORMATION_SOLVER_H_INCLUDED

#include "jester_ghost_information_model.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Stockfish::Ultimate::JesterGhostInformation {

// The production domain is a correlated set of 160 Boolean membership
// variables (two royal assignments times 80 possible Ghost squares).  It must
// never be represented as two independent Ghost beliefs: public actions can
// correlate royal identity and Ghost location.  ProductRobdd stores an exact
// ROBDD with the first 80 variables in an external upper arena and delegates
// the final 80-variable suffix to the already certified ExternalRobdd.
class ProductRobdd {
  public:
    using Id = std::uint64_t;
    static constexpr Id Invalid = ~Id{0};

    struct Limits {
        std::uint64_t maxUpperNodes = 0;
        std::uint64_t upperUniqueSlots = 0;
        std::uint64_t upperCacheEntries = 0;
        std::uint64_t budgetBytes = 0;
        std::uint32_t lowerMaxNodes = 0;
        std::uint64_t lowerUniqueSlots = 0;
        std::uint64_t lowerApplyCacheEntries = 0;
        std::uint64_t lowerUnaryCacheEntries = 0;
        std::uint64_t lowerComposeCacheEntries = 0;
    };

    struct CompactionCertificate {
        std::uint64_t oldUpperNodes = 0;
        std::uint64_t newUpperNodes = 0;
        std::uint32_t oldLowerNodes = 0;
        std::uint32_t newLowerNodes = 0;
        std::uint64_t rootResidual = 0;
        std::uint64_t structuralResidual = 0;
    };

    struct SuffixNode {
        std::uint8_t variable = Position::BoardSquares;
        std::uint32_t low = 0;
        std::uint32_t high = 0;
    };

    struct UpperNodeRecord {
        std::uint8_t variable = Position::BoardSquares;
        Id low = Invalid;
        Id high = Invalid;
    };

    ProductRobdd(const std::string& prefix, Limits limits, bool create);
    ~ProductRobdd();
    ProductRobdd(ProductRobdd&&) noexcept;
    ProductRobdd& operator=(ProductRobdd&&) noexcept;
    ProductRobdd(const ProductRobdd&) = delete;
    ProductRobdd& operator=(const ProductRobdd&) = delete;

    [[nodiscard]] Id constant(bool value) const;
    [[nodiscard]] Id variable(unsigned variable);
    [[nodiscard]] Id logical_not(Id root);
    [[nodiscard]] Id logical_and(Id lhs, Id rhs);
    [[nodiscard]] Id logical_or(Id lhs, Id rhs);
    [[nodiscard]] Id ite(Id condition, Id whenTrue, Id whenFalse);
    [[nodiscard]] Id any(const ProductMask& variables);
    [[nodiscard]] Id subset_of(const ProductMask& variables);
    [[nodiscard]] Id compose(Id root,
                             const std::array<Id, ProductVariables>& image,
                             std::uint64_t relationId);
    // Imports an audited 80-variable ROBDD without reinterpretation. The
    // complete tuple of every node is checked after insertion.
    [[nodiscard]] std::vector<Id> import_suffix(
      const std::vector<SuffixNode>& nodes);
    [[nodiscard]] bool evaluate(Id root, const ProductMask& assignment) const;
    [[nodiscard]] bool is_upward_closed(Id root,
                                        const ProductMask& allowed);
    [[nodiscard]] bool is_downward_closed(Id root);
    [[nodiscard]] std::uint64_t upper_node_count() const;
    [[nodiscard]] std::uint32_t lower_node_count() const;
    [[nodiscard]] UpperNodeRecord upper_node_record(Id id) const;
    [[nodiscard]] SuffixNode lower_node_record(std::uint32_t id) const;

    // Compaction is atomic at the caller level: the replacement is built at a
    // different prefix, all supplied roots are remapped and truth-checked, and
    // the old object remains valid until the returned object is installed.
    [[nodiscard]] std::pair<ProductRobdd, CompactionCertificate> compact(
      const std::string& replacementPrefix, const std::string& remapPath,
      std::vector<Id>& roots);

    [[nodiscard]] static std::uint64_t required_bytes(const Limits& limits);

  private:
    class Impl;
    Impl* impl_ = nullptr;
};

enum class CompiledChildDomain : std::uint8_t {
    SameClass,
    LowerJester,
    LowerGhost,
    ExactTerminal,
};

// Complete Move identity is retained independently of UI marker dots.  Dots
// refine a mover's private information; they are not an action encoding.
struct CompiledAction {
    ActionKey key;
    std::uint32_t id = 0;
};

struct CompiledEdge {
    std::uint32_t relation = 0;
    std::uint32_t action = 0;
    std::uint32_t childGeometry = 0;
    std::uint32_t childConcrete = 0;
    std::uint8_t childActual = 0;
    CompiledChildDomain domain = CompiledChildDomain::ExactTerminal;
    std::uint8_t terminalForces = 0; // bit 0 White, bit 1 Black
    std::uint8_t reserved = 0;
};

struct TransitionCompileOptions {
    std::string prefix;
    std::string sourceSha256;
    std::string modelSha256;
    std::string observationSha256;
    std::uint32_t rawGeometryBegin = 0;
    std::uint32_t rawGeometryCount = 0; // zero means through the full domain
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
    std::uint64_t lowerJester = 0;
    std::uint64_t lowerGhost = 0;
    std::uint64_t exactTerminal = 0;
    // Successful semantic checks executed while proving every canonical
    // block against all four D2 images.  Residuals remain zero in every
    // accepted artifact; the builder throws on the first mismatch.
    std::uint64_t codecChecks = 0;
    std::uint64_t actionChecks = 0;
    std::uint64_t decisionChecks = 0;
    std::uint64_t transitionChecks = 0;
    std::uint64_t symmetryChecks = 0;
    std::uint64_t codecResidual = 0;
    std::uint64_t actionResidual = 0;
    std::uint64_t decisionResidual = 0;
    std::uint64_t transitionResidual = 0;
    std::uint64_t symmetryResidual = 0;
    std::string payloadSha256;
};

// Writes an append-only, mmap-friendly transition database.  A partial range
// is a preflight artifact only; solve() rejects anything except a complete
// [0, RawGeometryCount) certificate.
[[nodiscard]] TransitionCertificate compile_transition_database(
  const TransitionCompileOptions& options);
[[nodiscard]] TransitionCertificate verify_transition_database(
  const std::string& prefix, const std::string& sourceSha256,
  const std::string& modelSha256, const std::string& observationSha256,
  bool requireComplete);
[[nodiscard]] TransitionCertificate merge_transition_databases(
  const std::vector<std::string>& shardPrefixes,
  const std::string& outputPrefix, const std::string& sourceSha256,
  const std::string& modelSha256,
  const std::string& observationSha256,
  bool requireComplete = true);

struct ResourceLimits {
    std::uint64_t maxDiskBytes = 0;
    std::uint64_t maxResidentBytes = 0;
    std::uint64_t maxBddNodes = 0;
    std::uint64_t minFreeDiskBytes = 0;
};

struct ResourceEstimate {
    std::uint64_t canonicalGeometries = 0;
    std::uint64_t productWorlds = 0;
    std::uint64_t transitionBytes = 0;
    std::uint64_t rootBytes = 0;
    // Conservative heap bound for exact root information-set deduplication.
    // The solver never substitutes a probabilistic hash/count for this set.
    std::uint64_t rootCatalogBytes = 0;
    std::uint64_t bddBytes = 0;
    std::uint64_t compactionBytes = 0;
    std::uint64_t peakDiskBytes = 0;
    std::uint64_t peakResidentBytes = 0;
    bool admitted = false;
};

// A sampled certificate scales only byte/node demand.  It is never accepted as
// a proof input.  full_domain_preflight consumes a complete transition header
// and applies hard disk/RAM/node gates before any solve arrays are allocated.
[[nodiscard]] ResourceEstimate sampled_resource_preflight(
  const TransitionCertificate& sample, std::uint64_t sampledRawGeometries,
  const ProductRobdd::Limits& bdd, const ResourceLimits& limits);
[[nodiscard]] ResourceEstimate full_domain_preflight(
  const std::string& transitionPrefix, const ProductRobdd::Limits& bdd,
  const ResourceLimits& limits, const std::string& scratchPrefix);

struct SolveOptions {
    std::string transitionPrefix;
    std::string sourceTable;
    std::string lowerJesterTable;
    std::string lowerJesterOverlay;
    std::string lowerGhostSidecar;
    std::string scratchPrefix;
    std::string outputOverlay;
    // Permanent exact all-beliefs probe artifact. Unlike UFIW2, this retains
    // every correlated 2 x 80 King/Jester/Ghost information-set result.
    std::string outputArbitrarySidecar;
    std::string sourceSha256;
    std::string modelSha256;
    std::string observationSha256;
    std::string lowerJesterModelSha256;
    std::string lowerJesterOverlaySha256;
    std::string lowerGhostModelSha256;
    std::string lowerGhostSourceSha256;
    std::string lowerGhostObservationSha256;
    std::string lowerGhostSidecarSha256;
    ProductRobdd::Limits bdd;
    ResourceLimits resources;
    std::uint32_t compactEvery = 4;
    // Nonzero is explicitly a resource run. It executes complete Bellman
    // sweeps and writes neither proof certificate nor overlay.
    std::uint32_t measureIterations = 0;
};

struct SolveCertificate {
    std::uint64_t iterations = 0;
    std::uint64_t upperBddNodes = 0;
    std::uint64_t lowerBddNodes = 0;
    std::uint64_t lowerJesterPairProbes = 0;
    std::uint64_t lowerJesterSingletonProbes = 0;
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
    std::string lowerJesterOverlaySha256;
    std::string lowerGhostSidecarSha256;
    std::string transitionHeaderSha256;
    std::string transitionMarkerSha256;
    std::string arbitrarySidecarSha256;
    std::uint64_t arbitraryStructuralResidual = 0;
    std::uint64_t arbitrarySingletonResidual = 0;
    std::uint64_t arbitraryRootResidual = 0;
};

// Authenticates and loads every immutable solve input through the same native
// codecs used by solve_exact, and applies the complete resource gates without
// allocating solve arrays or writing scratch state.
[[nodiscard]] ResourceEstimate verify_solve_inputs(
  const SolveOptions& options);
[[nodiscard]] SolveCertificate solve_exact(const SolveOptions& options);
[[nodiscard]] SolveCertificate verify_exact_overlay(
  const SolveOptions& options);

struct ArbitrarySidecarCertificate {
    std::uint64_t upperNodes = 0;
    std::uint64_t lowerNodes = 0;
    std::uint64_t geometries = 0;
    std::uint64_t strata = 0;
    std::uint64_t ownerRoots = 0;
    std::uint64_t bytes = 0;
    std::uint64_t structuralResidual = 0;
    std::string payloadSha256;
    std::string fileSha256;
    std::string transitionPayloadSha256;
    std::string transitionHeaderSha256;
    std::string transitionMarkerSha256;
    std::string lowerJesterOverlaySha256;
    std::string lowerGhostSidecarSha256;
};

// UFJG1 is a self-contained, mmap-friendly exact decision artifact. It stores
// the compact two-level ROBDD, canonical public geometries, complete legal-dot
// decision cells, informed-owner roots, and uninformed-observer roots. A query
// must supply the full correlated ProductMask and one actual member; the probe
// never caps, samples, marginalizes, or fresh-maximizes a history-refined set.
[[nodiscard]] ArbitrarySidecarCertificate verify_arbitrary_sidecar(
  const std::string& path, const SolveOptions& options);

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
                                    const ProductWorld& actual,
                                    const ProductMask& worlds) const;
    [[nodiscard]] bool observer_forces(const PublicFrame& frame,
                                       const ProductWorld& actual,
                                       const ProductMask& worlds) const;
    [[nodiscard]] const ArbitrarySidecarCertificate& certificate() const;

  private:
    class Impl;
    Impl* impl_ = nullptr;
};

// Permanent standalone tests call this on a deliberately small, exhaustive
// Boolean domain. It checks the product ROBDD truth table, composition,
// monotonic Bellman equations, serialization/reload, and compaction.
void exact_small_domain_self_test(const std::string& scratchPrefix);

inline constexpr std::uint32_t RawGeometryCount =
  2 * Position::BoardSquares *
  ((Position::BoardSquares - 1) * (Position::BoardSquares - 2) / 2) *
  (Position::BoardSquares - 2);

}  // namespace Stockfish::Ultimate::JesterGhostInformation

#endif
