/* Exact Parasite/Ghost information solver integration tests. GPLv3+. */

#include "ghost_parasite_information_solver.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>

namespace Exact = Stockfish::Ultimate::GhostParasiteExact;

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
    options.lowerParasiteTable =
      std::filesystem::exists("tablebases/kparasitek.uftb")
        ? "tablebases/kparasitek.uftb" : "../tablebases/kparasitek.uftb";
    options.lowerParasiteSha256 =
      "c53364f87ce4ff23372aa3565d279e9fadde706eb1aeca5695aecc1ea3e83047";
    options.lowerParasiteSourceSha256 = options.lowerParasiteSha256;
    options.lowerParasiteModelSha256 =
      "91a6bec907dc43993c3b0b96001ca5b415fe278add963f4211bd6c5f5d314e88";
    options.lowerGhostSidecar =
      std::filesystem::exists("tablebases/kghostk.ufgm")
        ? "tablebases/kghostk.ufgm" : "../tablebases/kghostk.ufgm";
    options.lowerGhostSidecarSha256 =
      "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b";
    options.lowerGhostSourceSha256 =
      "3be39c5ab2bfec00cb9dd500e26911bd145bcb1f4dde77fd2c84ef33d111fc31";
    options.lowerGhostModelSha256 =
      "4a2d9d7b503b29204cf9af08985345771b9046c07bd2116e592fab40ee12e430";
    options.lowerGhostObservationSha256 =
      "af09ebab834599de83d546f8729b8329dbe5ba8ff1cc7f24be3ac63086273adf";
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
              std::string("Parasite shard merge residual ") + suffix);
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
          "Parasite transition accepted a stale lower-model marker");
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
    if (orientation == Exact::Orientation::Opposing) {
        const std::string possession = base + "-possession";
        Exact::compile_transitions(transition_options(
          orientation, possession, Exact::possession_witness_geometry(), 1));
        Exact::verify_transitions(transition_options(
          orientation, possession, Exact::possession_witness_geometry(), 1));
        cleanup(possession);
    }

    const std::string decision = base + "-next-decision";
    Exact::compile_transitions(transition_options(
      orientation, decision,
      Exact::next_decision_witness_geometry(orientation), 1));
    Exact::verify_transitions(transition_options(
      orientation, decision,
      Exact::next_decision_witness_geometry(orientation), 1));
    const std::vector<char> decisionMarker = bytes(decision + ".verified");
    constexpr std::size_t RelationCertificateBytes = 80;
    constexpr std::size_t NextDecisionOffset = 48;
    if (decisionMarker.size() < RelationCertificateBytes ||
        std::string(decisionMarker.end() - RelationCertificateBytes,
                    decisionMarker.end() - RelationCertificateBytes + 6) !=
          "UFGPR1")
        throw std::runtime_error(
          "Parasite native-observation marker certificate is absent");
    std::uint64_t nextDecisionChecks = 0;
    std::memcpy(&nextDecisionChecks,
      decisionMarker.data() + decisionMarker.size() -
        RelationCertificateBytes + NextDecisionOffset,
      sizeof(nextDecisionChecks));
    if (!nextDecisionChecks)
        throw std::runtime_error(
          "Parasite native-observation nextDecision counter was not executed");
    cleanup(decision);
}

void measurement_test(const std::string& base,
                      Exact::Orientation orientation,
                      const std::string& source,
                      const std::string& sourceSha,
                      const std::string& normalizedSha,
                      const std::string& lowerGhost,
                      const std::string& lowerParasite) {
    std::filesystem::create_directories(base);
    const std::string transitions = base + "/transitions";
    Exact::TransitionOptions transition = transition_options(
      orientation, transitions, 0, 1);
    transition.lowerGhostSidecar = lowerGhost;
    transition.lowerParasiteTable = lowerParasite;
    Exact::compile_transitions(transition);
    Exact::SolveOptions options;
    options.orientation = orientation;
    options.transitionPrefix = transitions;
    options.sourceTable = source;
    options.lowerGhostSidecar = lowerGhost;
    options.lowerParasiteTable = lowerParasite;
    options.scratchPrefix = base + "/solve";
    options.outputOverlay = base + "/measurement-must-not-write.ufiw";
    options.outputArbitrary = base + "/measurement-must-not-write.ufgp";
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
    options.lowerParasiteFullSha256 =
      "c53364f87ce4ff23372aa3565d279e9fadde706eb1aeca5695aecc1ea3e83047";
    options.lowerParasiteSourceSha256 = options.lowerParasiteFullSha256;
    options.lowerParasiteModelSha256 =
      "91a6bec907dc43993c3b0b96001ca5b415fe278add963f4211bd6c5f5d314e88";
    options.maxNodes = 10'000'000;
    options.uniqueSlots = std::uint64_t{1} << 24;
    options.measureIterations = 1;
    const Exact::SolveCertificate certificate = Exact::solve_exact(options);
    if (certificate.normalizedSourceSha256 != normalizedSha ||
        !certificate.arbitrarySha256.empty() ||
        std::filesystem::exists(options.outputOverlay) ||
        std::filesystem::exists(options.outputArbitrary))
        throw std::runtime_error(
          "Parasite measurement wrote proof output or changed source binding");
    std::filesystem::remove_all(base);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::string base = "/tmp/ultimate-parasite-ghost-test-" +
                                 std::to_string(::getpid());
        if (argc == 6 && std::string(argv[1]) == "--measurement-smoke") {
            measurement_test(base + "-measure-same", Exact::Orientation::Same,
              argv[2],
              "1bb1c977b7af34b2295dd6c693d8a79006e69ab2f4291a828b930515742222ca",
              "eb4c64f08278f26cb6b3854ac27dd98018fc1012fec9b8ae1a5b2030d99a6968",
              argv[4], argv[5]);
            measurement_test(base + "-measure-opposing",
              Exact::Orientation::Opposing, argv[3],
              "18c6dc8986ac43d9f38315bbc9e0c2ab6b0bed32c6c638991be01478cc2aceff",
              "1cfa8cc56352a17b084fc65db54229f7112096aa2cd89f359e4bb8be62ae8394",
              argv[4], argv[5]);
            std::cout <<
              "parasite_ghost_measurement_smoke both orientations ok\n";
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
        if (estimate.geometries != 492'960 ||
            estimate.concreteWorlds != 37'957'920 ||
            estimate.ownerRoots != 39'436'800 ||
            estimate.peakScratchBytes <= estimate.estimatedTransitionBytes ||
            Exact::SolveOptions{}.compactEvery != 1)
            throw std::runtime_error("Parasite resource preflight residual");
        if (!opposingOnly)
            shard_test(base + "-same", Exact::Orientation::Same);
        if (!sameOnly)
            shard_test(base + "-opposing", Exact::Orientation::Opposing);

        const std::string malformed = base + ".ufgp";
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
                throw std::runtime_error("malformed UFGP1 was accepted");
        }
        std::filesystem::remove(malformed);
        std::cout << "parasite_ghost_information_solver_test ok\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Parasite/Ghost solver test failed: " << error.what()
                  << '\n';
        return 1;
    }
}
