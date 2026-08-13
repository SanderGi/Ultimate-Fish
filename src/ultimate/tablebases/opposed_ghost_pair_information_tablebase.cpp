/*
  Ultimate Fish - opposed Ghost/Ghost graph capture CLI
  Copyright (C) 2026 Ultimate Fish contributors
  GPLv3 or later.
*/

#include "opposed_ghost_pair_information_fixed_point.h"
#include "opposed_ghost_pair_information_lower_oracle.h"
#include "opposed_ghost_pair_information_sidecar.h"
#include "opposed_ghost_pair_information_solver.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdint>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <utility>

namespace Stockfish::Ultimate {
namespace {

namespace Model = OpposedGhostPairInformation;
namespace Solver = OpposedGhostPairSolver;
constexpr const char* NoDependencySha256 =
  "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

std::atomic<bool> stopRequested{false};

void request_stop(int) {
    stopRequested.store(true);
}

[[nodiscard]] std::uint64_t number(const std::string& text,
                                   const char* field) {
    std::size_t consumed = 0;
    const std::uint64_t value = std::stoull(text, &consumed);
    if (consumed != text.size())
        throw std::invalid_argument(std::string("invalid ") + field);
    return value;
}

[[nodiscard]] std::uint32_t number32(const std::string& text,
                                     const char* field) {
    const std::uint64_t value = number(text, field);
    if (value > std::numeric_limits<std::uint32_t>::max())
        throw std::out_of_range(std::string(field) + " exceeds uint32");
    return static_cast<std::uint32_t>(value);
}

void sync_file(const std::string& path) {
    const int descriptor = ::open(path.c_str(), O_RDONLY);
    if (descriptor < 0)
        throw std::runtime_error("cannot open crossed checkpoint for fsync");
    const int result = ::fsync(descriptor);
    const int saved = errno;
    (void)::close(descriptor);
    if (result != 0)
        throw std::runtime_error(
          "cannot fsync crossed checkpoint: errno " +
          std::to_string(saved));
}

void sync_parent(const std::string& path) {
    const std::size_t slash = path.find_last_of('/');
    const std::string parent = slash == std::string::npos ? "."
                             : slash == 0 ? "/" : path.substr(0, slash);
    const int descriptor = ::open(parent.c_str(), O_RDONLY);
    if (descriptor < 0)
        throw std::runtime_error(
          "cannot open crossed checkpoint directory for fsync");
    const int result = ::fsync(descriptor);
    const int saved = errno;
    (void)::close(descriptor);
    if (result != 0)
        throw std::runtime_error(
          "cannot fsync crossed checkpoint directory: errno " +
          std::to_string(saved));
}

[[nodiscard]] std::string parent_path(const std::string& path) {
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? "."
         : slash == 0 ? "/" : path.substr(0, slash);
}

void require_same_volume(const std::string& scratch,
                         const std::string& output) {
    struct stat scratchStatus {};
    struct stat outputStatus {};
    if (::stat(scratch.c_str(), &scratchStatus) != 0 ||
        ::stat(parent_path(output).c_str(), &outputStatus) != 0)
        throw std::runtime_error(
          "cannot stat crossed solve scratch/output directories");
    if (scratchStatus.st_dev != outputStatus.st_dev)
        throw std::runtime_error(
          "crossed solve scratch and sidecar must share one gated volume");
}

struct Options {
    Options() {
        lower.jesterTableSha256 = NoDependencySha256;
        lower.jesterOverlaySha256 = NoDependencySha256;
        lower.jesterModelSha256 = NoDependencySha256;
    }
    std::string checkpoint;
    std::uint32_t rawBegin = 0;
    std::uint32_t rawCount = Model::RawPublicFrameCount;
    std::uint32_t seedBatch = 10'000;
    std::uint64_t expansionBatch = 10'000;
    std::uint64_t maximumExpansions =
      std::numeric_limits<std::uint64_t>::max();
    std::uint64_t maximumResidentBytes =
      std::numeric_limits<std::uint64_t>::max();
    std::uint64_t maximumCheckpointBytes =
      std::numeric_limits<std::uint64_t>::max();
    std::uint64_t minimumFreeBytes = 0;
    std::uint64_t maximumSolveScratchBytes =
      std::numeric_limits<std::uint64_t>::max();
    std::string scratchDirectory;
    std::string outputSidecar;
    std::string outputOverlay;
    std::string sourceTable;
    std::string sourceSha256;
    std::string modelSha256;
    std::string observationSha256;
    std::string checkpointSha256;
    std::string sidecarSha256;
    std::string overlaySha256;
    Solver::LowerOracleOptions lower;
    bool verifyOnly = false;
    bool verifySidecar = false;
    bool verifyResult = false;
    bool measureSolve = false;
    bool solve = false;
};

struct Resources {
    std::uint64_t peakResidentBytes = 0;
    std::uint64_t checkpointBytes = 0;
    std::uint64_t freeBytes = 0;
};

[[nodiscard]] Resources resources(const std::string& checkpoint) {
    Resources result;
    struct rusage usage {};
    if (::getrusage(RUSAGE_SELF, &usage) != 0)
        throw std::runtime_error("cannot read opposed-Ghost graph peak RSS");
#if defined(__APPLE__)
    result.peakResidentBytes = static_cast<std::uint64_t>(usage.ru_maxrss);
#else
    result.peakResidentBytes = static_cast<std::uint64_t>(usage.ru_maxrss) * 1024;
#endif
    struct stat status {};
    if (::stat(checkpoint.c_str(), &status) != 0 || status.st_size < 0)
        throw std::runtime_error("cannot stat opposed-Ghost graph checkpoint");
    result.checkpointBytes = static_cast<std::uint64_t>(status.st_size);
    struct statvfs volume {};
    if (::statvfs(checkpoint.c_str(), &volume) != 0)
        throw std::runtime_error("cannot stat opposed-Ghost graph volume");
    if (volume.f_bavail && volume.f_frsize >
          std::numeric_limits<std::uint64_t>::max() / volume.f_bavail)
        result.freeBytes = std::numeric_limits<std::uint64_t>::max();
    else
        result.freeBytes = static_cast<std::uint64_t>(volume.f_bavail) *
                           volume.f_frsize;
    return result;
}

void resource_gate(const Options& options) {
    const Resources current = resources(options.checkpoint);
    std::cout << "crossed_graph_resources peak_rss_bytes "
              << current.peakResidentBytes << " checkpoint_bytes "
              << current.checkpointBytes << " free_bytes "
              << current.freeBytes << '\n';
    if (current.peakResidentBytes > options.maximumResidentBytes)
        throw std::runtime_error(
          "opposed-Ghost graph resident resource gate exceeded");
    if (current.checkpointBytes > options.maximumCheckpointBytes)
        throw std::runtime_error(
          "opposed-Ghost graph checkpoint resource gate exceeded");
    if (current.freeBytes < options.minimumFreeBytes)
        throw std::runtime_error("opposed-Ghost graph free-disk gate failed");
}

[[nodiscard]] Options parse(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        const auto value = [&](const char* field) -> std::string {
            if (++index >= argc)
                throw std::invalid_argument(std::string("missing ") + field);
            return argv[index];
        };
        if (argument == "--checkpoint")
            options.checkpoint = value("checkpoint path");
        else if (argument == "--raw-begin")
            options.rawBegin = number32(value("raw begin"), "raw begin");
        else if (argument == "--raw-count")
            options.rawCount = number32(value("raw count"), "raw count");
        else if (argument == "--seed-batch")
            options.seedBatch = number32(value("seed batch"), "seed batch");
        else if (argument == "--expansion-batch")
            options.expansionBatch = number(
              value("expansion batch"), "expansion batch");
        else if (argument == "--maximum-expansions")
            options.maximumExpansions = number(
              value("maximum expansions"), "maximum expansions");
        else if (argument == "--maximum-resident-bytes")
            options.maximumResidentBytes = number(
              value("maximum resident bytes"), "maximum resident bytes");
        else if (argument == "--maximum-checkpoint-bytes")
            options.maximumCheckpointBytes = number(
              value("maximum checkpoint bytes"), "maximum checkpoint bytes");
        else if (argument == "--minimum-free-bytes")
            options.minimumFreeBytes = number(
              value("minimum free bytes"), "minimum free bytes");
        else if (argument == "--maximum-solve-scratch-bytes")
            options.maximumSolveScratchBytes = number(
              value("maximum solve scratch bytes"),
              "maximum solve scratch bytes");
        else if (argument == "--scratch-directory")
            options.scratchDirectory = value("scratch directory");
        else if (argument == "--output-sidecar")
            options.outputSidecar = value("output sidecar");
        else if (argument == "--output-overlay")
            options.outputOverlay = value("output overlay");
        else if (argument == "--source-table")
            options.sourceTable = value("source table");
        else if (argument == "--source-sha256")
            options.sourceSha256 = value("source SHA-256");
        else if (argument == "--model-sha256")
            options.modelSha256 = value("model SHA-256");
        else if (argument == "--observation-sha256")
            options.observationSha256 = value("observation SHA-256");
        else if (argument == "--checkpoint-sha256")
            options.checkpointSha256 = value("checkpoint SHA-256");
        else if (argument == "--sidecar-sha256")
            options.sidecarSha256 = value("sidecar SHA-256");
        else if (argument == "--overlay-sha256")
            options.overlaySha256 = value("overlay SHA-256");
        else if (argument == "--lower-jester-table")
            options.lower.jesterTable = value("lower Jester table");
        else if (argument == "--lower-jester-table-sha256")
            options.lower.jesterTableSha256 =
              value("lower Jester table SHA-256");
        else if (argument == "--lower-jester-overlay")
            options.lower.jesterOverlay = value("lower Jester overlay");
        else if (argument == "--lower-jester-overlay-sha256")
            options.lower.jesterOverlaySha256 =
              value("lower Jester overlay SHA-256");
        else if (argument == "--lower-jester-model-sha256")
            options.lower.jesterModelSha256 =
              value("lower Jester model SHA-256");
        else if (argument == "--lower-ghost-sidecar")
            options.lower.ghostSidecar = value("lower Ghost sidecar");
        else if (argument == "--lower-ghost-sidecar-sha256")
            options.lower.ghostSidecarSha256 =
              value("lower Ghost sidecar SHA-256");
        else if (argument == "--lower-ghost-source-sha256")
            options.lower.ghostSourceSha256 =
              value("lower Ghost source SHA-256");
        else if (argument == "--lower-ghost-model-sha256")
            options.lower.ghostModelSha256 =
              value("lower Ghost model SHA-256");
        else if (argument == "--lower-ghost-observation-sha256")
            options.lower.ghostObservationSha256 =
              value("lower Ghost observation SHA-256");
        else if (argument == "--verify-checkpoint")
            options.verifyOnly = true;
        else if (argument == "--verify-sidecar")
            options.verifySidecar = true;
        else if (argument == "--verify-result")
            options.verifyResult = true;
        else if (argument == "--measure-solve")
            options.measureSolve = true;
        else if (argument == "--solve")
            options.solve = true;
        else
            throw std::invalid_argument("unknown opposed-Ghost graph argument: " +
                                        argument);
    }
    if (options.checkpoint.empty() || !options.seedBatch ||
        !options.expansionBatch)
        throw std::invalid_argument(
          "checkpoint, nonzero seed batch, and expansion batch are required");
    if (options.rawBegin > Model::RawPublicFrameCount ||
        options.rawCount > Model::RawPublicFrameCount - options.rawBegin)
        throw std::out_of_range("opposed-Ghost graph raw range is outside domain");
    if (unsigned(options.verifyOnly) + unsigned(options.verifySidecar) +
          unsigned(options.verifyResult) +
          unsigned(options.measureSolve) + unsigned(options.solve) > 1)
        throw std::invalid_argument(
          "crossed verification, measurement, and solve modes are exclusive");
    if ((options.measureSolve || options.solve) &&
        (options.rawBegin || options.rawCount != Model::RawPublicFrameCount ||
         options.checkpointSha256.empty() || options.scratchDirectory.empty()))
        throw std::invalid_argument(
          "crossed solve requires the full graph, checkpoint SHA, and scratch directory");
    if (options.solve &&
        (options.outputSidecar.empty() || options.outputOverlay.empty() ||
         options.sourceTable.empty() || options.sourceSha256.empty() ||
         options.modelSha256.empty() || options.observationSha256.empty() ||
         options.lower.ghostSidecar.empty() ||
         options.lower.ghostSidecarSha256.empty() ||
         options.lower.ghostSourceSha256.empty() ||
         options.lower.ghostModelSha256.empty() ||
         options.lower.ghostObservationSha256.empty()))
        throw std::invalid_argument(
          "crossed production solve is missing a provenance-bound input");
    if (options.verifyResult &&
        (options.outputSidecar.empty() || options.sidecarSha256.empty() ||
         options.outputOverlay.empty() || options.overlaySha256.empty() ||
         options.sourceTable.empty() || options.sourceSha256.empty() ||
         options.modelSha256.empty() || options.observationSha256.empty() ||
         options.checkpointSha256.empty() ||
         options.lower.ghostSidecarSha256.empty()))
        throw std::invalid_argument(
          "crossed result verification is missing a provenance binding");
    if (options.verifySidecar &&
        (options.outputSidecar.empty() || options.sidecarSha256.empty() ||
         options.sourceSha256.empty() || options.modelSha256.empty() ||
         options.observationSha256.empty() ||
         options.checkpointSha256.empty() ||
         options.lower.ghostSidecarSha256.empty()))
        throw std::invalid_argument(
          "crossed sidecar verification is missing a provenance binding");
    return options;
}

[[nodiscard]] Solver::GraphDiscovery load(
  const std::string& checkpoint) {
    std::ifstream input(checkpoint, std::ios::binary);
    if (!input)
        throw std::runtime_error("cannot open opposed-Ghost graph checkpoint");
    auto [discovery, certificate] = Solver::GraphDiscovery::read(input);
    std::cout << "crossed_graph_restored roots " << certificate.roots
              << " empty " << certificate.emptyRoots
              << " expanded " << certificate.expanded
              << " nodes " << certificate.graph.nodes
              << " payload_bytes " << certificate.graph.payloadBytes
              << " residual "
              << (certificate.rootBoundsResidual + certificate.cursorResidual +
                  certificate.extentResidual +
                  certificate.graph.keyRoundtripResidual +
                  certificate.graph.canonicalResidual +
                  certificate.graph.duplicateResidual)
              << '\n';
    return std::move(discovery);
}

void save(const Solver::GraphDiscovery& discovery,
          const std::string& checkpoint) {
    const std::string temporary = checkpoint + ".tmp";
    Solver::DiscoveryArchiveCertificate certificate;
    {
        std::ofstream output(temporary,
                             std::ios::binary | std::ios::trunc);
        if (!output)
            throw std::runtime_error(
              "cannot create opposed-Ghost graph checkpoint temporary");
        certificate = discovery.write(output);
        output.flush();
        if (!output)
            throw std::runtime_error("cannot flush opposed-Ghost graph checkpoint");
    }
    sync_file(temporary);
    if (std::rename(temporary.c_str(), checkpoint.c_str()) != 0)
        throw std::runtime_error("cannot install opposed-Ghost graph checkpoint");
    sync_parent(checkpoint);
    std::cout << "crossed_graph_checkpoint roots " << certificate.roots
              << " empty " << certificate.emptyRoots
              << " expanded " << certificate.expanded
              << " nodes " << certificate.graph.nodes
              << " payload_bytes " << certificate.graph.payloadBytes
              << " durable 1 residual 0\n";
}

[[nodiscard]] std::uint64_t checked_add(
  std::uint64_t lhs, std::uint64_t rhs, const char* field) {
    if (lhs > std::numeric_limits<std::uint64_t>::max() - rhs)
        throw std::overflow_error(std::string("crossed ") + field +
                                  " overflow");
    return lhs + rhs;
}

struct SolvePreflight {
    Solver::FixedPointPreflight white;
    Solver::FixedPointPreflight black;
    std::uint64_t sidecarBytes = 0;
    std::uint64_t overlayBytes = 0;
    std::uint64_t requiredFreeBytes = 0;
};

[[nodiscard]] SolvePreflight solve_preflight(
  Solver::GraphDiscovery& discovery, const Options& options) {
    if (!discovery.closed() || discovery.raw_begin() ||
        discovery.roots().size() != Model::RawPublicFrameCount)
        throw std::runtime_error(
          "crossed solve requires a closed full-domain graph");
    SolvePreflight result;
    result.white = Solver::preflight_closed_graph(
      discovery.arena(), Color::White);
    result.black = Solver::preflight_closed_graph(
      discovery.arena(), Color::Black);
    if (result.white.nodes != result.black.nodes ||
        result.white.atomVariables != result.black.atomVariables)
        throw std::runtime_error(
          "crossed target preflight dimensions disagree");
    std::uint64_t keyBytes = 0;
    for (Solver::NodeId node = 0; node < discovery.arena().size(); ++node)
        keyBytes = checked_add(
          keyBytes, discovery.arena().encoded_key(node).size(),
          "sidecar key bytes");
    const std::uint64_t bitBytes =
      (result.white.atomVariables + 7) / 8;
    result.sidecarBytes = checked_add(
      checked_add(
        checked_add(640,
          std::uint64_t(Model::RawPublicFrameCount) * 4,
          "sidecar root bytes"),
        result.white.nodes * 16, "sidecar index bytes"),
      checked_add(keyBytes, 2 * bitBytes, "sidecar payload bytes"),
      "sidecar extent");
    result.overlayBytes = checked_add(160, Model::StateCount,
                                      "dense overlay extent");
    const std::uint64_t scratch = std::max(
      result.white.peakScratchBytes, result.black.peakScratchBytes);
    if (scratch > options.maximumSolveScratchBytes)
        throw std::runtime_error(
          "crossed solve scratch resource gate exceeded");
    result.requiredFreeBytes = checked_add(
      checked_add(scratch, checked_add(result.sidecarBytes,
                                       result.overlayBytes,
                                       "solve result bytes"),
                  "solve durable peak bytes"),
      options.minimumFreeBytes, "solve free-disk floor");
    struct statvfs volume {};
    if (::statvfs(options.scratchDirectory.c_str(), &volume) != 0)
        throw std::runtime_error("cannot stat crossed solve scratch volume");
    const std::uint64_t freeBytes =
      volume.f_bavail && volume.f_frsize >
        std::numeric_limits<std::uint64_t>::max() / volume.f_bavail
      ? std::numeric_limits<std::uint64_t>::max()
      : std::uint64_t(volume.f_bavail) * volume.f_frsize;
    const auto print = [](const Solver::FixedPointPreflight& value) {
        std::cout << "crossed_solve_preflight target "
                  << (value.target == Color::White ? "white" : "black")
                  << " nodes " << value.nodes
                  << " atoms " << value.atomVariables
                  << " gates " << value.gateVariables
                  << " variables " << value.totalVariables
                  << " tokens " << value.tokenReferences
                  << " reverse_edges " << value.reverseEdges
                  << " external_constants " << value.externalConstants
                  << " peak_scratch_bytes " << value.peakScratchBytes
                  << " residual " << value.residual << '\n';
    };
    print(result.white);
    print(result.black);
    std::cout << "crossed_solve_resources sidecar_bytes "
              << result.sidecarBytes << " overlay_bytes "
              << result.overlayBytes << " required_free_bytes "
              << result.requiredFreeBytes << " actual_free_bytes "
              << freeBytes << " residual 0\n";
    if (freeBytes < result.requiredFreeBytes)
        throw std::runtime_error("crossed solve free-disk gate failed");
    return result;
}

void print_solution(const Solver::FixedPointCertificate& certificate) {
    std::cout << "crossed_fixed_point target "
              << (certificate.target == Color::White ? "white" : "black")
              << " nodes " << certificate.nodes
              << " atoms " << certificate.atomVariables
              << " gates " << certificate.gateVariables
              << " variables " << certificate.totalVariables
              << " internal_tokens " << certificate.internalTokens
              << " external_true " << certificate.externalTrue
              << " external_false " << certificate.externalFalse
              << " reverse_edges " << certificate.solve.reverseEdges
              << " activated " << certificate.solve.activated
              << " bellman_residual " << certificate.solve.bellmanResidual
              << " rank_residual " << certificate.solve.rankResidual
              << " equation_residual " << certificate.equationResidual
              << '\n';
}

void solve(Solver::GraphDiscovery& discovery, const Options& options) {
    require_same_volume(options.scratchDirectory, options.outputSidecar);
    require_same_volume(options.scratchDirectory, options.outputOverlay);
    const std::string checkpointSha =
      Solver::authenticated_file_sha256(options.checkpoint);
    if (checkpointSha != options.checkpointSha256)
        throw std::runtime_error("opposed-Ghost graph checkpoint SHA mismatch");
    (void)solve_preflight(discovery, options);
    Solver::AuthenticatedLowerForceOracle oracle(options.lower);
    const Solver::LowerOracleCertificate& lower = oracle.certificate();
    std::cout << "crossed_lower_oracle jester_states "
              << lower.jesterStates << " ghost_geometries "
              << lower.ghostGeometries << " ghost_strata "
              << lower.ghostStrata << " ghost_nodes " << lower.ghostNodes
              << " residual 0\n";
    Solver::PackedForcePlane white;
    {
        Solver::FixedPointSolution solution = Solver::solve_closed_graph(
          discovery.arena(), Color::White, oracle, options.scratchDirectory);
        print_solution(solution.certificate());
        white = solution.pack();
    }
    Solver::PackedForcePlane black;
    {
        Solver::FixedPointSolution solution = Solver::solve_closed_graph(
          discovery.arena(), Color::Black, oracle, options.scratchDirectory);
        print_solution(solution.certificate());
        black = solution.pack();
    }
    const Solver::SidecarBindings bindings{
      options.sourceSha256, options.modelSha256, options.observationSha256,
      checkpointSha, lower.jesterTableSha256, lower.jesterOverlaySha256,
      lower.ghostSidecarSha256};
    const Solver::SidecarCertificate written =
      Solver::write_arbitrary_sidecar(
        options.outputSidecar, discovery, white, black, bindings);
    Solver::CrossedSidecarProbe restored(
      options.outputSidecar, written.fileSha256, bindings);
    const Solver::SidecarCertificate& checked = restored.certificate();
    if (checked.nodes != written.nodes || checked.atoms != written.atoms ||
        checked.payloadSha256 != written.payloadSha256)
        throw std::runtime_error("crossed sidecar restore residual");
    const Solver::DenseOverlayCertificate overlay =
      Solver::write_dense_overlay(options.outputOverlay, options.sourceTable,
        discovery, white, black, bindings);
    const Solver::DenseOverlayCertificate overlayChecked =
      Solver::verify_dense_overlay(options.outputOverlay,
        overlay.fileSha256, options.sourceTable, discovery, restored, bindings);
    if (overlayChecked.totals != overlay.totals ||
        overlayChecked.unreachable != overlay.unreachable ||
        overlayChecked.informationSets != overlay.informationSets)
        throw std::runtime_error("crossed dense overlay restore residual");
    for (std::size_t side = 0; side < 2; ++side) {
        std::cout << "information_summary side " << side
                  << " win " << overlay.totals[side][1]
                  << " loss " << overlay.totals[side][2]
                  << " draw " << overlay.totals[side][3]
                  << " unreachable_win " << overlay.unreachable[side][1]
                  << " unreachable_loss " << overlay.unreachable[side][2]
                  << " unreachable_draw " << overlay.unreachable[side][3]
                  << " sets " << overlay.informationSets[side]
                  << " concrete " << Model::StateCount / 2
                  << " bellman_residual 0 belief_cap none exhaustive 1\n";
    }
    std::cout << "crossed_overlay_certificate bytes "
              << 160 + Model::StateCount << " file_sha256 "
              << overlay.fileSha256 << " root_residual "
              << overlay.rootResidual << " atom_residual "
              << overlay.atomResidual << " force_residual "
              << overlay.forceResidual << " dual_force_residual "
              << overlay.dualForceResidual << " conservation_residual "
              << overlay.conservationResidual << " restore_residual 0\n";
    std::cout << "crossed_sidecar_certificate roots " << written.roots
              << " nodes " << written.nodes << " atoms " << written.atoms
              << " key_bytes " << written.keyBytes
              << " payload_bytes " << written.payloadBytes
              << " payload_sha256 " << written.payloadSha256
              << " file_sha256 " << written.fileSha256
              << " root_residual " << written.rootBoundsResidual
              << " key_residual " << written.keyOrderResidual
              << " dual_force_residual " << written.dualForceResidual
              << " restore_residual 0\n";
}

int run(const Options& options) {
    if (options.verifyResult) {
        const std::string checkpointSha =
          Solver::authenticated_file_sha256(options.checkpoint);
        if (checkpointSha != options.checkpointSha256)
            throw std::runtime_error("opposed-Ghost graph checkpoint SHA mismatch");
        Solver::GraphDiscovery discovery = load(options.checkpoint);
        const Solver::SidecarBindings bindings{
          options.sourceSha256, options.modelSha256,
          options.observationSha256, checkpointSha,
          options.lower.jesterTableSha256,
          options.lower.jesterOverlaySha256,
          options.lower.ghostSidecarSha256};
        Solver::CrossedSidecarProbe sidecar(
          options.outputSidecar, options.sidecarSha256, bindings);
        const Solver::DenseOverlayCertificate overlay =
          Solver::verify_dense_overlay(options.outputOverlay,
            options.overlaySha256, options.sourceTable, discovery,
            sidecar, bindings);
        std::cout << "crossed_result_restore_certificate overlay_sha256 "
                  << overlay.fileSha256 << " sidecar_sha256 "
                  << sidecar.certificate().fileSha256
                  << " root_residual " << overlay.rootResidual
                  << " atom_residual " << overlay.atomResidual
                  << " force_residual " << overlay.forceResidual
                  << " dual_force_residual " << overlay.dualForceResidual
                  << " conservation_residual "
                  << overlay.conservationResidual
                  << " checkpoint_residual 0 restore_residual 0\n";
        return 0;
    }
    if (options.verifySidecar) {
        const std::string checkpointSha =
          Solver::authenticated_file_sha256(options.checkpoint);
        if (checkpointSha != options.checkpointSha256)
            throw std::runtime_error(
              "opposed-Ghost graph checkpoint SHA mismatch");
        const Solver::SidecarBindings bindings{
          options.sourceSha256, options.modelSha256,
          options.observationSha256, checkpointSha,
          options.lower.jesterTableSha256,
          options.lower.jesterOverlaySha256,
          options.lower.ghostSidecarSha256};
        Solver::CrossedSidecarProbe restored(
          options.outputSidecar, options.sidecarSha256, bindings);
        const Solver::SidecarCertificate& checked = restored.certificate();
        std::cout << "crossed_sidecar_restore_certificate roots "
                  << checked.roots << " nodes " << checked.nodes
                  << " atoms " << checked.atoms
                  << " key_bytes " << checked.keyBytes
                  << " payload_bytes " << checked.payloadBytes
                  << " payload_sha256 " << checked.payloadSha256
                  << " file_sha256 " << checked.fileSha256
                  << " root_residual " << checked.rootBoundsResidual
                  << " key_residual " << checked.keyOrderResidual
                  << " dual_force_residual " << checked.dualForceResidual
                  << " checkpoint_residual 0 restore_residual 0\n";
        return 0;
    }
    const bool exists = bool(std::ifstream(options.checkpoint,
                                           std::ios::binary));
    Solver::GraphDiscovery discovery = exists
      ? load(options.checkpoint)
      : Solver::GraphDiscovery::seed(options.rawBegin, 0);
    if (discovery.raw_begin() != options.rawBegin ||
        discovery.roots().size() > options.rawCount)
        throw std::runtime_error(
          "opposed-Ghost graph checkpoint does not match requested raw range");
    if (options.verifyOnly) {
        if (!exists)
            throw std::runtime_error(
              "cannot verify a missing opposed-Ghost graph checkpoint");
        resource_gate(options);
        return 0;
    }
    if (options.measureSolve || options.solve) {
        if (!exists)
            throw std::runtime_error(
              "cannot solve a missing opposed-Ghost graph checkpoint");
        if (options.measureSolve) {
            const std::string checkpointSha =
              Solver::authenticated_file_sha256(options.checkpoint);
            if (checkpointSha != options.checkpointSha256)
                throw std::runtime_error(
                  "opposed-Ghost graph checkpoint SHA mismatch");
            (void)solve_preflight(discovery, options);
        }
        else
            solve(discovery, options);
        return 0;
    }

    while (discovery.roots().size() < options.rawCount &&
           !stopRequested.load()) {
        const std::uint32_t count = static_cast<std::uint32_t>(std::min<
          std::uint64_t>(options.seedBatch,
          options.rawCount - discovery.roots().size()));
        const Solver::FreshSeedCertificate seeded =
          discovery.append_seed(count);
        std::cout << "crossed_graph_seed raw_begin " << seeded.rawBegin
                  << " raw_count " << seeded.rawCount
                  << " admitted " << seeded.admitted
                  << " empty " << seeded.empty
                  << " unique " << seeded.unique
                  << " duplicate " << seeded.duplicate
                  << " residual " << seeded.codecResidual << '\n';
        save(discovery, options.checkpoint);
        resource_gate(options);
    }

    std::uint64_t invocationExpanded = 0;
    while (!discovery.closed() &&
           invocationExpanded < options.maximumExpansions &&
           !stopRequested.load()) {
        const std::uint64_t count = std::min(
          options.expansionBatch,
          options.maximumExpansions - invocationExpanded);
        const Solver::DiscoveryCertificate progress = discovery.advance(count);
        invocationExpanded += progress.expanded;
        std::cout << "crossed_graph_expand expanded " << progress.expanded
                  << " cursor " << discovery.expanded()
                  << " nodes_before " << progress.nodesBefore
                  << " nodes_after " << progress.nodesAfter
                  << " actions " << progress.actions
                  << " observations " << progress.observations
                  << " outcomes " << progress.outcomes
                  << " new_nodes " << progress.newlyInterned
                  << " closed " << unsigned(progress.closed)
                  << " residual " << progress.residual << '\n';
        save(discovery, options.checkpoint);
        resource_gate(options);
        if (!progress.expanded)
            throw std::runtime_error(
              "opposed-Ghost graph discovery made no closure progress");
    }
    std::cout << "crossed_graph_status raw_begin " << discovery.raw_begin()
              << " raw_count " << discovery.roots().size()
              << " expanded " << discovery.expanded()
              << " nodes " << discovery.arena().size()
              << " closed " << unsigned(discovery.closed())
              << " interrupted " << unsigned(stopRequested.load())
              << " residual 0\n";
    return 0;
}

}  // namespace
}  // namespace Stockfish::Ultimate

int main(int argc, char** argv) {
    try {
        std::signal(SIGINT, Stockfish::Ultimate::request_stop);
        std::signal(SIGTERM, Stockfish::Ultimate::request_stop);
        return Stockfish::Ultimate::run(
          Stockfish::Ultimate::parse(argc, argv));
    }
    catch (const std::exception& error) {
        std::cerr << "Crossed Jester/Ghost graph error: " << error.what()
                  << '\n';
        return 1;
    }
}
