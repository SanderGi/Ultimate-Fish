/* Ultimate Fish exact crossed Jester/Ghost arbitrary-belief sidecar. GPLv3+. */

#ifndef ULTIMATE_CROSSED_JESTER_GHOST_INFORMATION_SIDECAR_H_INCLUDED
#define ULTIMATE_CROSSED_JESTER_GHOST_INFORMATION_SIDECAR_H_INCLUDED

#include "crossed_jester_ghost_information_fixed_point.h"

#include <cstdint>
#include <memory>
#include <string>

namespace Stockfish::Ultimate::CrossedJesterGhostSolver {

struct SidecarBindings {
    std::string sourceSha256;
    std::string modelSha256;
    std::string observationSha256;
    std::string graphCheckpointSha256;
    std::string lowerJesterTableSha256;
    std::string lowerJesterOverlaySha256;
    std::string lowerGhostSidecarSha256;
};

struct SidecarCertificate {
    std::uint64_t roots = 0;
    std::uint64_t nodes = 0;
    std::uint64_t atoms = 0;
    std::uint64_t keyBytes = 0;
    std::uint64_t payloadBytes = 0;
    std::uint64_t rootBoundsResidual = 0;
    std::uint64_t keyOrderResidual = 0;
    std::uint64_t dualForceResidual = 0;
    std::string payloadSha256;
    std::string fileSha256;
};

// Sorts the full collision-free graph keys, remaps every fresh root, and
// bit-packs both exact target force functions. Construction graph caches and
// fixed-point reverse edges are deliberately absent from the runtime artifact.
[[nodiscard]] SidecarCertificate write_arbitrary_sidecar(
  const std::string& path, const GraphDiscovery& graph,
  const FixedPointSolution& white, const FixedPointSolution& black,
  const SidecarBindings& bindings);

class CrossedSidecarProbe {
   public:
    CrossedSidecarProbe(const std::string& path,
                        const std::string& expectedFileSha256,
                        const SidecarBindings& expectedBindings);
    ~CrossedSidecarProbe();
    CrossedSidecarProbe(CrossedSidecarProbe&&) noexcept;
    CrossedSidecarProbe& operator=(CrossedSidecarProbe&&) noexcept;
    CrossedSidecarProbe(const CrossedSidecarProbe&) = delete;
    CrossedSidecarProbe& operator=(const CrossedSidecarProbe&) = delete;

    // `actualAtom` names the physical actual in the supplied state's atom
    // order. D2 canonicalization preserves atom order, so the same ordinal
    // addresses the sorted sidecar entry without leaking a hidden coordinate.
    [[nodiscard]] bool force(
      const CrossedJesterGhostInformation::KnowledgeState& state,
      std::uint32_t actualAtom, Color target) const;
    [[nodiscard]] std::uint32_t fresh_root(std::uint32_t rawFrame) const;
    [[nodiscard]] const SidecarCertificate& certificate() const;

   private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// Permanent focused regression for the portable layout, full-domain root map,
// arbitrary-belief lookup, force bitplanes, and provenance authentication.
void arbitrary_sidecar_format_self_test(const std::string& path);

}  // namespace Stockfish::Ultimate::CrossedJesterGhostSolver

#endif
