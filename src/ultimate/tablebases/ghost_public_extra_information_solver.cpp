/*
  Ultimate Fish - exact reciprocal public-extra/Ghost information solver
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.

  The certified same-side Bishop/Ghost implementation is intentionally frozen
  by its d597 model fingerprint.  This domain reuses that audited external
  transition and fixed-point implementation textually, without changing it,
  and applies the already-tested reciprocal ownership adapter in a distinct
  translation unit and fingerprint domain.
*/

#include "ghost_public_extra_information_solver.h"

#include "external_robdd.h"
#include "ghost_information_probe.h"
#include "information.h"
#include "position.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

// All headers above are guarded before this narrowly scoped access shim.  It
// exposes only implementation members from the frozen .cpp so this separate
// domain can retain and serialize the final exact force roots.  No frozen
// source byte or same-side fingerprint is changed.
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#define main ultimate_frozen_ghost_extra_standalone_main
#include "ghost_extra_information_tablebase.cpp"
#undef main
#undef private
#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace Stockfish::Ultimate::GhostPublicExtraExact {
namespace {

constexpr std::uint32_t SidecarVersion = 2;
constexpr std::uint32_t Endian = 0x01020304;
constexpr std::uint32_t Variables = Position::BoardSquares;
constexpr char Semantics[] = "fresh-maximal-public-view-v2:reciprocal-bishop-ghost";

#pragma pack(push, 1)
struct NodeDisk {
    std::uint8_t variable = Variables;
    std::uint32_t low = 0;
    std::uint32_t high = 0;
};

struct SidecarHeader {
    std::array<char, 8> magic{{'U','F','G','X','2','\0','\0','\0'}};
    std::uint32_t version = SidecarVersion;
    std::uint32_t headerBytes = sizeof(SidecarHeader);
    std::uint32_t endian = Endian;
    std::uint32_t primary = static_cast<std::uint32_t>(PieceType::Bishop);
    std::uint32_t secondary = static_cast<std::uint32_t>(PieceType::Ghost);
    std::uint32_t ghostColor = static_cast<std::uint32_t>(Color::Black);
    std::uint32_t squares = Variables;
    std::uint32_t stateCount = GhostPublicExtra::StateCount;
    std::uint32_t nodeBytes = sizeof(NodeDisk);
    std::uint32_t geometryBytes = sizeof(ExternalGeometryMeta);
    std::uint32_t maskBytes = sizeof(ExternalMask);
    std::uint32_t rootBytes = sizeof(ExternalRobdd::Id);
    std::uint32_t reserved = 0;
    std::uint64_t nodes = 0;
    std::uint64_t geometries = 0;
    std::uint64_t strata = 0;
    std::uint64_t ownerRoots = 0;
    std::uint64_t nodeOffset = 0;
    std::uint64_t geometryOffset = 0;
    std::uint64_t stratumOffset = 0;
    std::uint64_t ownerOffset = 0;
    std::uint64_t observerOffset = 0;
    std::uint64_t visibleOwnerOffset = 0;
    std::uint64_t visibleObserverOffset = 0;
    std::uint64_t payloadBytes = 0;
    std::array<char, 64> sourceSha{};
    std::array<char, 64> modelSha{};
    std::array<char, 64> observationSha{};
    std::array<char, 64> lowerGhostSha{};
    std::array<std::array<char, 64>, 6> transitionSha{};
    std::array<char, 64> transitionPayloadSha{};
    std::array<char, 64> payloadSha{};
    std::array<char, 64> semantics{};
};
#pragma pack(pop)

static_assert(sizeof(NodeDisk) == 9);
static_assert(sizeof(SidecarHeader) == 988);

[[nodiscard]] MaterialSpec reciprocal_material() {
    MaterialSpec material;
    material.ghostColor = Color::Black;
    return material;
}

[[nodiscard]] MaterialSpec constructor_material() {
    MaterialSpec material;
    material.ghostColor = Color::White;
    return material;
}

[[nodiscard]] std::string sha256_file(const std::string& path,
                                      std::uint64_t offset = 0) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("cannot hash reciprocal Ghost artifact: " + path);
    input.seekg(static_cast<std::streamoff>(offset));
    Sha256 hash;
    std::array<std::uint8_t, 1 << 20> buffer{};
    while (input) {
        input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        if (input.gcount() > 0)
            hash.update(buffer.data(), static_cast<std::size_t>(input.gcount()));
    }
    return hex_digest(hash.finish());
}

void copy_hash(std::array<char, 64>& target, const std::string& source,
               const char* label) {
    if (!valid_sha256(source))
        throw std::invalid_argument(std::string(label) + " is not SHA-256");
    std::copy(source.begin(), source.end(), target.begin());
}

[[nodiscard]] std::string transition_payload_sha(
  const std::string& prefix) {
    Sha256 hash;
    std::array<std::uint8_t, 1 << 20> buffer{};
    for (const char* suffix : {".header", ".meta", ".strata", ".index",
                               ".blocks", ".verified"}) {
        std::ifstream input(prefix + suffix, std::ios::binary);
        if (!input)
            throw std::runtime_error("missing reciprocal transition provenance");
        while (input) {
            input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
            if (input.gcount() > 0)
                hash.update(buffer.data(),
                            static_cast<std::size_t>(input.gcount()));
        }
    }
    return hex_digest(hash.finish());
}

template<typename Value>
void write_value(std::ostream& output, const Value& value) {
    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void validate_unique_node_tuples(const NodeDisk* nodes,
                                 std::uint64_t count) {
    std::uint64_t slotCount = 4;
    while (slotCount < count * 2) {
        if (slotCount > (std::uint64_t{1} << 32) / 2)
            throw std::runtime_error(
              "reciprocal tuple-uniqueness index exceeds 32 bits");
        slotCount *= 2;
    }
    std::vector<std::uint32_t> slots(
      static_cast<std::size_t>(slotCount),
      std::numeric_limits<std::uint32_t>::max());
    for (std::uint32_t id = 2; id < count; ++id) {
        const NodeDisk& node = nodes[id];
        std::uint64_t hash = node.variable;
        hash = (hash ^ node.low) * 0x9e3779b97f4a7c15ULL;
        hash = (hash ^ node.high) * 0xbf58476d1ce4e5b9ULL;
        std::size_t slot = static_cast<std::size_t>(
          hash & (slots.size() - 1));
        for (;;) {
            std::uint32_t& occupant = slots[slot];
            if (occupant == std::numeric_limits<std::uint32_t>::max()) {
                occupant = id;
                break;
            }
            const NodeDisk& previous = nodes[occupant];
            if (previous.variable == node.variable &&
                previous.low == node.low && previous.high == node.high)
                throw std::runtime_error(
                  "reciprocal sidecar contains a duplicate ROBDD tuple");
            slot = (slot + 1) & (slots.size() - 1);
        }
    }
}

void prove_arbitrary_disjoint(ExternalGhostExtraFixedPoint& solver,
                              SolveCertificate& certificate) {
    for (std::uint32_t geometry = 0;
         geometry < solver.database_.geometry_count(); ++geometry) {
        const ExternalGeometryMeta& meta = solver.database_.meta(geometry);
        if ((meta.terminalOwner.low & meta.terminalObserver.low) ||
            (meta.terminalOwner.high & meta.terminalObserver.high))
            ++certificate.arbitraryDualForceResidual;
        const PublicExtraGeometry& physical = solver.domain_[geometry];
        for (unsigned actual = 0; actual < Variables; ++actual) {
            if (!external_mask_test(meta.live, actual))
                continue;
            const std::uint64_t ownerIndex =
              std::uint64_t(geometry) * Variables + actual;
            if (physical.visible) {
                certificate.arbitraryDualForceResidual +=
                  solver.visibleOwnerCurrent_[ownerIndex] &&
                  solver.visibleObserverCurrent_[ownerIndex];
                continue;
            }
            const std::uint32_t stratum = meta.actualStratum[actual];
            if (stratum == NoIndex || stratum >= solver.observerCurrent_.size())
                throw std::runtime_error(
                  "reciprocal arbitrary root lacks a decision stratum");
            certificate.arbitraryDualForceResidual +=
              solver.bdd_->logical_and(solver.ownerCurrent_[ownerIndex],
                solver.observerCurrent_[stratum]) != ExternalRobdd::False;
        }
    }
    if (certificate.arbitraryDualForceResidual)
        throw std::runtime_error(
          "reciprocal arbitrary owner/observer force intersection is nonempty");
}

void compact_for_sidecar(ExternalGhostExtraFixedPoint& solver,
                         SolveCertificate& certificate) {
    std::vector<ExternalRobdd::Id> roots;
    roots.reserve(solver.ownerCurrent_.size() +
                  solver.observerCurrent_.size());
    for (std::uint64_t id = 0; id < solver.ownerCurrent_.size(); ++id)
        roots.push_back(solver.ownerCurrent_[id]);
    for (std::uint64_t id = 0; id < solver.observerCurrent_.size(); ++id)
        roots.push_back(solver.observerCurrent_[id]);
    auto [compact, proof] = solver.bdd_->compact(
      solver.options_.scratch + ".sidecar-bdd",
      solver.options_.scratch + ".sidecar-remap", roots);
    certificate.arbitraryStructuralResidual =
      proof.structuralResidual + proof.rootResidual;
    if (certificate.arbitraryStructuralResidual)
        throw std::runtime_error("reciprocal sidecar compaction residual");
    std::size_t cursor = 0;
    for (std::uint64_t id = 0; id < solver.ownerCurrent_.size(); ++id)
        solver.ownerCurrent_[id] = roots.at(cursor++);
    for (std::uint64_t id = 0; id < solver.observerCurrent_.size(); ++id)
        solver.observerCurrent_[id] = roots.at(cursor++);
    if (cursor != roots.size())
        throw std::runtime_error("reciprocal sidecar root remap residual");
    solver.bdd_ = std::move(compact);
}

[[nodiscard]] SolveCertificate write_sidecar(
  const std::string& path, const SolveOptions& options,
  ExternalGhostExtraFixedPoint& solver, SolveCertificate certificate) {
    if (path.empty())
        throw std::invalid_argument("reciprocal solve requires UFGX2 output");
    prove_arbitrary_disjoint(solver, certificate);
    compact_for_sidecar(solver, certificate);

    SidecarHeader header;
    header.nodes = solver.bdd_->node_count();
    header.geometries = solver.database_.geometry_count();
    header.strata = solver.database_.stratum_count();
    header.ownerRoots = solver.ownerCurrent_.size();
    header.nodeOffset = sizeof(header);
    header.geometryOffset = header.nodeOffset + header.nodes * sizeof(NodeDisk);
    header.stratumOffset = header.geometryOffset +
                           header.geometries * sizeof(ExternalGeometryMeta);
    header.ownerOffset = header.stratumOffset +
                         header.strata * sizeof(ExternalMask);
    header.observerOffset = header.ownerOffset +
                            header.ownerRoots * sizeof(ExternalRobdd::Id);
    header.visibleOwnerOffset = header.observerOffset +
                               header.strata * sizeof(ExternalRobdd::Id);
    header.visibleObserverOffset = header.visibleOwnerOffset +
                                  header.ownerRoots;
    const std::uint64_t extent = header.visibleObserverOffset +
                                 header.ownerRoots;
    header.payloadBytes = extent - sizeof(header);
    copy_hash(header.sourceSha, options.sourceSha256, "source SHA");
    copy_hash(header.modelSha, options.modelSha256, "model SHA");
    copy_hash(header.observationSha, options.observationSha256,
              "observation SHA");
    copy_hash(header.lowerGhostSha, options.lowerGhostSidecarSha256,
              "lower Ghost sidecar SHA");
    std::size_t component = 0;
    for (const char* suffix : {".header", ".meta", ".strata", ".index",
                               ".blocks", ".verified"}) {
        copy_hash(header.transitionSha[component++],
                  sha256_file(options.transitionPrefix + suffix),
                  "transition component SHA");
    }
    certificate.transitionHeaderSha256 =
      std::string(header.transitionSha[0].data(), 64);
    certificate.transitionVerifiedSha256 =
      std::string(header.transitionSha[5].data(), 64);
    certificate.transitionPayloadSha256 = transition_payload_sha(
      options.transitionPrefix);
    copy_hash(header.transitionPayloadSha,
              certificate.transitionPayloadSha256,
              "transition payload SHA");
    std::copy(std::begin(Semantics), std::end(Semantics),
              header.semantics.begin());

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    write_value(output, header);
    for (ExternalRobdd::Id id = 0; id < header.nodes; ++id) {
        const ExternalRobdd::Node node = solver.bdd_->node(id);
        write_value(output, NodeDisk{node.variable, node.low, node.high});
    }
    for (std::uint32_t id = 0; id < header.geometries; ++id)
        write_value(output, solver.database_.meta(id));
    for (std::uint32_t id = 0; id < header.strata; ++id)
        write_value(output, solver.database_.stratum(id));
    output.write(reinterpret_cast<const char*>(solver.ownerCurrent_.begin()),
                 static_cast<std::streamsize>(header.ownerRoots *
                                              sizeof(ExternalRobdd::Id)));
    output.write(reinterpret_cast<const char*>(solver.observerCurrent_.begin()),
                 static_cast<std::streamsize>(header.strata *
                                              sizeof(ExternalRobdd::Id)));
    output.write(reinterpret_cast<const char*>(
                   solver.visibleOwnerCurrent_.begin()),
                 static_cast<std::streamsize>(header.ownerRoots));
    output.write(reinterpret_cast<const char*>(
                   solver.visibleObserverCurrent_.begin()),
                 static_cast<std::streamsize>(header.ownerRoots));
    output.close();
    if (!output || external_file_bytes(path) != extent)
        throw std::runtime_error("reciprocal arbitrary sidecar extent residual");
    const std::string payload = sha256_file(path, sizeof(header));
    copy_hash(header.payloadSha, payload, "sidecar payload SHA");
    std::fstream rewrite(path, std::ios::binary | std::ios::in |
                               std::ios::out);
    write_value(rewrite, header);
    rewrite.close();
    if (!rewrite)
        throw std::runtime_error("cannot finalize reciprocal sidecar");
    certificate.arbitrarySha256 = sha256_file(path);
    return certificate;
}

[[nodiscard]] std::pair<PublicExtraGeometry, std::uint8_t>
canonical_query(const GhostPublicExtra::ConcreteState& state,
                GhostPublicExtra::GhostMask& belief) {
    const GhostPublicExtra::MaterialSpec material =
      GhostPublicExtra::bishop_reciprocal();
    PublicExtraGeometry raw{
      static_cast<std::uint8_t>(state.side), state.whiteKing,
      state.blackKing, GhostPublicExtra::extra_square(state, material),
      static_cast<std::uint8_t>(state.ghostVisible)};
    const CanonicalExtraGeometry canonical = canonical_geometry(raw);
    GhostPublicExtra::GhostMask mapped;
    for (unsigned square = 0; square < Variables; ++square)
        if (belief.test(square))
            mapped.set(rectangle_transform_square(
              static_cast<std::uint8_t>(square), canonical.transform));
    belief = mapped;
    return {canonical.geometry, rectangle_transform_square(
      GhostPublicExtra::ghost_square(state, material), canonical.transform)};
}

// The frozen d597 compiler intentionally rejects reciprocal ownership before
// entering its otherwise material-generic body.  Keep that source byte-stable
// and reproduce the exact audited compiler here for the distinct reciprocal
// fingerprint.  Its exhaustive reload verifier below independently rebuilds
// every metadata field and edge from native rules before issuing .verified.
void compile_reciprocal_external_transitions(
  const std::string& prefix, std::uint32_t geometryStart,
  std::uint32_t geometryLimit) {
    const MaterialSpec material = reciprocal_material();
    const auto started = std::chrono::steady_clock::now();
    const ExtraGeometryDomain domain;
    if (geometryStart >= domain.size())
        throw std::runtime_error(
          "reciprocal transition shard starts outside the geometry domain");
    const std::uint32_t remaining = static_cast<std::uint32_t>(
      domain.size() - geometryStart);
    const std::uint32_t count = geometryLimit
      ? std::min(geometryLimit, remaining) : remaining;
    std::ofstream headerFile(prefix + ".header",
      std::ios::binary | std::ios::trunc);
    std::ofstream metaFile(prefix + ".meta",
      std::ios::binary | std::ios::trunc);
    std::ofstream strataFile(prefix + ".strata",
      std::ios::binary | std::ios::trunc);
    std::ofstream indexFile(prefix + ".index",
      std::ios::binary | std::ios::trunc);
    std::ofstream blockFile(prefix + ".blocks",
      std::ios::binary | std::ios::trunc);
    if (!headerFile || !metaFile || !strataFile || !indexFile || !blockFile)
        throw std::runtime_error(
          "cannot create reciprocal Ghost-extra transition scratch");
    ExternalTransitionHeader header;
    header.material = static_cast<std::uint32_t>(material.ghostColor);
    header.geometryCount = count;
    header.reserved = geometryStart;
    headerFile.write(reinterpret_cast<const char*>(&header), sizeof(header));
    ExternalCompileSummary summary;
    const DisclosureContext observer{material.observer(), false};
    for (std::uint32_t localGeometry = 0; localGeometry < count;
         ++localGeometry) {
        const std::uint32_t geometryId = geometryStart + localGeometry;
        const PublicExtraGeometry& geometry = domain[geometryId];
        ExternalGeometryMeta meta;
        meta.actualStratum.fill(NoIndex);
        std::map<std::string, ExternalMask> decisionBlocks;
        for (std::uint8_t ghost = 0; ghost < Squares; ++ghost) {
            if (!valid_geometry_world(geometry, ghost))
                continue;
            Position position = make_geometry_position(geometry, ghost,
                                                       material);
            ++summary.worlds;
            if (position.game_over()) {
                external_mask_set(meta.terminal, ghost);
                const std::uint8_t flags = terminal_force_flags(position,
                                                                 material);
                if (flags & 1) external_mask_set(meta.terminalOwner, ghost);
                if (flags & 2) external_mask_set(meta.terminalObserver, ghost);
                ++summary.terminalWorlds;
                continue;
            }
            external_mask_set(meta.live, ghost);
            if (geometry.visible)
                continue;
            const std::string decision =
              static_cast<Color>(geometry.side) == material.observer()
              ? decision_observation_key(position, observer) : std::string();
            external_mask_set(decisionBlocks[decision], ghost);
        }
        meta.stratumBase = static_cast<std::uint32_t>(summary.strata);
        meta.stratumCount = static_cast<std::uint32_t>(decisionBlocks.size());
        for (const auto& [decision, mask] : decisionBlocks) {
            (void)decision;
            const std::uint32_t stratum =
              static_cast<std::uint32_t>(summary.strata++);
            strataFile.write(reinterpret_cast<const char*>(&mask),
                             sizeof(mask));
            for (unsigned ghost = 0; ghost < Squares; ++ghost)
                if (external_mask_test(mask, ghost))
                    meta.actualStratum[ghost] = stratum;
        }

        std::array<std::vector<ExternalCompiledEdge>, Squares> perSource;
        std::unordered_map<std::string, std::uint32_t> observations;
        std::unordered_map<std::string, std::uint32_t> actions;
        std::array<ExternalStringBijection, 4>
          actionBijection, observationBijection;
        for (std::uint8_t ghost = 0; ghost < Squares; ++ghost) {
            if (!external_mask_test(meta.live, ghost))
                continue;
            Position position = make_geometry_position(geometry, ghost,
                                                       material);
            const std::vector<Move> moves = position.legal_moves();
            for (const Move& move : moves) {
                Position child = position;
                Undo undo;
                if (!child.make_move(move, undo))
                    throw std::runtime_error(
                      "reciprocal compiler failed a legal move");
                const std::string observation = complete_transition_observation(
                  position, move, child, observer);
                const std::string action = position.move_to_string(move);
                const auto intern = [](auto& map, const std::string& text) {
                    if (const auto found = map.find(text); found != map.end())
                        return found->second;
                    const std::uint32_t id =
                      static_cast<std::uint32_t>(map.size());
                    if (!map.emplace(text, id).second)
                        throw std::runtime_error(
                          "reciprocal transition interner insertion failed");
                    return id;
                };
                ExternalCompiledEdge edge;
                edge.relation = intern(observations, observation);
                edge.action = intern(actions, action);
                const ExternalCompiledEdge childEdge = encode_external_child(
                  child, material, domain);
                edge.child = childEdge.child;
                edge.childActual = childEdge.childActual;
                edge.domain = childEdge.domain;
                edge.exact = childEdge.exact;
                if (edge.domain == ExternalChildDomain::SameClass)
                    ++summary.sameClass;
                else if (edge.domain == ExternalChildDomain::LowerGhost)
                    ++summary.lowerGhost;
                else
                    ++summary.exact;
                perSource[ghost].push_back(edge);
                ++summary.edges;
            }

            for (std::uint8_t transform = 1;
                 transform < GeometryTransformCount; ++transform) {
                const PublicExtraGeometry transformedGeometry =
                  transform_geometry(geometry, transform);
                const std::uint8_t transformedGhost =
                  rectangle_transform_square(ghost, transform);
                Position pairedPosition = make_geometry_position(
                  transformedGeometry, transformedGhost, material);
                const std::vector<Move> pairedMoves =
                  pairedPosition.legal_moves();
                if (pairedMoves.size() != moves.size())
                    throw std::runtime_error(
                      "D2 symmetry changed reciprocal legal-action count");
                std::unordered_map<std::uint64_t, std::size_t> pairedByMove;
                pairedByMove.reserve(pairedMoves.size() * 2);
                for (std::size_t pairedIndex = 0;
                     pairedIndex < pairedMoves.size(); ++pairedIndex) {
                    const std::uint64_t key = external_move_key(
                      pairedPosition, pairedMoves[pairedIndex], 0,
                      transformedGeometry.bishop);
                    if (!pairedByMove.emplace(key, pairedIndex).second)
                        throw std::runtime_error(
                          "D2 target duplicates reciprocal legal action");
                }
                for (std::size_t moveIndex = 0; moveIndex < moves.size();
                     ++moveIndex) {
                    const std::uint64_t wanted = external_move_key(
                      position, moves[moveIndex], transform, geometry.bishop);
                    const auto paired = pairedByMove.find(wanted);
                    if (paired == pairedByMove.end())
                        throw std::runtime_error(
                          "D2 symmetry lost reciprocal legal action");
                    const std::size_t pairedIndex = paired->second;
                    Position pairedChild = pairedPosition;
                    Undo pairedUndo;
                    if (!pairedChild.make_move(pairedMoves[pairedIndex],
                                               pairedUndo))
                        throw std::runtime_error(
                          "D2 reciprocal paired move failed");
                    verify_external_child_symmetry(
                      perSource[ghost][moveIndex], encode_external_child(
                        pairedChild, material, domain));
                    actionBijection[transform].bind(
                      perSource[ghost][moveIndex].action,
                      pairedPosition.move_to_string(pairedMoves[pairedIndex]),
                      "action");
                    observationBijection[transform].bind(
                      perSource[ghost][moveIndex].relation,
                      complete_transition_observation(
                        pairedPosition, pairedMoves[pairedIndex], pairedChild,
                        observer), "observation");
                    ++summary.symmetryEdges;
                }
                ++summary.symmetryWorlds;
            }
        }
        summary.observationClasses += observations.size();
        summary.actionClasses += actions.size();
        const std::uint64_t blockOffset =
          static_cast<std::uint64_t>(blockFile.tellp());
        indexFile.write(reinterpret_cast<const char*>(&blockOffset),
                        sizeof(blockOffset));
        std::array<std::uint32_t, Squares + 1> offsets{};
        std::uint32_t edgeCount = 0;
        for (unsigned ghost = 0; ghost < Squares; ++ghost) {
            offsets[ghost] = edgeCount;
            edgeCount += static_cast<std::uint32_t>(perSource[ghost].size());
        }
        offsets[Squares] = edgeCount;
        blockFile.write(reinterpret_cast<const char*>(offsets.data()),
                        sizeof(offsets));
        for (const auto& edges : perSource)
            blockFile.write(reinterpret_cast<const char*>(edges.data()),
                            edges.size() * sizeof(ExternalCompiledEdge));
        metaFile.write(reinterpret_cast<const char*>(&meta), sizeof(meta));
        ++summary.geometries;
        header.completedGeometries = localGeometry + 1;
        header.edges = summary.edges;
        header.strata = summary.strata;
        header.blockBytes = static_cast<std::uint64_t>(blockFile.tellp());
        if ((localGeometry + 1) % 1'000 == 0 ||
            localGeometry + 1 == count) {
            headerFile.seekp(0);
            headerFile.write(reinterpret_cast<const char*>(&header),
                             sizeof(header));
            headerFile.flush();
            metaFile.flush();
            strataFile.flush();
            indexFile.flush();
            blockFile.flush();
            const double elapsed = std::chrono::duration<double>(
              std::chrono::steady_clock::now() - started).count();
            std::cout << "reciprocal_ghost_extra_compile "
                      << localGeometry + 1 << '/' << count
                      << " global_geometry " << geometryId + 1
                      << " worlds " << summary.worlds
                      << " edges " << summary.edges
                      << " strata " << summary.strata
                      << " block_bytes " << header.blockBytes
                      << " peak_rss_bytes " << peak_rss_bytes()
                      << " elapsed " << elapsed << "s\n" << std::flush;
        }
    }
    const std::uint64_t finalOffset =
      static_cast<std::uint64_t>(blockFile.tellp());
    indexFile.write(reinterpret_cast<const char*>(&finalOffset),
                    sizeof(finalOffset));
    if (!headerFile || !metaFile || !strataFile || !indexFile || !blockFile)
        throw std::runtime_error(
          "failed writing reciprocal transition certificate");
    headerFile.close();
    metaFile.close();
    strataFile.close();
    indexFile.close();
    blockFile.close();
    verify_external_transition_certificate(prefix, material);
}

}  // namespace

void compile_transitions(const TransitionOptions& options) {
    if (options.prefix.empty() || !options.geometryCount)
        throw std::invalid_argument("reciprocal transition range is empty");
    compile_reciprocal_external_transitions(
      options.prefix, options.geometryBegin, options.geometryCount);
}

void merge_transitions(const std::string& outputPrefix,
                       const std::vector<std::string>& shards,
                       std::uint32_t expectedGeometries) {
    merge_external_transition_shards(outputPrefix, shards,
      reciprocal_material(), expectedGeometries);
}

void verify_transitions(const std::string& prefix) {
    verify_external_transition_certificate(prefix, reciprocal_material());
}

ResourceEstimate resource_estimate() {
    ResourceEstimate result;
    result.geometries =
      2ULL * Variables * (Variables - 1) * (Variables - 2) * 2 / 4;
    result.variables = Variables;
    result.ownerRoots = result.geometries * Variables;
    result.estimatedTransitionBytes = 14'500'000'000ULL;
    result.rootBytes = result.ownerRoots *
      (2 * sizeof(ExternalRobdd::Id) + 2) +
      result.geometries * 8 * sizeof(ExternalRobdd::Id);
    result.bddBytes = ExternalRobdd::required_bytes(
      ExternalRobdd::Limits{});
    result.peakScratchBytes = result.estimatedTransitionBytes +
      2 * result.bddBytes + result.rootBytes;
    // Strict restore additionally collision-checks every full ROBDD tuple in
    // a uint32 open-address index; budget its worst configured ~4 GiB here.
    result.peakResidentBytes = 18ULL << 30;
    return result;
}

void prove_sidecar_singletons(const ArbitrarySidecarProbe& probe,
                              ExternalGhostExtraFixedPoint& solver,
                              SolveCertificate& certificate) {
    const GhostPublicExtra::MaterialSpec material =
      GhostPublicExtra::bishop_reciprocal();
    for (std::uint32_t geometry = 0;
         geometry < solver.database_.geometry_count(); ++geometry) {
        const PublicExtraGeometry& physical = solver.domain_[geometry];
        const ExternalGeometryMeta& meta = solver.database_.meta(geometry);
        for (unsigned actual = 0; actual < Variables; ++actual) {
            if (!external_mask_test(meta.live, actual) &&
                !external_mask_test(meta.terminal, actual))
                continue;
            GhostPublicExtra::ConcreteState state;
            state.side = static_cast<Color>(physical.side);
            state.whiteKing = physical.whiteKing;
            state.blackKing = physical.blackKing;
            state.ghostVisible = physical.visible != 0;
            state = GhostPublicExtra::with_piece_squares(
              state, material, physical.bishop,
              static_cast<std::uint8_t>(actual));
            GhostPublicExtra::GhostMask singleton;
            singleton.set(actual);
            const std::uint64_t ownerIndex =
              std::uint64_t(geometry) * Variables + actual;
            bool expectedOwner = false;
            bool expectedObserver = false;
            if (external_mask_test(meta.terminal, actual)) {
                expectedOwner = external_mask_test(meta.terminalOwner, actual);
                expectedObserver = external_mask_test(meta.terminalObserver,
                                                       actual);
            }
            else if (physical.visible) {
                expectedOwner = solver.visibleOwnerCurrent_[ownerIndex];
                expectedObserver = solver.visibleObserverCurrent_[ownerIndex];
            }
            else {
                const std::uint32_t stratum = meta.actualStratum[actual];
                if (stratum == NoIndex ||
                    stratum >= solver.observerCurrent_.size())
                    throw std::runtime_error(
                      "reciprocal singleton lacks decision stratum");
                expectedOwner = solver.bdd_->evaluate(
                  solver.ownerCurrent_[ownerIndex], singleton.low,
                  singleton.high);
                expectedObserver = solver.bdd_->evaluate(
                  solver.observerCurrent_[stratum], singleton.low,
                  singleton.high);
            }
            certificate.arbitrarySingletonResidual +=
              probe.owner_forces(state, singleton) != expectedOwner;
            certificate.arbitrarySingletonResidual +=
              probe.observer_forces(state, singleton) != expectedObserver;
        }
    }
    if (certificate.arbitrarySingletonResidual)
        throw std::runtime_error(
          "reciprocal arbitrary singleton reproduction residual");
}

void reciprocal_fresh_admission_self_test() {
    const GhostPublicExtra::MaterialSpec material =
      GhostPublicExtra::bishop_reciprocal();
    const auto square = [](const char* name) {
        const int value = Position::square_from_name(name);
        if (value == Position::NoSquare)
            throw std::runtime_error("invalid reciprocal admission square");
        return static_cast<std::uint8_t>(value);
    };
    GhostPublicExtra::ConcreteState nearObserver;
    nearObserver.side = Color::Black;
    nearObserver.whiteKing = square("a2");
    nearObserver.blackKing = square("h10");
    nearObserver = GhostPublicExtra::with_piece_squares(
      nearObserver, material, square("d4"), square("b2"));
    const Position rejected = GhostPublicExtra::make_position(
      nearObserver, material);
    if (GhostPublicExtra::classify_fresh_root_admission(rejected, material) !=
          GhostPublicExtra::FreshAdmissionVerdict::Reject)
        throw std::runtime_error(
          "reciprocal fresh admission exposed a Ghost by the White King");

    GhostPublicExtra::ConcreteState nearOwner;
    nearOwner.side = Color::Black;
    nearOwner.whiteKing = square("a2");
    nearOwner.blackKing = square("h10");
    nearOwner = GhostPublicExtra::with_piece_squares(
      nearOwner, material, square("d4"), square("g9"));
    const Position admitted = GhostPublicExtra::make_position(nearOwner,
                                                               material);
    if (GhostPublicExtra::classify_fresh_root_admission(admitted, material) !=
          GhostPublicExtra::FreshAdmissionVerdict::Admit)
        throw std::runtime_error(
          "reciprocal fresh admission rejected a Ghost by its own Black King");
    std::cout << "reciprocal_fresh_admission observer_adjacent_rejected 1"
              << " owner_adjacent_admitted 1 residual 0\n";
}

[[nodiscard]] bool run_reciprocal_fixed_point(
  ExternalGhostExtraFixedPoint& solver, ExtraGeometryDomain& domain) {
    const auto started = std::chrono::steady_clock::now();
    for (;;) {
        ++solver.iteration_;
        std::uint64_t changedOwner = 0;
        std::uint64_t changedObserver = 0;
        std::uint64_t changedVisible = 0;
        for (std::uint32_t geometry = 0;
             geometry < solver.database_.geometry_count(); ++geometry) {
            solver.bdd_->clear_computed_caches();
            const ExternalSolverBlock block = build_external_solver_block(
              geometry, solver.database_, solver.lower_, solver.domain_,
              solver.material_);
            solver.bellman_geometry(geometry, block);
            const ExternalGeometryMeta& meta =
              solver.database_.meta(geometry);
            for (unsigned actual = 0; actual < Squares; ++actual) {
                const std::uint64_t index =
                  std::uint64_t(geometry) * Squares + actual;
                const ExternalRobdd::Id oldOwner = solver.ownerCurrent_[index];
                const ExternalRobdd::Id newOwner = solver.ownerNext_[index];
                if (solver.bdd_->logical_and(oldOwner,
                      solver.bdd_->logical_not(newOwner)) !=
                    ExternalRobdd::False)
                    throw std::runtime_error(
                      "reciprocal owner least fixed point regressed");
                changedOwner += oldOwner != newOwner;
                if ((solver.visibleOwnerCurrent_[index] &&
                     !solver.visibleOwnerNext_[index]) ||
                    (solver.visibleObserverCurrent_[index] &&
                     !solver.visibleObserverNext_[index]))
                    throw std::runtime_error(
                      "reciprocal visible least fixed point regressed");
                changedVisible += solver.visibleOwnerCurrent_[index] !=
                                  solver.visibleOwnerNext_[index];
                changedVisible += solver.visibleObserverCurrent_[index] !=
                                  solver.visibleObserverNext_[index];
            }
            for (std::uint32_t local = 0; local < meta.stratumCount; ++local) {
                const std::uint32_t stratum = meta.stratumBase + local;
                const ExternalRobdd::Id oldObserver =
                  solver.observerCurrent_[stratum];
                const ExternalRobdd::Id newObserver =
                  solver.observerNext_[stratum];
                if (solver.bdd_->logical_and(oldObserver,
                      solver.bdd_->logical_not(newObserver)) !=
                    ExternalRobdd::False)
                    throw std::runtime_error(
                      "reciprocal observer least fixed point regressed");
                changedObserver += oldObserver != newObserver;
            }
            if ((geometry + 1) % 5'000 == 0 ||
                geometry + 1 == solver.database_.geometry_count()) {
                const double elapsed = std::chrono::duration<double>(
                  std::chrono::steady_clock::now() - started).count();
                std::cout << "reciprocal_ghost_extra_bellman iteration "
                          << solver.iteration_ << " geometry "
                          << geometry + 1 << '/'
                          << solver.database_.geometry_count()
                          << " bdd_nodes " << solver.bdd_->node_count()
                          << " peak_rss_bytes " << peak_rss_bytes()
                          << " elapsed " << elapsed << "s\n" << std::flush;
            }
        }
        solver.swap_force_arrays();
        std::cout << "reciprocal_ghost_extra_iteration " << solver.iteration_
                  << " bdd_nodes " << solver.bdd_->node_count()
                  << " changed_owner " << changedOwner
                  << " changed_observer " << changedObserver
                  << " changed_visible " << changedVisible
                  << " peak_rss_bytes " << peak_rss_bytes() << '\n'
                  << std::flush;
        if (solver.options_.measureIterations &&
            solver.iteration_ >= solver.options_.measureIterations) {
            std::cout << "reciprocal_ghost_extra_measurement iterations "
                      << solver.iteration_ << " bdd_nodes "
                      << solver.bdd_->node_count()
                      << " peak_rss_bytes " << peak_rss_bytes()
                      << " proof_complete 0 overlay_written 0\n" << std::flush;
            return false;
        }
        if (!changedOwner && !changedObserver && !changedVisible)
            break;
        if (solver.options_.compactEvery &&
            solver.iteration_ % solver.options_.compactEvery == 0)
            solver.compact();
    }

    // Frozen verification is otherwise material-generic. Its final size test
    // is the sole call into the hard-coded d597 fresh reporter, so add a
    // temporary non-addressable sentinel geometry while the independent
    // Bellman/monotonicity/singleton proof runs, then use the reciprocal
    // role-normalized reporter below.
    domain.geometries_.push_back(domain.geometries_.front());
    try {
        solver.verify();
    }
    catch (...) {
        domain.geometries_.pop_back();
        throw;
    }
    domain.geometries_.pop_back();
    return true;
}

void report_reciprocal_fresh_roots(ExternalGhostExtraFixedPoint& solver) {
    const auto started = std::chrono::steady_clock::now();
    const GhostPublicExtra::MaterialSpec admissionMaterial =
      GhostPublicExtra::bishop_reciprocal();
    std::vector<std::uint8_t> admitted((StateCount + 7) / 8, 0);
    std::vector<ExternalMask> freshMasks(solver.database_.stratum_count());
    std::array<std::array<std::uint64_t, 4>, 2> unreachable{};
    std::uint64_t admittedCount = 0;
    for (std::uint32_t index = 0; index < StateCount; ++index) {
        const FourState state = decode_index(index);
        const Position position = make_position(index, solver.material_);
        const GhostPublicExtra::FreshAdmissionVerdict verdict =
          GhostPublicExtra::classify_fresh_root_admission(
            position, admissionMaterial);
        if (verdict ==
              GhostPublicExtra::FreshAdmissionVerdict::NeedsExactCausalAudit)
            throw std::runtime_error(
              "Bishop reciprocal admission unexpectedly needs causal audit");
        if (verdict != GhostPublicExtra::FreshAdmissionVerdict::Admit) {
            ++unreachable[static_cast<std::size_t>(state.side)]
                          [solver.concrete_.result(index)];
            continue;
        }
        admitted[index / 8] |= static_cast<std::uint8_t>(1u << (index % 8));
        ++admittedCount;
        if (!state.visible) {
            const PublicExtraGeometry raw{
              static_cast<std::uint8_t>(state.side), state.whiteKing,
              state.blackKing, state.bishop, 0};
            const auto [geometry, transform] = solver.domain_.locate(raw);
            const unsigned actual = rectangle_transform_square(
              state.ghost, transform);
            const std::uint32_t stratum =
              solver.database_.meta(geometry).actualStratum[actual];
            if (stratum != NoIndex)
                external_mask_set(freshMasks[stratum], actual);
        }
        if ((index + 1) % 5'000'000 == 0)
            std::cout << "reciprocal_fresh_admission " << index + 1 << '/'
                      << StateCount << " admitted " << admittedCount
                      << " elapsed " << std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - started).count()
                      << "s\n" << std::flush;
    }

    std::array<std::array<std::uint64_t, 4>, 2> totals{};
    std::vector<std::uint8_t> flags(StateCount, 0);
    std::array<std::uint64_t, 2> rootSets{};
    std::vector<std::uint8_t> seenVisible(
      std::uint64_t(solver.database_.geometry_count()) * Squares, 0);
    std::vector<std::uint8_t> seenStratum(
      solver.database_.stratum_count(), 0);
    std::vector<std::uint8_t> seenTerminal(
      std::uint64_t(solver.database_.geometry_count()) * 3, 0);
    std::vector<std::uint8_t> independentlySeenVisible(
      std::uint64_t(solver.database_.geometry_count()) * Squares, 0);
    std::vector<std::uint8_t> independentlySeenStratum(
      solver.database_.stratum_count(), 0);
    std::vector<std::uint8_t> independentlySeenTerminal(
      std::uint64_t(solver.database_.geometry_count()) * 3, 0);
    std::array<std::uint64_t, 2> independentSets{};
    std::array<std::uint64_t, 2> realizationCounts{};
    for (std::uint32_t index = 0; index < StateCount; ++index) {
        if (!((admitted[index / 8] >> (index % 8)) & 1u))
            continue;
        const FourState state = decode_index(index);
        const std::size_t side = static_cast<std::size_t>(state.side);
        const PublicExtraGeometry raw{
          static_cast<std::uint8_t>(state.side), state.whiteKing,
          state.blackKing, state.bishop,
          static_cast<std::uint8_t>(state.visible)};
        const auto [geometry, transform] = solver.domain_.locate(raw);
        const unsigned actual = rectangle_transform_square(
          state.ghost, transform);
        const ExternalGeometryMeta& meta = solver.database_.meta(geometry);
        ++realizationCounts[side];
        if (state.visible) {
            const std::uint64_t key =
              std::uint64_t(geometry) * Squares + actual;
            if (!independentlySeenVisible[key]) {
                independentlySeenVisible[key] = 1;
                ++independentSets[side];
            }
        }
        else if (external_mask_test(meta.terminal, actual)) {
            const std::size_t outcome =
              external_mask_test(meta.terminalOwner, actual) ? 0 :
              external_mask_test(meta.terminalObserver, actual) ? 1 : 2;
            const std::uint64_t key = std::uint64_t(geometry) * 3 + outcome;
            if (!independentlySeenTerminal[key]) {
                independentlySeenTerminal[key] = 1;
                ++independentSets[side];
            }
        }
        else {
            const std::uint32_t key = meta.actualStratum[actual];
            if (key == NoIndex)
                throw std::runtime_error(
                  "reciprocal live root lacks a decision observation");
            if (!independentlySeenStratum[key]) {
                independentlySeenStratum[key] = 1;
                ++independentSets[side];
            }
        }

        bool owner = false;
        bool observer = false;
        if (external_mask_test(meta.terminal, actual)) {
            owner = external_mask_test(meta.terminalOwner, actual);
            observer = external_mask_test(meta.terminalObserver, actual);
            if (state.visible) {
                const std::uint64_t root =
                  std::uint64_t(geometry) * Squares + actual;
                if (!seenVisible[root]) {
                    seenVisible[root] = 1;
                    ++rootSets[side];
                }
            }
            else {
                const std::size_t outcome = owner ? 0 : observer ? 1 : 2;
                const std::uint64_t root =
                  std::uint64_t(geometry) * 3 + outcome;
                if (!seenTerminal[root]) {
                    seenTerminal[root] = 1;
                    ++rootSets[side];
                }
            }
        }
        else if (state.visible) {
            const std::uint64_t root =
              std::uint64_t(geometry) * Squares + actual;
            owner = solver.visibleOwnerCurrent_[root];
            observer = solver.visibleObserverCurrent_[root];
            if (!seenVisible[root]) {
                seenVisible[root] = 1;
                ++rootSets[side];
            }
        }
        else {
            const std::uint32_t stratum = meta.actualStratum[actual];
            if (stratum == NoIndex ||
                !external_mask_test(freshMasks[stratum], actual))
                throw std::runtime_error(
                  "reciprocal admitted root crosses its decision stratum");
            owner = solver.bdd_->evaluate(
              solver.ownerCurrent_[std::uint64_t(geometry) * Squares + actual],
              freshMasks[stratum].low, freshMasks[stratum].high);
            observer = solver.bdd_->evaluate(
              solver.observerCurrent_[stratum], freshMasks[stratum].low,
              freshMasks[stratum].high);
            if (!seenStratum[stratum]) {
                seenStratum[stratum] = 1;
                ++rootSets[side];
            }
        }
        if (owner && observer)
            throw std::runtime_error(
              "both players force a reciprocal fresh-root win");
        flags[index] = static_cast<std::uint8_t>(
          4 | (owner ? 1 : 0) | (observer ? 2 : 0));
        const bool moverWins = state.side == solver.material_.ghostColor
                             ? owner : observer;
        const bool moverLoses = state.side == solver.material_.ghostColor
                              ? observer : owner;
        const std::size_t result = moverWins ? 1 : moverLoses ? 2 : 3;
        ++totals[side][result];
    }
    for (std::size_t side = 0; side < 2; ++side) {
        std::uint64_t conserved = 0;
        for (std::size_t result = 1; result < 4; ++result)
            conserved += totals[side][result] + unreachable[side][result];
        if (conserved != StateCount / 2)
            throw std::runtime_error(
              "reciprocal fresh-root summary does not conserve states");
        const std::uint64_t realized = totals[side][1] + totals[side][2] +
                                       totals[side][3];
        if (rootSets[side] != independentSets[side] ||
            realized != realizationCounts[side])
            throw std::runtime_error(
              "reciprocal independent public-root grouping residual");
        std::cout << "information_summary side " << side
                  << " win " << totals[side][1]
                  << " loss " << totals[side][2]
                  << " draw " << totals[side][3]
                  << " unreachable_win " << unreachable[side][1]
                  << " unreachable_loss " << unreachable[side][2]
                  << " unreachable_draw " << unreachable[side][3]
                  << " sets " << rootSets[side]
                  << " concrete " << StateCount / 2
                  << " bellman_residual 0 rank_residual 0"
                  << " belief_cap none exhaustive 1\n";
    }
    std::cout << "reciprocal_root_conservation admitted " << admittedCount
              << " total " << StateCount
              << " independent_grouping_residual 0"
              << " realization_residual 0 conservation_residual 0\n"
              << std::flush;

    std::ofstream output(solver.options_.output,
      std::ios::binary | std::ios::trunc);
    if (!output)
        throw std::runtime_error(
          "cannot create reciprocal information overlay");
    const auto writeU32 = [&](std::uint32_t value) {
        output.write(reinterpret_cast<const char*>(&value), sizeof(value));
    };
    output.write("UFIW2\0\0\0", 8);
    writeU32(2);
    writeU32(static_cast<std::uint32_t>(PieceType::Bishop));
    writeU32(static_cast<std::uint32_t>(PieceType::Ghost));
    writeU32(static_cast<std::uint32_t>(solver.material_.ghostColor));
    writeU32(StateCount);
    writeU32(GhostSubstates);
    output.write(solver.options_.sourceSha256.data(),
      static_cast<std::streamsize>(solver.options_.sourceSha256.size()));
    output.write(solver.options_.modelSha256.data(),
      static_cast<std::streamsize>(solver.options_.modelSha256.size()));
    output.write(reinterpret_cast<const char*>(flags.data()),
                 static_cast<std::streamsize>(flags.size()));
    if (!output)
        throw std::runtime_error(
          "failed writing reciprocal information overlay");
    std::cout << "information_overlay " << solver.options_.output
              << " bytes " << flags.size() + 160
              << " source_sha256 " << solver.options_.sourceSha256
              << " solver_model_sha256 " << solver.options_.modelSha256
              << " observation_model_sha256 "
              << solver.options_.observationSha256
              << " root_grouping_residual 0 conservation_residual 0\n"
              << std::flush;
}

SolveCertificate solve_exact(const SolveOptions& options) {
    if (options.measureIterations == 0 &&
        (options.outputOverlay.empty() || options.outputArbitrary.empty()))
        throw std::invalid_argument(
          "reciprocal exact solve requires UFIW2 and UFGX2 outputs");
    if (sha256_file(options.lowerGhostSidecar) !=
        options.lowerGhostSidecarSha256)
        throw std::runtime_error("lower Ghost sidecar full SHA mismatch");
    verify_transitions(options.transitionPrefix);
    const MaterialSpec reciprocal = reciprocal_material();
    PackedFourTable concrete(options.sourceTable, reciprocal);
    if (hex_digest(concrete.sha()) != options.sourceSha256)
        throw std::runtime_error("reciprocal concrete source SHA mismatch");
    ExternalTransitionDatabase database(options.transitionPrefix, reciprocal);
    ExternalGhostExtraSolveOptions legacy;
    legacy.scratch = options.scratchPrefix;
    legacy.output = options.outputOverlay;
    legacy.sourceSha256 = options.sourceSha256;
    legacy.modelSha256 = options.modelSha256;
    legacy.observationSha256 = options.observationSha256;
    legacy.bddLimits.maxNodes = options.maxNodes;
    legacy.bddLimits.uniqueSlots = options.uniqueSlots;
    legacy.bddLimits.applyCacheEntries = options.applyCacheEntries;
    legacy.bddLimits.unaryCacheEntries = options.unaryCacheEntries;
    legacy.bddLimits.composeCacheEntries = options.composeCacheEntries;
    legacy.bddLimits.budgetBytes = options.bddBudgetBytes;
    legacy.compactEvery = options.compactEvery;
    legacy.measureIterations = options.measureIterations;
    gate_external_ghost_extra_solve(options.transitionPrefix, database, legacy);
    LowerGhostSymbolicSidecar lower(options.lowerGhostSidecar,
      options.lowerGhostSourceSha256, options.lowerGhostModelSha256,
      options.lowerGhostObservationSha256);
    ExtraGeometryDomain domain;

    // The frozen constructor's only non-generic branch protects the d597
    // same-side proof. Construct its material-independent storage/caches under
    // that admitted material, then install the independently validated
    // reciprocal adapter before any Bellman sweep. Re-run both native
    // self-certificates after the install so no same-side result is reused.
    ExternalGhostExtraFixedPoint solver(database, lower, concrete, domain,
      constructor_material(), legacy);
    solver.material_ = reciprocal;
    solver.inherited_lower_mask_self_test();
    reciprocal_fresh_admission_self_test();
    const bool proofComplete = run_reciprocal_fixed_point(solver, domain);
    SolveCertificate certificate;
    if (!proofComplete)
        return certificate;
    report_reciprocal_fresh_roots(solver);
    certificate = write_sidecar(options.outputArbitrary, options, solver,
                                std::move(certificate));
    ArbitrarySidecarProbe probe(options.outputArbitrary, options);
    certificate.arbitraryStructuralResidual +=
      probe.certificate().arbitraryStructuralResidual;
    prove_sidecar_singletons(probe, solver, certificate);
    return certificate;
}

void exact_self_test(const std::string& scratchPrefix) {
    (void)scratchPrefix;
    const MaterialSpec reciprocal = reciprocal_material();
    codec_self_test(reciprocal);
    tiny_public_geometry_self_test();
    lower_color_normalization_self_test();
    reciprocal_fresh_admission_self_test();
    const std::array<NodeDisk, 4> duplicateNodes{{
      {Variables, 0, 0}, {Variables, 1, 1}, {0, 0, 1}, {0, 0, 1}}};
    bool duplicateRejected = false;
    try {
        validate_unique_node_tuples(duplicateNodes.data(),
                                    duplicateNodes.size());
    }
    catch (const std::runtime_error&) {
        duplicateRejected = true;
    }
    if (!duplicateRejected)
        throw std::runtime_error(
          "reciprocal sidecar tuple-uniqueness regression failed");
    const ResourceEstimate estimate = resource_estimate();
    if (estimate.geometries != 492'960 ||
        estimate.ownerRoots != 39'436'800 || !estimate.rootBytes ||
        !estimate.bddBytes || !estimate.peakScratchBytes)
        throw std::runtime_error("reciprocal resource preflight residual");
}

class ArbitrarySidecarProbe::Impl {
  public:
    Impl(const std::string& path, const SolveOptions& bindings)
      : path_(path) {
        open_and_validate({bindings.sourceSha256, bindings.modelSha256,
          bindings.observationSha256, bindings.lowerGhostSidecarSha256, {}},
          &bindings);
    }

    Impl(const std::string& path, const ProbeBindings& bindings)
      : path_(path) {
        open_and_validate(bindings, nullptr);
    }

    void open_and_validate(const ProbeBindings& bindings,
                           const SolveOptions* restore) {
        descriptor_ = ::open(path_.c_str(), O_RDONLY);
        if (descriptor_ < 0)
            external_system_error("cannot open", path_);
        struct stat status{};
        if (::fstat(descriptor_, &status) || status.st_size <= 0)
            external_system_error("cannot stat", path_);
        bytes_ = static_cast<std::uint64_t>(status.st_size);
        data_ = static_cast<const std::uint8_t*>(::mmap(
          nullptr, static_cast<std::size_t>(bytes_), PROT_READ, MAP_PRIVATE,
          descriptor_, 0));
        if (data_ == MAP_FAILED) {
            data_ = nullptr;
            external_system_error("cannot mmap", path_);
        }
        if (bytes_ < sizeof(SidecarHeader))
            throw std::runtime_error("truncated reciprocal arbitrary sidecar");
        std::memcpy(&header_, data_, sizeof(header_));
        validate(bindings, restore);
    }

    ~Impl() {
        if (data_) ::munmap(const_cast<std::uint8_t*>(data_), bytes_);
        if (descriptor_ >= 0) ::close(descriptor_);
    }

    [[nodiscard]] bool query(const GhostPublicExtra::ConcreteState& state,
                             GhostPublicExtra::GhostMask belief,
                             bool owner) const {
        const auto [geometry, actual] = canonical_query(state, belief);
        const auto [geometryId, transform] = domain_.locate(geometry);
        if (transform != 0 || geometryId >= header_.geometries)
            throw std::runtime_error("reciprocal query geometry residual");
        if (!belief.count() || !belief.test(actual))
            throw std::invalid_argument(
              "arbitrary belief must be nonempty and contain actual");
        if (geometry.visible && belief.count() != 1)
            throw std::invalid_argument(
              "visible Ghost query must be a singleton");
        const ExternalGeometryMeta& meta = geometries()[geometryId];
        const std::uint64_t ownerIndex =
          std::uint64_t(geometryId) * Variables + actual;
        if (external_mask_test(meta.terminal, actual)) {
            const bool actualOwner = external_mask_test(
              meta.terminalOwner, actual);
            const bool actualObserver = external_mask_test(
              meta.terminalObserver, actual);
            for (unsigned candidate = 0; candidate < Variables; ++candidate)
                if (belief.test(candidate) &&
                    (!external_mask_test(meta.terminal, candidate) ||
                     external_mask_test(meta.terminalOwner, candidate) !=
                       actualOwner ||
                     external_mask_test(meta.terminalObserver, candidate) !=
                       actualObserver))
                    throw std::invalid_argument(
                      "terminal belief spans public outcome observations");
            return external_mask_test(owner ? meta.terminalOwner
                                             : meta.terminalObserver, actual);
        }
        if (!external_mask_test(meta.live, actual))
            throw std::invalid_argument("arbitrary query actual is not live");
        if (geometry.visible) {
            return (owner ? visible_owner() : visible_observer())[ownerIndex];
        }
        const std::uint32_t stratum = meta.actualStratum[actual];
        if (stratum == NoIndex || stratum >= header_.strata)
            throw std::runtime_error("arbitrary query has no decision cell");
        const ExternalMask& allowed = strata()[stratum];
        if ((belief.low & ~allowed.low) || (belief.high & ~allowed.high))
            throw std::invalid_argument(
              "arbitrary belief spans legal-dot decision cells");
        const ExternalRobdd::Id root = owner
          ? owner_roots()[ownerIndex] : observer_roots()[stratum];
        return evaluate(root, belief);
    }

    [[nodiscard]] const SolveCertificate& certificate() const {
        return certificate_;
    }

  private:
    template<typename Value>
    [[nodiscard]] const Value* at(std::uint64_t offset,
                                  std::uint64_t count) const {
        if (offset > bytes_ || count > (bytes_ - offset) / sizeof(Value))
            throw std::runtime_error("reciprocal sidecar section overflow");
        return reinterpret_cast<const Value*>(data_ + offset);
    }

    [[nodiscard]] const NodeDisk* nodes() const {
        return at<NodeDisk>(header_.nodeOffset, header_.nodes);
    }
    [[nodiscard]] const ExternalGeometryMeta* geometries() const {
        return at<ExternalGeometryMeta>(header_.geometryOffset,
                                        header_.geometries);
    }
    [[nodiscard]] const ExternalMask* strata() const {
        return at<ExternalMask>(header_.stratumOffset, header_.strata);
    }
    [[nodiscard]] const ExternalRobdd::Id* owner_roots() const {
        return at<ExternalRobdd::Id>(header_.ownerOffset, header_.ownerRoots);
    }
    [[nodiscard]] const ExternalRobdd::Id* observer_roots() const {
        return at<ExternalRobdd::Id>(header_.observerOffset, header_.strata);
    }
    [[nodiscard]] const std::uint8_t* visible_owner() const {
        return at<std::uint8_t>(header_.visibleOwnerOffset,
                                header_.ownerRoots);
    }
    [[nodiscard]] const std::uint8_t* visible_observer() const {
        return at<std::uint8_t>(header_.visibleObserverOffset,
                                header_.ownerRoots);
    }

    [[nodiscard]] bool evaluate(ExternalRobdd::Id root,
                                const GhostPublicExtra::GhostMask& mask) const {
        while (root > ExternalRobdd::True) {
            if (root >= header_.nodes)
                throw std::runtime_error("reciprocal sidecar root is invalid");
            const NodeDisk& node = nodes()[root];
            root = mask.test(node.variable) ? node.high : node.low;
        }
        return root == ExternalRobdd::True;
    }

    void validate(const ProbeBindings& bindings,
                  const SolveOptions* restore) {
        const std::uint64_t expectedNodeOffset = sizeof(header_);
        const std::uint64_t expectedGeometryOffset = expectedNodeOffset +
          header_.nodes * sizeof(NodeDisk);
        const std::uint64_t expectedStratumOffset = expectedGeometryOffset +
          header_.geometries * sizeof(ExternalGeometryMeta);
        const std::uint64_t expectedOwnerOffset = expectedStratumOffset +
          header_.strata * sizeof(ExternalMask);
        const std::uint64_t expectedObserverOffset = expectedOwnerOffset +
          header_.ownerRoots * sizeof(ExternalRobdd::Id);
        const std::uint64_t expectedVisibleOwnerOffset =
          expectedObserverOffset + header_.strata * sizeof(ExternalRobdd::Id);
        const std::uint64_t expectedVisibleObserverOffset =
          expectedVisibleOwnerOffset + header_.ownerRoots;
        const std::uint64_t expectedExtent = header_.visibleObserverOffset +
                                             header_.ownerRoots;
        if (header_.magic !=
              std::array<char, 8>{{'U','F','G','X','2','\0','\0','\0'}} ||
            header_.version != SidecarVersion ||
            header_.headerBytes != sizeof(header_) ||
            header_.endian != Endian ||
            header_.primary != static_cast<std::uint32_t>(PieceType::Bishop) ||
            header_.secondary != static_cast<std::uint32_t>(PieceType::Ghost) ||
            header_.ghostColor != static_cast<std::uint32_t>(Color::Black) ||
            header_.squares != Variables ||
            header_.stateCount != GhostPublicExtra::StateCount ||
            header_.nodeBytes != sizeof(NodeDisk) ||
            header_.geometryBytes != sizeof(ExternalGeometryMeta) ||
            header_.maskBytes != sizeof(ExternalMask) ||
            header_.rootBytes != sizeof(ExternalRobdd::Id) ||
            header_.reserved || header_.nodes < 2 ||
            header_.nodes > std::numeric_limits<std::uint32_t>::max() ||
            header_.geometries != 492'960 ||
            header_.ownerRoots != header_.geometries * Variables ||
            header_.nodeOffset != expectedNodeOffset ||
            header_.geometryOffset != expectedGeometryOffset ||
            header_.stratumOffset != expectedStratumOffset ||
            header_.ownerOffset != expectedOwnerOffset ||
            header_.observerOffset != expectedObserverOffset ||
            header_.visibleOwnerOffset != expectedVisibleOwnerOffset ||
            header_.visibleObserverOffset != expectedVisibleObserverOffset ||
            expectedExtent != bytes_ ||
            header_.payloadBytes != bytes_ - sizeof(header_) ||
            std::string(header_.sourceSha.data(), 64) !=
              bindings.sourceSha256 ||
            std::string(header_.modelSha.data(), 64) !=
              bindings.modelSha256 ||
            std::string(header_.observationSha.data(), 64) !=
              bindings.observationSha256 ||
            std::string(header_.lowerGhostSha.data(), 64) !=
              bindings.lowerGhostSidecarSha256 ||
            std::string(header_.semantics.data(), std::strlen(Semantics)) !=
              Semantics ||
            sha256_file(path_, sizeof(header_)) !=
              std::string(header_.payloadSha.data(), 64))
            throw std::runtime_error(
              "reciprocal arbitrary sidecar header/binding residual");
        const std::string fullSha = sha256_file(path_);
        if (!restore && !valid_sha256(bindings.arbitrarySidecarSha256))
            throw std::invalid_argument(
              "probe-only reciprocal load requires the full UFGX2 SHA-256");
        if (!bindings.arbitrarySidecarSha256.empty() &&
            fullSha != bindings.arbitrarySidecarSha256)
            throw std::runtime_error(
              "reciprocal arbitrary full-file SHA mismatch");
        if (restore) {
            if (restore->sourceTable.empty() ||
                sha256_file(restore->sourceTable) != bindings.sourceSha256)
                throw std::runtime_error(
                  "reciprocal arbitrary source-table SHA mismatch");
            if (restore->lowerGhostSidecar.empty() ||
                sha256_file(restore->lowerGhostSidecar) !=
                  bindings.lowerGhostSidecarSha256)
                throw std::runtime_error(
                  "reciprocal arbitrary lower-UFGM SHA mismatch");
            std::size_t component = 0;
            for (const char* suffix : {".header", ".meta", ".strata",
                                       ".index", ".blocks", ".verified"}) {
                if (sha256_file(restore->transitionPrefix + suffix) !=
                    std::string(header_.transitionSha[component++].data(), 64))
                    throw std::runtime_error(
                      "reciprocal transition provenance mismatch");
            }
            if (transition_payload_sha(restore->transitionPrefix) !=
                std::string(header_.transitionPayloadSha.data(), 64))
                throw std::runtime_error(
                  "reciprocal transition combined provenance mismatch");
        }
        for (std::uint64_t id = 0; id < header_.nodes; ++id) {
            const NodeDisk& node = nodes()[id];
            if (id <= 1) {
                if (node.variable != Variables || node.low != id ||
                    node.high != id)
                    throw std::runtime_error(
                      "reciprocal sidecar terminal tuple residual");
            }
            else if (node.variable >= Variables || node.low >= id ||
                     node.high >= id || node.low == node.high ||
                     (node.low > 1 && nodes()[node.low].variable <=
                                      node.variable) ||
                     (node.high > 1 && nodes()[node.high].variable <=
                                       node.variable))
                throw std::runtime_error(
                  "reciprocal sidecar ROBDD structural residual");
        }
        if (restore)
            validate_unique_node_tuples(nodes(), header_.nodes);
        for (std::uint64_t id = 0; id < header_.ownerRoots; ++id)
            if (owner_roots()[id] >= header_.nodes ||
                visible_owner()[id] > 1 || visible_observer()[id] > 1)
                throw std::runtime_error(
                  "reciprocal sidecar owner-root residual");
        for (std::uint64_t id = 0; id < header_.strata; ++id)
            if (observer_roots()[id] >= header_.nodes)
                throw std::runtime_error(
                  "reciprocal sidecar observer-root residual");
        certificate_.transitionHeaderSha256 =
          std::string(header_.transitionSha[0].data(), 64);
        certificate_.transitionVerifiedSha256 =
          std::string(header_.transitionSha[5].data(), 64);
        certificate_.transitionPayloadSha256 =
          std::string(header_.transitionPayloadSha.data(), 64);
        certificate_.arbitrarySha256 = fullSha;
    }

    std::string path_;
    int descriptor_ = -1;
    const std::uint8_t* data_ = nullptr;
    std::uint64_t bytes_ = 0;
    SidecarHeader header_{};
    ExtraGeometryDomain domain_;
    SolveCertificate certificate_;
};

ArbitrarySidecarProbe::ArbitrarySidecarProbe(
  const std::string& path, const SolveOptions& bindings)
  : impl_(std::make_unique<Impl>(path, bindings)) {}
ArbitrarySidecarProbe::ArbitrarySidecarProbe(
  const std::string& path, const ProbeBindings& bindings)
  : impl_(std::make_unique<Impl>(path, bindings)) {}
ArbitrarySidecarProbe::~ArbitrarySidecarProbe() = default;
ArbitrarySidecarProbe::ArbitrarySidecarProbe(
  ArbitrarySidecarProbe&&) noexcept = default;
ArbitrarySidecarProbe& ArbitrarySidecarProbe::operator=(
  ArbitrarySidecarProbe&&) noexcept = default;

bool ArbitrarySidecarProbe::owner_forces(
  const GhostPublicExtra::ConcreteState& actual,
  const GhostPublicExtra::GhostMask& belief) const {
    return impl_->query(actual, belief, true);
}

bool ArbitrarySidecarProbe::observer_forces(
  const GhostPublicExtra::ConcreteState& actual,
  const GhostPublicExtra::GhostMask& belief) const {
    return impl_->query(actual, belief, false);
}

const SolveCertificate& ArbitrarySidecarProbe::certificate() const {
    return impl_->certificate();
}

}  // namespace Stockfish::Ultimate::GhostPublicExtraExact
