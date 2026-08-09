/*
  Ultimate Fish - exact arbitrary-belief K+Ghost-v-K information probe
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#ifndef ULTIMATE_GHOST_INFORMATION_PROBE_H_INCLUDED
#define ULTIMATE_GHOST_INFORMATION_PROBE_H_INCLUDED

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Stockfish::Ultimate {

struct GhostInformationMask {
    std::uint64_t low = 0;
    std::uint16_t high = 0;
};

struct GhostInformationProbeResult {
    bool ownerForce = false;
    bool observerForce = false;
    bool terminal = false;
    std::uint32_t geometry = 0;
    std::uint32_t stratum = 0xffffffffu;
};

// Loads the collision-free ROBDD certificate emitted by the exact K+Ghost-v-K
// solver.  The canonical material orientation is a White Ghost owner versus a
// Black observer.  Callers with the opposite ownership must color-swap the two
// Kings and side to move before probing.
class GhostInformationProbe {
  public:
    static constexpr std::uint32_t NoStratum = 0xffffffffu;
    static constexpr std::uint32_t Squares = 80;

    GhostInformationProbe(const std::string& path,
                          const std::string& expectedConcreteSha256,
                          const std::string& expectedModelSha256,
                          const std::string& expectedObservationSha256);

    [[nodiscard]] GhostInformationProbeResult probe(
      std::uint8_t side, std::uint8_t ownerKing, std::uint8_t observerKing,
      std::uint8_t ghost, bool visible, GhostInformationMask belief) const;

    [[nodiscard]] std::uint32_t node_count() const;
    [[nodiscard]] std::uint32_t geometry_count() const;
    [[nodiscard]] std::uint32_t stratum_count() const;
    [[nodiscard]] const std::string& concrete_sha256() const;
    [[nodiscard]] const std::string& model_sha256() const;
    [[nodiscard]] const std::string& observation_sha256() const;

  private:
    struct Node {
        std::uint8_t variable = Squares;
        std::uint32_t low = 0;
        std::uint32_t high = 0;
    };
    struct Geometry {
        std::uint8_t side = 0;
        std::uint8_t ownerKing = 0;
        std::uint8_t observerKing = 0;
        std::uint8_t visible = 0;
        GhostInformationMask live;
        GhostInformationMask terminal;
        GhostInformationMask terminalOwner;
        GhostInformationMask terminalObserver;
        std::array<std::uint32_t, Squares> actualStratum{};
        std::array<std::uint32_t, Squares> ownerRoot{};
        std::array<std::uint8_t, Squares> visibleOwner{};
        std::array<std::uint8_t, Squares> visibleObserver{};
    };
    struct Stratum {
        std::uint32_t geometry = NoStratum;
        GhostInformationMask live;
        std::uint32_t observerRoot = 0;
    };

    [[nodiscard]] bool evaluate(std::uint32_t root,
                                GhostInformationMask assignment) const;
    [[nodiscard]] std::uint32_t find_geometry(
      std::uint8_t side, std::uint8_t ownerKing, std::uint8_t observerKing,
      bool visible, std::uint8_t& transform) const;

    std::vector<Node> nodes_;
    std::vector<Geometry> geometries_;
    std::vector<Stratum> strata_;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> geometryIndex_;
    std::string concreteSha256_;
    std::string modelSha256_;
    std::string observationSha256_;
};

}  // namespace Stockfish::Ultimate

#endif  // ULTIMATE_GHOST_INFORMATION_PROBE_H_INCLUDED
