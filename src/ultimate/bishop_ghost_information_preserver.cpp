/*
  Ultimate Fish - post-proof Bishop/Ghost arbitrary-belief preservation
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.

  This utility is intentionally outside the d597 solver-model source list.  It
  never resumes or mutates a live solve.  It reads a restored, quiescent copy
  of a completed same-side K+Bishop+Ghost-v-K proof, authenticates the final
  zero-Bellman certificate and active compaction arena, and emits a compact,
  portable-little-endian UFGX2 sidecar containing every arbitrary-belief force
  predicate.  Strict restore validates the sidecar without trusting hashes
  alone; --full-bellman additionally imports it into a fresh solver scratch and
  independently regenerates the complete Bellman/singleton/fresh-root proof.
*/

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#define main ultimate_frozen_bishop_ghost_preserver_embedded_main
#include "ghost_extra_information_tablebase.cpp"
#undef main
#undef private
#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace Stockfish::Ultimate::BishopGhostPreservation {
namespace {

constexpr std::uint32_t Variables = Position::BoardSquares;
constexpr std::uint32_t FullGeometries = 492'960;
constexpr std::uint32_t PortableHeaderBytes = 988;
constexpr std::uint32_t PortableMetaBytes = 392;
constexpr std::uint32_t PortableNodeBytes = 9;
constexpr std::uint32_t PortableMaskBytes = 16;
constexpr std::uint32_t PortableRootBytes = 4;
constexpr std::uint32_t EndianMarker = 0x01020304;
constexpr char Semantics[] =
  "fresh-maximal-public-view-v2:same-side-bishop-ghost-preserved";
constexpr char ManifestSchema[] =
  "ultimatefish-bishop-ghost-preservation-v1";

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error("Bishop/Ghost preservation: " + message);
}

[[nodiscard]] bool valid_sha(const std::string& value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(),
      [](unsigned char c) { return std::isxdigit(c); });
}

[[nodiscard]] std::uint64_t file_bytes(const std::string& path) {
    struct stat status{};
    if (::stat(path.c_str(), &status) || status.st_size < 0)
        fail("cannot stat " + path);
    return static_cast<std::uint64_t>(status.st_size);
}

[[nodiscard]] std::string sha_file(const std::string& path,
                                   std::uint64_t offset = 0,
                                   std::optional<std::uint64_t> limit = {}) {
    std::ifstream input(path, std::ios::binary);
    if (!input) fail("cannot hash " + path);
    input.seekg(static_cast<std::streamoff>(offset));
    if (!input) fail("cannot seek while hashing " + path);
    Sha256 hash;
    std::array<std::uint8_t, 1 << 20> buffer{};
    std::uint64_t remaining = limit.value_or(
      std::numeric_limits<std::uint64_t>::max());
    while (remaining && input) {
        const std::size_t request = static_cast<std::size_t>(
          std::min<std::uint64_t>(remaining, buffer.size()));
        input.read(reinterpret_cast<char*>(buffer.data()), request);
        const std::size_t got = static_cast<std::size_t>(input.gcount());
        if (got) {
            hash.update(buffer.data(), got);
            remaining -= got;
        }
    }
    if (limit && remaining)
        fail("truncated while hashing " + path);
    return hex_digest(hash.finish());
}

class ReadMapping {
  public:
    explicit ReadMapping(const std::string& path) : path_(path) {
        descriptor_ = ::open(path.c_str(), O_RDONLY);
        if (descriptor_ < 0) fail("cannot open " + path);
        bytes_ = file_bytes(path);
        if (!bytes_) fail("empty file " + path);
        data_ = static_cast<const std::uint8_t*>(::mmap(nullptr, bytes_,
          PROT_READ, MAP_PRIVATE, descriptor_, 0));
        if (data_ == MAP_FAILED) {
            data_ = nullptr;
            fail("cannot map " + path);
        }
    }
    ~ReadMapping() {
        if (data_) ::munmap(const_cast<std::uint8_t*>(data_), bytes_);
        if (descriptor_ >= 0) ::close(descriptor_);
    }
    ReadMapping(const ReadMapping&) = delete;
    ReadMapping& operator=(const ReadMapping&) = delete;
    [[nodiscard]] const std::uint8_t* data() const { return data_; }
    [[nodiscard]] std::uint64_t size() const { return bytes_; }
    [[nodiscard]] const std::string& path() const { return path_; }
  private:
    std::string path_;
    int descriptor_ = -1;
    const std::uint8_t* data_ = nullptr;
    std::uint64_t bytes_ = 0;
};

class WriteMapping {
  public:
    WriteMapping(const std::string& path, std::uint64_t bytes) : path_(path),
      bytes_(bytes) {
        descriptor_ = ::open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0600);
        if (descriptor_ < 0 || ::ftruncate(descriptor_, bytes_))
            fail("cannot create mapping " + path);
        data_ = static_cast<std::uint8_t*>(::mmap(nullptr, bytes_,
          PROT_READ | PROT_WRITE, MAP_SHARED, descriptor_, 0));
        if (data_ == MAP_FAILED) {
            data_ = nullptr;
            fail("cannot map output " + path);
        }
    }
    ~WriteMapping() {
        if (data_) ::munmap(data_, bytes_);
        if (descriptor_ >= 0) ::close(descriptor_);
    }
    [[nodiscard]] std::uint8_t* data() { return data_; }
    void flush() {
        if (::msync(data_, bytes_, MS_SYNC)) fail("cannot flush " + path_);
    }
  private:
    std::string path_;
    int descriptor_ = -1;
    std::uint8_t* data_ = nullptr;
    std::uint64_t bytes_ = 0;
};

[[nodiscard]] std::uint32_t le32(const std::uint8_t* source) {
    return std::uint32_t(source[0]) | (std::uint32_t(source[1]) << 8) |
           (std::uint32_t(source[2]) << 16) |
           (std::uint32_t(source[3]) << 24);
}

[[nodiscard]] std::uint64_t le64(const std::uint8_t* source) {
    std::uint64_t value = 0;
    for (unsigned byte = 0; byte < 8; ++byte)
        value |= std::uint64_t(source[byte]) << (8 * byte);
    return value;
}

void put32(std::uint8_t* target, std::uint32_t value) {
    for (unsigned byte = 0; byte < 4; ++byte)
        target[byte] = static_cast<std::uint8_t>(value >> (8 * byte));
}

void put64(std::uint8_t* target, std::uint64_t value) {
    for (unsigned byte = 0; byte < 8; ++byte)
        target[byte] = static_cast<std::uint8_t>(value >> (8 * byte));
}

void write32(std::ostream& output, std::uint32_t value) {
    std::array<std::uint8_t, 4> bytes{};
    put32(bytes.data(), value);
    output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

void write64(std::ostream& output, std::uint64_t value) {
    std::array<std::uint8_t, 8> bytes{};
    put64(bytes.data(), value);
    output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

struct DiskNode {
    std::uint8_t variable = Variables;
    std::uint32_t low = 0;
    std::uint32_t high = 0;
};

[[nodiscard]] DiskNode read_node(const std::uint8_t* source) {
    return {source[0], le32(source + 1), le32(source + 5)};
}

void write_node(std::ostream& output, const DiskNode& node) {
    output.put(static_cast<char>(node.variable));
    write32(output, node.low);
    write32(output, node.high);
}

[[nodiscard]] std::uint64_t mix64(std::uint64_t value) {
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

struct ProofLog {
    std::uint32_t iteration = 0;
    std::uint32_t compactions = 0;
    std::uint32_t arenaNodes = 0;
    char arena = 'a';
    std::string certificateLine;
    std::array<std::string, 2> summaryLines{};
    std::string conservationLine;
    std::uint64_t overlayBytes = 0;
    std::string overlaySourceSha;
    std::string overlayModelSha;
    std::string overlayObservationSha;
    std::string overlayLine;
};

[[nodiscard]] ProofLog parse_proof_log(const std::string& path) {
    std::ifstream input(path);
    if (!input) fail("cannot read proof log " + path);
    const std::regex iterationPattern(
      R"(^ghost_extra_external_iteration ([0-9]+) bdd_nodes ([0-9]+) changed_owner ([0-9]+) changed_observer ([0-9]+) changed_visible ([0-9]+).*$)");
    const std::regex compactionPattern(
      R"(^ghost_extra_external_compaction iteration ([0-9]+).*structural_residual 0 root_residual 0$)");
    const std::regex certificatePattern(
      R"(^information_symbolic_certificate iterations ([0-9]+) bdd_nodes ([0-9]+) bellman_residual 0 monotonicity_residual 0 singleton_residual 0 compaction_root_residual 0 belief_cap none powerset_exact 1$)");
    const std::regex overlayPattern(
      R"(^information_overlay [^ ]+ bytes ([0-9]+) source_sha256 ([0-9a-f]{64}) solver_model_sha256 ([0-9a-f]{64}) observation_model_sha256 ([0-9a-f]{64}) root_grouping_residual 0 conservation_residual 0$)");
    std::smatch match;
    std::string line;
    std::uint32_t lastIteration = 0;
    std::uint64_t lastChanged = 1;
    std::vector<std::uint32_t> compacted;
    ProofLog result;
    unsigned summaries = 0;
    while (std::getline(input, line)) {
        if (std::regex_match(line, match, iterationPattern)) {
            lastIteration = static_cast<std::uint32_t>(std::stoul(match[1]));
            lastChanged = std::stoull(match[3]) + std::stoull(match[4]) +
                          std::stoull(match[5]);
        }
        else if (std::regex_match(line, match, compactionPattern))
            compacted.push_back(static_cast<std::uint32_t>(std::stoul(match[1])));
        else if (std::regex_match(line, match, certificatePattern)) {
            result.iteration = static_cast<std::uint32_t>(std::stoul(match[1]));
            result.arenaNodes = static_cast<std::uint32_t>(std::stoul(match[2]));
            result.certificateLine = line;
        }
        else if (line.rfind("information_summary side ", 0) == 0) {
            if (summaries >= 2) fail("proof log has duplicate summaries");
            result.summaryLines[summaries++] = line;
        }
        else if (line.rfind("ghost_extra_external_root_conservation ", 0) == 0)
            result.conservationLine = line;
        else if (std::regex_match(line, match, overlayPattern)) {
            if (!result.overlayLine.empty()) fail("proof log has duplicate overlays");
            result.overlayBytes = std::stoull(match[1]);
            result.overlaySourceSha = match[2];
            result.overlayModelSha = match[3];
            result.overlayObservationSha = match[4];
            result.overlayLine = line;
        }
    }
    if (!result.iteration || result.iteration != lastIteration || lastChanged ||
        result.certificateLine.empty() || summaries != 2 ||
        result.conservationLine.empty() || result.overlayLine.empty())
        fail("proof log does not end in a complete zero-Bellman certificate");
    if (result.conservationLine.find("independent_grouping_residual 0") ==
          std::string::npos ||
        result.conservationLine.find("realization_residual 0") ==
          std::string::npos ||
        result.conservationLine.find("conservation_residual 0") ==
          std::string::npos)
        fail("fresh-root certificate has a residual");
    if (compacted.size() + 1 != result.iteration)
        fail("compaction count does not match compact-every-1 proof");
    for (std::size_t index = 0; index < compacted.size(); ++index)
        if (compacted[index] != index + 1)
            fail("compaction sequence is incomplete");
    result.compactions = static_cast<std::uint32_t>(compacted.size());
    result.arena = result.compactions % 2 ? 'b' : 'a';
    return result;
}

void validate_overlay(const std::string& path, const ProofLog& proof,
                      const std::string& sourceSha,
                      const std::string& modelSha,
                      const std::string& observationSha) {
    ReadMapping overlay(path);
    if (proof.overlayBytes != overlay.size() ||
        proof.overlaySourceSha != sourceSha ||
        proof.overlayModelSha != modelSha ||
        proof.overlayObservationSha != observationSha ||
        overlay.size() < 160 ||
        std::memcmp(overlay.data(), "UFIW2\0\0\0", 8) ||
        le32(overlay.data() + 8) != 2 ||
        le32(overlay.data() + 12) !=
          static_cast<std::uint32_t>(PieceType::Bishop) ||
        le32(overlay.data() + 16) !=
          static_cast<std::uint32_t>(PieceType::Ghost) ||
        le32(overlay.data() + 20) !=
          static_cast<std::uint32_t>(Color::White) ||
        le32(overlay.data() + 24) != StateCount ||
        le32(overlay.data() + 28) != GhostSubstates ||
        std::string(reinterpret_cast<const char*>(overlay.data() + 32), 64) !=
          sourceSha ||
        std::string(reinterpret_cast<const char*>(overlay.data() + 96), 64) !=
          modelSha)
        fail("proof overlay metadata/hash binding mismatch");
}

[[nodiscard]] bool equal_files(const std::string& first,
                               const std::string& second,
                               std::uint64_t expected) {
    if (file_bytes(first) != expected || file_bytes(second) != expected)
        return false;
    std::ifstream lhs(first, std::ios::binary), rhs(second, std::ios::binary);
    std::array<char, 1 << 20> a{}, b{};
    while (lhs && rhs) {
        lhs.read(a.data(), a.size());
        rhs.read(b.data(), b.size());
        if (lhs.gcount() != rhs.gcount() ||
            std::memcmp(a.data(), b.data(), static_cast<std::size_t>(lhs.gcount())))
            return false;
    }
    return true;
}

struct Manifest {
    std::map<std::string, std::string> values;
};

void write_manifest(const std::string& path, const Manifest& manifest) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) fail("cannot write manifest " + path);
    output << ManifestSchema << '\n';
    for (const auto& [key, value] : manifest.values) {
        if (key.find('=') != std::string::npos || value.find('\n') != std::string::npos)
            fail("manifest key/value is not canonical");
        output << key << '=' << value << '\n';
    }
    if (!output) fail("failed writing manifest " + path);
}

[[nodiscard]] Manifest read_manifest(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) fail("cannot read manifest " + path);
    std::string line;
    if (!std::getline(input, line) || line != ManifestSchema)
        fail("manifest schema mismatch");
    Manifest result;
    while (std::getline(input, line)) {
        const std::size_t split = line.find('=');
        if (!split || split == std::string::npos ||
            !result.values.emplace(line.substr(0, split),
                                   line.substr(split + 1)).second)
            fail("malformed or duplicate manifest field");
    }
    return result;
}

[[nodiscard]] const std::string& field(const Manifest& manifest,
                                       const std::string& key) {
    const auto found = manifest.values.find(key);
    if (found == manifest.values.end()) fail("manifest lacks " + key);
    return found->second;
}

struct Header {
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
    std::string sourceSha;
    std::string modelSha;
    std::string observationSha;
    std::string lowerSha;
    std::array<std::string, 6> transitionSha{};
    std::string transitionPayloadSha;
    std::string payloadSha;
};

[[nodiscard]] Header parse_header(const ReadMapping& file) {
    const std::uint8_t* data = file.data();
    if (file.size() < PortableHeaderBytes ||
        std::memcmp(data, "UFGX2\0\0\0", 8) || le32(data + 8) != 2 ||
        le32(data + 12) != PortableHeaderBytes ||
        le32(data + 16) != EndianMarker ||
        le32(data + 20) != static_cast<std::uint32_t>(PieceType::Bishop) ||
        le32(data + 24) != static_cast<std::uint32_t>(PieceType::Ghost) ||
        le32(data + 28) != static_cast<std::uint32_t>(Color::White) ||
        le32(data + 32) != Variables || le32(data + 36) != StateCount ||
        le32(data + 40) != PortableNodeBytes ||
        le32(data + 44) != PortableMetaBytes ||
        le32(data + 48) != PortableMaskBytes ||
        le32(data + 52) != PortableRootBytes || le32(data + 56))
        fail("UFGX2 fixed header mismatch");
    Header result;
    result.nodes = le64(data + 60);
    result.geometries = le64(data + 68);
    result.strata = le64(data + 76);
    result.ownerRoots = le64(data + 84);
    result.nodeOffset = le64(data + 92);
    result.geometryOffset = le64(data + 100);
    result.stratumOffset = le64(data + 108);
    result.ownerOffset = le64(data + 116);
    result.observerOffset = le64(data + 124);
    result.visibleOwnerOffset = le64(data + 132);
    result.visibleObserverOffset = le64(data + 140);
    result.payloadBytes = le64(data + 148);
    const auto text = [&](std::size_t offset) {
        return std::string(reinterpret_cast<const char*>(data + offset), 64);
    };
    result.sourceSha = text(156);
    result.modelSha = text(220);
    result.observationSha = text(284);
    result.lowerSha = text(348);
    for (std::size_t index = 0; index < 6; ++index)
        result.transitionSha[index] = text(412 + index * 64);
    result.transitionPayloadSha = text(796);
    result.payloadSha = text(860);
    if (std::string(reinterpret_cast<const char*>(data + 924),
                    std::strlen(Semantics)) != Semantics)
        fail("UFGX2 semantic contract mismatch");
    return result;
}

[[nodiscard]] std::array<std::uint8_t, PortableHeaderBytes> make_header(
  const Header& header) {
    std::array<std::uint8_t, PortableHeaderBytes> data{};
    std::memcpy(data.data(), "UFGX2\0\0\0", 8);
    put32(data.data() + 8, 2);
    put32(data.data() + 12, PortableHeaderBytes);
    put32(data.data() + 16, EndianMarker);
    put32(data.data() + 20, static_cast<std::uint32_t>(PieceType::Bishop));
    put32(data.data() + 24, static_cast<std::uint32_t>(PieceType::Ghost));
    put32(data.data() + 28, static_cast<std::uint32_t>(Color::White));
    put32(data.data() + 32, Variables);
    put32(data.data() + 36, StateCount);
    put32(data.data() + 40, PortableNodeBytes);
    put32(data.data() + 44, PortableMetaBytes);
    put32(data.data() + 48, PortableMaskBytes);
    put32(data.data() + 52, PortableRootBytes);
    put64(data.data() + 60, header.nodes);
    put64(data.data() + 68, header.geometries);
    put64(data.data() + 76, header.strata);
    put64(data.data() + 84, header.ownerRoots);
    put64(data.data() + 92, header.nodeOffset);
    put64(data.data() + 100, header.geometryOffset);
    put64(data.data() + 108, header.stratumOffset);
    put64(data.data() + 116, header.ownerOffset);
    put64(data.data() + 124, header.observerOffset);
    put64(data.data() + 132, header.visibleOwnerOffset);
    put64(data.data() + 140, header.visibleObserverOffset);
    put64(data.data() + 148, header.payloadBytes);
    const auto copy = [&](std::size_t offset, const std::string& value) {
        if (!valid_sha(value)) fail("invalid SHA binding in header");
        std::memcpy(data.data() + offset, value.data(), 64);
    };
    copy(156, header.sourceSha);
    copy(220, header.modelSha);
    copy(284, header.observationSha);
    copy(348, header.lowerSha);
    for (std::size_t index = 0; index < 6; ++index)
        copy(412 + index * 64, header.transitionSha[index]);
    copy(796, header.transitionPayloadSha);
    copy(860, header.payloadSha);
    std::memcpy(data.data() + 924, Semantics, std::strlen(Semantics));
    return data;
}

[[nodiscard]] std::string combined_transition_sha(const std::string& prefix) {
    Sha256 hash;
    std::array<std::uint8_t, 1 << 20> buffer{};
    for (const char* suffix : {".header", ".meta", ".strata", ".index",
                               ".blocks", ".verified"}) {
        std::ifstream input(prefix + suffix, std::ios::binary);
        if (!input) fail("missing transition component");
        while (input) {
            input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
            if (input.gcount() > 0)
                hash.update(buffer.data(),
                            static_cast<std::size_t>(input.gcount()));
        }
    }
    return hex_digest(hash.finish());
}

void authenticate_transition_marker(const std::string& prefix) {
    if (!equal_files(prefix + ".header", prefix + ".verified",
                     sizeof(ExternalTransitionHeader)))
        fail("transition regeneration marker does not match its header");
}

struct Options {
    enum class Command { None, Export, Verify, SelfTest } command = Command::None;
    std::string scratch;
    std::string transitions;
    std::string source;
    std::string lower;
    std::string solveLog;
    std::string transitionArchive;
    std::string overlay;
    std::string input;
    std::string output;
    std::string manifest;
    std::string verifyScratch;
    std::string sourceSha;
    std::string modelSha;
    std::string observationSha;
    std::string transitionArchiveSha;
    bool fullBellman = false;
};

[[nodiscard]] std::string lower_header_hash(const ReadMapping& lower,
                                            std::size_t offset) {
    if (lower.size() < 320 || std::memcmp(lower.data(), "UFGM1\0\0\0", 8) ||
        le32(lower.data() + 12) != 320)
        fail("lower UFGM1 header mismatch");
    return std::string(reinterpret_cast<const char*>(lower.data() + offset), 64);
}

class ArenaView {
  public:
    ArenaView(const std::string& path, std::uint32_t count)
      : file_(path), count_(count) {
        if (file_.size() < std::uint64_t(count_) * PortableNodeBytes || count_ < 2)
            fail("active BDD arena is truncated");
        const DiskNode zero = node(0), one = node(1);
        if (zero.variable != Variables || zero.low || zero.high ||
            one.variable != Variables || one.low != 1 || one.high != 1)
            fail("active BDD arena terminals are invalid");
    }
    [[nodiscard]] DiskNode node(std::uint32_t id) const {
        if (id >= count_) fail("BDD node id outside certified arena");
        return read_node(file_.data() + std::uint64_t(id) * PortableNodeBytes);
    }
    [[nodiscard]] std::uint32_t count() const { return count_; }
    [[nodiscard]] const ReadMapping& file() const { return file_; }
  private:
    ReadMapping file_;
    std::uint32_t count_ = 0;
};

void mark_root(const ArenaView& arena, std::vector<std::uint8_t>& marks,
               std::uint32_t root, std::uint64_t& marked) {
    std::vector<std::uint32_t> stack{root};
    while (!stack.empty()) {
        const std::uint32_t id = stack.back();
        stack.pop_back();
        if (id >= arena.count()) fail("force root exceeds certified arena");
        const std::uint8_t bit = static_cast<std::uint8_t>(1u << (id % 8));
        if (marks[id / 8] & bit) continue;
        marks[id / 8] |= bit;
        ++marked;
        if (id > 1) {
            const DiskNode node = arena.node(id);
            if (node.variable >= Variables || node.low >= id || node.high >= id ||
                node.low == node.high)
                fail("source BDD structural residual");
            stack.push_back(node.low);
            stack.push_back(node.high);
        }
    }
}

[[nodiscard]] std::uint32_t mapped_root(const ReadMapping& roots,
                                        std::uint64_t index,
                                        const std::uint8_t* remap,
                                        std::uint32_t sourceNodes) {
    const std::uint32_t old = le32(roots.data() + index * 4);
    if (old >= sourceNodes) fail("source root exceeds certified BDD arena");
    const std::uint32_t mapped = le32(remap + std::uint64_t(old) * 4);
    if (old > 1 && mapped <= 1) fail("reachable source root was not copied");
    return mapped;
}

void write_meta(std::ostream& output, const ExternalGeometryMeta& meta) {
    for (const ExternalMask* mask : {&meta.live, &meta.terminal,
                                     &meta.terminalOwner,
                                     &meta.terminalObserver}) {
        write64(output, mask->low);
        write64(output, mask->high);
    }
    for (std::uint32_t value : meta.actualStratum) write32(output, value);
    write32(output, meta.stratumBase);
    write32(output, meta.stratumCount);
}

void write_mask(std::ostream& output, const ExternalMask& mask) {
    write64(output, mask.low);
    write64(output, mask.high);
}

[[nodiscard]] ExternalMask read_mask(const std::uint8_t* source) {
    ExternalMask result;
    result.low = le64(source);
    result.high = static_cast<std::uint16_t>(le64(source + 8));
    return result;
}

[[nodiscard]] ExternalGeometryMeta read_meta(const std::uint8_t* source) {
    ExternalGeometryMeta meta;
    meta.live = read_mask(source);
    meta.terminal = read_mask(source + 16);
    meta.terminalOwner = read_mask(source + 32);
    meta.terminalObserver = read_mask(source + 48);
    for (unsigned square = 0; square < Variables; ++square)
        meta.actualStratum[square] = le32(source + 64 + square * 4);
    meta.stratumBase = le32(source + 384);
    meta.stratumCount = le32(source + 388);
    return meta;
}

void export_sidecar(const Options& options) {
    for (const std::string* required : {&options.scratch, &options.transitions,
         &options.source, &options.lower, &options.solveLog,
         &options.transitionArchive, &options.overlay, &options.output,
         &options.manifest, &options.sourceSha, &options.modelSha,
         &options.observationSha, &options.transitionArchiveSha})
        if (required->empty()) fail("export is missing a required argument");
    for (const std::string* hash : {&options.sourceSha, &options.modelSha,
                                    &options.observationSha,
                                    &options.transitionArchiveSha})
        if (!valid_sha(*hash)) fail("export binding is not SHA-256");
    if (sha_file(options.source) != options.sourceSha)
        fail("source table SHA mismatch");
    if (sha_file(options.transitionArchive) != options.transitionArchiveSha)
        fail("transition recovery archive SHA mismatch");
    const std::string lowerSha = sha_file(options.lower);
    ReadMapping lower(options.lower);
    const std::string lowerSource = lower_header_hash(lower, 96);
    const std::string lowerModel = lower_header_hash(lower, 160);
    const std::string lowerObservation = lower_header_hash(lower, 224);
    if (lowerObservation != options.observationSha)
        fail("lower sidecar observation-model mismatch");

    const ProofLog proof = parse_proof_log(options.solveLog);
    validate_overlay(options.overlay, proof, options.sourceSha,
                     options.modelSha, options.observationSha);
    const MaterialSpec material{Color::White};
    authenticate_transition_marker(options.transitions);
    ExternalTransitionDatabase database(options.transitions, material);
    if (database.geometry_count() != FullGeometries)
        fail("transition database is not the complete geometry domain");
    const std::uint64_t ownerRoots =
      std::uint64_t(database.geometry_count()) * Variables;
    const std::uint64_t ownerBytes = ownerRoots * 4;
    const std::uint64_t observerBytes = database.stratum_count() * 4;
    const std::uint64_t scalarBytes = ownerRoots;
    for (const auto& [current, next, bytes] : {
           std::tuple{options.scratch + ".owner-current",
                      options.scratch + ".owner-next", ownerBytes},
           std::tuple{options.scratch + ".observer-current",
                      options.scratch + ".observer-next", observerBytes},
           std::tuple{options.scratch + ".visible-owner-current",
                      options.scratch + ".visible-owner-next", scalarBytes},
           std::tuple{options.scratch + ".visible-observer-current",
                      options.scratch + ".visible-observer-next", scalarBytes}})
        if (!equal_files(current, next, bytes))
            fail("current/next force arrays differ after zero Bellman pass");

    const std::string arenaPath = options.scratch + ".bdd-" + proof.arena +
                                  ".nodes";
    ArenaView arena(arenaPath, proof.arenaNodes);
    ReadMapping owner(options.scratch + ".owner-current");
    ReadMapping observer(options.scratch + ".observer-current");
    ReadMapping visibleOwner(options.scratch + ".visible-owner-current");
    ReadMapping visibleObserver(options.scratch + ".visible-observer-current");
    std::vector<std::uint8_t> marks((arena.count() + 7) / 8, 0);
    std::uint64_t marked = 0;
    mark_root(arena, marks, 0, marked);
    mark_root(arena, marks, 1, marked);
    for (std::uint64_t index = 0; index < ownerRoots; ++index)
        mark_root(arena, marks, le32(owner.data() + index * 4), marked);
    for (std::uint64_t index = 0; index < database.stratum_count(); ++index)
        mark_root(arena, marks, le32(observer.data() + index * 4), marked);
    if (marked > std::numeric_limits<std::uint32_t>::max())
        fail("compact sidecar exceeds 32-bit ROBDD IDs");

    const std::string remapPath = options.output + ".remap.tmp";
    WriteMapping remap(remapPath, std::uint64_t(arena.count()) * 4);
    Header header;
    header.nodes = marked;
    header.geometries = database.geometry_count();
    header.strata = database.stratum_count();
    header.ownerRoots = ownerRoots;
    header.nodeOffset = PortableHeaderBytes;
    header.geometryOffset = header.nodeOffset + header.nodes * PortableNodeBytes;
    header.stratumOffset = header.geometryOffset +
                           header.geometries * PortableMetaBytes;
    header.ownerOffset = header.stratumOffset +
                         header.strata * PortableMaskBytes;
    header.observerOffset = header.ownerOffset + ownerRoots * PortableRootBytes;
    header.visibleOwnerOffset = header.observerOffset +
                                header.strata * PortableRootBytes;
    header.visibleObserverOffset = header.visibleOwnerOffset + ownerRoots;
    const std::uint64_t extent = header.visibleObserverOffset + ownerRoots;
    header.payloadBytes = extent - PortableHeaderBytes;
    header.sourceSha = options.sourceSha;
    header.modelSha = options.modelSha;
    header.observationSha = options.observationSha;
    header.lowerSha = lowerSha;
    std::size_t component = 0;
    for (const char* suffix : {".header", ".meta", ".strata", ".index",
                               ".blocks", ".verified"})
        header.transitionSha[component++] = sha_file(options.transitions + suffix);
    header.transitionPayloadSha = combined_transition_sha(options.transitions);
    header.payloadSha = std::string(64, '0');

    std::ofstream output(options.output, std::ios::binary | std::ios::trunc);
    if (!output) fail("cannot create UFGX2 output");
    const auto blankHeader = make_header(header);
    output.write(reinterpret_cast<const char*>(blankHeader.data()),
                 blankHeader.size());
    std::uint32_t nextId = 0;
    for (std::uint32_t old = 0; old < arena.count(); ++old) {
        if (!((marks[old / 8] >> (old % 8)) & 1u)) continue;
        put32(remap.data() + std::uint64_t(old) * 4, nextId);
        const DiskNode source = arena.node(old);
        DiskNode compact = source;
        if (old <= 1) {
            compact.variable = Variables;
            compact.low = compact.high = old;
        }
        else {
            compact.low = le32(remap.data() + std::uint64_t(source.low) * 4);
            compact.high = le32(remap.data() + std::uint64_t(source.high) * 4);
            if (compact.low >= nextId || compact.high >= nextId ||
                compact.low == compact.high)
                fail("streaming BDD copy order residual");
        }
        write_node(output, compact);
        ++nextId;
    }
    if (nextId != marked) fail("marked/copy node conservation residual");
    for (std::uint32_t geometry = 0; geometry < database.geometry_count();
         ++geometry)
        write_meta(output, database.meta(geometry));
    for (std::uint32_t stratum = 0; stratum < database.stratum_count();
         ++stratum)
        write_mask(output, database.stratum(stratum));
    for (std::uint64_t index = 0; index < ownerRoots; ++index)
        write32(output, mapped_root(owner, index, remap.data(), arena.count()));
    for (std::uint64_t index = 0; index < database.stratum_count(); ++index)
        write32(output, mapped_root(observer, index, remap.data(), arena.count()));
    output.write(reinterpret_cast<const char*>(visibleOwner.data()), scalarBytes);
    output.write(reinterpret_cast<const char*>(visibleObserver.data()), scalarBytes);
    output.close();
    remap.flush();
    if (!output || file_bytes(options.output) != extent)
        fail("UFGX2 extent residual");
    header.payloadSha = sha_file(options.output, PortableHeaderBytes);
    const auto finalHeader = make_header(header);
    std::fstream rewrite(options.output,
      std::ios::binary | std::ios::in | std::ios::out);
    rewrite.write(reinterpret_cast<const char*>(finalHeader.data()),
                  finalHeader.size());
    rewrite.close();
    if (!rewrite) fail("cannot finalize UFGX2 header");
    std::filesystem::remove(remapPath);

    Manifest manifest;
    auto& values = manifest.values;
    values["active_arena"] = std::string(1, proof.arena);
    values["active_arena_certified_nodes"] = std::to_string(proof.arenaNodes);
    values["active_arena_extent"] = std::to_string(arena.file().size());
    values["active_arena_used_sha256"] = sha_file(arenaPath, 0,
      std::uint64_t(proof.arenaNodes) * PortableNodeBytes);
    values["compactions"] = std::to_string(proof.compactions);
    values["final_certificate_line_sha256"] = [&] {
        Sha256 hash;
        hash.update(reinterpret_cast<const std::uint8_t*>(
          proof.certificateLine.data()), proof.certificateLine.size());
        return hex_digest(hash.finish());
    }();
    values["final_iteration"] = std::to_string(proof.iteration);
    values["final_overlay_line_sha256"] = [&] {
        Sha256 hash;
        hash.update(reinterpret_cast<const std::uint8_t*>(
          proof.overlayLine.data()), proof.overlayLine.size());
        return hex_digest(hash.finish());
    }();
    values["lower_model_sha256"] = lowerModel;
    values["lower_observation_sha256"] = lowerObservation;
    values["lower_sidecar_sha256"] = lowerSha;
    values["lower_source_sha256"] = lowerSource;
    values["model_sha256"] = options.modelSha;
    values["observation_sha256"] = options.observationSha;
    values["overlay_bytes"] = std::to_string(file_bytes(options.overlay));
    values["overlay_sha256"] = sha_file(options.overlay);
    values["sidecar_bytes"] = std::to_string(file_bytes(options.output));
    values["sidecar_sha256"] = sha_file(options.output);
    values["solve_log_sha256"] = sha_file(options.solveLog);
    values["source_sha256"] = options.sourceSha;
    values["transition_archive_sha256"] = options.transitionArchiveSha;
    values["transition_payload_sha256"] = header.transitionPayloadSha;
    for (std::size_t index = 0; index < 6; ++index)
        values["transition_" + std::to_string(index) + "_sha256"] =
          header.transitionSha[index];
    for (const auto& [name, path] : {
           std::pair{"owner_current", options.scratch + ".owner-current"},
           {"owner_next", options.scratch + ".owner-next"},
           {"observer_current", options.scratch + ".observer-current"},
           {"observer_next", options.scratch + ".observer-next"},
           {"visible_owner_current", options.scratch + ".visible-owner-current"},
           {"visible_owner_next", options.scratch + ".visible-owner-next"},
           {"visible_observer_current", options.scratch + ".visible-observer-current"},
           {"visible_observer_next", options.scratch + ".visible-observer-next"}})
        values[std::string(name) + "_sha256"] = sha_file(path);
    values["reachable_nodes"] = std::to_string(marked);
    write_manifest(options.manifest, manifest);
    std::cout << "bishop_ghost_preservation_export nodes " << marked
              << " source_nodes " << proof.arenaNodes
              << " compactions " << proof.compactions
              << " active_arena " << proof.arena
              << " root_pair_residual 0 sidecar_sha256 "
              << values["sidecar_sha256"] << '\n';
}

class SidecarView {
  public:
    explicit SidecarView(const std::string& path) : file_(path),
      header_(parse_header(file_)) {
        const std::uint64_t expectedGeometry = header_.nodeOffset +
          header_.nodes * PortableNodeBytes;
        const std::uint64_t expectedStratum = expectedGeometry +
          header_.geometries * PortableMetaBytes;
        const std::uint64_t expectedOwner = expectedStratum +
          header_.strata * PortableMaskBytes;
        const std::uint64_t expectedObserver = expectedOwner +
          header_.ownerRoots * PortableRootBytes;
        const std::uint64_t expectedVisibleOwner = expectedObserver +
          header_.strata * PortableRootBytes;
        const std::uint64_t expectedVisibleObserver = expectedVisibleOwner +
          header_.ownerRoots;
        const std::uint64_t extent = expectedVisibleObserver +
                                     header_.ownerRoots;
        if (header_.nodes < 2 ||
            header_.nodes > std::numeric_limits<std::uint32_t>::max() ||
            header_.geometries != FullGeometries ||
            header_.ownerRoots != header_.geometries * Variables ||
            header_.nodeOffset != PortableHeaderBytes ||
            header_.geometryOffset != expectedGeometry ||
            header_.stratumOffset != expectedStratum ||
            header_.ownerOffset != expectedOwner ||
            header_.observerOffset != expectedObserver ||
            header_.visibleOwnerOffset != expectedVisibleOwner ||
            header_.visibleObserverOffset != expectedVisibleObserver ||
            header_.payloadBytes != extent - PortableHeaderBytes ||
            extent != file_.size() ||
            sha_file(path, PortableHeaderBytes) != header_.payloadSha)
            fail("UFGX2 section/extent/payload residual");
    }
    [[nodiscard]] const Header& header() const { return header_; }
    [[nodiscard]] DiskNode node(std::uint32_t id) const {
        if (id >= header_.nodes) fail("sidecar node outside arena");
        return read_node(file_.data() + header_.nodeOffset +
                         std::uint64_t(id) * PortableNodeBytes);
    }
    [[nodiscard]] ExternalGeometryMeta meta(std::uint32_t id) const {
        if (id >= header_.geometries) fail("sidecar geometry outside domain");
        return read_meta(file_.data() + header_.geometryOffset +
                         std::uint64_t(id) * PortableMetaBytes);
    }
    [[nodiscard]] ExternalMask stratum(std::uint32_t id) const {
        if (id >= header_.strata) fail("sidecar stratum outside domain");
        return read_mask(file_.data() + header_.stratumOffset +
                         std::uint64_t(id) * PortableMaskBytes);
    }
    [[nodiscard]] std::uint32_t owner_root(std::uint64_t id) const {
        if (id >= header_.ownerRoots) fail("owner root outside domain");
        return le32(file_.data() + header_.ownerOffset + id * 4);
    }
    [[nodiscard]] std::uint32_t observer_root(std::uint64_t id) const {
        if (id >= header_.strata) fail("observer root outside domain");
        return le32(file_.data() + header_.observerOffset + id * 4);
    }
    [[nodiscard]] std::uint8_t visible_owner(std::uint64_t id) const {
        if (id >= header_.ownerRoots) fail("visible root outside domain");
        return file_.data()[header_.visibleOwnerOffset + id];
    }
    [[nodiscard]] std::uint8_t visible_observer(std::uint64_t id) const {
        if (id >= header_.ownerRoots) fail("visible root outside domain");
        return file_.data()[header_.visibleObserverOffset + id];
    }
    [[nodiscard]] bool evaluate(std::uint32_t root,
                                const ExternalMask& mask) const {
        while (root > 1) {
            const DiskNode current = node(root);
            root = external_mask_test(mask, current.variable)
                 ? current.high : current.low;
        }
        return root == 1;
    }
  private:
    ReadMapping file_;
    Header header_;
};

void validate_nodes(const SidecarView& sidecar) {
    const std::uint64_t count = sidecar.header().nodes;
    std::uint64_t slotsCount = 4;
    while (slotsCount < count * 2) slotsCount *= 2;
    std::vector<std::uint32_t> slots(slotsCount,
      std::numeric_limits<std::uint32_t>::max());
    for (std::uint32_t id = 0; id < count; ++id) {
        const DiskNode node = sidecar.node(id);
        if (id <= 1) {
            if (node.variable != Variables || node.low != id || node.high != id)
                fail("sidecar terminal tuple residual");
            continue;
        }
        if (node.variable >= Variables || node.low >= id || node.high >= id ||
            node.low == node.high ||
            (node.low > 1 && sidecar.node(node.low).variable <= node.variable) ||
            (node.high > 1 && sidecar.node(node.high).variable <= node.variable))
            fail("sidecar ROBDD structural residual");
        std::uint64_t hash = mix64(node.variable);
        hash = mix64(hash ^ node.low);
        hash = mix64(hash ^ node.high);
        std::size_t slot = static_cast<std::size_t>(hash & (slots.size() - 1));
        for (;;) {
            std::uint32_t& previous = slots[slot];
            if (previous == std::numeric_limits<std::uint32_t>::max()) {
                previous = id;
                break;
            }
            const DiskNode other = sidecar.node(previous);
            if (other.variable == node.variable && other.low == node.low &&
                other.high == node.high)
                fail("sidecar contains a duplicate full ROBDD tuple");
            slot = (slot + 1) & (slots.size() - 1);
        }
    }
}

struct PairHash {
    std::size_t operator()(std::uint64_t value) const noexcept {
        return static_cast<std::size_t>(mix64(value));
    }
};

[[nodiscard]] bool conjunction_sat(const SidecarView& sidecar,
                                   std::uint32_t first, std::uint32_t second,
                                   const ExternalMask& domain,
                                   unsigned requiredActual,
                                   std::unordered_map<std::uint64_t, bool,
                                                      PairHash>& memo) {
    if (!first || !second) return false;
    if (first == 1 && second == 1) return true;
    if (first > second) std::swap(first, second);
    const std::uint64_t key = (std::uint64_t(first) << 32) | second;
    if (const auto found = memo.find(key); found != memo.end())
        return found->second;
    const DiskNode lhs = first > 1 ? sidecar.node(first) : DiskNode{};
    const DiskNode rhs = second > 1 ? sidecar.node(second) : DiskNode{};
    const std::uint8_t variable = std::min(lhs.variable, rhs.variable);
    if (variable >= Variables)
        fail("nonterminal conjunction reached no decision variable");
    const std::uint32_t lhsLow = lhs.variable == variable ? lhs.low : first;
    const std::uint32_t lhsHigh = lhs.variable == variable ? lhs.high : first;
    const std::uint32_t rhsLow = rhs.variable == variable ? rhs.low : second;
    const std::uint32_t rhsHigh = rhs.variable == variable ? rhs.high : second;
    bool result = false;
    if (variable != requiredActual)
        result = conjunction_sat(sidecar, lhsLow, rhsLow, domain,
                                 requiredActual, memo);
    if (!result && external_mask_test(domain, variable))
        result = conjunction_sat(sidecar, lhsHigh, rhsHigh, domain,
                                 requiredActual, memo);
    memo.emplace(key, result);
    return result;
}

void validate_domain(const SidecarView& sidecar,
                     const std::string& transitions) {
    const MaterialSpec material{Color::White};
    authenticate_transition_marker(transitions);
    ExternalTransitionDatabase database(transitions, material);
    const ExtraGeometryDomain domain;
    if (database.geometry_count() != sidecar.header().geometries ||
        database.stratum_count() != sidecar.header().strata)
        fail("sidecar/transition domain counts differ");
    std::uint64_t dual = 0;
    for (std::uint32_t geometry = 0; geometry < database.geometry_count();
         ++geometry) {
        const ExternalGeometryMeta expected = database.meta(geometry);
        const ExternalGeometryMeta meta = sidecar.meta(geometry);
        if (std::memcmp(&expected, &meta, sizeof(meta)))
            fail("portable geometry metadata differs from transitions");
        if ((meta.live.low & meta.terminal.low) ||
            (meta.live.high & meta.terminal.high) ||
            (meta.terminalOwner.low & meta.terminalObserver.low) ||
            (meta.terminalOwner.high & meta.terminalObserver.high) ||
            (meta.terminalOwner.low & ~meta.terminal.low) ||
            (meta.terminalOwner.high & ~meta.terminal.high) ||
            (meta.terminalObserver.low & ~meta.terminal.low) ||
            (meta.terminalObserver.high & ~meta.terminal.high))
            fail("live/terminal outcome masks are inconsistent");
        const bool visible = domain[geometry].visible != 0;
        for (unsigned actual = 0; actual < Variables; ++actual) {
            const std::uint64_t root = std::uint64_t(geometry) * Variables + actual;
            if (sidecar.owner_root(root) >= sidecar.header().nodes ||
                sidecar.visible_owner(root) > 1 ||
                sidecar.visible_observer(root) > 1)
                fail("sidecar owner/scalar root residual");
            const bool live = external_mask_test(meta.live, actual);
            const bool terminal = external_mask_test(meta.terminal, actual);
            const std::uint32_t stratum = meta.actualStratum[actual];
            if ((!live || visible) && stratum != NoIndex)
                fail("non-hidden/non-live world has a decision stratum");
            if (live && !visible && stratum == NoIndex)
                fail("live hidden world lacks a decision stratum");
            if ((!visible || !live || terminal) &&
                (sidecar.visible_owner(root) ||
                 sidecar.visible_observer(root)))
                fail("visible scalar leaks into a hidden/non-live world");
            if (!live) continue;
            if (sidecar.visible_owner(root) && sidecar.visible_observer(root))
                ++dual;
            if (visible)
                continue;
            if (stratum == NoIndex || stratum >= sidecar.header().strata ||
                !external_mask_test(sidecar.stratum(stratum), actual))
                fail("live world lacks its legal-dot decision stratum");
            std::unordered_map<std::uint64_t, bool, PairHash> memo;
            if (conjunction_sat(sidecar, sidecar.owner_root(root),
                                sidecar.observer_root(stratum),
                                sidecar.stratum(stratum), actual, memo))
                ++dual;
        }
    }
    for (std::uint32_t stratum = 0; stratum < database.stratum_count();
         ++stratum) {
        const ExternalMask expected = database.stratum(stratum);
        const ExternalMask actual = sidecar.stratum(stratum);
        if (expected.low != actual.low || expected.high != actual.high ||
            sidecar.observer_root(stratum) >= sidecar.header().nodes)
            fail("portable stratum/root residual");
    }
    if (dual) fail("arbitrary owner/observer force intersection is nonempty");
    std::cout << "bishop_ghost_arbitrary_disjoint geometries "
              << database.geometry_count() << " residual 0\n";
}

void validate_manifest_bindings(const Options& options, const SidecarView& sidecar,
                                const Manifest& manifest) {
    if (field(manifest, "sidecar_sha256") != sha_file(options.input) ||
        std::stoull(field(manifest, "sidecar_bytes")) != file_bytes(options.input) ||
        field(manifest, "source_sha256") != sidecar.header().sourceSha ||
        field(manifest, "model_sha256") != sidecar.header().modelSha ||
        field(manifest, "observation_sha256") !=
          sidecar.header().observationSha ||
        field(manifest, "lower_sidecar_sha256") != sidecar.header().lowerSha ||
        field(manifest, "transition_payload_sha256") !=
          sidecar.header().transitionPayloadSha)
        fail("manifest/sidecar provenance mismatch");
    for (std::size_t index = 0; index < 6; ++index)
        if (field(manifest, "transition_" + std::to_string(index) + "_sha256") !=
              sidecar.header().transitionSha[index])
            fail("manifest transition component mismatch");
    if (options.source.empty() || options.lower.empty() ||
        options.solveLog.empty() || options.transitionArchive.empty() ||
        options.overlay.empty())
        fail("strict restore needs every archived proof dependency");
    if (sha_file(options.source) != field(manifest, "source_sha256") ||
        sha_file(options.lower) != field(manifest, "lower_sidecar_sha256") ||
        sha_file(options.solveLog) != field(manifest, "solve_log_sha256") ||
        sha_file(options.transitionArchive) !=
          field(manifest, "transition_archive_sha256") ||
        sha_file(options.overlay) != field(manifest, "overlay_sha256") ||
        std::stoull(field(manifest, "overlay_bytes")) !=
          file_bytes(options.overlay))
        fail("restored proof dependency SHA/extent mismatch");
    const ProofLog proof = parse_proof_log(options.solveLog);
    if (std::to_string(proof.iteration) != field(manifest, "final_iteration") ||
        std::to_string(proof.compactions) != field(manifest, "compactions") ||
        std::to_string(proof.arenaNodes) !=
          field(manifest, "active_arena_certified_nodes") ||
        std::string(1, proof.arena) != field(manifest, "active_arena"))
        fail("restored proof-log certificate differs from manifest");
    validate_overlay(options.overlay, proof, sidecar.header().sourceSha,
                     sidecar.header().modelSha,
                     sidecar.header().observationSha);
}

void validate_singletons_and_fresh(const Options& options,
                                   const SidecarView& sidecar) {
    const MaterialSpec material{Color::White};
    PackedFourTable concrete(options.source, material);
    if (hex_digest(concrete.sha()) != sidecar.header().sourceSha)
        fail("concrete singleton source binding mismatch");
    const ExtraGeometryDomain domain;
    std::vector<std::uint8_t> admitted((StateCount + 7) / 8, 0);
    std::vector<ExternalMask> freshMasks(sidecar.header().strata);
    std::array<std::array<std::uint64_t, 4>, 2> unreachable{};
    std::uint64_t singletonResidual = 0;
    std::uint64_t admittedCount = 0;
    for (std::uint32_t index = 0; index < StateCount; ++index) {
        const FourState state = decode_index(index);
        const PublicExtraGeometry raw{
          static_cast<std::uint8_t>(state.side), state.whiteKing,
          state.blackKing, state.bishop,
          static_cast<std::uint8_t>(state.visible)};
        const auto [geometry, transform] = domain.locate(raw);
        const unsigned actual = rectangle_transform_square(state.ghost,
                                                            transform);
        const ExternalGeometryMeta meta = sidecar.meta(geometry);
        bool owner = false;
        bool observer = false;
        if (external_mask_test(meta.terminal, actual)) {
            owner = external_mask_test(meta.terminalOwner, actual);
            observer = external_mask_test(meta.terminalObserver, actual);
        }
        else if (state.visible) {
            const std::uint64_t root = std::uint64_t(geometry) * Variables +
                                       actual;
            owner = sidecar.visible_owner(root) != 0;
            observer = sidecar.visible_observer(root) != 0;
        }
        else {
            const std::uint32_t stratum = meta.actualStratum[actual];
            if (stratum == NoIndex) fail("singleton lacks decision stratum");
            const ExternalMask singleton = external_singleton_mask(actual);
            owner = sidecar.evaluate(sidecar.owner_root(
              std::uint64_t(geometry) * Variables + actual), singleton);
            observer = sidecar.evaluate(sidecar.observer_root(stratum),
                                        singleton);
        }
        const std::uint8_t exact = concrete.result(index);
        const bool expectedOwner =
          (state.side == Color::White && exact == 1) ||
          (state.side == Color::Black && exact == 2);
        const bool expectedObserver =
          (state.side == Color::Black && exact == 1) ||
          (state.side == Color::White && exact == 2);
        singletonResidual += owner != expectedOwner ||
                             observer != expectedObserver;

        const bool hiddenAdjacent = !state.visible &&
          std::abs(int(state.ghost % Position::BoardFiles) -
                   int(state.blackKing % Position::BoardFiles)) <= 1 &&
          std::abs(int(state.ghost / Position::BoardFiles) -
                   int(state.blackKing / Position::BoardFiles)) <= 1;
        const Position position = make_position(index, material);
        if (hiddenAdjacent || position.has_forced_action() ||
            !position.ordinary_predecessor_king_safe()) {
            ++unreachable[static_cast<std::size_t>(state.side)][exact];
            continue;
        }
        admitted[index / 8] |= static_cast<std::uint8_t>(1u << (index % 8));
        ++admittedCount;
        if (!state.visible) {
            const std::uint32_t stratum = meta.actualStratum[actual];
            if (stratum != NoIndex) external_mask_set(freshMasks[stratum], actual);
        }
    }
    if (singletonResidual)
        fail("UFGX2 singleton result differs from concrete WDL");

    std::array<std::array<std::uint64_t, 4>, 2> totals{};
    std::array<std::uint64_t, 2> rootSets{};
    std::array<std::uint64_t, 2> independentSets{};
    std::array<std::uint64_t, 2> realizations{};
    std::vector<std::uint8_t> seenVisible(sidecar.header().ownerRoots, 0);
    std::vector<std::uint8_t> seenStratum(sidecar.header().strata, 0);
    std::vector<std::uint8_t> seenTerminal(
      std::uint64_t(sidecar.header().geometries) * 3, 0);
    std::vector<std::uint8_t> independentlySeenVisible(
      sidecar.header().ownerRoots, 0);
    std::vector<std::uint8_t> independentlySeenStratum(
      sidecar.header().strata, 0);
    std::vector<std::uint8_t> independentlySeenTerminal(
      std::uint64_t(sidecar.header().geometries) * 3, 0);
    for (std::uint32_t index = 0; index < StateCount; ++index) {
        if (!((admitted[index / 8] >> (index % 8)) & 1u)) continue;
        const FourState state = decode_index(index);
        const std::size_t side = static_cast<std::size_t>(state.side);
        const PublicExtraGeometry raw{
          static_cast<std::uint8_t>(state.side), state.whiteKing,
          state.blackKing, state.bishop,
          static_cast<std::uint8_t>(state.visible)};
        const auto [geometry, transform] = domain.locate(raw);
        const unsigned actual = rectangle_transform_square(state.ghost,
                                                            transform);
        const ExternalGeometryMeta meta = sidecar.meta(geometry);
        ++realizations[side];
        if (state.visible) {
            const std::uint64_t key = std::uint64_t(geometry) * Variables +
                                      actual;
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
                fail("independent fresh root lacks a decision stratum");
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
                const std::uint64_t key = std::uint64_t(geometry) * Variables +
                                          actual;
                if (!seenVisible[key]) {
                    seenVisible[key] = 1;
                    ++rootSets[side];
                }
            }
            else {
                const std::uint64_t key = std::uint64_t(geometry) * 3 +
                  (owner ? 0 : observer ? 1 : 2);
                if (!seenTerminal[key]) {
                    seenTerminal[key] = 1;
                    ++rootSets[side];
                }
            }
        }
        else if (state.visible) {
            const std::uint64_t key = std::uint64_t(geometry) * Variables +
                                      actual;
            owner = sidecar.visible_owner(key) != 0;
            observer = sidecar.visible_observer(key) != 0;
            if (!seenVisible[key]) {
                seenVisible[key] = 1;
                ++rootSets[side];
            }
        }
        else {
            const std::uint32_t stratum = meta.actualStratum[actual];
            if (stratum == NoIndex ||
                !external_mask_test(freshMasks[stratum], actual))
                fail("fresh hidden root crosses its decision stratum");
            owner = sidecar.evaluate(sidecar.owner_root(
              std::uint64_t(geometry) * Variables + actual),
              freshMasks[stratum]);
            observer = sidecar.evaluate(sidecar.observer_root(stratum),
                                        freshMasks[stratum]);
            if (!seenStratum[stratum]) {
                seenStratum[stratum] = 1;
                ++rootSets[side];
            }
        }
        if (owner && observer) fail("fresh root has dual force");
        const bool moverWins = state.side == Color::White ? owner : observer;
        const bool moverLoses = state.side == Color::White ? observer : owner;
        ++totals[side][moverWins ? 1 : moverLoses ? 2 : 3];
    }
    const ProofLog proof = parse_proof_log(options.solveLog);
    std::array<std::map<std::string, std::string>, 2> certified{};
    std::array<bool, 2> certifiedSide{};
    for (const std::string& line : proof.summaryLines) {
        // The first value after the literal `side` is the side number; parse
        // it directly because the remainder is ordinary key/value data.
        std::istringstream input(line);
        std::string first, second;
        unsigned side = 2;
        input >> first >> second >> side;
        if (first != "information_summary" || second != "side" || side > 1 ||
            certifiedSide[side])
            fail("invalid certified summary side");
        certifiedSide[side] = true;
        std::string key, value;
        std::map<std::string, std::string> fields;
        while (input >> key >> value)
            if (!fields.emplace(key, value).second)
                fail("duplicate certified summary field");
        certified[side] = std::move(fields);
    }
    const auto certified_u64 = [&](std::size_t side, const char* key) {
        const auto found = certified[side].find(key);
        if (found == certified[side].end())
            fail(std::string("certified summary lacks ") + key);
        return std::stoull(found->second);
    };
    for (std::size_t side = 0; side < 2; ++side) {
        std::uint64_t conserved = 0;
        for (std::size_t result = 1; result < 4; ++result)
            conserved += totals[side][result] + unreachable[side][result];
        if (conserved != StateCount / 2 ||
            realizations[side] != totals[side][1] + totals[side][2] +
                                  totals[side][3] ||
            rootSets[side] != independentSets[side])
            fail("fresh-root W/L/D does not conserve dense states");
        if (certified_u64(side, "win") != totals[side][1] ||
            certified_u64(side, "loss") != totals[side][2] ||
            certified_u64(side, "draw") != totals[side][3] ||
            certified_u64(side, "unreachable_win") != unreachable[side][1] ||
            certified_u64(side, "unreachable_loss") != unreachable[side][2] ||
            certified_u64(side, "unreachable_draw") != unreachable[side][3] ||
            certified_u64(side, "sets") != rootSets[side] ||
            certified_u64(side, "concrete") != StateCount / 2)
            fail("restored fresh-root result differs from proof summary");
        std::cout << "bishop_ghost_preserved_summary side " << side
                  << " win " << totals[side][1]
                  << " loss " << totals[side][2]
                  << " draw " << totals[side][3]
                  << " sets " << rootSets[side]
                  << " independent_grouping_residual 0"
                  << " realization_residual 0 conservation_residual 0\n";
    }
    std::cout << "bishop_ghost_preserved_singletons states " << StateCount
              << " residual 0 fresh_admitted " << admittedCount
              << " conservation_residual 0\n";
}

void import_and_full_bellman(const Options& options, const SidecarView& sidecar,
                             const Manifest& manifest) {
    if (options.source.empty() || options.lower.empty() ||
        options.transitions.empty() || options.verifyScratch.empty())
        fail("full Bellman restore needs source/lower/transitions/verify-scratch");
    if (sha_file(options.source) != sidecar.header().sourceSha ||
        sha_file(options.lower) != sidecar.header().lowerSha)
        fail("full Bellman source/lower binding mismatch");
    for (std::size_t index = 0; index < 6; ++index) {
        static constexpr std::array<const char*, 6> suffixes{
          ".header", ".meta", ".strata", ".index", ".blocks", ".verified"};
        if (sha_file(options.transitions + suffixes[index]) !=
              sidecar.header().transitionSha[index])
            fail("full Bellman transition component mismatch");
    }
    const MaterialSpec material{Color::White};
    verify_external_transition_certificate(options.transitions, material);
    PackedFourTable concrete(options.source, material);
    ExternalTransitionDatabase database(options.transitions, material);
    ExternalGhostExtraSolveOptions legacy;
    legacy.scratch = options.verifyScratch;
    legacy.output = options.verifyScratch + ".ufiw";
    legacy.sourceSha256 = sidecar.header().sourceSha;
    legacy.modelSha256 = sidecar.header().modelSha;
    legacy.observationSha256 = sidecar.header().observationSha;
    legacy.bddLimits.maxNodes = 500'000'000;
    legacy.bddLimits.uniqueSlots = std::uint64_t{1} << 30;
    legacy.compactEvery = 1;
    gate_external_ghost_extra_solve(options.transitions, database, legacy);
    LowerGhostSymbolicSidecar lower(options.lower,
      field(manifest, "lower_source_sha256"),
      field(manifest, "lower_model_sha256"),
      field(manifest, "lower_observation_sha256"));
    ExtraGeometryDomain domain;
    ExternalGhostExtraFixedPoint solver(database, lower, concrete, domain,
                                         material, legacy);
    std::vector<ExternalRobdd::Id> remap(sidecar.header().nodes, 0);
    remap[1] = 1;
    for (std::uint32_t id = 2; id < sidecar.header().nodes; ++id) {
        const DiskNode node = sidecar.node(id);
        remap[id] = solver.bdd_->make(node.variable, remap[node.low],
                                     remap[node.high]);
    }
    for (std::uint64_t index = 0; index < sidecar.header().ownerRoots; ++index) {
        solver.ownerCurrent_[index] = remap.at(sidecar.owner_root(index));
        solver.visibleOwnerCurrent_[index] = sidecar.visible_owner(index);
        solver.visibleObserverCurrent_[index] = sidecar.visible_observer(index);
    }
    for (std::uint64_t index = 0; index < sidecar.header().strata; ++index)
        solver.observerCurrent_[index] = remap.at(sidecar.observer_root(index));
    solver.verify();
    if (!options.overlay.empty() &&
        sha_file(options.overlay) != sha_file(legacy.output))
        fail("full Bellman fresh overlay differs from preserved proof");
    std::cout << "bishop_ghost_full_bellman_restore residual 0\n";
}

void verify_sidecar(const Options& options) {
    if (options.input.empty() || options.manifest.empty() ||
        options.transitions.empty())
        fail("verify needs input/manifest/transitions");
    const Manifest manifest = read_manifest(options.manifest);
    SidecarView sidecar(options.input);
    validate_manifest_bindings(options, sidecar, manifest);
    validate_nodes(sidecar);
    validate_domain(sidecar, options.transitions);
    codec_self_test(MaterialSpec{Color::White});
    validate_singletons_and_fresh(options, sidecar);
    if (options.fullBellman)
        import_and_full_bellman(options, sidecar, manifest);
    std::cout << "bishop_ghost_preservation_restore nodes "
              << sidecar.header().nodes
              << " tuple_residual 0 domain_residual 0 d2_codec_residual 0"
              << " full_bellman " << options.fullBellman << '\n';
}

void self_test() {
    const std::string path = "/tmp/ultimate-bishop-ghost-preserver-" +
                             std::to_string(::getpid());
    {
        std::ofstream log(path + ".log");
        log << "ghost_extra_external_iteration 1 bdd_nodes 20 changed_owner 1 changed_observer 0 changed_visible 1 peak_rss_bytes 1 elapsed 1s\n"
            << "ghost_extra_external_compaction iteration 1 roots 2 marked_nodes 4 copied_nodes 4 structural_residual 0 root_residual 0\n"
            << "ghost_extra_external_iteration 2 bdd_nodes 24 changed_owner 0 changed_observer 0 changed_visible 0 peak_rss_bytes 1 elapsed 2s\n"
            << "information_symbolic_certificate iterations 2 bdd_nodes 24 bellman_residual 0 monotonicity_residual 0 singleton_residual 0 compaction_root_residual 0 belief_cap none powerset_exact 1\n"
            << "information_summary side 0 win 1 loss 0 draw 0 unreachable_win 0 unreachable_loss 0 unreachable_draw 0 sets 1 concrete 1 bellman_residual 0 rank_residual 0 belief_cap none exhaustive 1\n"
            << "information_summary side 1 win 1 loss 0 draw 0 unreachable_win 0 unreachable_loss 0 unreachable_draw 0 sets 1 concrete 1 bellman_residual 0 rank_residual 0 belief_cap none exhaustive 1\n"
            << "ghost_extra_external_root_conservation admitted 2 total 2 independent_grouping_residual 0 realization_residual 0 conservation_residual 0\n"
            << "information_overlay /tmp/test.ufiw bytes 160 source_sha256 "
            << std::string(64, '1') << " solver_model_sha256 "
            << std::string(64, '2') << " observation_model_sha256 "
            << std::string(64, '3')
            << " root_grouping_residual 0 conservation_residual 0\n";
    }
    const ProofLog proof = parse_proof_log(path + ".log");
    if (proof.iteration != 2 || proof.compactions != 1 || proof.arena != 'b' ||
        proof.arenaNodes != 24)
        fail("proof-log parser self-test residual");
    Manifest original;
    original.values["a"] = "1";
    original.values["b"] = "2";
    write_manifest(path + ".manifest", original);
    if (read_manifest(path + ".manifest").values != original.values)
        fail("manifest round-trip residual");
    {
        std::ofstream arenaFile(path + ".nodes", std::ios::binary);
        write_node(arenaFile, DiskNode{Variables, 0, 0});
        write_node(arenaFile, DiskNode{Variables, 1, 1});
        write_node(arenaFile, DiskNode{2, 0, 1});
        write_node(arenaFile, DiskNode{1, 0, 2});
        write_node(arenaFile, DiskNode{0, 0, 1});
    }
    ArenaView arena(path + ".nodes", 5);
    std::vector<std::uint8_t> marks(1, 0);
    std::uint64_t marked = 0;
    mark_root(arena, marks, 3, marked);
    if (marked != 4 || (marks[0] & 0x0f) != 0x0f || (marks[0] & 0x10))
        fail("streaming reachability mark self-test residual");
    std::filesystem::remove(path + ".log");
    std::filesystem::remove(path + ".manifest");
    std::filesystem::remove(path + ".nodes");
    std::cout << "bishop_ghost_preservation_self_test residual 0\n";
}

[[nodiscard]] Options parse_options(int argc, char** argv) {
    Options result;
    const auto choose = [&](Options::Command command) {
        if (result.command != Options::Command::None)
            fail("choose exactly one command");
        result.command = command;
    };
    for (int index = 1; index < argc; ++index) {
        const std::string option = argv[index];
        const auto value = [&]() {
            if (++index >= argc) fail(option + " needs a value");
            return std::string(argv[index]);
        };
        if (option == "--export") choose(Options::Command::Export);
        else if (option == "--verify") choose(Options::Command::Verify);
        else if (option == "--self-test") choose(Options::Command::SelfTest);
        else if (option == "--full-bellman") result.fullBellman = true;
        else if (option == "--scratch") result.scratch = value();
        else if (option == "--transitions") result.transitions = value();
        else if (option == "--source") result.source = value();
        else if (option == "--lower") result.lower = value();
        else if (option == "--solve-log") result.solveLog = value();
        else if (option == "--transition-archive")
            result.transitionArchive = value();
        else if (option == "--overlay") result.overlay = value();
        else if (option == "--input") result.input = value();
        else if (option == "--output") result.output = value();
        else if (option == "--manifest") result.manifest = value();
        else if (option == "--verify-scratch") result.verifyScratch = value();
        else if (option == "--source-sha256") result.sourceSha = value();
        else if (option == "--model-sha256") result.modelSha = value();
        else if (option == "--observation-sha256")
            result.observationSha = value();
        else if (option == "--transition-archive-sha256")
            result.transitionArchiveSha = value();
        else fail("unknown option " + option);
    }
    if (result.command == Options::Command::None) fail("missing command");
    return result;
}

}  // namespace
}  // namespace Stockfish::Ultimate::BishopGhostPreservation

int main(int argc, char** argv) {
    try {
        using namespace Stockfish::Ultimate::BishopGhostPreservation;
        const auto options = parse_options(argc, argv);
        if (options.command == Options::Command::Export)
            export_sidecar(options);
        else if (options.command == Options::Command::Verify)
            verify_sidecar(options);
        else
            self_test();
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
