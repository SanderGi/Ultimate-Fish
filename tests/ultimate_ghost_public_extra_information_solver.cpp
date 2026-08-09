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

}  // namespace

int main() {
    try {
        const std::string base = "/tmp/ultimate-reciprocal-bishop-ghost-test-" +
                                 std::to_string(::getpid());
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
