/* Exact Bomb/Ghost information solver integration tests. GPLv3+. */

#include "ghost_bomb_information_solver.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>

namespace Exact = Stockfish::Ultimate::GhostBombExact;

namespace {

[[nodiscard]] std::vector<char> bytes(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

void cleanup(const std::string& prefix) {
    for (const char* suffix : {".header", ".meta", ".strata", ".index",
                               ".blocks", ".verified"})
        std::filesystem::remove(prefix + suffix);
}

Exact::TransitionOptions transition_options(Exact::Orientation orientation,
                                            const std::string& prefix,
                                            std::uint32_t begin,
                                            std::uint32_t count) {
    Exact::TransitionOptions options;
    options.orientation = orientation;
    options.prefix = prefix;
    options.lowerBombTable =
      std::filesystem::exists("tablebases/kbombk.uftb")
        ? "tablebases/kbombk.uftb" : "../tablebases/kbombk.uftb";
    options.lowerBombSha256 =
      "18e057c83faf940db1ad7404a39623a5db208724de892d604735576d7583ce2f";
    options.lowerBombSourceSha256 = options.lowerBombSha256;
    options.lowerBombModelSha256 = std::string(64, 'd');
    options.geometryBegin = begin;
    options.geometryCount = count;
    return options;
}

void shard_test(const std::string& base, Exact::Orientation orientation) {
    const std::string direct = base + "-direct";
    const std::string first = base + "-first";
    const std::string second = base + "-second";
    const std::string merged = base + "-merged";
    Exact::compile_transitions(transition_options(
      orientation, direct, 0, 2));
    Exact::compile_transitions(transition_options(
      orientation, first, 0, 1));
    Exact::compile_transitions(transition_options(
      orientation, second, 1, 1));
    Exact::merge_transitions(transition_options(
      orientation, merged, 0, 2), {first, second}, 2);
    Exact::verify_transitions(transition_options(
      orientation, merged, 0, 2));
    for (const char* suffix : {".header", ".meta", ".strata", ".index",
                               ".blocks", ".verified"})
        if (bytes(direct + suffix) != bytes(merged + suffix))
            throw std::runtime_error(
              std::string("Bomb shard merge residual ") + suffix);
    std::vector<char> marker = bytes(first + ".verified");
    marker.back() ^= 1;
    {
        std::ofstream output(first + ".verified", std::ios::binary |
                                                   std::ios::trunc);
        output.write(marker.data(), static_cast<std::streamsize>(marker.size()));
    }
    bool dependencyRejected = false;
    try {
        Exact::verify_transitions(transition_options(
          orientation, first, 0, 1));
    }
    catch (const std::exception&) {
        dependencyRejected = true;
    }
    if (!dependencyRejected)
        throw std::runtime_error(
          "Bomb transition accepted a stale lower-model marker");
    for (const std::string& prefix : {direct, first, second, merged})
        cleanup(prefix);

    const std::string lower = base + "-lower-capture";
    const std::uint32_t geometry =
      Exact::lower_capture_witness_geometry(orientation);
    Exact::compile_transitions(transition_options(
      orientation, lower, geometry, 1));
    Exact::verify_transitions(transition_options(
      orientation, lower, geometry, 1));
    cleanup(lower);
}

void explosion_test(const std::string& base,
                    Exact::Orientation orientation, bool draw) {
    const std::uint32_t geometry = draw
      ? Exact::bomb_draw_witness_geometry()
      : Exact::bomb_win_witness_geometry(orientation);
    Exact::compile_transitions(transition_options(
      orientation, base, geometry, 1));
    const Exact::TransitionAuditCertificate certificate =
      Exact::audit_transitions(transition_options(
        orientation, base, geometry, 1));
    if (!certificate.bombCaptureEdges || certificate.residual ||
        (draw && !certificate.bombCaptureDraws) ||
        (!draw && orientation == Exact::Orientation::Same &&
         !certificate.bombCaptureOwnerWins) ||
        (!draw && orientation == Exact::Orientation::Opposing &&
         !certificate.bombCaptureObserverWins))
        throw std::runtime_error(
          "Bomb capture/explosion outcome certificate residual");
    cleanup(base);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::string base = "/tmp/ultimate-bomb-ghost-test-" +
                                 std::to_string(::getpid());
        const bool sanitizerSmoke = argc == 2 &&
                                    std::string(argv[1]) ==
                                      "--sanitizer-smoke";
        const bool sameOnly = argc == 2 &&
                              std::string(argv[1]) == "--same-only";
        const bool opposingOnly = argc == 2 &&
                                  std::string(argv[1]) == "--opposing-only";
        if (!sanitizerSmoke && !opposingOnly)
            Exact::exact_self_test(base);
        const auto estimate = Exact::resource_estimate();
        if (estimate.geometries != 492'960 ||
            estimate.concreteWorlds != 37'957'920 ||
            estimate.ownerRoots != 39'436'800 ||
            estimate.peakScratchBytes <= estimate.estimatedTransitionBytes ||
            Exact::SolveOptions{}.compactEvery != 1)
            throw std::runtime_error("Bomb resource preflight residual");
        if (!opposingOnly)
            shard_test(base + "-same", Exact::Orientation::Same);
        if (!sameOnly)
            shard_test(base + "-opposing", Exact::Orientation::Opposing);
        if (!opposingOnly)
            explosion_test(base + "-same-explosion",
                           Exact::Orientation::Same, false);
        if (!sameOnly) {
            explosion_test(base + "-opposing-explosion",
                           Exact::Orientation::Opposing, false);
            explosion_test(base + "-opposing-draw",
                           Exact::Orientation::Opposing, true);
        }

        const std::string malformed = base + ".ufgb";
        {
            std::ofstream output(malformed, std::ios::binary);
            output << "not-a-sidecar";
        }
        for (const Exact::Orientation orientation : {
               Exact::Orientation::Same, Exact::Orientation::Opposing}) {
            bool rejected = false;
            try {
                Exact::ProbeBindings bindings;
                bindings.orientation = orientation;
                bindings.arbitrarySidecarSha256 = std::string(64, '0');
                Exact::ArbitrarySidecarProbe probe(malformed, bindings);
                (void)probe;
            }
            catch (const std::exception&) {
                rejected = true;
            }
            if (!rejected)
                throw std::runtime_error("malformed UFGB1 was accepted");
        }
        std::filesystem::remove(malformed);
        std::cout << "bomb_ghost_information_solver_test ok\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Bomb/Ghost solver test failed: " << error.what()
                  << '\n';
        return 1;
    }
}
