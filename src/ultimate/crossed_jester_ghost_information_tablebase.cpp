/*
  Ultimate Fish - crossed Jester/Ghost graph capture CLI
  Copyright (C) 2026 Ultimate Fish contributors
  GPLv3 or later.
*/

#include "crossed_jester_ghost_information_solver.h"

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
#include <unistd.h>
#include <utility>

namespace Stockfish::Ultimate {
namespace {

namespace Model = CrossedJesterGhostInformation;
namespace Solver = CrossedJesterGhostSolver;

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

struct Options {
    std::string checkpoint;
    std::uint32_t rawBegin = 0;
    std::uint32_t rawCount = Model::RawPublicFrameCount;
    std::uint32_t seedBatch = 10'000;
    std::uint64_t expansionBatch = 10'000;
    std::uint64_t maximumExpansions =
      std::numeric_limits<std::uint64_t>::max();
    bool verifyOnly = false;
};

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
        else if (argument == "--verify-checkpoint")
            options.verifyOnly = true;
        else
            throw std::invalid_argument("unknown crossed graph argument: " +
                                        argument);
    }
    if (options.checkpoint.empty() || !options.seedBatch ||
        !options.expansionBatch)
        throw std::invalid_argument(
          "checkpoint, nonzero seed batch, and expansion batch are required");
    if (options.rawBegin > Model::RawPublicFrameCount ||
        options.rawCount > Model::RawPublicFrameCount - options.rawBegin)
        throw std::out_of_range("crossed graph raw range is outside domain");
    return options;
}

[[nodiscard]] Solver::GraphDiscovery load(
  const std::string& checkpoint) {
    std::ifstream input(checkpoint, std::ios::binary);
    if (!input)
        throw std::runtime_error("cannot open crossed graph checkpoint");
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
              "cannot create crossed graph checkpoint temporary");
        certificate = discovery.write(output);
        output.flush();
        if (!output)
            throw std::runtime_error("cannot flush crossed graph checkpoint");
    }
    sync_file(temporary);
    if (std::rename(temporary.c_str(), checkpoint.c_str()) != 0)
        throw std::runtime_error("cannot install crossed graph checkpoint");
    sync_parent(checkpoint);
    std::cout << "crossed_graph_checkpoint roots " << certificate.roots
              << " empty " << certificate.emptyRoots
              << " expanded " << certificate.expanded
              << " nodes " << certificate.graph.nodes
              << " payload_bytes " << certificate.graph.payloadBytes
              << " durable 1 residual 0\n";
}

int run(const Options& options) {
    const bool exists = bool(std::ifstream(options.checkpoint,
                                           std::ios::binary));
    Solver::GraphDiscovery discovery = exists
      ? load(options.checkpoint)
      : Solver::GraphDiscovery::seed(options.rawBegin, 0);
    if (discovery.raw_begin() != options.rawBegin ||
        discovery.roots().size() > options.rawCount)
        throw std::runtime_error(
          "crossed graph checkpoint does not match requested raw range");
    if (options.verifyOnly) {
        if (!exists)
            throw std::runtime_error(
              "cannot verify a missing crossed graph checkpoint");
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
        if (!progress.expanded)
            throw std::runtime_error(
              "crossed graph discovery made no closure progress");
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
