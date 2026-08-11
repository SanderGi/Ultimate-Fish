/* Exact Mage/Ghost information solver integration tests. GPLv3+. */

#include "ghost_mage_information_solver.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>

namespace Exact = Stockfish::Ultimate::GhostMageExact;

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
              std::string("Mage shard merge residual ") + suffix);
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
          "Mage transition accepted a corrupted proof marker");
    for (const std::string& prefix : {direct, first, second, merged})
        cleanup(prefix);

    const std::string lower = base + "-swap";
    const std::uint32_t geometry = Exact::swap_witness_geometry(orientation);
    Exact::compile_transitions(transition_options(
      orientation, lower, geometry, 1));
    Exact::verify_transitions(transition_options(
      orientation, lower, geometry, 1));
    cleanup(lower);

    const std::string royal = base + "-royal-swap";
    const std::uint32_t royalGeometry =
      Exact::royal_swap_witness_geometry(orientation);
    Exact::compile_transitions(transition_options(
      orientation, royal, royalGeometry, 1));
    const auto royalCertificate = Exact::audit_transitions(
      transition_options(orientation, royal, royalGeometry, 1));
    if (!royalCertificate.swapRoyalEdges ||
        !royalCertificate.actionCollisionChecks ||
        royalCertificate.forcedContinuationEdges ||
        royalCertificate.residual)
        throw std::runtime_error("Mage royal-swap certificate residual");
    cleanup(royal);
}

void interaction_test(const std::string& base,
                      Exact::Orientation orientation,
                      bool lowerGhost = false,
                      bool mageDraw = false,
                      bool capturedMage = false) {
    const std::uint32_t geometry = mageDraw
      ? Exact::mage_only_draw_witness_geometry()
      : capturedMage ? Exact::mage_capture_witness_geometry()
      : lowerGhost ? Exact::ghost_capture_witness_geometry()
                   : Exact::swap_witness_geometry(orientation);
    Exact::compile_transitions(transition_options(
      orientation, base, geometry, 1));
    const Exact::TransitionAuditCertificate certificate =
      Exact::audit_transitions(transition_options(
        orientation, base, geometry, 1));
    if (certificate.residual || !certificate.actionCollisionChecks ||
        certificate.forcedContinuationEdges ||
        (lowerGhost && !certificate.lowerGhostEdges) ||
        (mageDraw && !certificate.insufficientMageEdges) ||
        (!lowerGhost && !mageDraw && !certificate.swapEdges))
        throw std::runtime_error(
          "Mage interaction certificate residual");
    cleanup(base);
}

void measurement_test(const std::string& base,
                      Exact::Orientation orientation,
                      const std::string& source,
                      const std::string& sourceSha,
                      const std::string& normalizedSha,
                      const std::string& lower) {
    std::filesystem::create_directories(base);
    const std::string transitions = base + "/transitions";
    Exact::compile_transitions(transition_options(
      orientation, transitions, 0, 1));
    Exact::SolveOptions options;
    options.orientation = orientation;
    options.transitionPrefix = transitions;
    options.sourceTable = source;
    options.lowerGhostSidecar = lower;
    options.scratchPrefix = base + "/solve";
    options.outputOverlay = base + "/measurement-must-not-write.ufiw";
    options.outputArbitrary = base + "/measurement-must-not-write.ufmg";
    options.sourceSha256 = sourceSha;
    options.modelSha256 = std::string(64, 'a');
    options.observationSha256 =
      "af09ebab834599de83d546f8729b8329dbe5ba8ff1cc7f24be3ac63086273adf";
    options.lowerGhostSourceSha256 =
      "3be39c5ab2bfec00cb9dd500e26911bd145bcb1f4dde77fd2c84ef33d111fc31";
    options.lowerGhostModelSha256 =
      "4a2d9d7b503b29204cf9af08985345771b9046c07bd2116e592fab40ee12e430";
    options.lowerGhostObservationSha256 = options.observationSha256;
    options.lowerGhostSidecarSha256 =
      "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b";
    // UFGM contains 5,477,899 permanent nodes. This smoke budget imports it
    // in full and leaves deterministic headroom for one native Bellman sweep
    // without allocating the production 500-million-node arena.
    options.maxNodes = 10'000'000;
    options.uniqueSlots = std::uint64_t{1} << 24;
    options.measureIterations = 1;
    const Exact::SolveCertificate certificate = Exact::solve_exact(options);
    if (certificate.normalizedSourceSha256 != normalizedSha ||
        !certificate.arbitrarySha256.empty() ||
        std::filesystem::exists(options.outputOverlay) ||
        std::filesystem::exists(options.outputArbitrary))
        throw std::runtime_error(
          "Mage measurement wrote proof output or changed source binding");
    std::filesystem::remove_all(base);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::string base = "/tmp/ultimate-mage-ghost-test-" +
                                 std::to_string(::getpid());
        const bool sanitizerSmoke = argc == 2 &&
                                    std::string(argv[1]) ==
                                      "--sanitizer-smoke";
        const bool sameOnly = argc == 2 &&
                              std::string(argv[1]) == "--same-only";
        const bool opposingOnly = argc == 2 &&
                                  std::string(argv[1]) == "--opposing-only";
        if (argc == 5 && std::string(argv[1]) == "--measurement-smoke") {
            measurement_test(base + "-measure-same", Exact::Orientation::Same,
              argv[2],
              "061a250c66c18e769baec7410be2257db98e669cf6b3d6b39cd5153b27792596",
              "17d07b24bb3fd08a42ee3464257894b04554f3d82d25c171e334e01e831b0a58",
              argv[4]);
            measurement_test(base + "-measure-opposing",
              Exact::Orientation::Opposing, argv[3],
              "d23e1c2253e1ff26822069f5852989c73c1ab308b3720ce4a5473fa29c881519",
              "05cd0d3ed092e1d30ed9572f0561444b92dc1acc10f3781c3cb1336ffe0da9b2",
              argv[4]);
            std::cout << "mage_ghost_measurement_smoke both orientations ok\n";
            return 0;
        }
        if (!sanitizerSmoke && !opposingOnly)
            Exact::exact_self_test(base);
        const auto estimate = Exact::resource_estimate();
        if (estimate.geometries != 492'960 ||
            estimate.concreteWorlds != 37'957'920 ||
            estimate.ownerRoots != 39'436'800 ||
            estimate.peakScratchBytes <= estimate.estimatedTransitionBytes ||
            Exact::SolveOptions{}.compactEvery != 1)
            throw std::runtime_error("Mage resource preflight residual");
        if (!opposingOnly)
            shard_test(base + "-same", Exact::Orientation::Same);
        if (!sameOnly)
            shard_test(base + "-opposing", Exact::Orientation::Opposing);
        if (!opposingOnly)
            interaction_test(base + "-same-swap",
                             Exact::Orientation::Same);
        if (!opposingOnly)
            interaction_test(base + "-same-mage-draw",
                             Exact::Orientation::Same, false, true);
        if (!opposingOnly)
            interaction_test(base + "-same-lower-ghost",
                             Exact::Orientation::Same, true, false, true);
        if (!sameOnly) {
            interaction_test(base + "-opposing-swap",
                             Exact::Orientation::Opposing);
            interaction_test(base + "-opposing-lower-ghost",
                             Exact::Orientation::Opposing, true);
        }

        const std::string malformed = base + ".ufmg";
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
                throw std::runtime_error("malformed UFMG1 was accepted");
        }
        std::filesystem::remove(malformed);
        std::cout << "mage_ghost_information_solver_test ok\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Mage/Ghost solver test failed: " << error.what()
                  << '\n';
        return 1;
    }
}
