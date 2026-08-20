/* Exact reciprocal Bishop/Ghost solver integration tests. GPLv3+. */

#include "ghost_public_extra_information_solver.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>

namespace Exact = Stockfish::Ultimate::GhostPublicExtraExact;

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

void cleanup_measurement(const std::string& prefix) {
    for (const char* suffix : {
           ".bdd-a.nodes", ".bdd-a.unique", ".bdd-b.nodes",
           ".bdd-b.unique", ".bdd-remap", ".domains",
           ".observer-current", ".observer-next", ".owner-current",
           ".owner-next", ".visible-observer-current",
           ".visible-observer-next", ".visible-owner-current",
           ".visible-owner-next"})
        std::filesystem::remove(prefix + suffix);
}

[[nodiscard]] std::string table_path(const std::string& filename) {
    const std::string local = "tablebases/" + filename;
    return std::filesystem::exists(local) ? local : "../" + local;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::string base = "/tmp/ultimate-reciprocal-bishop-ghost-test-" +
                                 std::to_string(::getpid());
        const bool sanitizerSmoke = argc == 2 &&
          std::string(argv[1]) == "--sanitizer-smoke";
        if (!sanitizerSmoke)
            Exact::exact_self_test(base);
        const Exact::ResourceEstimate estimate = Exact::resource_estimate();
        if (estimate.geometries != 492'960 ||
            estimate.variables != 80 ||
            estimate.ownerRoots != 39'436'800 ||
            estimate.peakScratchBytes <= estimate.estimatedTransitionBytes)
            throw std::runtime_error("reciprocal resource estimate residual");
        if (Exact::SolveOptions{}.compactEvery != 1)
            throw std::runtime_error(
              "reciprocal compaction cadence is not fail-safe");

        const std::string direct = base + "-direct";
        const std::string first = base + "-first";
        const std::string second = base + "-second";
        const std::string merged = base + "-merged";
        Exact::compile_transitions({direct, 0, 2});
        Exact::compile_transitions({first, 0, 1});
        Exact::compile_transitions({second, 1, 1});
        Exact::merge_transitions(merged, {first, second}, 2);
        Exact::verify_transitions(merged);
        for (const char* suffix : {".header", ".meta", ".strata", ".index",
                                   ".blocks", ".verified"})
            if (bytes(direct + suffix) != bytes(merged + suffix))
                throw std::runtime_error(
                  std::string("reciprocal shard merge residual ") + suffix);

        // Run a complete Bellman measurement over the tiny certified slice.
        // This enters the real reciprocal fixed-point constructor and proves
        // the inherited KGhost image without ever requesting Black's private
        // legal dots through the White observer disclosure context.
        const std::string measurement = base + "-measurement";
        Exact::SolveOptions solve;
        solve.transitionPrefix = merged;
        solve.sourceTable = table_path("kbishopkghost.uftb");
        solve.lowerGhostSidecar = table_path("kghostk.ufgm");
        solve.scratchPrefix = measurement;
        solve.outputOverlay = measurement + ".ufiw";
        solve.outputArbitrary = measurement + ".ufgx";
        solve.sourceSha256 =
          "0649b7859ba72c8534929a902f18b3ca1a74cd7d7b3713ac109633e83a85ac15";
        solve.modelSha256 = std::string(64, 'a');
        solve.observationSha256 =
          "890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23";
        solve.lowerGhostSidecarSha256 =
          "472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb";
        solve.lowerGhostSourceSha256 =
          "11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5";
        solve.lowerGhostModelSha256 =
          "ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5";
        solve.lowerGhostObservationSha256 = solve.observationSha256;
        solve.maxNodes = 6'000'000;
        solve.uniqueSlots = std::uint64_t{1} << 23;
        solve.compactEvery = 1;
        solve.measureIterations = 1;
        const Exact::SolveCertificate measurementCertificate =
          Exact::solve_exact(solve);
        if (!measurementCertificate.arbitrarySha256.empty())
            throw std::runtime_error(
              "reciprocal measurement unexpectedly emitted an artifact");

        // A resource stop during the next sweep must reopen the last complete
        // parity-bound iterate rather than replaying every earlier sweep.  The
        // first measurement swapped logical current into the physical next
        // arrays; the resumed second sweep swaps it back into current.
        solve.resumeFixedPoint = true;
        solve.resumeIteration = 1;
        solve.resumeCurrentInNextSlot = true;
        solve.resumeBddSlot = 'a';
        solve.measureIterations = 2;
        (void)Exact::solve_exact(solve);
        solve.resumeIteration = 2;
        solve.resumeCurrentInNextSlot = false;
        solve.measureIterations = 3;
        (void)Exact::solve_exact(solve);
        cleanup_measurement(measurement);
        std::filesystem::remove(solve.outputOverlay);
        std::filesystem::remove(solve.outputArbitrary);

        const std::string malformed = base + "-malformed.ufgx";
        {
            std::ofstream output(malformed, std::ios::binary);
            output << "not-a-sidecar";
        }
        bool rejected = false;
        try {
            Exact::SolveOptions bindings;
            Exact::ArbitrarySidecarProbe probe(malformed, bindings);
            (void)probe;
        }
        catch (const std::exception&) {
            rejected = true;
        }
        if (!rejected)
            throw std::runtime_error("malformed UFGX2 was accepted");
        rejected = false;
        try {
            Exact::ProbeBindings bindings;
            bindings.arbitrarySidecarSha256 = std::string(64, '0');
            Exact::ArbitrarySidecarProbe probe(malformed, bindings);
            (void)probe;
        }
        catch (const std::exception&) {
            rejected = true;
        }
        if (!rejected)
            throw std::runtime_error(
              "probe-only loader accepted malformed UFGX2");

        for (const std::string& prefix : {direct, first, second, merged})
            cleanup(prefix);
        std::filesystem::remove(malformed);
        std::cout << "reciprocal_bishop_ghost_solver_test ok\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "reciprocal Bishop/Ghost solver test failed: "
                  << error.what() << '\n';
        return 1;
    }
}
