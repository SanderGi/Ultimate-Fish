/*
  Standalone exact K+Jester+Ghost-v-K solver-kernel tests.

  This file is intentionally not wired into the shared test binary while the
  information-tablebase proof sources are frozen. Compile it with
  jester_ghost_information_solver.cpp, jester_ghost_information_model.cpp,
  external_robdd.cpp, ghost_information_probe.cpp, information.cpp and
  position.cpp.
*/

#include "jester_ghost_information_solver.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include <unistd.h>

using namespace Stockfish::Ultimate;
using namespace Stockfish::Ultimate::JesterGhostInformation;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename Callable>
void require_failure(Callable&& callable, const char* message) {
    try {
        callable();
    }
    catch (const std::exception&) {
        return;
    }
    throw std::runtime_error(message);
}

ProductRobdd::Limits tiny_limits() {
    ProductRobdd::Limits limits;
    limits.maxUpperNodes = 10'000;
    limits.upperUniqueSlots = 32'768;
    limits.upperCacheEntries = 20'000;
    limits.budgetBytes = 1ULL << 30;
    limits.lowerMaxNodes = 10'000;
    limits.lowerUniqueSlots = 32'768;
    limits.lowerApplyCacheEntries = 20'000;
    limits.lowerUnaryCacheEntries = 10'000;
    limits.lowerComposeCacheEntries = 10'000;
    return limits;
}

} // namespace

int main() {
    try {
        const std::string prefix = "/tmp/ultimate-jester-ghost-solver-test-" +
                                   std::to_string(::getpid());
        exact_small_domain_self_test(prefix);

        TransitionCompileOptions compile;
        compile.prefix = prefix + ".transitions";
        compile.sourceSha256 = std::string(64, '1');
        compile.modelSha256 = std::string(64, '2');
        compile.observationSha256 = std::string(64, '3');
        compile.rawGeometryBegin = 0;
        compile.rawGeometryCount = 1;
        compile.exhaustiveSymmetryCertificate = true;
        const TransitionCertificate written =
          compile_transition_database(compile);
        const TransitionCertificate loaded = verify_transition_database(
          compile.prefix, compile.sourceSha256, compile.modelSha256,
          compile.observationSha256, false);
        require(written.rawGeometries == 1,
                "sample transition raw-domain count");
        require(written.canonicalGeometries == loaded.canonicalGeometries &&
                written.edges == loaded.edges &&
                written.payloadSha256 == loaded.payloadSha256,
                "transition compile/reload certificate");
        require(written.codecResidual == 0 && written.actionResidual == 0 &&
                written.decisionResidual == 0 &&
                written.transitionResidual == 0 &&
                written.symmetryResidual == 0,
                "transition semantic residual");
        require_failure([&] {
            (void)verify_transition_database(compile.prefix,
              std::string(64, '4'), compile.modelSha256,
              compile.observationSha256, false);
        }, "stale transition source binding was accepted");
        require_failure([&] {
            (void)verify_transition_database(compile.prefix,
              compile.sourceSha256, compile.modelSha256,
              compile.observationSha256, true);
        }, "partial transition database was accepted as a proof");
        require_failure([&] {
            (void)merge_transition_databases({compile.prefix},
              prefix + ".invalid-merge", compile.sourceSha256,
              compile.modelSha256, compile.observationSha256);
        }, "gapped transition shard set was accepted");
        TransitionCompileOptions second=compile;
        second.prefix=prefix+".transitions-second";
        second.rawGeometryBegin=1;
        const TransitionCertificate secondWritten=
          compile_transition_database(second);
        TransitionCompileOptions direct=compile;
        direct.prefix=prefix+".transitions-direct";
        direct.rawGeometryCount=2;
        const TransitionCertificate directWritten=
          compile_transition_database(direct);
        const TransitionCertificate merged=merge_transition_databases(
          {second.prefix,compile.prefix},prefix+".transitions-merged",
          compile.sourceSha256,compile.modelSha256,
          compile.observationSha256,false);
        require(merged.rawGeometries==2&&
                merged.canonicalGeometries==directWritten.canonicalGeometries&&
                merged.worlds==directWritten.worlds&&
                merged.liveWorlds==directWritten.liveWorlds&&
                merged.actions==directWritten.actions&&
                merged.observations==directWritten.observations&&
                merged.edges==directWritten.edges&&
                merged.payloadSha256==directWritten.payloadSha256&&
                secondWritten.rawGeometries==1,
                "positive shard merge/rebase/direct-compile certificate");

        ResourceLimits resources;
        resources.maxDiskBytes = std::numeric_limits<std::uint64_t>::max();
        resources.maxResidentBytes = std::numeric_limits<std::uint64_t>::max();
        resources.maxBddNodes = 100'000;
        TransitionCompileOptions cycle = compile;
        cycle.prefix = prefix + ".transitions-visibility-cycle";
        cycle.rawGeometryCount = 78;
        const TransitionCertificate cycleWritten =
          compile_transition_database(cycle);
        const ResourceEstimate estimate = sampled_resource_preflight(
          cycleWritten, 78, tiny_limits(), resources);
        require(estimate.canonicalGeometries >= written.canonicalGeometries &&
                estimate.productWorlds >= written.worlds &&
                estimate.transitionBytes && estimate.rootBytes &&
                estimate.rootCatalogBytes &&
                estimate.bddBytes && estimate.compactionBytes &&
                estimate.peakDiskBytes && estimate.peakResidentBytes &&
                estimate.admitted,
                "sampled full-domain resource preflight");

        std::cout << "sample_transition raw " << written.rawGeometries
                  << " canonical " << written.canonicalGeometries
                  << " worlds " << written.worlds
                  << " live " << written.liveWorlds
                  << " admitted " << written.admittedFreshWorlds
                  << " terminal " << written.terminalWorlds
                  << " actions " << written.actions
                  << " observations " << written.observations
                  << " edges " << written.edges
                  << " payload_sha256 " << written.payloadSha256 << '\n';
        std::cout << "two_raw_transition raw " << directWritten.rawGeometries
                  << " canonical " << directWritten.canonicalGeometries
                  << " worlds " << directWritten.worlds
                  << " live " << directWritten.liveWorlds
                  << " actions " << directWritten.actions
                  << " observations " << directWritten.observations
                  << " edges " << directWritten.edges
                  << " payload_sha256 " << directWritten.payloadSha256 << '\n';
        std::cout << "scaled_resource canonical "
                  << estimate.canonicalGeometries
                  << " worlds " << estimate.productWorlds
                  << " transition_bytes " << estimate.transitionBytes
                  << " root_bytes " << estimate.rootBytes
                  << " root_catalog_bytes " << estimate.rootCatalogBytes
                  << " bdd_bytes " << estimate.bddBytes
                  << " compaction_bytes " << estimate.compactionBytes
                  << " peak_disk_bytes " << estimate.peakDiskBytes
                  << " peak_resident_bytes " << estimate.peakResidentBytes
                  << " admitted " << estimate.admitted << '\n';
        std::cout << "visibility_cycle raw " << cycleWritten.rawGeometries
                  << " canonical " << cycleWritten.canonicalGeometries
                  << " worlds " << cycleWritten.worlds
                  << " live " << cycleWritten.liveWorlds
                  << " actions " << cycleWritten.actions
                  << " observations " << cycleWritten.observations
                  << " edges " << cycleWritten.edges
                  << " payload_sha256 " << cycleWritten.payloadSha256
                  << '\n';

        std::cout << "ultimate Jester/Ghost information solver tests passed\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "ultimate Jester/Ghost solver test failure: "
                  << error.what() << '\n';
        return 1;
    }
}
