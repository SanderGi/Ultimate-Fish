/*
  Ultimate Fish - exact K+Jester+Ghost versus K information CLI
  Copyright (C) 2026 Ultimate Fish contributors
  GPLv3 or later.
*/

#include "jester_ghost_information_solver.h"

#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace Stockfish::Ultimate::JesterGhostInformation {
namespace {

constexpr char SourceSha[] =
  "ad82489372318a7561a43a7c2d0cbc1fdc3e5a840c67470c5eb25704fdfecf7e";
constexpr char LowerJesterModelSha[] =
  "0ed6d361e313623234c21f9a1c800947014ce47320b4a71fca4fb20a255587c2";
constexpr char LowerGhostSidecarSha[] =
  "400e70da9da18762b659f55a8db93fe89d5a1754d10799b2d18422dd34428a0b";
constexpr char LowerGhostSourceSha[] =
  "3be39c5ab2bfec00cb9dd500e26911bd145bcb1f4dde77fd2c84ef33d111fc31";
constexpr char LowerGhostModelSha[] =
  "4a2d9d7b503b29204cf9af08985345771b9046c07bd2116e592fab40ee12e430";
constexpr char ObservationSha[] =
  "af09ebab834599de83d546f8729b8329dbe5ba8ff1cc7f24be3ac63086273adf";

struct Arguments {
    enum class Command { None, SelfTest, Compile, Merge, VerifyTransitions,
                         Measure, Solve, VerifyOverlay, VerifyArbitrary };
    Command command = Command::None;
    std::string transitionPrefix;
    std::vector<std::string> shards;
    std::string input = "../tablebases/kjesterghostk.uftb";
    std::string lowerJesterTable = "../tablebases/kjesterk.uftb";
    std::string lowerJesterOverlay;
    std::string lowerGhost = "../tablebases/kghostk.ufgm";
    std::string scratch = "/tmp/kjesterghostk-exact";
    std::string output = "/tmp/kjesterghostk.ufiw";
    std::string arbitrary = "/tmp/kjesterghostk.ufjg";
    std::string sourceSha = SourceSha;
    std::string modelSha;
    std::string observationSha = ObservationSha;
    std::string lowerJesterModelSha = LowerJesterModelSha;
    std::string lowerJesterOverlaySha;
    std::string lowerGhostSidecarSha = LowerGhostSidecarSha;
    std::string lowerGhostSourceSha = LowerGhostSourceSha;
    std::string lowerGhostModelSha = LowerGhostModelSha;
    std::string lowerGhostObservationSha = ObservationSha;
    std::uint32_t rawBegin = 0, rawCount = 0, iterations = 0;
    std::uint32_t compactEvery = 4;
    bool requireComplete = true;
    ProductRobdd::Limits bdd;
    ResourceLimits resources;
};

std::uint64_t number(const std::string& text, const char* label) {
    std::size_t used = 0;
    const auto value = std::stoull(text, &used);
    if (used != text.size())
        throw std::invalid_argument(std::string("invalid ") + label);
    return value;
}
std::string value(int& index, int count, char** values, const char* option) {
    if (++index >= count)
        throw std::invalid_argument(std::string(option) + " needs a value");
    return values[index];
}
void command(Arguments& out, Arguments::Command next) {
    if (out.command != Arguments::Command::None)
        throw std::invalid_argument("choose exactly one Jester/Ghost command");
    out.command = next;
}

Arguments parse(int count, char** values) {
    Arguments out;
    out.bdd.maxUpperNodes = 500'000'000;
    out.bdd.upperUniqueSlots = std::uint64_t{1} << 30;
    out.bdd.upperCacheEntries = 1'000'000;
    out.bdd.lowerMaxNodes = 500'000'000;
    out.bdd.lowerUniqueSlots = std::uint64_t{1} << 30;
    out.bdd.lowerApplyCacheEntries = 1'000'000;
    out.bdd.lowerUnaryCacheEntries = 500'000;
    out.bdd.lowerComposeCacheEntries = 500'000;
    out.bdd.budgetBytes = 220ULL << 30;
    out.resources.maxDiskBytes = 1'700ULL << 30;
    out.resources.maxResidentBytes = 170ULL << 30;
    out.resources.minFreeDiskBytes = 50ULL << 30;
    for (int index = 1; index < count; ++index) {
        const std::string option = values[index];
        if (option == "--self-test") command(out, Arguments::Command::SelfTest);
        else if (option == "--compile-transitions") command(out, Arguments::Command::Compile);
        else if (option == "--merge-transitions") command(out, Arguments::Command::Merge);
        else if (option == "--verify-transitions") command(out, Arguments::Command::VerifyTransitions);
        else if (option == "--solve") command(out, Arguments::Command::Solve);
        else if (option == "--verify-overlay") command(out, Arguments::Command::VerifyOverlay);
        else if (option == "--verify-arbitrary") command(out, Arguments::Command::VerifyArbitrary);
        else if (option == "--measure") { command(out, Arguments::Command::Measure);
            out.iterations = static_cast<std::uint32_t>(number(value(index,count,values,option.c_str()),"iterations")); }
        else if (option == "--transition-prefix") out.transitionPrefix=value(index,count,values,option.c_str());
        else if (option == "--shard") out.shards.push_back(value(index,count,values,option.c_str()));
        else if (option == "--raw-begin") out.rawBegin=static_cast<std::uint32_t>(number(value(index,count,values,option.c_str()),"raw begin"));
        else if (option == "--raw-count") out.rawCount=static_cast<std::uint32_t>(number(value(index,count,values,option.c_str()),"raw count"));
        else if (option == "--input") out.input=value(index,count,values,option.c_str());
        else if (option == "--lower-jester-table") out.lowerJesterTable=value(index,count,values,option.c_str());
        else if (option == "--lower-jester-overlay") out.lowerJesterOverlay=value(index,count,values,option.c_str());
        else if (option == "--lower-jester-model-sha256") out.lowerJesterModelSha=value(index,count,values,option.c_str());
        else if (option == "--lower-jester-overlay-sha256") out.lowerJesterOverlaySha=value(index,count,values,option.c_str());
        else if (option == "--lower-ghost-sidecar") out.lowerGhost=value(index,count,values,option.c_str());
        else if (option == "--lower-ghost-sidecar-sha256") out.lowerGhostSidecarSha=value(index,count,values,option.c_str());
        else if (option == "--scratch") out.scratch=value(index,count,values,option.c_str());
        else if (option == "--output") out.output=value(index,count,values,option.c_str());
        else if (option == "--output-arbitrary") out.arbitrary=value(index,count,values,option.c_str());
        else if (option == "--source-sha256") out.sourceSha=value(index,count,values,option.c_str());
        else if (option == "--model-sha256") out.modelSha=value(index,count,values,option.c_str());
        else if (option == "--observation-sha256") out.observationSha=value(index,count,values,option.c_str());
        else if (option == "--compact-every") out.compactEvery=static_cast<std::uint32_t>(number(value(index,count,values,option.c_str()),"compact interval"));
        else if (option == "--bdd-max-upper-nodes") out.bdd.maxUpperNodes=number(value(index,count,values,option.c_str()),"upper nodes");
        else if (option == "--bdd-upper-unique-slots") out.bdd.upperUniqueSlots=number(value(index,count,values,option.c_str()),"upper unique slots");
        else if (option == "--bdd-lower-max-nodes") out.bdd.lowerMaxNodes=static_cast<std::uint32_t>(number(value(index,count,values,option.c_str()),"lower nodes"));
        else if (option == "--bdd-lower-unique-slots") out.bdd.lowerUniqueSlots=number(value(index,count,values,option.c_str()),"lower unique slots");
        else if (option == "--bdd-budget-bytes") out.bdd.budgetBytes=number(value(index,count,values,option.c_str()),"BDD budget");
        else if (option == "--max-disk-bytes") out.resources.maxDiskBytes=number(value(index,count,values,option.c_str()),"disk gate");
        else if (option == "--max-resident-bytes") out.resources.maxResidentBytes=number(value(index,count,values,option.c_str()),"RAM gate");
        else if (option == "--min-free-disk-bytes") out.resources.minFreeDiskBytes=number(value(index,count,values,option.c_str()),"free disk gate");
        else if (option == "--allow-partial-merge") out.requireComplete=false;
        else throw std::invalid_argument("unknown option " + option);
    }
    if (out.command == Arguments::Command::None) throw std::invalid_argument("missing command");
    if (out.command != Arguments::Command::SelfTest && out.modelSha.empty())
        throw std::invalid_argument("--model-sha256 is required");
    if ((out.command==Arguments::Command::Compile||out.command==Arguments::Command::Merge||
         out.command==Arguments::Command::VerifyTransitions||out.command==Arguments::Command::Measure||
         out.command==Arguments::Command::Solve) && out.transitionPrefix.empty())
        throw std::invalid_argument("--transition-prefix is required");
    if (out.command==Arguments::Command::Compile&&!out.rawCount) throw std::invalid_argument("--raw-count must be positive");
    if (out.command==Arguments::Command::Merge&&out.shards.empty()) throw std::invalid_argument("--shard is required");
    if ((out.command==Arguments::Command::Solve||out.command==Arguments::Command::Measure||
         out.command==Arguments::Command::VerifyArbitrary) &&
        (out.lowerJesterOverlay.empty()||out.lowerJesterOverlaySha.empty()))
        throw std::invalid_argument("lower Jester overlay path and SHA are required");
    out.resources.maxBddNodes=out.bdd.maxUpperNodes+out.bdd.lowerMaxNodes;
    return out;
}

SolveOptions solve_options(const Arguments& in) {
    SolveOptions out;out.transitionPrefix=in.transitionPrefix;out.sourceTable=in.input;
    out.lowerJesterTable=in.lowerJesterTable;out.lowerJesterOverlay=in.lowerJesterOverlay;
    out.lowerGhostSidecar=in.lowerGhost;out.scratchPrefix=in.scratch;
    out.outputOverlay=in.output;out.outputArbitrarySidecar=in.arbitrary;
    out.sourceSha256=in.sourceSha;out.modelSha256=in.modelSha;
    out.observationSha256=in.observationSha;out.lowerJesterModelSha256=in.lowerJesterModelSha;
    out.lowerJesterOverlaySha256=in.lowerJesterOverlaySha;
    out.lowerGhostModelSha256=LowerGhostModelSha;out.lowerGhostSourceSha256=LowerGhostSourceSha;
    out.lowerGhostObservationSha256=ObservationSha;out.lowerGhostSidecarSha256=in.lowerGhostSidecarSha;
    out.bdd=in.bdd;out.resources=in.resources;out.compactEvery=in.compactEvery;
    out.measureIterations=in.iterations;return out;
}

void print_transition(const TransitionCertificate& c) {
    std::cout<<"jester_ghost_transition raw "<<c.rawGeometries<<" canonical "<<c.canonicalGeometries
      <<" worlds "<<c.worlds<<" live "<<c.liveWorlds<<" actions "<<c.actions
      <<" observations "<<c.observations<<" edges "<<c.edges
      <<" codec_checks "<<c.codecChecks<<" action_checks "<<c.actionChecks
      <<" decision_checks "<<c.decisionChecks
      <<" transition_checks "<<c.transitionChecks
      <<" symmetry_checks "<<c.symmetryChecks
      <<" codec_residual "<<c.codecResidual<<" action_residual "<<c.actionResidual
      <<" decision_residual "<<c.decisionResidual<<" transition_residual "<<c.transitionResidual
      <<" symmetry_residual "<<c.symmetryResidual<<" payload_sha256 "<<c.payloadSha256<<'\n';
}
void print_solve(const SolveCertificate& c) {
    for(unsigned side=0;side<2;++side){const unsigned win=side?2:1,loss=side?1:2;
        std::cout<<"information_summary side "<<side<<" win "<<c.totals[side][win]
          <<" loss "<<c.totals[side][loss]<<" draw "<<c.totals[side][3]
          <<" unreachable_win "<<c.unreachable[side][win]<<" unreachable_loss "<<c.unreachable[side][loss]
          <<" unreachable_draw "<<c.unreachable[side][3]<<" sets "<<c.informationSets[side]
          <<" concrete "<<c.legalRealizations[side]+c.unreachableRealizations[side]
          <<" bellman_residual "<<c.bellmanResidual<<" rank_residual "<<c.rankResidual
          <<" belief_cap none exhaustive 1\n";}
    std::cout<<"information_symbolic_certificate iterations "<<c.iterations
      <<" upper_nodes "<<c.upperBddNodes<<" lower_nodes "<<c.lowerBddNodes
      <<" bellman_residual "<<c.bellmanResidual<<" monotonicity_residual "<<c.rankResidual
      <<" singleton_residual "<<c.singletonResidual<<" belief_cap none powerset_exact 1\n"
      <<"jester_ghost_artifacts overlay_sha256 "<<c.overlaySha256
      <<" arbitrary_sha256 "<<c.arbitrarySidecarSha256
      <<" arbitrary_structural_residual "<<c.arbitraryStructuralResidual
      <<" arbitrary_singleton_residual "<<c.arbitrarySingletonResidual
      <<" arbitrary_root_residual "<<c.arbitraryRootResidual<<'\n';
}

} // namespace
} // namespace Stockfish::Ultimate::JesterGhostInformation

int main(int argc,char**argv){
    using namespace Stockfish::Ultimate::JesterGhostInformation;
    try{const Arguments a=parse(argc,argv);switch(a.command){
      case Arguments::Command::SelfTest: exact_small_domain_self_test(a.scratch);break;
      case Arguments::Command::Compile: print_transition(compile_transition_database({a.transitionPrefix,a.sourceSha,a.modelSha,a.observationSha,a.rawBegin,a.rawCount,true}));break;
      case Arguments::Command::Merge: print_transition(merge_transition_databases(a.shards,a.transitionPrefix,a.sourceSha,a.modelSha,a.observationSha,a.requireComplete));break;
      case Arguments::Command::VerifyTransitions: print_transition(verify_transition_database(a.transitionPrefix,a.sourceSha,a.modelSha,a.observationSha,a.requireComplete));break;
      case Arguments::Command::Measure: case Arguments::Command::Solve: print_solve(solve_exact(solve_options(a)));break;
      case Arguments::Command::VerifyOverlay: print_solve(verify_exact_overlay(solve_options(a)));break;
      case Arguments::Command::VerifyArbitrary:{const auto c=verify_arbitrary_sidecar(a.arbitrary,solve_options(a));
        std::cout<<"jester_ghost_arbitrary upper_nodes "<<c.upperNodes<<" lower_nodes "<<c.lowerNodes
          <<" geometries "<<c.geometries<<" strata "<<c.strata<<" owner_roots "<<c.ownerRoots
          <<" bytes "<<c.bytes<<" payload_sha256 "<<c.payloadSha256<<" file_sha256 "<<c.fileSha256<<'\n';break;}
      case Arguments::Command::None: throw std::runtime_error("missing command");}
      return 0;}catch(const std::exception&e){std::cerr<<"jester/ghost tablebase error: "<<e.what()<<'\n';return 1;}}
