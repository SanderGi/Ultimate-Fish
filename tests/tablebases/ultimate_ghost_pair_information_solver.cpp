/*
  Standalone tests for the exact K+Ghost+Ghost-v-K information solver.

  Compile directly while proof-bound shared build sources remain frozen:

    c++ -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Werror -Isrc/ultimate \
      tests/tablebases/ultimate_ghost_pair_information_solver.cpp \
      src/ultimate/tablebases/ghost_pair_information_solver.cpp \
      src/ultimate/tablebases/ghost_pair_information_model.cpp \
      src/ultimate/tablebases/ghost_information_probe.cpp \
      src/ultimate/tablebases/information.cpp src/ultimate/position.cpp \
      src/ultimate/nnue.cpp -o /tmp/ultimate-ghost-pair-solver-test
*/

#include "ghost_pair_information_solver.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include <unistd.h>

namespace Stockfish::Ultimate {
namespace {

namespace Solver = GhostPairInformation;

void require(bool condition,const std::string&message){
    if(!condition)throw std::runtime_error(message);
}

template<typename Operation>
void require_failure(Operation operation,const std::string&message){
    bool rejected=false;try{operation();}catch(const std::exception&){rejected=true;}
    require(rejected,message);
}

[[nodiscard]] Solver::PairRobdd::Limits tiny_bdd(){
    Solver::PairRobdd::Limits result;result.maxNodes=30'000;
    result.uniqueSlots=1<<16;result.applyCacheEntries=30'000;
    result.unaryCacheEntries=15'000;result.budgetBytes=1ULL<<30;return result;
}

}  // namespace
}  // namespace Stockfish::Ultimate

int main(){
    using namespace Stockfish::Ultimate;
    try{
        const std::string prefix="/tmp/ultimate-ghost-pair-solver-"+
          std::to_string(static_cast<long long>(::getpid()));
        Solver::exact_small_domain_self_test(prefix+".bdd-selftest");

        Solver::TransitionCompileOptions hidden;
        hidden.prefix=prefix+".hidden";
        hidden.sourceSha256=std::string(64,'1');
        hidden.modelSha256=std::string(64,'2');
        hidden.observationSha256=std::string(64,'3');
        hidden.rawGeometryBegin=0;hidden.rawGeometryCount=1;
        hidden.exhaustiveSymmetryCertificate=true;
        const Solver::TransitionCertificate hiddenWritten=
          Solver::compile_transition_database(hidden);
        Solver::TransitionCompileOptions mixed=hidden;
        mixed.prefix=prefix+".mixed";mixed.rawGeometryBegin=1;
        const Solver::TransitionCertificate mixedWritten=
          Solver::compile_transition_database(mixed);
        // The adjacent-King raw frame is a dense geometric artifact: native
        // transition closure retains all 3,003 live variables, while its
        // admittedFresh set is empty.  Solve/overlay root reporting tests that
        // admission bit before either live or terminal result classification.
        require(hiddenWritten.canonicalGeometries==1&&
                hiddenWritten.worlds==Solver::MaximumWorlds&&
                hiddenWritten.terminalWorlds==0&&
                hiddenWritten.liveWorlds==Solver::MaximumWorlds&&
                hiddenWritten.admittedFreshWorlds==0&&
                mixedWritten.canonicalGeometries==1&&
                mixedWritten.worlds==Position::BoardSquares-3,
                "HH/HV correlated-world transition strata");

        // Raw 79 is the D2-canonical visible-visible frame
        // White: King a1, Black: King b1, visible Ghosts c1/d1.  Its one-world
        // block is the smallest complete native transition certificate.
        Solver::TransitionCompileOptions first;
        first.prefix=prefix+".first";first.sourceSha256=std::string(64,'1');
        first.modelSha256=std::string(64,'2');
        first.observationSha256=std::string(64,'3');
        first.rawGeometryBegin=79;first.rawGeometryCount=1;
        first.exhaustiveSymmetryCertificate=true;
        const Solver::TransitionCertificate written=
          Solver::compile_transition_database(first);
        const Solver::TransitionCertificate loaded=
          Solver::verify_transition_database(first.prefix,
            first.sourceSha256,first.modelSha256,
            first.observationSha256,false);
        require(written.rawGeometries==1&&written.canonicalGeometries==1&&
                written.worlds==1&&written.liveWorlds<=1&&
                written.payloadSha256==loaded.payloadSha256&&
                written.edges==loaded.edges,
                "visible-visible transition compile/reload certificate");
        require(written.codecResidual==0&&written.actionResidual==0&&
                written.decisionResidual==0&&
                written.transitionResidual==0&&
                written.symmetryResidual==0,
                "visible-visible semantic residual");
        require_failure([&]{(void)Solver::verify_transition_database(
          first.prefix,std::string(64,'4'),first.modelSha256,
          first.observationSha256,false);},
          "stale source binding was accepted");
        require_failure([&]{(void)Solver::verify_transition_database(
          first.prefix,first.sourceSha256,first.modelSha256,
          first.observationSha256,true);},
          "partial transition shard was accepted as full proof");
        require_failure([&]{(void)Solver::merge_transition_databases(
          {first.prefix},prefix+".bad-merge",first.sourceSha256,
          first.modelSha256,first.observationSha256);},
          "incomplete shard merge was accepted by production default");

        Solver::TransitionCompileOptions second=first;
        second.prefix=prefix+".second";second.rawGeometryBegin=80;
        const Solver::TransitionCertificate secondWritten=
          Solver::compile_transition_database(second);
        Solver::TransitionCompileOptions direct=first;
        direct.prefix=prefix+".direct";direct.rawGeometryCount=2;
        const Solver::TransitionCertificate directWritten=
          Solver::compile_transition_database(direct);
        const Solver::TransitionCertificate merged=
          Solver::merge_transition_databases({second.prefix,first.prefix},
            prefix+".merged",first.sourceSha256,first.modelSha256,
            first.observationSha256,false);
        require(secondWritten.rawGeometries==1&&merged.rawGeometries==2&&
                merged.canonicalGeometries==directWritten.canonicalGeometries&&
                merged.worlds==directWritten.worlds&&
                merged.liveWorlds==directWritten.liveWorlds&&
                merged.actions==directWritten.actions&&
                merged.observations==directWritten.observations&&
                merged.edges==directWritten.edges&&
                merged.payloadSha256==directWritten.payloadSha256,
                "positive shard merge/rebase/direct payload certificate");

        Solver::ResourceLimits resources;
        resources.maxDiskBytes=std::numeric_limits<std::uint64_t>::max();
        resources.maxResidentBytes=std::numeric_limits<std::uint64_t>::max();
        resources.maxBddNodes=100'000;
        const Solver::ResourceEstimate estimate=
          Solver::sampled_resource_preflight(written,1,tiny_bdd(),resources);
        require(estimate.canonicalGeometries>=written.canonicalGeometries&&
                estimate.correlatedWorlds>=written.worlds&&
                estimate.transitionBytes&&estimate.rootBytes&&
                estimate.rootCatalogBytes&&estimate.bddBytes&&
                estimate.compactionBytes&&estimate.peakDiskBytes&&
                estimate.peakResidentBytes&&estimate.admitted,
                "Ghost-pair sampled resource preflight");

        std::cout<<"ghost_pair_sample raw "<<written.rawGeometries
          <<" canonical "<<written.canonicalGeometries
          <<" worlds "<<written.worlds<<" live "<<written.liveWorlds
          <<" admitted "<<written.admittedFreshWorlds
          <<" terminal "<<written.terminalWorlds
          <<" actions "<<written.actions
          <<" observations "<<written.observations
          <<" edges "<<written.edges
          <<" payload_sha256 "<<written.payloadSha256<<'\n';
        std::cout<<"ghost_pair_visibility_worlds hidden_hidden "
          <<hiddenWritten.worlds<<" hidden_visible "<<mixedWritten.worlds
          <<" visible_visible "<<written.worlds<<" residual 0\n";
        std::cout<<"ghost_pair_two_raw raw "<<directWritten.rawGeometries
          <<" canonical "<<directWritten.canonicalGeometries
          <<" worlds "<<directWritten.worlds
          <<" live "<<directWritten.liveWorlds
          <<" actions "<<directWritten.actions
          <<" observations "<<directWritten.observations
          <<" edges "<<directWritten.edges
          <<" payload_sha256 "<<directWritten.payloadSha256<<'\n';
        std::cout<<"ghost_pair_scaled_resource canonical "
          <<estimate.canonicalGeometries
          <<" worlds "<<estimate.correlatedWorlds
          <<" transition_bytes "<<estimate.transitionBytes
          <<" root_bytes "<<estimate.rootBytes
          <<" root_catalog_bytes "<<estimate.rootCatalogBytes
          <<" bdd_bytes "<<estimate.bddBytes
          <<" compaction_bytes "<<estimate.compactionBytes
          <<" peak_disk_bytes "<<estimate.peakDiskBytes
          <<" peak_resident_bytes "<<estimate.peakResidentBytes
          <<" admitted "<<estimate.admitted<<'\n';
        std::cout<<"ultimate Ghost-pair information solver tests passed\n";
        return EXIT_SUCCESS;
    }catch(const std::exception&error){
        std::cerr<<"ultimate Ghost-pair solver test failure: "
                 <<error.what()<<'\n';return EXIT_FAILURE;
    }
}
