/*
  Ultimate Fish - exact K+Ghost+Ghost versus K information CLI
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#include "ghost_pair_information_solver.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace Stockfish::Ultimate::GhostPairInformation {
namespace {

constexpr char ConcreteSha[] =
  "204f4de6d0f9ff6da111d3d0c0a08c3562493cdec2130cc946b4eae7183012ee";
constexpr char LowerSidecarSha[] =
  "472721217166c8270aa8b68f19645f97cbb84084096f197364289e9d1a7588cb";
constexpr char LowerSourceSha[] =
  "11b7b57aa9819b1fb9ac3f7bd273ab73cfa3627fb09a855856ae1426af2c0ba5";
constexpr char LowerModelSha[] =
  "ec6ed34ab80733ee06354675b9bf0ea9e190c927583ba586fe94f4026afdf9c5";
constexpr char ObservationSha[] =
  "890c399d6856fbf1773766f1840d0c38e46669c18b609e20c38d2750a09dbc23";

struct Arguments {
    enum class Command {
        None,
        SelfTest,
        Compile,
        Merge,
        VerifyTransitions,
        Measure,
        Solve,
        VerifyOverlay,
        VerifyArbitrary,
    } command = Command::None;

    std::string transitionPrefix;
    std::vector<std::string> shards;
    std::string input = "../tablebases/kghostghostk.uftb";
    std::string lower = "../tablebases/kghostk.ufgm";
    std::string scratch = "/tmp/kghostghostk-exact";
    std::string output = "/tmp/kghostghostk.ufiw";
    std::string arbitrary = "/tmp/kghostghostk.ufgg";
    std::string sourceSha = ConcreteSha;
    std::string modelSha;
    std::string observationSha = ObservationSha;
    std::string lowerSidecarSha = LowerSidecarSha;
    std::string lowerSourceSha = LowerSourceSha;
    std::string lowerModelSha = LowerModelSha;
    std::string lowerObservationSha = ObservationSha;
    std::uint32_t rawBegin = 0;
    std::uint32_t rawCount = 0;
    std::uint32_t iterations = 0;
    std::uint32_t workers = 1;
    std::uint32_t compactEvery = 4;
    bool requireComplete = true;
    PairRobdd::Limits bdd;
    ResourceLimits resources;
};

[[nodiscard]] std::uint64_t number(const std::string& text,
                                   const char* label) {
    std::size_t consumed = 0;
    const unsigned long long value = std::stoull(text, &consumed);
    if (consumed != text.size())
        throw std::invalid_argument(std::string("invalid ") + label);
    return static_cast<std::uint64_t>(value);
}

[[nodiscard]] std::string value(int& index, int count, char** values,
                                const char* option) {
    if (++index >= count)
        throw std::invalid_argument(std::string(option) + " needs a value");
    return values[index];
}

void set_command(Arguments& arguments, Arguments::Command command) {
    if (arguments.command != Arguments::Command::None)
        throw std::invalid_argument("choose exactly one Ghost-pair command");
    arguments.command = command;
}

[[nodiscard]] Arguments parse(int count, char** values) {
    Arguments result;
    result.bdd.variables = MaximumWorlds;
    result.resources.maxDiskBytes = 1'700ULL << 30;
    result.resources.maxResidentBytes = 170ULL << 30;
    result.resources.maxBddNodes = result.bdd.maxNodes;
    result.resources.minFreeDiskBytes = 50ULL << 30;
    for (int index = 1; index < count; ++index) {
        const std::string option = values[index];
        if (option == "--self-test")
            set_command(result, Arguments::Command::SelfTest);
        else if (option == "--compile-transitions")
            set_command(result, Arguments::Command::Compile);
        else if (option == "--merge-transitions")
            set_command(result, Arguments::Command::Merge);
        else if (option == "--verify-transitions")
            set_command(result, Arguments::Command::VerifyTransitions);
        else if (option == "--measure") {
            set_command(result, Arguments::Command::Measure);
            result.iterations = static_cast<std::uint32_t>(number(
              value(index, count, values, "--measure"), "iteration count"));
        }
        else if (option == "--solve")
            set_command(result, Arguments::Command::Solve);
        else if (option == "--verify-overlay")
            set_command(result, Arguments::Command::VerifyOverlay);
        else if (option == "--verify-arbitrary")
            set_command(result, Arguments::Command::VerifyArbitrary);
        else if (option == "--transition-prefix")
            result.transitionPrefix = value(index, count, values, option.c_str());
        else if (option == "--shard")
            result.shards.push_back(value(index, count, values, option.c_str()));
        else if (option == "--raw-begin")
            result.rawBegin = static_cast<std::uint32_t>(number(
              value(index, count, values, option.c_str()), "raw begin"));
        else if (option == "--raw-count")
            result.rawCount = static_cast<std::uint32_t>(number(
              value(index, count, values, option.c_str()), "raw count"));
        else if (option == "--input")
            result.input = value(index, count, values, option.c_str());
        else if (option == "--lower-ghost-sidecar")
            result.lower = value(index, count, values, option.c_str());
        else if (option == "--scratch")
            result.scratch = value(index, count, values, option.c_str());
        else if (option == "--output")
            result.output = value(index, count, values, option.c_str());
        else if (option == "--output-arbitrary")
            result.arbitrary = value(index, count, values, option.c_str());
        else if (option == "--source-sha256")
            result.sourceSha = value(index, count, values, option.c_str());
        else if (option == "--model-sha256")
            result.modelSha = value(index, count, values, option.c_str());
        else if (option == "--observation-sha256")
            result.observationSha = value(index, count, values, option.c_str());
        else if (option == "--lower-sidecar-sha256")
            result.lowerSidecarSha = value(index, count, values, option.c_str());
        else if (option == "--lower-source-sha256")
            result.lowerSourceSha = value(index, count, values, option.c_str());
        else if (option == "--lower-model-sha256")
            result.lowerModelSha = value(index, count, values, option.c_str());
        else if (option == "--lower-observation-sha256")
            result.lowerObservationSha = value(index, count, values,
                                                option.c_str());
        else if (option == "--compact-every")
            result.compactEvery = static_cast<std::uint32_t>(number(
              value(index, count, values, option.c_str()), "compact interval"));
        else if (option == "--workers")
            result.workers = static_cast<std::uint32_t>(number(
              value(index, count, values, option.c_str()), "worker count"));
        else if (option == "--bdd-max-nodes")
            result.bdd.maxNodes = static_cast<std::uint32_t>(number(
              value(index, count, values, option.c_str()), "BDD nodes"));
        else if (option == "--bdd-unique-slots")
            result.bdd.uniqueSlots = number(value(index, count, values,
              option.c_str()), "BDD unique slots");
        else if (option == "--bdd-apply-cache")
            result.bdd.applyCacheEntries = number(value(index, count, values,
              option.c_str()), "BDD apply cache");
        else if (option == "--bdd-unary-cache")
            result.bdd.unaryCacheEntries = number(value(index, count, values,
              option.c_str()), "BDD unary cache");
        else if (option == "--bdd-budget-bytes")
            result.bdd.budgetBytes = number(value(index, count, values,
              option.c_str()), "BDD byte budget");
        else if (option == "--max-disk-bytes")
            result.resources.maxDiskBytes = number(value(index, count, values,
              option.c_str()), "disk gate");
        else if (option == "--max-resident-bytes")
            result.resources.maxResidentBytes = number(value(index, count,
              values, option.c_str()), "resident gate");
        else if (option == "--min-free-disk-bytes")
            result.resources.minFreeDiskBytes = number(value(index, count,
              values, option.c_str()), "free-disk gate");
        else if (option == "--allow-partial-merge")
            result.requireComplete = false;
        else
            throw std::invalid_argument("unknown option " + option);
    }
    if (result.command == Arguments::Command::None)
        throw std::invalid_argument("missing Ghost-pair command");
    if (result.command != Arguments::Command::SelfTest && result.modelSha.empty())
        throw std::invalid_argument("--model-sha256 is required");
    if ((result.command == Arguments::Command::Compile ||
         result.command == Arguments::Command::Merge ||
         result.command == Arguments::Command::VerifyTransitions ||
         result.command == Arguments::Command::Measure ||
         result.command == Arguments::Command::Solve) &&
        result.transitionPrefix.empty())
        throw std::invalid_argument("--transition-prefix is required");
    if (result.command == Arguments::Command::Compile && !result.rawCount)
        throw std::invalid_argument("--raw-count must be positive");
    if (result.command == Arguments::Command::Merge && result.shards.empty())
        throw std::invalid_argument("--merge-transitions needs --shard inputs");
    if (result.command == Arguments::Command::Measure && !result.iterations)
        throw std::invalid_argument("--measure must be positive");
    if (!result.workers)
        throw std::invalid_argument("--workers must be positive");
    result.resources.maxBddNodes = result.bdd.maxNodes;
    return result;
}

[[nodiscard]] SolveOptions solve_options(const Arguments& arguments) {
    SolveOptions result;
    result.transitionPrefix = arguments.transitionPrefix;
    result.sourceTable = arguments.input;
    result.lowerGhostSidecar = arguments.lower;
    result.scratchPrefix = arguments.scratch;
    result.outputOverlay = arguments.output;
    result.outputArbitrarySidecar = arguments.arbitrary;
    result.sourceSha256 = arguments.sourceSha;
    result.modelSha256 = arguments.modelSha;
    result.observationSha256 = arguments.observationSha;
    result.lowerGhostSourceSha256 = arguments.lowerSourceSha;
    result.lowerGhostModelSha256 = arguments.lowerModelSha;
    result.lowerGhostObservationSha256 = arguments.lowerObservationSha;
    result.lowerGhostSidecarSha256 = arguments.lowerSidecarSha;
    result.bdd = arguments.bdd;
    result.resources = arguments.resources;
    result.workers = arguments.workers;
    result.compactEvery = arguments.compactEvery;
    result.measureIterations = arguments.iterations;
    return result;
}

void print_transition(const TransitionCertificate& certificate) {
    std::cout << "ghost_pair_transition raw " << certificate.rawGeometries
      << " canonical " << certificate.canonicalGeometries
      << " worlds " << certificate.worlds
      << " live " << certificate.liveWorlds
      << " admitted " << certificate.admittedFreshWorlds
      << " terminal " << certificate.terminalWorlds
      << " actions " << certificate.actions
      << " observations " << certificate.observations
      << " edges " << certificate.edges
      << " codec_residual " << certificate.codecResidual
      << " action_residual " << certificate.actionResidual
      << " decision_residual " << certificate.decisionResidual
      << " transition_residual " << certificate.transitionResidual
      << " symmetry_residual " << certificate.symmetryResidual
      << " payload_sha256 " << certificate.payloadSha256 << '\n';
}

void print_solve(const SolveCertificate& certificate) {
    for (std::size_t side = 0; side < 2; ++side) {
        const unsigned win = side == 0 ? 1 : 2;
        const unsigned loss = side == 0 ? 2 : 1;
        std::cout << "information_summary side " << side
          << " win " << certificate.totals[side][win]
          << " loss " << certificate.totals[side][loss]
          << " draw " << certificate.totals[side][3]
          << " unreachable_win " << certificate.unreachable[side][win]
          << " unreachable_loss " << certificate.unreachable[side][loss]
          << " unreachable_draw " << certificate.unreachable[side][3]
          << " sets " << certificate.informationSets[side]
          << " concrete " << certificate.legalRealizations[side] +
                               certificate.unreachableRealizations[side]
          << " bellman_residual " << certificate.bellmanResidual
          << " rank_residual " << certificate.rankResidual
          << " belief_cap none exhaustive 1\n";
    }
    std::cout << "information_symbolic_certificate iterations "
      << certificate.iterations << " bdd_nodes " << certificate.bddNodes
      << " bellman_residual " << certificate.bellmanResidual
      << " monotonicity_residual " << certificate.rankResidual
      << " singleton_residual " << certificate.singletonResidual
      << " belief_cap none powerset_exact 1\n"
      << "ghost_pair_domain_cache roots " << certificate.domainRoots
      << " hits " << certificate.domainCacheHits
      << " misses " << certificate.domainCacheMisses
      << " entries " << certificate.domainCacheEntries
      << " residual 0\n"
      << "ghost_pair_artifacts transition_payload_sha256 "
      << certificate.transitionPayloadSha256
      << " transition_header_sha256 " << certificate.transitionHeaderSha256
      << " transition_marker_sha256 " << certificate.transitionMarkerSha256
      << " lower_sidecar_sha256 " << certificate.lowerGhostSidecarSha256
      << " overlay_sha256 " << certificate.overlaySha256
      << " arbitrary_sha256 " << certificate.arbitrarySidecarSha256 << '\n';
}

}  // namespace
}  // namespace Stockfish::Ultimate::GhostPairInformation

int main(int count, char** values) {
    using namespace Stockfish::Ultimate::GhostPairInformation;
    try {
        const Arguments arguments = parse(count, values);
        switch (arguments.command) {
          case Arguments::Command::SelfTest:
            exact_small_domain_self_test(arguments.scratch + ".self-test");
            std::cout << "ghost_pair_self_test residual 0\n";
            break;
          case Arguments::Command::Compile: {
            TransitionCompileOptions options;
            options.prefix = arguments.transitionPrefix;
            options.sourceSha256 = arguments.sourceSha;
            options.modelSha256 = arguments.modelSha;
            options.observationSha256 = arguments.observationSha;
            options.rawGeometryBegin = arguments.rawBegin;
            options.rawGeometryCount = arguments.rawCount;
            print_transition(compile_transition_database(options));
            break;
          }
          case Arguments::Command::Merge:
            print_transition(merge_transition_databases(arguments.shards,
              arguments.transitionPrefix, arguments.sourceSha,
              arguments.modelSha, arguments.observationSha,
              arguments.requireComplete));
            break;
          case Arguments::Command::VerifyTransitions:
            print_transition(verify_transition_database(
              arguments.transitionPrefix, arguments.sourceSha,
              arguments.modelSha, arguments.observationSha,
              arguments.requireComplete));
            break;
          case Arguments::Command::Measure:
          case Arguments::Command::Solve:
            print_solve(solve_exact(solve_options(arguments)));
            break;
          case Arguments::Command::VerifyOverlay:
            print_solve(verify_exact_overlay(solve_options(arguments)));
            break;
          case Arguments::Command::VerifyArbitrary: {
            const ArbitrarySidecarCertificate certificate =
              verify_arbitrary_sidecar(arguments.arbitrary,
                                        solve_options(arguments));
            std::cout << "ghost_pair_arbitrary nodes " << certificate.nodes
              << " geometries " << certificate.geometries
              << " strata " << certificate.strata
              << " owner_roots " << certificate.ownerRoots
              << " bytes " << certificate.bytes
              << " payload_sha256 " << certificate.payloadSha256
              << " file_sha256 " << certificate.fileSha256 << '\n';
            break;
          }
          case Arguments::Command::None:
            throw std::logic_error("unreachable Ghost-pair command");
        }
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error) {
        std::cerr << "Ghost-pair information error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
