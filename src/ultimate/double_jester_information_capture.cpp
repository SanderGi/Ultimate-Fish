/*
  Ultimate Fish - persistent exact K+2 Jesters-v-K information capture
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.

  The ordinary proof kernel intentionally keeps its multi-gigabyte fixed-point
  arena anonymous.  This isolated capture kernel reuses that exact model but
  keeps an authenticated recovery arena and writes a compact, portable view of
  every reachable private information class.  It is deliberately a separate
  translation unit and model fingerprint: changing preservation machinery must
  never rebind an already-running proof.
*/

#include <cerrno>
#include <filesystem>
#include <system_error>

#include <sys/stat.h>
#include <unistd.h>

// Reuse the audited domain codec, closure, observation model, lower-overlay
// probes, SHA-256 implementation, and exhaustive codec tests.  Renaming the
// original entry point keeps the capture CLI independent.
#define main ultimate_double_jester_nonpersistent_main
#include "double_jester_information_tablebase.cpp"
#undef main

namespace {

std::vector<std::string> CaptureScratchPaths;

// information_solver.cpp calls ::unlink immediately after mkstemp.  The
// capture-only copy below redirects that one call here.  Files stay named and
// are later renamed by creation order into a stable recovery schema.
int ultimate_capture_keep_file(const char* path) {
    CaptureScratchPaths.emplace_back(path);
    return 0;
}

}  // namespace

// Declare a namespace-isolated copy of InformationFixedPoint.  Its
// implementation is compiled directly from the shared audited solver below;
// only unlink is redirected, so equations, propagation, and verification stay
// byte-for-byte source-identical to the normal solver.
namespace CaptureStockfish::Ultimate {

using InformationToken = Stockfish::Ultimate::InformationToken;
inline constexpr InformationToken InformationFalse =
  Stockfish::Ultimate::InformationFalse;
inline constexpr InformationToken InformationTrue =
  Stockfish::Ultimate::InformationTrue;
inline constexpr std::uint32_t InformationNoWitness =
  Stockfish::Ultimate::InformationNoWitness;
using InformationPair = Stockfish::Ultimate::InformationPair;
using InformationEquationKind = Stockfish::Ultimate::InformationEquationKind;
using InformationSolveSummary = Stockfish::Ultimate::InformationSolveSummary;

class InformationFixedPoint {
  public:
    InformationFixedPoint(std::uint32_t variableCount,
                          std::string scratchDirectory);
    ~InformationFixedPoint();

    InformationFixedPoint(const InformationFixedPoint&) = delete;
    InformationFixedPoint& operator=(const InformationFixedPoint&) = delete;
    InformationFixedPoint(InformationFixedPoint&&) noexcept;
    InformationFixedPoint& operator=(InformationFixedPoint&&) noexcept;

    void define_or(std::uint32_t parent, const InformationToken* children,
                   std::size_t count);
    void define_and(std::uint32_t parent, const InformationToken* children,
                    std::size_t count);
    void define_or_of_pairs(std::uint32_t parent, const InformationPair* pairs,
                            std::size_t count);

    void define_or(std::uint32_t parent,
                   const std::vector<InformationToken>& children) {
        define_or(parent, children.data(), children.size());
    }
    void define_and(std::uint32_t parent,
                    const std::vector<InformationToken>& children) {
        define_and(parent, children.data(), children.size());
    }
    void define_or_of_pairs(std::uint32_t parent,
                            const std::vector<InformationPair>& pairs) {
        define_or_of_pairs(parent, pairs.data(), pairs.size());
    }

    [[nodiscard]] InformationSolveSummary solve();
    [[nodiscard]] InformationSolveSummary verify() const;
    [[nodiscard]] bool value(InformationToken token) const;
    [[nodiscard]] std::uint32_t activation_rank(std::uint32_t variable) const;
    [[nodiscard]] std::uint32_t witness_index(std::uint32_t variable) const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace CaptureStockfish::Ultimate

#define Stockfish CaptureStockfish
#define unlink ultimate_capture_keep_file
#include "information_solver.cpp"
#undef unlink
#undef Stockfish

namespace Stockfish::Ultimate {
namespace {

using CaptureFixedPoint = CaptureStockfish::Ultimate::InformationFixedPoint;

constexpr std::uint32_t CaptureFormatVersion = 2;
constexpr std::uint32_t CaptureHeaderBytes = 1024;
constexpr std::uint64_t DefaultRequiredFreeBytes = 100ULL << 30;
constexpr std::uint64_t DefaultMaximumRawBytes = 100ULL << 30;

struct CaptureOptions {
    std::string input = "tablebases/kjesterjesterk.uftb";
    std::string lowerOverlay;
    std::string lowerConcrete;
    std::string lowerSourceSha256;
    std::string lowerModelSha256;
    std::string output;
    std::string sidecar;
    std::string rawDirectory;
    std::string expectedOverlay;
    std::string sourceSha256;
    std::string modelSha256;
    std::string probeSidecar;
    std::vector<std::uint32_t> probeWorlds;
    std::uint32_t probeActual = NoActual;
    std::uint64_t requiredFreeBytes = DefaultRequiredFreeBytes;
    std::uint64_t maximumRawBytes = DefaultMaximumRawBytes;
    bool selfTestOnly = false;
    bool estimateOnly = false;
};

[[nodiscard]] std::uint64_t parse_u64(const std::string& text,
                                      const char* option) {
    std::size_t consumed = 0;
    const unsigned long long value = std::stoull(text, &consumed);
    if (consumed != text.size())
        throw std::runtime_error(std::string("invalid value for ") + option);
    return static_cast<std::uint64_t>(value);
}

[[nodiscard]] std::array<std::uint8_t, 32> sha256_path(
  const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("cannot hash capture file: " + path.string());
    Sha256 hasher;
    std::array<std::uint8_t, 1 << 20> buffer{};
    while (input) {
        input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        const std::streamsize count = input.gcount();
        if (count > 0)
            hasher.update(buffer.data(), static_cast<std::size_t>(count));
    }
    return hasher.finish();
}

[[nodiscard]] std::array<std::uint8_t, 32> sha256_payload(
  const std::filesystem::path& path, std::uint64_t offset) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("cannot hash capture payload: " + path.string());
    input.seekg(static_cast<std::streamoff>(offset));
    if (!input)
        throw std::runtime_error("capture payload offset is outside file");
    Sha256 hasher;
    std::array<std::uint8_t, 1 << 20> buffer{};
    while (input) {
        input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        const std::streamsize count = input.gcount();
        if (count > 0)
            hasher.update(buffer.data(), static_cast<std::size_t>(count));
    }
    return hasher.finish();
}

[[nodiscard]] std::string overlay_model_sha256(
  const std::filesystem::path& path, const std::string& expectedSource) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("cannot inspect bound overlay: " + path.string());
    std::array<char, 160> header{};
    input.read(header.data(), header.size());
    if (static_cast<std::size_t>(input.gcount()) != header.size() ||
        std::memcmp(header.data(), "UFIW2\0\0\0", 8) != 0 ||
        std::string(header.data() + 32, 64) != expectedSource)
        throw std::runtime_error("bound overlay source/header mismatch");
    const std::string model(header.data() + 96, 64);
    if (!valid_sha256(model))
        throw std::runtime_error("bound overlay has invalid model SHA-256");
    return model;
}

void require_capture_resources(const CaptureOptions& options) {
    if (options.rawDirectory.empty())
        throw std::runtime_error("capture requires --raw-directory");
    const std::filesystem::path raw(options.rawDirectory);
    std::error_code error;
    std::filesystem::create_directories(raw, error);
    if (error)
        throw std::runtime_error("cannot create capture raw directory: " +
                                 error.message());
    const std::filesystem::space_info space = std::filesystem::space(raw, error);
    if (error)
        throw std::runtime_error("cannot measure capture filesystem: " +
                                 error.message());
    if (!std::filesystem::is_empty(raw, error) || error)
        throw std::runtime_error(
          "capture raw directory must be an existing empty isolated directory");
    std::cout << "capture_resources available_bytes " << space.available
              << " required_free_bytes " << options.requiredFreeBytes
              << " maximum_raw_bytes " << options.maximumRawBytes << '\n'
              << std::flush;
    if (space.available < options.requiredFreeBytes)
        throw std::runtime_error(
          "capture filesystem is below --required-free-bytes; use an AWS volume");
}

}  // namespace
}  // namespace Stockfish::Ultimate

namespace Stockfish::Ultimate {
namespace {

struct CaptureLayout {
    std::vector<std::uint64_t> whiteBase;
    std::vector<std::uint64_t> auxiliaryBase;
    std::vector<std::uint8_t> commonCounts;
    std::uint64_t blackBase = 0;
    std::uint64_t auxiliaryStart = 0;
    std::uint64_t variableCount = 0;
    std::uint64_t softLocks = 0;
};

struct CaptureSolved {
    std::vector<std::uint8_t> freshFlags;
    InformationSolveSummary summary;
    std::uint64_t whiteMemberships = 0;
    std::uint64_t blackBeliefs = 0;
    std::uint64_t variables = 0;
    std::uint64_t softLocks = 0;
    std::uint64_t domainBellmanResidual = 0;
    std::uint64_t dualWinResidual = 0;
};

struct CaptureSection {
    std::uint64_t offset = 0;
    std::uint64_t bytes = 0;
    std::array<std::uint8_t, 32> sha{};
};

// UFICAP2 is explicitly little-endian. Its 1,024-byte authenticated header
// binds the concrete table, original proof model/dense result, capture model,
// lower concrete/model/overlay, legal-dot-v2 semantics, exact counts, and five
// hashed sections in this order:
//   1. variable-length canonical BeliefKeys (u8 count + count*u32 worlds),
//   2. u32 fresh-root-to-belief map (NoBelief means exact singleton),
//   3. bitpacked concrete-root admission flags,
//   4. bitpacked Ivory-owner sure wins per (belief, actual-world ordinal),
//   5. bitpacked bare-King sure wins per belief.
// Thus a probe can recover every non-singleton private partition and both
// exact target values without the multi-gigabyte fixed-point arena. Concrete
// singleton outcomes remain bound to, and are probed from, the exact UFTB.

class HashedSectionWriter {
  public:
    explicit HashedSectionWriter(std::ofstream& output) : output_(output) {
        const std::streampos position = output_.tellp();
        if (position < 0)
            throw std::runtime_error("cannot measure capture section offset");
        section_.offset = static_cast<std::uint64_t>(position);
        buffer_.reserve(1 << 20);
    }

    void byte(std::uint8_t value) {
        buffer_.push_back(value);
        flush_if_full();
    }

    void u32(std::uint32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8)
            byte(static_cast<std::uint8_t>(value >> shift));
    }

    void bytes(const std::uint8_t* data, std::size_t count) {
        while (count) {
            const std::size_t room = (1 << 20) - buffer_.size();
            const std::size_t take = std::min(room, count);
            buffer_.insert(buffer_.end(), data, data + take);
            data += take;
            count -= take;
            flush_if_full();
        }
    }

    [[nodiscard]] CaptureSection finish() {
        flush();
        section_.sha = sha_.finish();
        return section_;
    }

  private:
    void flush_if_full() {
        if (buffer_.size() == (1 << 20))
            flush();
    }

    void flush() {
        if (buffer_.empty())
            return;
        output_.write(reinterpret_cast<const char*>(buffer_.data()),
                      static_cast<std::streamsize>(buffer_.size()));
        if (!output_)
            throw std::runtime_error("cannot write capture section");
        sha_.update(buffer_.data(), buffer_.size());
        section_.bytes += buffer_.size();
        buffer_.clear();
    }

    std::ofstream& output_;
    CaptureSection section_;
    Sha256 sha_;
    std::vector<std::uint8_t> buffer_;
};

class BitWriter {
  public:
    explicit BitWriter(HashedSectionWriter& output) : output_(output) {}

    void bit(bool value) {
        if (value)
            byte_ |= static_cast<std::uint8_t>(1u << used_);
        ++used_;
        ++bits_;
        if (used_ == 8) {
            output_.byte(byte_);
            byte_ = 0;
            used_ = 0;
        }
    }

    void finish() {
        if (used_)
            output_.byte(byte_);
    }

    [[nodiscard]] std::uint64_t bits() const { return bits_; }

  private:
    HashedSectionWriter& output_;
    std::uint8_t byte_ = 0;
    unsigned used_ = 0;
    std::uint64_t bits_ = 0;
};

void store_le32(std::vector<std::uint8_t>& bytes, std::size_t offset,
                std::uint32_t value) {
    if (offset + 4 > bytes.size())
        throw std::runtime_error("capture header u32 overflow");
    for (unsigned shift = 0; shift < 32; shift += 8)
        bytes[offset++] = static_cast<std::uint8_t>(value >> shift);
}

void store_le64(std::vector<std::uint8_t>& bytes, std::size_t offset,
                std::uint64_t value) {
    if (offset + 8 > bytes.size())
        throw std::runtime_error("capture header u64 overflow");
    for (unsigned shift = 0; shift < 64; shift += 8)
        bytes[offset++] = static_cast<std::uint8_t>(value >> shift);
}

[[nodiscard]] std::array<std::uint8_t, 32> decode_sha256(
  const std::string& text, const char* label) {
    if (!valid_sha256(text))
        throw std::runtime_error(std::string("invalid ") + label + " SHA-256");
    const auto nybble = [](char value) -> std::uint8_t {
        if (value >= '0' && value <= '9')
            return static_cast<std::uint8_t>(value - '0');
        if (value >= 'a' && value <= 'f')
            return static_cast<std::uint8_t>(value - 'a' + 10);
        if (value >= 'A' && value <= 'F')
            return static_cast<std::uint8_t>(value - 'A' + 10);
        throw std::runtime_error("non-hex SHA-256 digit");
    };
    std::array<std::uint8_t, 32> digest{};
    for (std::size_t index = 0; index < digest.size(); ++index)
        digest[index] = static_cast<std::uint8_t>(
          (nybble(text[2 * index]) << 4) | nybble(text[2 * index + 1]));
    return digest;
}

void store_digest(std::vector<std::uint8_t>& bytes, std::size_t offset,
                  const std::array<std::uint8_t, 32>& digest) {
    if (offset + digest.size() > bytes.size())
        throw std::runtime_error("capture header digest overflow");
    std::copy(digest.begin(), digest.end(), bytes.begin() + offset);
}

void store_text(std::vector<std::uint8_t>& bytes, std::size_t offset,
                std::size_t width, const std::string& text) {
    if (text.size() >= width || offset + width > bytes.size())
        throw std::runtime_error("capture header text overflow");
    std::copy(text.begin(), text.end(), bytes.begin() + offset);
}

void store_section(std::vector<std::uint8_t>& header, std::size_t offset,
                   const CaptureSection& section) {
    store_le64(header, offset, section.offset);
    store_le64(header, offset + 8, section.bytes);
    store_digest(header, offset + 16, section.sha);
}

[[nodiscard]] bool same_file_payload(const std::filesystem::path& first,
                                     const std::filesystem::path& second,
                                     std::uint64_t offset) {
    std::error_code error;
    const std::uint64_t firstSize = std::filesystem::file_size(first, error);
    if (error)
        return false;
    const std::uint64_t secondSize = std::filesystem::file_size(second, error);
    if (error || firstSize != secondSize || firstSize < offset)
        return false;
    std::ifstream lhs(first, std::ios::binary);
    std::ifstream rhs(second, std::ios::binary);
    lhs.seekg(static_cast<std::streamoff>(offset));
    rhs.seekg(static_cast<std::streamoff>(offset));
    std::array<char, 1 << 20> left{};
    std::array<char, 1 << 20> right{};
    while (lhs && rhs) {
        lhs.read(left.data(), left.size());
        rhs.read(right.data(), right.size());
        if (lhs.gcount() != rhs.gcount() ||
            std::memcmp(left.data(), right.data(),
                        static_cast<std::size_t>(lhs.gcount())) != 0)
            return false;
    }
    return lhs.eof() && rhs.eof();
}

[[nodiscard]] std::uint32_t load_le32(const std::uint8_t* bytes) {
    std::uint32_t value = 0;
    for (unsigned shift = 0; shift < 32; shift += 8)
        value |= std::uint32_t(*bytes++) << shift;
    return value;
}

[[nodiscard]] std::uint64_t load_le64(const std::uint8_t* bytes) {
    std::uint64_t value = 0;
    for (unsigned shift = 0; shift < 64; shift += 8)
        value |= std::uint64_t(*bytes++) << shift;
    return value;
}

[[nodiscard]] std::array<std::uint8_t, 32> sha256_range(
  const std::filesystem::path& path, std::uint64_t offset,
  std::uint64_t bytes) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("cannot hash capture range: " + path.string());
    input.seekg(static_cast<std::streamoff>(offset));
    if (!input)
        throw std::runtime_error("capture hash range offset is invalid");
    Sha256 hasher;
    std::array<std::uint8_t, 1 << 20> buffer{};
    while (bytes) {
        const std::size_t take = static_cast<std::size_t>(
          std::min<std::uint64_t>(bytes, buffer.size()));
        input.read(reinterpret_cast<char*>(buffer.data()),
                   static_cast<std::streamsize>(take));
        if (static_cast<std::size_t>(input.gcount()) != take)
            throw std::runtime_error("capture hash range is truncated");
        hasher.update(buffer.data(), take);
        bytes -= take;
    }
    return hasher.finish();
}

[[nodiscard]] CaptureLayout make_capture_layout(DoubleJesterGraph& graph) {
    const auto& nodes = graph.nodes();
    CaptureLayout layout;
    layout.whiteBase.resize(nodes.size() + 1, 0);
    layout.auxiliaryBase.resize(nodes.size() + 1, 0);
    layout.commonCounts.resize(nodes.size(), 0);
    std::uint32_t firstSoftLock = NoBelief;
    for (std::size_t belief = 0; belief < nodes.size(); ++belief)
        layout.whiteBase[belief + 1] =
          layout.whiteBase[belief] + belief_size(nodes[belief]);
    layout.blackBase = layout.whiteBase.back();
    layout.auxiliaryStart = layout.blackBase + nodes.size();
    for (std::size_t belief = 0; belief < nodes.size(); ++belief) {
        const auto worlds = graph.worlds(static_cast<std::uint32_t>(belief));
        if (decode_index(base_index(worlds.front())).side == Color::Black) {
            const Position first = make_oriented_position(worlds.front());
            if (!first.game_over()) {
                layout.commonCounts[belief] = graph.common_action_count(
                  static_cast<std::uint32_t>(belief));
                if (!layout.commonCounts[belief]) {
                    ++layout.softLocks;
                    if (firstSoftLock == NoBelief)
                        firstSoftLock = static_cast<std::uint32_t>(belief);
                }
            }
        }
        layout.auxiliaryBase[belief + 1] =
          layout.auxiliaryBase[belief] + layout.commonCounts[belief];
        if ((belief + 1) % 500'000 == 0)
            std::cout << "capture_action_scan " << belief + 1 << '/'
                      << nodes.size() << " common_actions "
                      << layout.auxiliaryBase[belief + 1] << " soft_locks "
                      << layout.softLocks << '\n' << std::flush;
    }
    layout.variableCount = layout.auxiliaryStart + layout.auxiliaryBase.back();
    if (!layout.variableCount || layout.variableCount >= InformationTrue)
        throw std::runtime_error("capture information variables exceed token domain");
    std::cout << "capture_soft_locks sets " << layout.softLocks
              << " first_belief ";
    if (firstSoftLock == NoBelief)
        std::cout << "none\n";
    else
        std::cout << firstSoftLock << " example "
                  << make_oriented_position(
                       graph.worlds(firstSoftLock).front()).upn()
                  << '\n';
    std::cout << "capture_equations white_variables " << layout.blackBase
              << " black_variables " << nodes.size()
              << " action_and_variables " << layout.auxiliaryBase.back()
              << " total " << layout.variableCount << '\n' << std::flush;
    return layout;
}

[[nodiscard]] InformationToken capture_white_variable(
  const CaptureLayout& layout, std::uint32_t belief, std::size_t ordinal) {
    return static_cast<InformationToken>(layout.whiteBase.at(belief) + ordinal);
}

[[nodiscard]] InformationToken capture_black_variable(
  const CaptureLayout& layout, std::uint32_t belief) {
    return static_cast<InformationToken>(layout.blackBase + belief);
}

[[nodiscard]] InformationToken capture_auxiliary_variable(
  const CaptureLayout& layout, std::uint32_t belief, std::size_t action) {
    return static_cast<InformationToken>(layout.auxiliaryStart +
      layout.auxiliaryBase.at(belief) + action);
}

[[nodiscard]] std::size_t capture_actual_ordinal(
  DoubleJesterGraph& graph, const Successor& successor) {
    const auto worlds = graph.worlds(successor.belief);
    const auto found = std::lower_bound(
      worlds.begin(), worlds.end(), successor.actual);
    if (found == worlds.end() || *found != successor.actual)
        throw std::runtime_error("capture successor omits actual world");
    return static_cast<std::size_t>(found - worlds.begin());
}

[[nodiscard]] InformationToken capture_white_token(
  DoubleJesterGraph& graph, const CaptureLayout& layout,
  const Successor& successor) {
    return successor.belief == NoBelief
         ? (successor.exactWhite ? InformationTrue : InformationFalse)
         : capture_white_variable(
             layout, successor.belief,
             capture_actual_ordinal(graph, successor));
}

[[nodiscard]] InformationToken capture_black_token(
  const CaptureLayout& layout, const Successor& successor) {
    return successor.belief == NoBelief
         ? (successor.exactBlack ? InformationTrue : InformationFalse)
         : capture_black_variable(layout, successor.belief);
}

void define_capture_equations(DoubleJesterGraph& graph,
                              const CaptureLayout& layout,
                              CaptureFixedPoint& solver) {
    const auto& nodes = graph.nodes();
    std::vector<InformationToken> tokens;
    for (std::uint32_t belief = 0; belief < nodes.size(); ++belief) {
        const GeneratedNode node = graph.regenerate(belief);
        if (node.terminal) {
            for (std::size_t world = 0; world < node.worlds.size(); ++world) {
                const InformationToken value = node.terminalWhite[world]
                                             ? InformationTrue : InformationFalse;
                solver.define_or(capture_white_variable(layout, belief, world),
                                 &value, 1);
            }
            const InformationToken value = node.terminalBlack
                                         ? InformationTrue : InformationFalse;
            solver.define_or(capture_black_variable(layout, belief), &value, 1);
        }
        else if (node.mover == Color::White) {
            tokens.clear();
            for (std::size_t world = 0; world < node.worlds.size(); ++world) {
                std::vector<InformationToken> choices;
                for (std::size_t edge = node.informedOffsets[world];
                     edge < node.informedOffsets[world + 1]; ++edge) {
                    choices.push_back(capture_white_token(
                      graph, layout, node.informedEdges[edge]));
                    tokens.push_back(capture_black_token(
                      layout, node.informedEdges[edge]));
                }
                solver.define_or(capture_white_variable(layout, belief, world),
                                 choices);
            }
            solver.define_and(capture_black_variable(layout, belief), tokens);
        }
        else {
            if (node.commonActionCount != layout.commonCounts[belief])
                throw std::runtime_error(
                  "capture uniform action count changed during regeneration");
            for (std::size_t world = 0; world < node.worlds.size(); ++world) {
                tokens.clear();
                for (std::size_t action = 0;
                     action < node.commonActionCount; ++action)
                    tokens.push_back(capture_white_token(
                      graph, layout, node.commonEdges[
                        action * node.worlds.size() + world]));
                solver.define_and(capture_white_variable(layout, belief, world),
                                  tokens);
            }
            std::vector<InformationToken> actionGates;
            for (std::size_t action = 0; action < node.commonActionCount; ++action) {
                tokens.clear();
                for (std::size_t world = 0; world < node.worlds.size(); ++world)
                    tokens.push_back(capture_black_token(
                      layout, node.commonEdges[
                        action * node.worlds.size() + world]));
                const InformationToken gate = capture_auxiliary_variable(
                  layout, belief, action);
                solver.define_and(gate, tokens);
                actionGates.push_back(gate);
            }
            solver.define_or(capture_black_variable(layout, belief), actionGates);
        }
        if ((belief + 1) % 500'000 == 0)
            std::cout << "capture_equations_defined " << belief + 1 << '/'
                      << nodes.size() << '\n' << std::flush;
    }
}

[[nodiscard]] bool capture_token_value(const CaptureFixedPoint& solver,
                                       InformationToken token) {
    if (token == InformationTrue)
        return true;
    if (token == InformationFalse)
        return false;
    return solver.value(token);
}

[[nodiscard]] std::pair<std::uint64_t, std::uint64_t>
verify_capture_domain(DoubleJesterGraph& graph, const CaptureLayout& layout,
                      const CaptureFixedPoint& solver) {
    std::uint64_t bellmanResidual = 0;
    std::uint64_t dualWinResidual = 0;
    const auto& nodes = graph.nodes();
    for (std::uint32_t belief = 0; belief < nodes.size(); ++belief) {
        const GeneratedNode node = graph.regenerate(belief);
        bool expectedBlack = false;
        if (node.terminal) {
            expectedBlack = node.terminalBlack;
            for (std::size_t world = 0; world < node.worlds.size(); ++world)
                bellmanResidual += solver.value(capture_white_variable(
                  layout, belief, world)) != bool(node.terminalWhite[world]);
        }
        else if (node.mover == Color::White) {
            expectedBlack = true;
            for (std::size_t world = 0; world < node.worlds.size(); ++world) {
                bool expectedWhite = false;
                for (std::size_t edge = node.informedOffsets[world];
                     edge < node.informedOffsets[world + 1]; ++edge) {
                    expectedWhite = expectedWhite || capture_token_value(
                      solver, capture_white_token(
                        graph, layout, node.informedEdges[edge]));
                    expectedBlack = expectedBlack && capture_token_value(
                      solver, capture_black_token(
                        layout, node.informedEdges[edge]));
                }
                bellmanResidual += solver.value(capture_white_variable(
                  layout, belief, world)) != expectedWhite;
            }
        }
        else {
            expectedBlack = false;
            for (std::size_t world = 0; world < node.worlds.size(); ++world) {
                bool expectedWhite = true;
                for (std::size_t action = 0;
                     action < node.commonActionCount; ++action)
                    expectedWhite = expectedWhite && capture_token_value(
                      solver, capture_white_token(
                        graph, layout, node.commonEdges[
                          action * node.worlds.size() + world]));
                bellmanResidual += solver.value(capture_white_variable(
                  layout, belief, world)) != expectedWhite;
            }
            for (std::size_t action = 0; action < node.commonActionCount; ++action) {
                bool expectedGate = true;
                for (std::size_t world = 0; world < node.worlds.size(); ++world)
                    expectedGate = expectedGate && capture_token_value(
                      solver, capture_black_token(
                        layout, node.commonEdges[
                          action * node.worlds.size() + world]));
                bellmanResidual += solver.value(capture_auxiliary_variable(
                  layout, belief, action)) != expectedGate;
                expectedBlack = expectedBlack || expectedGate;
            }
        }
        const bool black = solver.value(capture_black_variable(layout, belief));
        bellmanResidual += black != expectedBlack;
        for (std::size_t world = 0; world < node.worlds.size(); ++world)
            dualWinResidual += black && solver.value(capture_white_variable(
              layout, belief, world));
        if ((belief + 1) % 500'000 == 0)
            std::cout << "capture_domain_verified " << belief + 1 << '/'
                      << nodes.size() << " bellman_residual " << bellmanResidual
                      << " dual_win_residual " << dualWinResidual << '\n'
                      << std::flush;
    }
    return {bellmanResidual, dualWinResidual};
}

[[nodiscard]] std::vector<std::uint8_t> capture_fresh_flags(
  DoubleJesterGraph& graph, const CaptureLayout& layout,
  const CaptureFixedPoint& solver) {
    std::vector<std::uint8_t> flags(ConcreteStateCount, 0);
    for (std::uint32_t index = 0; index < ConcreteStateCount; ++index) {
        if (!graph.admitted()[index])
            continue;
        const std::uint32_t belief = graph.roots()[index];
        bool white = false;
        bool black = false;
        if (belief == NoBelief) {
            white = graph.exact_index_forces(index, Color::White);
            black = graph.exact_index_forces(index, Color::Black);
        }
        else {
            const auto worlds = graph.worlds(belief);
            const auto found = std::find_if(worlds.begin(), worlds.end(),
              [&](std::uint32_t world) { return base_index(world) == index; });
            if (found == worlds.end())
                throw std::runtime_error("capture root belief omits actual world");
            const std::size_t ordinal =
              static_cast<std::size_t>(found - worlds.begin());
            white = solver.value(capture_white_variable(layout, belief, ordinal));
            black = solver.value(capture_black_variable(layout, belief));
        }
        if (white && black)
            throw std::runtime_error("capture fresh root is a dual forced win");
        flags[index] = static_cast<std::uint8_t>(
          4 | (white ? 1 : 0) | (black ? 2 : 0));
    }
    return flags;
}

[[nodiscard]] CaptureSection write_belief_keys(std::ofstream& output,
                                               DoubleJesterGraph& graph) {
    HashedSectionWriter writer(output);
    for (const BeliefKey& key : graph.nodes()) {
        if (key.count < 2 || key.count > 3)
            throw std::runtime_error("capture belief key has invalid cardinality");
        writer.byte(static_cast<std::uint8_t>(key.count));
        for (std::uint32_t ordinal = 0; ordinal < key.count; ++ordinal)
            writer.u32(key.worlds[ordinal]);
    }
    return writer.finish();
}

[[nodiscard]] CaptureSection write_root_map(std::ofstream& output,
                                            DoubleJesterGraph& graph) {
    HashedSectionWriter writer(output);
    for (const std::uint32_t belief : graph.roots())
        writer.u32(belief);
    return writer.finish();
}

[[nodiscard]] CaptureSection write_admitted(std::ofstream& output,
                                            DoubleJesterGraph& graph) {
    HashedSectionWriter writer(output);
    BitWriter bits(writer);
    for (const std::uint8_t value : graph.admitted())
        bits.bit(value != 0);
    bits.finish();
    if (bits.bits() != ConcreteStateCount)
        throw std::runtime_error("capture admitted bit count mismatch");
    return writer.finish();
}

[[nodiscard]] CaptureSection write_white_values(
  std::ofstream& output, DoubleJesterGraph& graph,
  const CaptureLayout& layout, const CaptureFixedPoint& solver) {
    HashedSectionWriter writer(output);
    BitWriter bits(writer);
    for (std::uint32_t belief = 0; belief < graph.nodes().size(); ++belief)
        for (std::size_t world = 0; world < belief_size(graph.nodes()[belief]);
             ++world)
            bits.bit(solver.value(capture_white_variable(layout, belief, world)));
    bits.finish();
    if (bits.bits() != layout.blackBase)
        throw std::runtime_error("capture White value bit count mismatch");
    return writer.finish();
}

[[nodiscard]] CaptureSection write_black_values(
  std::ofstream& output, DoubleJesterGraph& graph,
  const CaptureLayout& layout, const CaptureFixedPoint& solver) {
    HashedSectionWriter writer(output);
    BitWriter bits(writer);
    for (std::uint32_t belief = 0; belief < graph.nodes().size(); ++belief)
        bits.bit(solver.value(capture_black_variable(layout, belief)));
    bits.finish();
    if (bits.bits() != graph.nodes().size())
        throw std::runtime_error("capture Black value bit count mismatch");
    return writer.finish();
}

[[nodiscard]] std::uint64_t admitted_count(DoubleJesterGraph& graph) {
    return static_cast<std::uint64_t>(std::count(
      graph.admitted().begin(), graph.admitted().end(), std::uint8_t{1}));
}

void write_capture_sidecar(
  const std::filesystem::path& path, DoubleJesterGraph& graph,
  const CaptureLayout& layout, const CaptureFixedPoint& solver,
  const CaptureSolved& solved, const CaptureOptions& options,
  const std::array<std::uint8_t, 32>& lowerPayloadSha) {
    if (path.empty())
        throw std::runtime_error("capture requires --sidecar");
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        throw std::runtime_error("cannot create capture sidecar: " + path.string());
    std::vector<std::uint8_t> header(CaptureHeaderBytes, 0);
    output.write(reinterpret_cast<const char*>(header.data()), header.size());
    if (!output)
        throw std::runtime_error("cannot reserve capture sidecar header");

    const CaptureSection keys = write_belief_keys(output, graph);
    const CaptureSection roots = write_root_map(output, graph);
    const CaptureSection admitted = write_admitted(output, graph);
    const CaptureSection white = write_white_values(
      output, graph, layout, solver);
    const CaptureSection black = write_black_values(
      output, graph, layout, solver);

    static constexpr std::array<char, 8> Magic{{
      'U', 'F', 'I', 'C', 'A', 'P', '2', '\0'}};
    std::copy(Magic.begin(), Magic.end(), header.begin());
    store_le32(header, 8, CaptureFormatVersion);
    store_le32(header, 12, CaptureHeaderBytes);
    store_le32(header, 16, 1);  // exhaustive, no cap, exact sure-win pair
    store_le32(header, 20, 5);
    store_le32(header, 24, 0x01020304);
    store_text(header, 32, 32, "kjesterjesterk");
    store_text(header, 64, 64, "fresh-maximal-public-view-v2");
    store_digest(header, 128,
                 decode_sha256(options.sourceSha256, "concrete source"));
    store_digest(header, 160,
                 decode_sha256(options.modelSha256, "capture model"));
    store_digest(header, 192,
                 decode_sha256(options.lowerSourceSha256, "lower source"));
    store_digest(header, 224,
                 decode_sha256(options.lowerModelSha256, "lower model"));
    store_digest(header, 256, lowerPayloadSha);
    store_le64(header, 288, ConcreteStateCount);
    store_le64(header, 296, graph.nodes().size());
    store_le64(header, 304, layout.blackBase);
    store_le64(header, 312, graph.roots().size());
    store_le64(header, 320, admitted_count(graph));
    store_le64(header, 328, layout.variableCount);
    store_le64(header, 336, solved.summary.reverseEdges);
    store_le64(header, 344, solved.summary.activated);
    store_le64(header, 352, solved.summary.bellmanResidual);
    store_le64(header, 360, solved.summary.rankResidual);
    store_le64(header, 368, solved.domainBellmanResidual);
    store_le64(header, 376, solved.dualWinResidual);
    store_le64(header, 384, solved.softLocks);
    store_le64(header, 392, std::numeric_limits<std::uint64_t>::max());
    store_section(header, 416, keys);
    store_section(header, 464, roots);
    store_section(header, 512, admitted);
    store_section(header, 560, white);
    store_section(header, 608, black);
    store_digest(header, 672, decode_sha256(
      overlay_model_sha256(options.expectedOverlay, options.sourceSha256),
      "original proof model"));
    store_digest(header, 704, sha256_path(options.expectedOverlay));
    store_digest(header, 736, sha256_path(options.lowerOverlay));
    store_digest(header, 768, sha256_path(options.output));
    Sha256 headerHasher;
    headerHasher.update(header.data(), 992);
    store_digest(header, 992, headerHasher.finish());
    output.seekp(0);
    output.write(reinterpret_cast<const char*>(header.data()), header.size());
    output.close();
    if (!output)
        throw std::runtime_error("cannot finalize capture sidecar");

    std::cout << "capture_sidecar path " << path
              << " bytes " << std::filesystem::file_size(path)
              << " beliefs " << graph.nodes().size()
              << " memberships " << layout.blackBase
              << " roots " << graph.roots().size()
              << " sha256 " << hex_digest(sha256_path(path)) << '\n'
              << std::flush;
}

void verify_capture_sidecar(
  const std::filesystem::path& path, DoubleJesterGraph& graph,
  const CaptureLayout& layout, const CaptureFixedPoint& solver,
  const CaptureSolved& solved, const CaptureOptions& options,
  const std::array<std::uint8_t, 32>& lowerPayloadSha) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("cannot reopen capture sidecar");
    std::vector<std::uint8_t> header(CaptureHeaderBytes);
    input.read(reinterpret_cast<char*>(header.data()), header.size());
    if (static_cast<std::size_t>(input.gcount()) != header.size() ||
        std::memcmp(header.data(), "UFICAP2\0", 8) != 0 ||
        load_le32(header.data() + 8) != CaptureFormatVersion ||
        load_le32(header.data() + 12) != CaptureHeaderBytes ||
        load_le32(header.data() + 16) != 1 ||
        load_le32(header.data() + 20) != 5 ||
        load_le32(header.data() + 24) != 0x01020304)
        throw std::runtime_error("capture sidecar header is invalid");
    if (std::string(reinterpret_cast<const char*>(header.data() + 32)) !=
          "kjesterjesterk" ||
        std::string(reinterpret_cast<const char*>(header.data() + 64)) !=
          "fresh-maximal-public-view-v2")
        throw std::runtime_error("capture sidecar domain binding mismatch");
    const auto requireDigest = [&](std::size_t offset,
                                   const std::array<std::uint8_t, 32>& expected,
                                   const char* label) {
        if (!std::equal(expected.begin(), expected.end(),
                        header.begin() + offset))
            throw std::runtime_error(std::string("capture sidecar ") + label +
                                     " digest mismatch");
    };
    requireDigest(128, decode_sha256(options.sourceSha256, "source"), "source");
    requireDigest(160, decode_sha256(options.modelSha256, "model"), "model");
    requireDigest(192, decode_sha256(options.lowerSourceSha256, "lower source"),
                  "lower source");
    requireDigest(224, decode_sha256(options.lowerModelSha256, "lower model"),
                  "lower model");
    requireDigest(256, lowerPayloadSha, "lower payload");
    requireDigest(672, decode_sha256(
      overlay_model_sha256(options.expectedOverlay, options.sourceSha256),
      "original model"), "original model");
    requireDigest(704, sha256_path(options.expectedOverlay),
                  "expected dense overlay");
    requireDigest(736, sha256_path(options.lowerOverlay), "lower overlay");
    requireDigest(768, sha256_path(options.output), "generated dense overlay");
    Sha256 headerHasher;
    headerHasher.update(header.data(), 992);
    requireDigest(992, headerHasher.finish(), "header");
    if (load_le64(header.data() + 288) != ConcreteStateCount ||
        load_le64(header.data() + 296) != graph.nodes().size() ||
        load_le64(header.data() + 304) != layout.blackBase ||
        load_le64(header.data() + 312) != graph.roots().size() ||
        load_le64(header.data() + 320) != admitted_count(graph) ||
        load_le64(header.data() + 328) != layout.variableCount ||
        load_le64(header.data() + 336) != solved.summary.reverseEdges ||
        load_le64(header.data() + 344) != solved.summary.activated ||
        load_le64(header.data() + 352) != 0 ||
        load_le64(header.data() + 360) != 0 ||
        load_le64(header.data() + 368) != 0 ||
        load_le64(header.data() + 376) != 0 ||
        load_le64(header.data() + 384) != solved.softLocks ||
        load_le64(header.data() + 392) !=
          std::numeric_limits<std::uint64_t>::max())
        throw std::runtime_error("capture sidecar count/residual binding mismatch");

    std::array<CaptureSection, 5> sections{};
    for (std::size_t index = 0; index < sections.size(); ++index) {
        const std::size_t offset = 416 + 48 * index;
        sections[index].offset = load_le64(header.data() + offset);
        sections[index].bytes = load_le64(header.data() + offset + 8);
        std::copy(header.begin() + offset + 16,
                  header.begin() + offset + 48, sections[index].sha.begin());
        if (sections[index].offset !=
              (index ? sections[index - 1].offset + sections[index - 1].bytes
                     : CaptureHeaderBytes) ||
            sha256_range(path, sections[index].offset, sections[index].bytes) !=
              sections[index].sha)
            throw std::runtime_error("capture sidecar section/hash mismatch");
    }
    const std::uint64_t triples = static_cast<std::uint64_t>(std::count_if(
      graph.nodes().begin(), graph.nodes().end(),
      [](const BeliefKey& key) { return key.count == 3; }));
    const std::uint64_t expectedKeyBytes =
      9 * (graph.nodes().size() - triples) + 13 * triples;
    const std::array<std::uint64_t, 5> expectedBytes{{
      expectedKeyBytes,
      std::uint64_t(ConcreteStateCount) * 4,
      (std::uint64_t(ConcreteStateCount) + 7) / 8,
      (layout.blackBase + 7) / 8,
      (std::uint64_t(graph.nodes().size()) + 7) / 8,
    }};
    for (std::size_t index = 0; index < sections.size(); ++index)
        if (sections[index].bytes != expectedBytes[index])
            throw std::runtime_error("capture sidecar section length mismatch");
    if (sections.back().offset + sections.back().bytes !=
          std::filesystem::file_size(path))
        throw std::runtime_error("capture sidecar has trailing bytes");

    input.clear();
    input.seekg(static_cast<std::streamoff>(sections[0].offset));
    for (const BeliefKey& key : graph.nodes()) {
        const int count = input.get();
        if (count != static_cast<int>(key.count))
            throw std::runtime_error("capture restored belief count mismatch");
        std::array<std::uint8_t, 4> encoded{};
        for (std::uint32_t ordinal = 0; ordinal < key.count; ++ordinal) {
            input.read(reinterpret_cast<char*>(encoded.data()), encoded.size());
            if (static_cast<std::size_t>(input.gcount()) != encoded.size() ||
                load_le32(encoded.data()) != key.worlds[ordinal])
                throw std::runtime_error("capture restored belief world mismatch");
        }
    }
    input.clear();
    input.seekg(static_cast<std::streamoff>(sections[1].offset));
    std::array<std::uint8_t, 4> encoded{};
    for (const std::uint32_t root : graph.roots()) {
        input.read(reinterpret_cast<char*>(encoded.data()), encoded.size());
        if (static_cast<std::size_t>(input.gcount()) != encoded.size() ||
            load_le32(encoded.data()) != root)
            throw std::runtime_error("capture restored root map mismatch");
    }
    const auto verifyBits = [&](const CaptureSection& section,
                                std::uint64_t bitCount,
                                const auto& value,
                                const char* label) {
        input.clear();
        input.seekg(static_cast<std::streamoff>(section.offset));
        std::uint8_t packed = 0;
        for (std::uint64_t bit = 0; bit < bitCount; ++bit) {
            if ((bit & 7) == 0) {
                const int byte = input.get();
                if (byte == std::char_traits<char>::eof())
                    throw std::runtime_error(std::string("truncated ") + label);
                packed = static_cast<std::uint8_t>(byte);
            }
            if (((packed >> (bit & 7)) & 1) != value(bit))
                throw std::runtime_error(std::string("capture restored ") +
                                         label + " mismatch");
        }
    };
    verifyBits(sections[2], ConcreteStateCount,
      [&](std::uint64_t bit) { return graph.admitted()[bit] != 0; },
      "admitted bit");
    verifyBits(sections[3], layout.blackBase,
      [&](std::uint64_t bit) {
          return solver.value(static_cast<InformationToken>(bit));
      }, "White target bit");
    verifyBits(sections[4], graph.nodes().size(),
      [&](std::uint64_t bit) {
          return solver.value(static_cast<InformationToken>(
            layout.blackBase + bit));
      }, "Black target bit");
    std::cout << "capture_restore_verified beliefs " << graph.nodes().size()
              << " memberships " << layout.blackBase
              << " section_hash_residual 0 key_residual 0 root_residual 0"
              << " value_residual 0\n" << std::flush;
}

[[nodiscard]] bool read_capture_bit(std::ifstream& input,
                                    const CaptureSection& section,
                                    std::uint64_t bit) {
    if (bit >= section.bytes * 8)
        throw std::runtime_error("capture probe bit is outside section");
    input.clear();
    input.seekg(static_cast<std::streamoff>(section.offset + bit / 8));
    const int byte = input.get();
    if (byte == std::char_traits<char>::eof())
        throw std::runtime_error("capture probe bit section is truncated");
    return (static_cast<std::uint8_t>(byte) >> (bit & 7)) & 1;
}

void probe_capture_sidecar(const CaptureOptions& options) {
    std::vector<std::uint32_t> requested = options.probeWorlds;
    std::sort(requested.begin(), requested.end());
    requested.erase(std::unique(requested.begin(), requested.end()),
                    requested.end());
    if (requested.size() < 2 || requested.size() > 3)
        throw std::runtime_error(
          "capture probe requires exactly two or three distinct --probe-worlds");
    if (options.probeActual == NoActual ||
        !std::binary_search(requested.begin(), requested.end(),
                            options.probeActual))
        throw std::runtime_error(
          "capture probe --probe-actual must belong to the private set");

    const std::filesystem::path path(options.probeSidecar);
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("cannot open capture sidecar for probe");
    std::vector<std::uint8_t> header(CaptureHeaderBytes);
    input.read(reinterpret_cast<char*>(header.data()), header.size());
    if (static_cast<std::size_t>(input.gcount()) != header.size() ||
        std::memcmp(header.data(), "UFICAP2\0", 8) != 0 ||
        load_le32(header.data() + 8) != CaptureFormatVersion ||
        load_le32(header.data() + 12) != CaptureHeaderBytes ||
        std::string(reinterpret_cast<const char*>(header.data() + 32)) !=
          "kjesterjesterk" ||
        std::string(reinterpret_cast<const char*>(header.data() + 64)) !=
          "fresh-maximal-public-view-v2")
        throw std::runtime_error("capture probe header/domain mismatch");
    Sha256 headerHasher;
    headerHasher.update(header.data(), 992);
    const auto headerSha = headerHasher.finish();
    if (!std::equal(headerSha.begin(), headerSha.end(), header.begin() + 992))
        throw std::runtime_error("capture probe header hash mismatch");
    const std::uint64_t beliefCount = load_le64(header.data() + 296);
    const std::uint64_t membershipCount = load_le64(header.data() + 304);
    std::array<CaptureSection, 5> sections{};
    for (std::size_t index = 0; index < sections.size(); ++index) {
        const std::size_t offset = 416 + 48 * index;
        sections[index].offset = load_le64(header.data() + offset);
        sections[index].bytes = load_le64(header.data() + offset + 8);
        std::copy(header.begin() + offset + 16,
                  header.begin() + offset + 48, sections[index].sha.begin());
        if (sections[index].offset !=
              (index ? sections[index - 1].offset + sections[index - 1].bytes
                     : CaptureHeaderBytes) ||
            sha256_range(path, sections[index].offset, sections[index].bytes) !=
              sections[index].sha)
            throw std::runtime_error("capture probe section/hash mismatch");
    }
    if (sections.back().offset + sections.back().bytes !=
          std::filesystem::file_size(path))
        throw std::runtime_error("capture probe sidecar has trailing bytes");

    input.clear();
    input.seekg(static_cast<std::streamoff>(sections[0].offset));
    std::uint64_t membershipBase = 0;
    std::uint64_t foundBelief = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t foundMembership = 0;
    std::array<std::uint8_t, 4> encoded{};
    for (std::uint64_t belief = 0; belief < beliefCount; ++belief) {
        const int count = input.get();
        if (count < 2 || count > 3)
            throw std::runtime_error("capture probe belief stream is invalid");
        std::array<std::uint32_t, 3> worlds{};
        for (int ordinal = 0; ordinal < count; ++ordinal) {
            input.read(reinterpret_cast<char*>(encoded.data()), encoded.size());
            if (static_cast<std::size_t>(input.gcount()) != encoded.size())
                throw std::runtime_error("capture probe belief stream truncated");
            worlds[ordinal] = load_le32(encoded.data());
        }
        if (static_cast<std::size_t>(count) == requested.size() &&
            std::equal(requested.begin(), requested.end(), worlds.begin())) {
            foundBelief = belief;
            const auto actual = std::find(
              worlds.begin(), worlds.begin() + count, options.probeActual);
            if (actual == worlds.begin() + count)
                throw std::runtime_error("capture probe actual world disappeared");
            foundMembership = membershipBase +
              static_cast<std::uint64_t>(actual - worlds.begin());
            break;
        }
        membershipBase += static_cast<std::uint64_t>(count);
    }
    if (foundBelief == std::numeric_limits<std::uint64_t>::max())
        throw std::runtime_error(
          "capture probe private set is not in the reachable exact closure");
    if (foundMembership >= membershipCount)
        throw std::runtime_error("capture probe membership index overflow");
    const bool white = read_capture_bit(input, sections[3], foundMembership);
    const bool black = read_capture_bit(input, sections[4], foundBelief);
    if (white && black)
        throw std::runtime_error("capture probe found a dual forced win");
    std::cout << "capture_probe belief " << foundBelief
              << " membership " << foundMembership
              << " white_forces " << white
              << " black_forces " << black
              << " outcome " << (white ? "white" : black ? "black" : "draw")
              << " exhaustive 1 belief_cap none\n";
}

struct RawArtifact {
    const char* role = nullptr;
    const char* filename = nullptr;
    std::uint32_t recordBytes = 0;
};

constexpr std::array<RawArtifact, 11> RawArtifacts{{
  {"equation_records", "equations.le16", 16},
  {"equation_tokens", "tokens.le32", 4},
  {"reverse_offsets", "reverse-offsets.le64", 8},
  {"reverse_cursors_final", "reverse-cursors-final.le64", 8},
  {"reverse_edges", "reverse-edges.le12", 12},
  {"fixed_point_values", "values.u8", 1},
  {"fixed_point_settled", "settled.u8", 1},
  {"activation_ranks", "ranks.le32", 4},
  {"activation_witnesses", "witnesses.le32", 4},
  {"and_remaining", "remaining.le64", 8},
  {"activation_queue", "queue.le32", 4},
}};

[[nodiscard]] std::uint64_t finalize_raw_capture(
  const CaptureOptions& options, const CaptureSolved& solved,
  const std::array<std::uint8_t, 32>& sidecarSha,
  const std::array<std::uint8_t, 32>& lowerPayloadSha) {
    if (::CaptureScratchPaths.size() != RawArtifacts.size())
        throw std::runtime_error("capture raw scratch creation count mismatch");
    const std::filesystem::path directory(options.rawDirectory);
    std::uint64_t total = 0;
    std::vector<std::filesystem::path> finalPaths;
    finalPaths.reserve(RawArtifacts.size());
    for (std::size_t index = 0; index < RawArtifacts.size(); ++index) {
        const std::filesystem::path target =
          directory / RawArtifacts[index].filename;
        if (std::filesystem::exists(target))
            throw std::runtime_error("capture raw target already exists: " +
                                     target.string());
        std::filesystem::rename(::CaptureScratchPaths[index], target);
        const std::uint64_t bytes = std::filesystem::file_size(target);
        if (bytes % RawArtifacts[index].recordBytes)
            throw std::runtime_error("capture raw record alignment mismatch");
        const std::uint64_t records = bytes / RawArtifacts[index].recordBytes;
        const std::uint64_t expectedRecords =
          index == 1 ? records
        : index == 2 ? solved.variables + 1
        : index == 4 ? solved.summary.reverseEdges
                     : solved.variables;
        if (records != expectedRecords)
            throw std::runtime_error("capture raw record count mismatch");
        if (total > std::numeric_limits<std::uint64_t>::max() - bytes)
            throw std::runtime_error("capture raw byte total overflow");
        total += bytes;
        finalPaths.push_back(target);
    }
    if (total > options.maximumRawBytes)
        throw std::runtime_error(
          "capture raw arena exceeds --maximum-raw-bytes; files retained");

    const std::filesystem::path manifest = directory / "manifest.json";
    std::ofstream output(manifest, std::ios::trunc);
    if (!output)
        throw std::runtime_error("cannot create capture raw manifest");
    output << "{\n"
           << "  \"schema\": \"ultimate-double-jester-raw-v2\",\n"
           << "  \"semantics\": \"fresh-maximal-public-view-v2\",\n"
           << "  \"domain\": \"kjesterjesterk\",\n"
           << "  \"source_sha256\": \"" << options.sourceSha256 << "\",\n"
           << "  \"original_model_sha256\": \""
           << overlay_model_sha256(
                options.expectedOverlay, options.sourceSha256) << "\",\n"
           << "  \"model_sha256\": \"" << options.modelSha256 << "\",\n"
           << "  \"lower_source_sha256\": \""
           << options.lowerSourceSha256 << "\",\n"
           << "  \"lower_model_sha256\": \""
           << options.lowerModelSha256 << "\",\n"
           << "  \"lower_payload_sha256\": \""
           << hex_digest(lowerPayloadSha) << "\",\n"
           << "  \"sidecar_sha256\": \"" << hex_digest(sidecarSha) << "\",\n"
           << "  \"expected_overlay_sha256\": \""
           << hex_digest(sha256_path(options.expectedOverlay)) << "\",\n"
           << "  \"generated_overlay_sha256\": \""
           << hex_digest(sha256_path(options.output)) << "\",\n"
           << "  \"endianness\": \"little\",\n"
           << "  \"variables\": " << solved.variables << ",\n"
           << "  \"white_memberships\": " << solved.whiteMemberships << ",\n"
           << "  \"black_beliefs\": " << solved.blackBeliefs << ",\n"
           << "  \"reverse_edges\": " << solved.summary.reverseEdges << ",\n"
           << "  \"total_bytes\": " << total << ",\n"
           << "  \"files\": [\n";
    for (std::size_t index = 0; index < RawArtifacts.size(); ++index) {
        const std::uint64_t bytes = std::filesystem::file_size(finalPaths[index]);
        output << "    {\"role\": \"" << RawArtifacts[index].role
               << "\", \"path\": \"" << RawArtifacts[index].filename
               << "\", \"bytes\": " << bytes
               << ", \"record_bytes\": " << RawArtifacts[index].recordBytes
               << ", \"records\": "
               << bytes / RawArtifacts[index].recordBytes
               << ", \"sha256\": \""
               << hex_digest(sha256_path(finalPaths[index])) << "\"}"
               << (index + 1 == RawArtifacts.size() ? "\n" : ",\n");
    }
    output << "  ]\n}\n";
    output.close();
    if (!output)
        throw std::runtime_error("cannot finalize capture raw manifest");
    std::cout << "capture_raw_complete directory " << directory
              << " files " << RawArtifacts.size()
              << " bytes " << total
              << " manifest_sha256 " << hex_digest(sha256_path(manifest))
              << '\n' << std::flush;
    return total;
}

void require_expected_overlay_payload(const CaptureOptions& options) {
    if (options.expectedOverlay.empty())
        throw std::runtime_error(
          "capture requires --expected-fresh-overlay from the audited proof run");
    std::ifstream expected(options.expectedOverlay, std::ios::binary);
    if (!expected)
        throw std::runtime_error("cannot open expected fresh overlay");
    std::array<char, 160> header{};
    expected.read(header.data(), header.size());
    if (static_cast<std::size_t>(expected.gcount()) != header.size() ||
        std::memcmp(header.data(), "UFIW2\0\0\0", 8) != 0 ||
        std::string(header.data() + 32, 64) != options.sourceSha256)
        throw std::runtime_error("expected fresh overlay binding is invalid");
    if (!same_file_payload(options.output, options.expectedOverlay, 160))
        throw std::runtime_error(
          "capture fresh overlay payload differs from audited proof run");
    std::cout << "capture_fresh_overlay_equal payload_bytes "
              << ConcreteStateCount << " generated_sha256 "
              << hex_digest(sha256_path(options.output))
              << " expected_sha256 "
              << hex_digest(sha256_path(options.expectedOverlay)) << '\n'
              << std::flush;
}

void capture_probe_self_test(const std::filesystem::path& path) {
    const std::filesystem::path sidecar = path / "probe.uficapture";
    std::ofstream output(sidecar, std::ios::binary | std::ios::trunc);
    std::vector<std::uint8_t> header(CaptureHeaderBytes, 0);
    output.write(reinterpret_cast<const char*>(header.data()), header.size());
    HashedSectionWriter keyWriter(output);
    keyWriter.byte(2);
    keyWriter.u32(10);
    keyWriter.u32(20);
    const CaptureSection keys = keyWriter.finish();
    HashedSectionWriter rootWriter(output);
    const CaptureSection roots = rootWriter.finish();
    HashedSectionWriter admittedWriter(output);
    const CaptureSection admitted = admittedWriter.finish();
    HashedSectionWriter whiteWriter(output);
    whiteWriter.byte(2);  // actual ordinal one is a sure Ivory win
    const CaptureSection white = whiteWriter.finish();
    HashedSectionWriter blackWriter(output);
    blackWriter.byte(0);
    const CaptureSection black = blackWriter.finish();
    std::memcpy(header.data(), "UFICAP2\0", 8);
    store_le32(header, 8, CaptureFormatVersion);
    store_le32(header, 12, CaptureHeaderBytes);
    store_text(header, 32, 32, "kjesterjesterk");
    store_text(header, 64, 64, "fresh-maximal-public-view-v2");
    store_le64(header, 296, 1);
    store_le64(header, 304, 2);
    store_section(header, 416, keys);
    store_section(header, 464, roots);
    store_section(header, 512, admitted);
    store_section(header, 560, white);
    store_section(header, 608, black);
    Sha256 headerHasher;
    headerHasher.update(header.data(), 992);
    store_digest(header, 992, headerHasher.finish());
    output.seekp(0);
    output.write(reinterpret_cast<const char*>(header.data()), header.size());
    output.close();
    if (!output)
        throw std::runtime_error("cannot create capture probe self-test sidecar");
    CaptureOptions options;
    options.probeSidecar = sidecar.string();
    options.probeWorlds = {20, 10};
    options.probeActual = 20;
    probe_capture_sidecar(options);
}

void capture_solver_self_test() {
    std::array<char, 64> pattern{};
    const std::string prefix = "/tmp/ultimatefish-double-capture-selftest-XXXXXX";
    if (prefix.size() + 1 > pattern.size())
        throw std::runtime_error("capture self-test template overflow");
    std::copy(prefix.begin(), prefix.end(), pattern.begin());
    char* directory = ::mkdtemp(pattern.data());
    if (!directory)
        throw std::runtime_error("cannot create capture self-test directory");
    const std::filesystem::path path(directory);
    ::CaptureScratchPaths.clear();
    CaptureSolved captured;
    CaptureOptions options;
    options.rawDirectory = path.string();
    options.sourceSha256 = std::string(64, '1');
    options.modelSha256 = std::string(64, '2');
    options.lowerSourceSha256 = std::string(64, '3');
    options.lowerModelSha256 = std::string(64, '4');
    options.expectedOverlay = (path / "expected.ufiw").string();
    options.output = (path / "generated.ufiw").string();
    std::array<char, 160> overlayHeader{};
    std::memcpy(overlayHeader.data(), "UFIW2\0\0\0", 8);
    std::copy(options.sourceSha256.begin(), options.sourceSha256.end(),
              overlayHeader.begin() + 32);
    const std::string originalModel(64, '5');
    std::copy(originalModel.begin(), originalModel.end(),
              overlayHeader.begin() + 96);
    for (const std::string& overlay :
         {options.expectedOverlay, options.output}) {
        std::ofstream stream(overlay, std::ios::binary | std::ios::trunc);
        stream.write(overlayHeader.data(), overlayHeader.size());
        if (!stream)
            throw std::runtime_error("cannot create capture self-test overlay");
    }
    {
        CaptureFixedPoint solver(4, path.string());
        const InformationToken yes = InformationTrue;
        const InformationToken no = InformationFalse;
        const InformationToken first = 0;
        solver.define_or(0, &yes, 1);
        solver.define_and(1, &first, 1);
        solver.define_or(2, &no, 1);
        solver.define_and(3, nullptr, 0);
        const InformationSolveSummary summary = solver.solve();
        if (summary.bellmanResidual || summary.rankResidual ||
            !solver.value(0) || !solver.value(1) || solver.value(2) ||
            !solver.value(3) || ::CaptureScratchPaths.size() != RawArtifacts.size())
            throw std::runtime_error("capture fixed-point self-test failed");
        captured.summary = summary;
        captured.variables = 4;
        captured.whiteMemberships = 2;
        captured.blackBeliefs = 2;
        const std::array<std::uint8_t, 32> zero{};
        (void)finalize_raw_capture(options, captured, zero, zero);
    }
    for (const RawArtifact& artifact : RawArtifacts) {
        const std::filesystem::path file = path / artifact.filename;
        if (!std::filesystem::is_regular_file(file) ||
            std::filesystem::file_size(file) == 0)
            throw std::runtime_error("capture named scratch self-test failed");
    }
    if (!std::filesystem::is_regular_file(path / "manifest.json"))
        throw std::runtime_error("capture raw manifest self-test failed");
    capture_probe_self_test(path);
    std::error_code error;
    std::filesystem::remove_all(path, error);
    if (error)
        throw std::runtime_error("cannot clean capture self-test directory");
    ::CaptureScratchPaths.clear();
    std::cout << "capture_selftest fixed_point_residual 0 named_files "
              << RawArtifacts.size() << " endian little sidecar_version "
              << CaptureFormatVersion << '\n';
}

void run_capture(const CaptureOptions& options) {
    require_capture_resources(options);
    if (options.output.empty() || options.sidecar.empty())
        throw std::runtime_error("capture requires --output and --sidecar");
    if (!valid_sha256(options.modelSha256))
        throw std::runtime_error("capture requires --information-model-sha256");
    if (!valid_sha256(options.lowerModelSha256))
        throw std::runtime_error(
          "capture requires --lower-information-model-sha256");
    const PackedTable concrete = load_table(options.input);
    if (options.sourceSha256 != hex_digest(concrete.sha))
        throw std::runtime_error("capture concrete source SHA-256 mismatch");
    std::cout << "capture_input " << options.input
              << " concrete " << ConcreteStateCount
              << " sha256 " << options.sourceSha256 << '\n' << std::flush;
    const LowerJesterOverlay lower(
      options.lowerOverlay, options.lowerConcrete, options.lowerSourceSha256,
      options.lowerModelSha256);
    const std::array<std::uint8_t, 32> lowerPayloadSha =
      sha256_payload(options.lowerOverlay, 160);
    DoubleJesterGraph graph(concrete, lower);
    graph.build_roots();
    graph.expand_all();
    CaptureLayout layout = make_capture_layout(graph);
    ::CaptureScratchPaths.clear();
    CaptureFixedPoint solver(
      static_cast<std::uint32_t>(layout.variableCount), options.rawDirectory);
    define_capture_equations(graph, layout, solver);
    const InformationSolveSummary summary = solver.solve();
    const InformationSolveSummary verified = solver.verify();
    if (summary.bellmanResidual || summary.rankResidual ||
        verified.bellmanResidual || verified.rankResidual ||
        summary.variables != layout.variableCount ||
        verified.variables != summary.variables ||
        verified.reverseEdges != summary.reverseEdges ||
        verified.activated != summary.activated)
        throw std::runtime_error("capture fixed point verification failed");
    std::cout << "capture_fixed_point variables " << summary.variables
              << " reverse_edges " << summary.reverseEdges
              << " activated " << summary.activated
              << " bellman_residual " << summary.bellmanResidual
              << " rank_residual " << summary.rankResidual << '\n'
              << std::flush;

    const auto [domainResidual, dualResidual] =
      verify_capture_domain(graph, layout, solver);
    if (domainResidual || dualResidual)
        throw std::runtime_error("capture domain regeneration residual is nonzero");
    CaptureSolved solved;
    solved.summary = summary;
    solved.whiteMemberships = layout.blackBase;
    solved.blackBeliefs = graph.nodes().size();
    solved.variables = layout.variableCount;
    solved.softLocks = layout.softLocks;
    solved.domainBellmanResidual = domainResidual;
    solved.dualWinResidual = dualResidual;
    solved.freshFlags = capture_fresh_flags(graph, layout, solver);

    SolvedInformation ordinary;
    ordinary.flags = solved.freshFlags;
    ordinary.fixedPoint = summary;
    ordinary.variables = layout.variableCount;
    ordinary.softLocks = layout.softLocks;
    report_results(graph, concrete, ordinary);
    write_overlay(options.output, concrete, ordinary,
                  options.sourceSha256, options.modelSha256);
    require_expected_overlay_payload(options);
    write_capture_sidecar(options.sidecar, graph, layout, solver, solved,
                          options, lowerPayloadSha);
    verify_capture_sidecar(options.sidecar, graph, layout, solver,
                           solved, options, lowerPayloadSha);
    const std::array<std::uint8_t, 32> sidecarSha =
      sha256_path(options.sidecar);
    (void)finalize_raw_capture(
      options, solved, sidecarSha, lowerPayloadSha);
    const InformationSolveSummary afterArchive = solver.verify();
    if (afterArchive.bellmanResidual || afterArchive.rankResidual)
        throw std::runtime_error("capture arena changed during archival");
    std::cout << "capture_complete exhaustive 1 belief_cap none"
              << " domain_bellman_residual 0 dual_win_residual 0"
              << " uniform_action_residual 0 singleton_residual 0"
              << " fresh_overlay_residual 0\n" << std::flush;
}

[[nodiscard]] CaptureOptions parse_capture_options(int argc, char** argv) {
    CaptureOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        const auto value = [&](const char* option) {
            if (index + 1 >= argc)
                throw std::runtime_error(
                  std::string("missing value for ") + option);
            return std::string(argv[++index]);
        };
        if (argument == "--input")
            options.input = value("--input");
        else if (argument == "--lower-information-overlay")
            options.lowerOverlay = value("--lower-information-overlay");
        else if (argument == "--lower-concrete")
            options.lowerConcrete = value("--lower-concrete");
        else if (argument == "--lower-information-source-sha256")
            options.lowerSourceSha256 = value(argument.c_str());
        else if (argument == "--lower-information-model-sha256")
            options.lowerModelSha256 = value(argument.c_str());
        else if (argument == "--output")
            options.output = value("--output");
        else if (argument == "--sidecar")
            options.sidecar = value("--sidecar");
        else if (argument == "--raw-directory")
            options.rawDirectory = value("--raw-directory");
        else if (argument == "--expected-fresh-overlay")
            options.expectedOverlay = value("--expected-fresh-overlay");
        else if (argument == "--information-source-sha256")
            options.sourceSha256 = value(argument.c_str());
        else if (argument == "--information-model-sha256")
            options.modelSha256 = value(argument.c_str());
        else if (argument == "--probe-sidecar")
            options.probeSidecar = value(argument.c_str());
        else if (argument == "--probe-world") {
            const std::uint64_t world = parse_u64(
              value(argument.c_str()), argument.c_str());
            if (world > std::numeric_limits<std::uint32_t>::max())
                throw std::runtime_error("--probe-world exceeds u32");
            options.probeWorlds.push_back(static_cast<std::uint32_t>(world));
        }
        else if (argument == "--probe-actual") {
            const std::uint64_t actual = parse_u64(
              value(argument.c_str()), argument.c_str());
            if (actual > std::numeric_limits<std::uint32_t>::max())
                throw std::runtime_error("--probe-actual exceeds u32");
            options.probeActual = static_cast<std::uint32_t>(actual);
        }
        else if (argument == "--required-free-bytes")
            options.requiredFreeBytes = parse_u64(
              value(argument.c_str()), argument.c_str());
        else if (argument == "--maximum-raw-bytes")
            options.maximumRawBytes = parse_u64(
              value(argument.c_str()), argument.c_str());
        else if (argument == "--self-test")
            options.selfTestOnly = true;
        else if (argument == "--estimate-only")
            options.estimateOnly = true;
        else
            throw std::runtime_error("unknown capture argument: " + argument);
    }
    if (options.probeSidecar.empty() &&
        (!options.probeWorlds.empty() || options.probeActual != NoActual))
        throw std::runtime_error(
          "--probe-world/--probe-actual require --probe-sidecar");
    return options;
}

void print_capture_estimate() {
    constexpr std::uint64_t Variables = 270'783'454;
    constexpr std::uint64_t EquationBytes = Variables * 16;
    constexpr std::uint64_t ConservativeTokenBytes = 12ULL << 30;
    constexpr std::uint64_t OffsetAndCursorBytes = (Variables * 8) * 2;
    constexpr std::uint64_t ConservativeReverseBytes = 36ULL << 30;
    constexpr std::uint64_t SolutionBytes = Variables * 22;
    constexpr std::uint64_t PortableBytes = 600ULL << 20;
    constexpr std::uint64_t Total = EquationBytes + ConservativeTokenBytes +
      OffsetAndCursorBytes + ConservativeReverseBytes + SolutionBytes +
      PortableBytes;
    std::cout << "capture_estimate variables " << Variables
              << " equation_bytes " << EquationBytes
              << " token_bytes_upper " << ConservativeTokenBytes
              << " offset_cursor_bytes " << OffsetAndCursorBytes
              << " reverse_bytes_upper " << ConservativeReverseBytes
              << " solution_bytes " << SolutionBytes
              << " portable_bytes_upper " << PortableBytes
              << " total_bytes_upper " << Total
              << " recommended_free_bytes " << DefaultRequiredFreeBytes
              << '\n';
}

}  // namespace
}  // namespace Stockfish::Ultimate

int main(int argc, char** argv) {
    using namespace Stockfish::Ultimate;
    try {
        const CaptureOptions options = parse_capture_options(argc, argv);
        if (!options.probeSidecar.empty()) {
            probe_capture_sidecar(options);
            return 0;
        }
        codec_self_test();
        capture_solver_self_test();
        print_capture_estimate();
        if (options.selfTestOnly || options.estimateOnly)
            return 0;
        run_capture(options);
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "double-jester-information-capture: "
                  << error.what() << '\n';
        return 1;
    }
}
