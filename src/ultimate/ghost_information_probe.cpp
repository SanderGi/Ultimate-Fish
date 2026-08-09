/*
  Ultimate Fish - exact arbitrary-belief K+Ghost-v-K information probe
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#include "ghost_information_probe.h"

#include "position.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <unordered_set>

namespace Stockfish::Ultimate {
namespace {

constexpr std::uint32_t HeaderBytes = 320;
constexpr std::uint32_t NodeBytes = 9;
constexpr std::uint32_t GeometryBytes = 844;
constexpr std::uint32_t StratumBytes = 18;

[[nodiscard]] std::uint16_t read_u16(std::istream& input) {
    std::array<unsigned char, 2> bytes{};
    input.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    if (!input)
        throw std::runtime_error("truncated Ghost information sidecar");
    return std::uint16_t(bytes[0]) | (std::uint16_t(bytes[1]) << 8);
}

[[nodiscard]] std::uint32_t read_u32(std::istream& input) {
    std::array<unsigned char, 4> bytes{};
    input.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    if (!input)
        throw std::runtime_error("truncated Ghost information sidecar");
    return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8) |
           (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
}

[[nodiscard]] std::uint64_t read_u64(std::istream& input) {
    const std::uint64_t low = read_u32(input);
    return low | (std::uint64_t(read_u32(input)) << 32);
}

[[nodiscard]] GhostInformationMask read_mask(std::istream& input) {
    return {read_u64(input), read_u16(input)};
}

[[nodiscard]] bool test(GhostInformationMask mask, unsigned square) {
    return square < 64 ? (mask.low >> square) & 1u
                       : (mask.high >> (square - 64)) & 1u;
}

[[nodiscard]] bool empty(GhostInformationMask mask) {
    return !mask.low && !mask.high;
}

[[nodiscard]] bool subset(GhostInformationMask lhs,
                          GhostInformationMask rhs) {
    return !(lhs.low & ~rhs.low) &&
           !(lhs.high & static_cast<std::uint16_t>(~rhs.high));
}

[[nodiscard]] GhostInformationMask mask_and(GhostInformationMask lhs,
                                             GhostInformationMask rhs) {
    return {lhs.low & rhs.low,
            static_cast<std::uint16_t>(lhs.high & rhs.high)};
}

[[nodiscard]] GhostInformationMask mask_or(GhostInformationMask lhs,
                                            GhostInformationMask rhs) {
    return {lhs.low | rhs.low,
            static_cast<std::uint16_t>(lhs.high | rhs.high)};
}

[[nodiscard]] int transform_square(int square, std::uint8_t transform) {
    int file = square % Position::BoardFiles;
    int rank = square / Position::BoardFiles;
    if (transform & 1)
        file = Position::BoardFiles - 1 - file;
    if (transform & 2)
        rank = Position::BoardRanks - 1 - rank;
    return rank * Position::BoardFiles + file;
}

[[nodiscard]] GhostInformationMask transform_mask(
  GhostInformationMask source, std::uint8_t transform) {
    GhostInformationMask result;
    for (unsigned square = 0; square < GhostInformationProbe::Squares; ++square)
        if (test(source, square)) {
            const unsigned target = transform_square(square, transform);
            if (target < 64)
                result.low |= std::uint64_t(1) << target;
            else
                result.high |= std::uint16_t(1) << (target - 64);
        }
    return result;
}

[[nodiscard]] std::uint32_t geometry_code(std::uint8_t side,
                                          std::uint8_t ownerKing,
                                          std::uint8_t observerKing,
                                          bool visible) {
    return side | (std::uint32_t(ownerKing) << 1) |
           (std::uint32_t(observerKing) << 8) |
           (std::uint32_t(visible) << 15);
}

[[nodiscard]] bool valid_sha256(const std::string& text) {
    return text.size() == 64 &&
           std::all_of(text.begin(), text.end(), [](char character) {
               return (character >= '0' && character <= '9') ||
                      (character >= 'a' && character <= 'f');
           });
}

struct NodeKey {
    std::uint8_t variable;
    std::uint32_t low;
    std::uint32_t high;
    friend bool operator==(const NodeKey& lhs, const NodeKey& rhs) {
        return lhs.variable == rhs.variable && lhs.low == rhs.low &&
               lhs.high == rhs.high;
    }
};

struct NodeKeyHash {
    std::size_t operator()(const NodeKey& key) const noexcept {
        std::uint64_t hash = key.variable + 0x9e3779b97f4a7c15ULL;
        hash ^= std::uint64_t(key.low) + (hash << 6) + (hash >> 2);
        hash ^= std::uint64_t(key.high) + (hash << 6) + (hash >> 2);
        return static_cast<std::size_t>(hash);
    }
};

}  // namespace

GhostInformationProbe::GhostInformationProbe(
  const std::string& path, const std::string& expectedConcreteSha256,
  const std::string& expectedModelSha256,
  const std::string& expectedObservationSha256) {
    for (const auto& [label, digest] :
         std::array<std::pair<const char*, const std::string*>, 3>{{
           {"concrete", &expectedConcreteSha256},
           {"model", &expectedModelSha256},
           {"observation", &expectedObservationSha256}}})
        if (!valid_sha256(*digest))
            throw std::runtime_error(std::string("invalid expected ") + label +
                                     " SHA-256");
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("cannot open Ghost information sidecar: " + path);
    std::array<char, 8> magic{};
    input.read(magic.data(), magic.size());
    if (!input || std::memcmp(magic.data(), "UFGM1\0\0\0", 8))
        throw std::runtime_error("invalid Ghost information sidecar magic");
    const std::uint32_t version = read_u32(input);
    const std::uint32_t headerBytes = read_u32(input);
    const std::uint32_t piece = read_u32(input);
    const std::uint32_t ownerColor = read_u32(input);
    const std::uint32_t files = read_u32(input);
    const std::uint32_t ranks = read_u32(input);
    const std::uint32_t squares = read_u32(input);
    const std::uint32_t concreteCount = read_u32(input);
    const std::uint32_t substates = read_u32(input);
    const std::uint32_t geometryCount = read_u32(input);
    const std::uint32_t stratumCount = read_u32(input);
    const std::uint32_t nodeCount = read_u32(input);
    const std::uint32_t nodeBytes = read_u32(input);
    const std::uint32_t geometryBytes = read_u32(input);
    const std::uint32_t stratumBytes = read_u32(input);
    (void)read_u32(input);
    const std::uint64_t nodeOffset = read_u64(input);
    const std::uint64_t geometryOffset = read_u64(input);
    const std::uint64_t stratumOffset = read_u64(input);
    std::array<char, 64> digest{};
    input.read(digest.data(), digest.size());
    concreteSha256_.assign(digest.data(), digest.size());
    input.read(digest.data(), digest.size());
    modelSha256_.assign(digest.data(), digest.size());
    input.read(digest.data(), digest.size());
    observationSha256_.assign(digest.data(), digest.size());
    std::array<char, 32> semantics{};
    input.read(semantics.data(), semantics.size());
    if (!input || version != 1 || headerBytes != HeaderBytes ||
        piece != static_cast<std::uint32_t>(PieceType::Ghost) ||
        ownerColor != static_cast<std::uint32_t>(Color::White) ||
        files != Position::BoardFiles ||
        ranks != Position::BoardRanks || squares != Squares ||
        concreteCount != 1'971'840 || substates != 2 ||
        nodeBytes != NodeBytes || geometryBytes != GeometryBytes ||
        stratumBytes != StratumBytes || nodeCount < 2 ||
        std::string(semantics.data()) != "history-mask-public-view-v2" ||
        concreteSha256_ != expectedConcreteSha256 ||
        modelSha256_ != expectedModelSha256 ||
        observationSha256_ != expectedObservationSha256)
        throw std::runtime_error(
          "Ghost information sidecar metadata/hash mismatch");
    const std::uint64_t expectedGeometryOffset =
      nodeOffset + std::uint64_t(nodeCount) * NodeBytes;
    const std::uint64_t expectedStratumOffset =
      geometryOffset + std::uint64_t(geometryCount) * GeometryBytes;
    if (nodeOffset != HeaderBytes || geometryOffset != expectedGeometryOffset ||
        stratumOffset != expectedStratumOffset)
        throw std::runtime_error("Ghost information sidecar section mismatch");

    input.seekg(static_cast<std::streamoff>(nodeOffset));
    nodes_.resize(nodeCount);
    std::unordered_set<NodeKey, NodeKeyHash> unique;
    unique.reserve(nodeCount);
    for (std::uint32_t id = 0; id < nodeCount; ++id) {
        Node& node = nodes_[id];
        node.variable = static_cast<std::uint8_t>(input.get());
        node.low = read_u32(input);
        node.high = read_u32(input);
        if (id < 2) {
            if (node.variable != Squares || node.low != id || node.high != id)
                throw std::runtime_error("invalid ROBDD terminal node");
        }
        else {
            if (node.variable >= Squares || node.low >= id || node.high >= id ||
                node.low == node.high)
                throw std::runtime_error("invalid reduced ROBDD node");
            const auto childVariable = [&](std::uint32_t child) {
                return child < 2 ? Squares : nodes_[child].variable;
            };
            if (childVariable(node.low) <= node.variable ||
                childVariable(node.high) <= node.variable ||
                !unique.emplace(NodeKey{node.variable, node.low, node.high}).second)
                throw std::runtime_error(
                  "unordered or duplicate ROBDD node in sidecar");
        }
    }

    input.seekg(static_cast<std::streamoff>(geometryOffset));
    geometries_.resize(geometryCount);
    geometryIndex_.reserve(geometryCount);
    for (std::uint32_t id = 0; id < geometryCount; ++id) {
        Geometry& geometry = geometries_[id];
        geometry.side = static_cast<std::uint8_t>(input.get());
        geometry.ownerKing = static_cast<std::uint8_t>(input.get());
        geometry.observerKing = static_cast<std::uint8_t>(input.get());
        geometry.visible = static_cast<std::uint8_t>(input.get());
        geometry.live = read_mask(input);
        geometry.terminal = read_mask(input);
        geometry.terminalOwner = read_mask(input);
        geometry.terminalObserver = read_mask(input);
        for (std::uint32_t& stratum : geometry.actualStratum)
            stratum = read_u32(input);
        for (std::uint32_t& root : geometry.ownerRoot) {
            root = read_u32(input);
            if (root >= nodeCount)
                throw std::runtime_error("owner ROBDD root is out of range");
        }
        input.read(reinterpret_cast<char*>(geometry.visibleOwner.data()),
                   geometry.visibleOwner.size());
        input.read(reinterpret_cast<char*>(geometry.visibleObserver.data()),
                   geometry.visibleObserver.size());
        const std::uint32_t code = geometry_code(
          geometry.side, geometry.ownerKing, geometry.observerKing,
          geometry.visible != 0);
        geometryIndex_.emplace_back(code, id);
        if (geometry.side > 1 || geometry.ownerKing >= Squares ||
            geometry.observerKing >= Squares ||
            geometry.ownerKing == geometry.observerKing ||
            geometry.visible > 1 ||
            !subset(geometry.terminalOwner, geometry.terminal) ||
            !subset(geometry.terminalObserver, geometry.terminal) ||
            !empty(mask_and(geometry.live, geometry.terminal)) ||
            !empty(mask_and(geometry.terminalOwner,
                            geometry.terminalObserver)) ||
            std::any_of(geometry.visibleOwner.begin(),
                        geometry.visibleOwner.end(),
                        [](std::uint8_t value) { return value > 1; }) ||
            std::any_of(geometry.visibleObserver.begin(),
                        geometry.visibleObserver.end(),
                        [](std::uint8_t value) { return value > 1; }))
            throw std::runtime_error(
              "inconsistent geometry force/domain masks in sidecar");
    }
    std::sort(geometryIndex_.begin(), geometryIndex_.end());
    if (std::adjacent_find(geometryIndex_.begin(), geometryIndex_.end(),
          [](const auto& lhs, const auto& rhs) { return lhs.first == rhs.first; }) !=
        geometryIndex_.end())
        throw std::runtime_error("duplicate public geometry in sidecar");

    input.seekg(static_cast<std::streamoff>(stratumOffset));
    strata_.resize(stratumCount);
    for (Stratum& stratum : strata_) {
        stratum.geometry = read_u32(input);
        stratum.live = read_mask(input);
        stratum.observerRoot = read_u32(input);
        if (stratum.geometry >= geometryCount || stratum.observerRoot >= nodeCount)
            throw std::runtime_error("invalid stratum record in sidecar");
    }
    for (std::uint32_t geometry = 0; geometry < geometryCount; ++geometry)
        for (unsigned actual = 0; actual < Squares; ++actual) {
            const std::uint32_t stratum =
              geometries_[geometry].actualStratum[actual];
            const bool live = test(geometries_[geometry].live, actual);
            const bool terminal = test(geometries_[geometry].terminal, actual);
            if (actual == geometries_[geometry].ownerKing ||
                actual == geometries_[geometry].observerKing) {
                if (live || terminal || stratum != NoStratum)
                    throw std::runtime_error(
                      "occupied King square appears in Ghost domain");
                continue;
            }
            if (live && !geometries_[geometry].visible &&
                stratum == NoStratum)
                throw std::runtime_error(
                  "live hidden Ghost world has no decision stratum");
            if ((!live || geometries_[geometry].visible) &&
                stratum != NoStratum)
                throw std::runtime_error(
                  "non-hidden Ghost world has a decision stratum");
            if (stratum != NoStratum &&
                (stratum >= strata_.size() ||
                 strata_[stratum].geometry != geometry ||
                 !test(strata_[stratum].live, actual)))
                throw std::runtime_error(
                  "geometry actual-to-stratum map is inconsistent");
        }
    for (std::uint32_t stratum = 0; stratum < strata_.size(); ++stratum) {
        const Stratum& value = strata_[stratum];
        if (empty(value.live) ||
            !subset(value.live, geometries_[value.geometry].live))
            throw std::runtime_error("invalid decision-stratum domain mask");
        for (unsigned actual = 0; actual < Squares; ++actual)
            if (test(value.live, actual) &&
                geometries_[value.geometry].actualStratum[actual] != stratum)
                throw std::runtime_error(
                  "decision stratum reverse map is inconsistent");
    }
    input.peek();
    if (!input.eof())
        throw std::runtime_error("Ghost information sidecar has trailing bytes");
}

bool GhostInformationProbe::evaluate(
  std::uint32_t root, GhostInformationMask assignment) const {
    while (root > 1) {
        const Node& node = nodes_.at(root);
        root = test(assignment, node.variable) ? node.high : node.low;
    }
    return root == 1;
}

std::uint32_t GhostInformationProbe::find_geometry(
  std::uint8_t side, std::uint8_t ownerKing, std::uint8_t observerKing,
  bool visible, std::uint8_t& transform) const {
    std::tuple<std::uint8_t, std::uint8_t, std::uint8_t, bool> best{
      side, ownerKing, observerKing, visible};
    transform = 0;
    for (std::uint8_t candidate = 1; candidate < 4; ++candidate) {
        const auto key = std::tuple<std::uint8_t, std::uint8_t,
          std::uint8_t, bool>{side,
          static_cast<std::uint8_t>(transform_square(ownerKing, candidate)),
          static_cast<std::uint8_t>(transform_square(observerKing, candidate)),
          visible};
        if (key < best) {
            best = key;
            transform = candidate;
        }
    }
    const std::uint32_t code = geometry_code(
      std::get<0>(best), std::get<1>(best), std::get<2>(best),
      std::get<3>(best));
    const auto found = std::lower_bound(geometryIndex_.begin(),
      geometryIndex_.end(), std::pair<std::uint32_t, std::uint32_t>{code, 0});
    if (found == geometryIndex_.end() || found->first != code)
        throw std::runtime_error("public geometry is absent from sidecar");
    return found->second;
}

GhostInformationProbeResult GhostInformationProbe::probe(
  std::uint8_t side, std::uint8_t ownerKing, std::uint8_t observerKing,
  std::uint8_t ghost, bool visible, GhostInformationMask belief) const {
    if (side > 1 || ownerKing >= Squares || observerKing >= Squares ||
        ghost >= Squares || ownerKing == observerKing || ghost == ownerKing ||
        ghost == observerKing || empty(belief) || !test(belief, ghost))
        throw std::runtime_error("invalid arbitrary-mask Ghost probe");
    std::uint8_t transform = 0;
    const std::uint32_t geometryId = find_geometry(
      side, ownerKing, observerKing, visible, transform);
    const unsigned actual = transform_square(ghost, transform);
    belief = transform_mask(belief, transform);
    const Geometry& geometry = geometries_.at(geometryId);
    GhostInformationProbeResult result;
    result.geometry = geometryId;
    if (test(geometry.terminal, actual)) {
        GhostInformationMask outcome;
        if (test(geometry.terminalOwner, actual))
            outcome = geometry.terminalOwner;
        else if (test(geometry.terminalObserver, actual))
            outcome = geometry.terminalObserver;
        else {
            const GhostInformationMask decisive = mask_or(
              geometry.terminalOwner, geometry.terminalObserver);
            outcome = {geometry.terminal.low & ~decisive.low,
              static_cast<std::uint16_t>(geometry.terminal.high &
                                         ~decisive.high)};
        }
        if (!subset(belief, outcome))
            throw std::runtime_error(
              "terminal Ghost belief mixes public outcome observations");
        result.terminal = true;
        result.ownerForce = test(geometry.terminalOwner, actual);
        result.observerForce = test(geometry.terminalObserver, actual);
        return result;
    }
    if (visible) {
        GhostInformationMask singleton;
        if (actual < 64)
            singleton.low = std::uint64_t(1) << actual;
        else
            singleton.high = std::uint16_t(1) << (actual - 64);
        if (belief.low != singleton.low || belief.high != singleton.high)
            throw std::runtime_error(
              "visible Ghost probe requires the public singleton belief");
        result.ownerForce = geometry.visibleOwner[actual] != 0;
        result.observerForce = geometry.visibleObserver[actual] != 0;
        return result;
    }
    const std::uint32_t stratumId = geometry.actualStratum[actual];
    if (stratumId == NoStratum)
        throw std::runtime_error("live hidden Ghost actual has no dot stratum");
    const Stratum& stratum = strata_.at(stratumId);
    if (!subset(belief, stratum.live))
        throw std::runtime_error(
          "hidden Ghost belief crosses a mover-private dot observation");
    result.stratum = stratumId;
    result.ownerForce = evaluate(geometry.ownerRoot[actual], belief);
    result.observerForce = evaluate(stratum.observerRoot, belief);
    if (result.ownerForce && result.observerForce)
        throw std::runtime_error(
          "arbitrary Ghost belief gives both players a forced win");
    return result;
}

std::uint32_t GhostInformationProbe::node_count() const {
    return static_cast<std::uint32_t>(nodes_.size());
}
std::uint32_t GhostInformationProbe::geometry_count() const {
    return static_cast<std::uint32_t>(geometries_.size());
}
std::uint32_t GhostInformationProbe::stratum_count() const {
    return static_cast<std::uint32_t>(strata_.size());
}
const std::string& GhostInformationProbe::concrete_sha256() const {
    return concreteSha256_;
}
const std::string& GhostInformationProbe::model_sha256() const {
    return modelSha256_;
}
const std::string& GhostInformationProbe::observation_sha256() const {
    return observationSha256_;
}

}  // namespace Stockfish::Ultimate
