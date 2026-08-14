/* Exact Giant/Ghost information solver integration tests. GPLv3+. */

#include "ghost_giant_information_solver.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>

namespace Exact = Stockfish::Ultimate::GhostGiantExact;

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
    options.lowerGiantTable =
      std::filesystem::exists("tablebases/kgiantk.uftb")
        ? "tablebases/kgiantk.uftb" : "../tablebases/kgiantk.uftb";
    options.lowerGiantSha256 =
      "eb52f2c08cf88e1e3682d0c72dfde191d9009e79779ad7eee23ca82fdcade591";
    options.lowerGiantSourceSha256 = options.lowerGiantSha256;
    options.lowerGiantModelSha256 =
      "89df7874ea2bb5aab359acc001db24746a84e47b4c9783965e883467c724666b";
    options.modelSha256 = std::string(64, 'a');
    options.observationSha256 =
      "890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23";
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
              std::string("Giant shard merge residual ") + suffix);
    std::vector<char> corruptedBlocks = bytes(first + ".blocks");
    corruptedBlocks.back() ^= 1;
    {
        std::ofstream output(first + ".blocks", std::ios::binary |
                                               std::ios::trunc);
        output.write(corruptedBlocks.data(),
                     static_cast<std::streamsize>(corruptedBlocks.size()));
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
          "Giant transition accepted a corrupted payload component");
    corruptedBlocks.back() ^= 1;
    {
        std::ofstream output(first + ".blocks", std::ios::binary |
                                               std::ios::trunc);
        output.write(corruptedBlocks.data(),
                     static_cast<std::streamsize>(corruptedBlocks.size()));
    }
    Exact::TransitionOptions staleModel = transition_options(
      orientation, first, 0, 1);
    staleModel.modelSha256 = std::string(64, 'b');
    dependencyRejected = false;
    try {
        Exact::verify_transitions(staleModel);
    }
    catch (const std::exception&) {
        dependencyRejected = true;
    }
    if (!dependencyRejected)
        throw std::runtime_error(
          "Giant transition accepted a stale solver-model binding");
    std::vector<char> marker = bytes(first + ".verified");
    marker.back() ^= 1;
    {
        std::ofstream output(first + ".verified", std::ios::binary |
                                                   std::ios::trunc);
        output.write(marker.data(), static_cast<std::streamsize>(marker.size()));
    }
    dependencyRejected = false;
    try {
        Exact::verify_transitions(transition_options(
          orientation, first, 0, 1));
    }
    catch (const std::exception&) {
        dependencyRejected = true;
    }
    if (!dependencyRejected)
        throw std::runtime_error(
          "Giant transition accepted a stale lower-model marker");
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

    const std::string decision = base + "-decision";
    Exact::compile_transitions(transition_options(orientation, decision,
      Exact::decision_witness_geometry(orientation), 1));
    const std::vector<char> decisionMarker = bytes(decision + ".verified");
    constexpr std::size_t CertificateBytes = 72 + 5 * 64;
    constexpr std::size_t DecisionCountOffset = 8 + 4 * 8;
    if (decisionMarker.size() < CertificateBytes ||
        std::string(decisionMarker.end() - CertificateBytes,
                    decisionMarker.end() - CertificateBytes + 6) !=
          "UFGTV4")
        throw std::runtime_error(
          "Giant transition decision certificate is absent");
    std::uint64_t decisionChecks = 0;
    std::memcpy(&decisionChecks,
      decisionMarker.data() + decisionMarker.size() - CertificateBytes +
        DecisionCountOffset,
      sizeof(decisionChecks));
    if (!decisionChecks)
        throw std::runtime_error(
          "Giant legal-dot D2 certificate was not executed");
    cleanup(decision);
}

void measurement_test(const std::string& base,
                      Exact::Orientation orientation,
                      const std::string& source,
                      const std::string& sourceSha,
                      const std::string& normalizedSha,
                      const std::string& lowerGhost,
                      const std::string& lowerGiant) {
    std::filesystem::create_directories(base);
    const std::string transitions = base + "/transitions";
    Exact::TransitionOptions transition = transition_options(
      orientation, transitions, 0, 1);
    transition.lowerGiantTable = lowerGiant;
    Exact::compile_transitions(transition);
    Exact::SolveOptions options;
    options.orientation = orientation;
    options.transitionPrefix = transitions;
    options.sourceTable = source;
    options.lowerGhostSidecar = lowerGhost;
    options.lowerGiantTable = lowerGiant;
    options.scratchPrefix = base + "/solve";
    options.outputOverlay = base + "/measurement-must-not-write.ufiw";
    options.outputArbitrary = base + "/measurement-must-not-write.ufgi";
    options.sourceSha256 = sourceSha;
    options.modelSha256 = std::string(64, 'a');
    options.observationSha256 =
      "890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23";
    options.lowerGhostSourceSha256 =
      "11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5";
    options.lowerGhostModelSha256 =
      "ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5";
    options.lowerGhostObservationSha256 = options.observationSha256;
    options.lowerGhostSidecarSha256 =
      "472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb";
    options.lowerGiantFullSha256 =
      "eb52f2c08cf88e1e3682d0c72dfde191d9009e79779ad7eee23ca82fdcade591";
    options.lowerGiantSourceSha256 = options.lowerGiantFullSha256;
    options.lowerGiantModelSha256 =
      "89df7874ea2bb5aab359acc001db24746a84e47b4c9783965e883467c724666b";
    options.maxNodes = 10'000'000;
    options.uniqueSlots = std::uint64_t{1} << 24;
    options.measureIterations = 1;
    const Exact::SolveCertificate certificate = Exact::solve_exact(options);
    if (certificate.normalizedSourceSha256 != normalizedSha ||
        !certificate.arbitrarySha256.empty() ||
        std::filesystem::exists(options.outputOverlay) ||
        std::filesystem::exists(options.outputArbitrary))
        throw std::runtime_error(
          "Giant measurement wrote proof output or changed source binding");
    std::filesystem::remove_all(base);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::string base = "/tmp/ultimate-giant-ghost-test-" +
                                 std::to_string(::getpid());
        if (argc == 6 && std::string(argv[1]) == "--measurement-smoke") {
            measurement_test(base + "-measure-same", Exact::Orientation::Same,
              argv[2],
              "cd18051587a58abc21c84828ca05ff901967b8e42a6c4f694ba3066fab657b7d",
              "e67b2a61d44ce03885fba9817bf8628e2ae6d33bd097ffe7d472a80f324bf8e8",
              argv[4], argv[5]);
            measurement_test(base + "-measure-opposing",
              Exact::Orientation::Opposing, argv[3],
              "aecce77c512fb3427cbb00b147650b4f95777e41b325e024d2720f7fcf2869cd",
              "153045d7db5a0b1a7b64d52755411e00ad2425711ebe2d90a1b1d23614de487e",
              argv[4], argv[5]);
            std::cout <<
              "giant_ghost_measurement_smoke both orientations ok\n";
            return 0;
        }
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
        if (estimate.geometries != 359'100 ||
            estimate.concreteWorlds != 26'573'400 ||
            estimate.ownerRoots != 28'728'000 ||
            estimate.peakScratchBytes <= estimate.estimatedTransitionBytes ||
            Exact::SolveOptions{}.compactEvery != 1)
            throw std::runtime_error("Giant resource preflight residual");
        if (!opposingOnly)
            shard_test(base + "-same", Exact::Orientation::Same);
        if (!sameOnly)
            shard_test(base + "-opposing", Exact::Orientation::Opposing);

        const std::string malformed = base + ".ufgd";
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
                throw std::runtime_error("malformed UFGI1 was accepted");
        }
        std::filesystem::remove(malformed);
        std::cout << "giant_ghost_information_solver_test ok\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Giant/Ghost solver test failed: " << error.what()
                  << '\n';
        return 1;
    }
}
