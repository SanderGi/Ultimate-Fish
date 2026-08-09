/*
  Ultimate Fish - exact joint-Jester arbitrary-history preservation
  Copyright (C) 2026 Ultimate Fish contributors
  GPLv3 or later.

  This is an isolated capture build.  The running joint-Jester proof source is
  included byte-for-byte and is not modified or added to its model fingerprint.
  A fresh exact recomputation persists the otherwise heap-only epistemic arena
  and the otherwise anonymous/unlinked fixed-point solution.  Export and strict
  restore operate only on those persistent files.
*/

#define main ultimate_joint_jester_original_main
#include "joint_jester_information_tablebase.cpp"
#undef main

#include <cerrno>
#include <filesystem>
#include <numeric>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace Stockfish::Ultimate::JointJesterPreserver {
namespace {

constexpr std::uint32_t HeaderBytes = 2048;
constexpr std::uint32_t HeaderVersion = 1;
constexpr std::uint32_t Endian = 0x01020304;
constexpr std::uint32_t CaptureKind = 1;
constexpr std::uint32_t SidecarKind = 2;
constexpr std::uint32_t HashOffset = 320;
constexpr std::uint32_t HashBytes = 64;
constexpr std::uint32_t HashSlots = 16;
constexpr std::uint32_t SemanticsOffset = 1408;
constexpr char CaptureMagic[8] = {'U','F','J','R','1','\0','\0','\0'};
constexpr char SidecarMagic[8] = {'U','F','J','A','1','\0','\0','\0'};
constexpr char SidecarSemantics[] =
  "fresh-maximal-public-view-v2:joint-jester-arbitrary-history-d2-v1";
constexpr std::uint32_t NoWitness = InformationNoWitness;
static_assert(HashOffset + HashSlots * HashBytes <= SemanticsOffset);

enum HashSlot : std::size_t {
    SourceSha,
    OriginalModelSha,
    CaptureModelSha,
    ObservationSha,
    LowerTableSha,
    LowerOverlaySha,
    LowerModelSha,
    DenseOverlaySha,
    NodesSha,
    ValuesSha,
    RanksSha,
    WitnessesSha,
    RawMetaSha,
    PayloadSha,
    ProofLogSha,
    ReservedSha,
};

enum class EquationKind : std::uint8_t { Or, And };

struct Bindings {
    std::string sourceSha;
    std::string originalModelSha;
    std::string captureModelSha;
    std::string observationSha;
    std::string lowerTableSha;
    std::string lowerOverlaySha;
    std::string lowerModelSha;
    std::string proofLogSha;
};

struct Summary {
    std::array<std::array<std::uint64_t, 4>, 2> totals{};
    std::array<std::array<std::uint64_t, 4>, 2> unreachable{};
    std::array<std::uint64_t, 2> sets{};
    std::uint64_t dualResidual = 0;
    std::uint64_t conservationResidual = 0;
};

struct EquationPlan {
    std::uint64_t baseVariables = 0;
    std::array<std::vector<std::uint64_t>, 2> gateBase;
    std::array<std::uint64_t, 2> variables{};
    std::array<std::uint64_t, 2> tokens{};
    std::array<std::uint64_t, 2> variableReferences{};
};

struct CapturedTarget {
    Color target = Color::White;
    std::vector<std::uint8_t> values;
    std::vector<std::uint32_t> ranks;
    std::vector<std::uint32_t> witnesses;
    InformationSolveSummary fixedPoint;
};

struct CaptureSolutions {
    EquationPlan plan;
    std::array<CapturedTarget, 2> targets;
};

struct Header {
    std::array<std::uint8_t, HeaderBytes> bytes{};

    void put32(std::size_t offset, std::uint32_t value) {
        if (offset + 4 > bytes.size()) throw std::runtime_error("header u32 overflow");
        for (unsigned shift = 0; shift < 4; ++shift)
            bytes[offset + shift] = static_cast<std::uint8_t>(value >> (8 * shift));
    }
    void put64(std::size_t offset, std::uint64_t value) {
        if (offset + 8 > bytes.size()) throw std::runtime_error("header u64 overflow");
        for (unsigned shift = 0; shift < 8; ++shift)
            bytes[offset + shift] = static_cast<std::uint8_t>(value >> (8 * shift));
    }
    [[nodiscard]] std::uint32_t get32(std::size_t offset) const {
        if (offset + 4 > bytes.size()) throw std::runtime_error("header u32 overflow");
        std::uint32_t value = 0;
        for (unsigned shift = 0; shift < 4; ++shift)
            value |= std::uint32_t(bytes[offset + shift]) << (8 * shift);
        return value;
    }
    [[nodiscard]] std::uint64_t get64(std::size_t offset) const {
        if (offset + 8 > bytes.size()) throw std::runtime_error("header u64 overflow");
        std::uint64_t value = 0;
        for (unsigned shift = 0; shift < 8; ++shift)
            value |= std::uint64_t(bytes[offset + shift]) << (8 * shift);
        return value;
    }
    void put_hash(HashSlot slot, const std::string& value) {
        if (!valid_sha(value)) throw std::runtime_error("invalid preservation SHA-256");
        const std::size_t offset = HashOffset + std::size_t(slot) * HashBytes;
        std::copy(value.begin(), value.end(), bytes.begin() + offset);
    }
    [[nodiscard]] std::string hash(HashSlot slot) const {
        const std::size_t offset = HashOffset + std::size_t(slot) * HashBytes;
        return std::string(reinterpret_cast<const char*>(bytes.data() + offset), HashBytes);
    }
};

[[nodiscard]] std::string system_error(const std::string& message,
                                       const std::string& path) {
    return message + " " + path + ": " + std::strerror(errno);
}

class MappedFile {
  public:
    explicit MappedFile(const std::string& path) : path_(path) {
        descriptor_ = ::open(path.c_str(), O_RDONLY);
        if (descriptor_ < 0) throw std::runtime_error(system_error("cannot open", path));
        struct stat status{};
        if (::fstat(descriptor_, &status) || status.st_size < 0)
            throw std::runtime_error(system_error("cannot stat", path));
        bytes_ = static_cast<std::uint64_t>(status.st_size);
        if (bytes_) {
            data_ = static_cast<const std::uint8_t*>(::mmap(
              nullptr, static_cast<std::size_t>(bytes_), PROT_READ,
              MAP_PRIVATE, descriptor_, 0));
            if (data_ == MAP_FAILED) {
                data_ = nullptr;
                throw std::runtime_error(system_error("cannot mmap", path));
            }
        }
    }
    ~MappedFile() {
        if (data_) ::munmap(const_cast<std::uint8_t*>(data_), bytes_);
        if (descriptor_ >= 0) ::close(descriptor_);
    }
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;
    [[nodiscard]] const std::uint8_t* data() const { return data_; }
    [[nodiscard]] std::uint64_t size() const { return bytes_; }
    template<typename T> [[nodiscard]] const T* as(std::uint64_t count) const {
        if (count > bytes_ / sizeof(T) || count * sizeof(T) != bytes_)
            throw std::runtime_error(path_ + ": mapped extent mismatch");
        return reinterpret_cast<const T*>(data_);
    }
  private:
    std::string path_;
    int descriptor_ = -1;
    const std::uint8_t* data_ = nullptr;
    std::uint64_t bytes_ = 0;
};

[[nodiscard]] std::string sha256_file(const std::string& path,
                                      std::uint64_t offset = 0) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot hash " + path);
    input.seekg(static_cast<std::streamoff>(offset));
    if (!input) throw std::runtime_error("cannot seek while hashing " + path);
    Sha256 hash;
    std::array<std::uint8_t, 1 << 20> buffer{};
    while (input) {
        input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        if (input.gcount() > 0)
            hash.update(buffer.data(), static_cast<std::size_t>(input.gcount()));
    }
    return hex_digest(hash.finish());
}

void write_all(std::ofstream& output, const void* data, std::uint64_t bytes) {
    const auto* cursor = static_cast<const char*>(data);
    while (bytes) {
        const std::streamsize amount = static_cast<std::streamsize>(
          std::min<std::uint64_t>(bytes, 1ULL << 30));
        output.write(cursor, amount);
        if (!output) throw std::runtime_error("preservation write failed");
        cursor += amount;
        bytes -= static_cast<std::uint64_t>(amount);
    }
}

class LittleEndianWriter {
  public:
    explicit LittleEndianWriter(std::ofstream& output) : output_(output) {}
    ~LittleEndianWriter() { flush(); }

    void put8(std::uint8_t value) {
        if (used_ == buffer_.size()) flush();
        buffer_[used_++] = value;
    }
    void put32(std::uint32_t value) {
        for (unsigned byte = 0; byte < 4; ++byte)
            put8(static_cast<std::uint8_t>(value >> (8 * byte)));
    }
    void put64(std::uint64_t value) {
        for (unsigned byte = 0; byte < 8; ++byte)
            put8(static_cast<std::uint8_t>(value >> (8 * byte)));
    }
    void finish() {
        flush();
        if (!output_) throw std::runtime_error("portable preservation write failed");
    }
  private:
    void flush() {
        if (!used_) return;
        output_.write(reinterpret_cast<const char*>(buffer_.data()),
                      static_cast<std::streamsize>(used_));
        used_ = 0;
    }
    std::ofstream& output_;
    std::array<std::uint8_t, 1 << 20> buffer_{};
    std::size_t used_ = 0;
};

void write_header(const std::string& path, const Header& header) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    write_all(output, header.bytes.data(), header.bytes.size());
}

[[nodiscard]] Header read_header(const std::string& path) {
    Header result;
    std::ifstream input(path, std::ios::binary);
    input.read(reinterpret_cast<char*>(result.bytes.data()), result.bytes.size());
    if (!input || input.peek() != std::char_traits<char>::eof())
        throw std::runtime_error(path + ": invalid preservation header extent");
    return result;
}

void initialize_header(Header& header, const char (&magic)[8],
                       std::uint32_t kind) {
    std::copy(std::begin(magic), std::end(magic), header.bytes.begin());
    header.put32(8, HeaderVersion);
    header.put32(12, HeaderBytes);
    header.put32(16, Endian);
    header.put32(20, kind);
    header.put32(24, static_cast<std::uint32_t>(PieceType::Jester));
    header.put32(28, static_cast<std::uint32_t>(PieceType::Jester));
    header.put32(32, static_cast<std::uint32_t>(Color::Black));
    header.put32(36, Squares);
    header.put64(208, ConcreteCount);
    std::copy(std::begin(SidecarSemantics), std::end(SidecarSemantics),
              header.bytes.begin() + SemanticsOffset);
}

void validate_common_header(const Header& header, const char (&magic)[8],
                            std::uint32_t kind, const Bindings& bindings) {
    if (!std::equal(std::begin(magic), std::end(magic), header.bytes.begin()) ||
        header.get32(8) != HeaderVersion || header.get32(12) != HeaderBytes ||
        header.get32(16) != Endian || header.get32(20) != kind ||
        header.get32(24) != static_cast<std::uint32_t>(PieceType::Jester) ||
        header.get32(28) != static_cast<std::uint32_t>(PieceType::Jester) ||
        header.get32(32) != static_cast<std::uint32_t>(Color::Black) ||
        header.get32(36) != Squares || header.get64(208) != ConcreteCount ||
        std::string(reinterpret_cast<const char*>(
          header.bytes.data() + SemanticsOffset), std::strlen(SidecarSemantics)) !=
          SidecarSemantics)
        throw std::runtime_error("joint-Jester preservation header contract residual");
    const std::array<std::pair<HashSlot, const std::string*>, 8> expected{{
      {SourceSha, &bindings.sourceSha},
      {OriginalModelSha, &bindings.originalModelSha},
      {CaptureModelSha, &bindings.captureModelSha},
      {ObservationSha, &bindings.observationSha},
      {LowerTableSha, &bindings.lowerTableSha},
      {LowerOverlaySha, &bindings.lowerOverlaySha},
      {LowerModelSha, &bindings.lowerModelSha},
      {ProofLogSha, &bindings.proofLogSha},
    }};
    for (const auto& [slot, value] : expected)
        if (!valid_sha(*value) || header.hash(slot) != *value)
            throw std::runtime_error("joint-Jester preservation binding residual");
}

[[nodiscard]] InformationToken class_variable(std::uint32_t node,
                                               unsigned bit) {
    return static_cast<InformationToken>(4ULL * node + bit);
}

[[nodiscard]] EquationPlan equation_plan(const JointArena& arena) {
    EquationPlan plan;
    const auto& nodes = arena.nodes();
    plan.baseVariables = 4ULL * nodes.size();
    for (auto& base : plan.gateBase) base.resize(nodes.size() + 1, 0);
    for (std::size_t node = 0; node < nodes.size(); ++node) {
        const Geometry geometry = node_geometry(nodes[node]);
        const std::uint64_t actions = geometry.terminal ? 0 :
          arena.regenerate(static_cast<std::uint32_t>(node)).actions.size();
        for (const Color target : {Color::White, Color::Black}) {
            const std::size_t side = static_cast<std::size_t>(target);
            plan.gateBase[side][node + 1] = plan.gateBase[side][node] +
              (geometry.side == target ? actions : 0);
        }
    }
    for (std::size_t side = 0; side < 2; ++side) {
        plan.variables[side] = plan.baseVariables + plan.gateBase[side].back();
        if (!plan.variables[side] || plan.variables[side] >= InformationTrue)
            throw std::runtime_error("joint capture variable count exceeds token domain");
    }
    return plan;
}

template<typename Emit>
void emit_equations(const JointArena& arena, EquationPlan& plan, Emit&& emit) {
    const auto& nodes = arena.nodes();
    std::array<std::vector<InformationToken>, 2> buffers;
    for (std::size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
        const std::uint32_t node = static_cast<std::uint32_t>(nodeIndex);
        const Geometry geometry = node_geometry(nodes[nodeIndex]);
        const std::uint8_t relation = node_relation(nodes[nodeIndex]);
        const GeneratedNode generated = geometry.terminal ? GeneratedNode{} :
          arena.regenerate(node);
        for (const Color target : {Color::White, Color::Black}) {
            const std::size_t side = static_cast<std::size_t>(target);
            auto& tokens = buffers[side];
            const auto gate = [&](std::size_t action) {
                return static_cast<InformationToken>(plan.baseVariables +
                  plan.gateBase[side][nodeIndex] + action);
            };
            const auto child = [&](const Successor& successor) {
                if (successor.node == NoNode)
                    return successor.exact[side] ? InformationTrue : InformationFalse;
                return class_variable(successor.node, successor.bitIndex);
            };
            if (geometry.terminal) {
                for (unsigned bit = 0; bit < 4; ++bit) {
                    const std::uint8_t winner = target == Color::White ? 2 : 3;
                    tokens.assign(1, (relation & (1u << bit)) &&
                      geometry.terminal == winner ? InformationTrue : InformationFalse);
                    emit(side, class_variable(node, bit), EquationKind::Or, tokens);
                }
                continue;
            }
            if (geometry.side == target) {
                std::array<std::vector<InformationToken>, 4> choices;
                for (std::size_t action = 0; action < generated.actions.size(); ++action) {
                    tokens.clear();
                    for (const std::size_t candidate : generated.actions[action].candidates)
                        tokens.push_back(child(generated.candidates[candidate].successor));
                    emit(side, gate(action), EquationKind::And, tokens);
                    choices[generated.actions[action].actorBlock].push_back(gate(action));
                }
                for (unsigned bit = 0; bit < 4; ++bit) {
                    if (relation & (1u << bit)) {
                        const unsigned block = partition_representative(
                          relation, generated.effectivePartitions[side], bit);
                        tokens = choices[block];
                    } else tokens.assign(1, InformationFalse);
                    emit(side, class_variable(node, bit), EquationKind::Or, tokens);
                }
            } else {
                for (unsigned bit = 0; bit < 4; ++bit) {
                    tokens.clear();
                    if (relation & (1u << bit)) {
                        for (const UniformAction& action : generated.actions)
                            for (const std::size_t candidate : action.candidates) {
                                const Candidate& edge = generated.candidates[candidate];
                                if (partition_equivalent(
                                      generated.effectivePartitions[side], bit,
                                      edge.parentBit))
                                    tokens.push_back(child(edge.successor));
                            }
                        emit(side, class_variable(node, bit), EquationKind::And, tokens);
                    } else {
                        tokens.assign(1, InformationFalse);
                        emit(side, class_variable(node, bit), EquationKind::Or, tokens);
                    }
                }
            }
        }
    }
}

[[nodiscard]] CaptureSolutions solve_capture(const JointArena& arena,
                                             const std::string& scratch) {
    CaptureSolutions capture;
    capture.plan = equation_plan(arena);
    std::array<std::unique_ptr<InformationFixedPoint>, 2> solvers;
    for (std::size_t side = 0; side < 2; ++side)
        solvers[side] = std::make_unique<InformationFixedPoint>(
          static_cast<std::uint32_t>(capture.plan.variables[side]), scratch);
    emit_equations(arena, capture.plan,
      [&](std::size_t side, InformationToken parent, EquationKind kind,
          const std::vector<InformationToken>& tokens) {
          capture.plan.tokens[side] += tokens.size();
          capture.plan.variableReferences[side] += std::count_if(
            tokens.begin(), tokens.end(), [](InformationToken token) {
                return token < InformationTrue;
            });
          if (kind == EquationKind::Or)
              solvers[side]->define_or(parent, tokens);
          else
              solvers[side]->define_and(parent, tokens);
      });
    for (const Color target : {Color::White, Color::Black}) {
        const std::size_t side = static_cast<std::size_t>(target);
        CapturedTarget& result = capture.targets[side];
        result.target = target;
        result.fixedPoint = solvers[side]->solve();
        const std::size_t variables = static_cast<std::size_t>(capture.plan.variables[side]);
        result.values.resize(variables);
        result.ranks.resize(variables);
        result.witnesses.resize(variables);
        for (std::size_t variable = 0; variable < variables; ++variable) {
            result.values[variable] = solvers[side]->value(
              static_cast<InformationToken>(variable));
            result.ranks[variable] = solvers[side]->activation_rank(
              static_cast<std::uint32_t>(variable));
            result.witnesses[variable] = solvers[side]->witness_index(
              static_cast<std::uint32_t>(variable));
        }
        solvers[side].reset();
    }
    return capture;
}

[[nodiscard]] bool token_value(InformationToken token,
                               const CapturedTarget& target) {
    if (token == InformationTrue) return true;
    if (token == InformationFalse) return false;
    return target.values.at(token) != 0;
}

[[nodiscard]] std::uint32_t token_rank(InformationToken token,
                                       const CapturedTarget& target) {
    if (token == InformationTrue) return 0;
    if (token == InformationFalse || !target.values.at(token))
        throw std::runtime_error("rank requested for false restored token");
    return target.ranks.at(token);
}

struct RestoreResidual {
    std::uint64_t equation = 0;
    std::uint64_t bellman = 0;
    std::uint64_t rank = 0;
};

[[nodiscard]] RestoreResidual verify_captured_equations(
  const JointArena& arena, CaptureSolutions& capture) {
    RestoreResidual residual;
    std::array<std::vector<std::uint8_t>, 2> seen;
    std::array<std::uint64_t, 2> tokens{};
    std::array<std::uint64_t, 2> references{};
    for (std::size_t side = 0; side < 2; ++side)
        seen[side].resize(static_cast<std::size_t>(capture.plan.variables[side]), 0);
    emit_equations(arena, capture.plan,
      [&](std::size_t side, InformationToken parent, EquationKind kind,
          const std::vector<InformationToken>& children) {
          tokens[side] += children.size();
          references[side] += std::count_if(children.begin(), children.end(),
            [](InformationToken token) { return token < InformationTrue; });
          if (parent >= seen[side].size() || seen[side][parent]++) {
              ++residual.equation;
              return;
          }
          const CapturedTarget& target = capture.targets[side];
          bool expected = kind == EquationKind::And;
          std::uint32_t expectedRank = kind == EquationKind::And ? 1 : 0;
          bool witnessValid = false;
          if (kind == EquationKind::Or) {
              std::uint32_t best = std::numeric_limits<std::uint32_t>::max();
              for (const InformationToken child : children)
                  if (token_value(child, target)) {
                      expected = true;
                      best = std::min(best, token_rank(child, target) + 1);
                  }
              if (expected) {
                  expectedRank = best;
                  const std::uint32_t witness = target.witnesses[parent];
                  witnessValid = witness < children.size() &&
                    token_value(children[witness], target) &&
                    token_rank(children[witness], target) + 1 == expectedRank;
              }
          } else {
              std::uint32_t maximum = 0;
              for (const InformationToken child : children) {
                  if (!token_value(child, target)) {
                      expected = false;
                      break;
                  }
                  maximum = std::max(maximum, token_rank(child, target));
              }
              if (expected) {
                  expectedRank = maximum + 1;
                  const std::uint32_t witness = target.witnesses[parent];
                  witnessValid = children.empty() ? witness == NoWitness :
                    witness < children.size() &&
                    token_value(children[witness], target) &&
                    token_rank(children[witness], target) == maximum;
              }
          }
          const bool actual = target.values[parent] != 0;
          residual.bellman += actual != expected;
          if (actual)
              residual.rank += target.ranks[parent] != expectedRank || !witnessValid;
          else
              residual.rank += target.ranks[parent] != 0 ||
                               target.witnesses[parent] != NoWitness;
      });
    for (std::size_t side = 0; side < 2; ++side)
        residual.equation += std::count(seen[side].begin(), seen[side].end(), 0) +
          (tokens[side] != capture.plan.tokens[side]) +
          (references[side] != capture.plan.variableReferences[side]);
    return residual;
}

[[nodiscard]] Summary write_dense_overlay(
  const std::string& path, const std::string& sourceSha,
  const std::string& originalModelSha, const PackedWdl& concrete,
  const JointArena& arena, const CaptureSolutions& capture) {
    Summary summary;
    std::vector<std::uint8_t> flags(ConcreteCount, 0);
    std::vector<std::uint8_t> rootSeen(arena.nodes().size(), 0);
    for (std::uint32_t index = 0; index < ConcreteCount; ++index) {
        const FourState state = decode_four(index);
        const std::size_t side = static_cast<std::size_t>(state.side);
        const Wdl exact = concrete.result(index);
        const RootLookup root = root_lookup(arena, state);
        if (!root.admitted) {
            ++summary.unreachable[side][static_cast<std::size_t>(exact)];
            continue;
        }
        const std::uint8_t sideBit = static_cast<std::uint8_t>(1u << side);
        if (!(rootSeen[root.node] & sideBit)) {
            rootSeen[root.node] |= sideBit;
            ++summary.sets[side];
        }
        const bool white = capture.targets[0].values[
          4ULL * root.node + root.bitIndex] != 0;
        const bool black = capture.targets[1].values[
          4ULL * root.node + root.bitIndex] != 0;
        summary.dualResidual += white && black;
        flags[index] = static_cast<std::uint8_t>(4 | (white ? 1 : 0) |
                                                 (black ? 2 : 0));
        const bool mover = state.side == Color::White ? white : black;
        const bool opponent = state.side == Color::White ? black : white;
        ++summary.totals[side][mover ? 1 : opponent ? 2 : 3];
    }
    for (std::size_t side = 0; side < 2; ++side) {
        std::uint64_t total = 0;
        for (std::size_t result = 1; result < 4; ++result)
            total += summary.totals[side][result] +
                     summary.unreachable[side][result];
        summary.conservationResidual += total != ConcreteCount / 2;
    }
    if (summary.dualResidual || summary.conservationResidual)
        throw std::runtime_error("joint capture fresh summary residual");
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write("UFIW2\0\0\0", 8);
    const auto word = [&](std::uint32_t value) {
        output.write(reinterpret_cast<const char*>(&value), 4);
    };
    word(2); word(static_cast<std::uint32_t>(PieceType::Jester));
    word(static_cast<std::uint32_t>(PieceType::Jester));
    word(static_cast<std::uint32_t>(Color::Black));
    word(ConcreteCount); word(1);
    output.write(sourceSha.data(), 64);
    output.write(originalModelSha.data(), 64);
    write_all(output, flags.data(), flags.size());
    return summary;
}

void store_summary(Header& header, const Summary& summary) {
    std::size_t cursor = 64;
    for (const auto& side : summary.totals)
        for (const std::uint64_t count : side) {
            header.put64(cursor, count); cursor += 8;
        }
    cursor = 128;
    for (const auto& side : summary.unreachable)
        for (const std::uint64_t count : side) {
            header.put64(cursor, count); cursor += 8;
        }
    header.put64(192, summary.sets[0]);
    header.put64(200, summary.sets[1]);
    header.put64(304, summary.dualResidual);
    header.put64(312, summary.conservationResidual);
}

[[nodiscard]] Summary load_summary(const Header& header) {
    Summary summary;
    std::size_t cursor = 64;
    for (auto& side : summary.totals)
        for (std::uint64_t& count : side) {
            count = header.get64(cursor); cursor += 8;
        }
    cursor = 128;
    for (auto& side : summary.unreachable)
        for (std::uint64_t& count : side) {
            count = header.get64(cursor); cursor += 8;
        }
    summary.sets[0] = header.get64(192);
    summary.sets[1] = header.get64(200);
    summary.dualResidual = header.get64(304);
    summary.conservationResidual = header.get64(312);
    return summary;
}

[[nodiscard]] std::string raw_path(const std::string& prefix,
                                   const char* suffix) {
    return prefix + suffix;
}

void write_capture_files(const std::string& prefix, const Bindings& bindings,
                         const JointArena& arena,
                         const CaptureSolutions& capture,
                         const Summary& summary,
                         const std::string& overlay) {
    const auto& nodes = arena.nodes();
    {
        std::ofstream output(raw_path(prefix, ".nodes"),
          std::ios::binary | std::ios::trunc);
        LittleEndianWriter writer(output);
        for (const std::uint64_t key : nodes) writer.put64(key);
        writer.finish();
    }
    const auto write_values = [&](const char* suffix, auto member) {
        std::ofstream output(raw_path(prefix, suffix),
          std::ios::binary | std::ios::trunc);
        for (const CapturedTarget& target : capture.targets) {
            const auto& values = target.*member;
            write_all(output, values.data(), values.size() * sizeof(values[0]));
        }
    };
    const auto write_words = [&](const char* suffix, auto member) {
        std::ofstream output(raw_path(prefix, suffix),
          std::ios::binary | std::ios::trunc);
        LittleEndianWriter writer(output);
        for (const CapturedTarget& target : capture.targets)
          for (const std::uint32_t value : target.*member)
            writer.put32(value);
        writer.finish();
    };
    write_values(".values", &CapturedTarget::values);
    write_words(".ranks", &CapturedTarget::ranks);
    write_words(".witnesses", &CapturedTarget::witnesses);

    Header header;
    initialize_header(header, CaptureMagic, CaptureKind);
    header.put64(40, nodes.size());
    header.put64(48, arena.admitted_roots());
    header.put64(216, capture.plan.baseVariables);
    header.put64(224, capture.plan.variables[0]);
    header.put64(232, capture.plan.variables[1]);
    header.put64(240, capture.plan.gateBase[0].back());
    header.put64(248, capture.plan.gateBase[1].back());
    header.put64(256, capture.plan.tokens[0]);
    header.put64(264, capture.plan.tokens[1]);
    header.put64(272, capture.plan.variableReferences[0]);
    header.put64(280, capture.plan.variableReferences[1]);
    header.put64(288, capture.targets[0].fixedPoint.activated);
    header.put64(296, capture.targets[1].fixedPoint.activated);
    store_summary(header, summary);
    header.put_hash(SourceSha, bindings.sourceSha);
    header.put_hash(OriginalModelSha, bindings.originalModelSha);
    header.put_hash(CaptureModelSha, bindings.captureModelSha);
    header.put_hash(ObservationSha, bindings.observationSha);
    header.put_hash(LowerTableSha, bindings.lowerTableSha);
    header.put_hash(LowerOverlaySha, bindings.lowerOverlaySha);
    header.put_hash(LowerModelSha, bindings.lowerModelSha);
    header.put_hash(DenseOverlaySha, sha256_file(overlay));
    header.put_hash(NodesSha, sha256_file(raw_path(prefix, ".nodes")));
    header.put_hash(ValuesSha, sha256_file(raw_path(prefix, ".values")));
    header.put_hash(RanksSha, sha256_file(raw_path(prefix, ".ranks")));
    header.put_hash(WitnessesSha, sha256_file(raw_path(prefix, ".witnesses")));
    // The metadata file cannot recursively authenticate its own bytes.  Its
    // SHA-256 is carried by both the deterministic recovery archive manifest
    // and the derived arbitrary-history sidecar instead.
    header.put_hash(RawMetaSha, std::string(64, '0'));
    header.put_hash(ProofLogSha, bindings.proofLogSha);
    write_header(raw_path(prefix, ".meta"), header);
}

[[nodiscard]] std::uint8_t transform_square_d2(std::uint8_t square,
                                               unsigned transform) {
    int file = square % Position::BoardFiles;
    int rank = square / Position::BoardFiles;
    if (transform & 1) file = Position::BoardFiles - 1 - file;
    if (transform & 2) rank = Position::BoardRanks - 1 - rank;
    return static_cast<std::uint8_t>(rank * Position::BoardFiles + file);
}

struct TransformedKey {
    std::uint64_t key = 0;
    std::array<std::uint8_t, 4> bitMap{{0, 1, 2, 3}};
};

[[nodiscard]] TransformedKey transform_key_d2(std::uint64_t key,
                                              unsigned transform) {
    const Geometry source = node_geometry(key);
    struct Slot { std::uint8_t square; std::uint8_t old; };
    std::array<Slot, 2> white{{
      {transform_square_d2(source.white[0], transform), 0},
      {transform_square_d2(source.white[1], transform), 1}}};
    std::array<Slot, 2> black{{
      {transform_square_d2(source.black[0], transform), 0},
      {transform_square_d2(source.black[1], transform), 1}}};
    const auto order = [](const Slot& lhs, const Slot& rhs) {
        return lhs.square < rhs.square;
    };
    std::sort(white.begin(), white.end(), order);
    std::sort(black.begin(), black.end(), order);
    Geometry geometry{source.side,
      {white[0].square, white[1].square},
      {black[0].square, black[1].square}, source.terminal};
    const Geometry canonical = canonicalize_public(geometry);
    if (!(canonical == geometry)) {
        for (Slot& slot : white) slot.square = reflect_square(slot.square);
        for (Slot& slot : black) slot.square = reflect_square(slot.square);
        std::sort(white.begin(), white.end(), order);
        std::sort(black.begin(), black.end(), order);
        geometry = {source.side, {white[0].square, white[1].square},
          {black[0].square, black[1].square}, source.terminal};
    }
    std::array<std::uint8_t, 2> whiteMap{}, blackMap{};
    for (std::uint8_t slot = 0; slot < 2; ++slot) {
        whiteMap[white[slot].old] = slot;
        blackMap[black[slot].old] = slot;
    }
    TransformedKey result;
    std::uint8_t relation = 0;
    std::uint8_t whitePartition = 0;
    std::uint8_t blackPartition = 0;
    const std::uint8_t oldRelation = node_relation(key);
    const std::uint8_t oldWhite = node_partition(key, Color::White);
    const std::uint8_t oldBlack = node_partition(key, Color::Black);
    for (unsigned old = 0; old < 4; ++old) {
        const unsigned mapped = 2 * whiteMap[white_class(old)] +
                                blackMap[black_class(old)];
        result.bitMap[old] = static_cast<std::uint8_t>(mapped);
        if (oldRelation & (1u << old)) relation |= static_cast<std::uint8_t>(1u << mapped);
    }
    for (unsigned first = 0; first < 4; ++first)
      for (unsigned second = first + 1; second < 4; ++second) {
        const unsigned mappedFirst = result.bitMap[first];
        const unsigned mappedSecond = result.bitMap[second];
        if (partition_equivalent(oldWhite, first, second))
            whitePartition |= static_cast<std::uint8_t>(
              1u << pair_index(mappedFirst, mappedSecond));
        if (partition_equivalent(oldBlack, first, second))
            blackPartition |= static_cast<std::uint8_t>(
              1u << pair_index(mappedFirst, mappedSecond));
      }
    result.key = node_key(geometry, relation,
      restrict_partition(whitePartition, relation),
      restrict_partition(blackPartition, relation));
    return result;
}

[[nodiscard]] std::uint8_t permute_forces(std::uint8_t value,
                                         const TransformedKey& transform) {
    std::uint8_t result = 0;
    for (unsigned target = 0; target < 2; ++target)
      for (unsigned old = 0; old < 4; ++old)
        if (value & (1u << (4 * target + old)))
            result |= static_cast<std::uint8_t>(
              1u << (4 * target + transform.bitMap[old]));
    return result;
}

struct RawCapture {
    Header header;
    MappedFile nodes;
    MappedFile values;
    MappedFile ranks;
    MappedFile witnesses;
    std::uint64_t nodeCount = 0;
    std::array<std::uint64_t, 2> variables{};

    RawCapture(const std::string& prefix, const Bindings& bindings)
      : header(read_header(raw_path(prefix, ".meta"))),
        nodes(raw_path(prefix, ".nodes")), values(raw_path(prefix, ".values")),
        ranks(raw_path(prefix, ".ranks")),
        witnesses(raw_path(prefix, ".witnesses")) {
        validate_common_header(header, CaptureMagic, CaptureKind, bindings);
        nodeCount = header.get64(40);
        variables = {header.get64(224), header.get64(232)};
        if (!nodeCount || header.get64(216) != 4 * nodeCount ||
            nodes.size() != 8 * nodeCount ||
            values.size() != variables[0] + variables[1] ||
            ranks.size() != 4 * (variables[0] + variables[1]) ||
            witnesses.size() != 4 * (variables[0] + variables[1]) ||
            header.hash(NodesSha) != sha256_file(raw_path(prefix, ".nodes")) ||
            header.hash(ValuesSha) != sha256_file(raw_path(prefix, ".values")) ||
            header.hash(RanksSha) != sha256_file(raw_path(prefix, ".ranks")) ||
            header.hash(WitnessesSha) != sha256_file(raw_path(prefix, ".witnesses")))
            throw std::runtime_error("joint raw capture extent/hash residual");
    }
};

[[nodiscard]] std::uint64_t read_le64(const std::uint8_t* data) {
    std::uint64_t value = 0;
    for (unsigned byte = 0; byte < 8; ++byte)
        value |= std::uint64_t(data[byte]) << (8 * byte);
    return value;
}

[[nodiscard]] std::uint32_t read_le32(const std::uint8_t* data) {
    std::uint32_t value = 0;
    for (unsigned byte = 0; byte < 4; ++byte)
        value |= std::uint32_t(data[byte]) << (8 * byte);
    return value;
}

[[nodiscard]] std::vector<std::uint64_t> load_node_keys(const RawCapture& raw) {
    std::vector<std::uint64_t> nodes(static_cast<std::size_t>(raw.nodeCount));
    for (std::uint64_t index = 0; index < raw.nodeCount; ++index)
        nodes[index] = read_le64(raw.nodes.data() + 8 * index);
    return nodes;
}

[[nodiscard]] CaptureSolutions load_solutions(const RawCapture& raw) {
    CaptureSolutions capture;
    capture.plan.baseVariables = raw.header.get64(216);
    capture.plan.variables = raw.variables;
    capture.plan.tokens = {raw.header.get64(256), raw.header.get64(264)};
    capture.plan.variableReferences = {raw.header.get64(272), raw.header.get64(280)};
    std::uint64_t offset = 0;
    for (std::size_t side = 0; side < 2; ++side) {
        CapturedTarget& target = capture.targets[side];
        target.target = static_cast<Color>(side);
        const std::size_t count = static_cast<std::size_t>(raw.variables[side]);
        target.values.assign(raw.values.data() + offset,
                             raw.values.data() + offset + count);
        target.ranks.resize(count);
        target.witnesses.resize(count);
        for (std::size_t index = 0; index < count; ++index) {
            if (target.values[index] > 1)
                throw std::runtime_error("joint raw force value is not Boolean");
            target.ranks[index] = read_le32(raw.ranks.data() + 4 * (offset + index));
            target.witnesses[index] = read_le32(
              raw.witnesses.data() + 4 * (offset + index));
        }
        const std::uint64_t activated = static_cast<std::uint64_t>(std::count(
          target.values.begin(), target.values.end(), std::uint8_t{1}));
        if (activated != raw.header.get64(288 + 8 * side))
            throw std::runtime_error("joint raw activation-count residual");
        offset += count;
    }
    return capture;
}

void validate_node_domain(const std::vector<std::uint64_t>& nodes) {
    std::vector<std::uint64_t> sorted = nodes;
    std::sort(sorted.begin(), sorted.end());
    if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end())
        throw std::runtime_error("joint raw graph has duplicate full node keys");
    for (const std::uint64_t key : nodes) {
        const Geometry geometry = node_geometry(key);
        const std::uint8_t relation = node_relation(key);
        if (!relation || !(canonicalize_public(geometry) == geometry) ||
            !valid_partition(node_partition(key, Color::White), relation) ||
            !valid_partition(node_partition(key, Color::Black), relation))
            throw std::runtime_error("joint raw graph node-domain residual");
        for (unsigned bit = 0; bit < 4; ++bit)
            if (relation & (1u << bit)) (void)position_for(key, bit);
    }
}

[[nodiscard]] std::uint8_t force_byte(const CaptureSolutions& capture,
                                      std::uint64_t node) {
    std::uint8_t value = 0;
    for (unsigned side = 0; side < 2; ++side)
      for (unsigned bit = 0; bit < 4; ++bit)
        if (capture.targets[side].values.at(4 * node + bit))
            value |= static_cast<std::uint8_t>(1u << (4 * side + bit));
    return value;
}

struct SidecarEntry {
    std::uint64_t key = 0;
    std::uint8_t forces = 0;
};

[[nodiscard]] std::vector<SidecarEntry> quotient_entries(
  const std::vector<std::uint64_t>& nodes, const CaptureSolutions& capture,
  std::uint64_t& symmetryResidual, std::uint64_t& dualResidual) {
    struct NodeRef { std::uint64_t key; std::uint32_t id; };
    std::vector<NodeRef> lookup;
    lookup.reserve(nodes.size());
    for (std::uint32_t id = 0; id < nodes.size(); ++id)
        lookup.push_back({nodes[id], id});
    std::sort(lookup.begin(), lookup.end(), [](const NodeRef& lhs,
                                               const NodeRef& rhs) {
        return lhs.key < rhs.key;
    });
    if (std::adjacent_find(lookup.begin(), lookup.end(),
          [](const NodeRef& lhs, const NodeRef& rhs) {
              return lhs.key == rhs.key;
          }) != lookup.end())
        throw std::runtime_error("joint D2 lookup duplicate");
    const auto find = [&](std::uint64_t key) -> const NodeRef* {
        const auto iterator = std::lower_bound(lookup.begin(), lookup.end(), key,
          [](const NodeRef& item, std::uint64_t wanted) {
              return item.key < wanted;
          });
        return iterator != lookup.end() && iterator->key == key
             ? &*iterator : nullptr;
    };
    std::vector<SidecarEntry> result;
    result.reserve(nodes.size());
    for (std::uint32_t id = 0; id < nodes.size(); ++id) {
        const std::uint8_t original = force_byte(capture, id);
        for (unsigned bit = 0; bit < 4; ++bit)
            dualResidual += (original & (1u << bit)) &&
                            (original & (1u << (4 + bit)));
        std::uint64_t best = std::numeric_limits<std::uint64_t>::max();
        std::uint8_t bestForces = 0;
        for (unsigned transform = 0; transform < 4; ++transform) {
            const TransformedKey mapped = transform_key_d2(nodes[id], transform);
            const NodeRef* found = find(mapped.key);
            if (!found) {
                ++symmetryResidual;
                continue;
            }
            const std::uint8_t expected = permute_forces(original, mapped);
            symmetryResidual += force_byte(capture, found->id) != expected;
            if (mapped.key < best) { best = mapped.key; bestForces = expected; }
        }
        if (best == nodes[id]) result.push_back({best, bestForces});
    }
    std::sort(result.begin(), result.end(), [](const SidecarEntry& lhs,
                                                const SidecarEntry& rhs) {
        return lhs.key < rhs.key;
    });
    for (std::size_t index = 1; index < result.size(); ++index)
        if (result[index - 1].key == result[index].key) {
            symmetryResidual += result[index - 1].forces != result[index].forces;
            throw std::runtime_error("joint D2 representative is not unique");
        }
    return result;
}

void write_sidecar(const std::string& path, const Bindings& bindings,
                   const RawCapture& raw,
                   const std::vector<SidecarEntry>& entries,
                   std::uint64_t symmetryResidual,
                   std::uint64_t dualResidual,
                   const std::string& rawMetaSha) {
    if (symmetryResidual || dualResidual)
        throw std::runtime_error("joint sidecar quotient certificate residual");
    Header header = raw.header;
    initialize_header(header, SidecarMagic, SidecarKind);
    header.put64(40, raw.nodeCount);
    header.put64(48, entries.size());
    header.put64(216, 9);
    header.put64(304, dualResidual);
    header.put64(312, symmetryResidual);
    header.put_hash(RawMetaSha, rawMetaSha);
    validate_common_header(header, SidecarMagic, SidecarKind, bindings);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    write_all(output, header.bytes.data(), header.bytes.size());
    LittleEndianWriter writer(output);
    for (const SidecarEntry& entry : entries) {
        writer.put64(entry.key);
        writer.put8(entry.forces);
    }
    writer.finish();
    output.close();
    if (!output) throw std::runtime_error("cannot write joint arbitrary sidecar");
    header.put_hash(PayloadSha, sha256_file(path, HeaderBytes));
    std::fstream rewrite(path, std::ios::binary | std::ios::in | std::ios::out);
    rewrite.write(reinterpret_cast<const char*>(header.bytes.data()), header.bytes.size());
    if (!rewrite) throw std::runtime_error("cannot finalize joint arbitrary sidecar");
}

class ArbitraryProbe {
  public:
    ArbitraryProbe(const std::string& path, const Bindings& bindings,
                   const std::string& fullSha = {})
      : file_(path), header_(read_embedded_header()) {
        validate_common_header(header_, SidecarMagic, SidecarKind, bindings);
        nodeCount_ = header_.get64(40);
        entryCount_ = header_.get64(48);
        constexpr std::array<HashSlot, 6> RawHashSlots{{DenseOverlaySha,
          NodesSha, ValuesSha, RanksSha, WitnessesSha, RawMetaSha}};
        const bool rawHashesValid = std::all_of(
          RawHashSlots.begin(), RawHashSlots.end(),
          [&](HashSlot slot) { return valid_sha(header_.hash(slot)); });
        if (!nodeCount_ || !entryCount_ || entryCount_ > nodeCount_ ||
            header_.get64(216) != 9 || header_.get64(304) ||
            header_.get64(312) || !rawHashesValid ||
            file_.size() != HeaderBytes + 9 * entryCount_ ||
            header_.hash(PayloadSha) != sha256_file(path, HeaderBytes) ||
            (!fullSha.empty() && sha256_file(path) != fullSha))
            throw std::runtime_error("joint arbitrary sidecar extent/hash residual");
        std::uint64_t previous = 0;
        for (std::uint64_t index = 0; index < entryCount_; ++index) {
            const std::uint64_t key = entry_key(index);
            const std::uint8_t forces = entry_forces(index);
            const std::uint8_t relation = node_relation(key);
            if ((index && key <= previous) || !relation ||
                !(canonicalize_public(node_geometry(key)) == node_geometry(key)) ||
                !valid_partition(node_partition(key, Color::White), relation) ||
                !valid_partition(node_partition(key, Color::Black), relation))
                throw std::runtime_error("joint arbitrary sidecar index residual");
            for (unsigned bit = 0; bit < 4; ++bit) {
                if (!(relation & (1u << bit)) &&
                    (forces & ((1u << bit) | (1u << (4 + bit)))))
                    throw std::runtime_error("joint arbitrary sidecar force-domain residual");
                if ((forces & (1u << bit)) && (forces & (1u << (4 + bit))))
                    throw std::runtime_error("joint arbitrary sidecar dual force residual");
            }
            previous = key;
        }
    }

    [[nodiscard]] std::pair<bool, bool> forces(std::uint64_t key,
                                               unsigned actualBit) const {
        const std::uint8_t relation = node_relation(key);
        if (actualBit >= 4 || !(relation & (1u << actualBit)) ||
            !valid_partition(node_partition(key, Color::White), relation) ||
            !valid_partition(node_partition(key, Color::Black), relation))
            throw std::invalid_argument("invalid joint arbitrary query");
        std::uint64_t best = std::numeric_limits<std::uint64_t>::max();
        unsigned mappedBit = 0;
        for (unsigned transform = 0; transform < 4; ++transform) {
            const TransformedKey mapped = transform_key_d2(key, transform);
            if (mapped.key < best) {
                best = mapped.key;
                mappedBit = mapped.bitMap[actualBit];
            }
        }
        std::uint64_t low = 0, high = entryCount_;
        while (low < high) {
            const std::uint64_t middle = low + (high - low) / 2;
            if (entry_key(middle) < best) low = middle + 1;
            else high = middle;
        }
        if (low == entryCount_ || entry_key(low) != best)
            throw std::out_of_range("joint arbitrary query is not reachable");
        const std::uint8_t value = entry_forces(low);
        return {(value & (1u << mappedBit)) != 0,
                (value & (1u << (4 + mappedBit))) != 0};
    }

    [[nodiscard]] std::uint64_t nodes() const { return nodeCount_; }
    [[nodiscard]] std::uint64_t entries() const { return entryCount_; }

  private:
    [[nodiscard]] Header read_embedded_header() const {
        if (file_.size() < HeaderBytes)
            throw std::runtime_error("truncated joint arbitrary sidecar");
        Header result;
        std::memcpy(result.bytes.data(), file_.data(), HeaderBytes);
        return result;
    }
    [[nodiscard]] std::uint64_t entry_key(std::uint64_t index) const {
        return read_le64(file_.data() + HeaderBytes + 9 * index);
    }
    [[nodiscard]] std::uint8_t entry_forces(std::uint64_t index) const {
        return file_.data()[HeaderBytes + 9 * index + 8];
    }
    MappedFile file_;
    Header header_;
    std::uint64_t nodeCount_ = 0;
    std::uint64_t entryCount_ = 0;
};

struct Inputs {
    std::string input;
    std::string lowerTable;
    std::string lowerOverlay;
    std::string scratch;
    Bindings bindings;
};

[[nodiscard]] PackedWdl authenticated_concrete(const Inputs& inputs) {
    if (sha256_file(inputs.input) != inputs.bindings.sourceSha ||
        sha256_file(inputs.lowerTable) != inputs.bindings.lowerTableSha ||
        sha256_file(inputs.lowerOverlay) != inputs.bindings.lowerOverlaySha)
        throw std::runtime_error("joint preservation input SHA mismatch");
    PackedWdl concrete = load_table(inputs.input, ConcreteCount,
                                    PieceType::Jester, Color::Black);
    if (hex_digest(concrete.sha) != inputs.bindings.sourceSha)
        throw std::runtime_error("joint preservation logical source mismatch");
    return concrete;
}

[[nodiscard]] std::unique_ptr<JointArena> rebuild_arena(
  const Inputs& inputs, PackedWdl& lowerConcrete,
  std::unique_ptr<LowerOracle>& lower) {
    lowerConcrete = load_table(inputs.lowerTable, LowerCount,
                               PieceType::Count, Color::White);
    lower = std::make_unique<LowerOracle>(std::move(lowerConcrete),
      inputs.lowerOverlay, inputs.bindings.lowerModelSha);
    auto arena = std::make_unique<JointArena>(*lower);
    arena->build_roots();
    arena->expand_all();
    return arena;
}

struct ResourcePlan {
    std::uint64_t nodes = 0;
    std::array<std::uint64_t, 2> variables{};
    std::array<std::uint64_t, 2> tokens{};
    std::array<std::uint64_t, 2> references{};
    std::uint64_t rawBytes = 0;
    std::uint64_t scratchBytes = 0;
    std::uint64_t residentBytes = 0;
    std::uint64_t sidecarBytes = 0;
};

[[nodiscard]] ResourcePlan measure_plan(const JointArena& arena) {
    ResourcePlan result;
    EquationPlan plan = equation_plan(arena);
    emit_equations(arena, plan,
      [&](std::size_t side, InformationToken, EquationKind,
          const std::vector<InformationToken>& tokens) {
          plan.tokens[side] += tokens.size();
          plan.variableReferences[side] += std::count_if(
            tokens.begin(), tokens.end(), [](InformationToken token) {
                return token < InformationTrue;
            });
      });
    result.nodes = arena.nodes().size();
    result.variables = plan.variables;
    result.tokens = plan.tokens;
    result.references = plan.variableReferences;
    const std::uint64_t sumVariables = plan.variables[0] + plan.variables[1];
    const std::uint64_t equationBytes = 16 * sumVariables +
      4 * (plan.tokens[0] + plan.tokens[1]);
    const auto solveExtra = [&](std::size_t side) {
        return 30 * plan.variables[side] + 12 * plan.variableReferences[side];
    };
    result.scratchBytes = equationBytes + std::max(solveExtra(0), solveExtra(1));
    result.rawBytes = 8 * result.nodes + 9 * sumVariables +
                      HeaderBytes + ConcreteCount + 160;
    // Conservative peak: full node interner, both equation streams, the
    // active propagation arrays, and one already-extracted target solution.
    // The runner must compare the measured service peak with this estimate
    // before authorizing --full; it is intentionally not an RSS promise.
    result.residentBytes = 64 * result.nodes + 32 * sumVariables +
                           4 * (plan.tokens[0] + plan.tokens[1]) +
                           12 * std::max(plan.variableReferences[0],
                                         plan.variableReferences[1]);
    result.sidecarBytes = HeaderBytes + 9 * result.nodes; // before D2 quotient
    return result;
}

[[nodiscard]] std::uint64_t filesystem_available(const std::string& path) {
    struct statvfs status{};
    if (::statvfs(path.c_str(), &status))
        throw std::runtime_error(system_error("cannot statvfs", path));
    return static_cast<std::uint64_t>(status.f_bavail) * status.f_frsize;
}

[[nodiscard]] std::uint64_t physical_memory() {
    const long pages = ::sysconf(_SC_PHYS_PAGES);
    const long bytes = ::sysconf(_SC_PAGESIZE);
    if (pages <= 0 || bytes <= 0) return 0;
    return static_cast<std::uint64_t>(pages) * static_cast<std::uint64_t>(bytes);
}

void resource_gate(const ResourcePlan& plan, const std::string& scratch,
                   std::uint64_t scratchLimit, std::uint64_t residentLimit) {
    const std::uint64_t needed = plan.scratchBytes + plan.rawBytes;
    const std::uint64_t available = filesystem_available(scratch);
    const std::uint64_t memory = physical_memory();
    if (!scratchLimit || needed > scratchLimit || needed > available * 9 / 10)
        throw std::runtime_error("joint capture scratch resource gate rejected run");
    // Arena/interner and mapped solver arrays are empirically dominant.  The
    // explicit budget is still bounded against physical RAM; measurement is
    // required before --full by the runner.
    if (!residentLimit || plan.residentBytes > residentLimit ||
        (memory && residentLimit > memory * 9 / 10))
        throw std::runtime_error("joint capture resident resource gate rejected run");
}

void compare_ordered_nodes(const JointArena& arena,
                           const std::vector<std::uint64_t>& nodes) {
    if (arena.nodes() != nodes)
        throw std::runtime_error("joint restored graph/order residual");
}

void compare_summary(const Summary& expected, const Summary& actual) {
    if (expected.totals != actual.totals ||
        expected.unreachable != actual.unreachable ||
        expected.sets != actual.sets ||
        expected.dualResidual != actual.dualResidual ||
        expected.conservationResidual != actual.conservationResidual)
        throw std::runtime_error("joint restored fresh summary residual");
}

[[nodiscard]] std::string restore_overlay_path(const std::string& scratch) {
    return (std::filesystem::path(scratch) /
            "joint-jester-restored.ufiw").string();
}

struct StrictCertificate {
    std::uint64_t nodes = 0;
    std::uint64_t entries = 0;
    std::uint64_t bellmanResidual = 0;
    std::uint64_t rankResidual = 0;
    std::uint64_t symmetryResidual = 0;
    std::uint64_t singletonResidual = 0;
    std::string sidecarSha;
};

[[nodiscard]] StrictCertificate export_and_strict_restore(
  const Inputs& inputs, const std::string& rawPrefix,
  const std::string& sidecarPath, const std::string& expectedSidecarSha = {}) {
    RawCapture raw(rawPrefix, inputs.bindings);
    if (raw.header.hash(RawMetaSha) != std::string(64, '0')) {
        // Capture metadata is externally authenticated by the deterministic
        // raw archive manifest; it must not recursively claim its own hash.
        throw std::runtime_error("joint raw metadata self-hash is nonzero");
    }
    std::vector<std::uint64_t> nodes = load_node_keys(raw);
    validate_node_domain(nodes);
    CaptureSolutions capture = load_solutions(raw);
    PackedWdl concrete = authenticated_concrete(inputs);
    PackedWdl lowerConcrete;
    std::unique_ptr<LowerOracle> lower;
    std::unique_ptr<JointArena> arena = rebuild_arena(inputs, lowerConcrete, lower);
    compare_ordered_nodes(*arena, nodes);
    EquationPlan regenerated = equation_plan(*arena);
    if (regenerated.baseVariables != capture.plan.baseVariables ||
        regenerated.variables != capture.plan.variables ||
        regenerated.gateBase[0].back() != raw.header.get64(240) ||
        regenerated.gateBase[1].back() != raw.header.get64(248))
        throw std::runtime_error("joint restored equation-plan residual");
    regenerated.tokens = capture.plan.tokens;
    regenerated.variableReferences = capture.plan.variableReferences;
    capture.plan = std::move(regenerated);
    const RestoreResidual fixed = verify_captured_equations(*arena, capture);
    if (fixed.equation || fixed.bellman || fixed.rank)
        throw std::runtime_error("joint restored fixed-point residual");

    const std::string restoredOverlay = restore_overlay_path(inputs.scratch);
    const Summary summary = write_dense_overlay(restoredOverlay,
      inputs.bindings.sourceSha, inputs.bindings.originalModelSha,
      concrete, *arena, capture);
    compare_summary(load_summary(raw.header), summary);
    if (sha256_file(restoredOverlay) != raw.header.hash(DenseOverlaySha))
        throw std::runtime_error("joint restored dense overlay residual");

    std::uint64_t symmetryResidual = 0, dualResidual = 0;
    const std::vector<SidecarEntry> entries = quotient_entries(
      nodes, capture, symmetryResidual, dualResidual);
    const std::string rawMetaSha = sha256_file(raw_path(rawPrefix, ".meta"));
    const bool verifyExisting = !expectedSidecarSha.empty();
    const std::string generatedPath = verifyExisting
      ? (std::filesystem::path(inputs.scratch) /
         ("joint-jester-verify-" + std::to_string(::getpid()) + ".ufja")).string()
      : sidecarPath;
    write_sidecar(generatedPath, inputs.bindings, raw, entries,
                  symmetryResidual, dualResidual, rawMetaSha);
    const std::string fullSha = sha256_file(generatedPath);
    if (verifyExisting && (expectedSidecarSha != fullSha ||
                           sha256_file(sidecarPath) != expectedSidecarSha)) {
        std::filesystem::remove(generatedPath);
        throw std::runtime_error("joint sidecar deterministic restore SHA mismatch");
    }
    if (verifyExisting) std::filesystem::remove(generatedPath);
    ArbitraryProbe probe(sidecarPath, inputs.bindings, fullSha);
    std::uint64_t singletonResidual = 0;
    for (std::uint64_t node = 0; node < nodes.size(); ++node) {
        const std::uint8_t relation = node_relation(nodes[node]);
        for (unsigned bit = 0; bit < 4; ++bit)
            if (relation & (1u << bit)) {
                const auto result = probe.forces(nodes[node], bit);
                singletonResidual += result.first !=
                  bool(capture.targets[0].values[4 * node + bit]);
                singletonResidual += result.second !=
                  bool(capture.targets[1].values[4 * node + bit]);
            }
    }
    if (singletonResidual)
        throw std::runtime_error("joint sidecar singleton reproduction residual");
    return {nodes.size(), entries.size(), fixed.bellman, fixed.rank,
            symmetryResidual, singletonResidual, fullSha};
}

struct Cli {
    enum class Command { None, SelfTest, Measure, Capture, Export, Verify, Probe,
                         BindProof } command = Command::None;
    Inputs inputs;
    std::string rawPrefix;
    std::string outputOverlay;
    std::string outputSidecar;
    std::string sidecarSha;
    std::string proofLog;
    std::uint64_t scratchLimit = 0;
    std::uint64_t residentLimit = 0;
    std::uint64_t probeKey = 0;
    unsigned probeBit = 0;
};

[[nodiscard]] std::uint64_t parse_u64(const std::string& value) {
    std::size_t consumed = 0;
    const std::uint64_t result = std::stoull(value, &consumed, 0);
    if (consumed != value.size()) throw std::runtime_error("invalid integer " + value);
    return result;
}

[[nodiscard]] Cli parse_cli(int argc, char** argv) {
    Cli result;
    const auto choose = [&](Cli::Command command) {
        if (result.command != Cli::Command::None)
            throw std::runtime_error("choose one joint preservation command");
        result.command = command;
    };
    for (int index = 1; index < argc; ++index) {
        const std::string option = argv[index];
        const auto value = [&]() {
            if (++index >= argc) throw std::runtime_error(option + " needs a value");
            return std::string(argv[index]);
        };
        if (option == "--self-test") choose(Cli::Command::SelfTest);
        else if (option == "--measure") choose(Cli::Command::Measure);
        else if (option == "--capture") choose(Cli::Command::Capture);
        else if (option == "--export") choose(Cli::Command::Export);
        else if (option == "--verify") choose(Cli::Command::Verify);
        else if (option == "--probe") choose(Cli::Command::Probe);
        else if (option == "--bind-proof-log") choose(Cli::Command::BindProof);
        else if (option == "--input") result.inputs.input = value();
        else if (option == "--lower-table") result.inputs.lowerTable = value();
        else if (option == "--lower-overlay") result.inputs.lowerOverlay = value();
        else if (option == "--scratch") result.inputs.scratch = value();
        else if (option == "--raw-prefix") result.rawPrefix = value();
        else if (option == "--output") result.outputOverlay = value();
        else if (option == "--output-sidecar") result.outputSidecar = value();
        else if (option == "--source-sha256") result.inputs.bindings.sourceSha = value();
        else if (option == "--original-model-sha256")
            result.inputs.bindings.originalModelSha = value();
        else if (option == "--capture-model-sha256")
            result.inputs.bindings.captureModelSha = value();
        else if (option == "--observation-sha256")
            result.inputs.bindings.observationSha = value();
        else if (option == "--lower-table-sha256")
            result.inputs.bindings.lowerTableSha = value();
        else if (option == "--lower-overlay-sha256")
            result.inputs.bindings.lowerOverlaySha = value();
        else if (option == "--lower-model-sha256")
            result.inputs.bindings.lowerModelSha = value();
        else if (option == "--proof-log-sha256")
            result.inputs.bindings.proofLogSha = value();
        else if (option == "--sidecar-sha256") result.sidecarSha = value();
        else if (option == "--proof-log") result.proofLog = value();
        else if (option == "--scratch-limit") result.scratchLimit = parse_u64(value());
        else if (option == "--resident-limit") result.residentLimit = parse_u64(value());
        else if (option == "--node-key") result.probeKey = parse_u64(value());
        else if (option == "--actual-bit") result.probeBit =
          static_cast<unsigned>(parse_u64(value()));
        else throw std::runtime_error("unknown joint preservation option " + option);
    }
    if (result.command == Cli::Command::None)
        throw std::runtime_error("missing joint preservation command");
    if (result.inputs.scratch.empty()) result.inputs.scratch = "/tmp";
    return result;
}

void require_bindings(const Bindings& bindings) {
    for (const std::string* hash : {&bindings.sourceSha,
         &bindings.originalModelSha, &bindings.captureModelSha,
         &bindings.observationSha, &bindings.lowerTableSha,
         &bindings.lowerOverlaySha, &bindings.lowerModelSha,
         &bindings.proofLogSha})
        if (!valid_sha(*hash)) throw std::runtime_error("missing preservation binding");
}

void bind_proof_log(const Cli& cli) {
    if (cli.rawPrefix.empty() || cli.proofLog.empty())
        throw std::runtime_error("proof-log binding requires raw prefix and log");
    Header header = read_header(raw_path(cli.rawPrefix, ".meta"));
    if (!std::equal(std::begin(CaptureMagic), std::end(CaptureMagic),
                    header.bytes.begin()))
        throw std::runtime_error("proof-log binding has wrong raw header");
    header.put_hash(ProofLogSha, sha256_file(cli.proofLog));
    write_header(raw_path(cli.rawPrefix, ".meta"), header);
    std::cout << "joint_capture_proof_log sha256 " << header.hash(ProofLogSha)
              << " residual 0\n";
}

void self_test(const std::string& scratch) {
    self_test_codec(scratch);
    Geometry geometry{Color::White, {0, 9}, {70, 79}, 0};
    geometry = canonicalize_public(geometry);
    const std::uint64_t key = node_key(geometry, 0xf,
      root_white_partition(), root_black_partition());
    std::array<std::uint64_t, 4> transformed{};
    for (unsigned transform = 0; transform < 4; ++transform) {
        const TransformedKey mapped = transform_key_d2(key, transform);
        transformed[transform] = mapped.key;
        if (!valid_partition(node_partition(mapped.key, Color::White), 0xf) ||
            !valid_partition(node_partition(mapped.key, Color::Black), 0xf))
            throw std::runtime_error("joint D2 self-test partition residual");
    }
    if (transformed[0] != key || transformed[1] != key ||
        transformed[2] != transformed[3])
        throw std::runtime_error("joint D2 self-test group residual");
    Header header;
    initialize_header(header, CaptureMagic, CaptureKind);
    header.put64(40, 1);
    if (header.get64(40) != 1 || header.get32(32) !=
          static_cast<std::uint32_t>(Color::Black))
        throw std::runtime_error("joint preservation header self-test residual");

    const Bindings bindings{std::string(64, '1'), std::string(64, '2'),
      std::string(64, '3'), std::string(64, '4'), std::string(64, '5'),
      std::string(64, '6'), std::string(64, '7'), std::string(64, '8')};
    const TransformedKey representative = [&]() {
        TransformedKey best;
        best.key = std::numeric_limits<std::uint64_t>::max();
        for (unsigned transform = 0; transform < 4; ++transform) {
            const TransformedKey candidate = transform_key_d2(key, transform);
            if (candidate.key < best.key) best = candidate;
        }
        return best;
    }();
    const std::string testPrefix = (std::filesystem::path(scratch) /
      ("joint-preserver-self-test-" + std::to_string(::getpid()))).string();
    const auto write_fixture = [&](const std::string& path,
                                   const std::vector<SidecarEntry>& entries) {
        Header fixture;
        initialize_header(fixture, SidecarMagic, SidecarKind);
        fixture.put64(40, entries.size());
        fixture.put64(48, entries.size());
        fixture.put64(216, 9);
        fixture.put_hash(SourceSha, bindings.sourceSha);
        fixture.put_hash(OriginalModelSha, bindings.originalModelSha);
        fixture.put_hash(CaptureModelSha, bindings.captureModelSha);
        fixture.put_hash(ObservationSha, bindings.observationSha);
        fixture.put_hash(LowerTableSha, bindings.lowerTableSha);
        fixture.put_hash(LowerOverlaySha, bindings.lowerOverlaySha);
        fixture.put_hash(LowerModelSha, bindings.lowerModelSha);
        fixture.put_hash(ProofLogSha, bindings.proofLogSha);
        for (const HashSlot slot : {DenseOverlaySha, NodesSha, ValuesSha,
                                   RanksSha, WitnessesSha, RawMetaSha})
            fixture.put_hash(slot, std::string(64, '9'));
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        write_all(output, fixture.bytes.data(), fixture.bytes.size());
        for (const SidecarEntry& entry : entries) {
            for (unsigned byte = 0; byte < 8; ++byte)
                output.put(static_cast<char>(entry.key >> (8 * byte)));
            output.put(static_cast<char>(entry.forces));
        }
        output.close();
        fixture.put_hash(PayloadSha, sha256_file(path, HeaderBytes));
        std::fstream rewrite(path, std::ios::binary | std::ios::in | std::ios::out);
        rewrite.write(reinterpret_cast<const char*>(fixture.bytes.data()),
                      fixture.bytes.size());
    };
    const std::uint8_t whiteForce = static_cast<std::uint8_t>(
      1u << representative.bitMap[0]);
    const std::string validPath = testPrefix + ".valid";
    write_fixture(validPath, {{representative.key, whiteForce}});
    {
        ArbitraryProbe probe(validPath, bindings, sha256_file(validPath));
        if (probe.forces(key, 0) != std::pair<bool, bool>{true, false})
            throw std::runtime_error("joint arbitrary probe round-trip residual");
    }
    bool badHashRejected = false;
    try { ArbitraryProbe probe(validPath, bindings, std::string(64, 'f')); }
    catch (const std::runtime_error&) { badHashRejected = true; }
    if (!badHashRejected)
        throw std::runtime_error("joint arbitrary probe accepted bad full SHA");
    const std::string duplicatePath = testPrefix + ".duplicate";
    write_fixture(duplicatePath, {{representative.key, whiteForce},
                                  {representative.key, whiteForce}});
    bool duplicateRejected = false;
    try { ArbitraryProbe probe(duplicatePath, bindings); }
    catch (const std::runtime_error&) { duplicateRejected = true; }
    if (!duplicateRejected)
        throw std::runtime_error("joint arbitrary probe accepted duplicate index");
    const std::string dualPath = testPrefix + ".dual";
    write_fixture(dualPath, {{representative.key, static_cast<std::uint8_t>(
      whiteForce | (whiteForce << 4))}});
    bool dualRejected = false;
    try { ArbitraryProbe probe(dualPath, bindings); }
    catch (const std::runtime_error&) { dualRejected = true; }
    if (!dualRejected)
        throw std::runtime_error("joint arbitrary probe accepted dual forces");
    std::filesystem::remove(validPath);
    std::filesystem::remove(duplicatePath);
    std::filesystem::remove(dualPath);
    std::cout << "joint_preserver_self_test d2_transforms 4"
                 " private_partition_residual 0 header_residual 0"
                 " arbitrary_roundtrip_residual 0 bad_hash_rejected 1"
                 " duplicate_index_rejected 1 dual_force_rejected 1\n";
}

int run(int argc, char** argv) {
    const Cli cli = parse_cli(argc, argv);
    if (cli.command == Cli::Command::SelfTest) {
        self_test(cli.inputs.scratch);
        return 0;
    }
    if (cli.command == Cli::Command::BindProof) {
        bind_proof_log(cli);
        return 0;
    }
    require_bindings(cli.inputs.bindings);
    if (cli.command == Cli::Command::Probe) {
        if (cli.outputSidecar.empty() || !valid_sha(cli.sidecarSha))
            throw std::runtime_error("strict joint probe needs sidecar path and SHA-256");
        ArbitraryProbe probe(cli.outputSidecar, cli.inputs.bindings,
                             cli.sidecarSha);
        const auto result = probe.forces(cli.probeKey, cli.probeBit);
        std::cout << "joint_arbitrary_probe white_forces " << result.first
                  << " black_forces " << result.second << '\n';
        return 0;
    }
    if (cli.rawPrefix.empty() && cli.command != Cli::Command::Measure)
        throw std::runtime_error("joint preservation command needs raw prefix");
    if (cli.inputs.input.empty() || cli.inputs.lowerTable.empty() ||
        cli.inputs.lowerOverlay.empty())
        throw std::runtime_error("joint preservation inputs are incomplete");
    if (cli.command == Cli::Command::Export || cli.command == Cli::Command::Verify) {
        if (cli.outputSidecar.empty())
            throw std::runtime_error("joint export/verify needs sidecar output");
        if (cli.command == Cli::Command::Verify && !valid_sha(cli.sidecarSha))
            throw std::runtime_error("strict joint verify needs sidecar SHA-256");
        const StrictCertificate certificate = export_and_strict_restore(
          cli.inputs, cli.rawPrefix, cli.outputSidecar,
          cli.command == Cli::Command::Verify ? cli.sidecarSha : std::string());
        std::cout << "joint_arbitrary_certificate nodes " << certificate.nodes
                  << " d2_entries " << certificate.entries
                  << " bellman_residual " << certificate.bellmanResidual
                  << " rank_residual " << certificate.rankResidual
                  << " symmetry_residual " << certificate.symmetryResidual
                  << " singleton_residual " << certificate.singletonResidual
                  << " sidecar_sha256 " << certificate.sidecarSha << '\n';
        return 0;
    }

    PackedWdl concrete = authenticated_concrete(cli.inputs);
    PackedWdl lowerConcrete;
    std::unique_ptr<LowerOracle> lower;
    std::unique_ptr<JointArena> arena = rebuild_arena(cli.inputs, lowerConcrete, lower);
    const ResourcePlan resources = measure_plan(*arena);
    std::cout << "joint_capture_measurement nodes " << resources.nodes
              << " white_variables " << resources.variables[0]
              << " black_variables " << resources.variables[1]
              << " white_tokens " << resources.tokens[0]
              << " black_tokens " << resources.tokens[1]
              << " scratch_bytes " << resources.scratchBytes
              << " resident_bytes " << resources.residentBytes
              << " raw_bytes " << resources.rawBytes
              << " sidecar_upper_bytes " << resources.sidecarBytes
              << " solve_launched " << (cli.command == Cli::Command::Capture)
              << '\n' << std::flush;
    if (cli.command == Cli::Command::Measure) return 0;
    if (cli.outputOverlay.empty())
        throw std::runtime_error("joint capture needs dense overlay output");
    resource_gate(resources, cli.inputs.scratch,
                  cli.scratchLimit, cli.residentLimit);
    CaptureSolutions capture = solve_capture(*arena, cli.inputs.scratch);
    const Summary summary = write_dense_overlay(cli.outputOverlay,
      cli.inputs.bindings.sourceSha, cli.inputs.bindings.originalModelSha,
      concrete, *arena, capture);
    write_capture_files(cli.rawPrefix, cli.inputs.bindings, *arena,
                        capture, summary, cli.outputOverlay);
    std::cout << "joint_capture_complete nodes " << arena->nodes().size()
              << " white_variables " << capture.plan.variables[0]
              << " black_variables " << capture.plan.variables[1]
              << " white_activated " << capture.targets[0].fixedPoint.activated
              << " black_activated " << capture.targets[1].fixedPoint.activated
              << " bellman_residual 0 rank_residual 0 dual_residual 0"
                 " raw_prefix " << cli.rawPrefix << '\n';
    return 0;
}

} // namespace
} // namespace Stockfish::Ultimate::JointJesterPreserver

int main(int argc, char** argv) {
    try {
        return Stockfish::Ultimate::JointJesterPreserver::run(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "joint-Jester preserver error: " << error.what() << '\n';
        return 1;
    }
}
