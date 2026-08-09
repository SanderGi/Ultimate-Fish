/*
  Ultimate Fish - exact K+Ghost/Dragon public-information solver
  Copyright (C) 2026 Ultimate Fish contributors
  GPLv3 or later.

  This translation unit is deliberately isolated from the frozen d597 Bishop
  and a88 reciprocal-Bishop source domains. The audited point-piece kernel is
  included under a token-local Dragon specialization; no frozen source byte is
  modified and the Dragon model receives an independent catalog fingerprint.
*/

#include "ghost_dragon_information_solver.h"

#include "external_robdd.h"
#include "ghost_information_probe.h"
#include "information.h"
#include "position.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace Stockfish::Ultimate {

[[nodiscard]] bool dragon_kernel_lower_child(const Position& position) {
    unsigned live = 0;
    unsigned kings = 0;
    unsigned dragons = 0;
    for (int id = 0; id < position.piece_count(); ++id) {
        const PieceState& piece = position.piece(id);
        if (!piece.alive || !piece.onBoard)
            continue;
        ++live;
        kings += piece.type == PieceType::King;
        dragons += piece.type == PieceType::Dragon;
    }
    return live == 3 && kings == 2 && dragons == 1;
}

// The audited Bishop compiler's only material-specific escape assumes that a
// lone public extra is an insufficient draw.  Keep its bytes frozen, but make
// that branch reachable for Dragon so the isolated post-compiler can replace
// the placeholder with an authenticated K+Dragon-v-K result.  No native move
// generation or terminal rule sees this wrapper: only the textual proof
// kernel's child classifier does.
class DragonKernelPosition : public Position {
  public:
    using Position::Position;
    DragonKernelPosition() = default;
    DragonKernelPosition(const Position& position) : Position(position) {}
    DragonKernelPosition(Position&& position) : Position(std::move(position)) {}
    [[nodiscard]] bool game_over() const {
        return dragon_kernel_lower_child(*this) || Position::game_over();
    }
    [[nodiscard]] std::optional<Color> winner() const {
        return dragon_kernel_lower_child(*this) ? std::nullopt
                                                : Position::winner();
    }
};

}  // namespace Stockfish::Ultimate

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define Bishop Dragon
#define Position DragonKernelPosition
#define private public
#include "ghost_public_extra_information_solver.cpp"
#undef private
#undef Position
#undef Bishop
#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace Stockfish::Ultimate::GhostDragonExact {
namespace {

constexpr std::uint32_t Squares = Position::BoardSquares;
constexpr std::uint32_t StateCount = GhostPublicExtra::StateCount;
constexpr std::uint32_t PlacementCount = GhostPublicExtra::PlacementCount;
constexpr std::uint32_t Endian = 0x01020304;
constexpr char SidecarSemantics[] =
  "fresh-maximal-public-view-v2:dragon-ghost-generic";

#pragma pack(push, 1)
struct NodeDisk {
    std::uint8_t variable = Squares;
    std::uint32_t low = 0;
    std::uint32_t high = 0;
};

struct SidecarHeader {
    std::array<char, 8> magic{{'U','F','G','D','1','\0','\0','\0'}};
    std::uint32_t version = 1;
    std::uint32_t headerBytes = sizeof(SidecarHeader);
    std::uint32_t endian = Endian;
    std::uint32_t primary = static_cast<std::uint32_t>(PieceType::Ghost);
    std::uint32_t secondary = static_cast<std::uint32_t>(PieceType::Dragon);
    std::uint32_t ghostColor = static_cast<std::uint32_t>(Color::White);
    std::uint32_t orientation = 0;
    std::uint32_t squares = Squares;
    std::uint32_t stateCount = StateCount;
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
    std::array<char, 64> normalizedSourceSha{};
    std::array<char, 64> modelSha{};
    std::array<char, 64> observationSha{};
    std::array<char, 64> lowerGhostSha{};
    std::array<char, 64> lowerDragonFullSha{};
    std::array<char, 64> lowerDragonSourceSha{};
    std::array<char, 64> lowerDragonModelSha{};
    std::array<std::array<char, 64>, 6> transitionSha{};
    std::array<char, 64> transitionPayloadSha{};
    std::array<char, 64> payloadSha{};
    std::array<char, 64> semantics{};
};
#pragma pack(pop)

static_assert(sizeof(NodeDisk) == 9);
static_assert(sizeof(SidecarHeader) == 1248);

template<typename Value>
void write_value(std::ostream& output, const Value& value) {
    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

[[nodiscard]] bool valid_sha(const std::string& value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(),
      [](unsigned char character) { return std::isxdigit(character); });
}

void copy_hash(std::array<char, 64>& target, const std::string& source,
               const char* label) {
    if (!valid_sha(source))
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
            throw std::runtime_error("missing Dragon transition provenance");
        while (input) {
            input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
            if (input.gcount() > 0)
                hash.update(buffer.data(),
                            static_cast<std::size_t>(input.gcount()));
        }
    }
    return hex_digest(hash.finish());
}

void validate_unique_node_tuples(const NodeDisk* nodes,
                                 std::uint64_t count) {
    std::set<std::tuple<std::uint8_t, std::uint32_t, std::uint32_t>> seen;
    for (std::uint64_t id = 2; id < count; ++id)
        if (!seen.emplace(nodes[id].variable, nodes[id].low,
                          nodes[id].high).second)
            throw std::runtime_error(
              "Dragon sidecar contains a duplicate ROBDD tuple");
}

[[nodiscard]] GhostPublicExtra::MaterialSpec adapter_material(
  Orientation orientation) {
    return {PieceType::Dragon,
      orientation == Orientation::Same ? Color::White : Color::Black,
      Color::White, GhostPublicExtra::SourceOrder::GhostPrimary,
      GhostPublicExtra::HiddenAdjacentPolicy::ImpossibleWithoutForcedRelocation,
      orientation == Orientation::Same ? "kghostdragonk" : "kghostkdragon"};
}

[[nodiscard]] MaterialSpec normalized_material(Orientation orientation) {
    return {orientation == Orientation::Same ? Color::White : Color::Black};
}

[[nodiscard]] GhostPublicExtra::ConcreteState normalized_to_original(
  const FourState& state, Orientation orientation) {
    GhostPublicExtra::ConcreteState result;
    if (orientation == Orientation::Same) {
        result.side = state.side;
        result.whiteKing = state.whiteKing;
        result.blackKing = state.blackKing;
    }
    else {
        result.side = ~state.side;
        result.whiteKing = state.blackKing;
        result.blackKing = state.whiteKing;
    }
    result.first = state.ghost;
    result.second = state.bishop;
    result.ghostVisible = state.visible;
    return result;
}

[[nodiscard]] FourState original_to_normalized(
  const GhostPublicExtra::ConcreteState& state, Orientation orientation) {
    FourState result;
    if (orientation == Orientation::Same) {
        result.side = state.side;
        result.whiteKing = state.whiteKing;
        result.blackKing = state.blackKing;
    }
    else {
        result.side = ~state.side;
        result.whiteKing = state.blackKing;
        result.blackKing = state.whiteKing;
    }
    result.bishop = state.second;
    result.ghost = state.first;
    result.visible = state.ghostVisible;
    return result;
}

class OriginalTable {
  public:
    OriginalTable(const std::string& path, Orientation orientation) {
        std::ifstream input(path, std::ios::binary);
        if (!input)
            throw std::runtime_error("cannot open Dragon/Ghost source table");
        bytes_ = {std::istreambuf_iterator<char>(input),
                  std::istreambuf_iterator<char>()};
        const auto word = [&](std::size_t offset) {
            std::uint32_t value = 0;
            if (offset + 4 > bytes_.size())
                throw std::runtime_error("truncated Dragon/Ghost source header");
            std::memcpy(&value, bytes_.data() + offset, 4);
            return value;
        };
        if (bytes_.size() < 48 ||
            std::memcmp(bytes_.data(), "UFTB1\0\0\0", 8) ||
            word(8) < 5 || word(12) != static_cast<std::uint32_t>(PieceType::Ghost) ||
            word(16) != StateCount || word(24) != 2 ||
            word(28) != PlacementCount / 2 || word(32) != StateCount ||
            word(40) != static_cast<std::uint32_t>(PieceType::Dragon) ||
            word(44) != static_cast<std::uint32_t>(orientation ==
              Orientation::Opposing))
            throw std::runtime_error(
              "Dragon/Ghost source header does not match its orientation");
        wdlBytes_ = (std::uint64_t(StateCount) + 3) / 4;
        if (48 + wdlBytes_ > bytes_.size())
            throw std::runtime_error("truncated Dragon/Ghost WDL plane");
        Sha256 hash;
        hash.update(bytes_.data(), bytes_.size());
        sha_ = hex_digest(hash.finish());
    }

    [[nodiscard]] std::uint8_t result(std::uint32_t index) const {
        return (bytes_.at(48 + index / 4) >> (2 * (index % 4))) & 3;
    }
    [[nodiscard]] const std::string& sha() const { return sha_; }

  private:
    std::vector<std::uint8_t> bytes_;
    std::uint64_t wdlBytes_ = 0;
    std::string sha_;
};

struct NormalizedSource {
    std::string path;
    std::string sha;
    std::uint64_t remapResidual = 0;
};

enum class DragonWdl : std::uint8_t { Win = 1, Loss = 2, Draw = 3 };

class LowerDragonTable {
  public:
    explicit LowerDragonTable(const std::string& path) {
        std::ifstream input(path, std::ios::binary);
        bytes_ = {std::istreambuf_iterator<char>(input),
                  std::istreambuf_iterator<char>()};
        const auto word = [&](std::size_t offset) {
            std::uint32_t value = 0;
            if (offset + 4 > bytes_.size())
                throw std::runtime_error("truncated kdragonk header");
            std::memcpy(&value, bytes_.data() + offset, 4);
            return value;
        };
        if (bytes_.size() < 40 ||
            std::memcmp(bytes_.data(), "UFTB1\0\0\0", 8) ||
            word(8) != 4 ||
            word(12) != static_cast<std::uint32_t>(PieceType::Dragon) ||
            word(16) != 985'920 || word(24) != 1 ||
            word(28) != 246'480 || word(32) != 985'920 || word(36) != 0 ||
            bytes_.size() != 40 + 246'480 + 985'920)
            throw std::runtime_error("incompatible authenticated kdragonk");
    }

    [[nodiscard]] DragonWdl probe(const Position& position) const {
        int whiteKing = Position::NoSquare;
        int blackKing = Position::NoSquare;
        int dragon = Position::NoSquare;
        for (int id = 0; id < position.piece_count(); ++id) {
            const PieceState& piece = position.piece(id);
            if (!piece.alive || !piece.onBoard)
                continue;
            if (piece.type == PieceType::King && piece.color == Color::White)
                whiteKing = piece.square;
            else if (piece.type == PieceType::King &&
                     piece.color == Color::Black)
                blackKing = piece.square;
            else if (piece.type == PieceType::Dragon &&
                     piece.color == Color::White)
                dragon = piece.square;
            else
                throw std::runtime_error("kdragonk probe material residual");
        }
        if (whiteKing < 0 || blackKing < 0 || dragon < 0 ||
            whiteKing == blackKing || whiteKing == dragon ||
            blackKing == dragon)
            throw std::runtime_error("kdragonk probe placement residual");
        const std::uint32_t blackRank = blackKing -
          (blackKing > whiteKing ? 1u : 0u);
        const int low = std::min(whiteKing, blackKing);
        const int high = std::max(whiteKing, blackKing);
        const std::uint32_t dragonRank = dragon - (dragon > low ? 1u : 0u) -
          (dragon > high ? 1u : 0u);
        const std::uint32_t index =
          ((static_cast<std::uint32_t>(position.side_to_move()) * Squares +
             whiteKing) * (Squares - 1) + blackRank) * (Squares - 2) +
          dragonRank;
        const std::uint8_t value =
          (bytes_[40 + index / 4] >> (2 * (index % 4))) & 3;
        if (value < 1 || value > 3)
            throw std::runtime_error("kdragonk WDL value is invalid");
        return static_cast<DragonWdl>(value);
    }

  private:
    std::vector<std::uint8_t> bytes_;
};

[[nodiscard]] std::uint8_t lower_dragon_force_flags(
  const Position& child, const MaterialSpec& material,
  const LowerDragonTable& lower) {
    if (!dragon_kernel_lower_child(child))
        throw std::runtime_error("lower-Dragon force requested for wrong class");
    std::optional<Color> winner;
    if (child.game_over())
        winner = child.winner();
    else {
        const DragonWdl result = lower.probe(child);
        if (result == DragonWdl::Win)
            winner = child.side_to_move();
        else if (result == DragonWdl::Loss)
            winner = ~child.side_to_move();
    }
    return static_cast<std::uint8_t>(
      winner && *winner == material.ghostColor ? 1 :
      winner && *winner == material.observer() ? 2 : 0);
}

struct DragonPatchCertificate {
    std::uint64_t lowerEdges = 0;
    std::uint64_t ownerForces = 0;
    std::uint64_t observerForces = 0;
    std::uint64_t draws = 0;
};

void lower_dragon_probe_self_test(const MaterialSpec& material,
                                  const LowerDragonTable& lower) {
    bool found = false;
    std::string witness;
    for (const Color side : {Color::White, Color::Black}) {
        for (std::uint8_t whiteKing = 0; whiteKing < Squares && !found;
             ++whiteKing)
            for (std::uint8_t blackKing = 0; blackKing < Squares && !found;
                 ++blackKing)
                for (std::uint8_t dragon = 0; dragon < Squares && !found;
                     ++dragon) {
                    if (whiteKing == blackKing || whiteKing == dragon ||
                        blackKing == dragon)
                        continue;
                    const int fileDistance = std::abs(
                      int(whiteKing % Position::BoardFiles) -
                      int(blackKing % Position::BoardFiles));
                    const int rankDistance = std::abs(
                      int(whiteKing / Position::BoardFiles) -
                      int(blackKing / Position::BoardFiles));
                    if (fileDistance <= 1 && rankDistance <= 1)
                        continue;
                    Position position;
                    position.clear();
                    const int wk = position.add_piece(
                      PieceType::King, Color::White, whiteKing);
                    const int bk = position.add_piece(
                      PieceType::King, Color::Black, blackKing);
                    const int item = position.add_piece(
                      PieceType::Dragon, Color::White, dragon);
                    position.piece(wk).moved = position.piece(bk).moved =
                      position.piece(item).moved = true;
                    position.set_side_to_move(side);
                    if (position.game_over())
                        continue;
                    const DragonWdl result = lower.probe(position);
                    const bool whiteWins =
                      (result == DragonWdl::Win &&
                       side == Color::White) ||
                      (result == DragonWdl::Loss &&
                       side == Color::Black);
                    if (!whiteWins)
                        continue;
                    const std::uint8_t expected =
                      material.ghostColor == Color::White ? 1 : 2;
                    if (lower_dragon_force_flags(position, material, lower) !=
                        expected)
                        throw std::runtime_error(
                          "K+Dragon force-role normalization residual");
                    witness = position.upn();
                    found = true;
                }
        if (found) break;
    }
    if (!found)
        throw std::runtime_error("no non-draw K+Dragon witness was found");
    std::cout << "ghost_dragon_lower_witness orientation "
              << (material.ghostColor == Color::White ? "same" : "opposing")
              << " force "
              << (material.ghostColor == Color::White ? "owner" : "observer")
              << " upn " << witness << '\n';
}

DragonPatchCertificate rewrite_lower_dragon_edges(
  const std::string& prefix, const MaterialSpec& material,
  const LowerDragonTable& lower, bool placeholders,
  bool acceptPlaceholders) {
    std::ifstream headerFile(prefix + ".header", std::ios::binary);
    ExternalTransitionHeader header;
    headerFile.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!headerFile || header.material !=
          static_cast<std::uint32_t>(material.ghostColor))
        throw std::runtime_error("Dragon transition patch header mismatch");
    const auto indices = read_external_vector<std::uint64_t>(
      prefix + ".index", std::uint64_t(header.geometryCount) + 1);
    std::fstream blocks(prefix + ".blocks",
      std::ios::binary | std::ios::in | std::ios::out);
    if (!blocks)
        throw std::runtime_error("cannot patch Dragon transition blocks");
    const ExtraGeometryDomain domain;
    DragonPatchCertificate certificate;
    for (std::uint32_t local = 0; local < header.geometryCount; ++local) {
        const std::uint32_t geometryId = header.reserved + local;
        const std::uint64_t bytes = indices[local + 1] - indices[local];
        if (bytes < sizeof(std::array<std::uint32_t, Squares + 1>) ||
            (bytes - sizeof(std::array<std::uint32_t, Squares + 1>)) %
              sizeof(ExternalCompiledEdge))
            throw std::runtime_error("malformed Dragon transition block");
        std::array<std::uint32_t, Squares + 1> offsets{};
        std::vector<ExternalCompiledEdge> edges(
          (bytes - sizeof(offsets)) / sizeof(ExternalCompiledEdge));
        blocks.seekg(static_cast<std::streamoff>(indices[local]));
        blocks.read(reinterpret_cast<char*>(offsets.data()), sizeof(offsets));
        blocks.read(reinterpret_cast<char*>(edges.data()),
                    static_cast<std::streamsize>(edges.size() *
                                                 sizeof(edges.front())));
        if (!blocks || offsets.front() || offsets.back() != edges.size())
            throw std::runtime_error("Dragon transition block read residual");
        const PublicExtraGeometry& geometry = domain[geometryId];
        for (std::uint8_t ghost = 0; ghost < Squares; ++ghost) {
            if (ghost == geometry.whiteKing || ghost == geometry.blackKing ||
                ghost == geometry.bishop)
                continue;
            Position position = make_geometry_position(geometry, ghost,
                                                        material);
            if (position.game_over())
                continue;
            const std::vector<Move> moves = position.legal_moves();
            if (moves.size() != offsets[ghost + 1] - offsets[ghost])
                throw std::runtime_error(
                  "Dragon transition patch move-count residual");
            for (std::size_t ordinal = 0; ordinal < moves.size(); ++ordinal) {
                Position child = position;
                Undo undo;
                if (!child.make_move(moves[ordinal], undo))
                    throw std::runtime_error("Dragon patch move failed");
                if (!dragon_kernel_lower_child(child))
                    continue;
                ExternalCompiledEdge& edge = edges[offsets[ghost] + ordinal];
                if (edge.domain != ExternalChildDomain::Exact ||
                    (!acceptPlaceholders && edge.exact !=
                       lower_dragon_force_flags(child, material, lower)) ||
                    (acceptPlaceholders && edge.exact != 0 &&
                     edge.exact != lower_dragon_force_flags(
                       child, material, lower)))
                    throw std::runtime_error(
                      "Dragon lower-table transition residual");
                const std::uint8_t expected = lower_dragon_force_flags(
                  child, material, lower);
                edge.exact = placeholders ? 0 : expected;
                ++certificate.lowerEdges;
                certificate.ownerForces += expected == 1;
                certificate.observerForces += expected == 2;
                certificate.draws += expected == 0;
            }
        }
        blocks.clear();
        blocks.seekp(static_cast<std::streamoff>(indices[local]));
        blocks.write(reinterpret_cast<const char*>(offsets.data()),
                     sizeof(offsets));
        blocks.write(reinterpret_cast<const char*>(edges.data()),
                     static_cast<std::streamsize>(edges.size() *
                                                  sizeof(edges.front())));
        if (!blocks)
            throw std::runtime_error("Dragon transition patch write residual");
    }
    blocks.flush();
    std::cout << "ghost_dragon_lower_probe_certificate edges "
              << certificate.lowerEdges << " owner_forces "
              << certificate.ownerForces << " observer_forces "
              << certificate.observerForces << " draws " << certificate.draws
              << " residual 0\n" << std::flush;
    return certificate;
}

NormalizedSource normalize_source(const OriginalTable& source,
                                  Orientation orientation,
                                  const std::string& path) {
    std::vector<std::uint8_t> wdl((std::uint64_t(StateCount) + 3) / 4, 0);
    std::array<std::uint64_t, 4> originalCounts{};
    std::array<std::uint64_t, 4> normalizedCounts{};
    std::vector<std::uint8_t> seen((StateCount + 7) / 8, 0);
    std::uint64_t residual = 0;
    const GhostPublicExtra::MaterialSpec adapter = adapter_material(orientation);
    for (std::uint32_t index = 0; index < StateCount; ++index) {
        const FourState normalized = decode_index(index);
        const GhostPublicExtra::ConcreteState original =
          normalized_to_original(normalized, orientation);
        const std::uint32_t originalIndex =
          GhostPublicExtra::encode_index(original, adapter);
        const std::uint8_t bit = static_cast<std::uint8_t>(
          1u << (originalIndex % 8));
        residual += (seen[originalIndex / 8] & bit) != 0;
        seen[originalIndex / 8] |= bit;
        const FourState restored = original_to_normalized(
          GhostPublicExtra::decode_index(originalIndex, adapter), orientation);
        residual += encode_index(restored) != index;
        const std::uint8_t value = source.result(originalIndex);
        if (value < 1 || value > 3)
            throw std::runtime_error("Dragon/Ghost source has invalid WDL");
        wdl[index / 4] |= static_cast<std::uint8_t>(
          value << (2 * (index % 4)));
        ++originalCounts[value];
        ++normalizedCounts[value];
    }
    residual += std::any_of(seen.begin(), seen.end(),
      [](std::uint8_t byte) { return byte != 0xff; });
    if (residual || originalCounts != normalizedCounts)
        throw std::runtime_error("Dragon/Ghost source normalization residual");
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write("UFTB1\0\0\0", 8);
    const auto write32 = [&](std::uint32_t value) {
        output.write(reinterpret_cast<const char*>(&value), 4);
    };
    write32(5);
    write32(static_cast<std::uint32_t>(PieceType::Dragon));
    write32(StateCount);
    write32(0);
    write32(2);
    write32(PlacementCount / 2);
    write32(StateCount);
    write32(0);
    write32(static_cast<std::uint32_t>(PieceType::Ghost));
    write32(static_cast<std::uint32_t>(normalized_material(orientation).ghostColor));
    output.write(reinterpret_cast<const char*>(wdl.data()),
                 static_cast<std::streamsize>(wdl.size()));
    output.close();
    if (!output)
        throw std::runtime_error("failed writing normalized Dragon/Ghost source");
    const std::string sha = GhostPublicExtraExact::sha256_file(path);
    std::cout << "ghost_dragon_source_normalization states " << StateCount
              << " remap_residual 0 count_residual 0 normalized_sha256 "
              << sha << '\n';
    return {path, sha, residual};
}

struct FreshSummary {
    std::uint64_t admissionResidual = 0;
    std::uint64_t groupingResidual = 0;
    std::uint64_t conservationResidual = 0;
};

FreshSummary report_fresh_roots(
  ExternalGhostExtraFixedPoint& solver, const OriginalTable& original,
  Orientation orientation, const SolveOptions& options) {
    const GhostPublicExtra::MaterialSpec material =
      adapter_material(orientation);
    std::vector<std::uint8_t> admitted((StateCount + 7) / 8, 0);
    std::vector<ExternalMask> freshMasks(solver.database_.stratum_count());
    std::array<std::array<std::uint64_t, 4>, 2> unreachable{};
    std::uint64_t admittedCount = 0;
    for (std::uint32_t index = 0; index < StateCount; ++index) {
        const GhostPublicExtra::ConcreteState state =
          GhostPublicExtra::decode_index(index, material);
        const Position position = GhostPublicExtra::make_position(state,
                                                                   material);
        const auto verdict = GhostPublicExtra::classify_fresh_root_admission(
          position, material);
        if (verdict ==
              GhostPublicExtra::FreshAdmissionVerdict::NeedsExactCausalAudit)
            throw std::runtime_error(
              "Dragon admission unexpectedly needs a causal audit");
        if (verdict != GhostPublicExtra::FreshAdmissionVerdict::Admit) {
            ++unreachable[static_cast<std::size_t>(state.side)]
                          [original.result(index)];
            continue;
        }
        admitted[index / 8] |= static_cast<std::uint8_t>(1u << (index % 8));
        ++admittedCount;
        if (!state.ghostVisible) {
            const FourState normalized = original_to_normalized(
              state, orientation);
            const PublicExtraGeometry raw{
              static_cast<std::uint8_t>(normalized.side),
              normalized.whiteKing, normalized.blackKing,
              normalized.bishop, 0};
            const auto [geometry, transform] = solver.domain_.locate(raw);
            const unsigned actual = rectangle_transform_square(
              normalized.ghost, transform);
            const std::uint32_t stratum =
              solver.database_.meta(geometry).actualStratum[actual];
            if (stratum != NoIndex)
                external_mask_set(freshMasks[stratum], actual);
        }
    }

    std::array<std::array<std::uint64_t, 4>, 2> totals{};
    std::vector<std::uint8_t> flags(StateCount, 0);
    std::array<std::uint64_t, 2> rootSets{};
    std::array<std::uint64_t, 2> independentSets{};
    std::array<std::uint64_t, 2> realizations{};
    std::vector<std::uint8_t> seenVisible(
      std::uint64_t(solver.database_.geometry_count()) * Squares, 0);
    std::vector<std::uint8_t> seenStratum(solver.database_.stratum_count(), 0);
    std::vector<std::uint8_t> seenTerminal(
      std::uint64_t(solver.database_.geometry_count()) * 3, 0);
    std::vector<std::uint8_t> independentVisible(seenVisible.size(), 0);
    std::vector<std::uint8_t> independentStratum(seenStratum.size(), 0);
    std::vector<std::uint8_t> independentTerminal(seenTerminal.size(), 0);
    for (std::uint32_t index = 0; index < StateCount; ++index) {
        if (!((admitted[index / 8] >> (index % 8)) & 1u))
            continue;
        const GhostPublicExtra::ConcreteState state =
          GhostPublicExtra::decode_index(index, material);
        const FourState normalized = original_to_normalized(state,
                                                             orientation);
        const std::size_t side = static_cast<std::size_t>(state.side);
        const PublicExtraGeometry raw{
          static_cast<std::uint8_t>(normalized.side), normalized.whiteKing,
          normalized.blackKing, normalized.bishop,
          static_cast<std::uint8_t>(normalized.visible)};
        const auto [geometry, transform] = solver.domain_.locate(raw);
        const unsigned actual = rectangle_transform_square(
          normalized.ghost, transform);
        const ExternalGeometryMeta& meta = solver.database_.meta(geometry);
        ++realizations[side];
        std::uint64_t publicKey = 0;
        enum class RootKind { Visible, Stratum, Terminal } kind;
        if (normalized.visible) {
            kind = RootKind::Visible;
            publicKey = std::uint64_t(geometry) * Squares + actual;
            if (!independentVisible[publicKey]) {
                independentVisible[publicKey] = 1;
                ++independentSets[side];
            }
        }
        else if (external_mask_test(meta.terminal, actual)) {
            kind = RootKind::Terminal;
            const std::size_t outcome =
              external_mask_test(meta.terminalOwner, actual) ? 0 :
              external_mask_test(meta.terminalObserver, actual) ? 1 : 2;
            publicKey = std::uint64_t(geometry) * 3 + outcome;
            if (!independentTerminal[publicKey]) {
                independentTerminal[publicKey] = 1;
                ++independentSets[side];
            }
        }
        else {
            kind = RootKind::Stratum;
            publicKey = meta.actualStratum[actual];
            if (publicKey == NoIndex)
                throw std::runtime_error("Dragon fresh root lacks stratum");
            if (!independentStratum[publicKey]) {
                independentStratum[publicKey] = 1;
                ++independentSets[side];
            }
        }

        bool owner = false;
        bool observer = false;
        if (external_mask_test(meta.terminal, actual)) {
            owner = external_mask_test(meta.terminalOwner, actual);
            observer = external_mask_test(meta.terminalObserver, actual);
        }
        else if (normalized.visible) {
            const std::uint64_t root =
              std::uint64_t(geometry) * Squares + actual;
            owner = solver.visibleOwnerCurrent_[root];
            observer = solver.visibleObserverCurrent_[root];
        }
        else {
            const std::uint32_t stratum = meta.actualStratum[actual];
            if (stratum == NoIndex ||
                !external_mask_test(freshMasks[stratum], actual))
                throw std::runtime_error(
                  "Dragon admitted root crosses decision stratum");
            owner = solver.bdd_->evaluate(
              solver.ownerCurrent_[std::uint64_t(geometry) * Squares + actual],
              freshMasks[stratum].low, freshMasks[stratum].high);
            observer = solver.bdd_->evaluate(solver.observerCurrent_[stratum],
              freshMasks[stratum].low, freshMasks[stratum].high);
        }
        if (owner && observer)
            throw std::runtime_error("Dragon fresh root has dual force");
        std::vector<std::uint8_t>* seen = kind == RootKind::Visible
          ? &seenVisible : kind == RootKind::Terminal
          ? &seenTerminal : &seenStratum;
        if (!(*seen)[publicKey]) {
            (*seen)[publicKey] = 1;
            ++rootSets[side];
        }
        flags[index] = static_cast<std::uint8_t>(
          4 | (owner ? 1 : 0) | (observer ? 2 : 0));
        const bool moverWins = state.side == Color::White ? owner : observer;
        const bool moverLoses = state.side == Color::White ? observer : owner;
        ++totals[side][moverWins ? 1 : moverLoses ? 2 : 3];
    }
    FreshSummary certificate;
    for (std::size_t side = 0; side < 2; ++side) {
        std::uint64_t conserved = 0;
        for (std::size_t result = 1; result < 4; ++result)
            conserved += totals[side][result] + unreachable[side][result];
        certificate.conservationResidual += conserved != StateCount / 2;
        certificate.groupingResidual += rootSets[side] != independentSets[side];
        certificate.groupingResidual +=
          totals[side][1] + totals[side][2] + totals[side][3] !=
          realizations[side];
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
    if (certificate.groupingResidual || certificate.conservationResidual)
        throw std::runtime_error("Dragon fresh-root certificate residual");

    std::ofstream output(options.outputOverlay,
      std::ios::binary | std::ios::trunc);
    output.write("UFIW2\0\0\0", 8);
    const auto write32 = [&](std::uint32_t value) {
        output.write(reinterpret_cast<const char*>(&value), 4);
    };
    write32(2);
    write32(static_cast<std::uint32_t>(PieceType::Ghost));
    write32(static_cast<std::uint32_t>(PieceType::Dragon));
    write32(static_cast<std::uint32_t>(Color::White));
    write32(StateCount);
    write32(2);
    output.write(options.sourceSha256.data(), 64);
    output.write(options.modelSha256.data(), 64);
    output.write(reinterpret_cast<const char*>(flags.data()),
                 static_cast<std::streamsize>(flags.size()));
    output.close();
    if (!output)
        throw std::runtime_error("failed writing Dragon information overlay");
    std::cout << "ghost_dragon_root_conservation admitted " << admittedCount
              << " total " << StateCount
              << " grouping_residual 0 conservation_residual 0\n";
    return certificate;
}

SolveCertificate write_sidecar(const SolveOptions& options,
                               ExternalGhostExtraFixedPoint& solver,
                               const std::string& normalizedSha) {
    if (options.outputArbitrary.empty())
        throw std::invalid_argument("Dragon solve requires UFGD1 output");
    GhostPublicExtraExact::SolveCertificate inherited;
    GhostPublicExtraExact::prove_arbitrary_disjoint(solver, inherited);
    GhostPublicExtraExact::compact_for_sidecar(solver, inherited);

    SidecarHeader header;
    header.orientation = static_cast<std::uint32_t>(options.orientation);
    header.nodes = solver.bdd_->node_count();
    header.geometries = solver.database_.geometry_count();
    header.strata = solver.database_.stratum_count();
    header.ownerRoots = solver.ownerCurrent_.size();
    header.nodeOffset = sizeof(header);
    header.geometryOffset = header.nodeOffset +
                            header.nodes * sizeof(NodeDisk);
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
    copy_hash(header.normalizedSourceSha, normalizedSha,
              "normalized source SHA");
    copy_hash(header.modelSha, options.modelSha256, "model SHA");
    copy_hash(header.observationSha, options.observationSha256,
              "observation SHA");
    copy_hash(header.lowerGhostSha, options.lowerGhostSidecarSha256,
              "lower Ghost SHA");
    copy_hash(header.lowerDragonFullSha, options.lowerDragonFullSha256,
              "lower Dragon full SHA");
    copy_hash(header.lowerDragonSourceSha, options.lowerDragonSourceSha256,
              "lower Dragon source SHA");
    copy_hash(header.lowerDragonModelSha, options.lowerDragonModelSha256,
              "lower Dragon model SHA");
    std::size_t component = 0;
    for (const char* suffix : {".header", ".meta", ".strata", ".index",
                               ".blocks", ".verified"})
        copy_hash(header.transitionSha[component++],
          GhostPublicExtraExact::sha256_file(options.transitionPrefix + suffix),
          "transition component SHA");
    const std::string transitionSha = transition_payload_sha(
      options.transitionPrefix);
    copy_hash(header.transitionPayloadSha, transitionSha,
              "transition payload SHA");
    std::copy(std::begin(SidecarSemantics), std::end(SidecarSemantics),
              header.semantics.begin());

    std::ofstream output(options.outputArbitrary,
      std::ios::binary | std::ios::trunc);
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
    if (!output || GhostPublicExtraExact::sha256_file(
          options.outputArbitrary).empty())
        throw std::runtime_error("failed writing Dragon arbitrary sidecar");
    struct stat status{};
    if (::stat(options.outputArbitrary.c_str(), &status) ||
        static_cast<std::uint64_t>(status.st_size) != extent)
        throw std::runtime_error("Dragon arbitrary sidecar extent residual");
    copy_hash(header.payloadSha, GhostPublicExtraExact::sha256_file(
      options.outputArbitrary, sizeof(header)), "payload SHA");
    std::fstream rewrite(options.outputArbitrary,
      std::ios::binary | std::ios::in | std::ios::out);
    write_value(rewrite, header);
    rewrite.close();
    if (!rewrite)
        throw std::runtime_error("cannot finalize Dragon sidecar");

    SolveCertificate certificate;
    certificate.dualForceResidual = inherited.arbitraryDualForceResidual;
    certificate.structuralResidual = inherited.arbitraryStructuralResidual;
    certificate.normalizedSourceSha256 = normalizedSha;
    certificate.transitionPayloadSha256 = transitionSha;
    certificate.arbitrarySha256 = GhostPublicExtraExact::sha256_file(
      options.outputArbitrary);
    return certificate;
}

void prove_sidecar_singletons(const ArbitrarySidecarProbe& probe,
                              ExternalGhostExtraFixedPoint& solver,
                              Orientation orientation,
                              SolveCertificate& certificate) {
    for (std::uint32_t geometry = 0;
         geometry < solver.database_.geometry_count(); ++geometry) {
        const PublicExtraGeometry& physical = solver.domain_[geometry];
        const ExternalGeometryMeta& meta = solver.database_.meta(geometry);
        for (unsigned actual = 0; actual < Squares; ++actual) {
            if (!external_mask_test(meta.live, actual) &&
                !external_mask_test(meta.terminal, actual))
                continue;
            FourState normalized;
            normalized.side = static_cast<Color>(physical.side);
            normalized.whiteKing = physical.whiteKing;
            normalized.blackKing = physical.blackKing;
            normalized.bishop = physical.bishop;
            normalized.ghost = static_cast<std::uint8_t>(actual);
            normalized.visible = physical.visible != 0;
            const auto original = normalized_to_original(normalized,
                                                          orientation);
            GhostPublicExtra::GhostMask singleton;
            singleton.set(actual);
            const std::uint64_t ownerIndex =
              std::uint64_t(geometry) * Squares + actual;
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
                if (stratum == NoIndex)
                    throw std::runtime_error(
                      "Dragon singleton lacks decision stratum");
                expectedOwner = solver.bdd_->evaluate(
                  solver.ownerCurrent_[ownerIndex], singleton.low,
                  singleton.high);
                expectedObserver = solver.bdd_->evaluate(
                  solver.observerCurrent_[stratum], singleton.low,
                  singleton.high);
            }
            certificate.singletonResidual +=
              probe.owner_forces(original, singleton) != expectedOwner;
            certificate.singletonResidual +=
              probe.observer_forces(original, singleton) != expectedObserver;
        }
    }
    if (certificate.singletonResidual)
        throw std::runtime_error(
          "Dragon arbitrary singleton reproduction residual");
}

}  // namespace

namespace {

[[nodiscard]] LowerDragonTable authenticate_lower_dragon(
  const TransitionOptions& options) {
    if (!valid_sha(options.lowerDragonSha256) ||
        !valid_sha(options.lowerDragonSourceSha256) ||
        !valid_sha(options.lowerDragonModelSha256) ||
        options.lowerDragonSourceSha256 != options.lowerDragonSha256 ||
        GhostPublicExtraExact::sha256_file(options.lowerDragonTable) !=
          options.lowerDragonSha256)
        throw std::runtime_error(
          "Dragon transitions require authenticated compatible kdragonk");
    LowerDragonTable lower(options.lowerDragonTable);
    lower_dragon_probe_self_test(normalized_material(options.orientation),
                                 lower);
    return lower;
}

void write_dragon_marker(const TransitionOptions& options) {
    std::fstream marker(options.prefix + ".verified",
      std::ios::binary | std::ios::in | std::ios::out);
    ExternalTransitionHeader header;
    marker.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!marker)
        throw std::runtime_error("missing frozen transition marker");
    marker.seekp(sizeof(header));
    for (const std::string* hash : {&options.lowerDragonSha256,
                                    &options.lowerDragonSourceSha256,
                                    &options.lowerDragonModelSha256})
        marker.write(hash->data(), 64);
    marker.close();
    struct stat status{};
    if (::stat((options.prefix + ".verified").c_str(), &status) ||
        status.st_size != static_cast<off_t>(sizeof(header) + 192))
        throw std::runtime_error("Dragon transition marker extent residual");
}

void authenticate_dragon_marker(const TransitionOptions& options) {
    std::ifstream marker(options.prefix + ".verified", std::ios::binary);
    ExternalTransitionHeader header;
    std::array<char, 192> hashes{};
    marker.read(reinterpret_cast<char*>(&header), sizeof(header));
    marker.read(hashes.data(), hashes.size());
    if (!marker || marker.peek() != std::char_traits<char>::eof() ||
        std::string(hashes.data(), 64) != options.lowerDragonSha256 ||
        std::string(hashes.data() + 64, 64) !=
          options.lowerDragonSourceSha256 ||
        std::string(hashes.data() + 128, 64) !=
          options.lowerDragonModelSha256)
        throw std::runtime_error(
          "Dragon transition marker dependency residual");
}

void certify_dragon_transitions(const TransitionOptions& options,
                                const LowerDragonTable& lower) {
    const MaterialSpec material = normalized_material(options.orientation);
    rewrite_lower_dragon_edges(options.prefix, material, lower, true, false);
    try {
        verify_external_transition_certificate(options.prefix, material);
    }
    catch (...) {
        rewrite_lower_dragon_edges(options.prefix, material, lower,
                                   false, true);
        throw;
    }
    rewrite_lower_dragon_edges(options.prefix, material, lower, false, true);
    write_dragon_marker(options);
    authenticate_dragon_marker(options);
}

}  // namespace

void compile_transitions(const TransitionOptions& options) {
    if (options.prefix.empty() || !options.geometryCount)
        throw std::invalid_argument("Dragon/Ghost transition range is empty");
    const LowerDragonTable lower = authenticate_lower_dragon(options);
    if (options.orientation == Orientation::Same)
        compile_external_transitions(options.prefix,
          normalized_material(options.orientation), options.geometryBegin,
          options.geometryCount);
    else
        GhostPublicExtraExact::compile_reciprocal_external_transitions(
          options.prefix, options.geometryBegin, options.geometryCount);
    rewrite_lower_dragon_edges(options.prefix,
      normalized_material(options.orientation), lower, false, true);
    certify_dragon_transitions(options, lower);
}

void merge_transitions(const TransitionOptions& output,
                       const std::vector<std::string>& shards,
                       std::uint32_t expectedGeometries) {
    const LowerDragonTable lower = authenticate_lower_dragon(output);
    for (const std::string& shard : shards) {
        TransitionOptions input = output;
        input.prefix = shard;
        authenticate_dragon_marker(input);
    }
    merge_external_transition_shards(output.prefix, shards,
      normalized_material(output.orientation), expectedGeometries);
    certify_dragon_transitions(output, lower);
}

void verify_transitions(const TransitionOptions& options) {
    const LowerDragonTable lower = authenticate_lower_dragon(options);
    authenticate_dragon_marker(options);
    certify_dragon_transitions(options, lower);
}

ResourceEstimate resource_estimate() { return {}; }

std::uint32_t lower_capture_witness_geometry(Orientation orientation) {
    const MaterialSpec material = normalized_material(orientation);
    const ExtraGeometryDomain domain;
    const Color mover = material.observer();
    const std::uint8_t whiteKing = 0;
    const std::uint8_t blackKing = 79;
    const std::uint8_t ghost = orientation == Orientation::Same ? 70 : 9;
    for (std::uint8_t dragon = 0; dragon < Squares; ++dragon) {
        if (dragon == whiteKing || dragon == blackKing || dragon == ghost)
            continue;
        PublicExtraGeometry raw{static_cast<std::uint8_t>(mover), whiteKing,
          blackKing, dragon, 1};
        Position position = make_geometry_position(raw, ghost, material);
        if (position.game_over())
            continue;
        for (const Move& move : position.legal_moves()) {
            Position child = position;
            Undo undo;
            if (child.make_move(move, undo) &&
                dragon_kernel_lower_child(child))
                return domain.locate(raw).first;
        }
    }
    throw std::runtime_error(
      "cannot construct a legal Dragon/Ghost capture witness");
}

SolveCertificate solve_exact(const SolveOptions& options) {
    if (options.measureIterations == 0 &&
        (options.outputOverlay.empty() || options.outputArbitrary.empty()))
        throw std::invalid_argument(
          "Dragon exact solve requires UFIW2 and UFGD1 outputs");
    for (const auto& [value, label] : {
           std::pair<const std::string*, const char*>{&options.sourceSha256,
                                                      "source SHA"},
           {&options.modelSha256, "model SHA"},
           {&options.observationSha256, "observation SHA"},
           {&options.lowerGhostSourceSha256, "lower source SHA"},
           {&options.lowerGhostModelSha256, "lower model SHA"},
           {&options.lowerGhostObservationSha256,
            "lower observation SHA"},
           {&options.lowerGhostSidecarSha256, "lower sidecar SHA"}})
        if (!valid_sha(*value))
            throw std::invalid_argument(std::string(label) + " is invalid");
    if (!valid_sha(options.lowerDragonFullSha256) ||
        !valid_sha(options.lowerDragonSourceSha256) ||
        !valid_sha(options.lowerDragonModelSha256))
        throw std::invalid_argument("lower Dragon binding is invalid");
    if (GhostPublicExtraExact::sha256_file(options.lowerGhostSidecar) !=
        options.lowerGhostSidecarSha256)
        throw std::runtime_error("lower Ghost UFGM full SHA mismatch");
    const OriginalTable original(options.sourceTable, options.orientation);
    if (original.sha() != options.sourceSha256)
        throw std::runtime_error("Dragon/Ghost concrete source SHA mismatch");
    const NormalizedSource normalized = normalize_source(original,
      options.orientation, options.scratchPrefix + ".normalized.uftb");
    TransitionOptions transitions;
    transitions.orientation = options.orientation;
    transitions.prefix = options.transitionPrefix;
    transitions.lowerDragonTable = options.lowerDragonTable;
    transitions.lowerDragonSha256 = options.lowerDragonFullSha256;
    transitions.lowerDragonSourceSha256 = options.lowerDragonSourceSha256;
    transitions.lowerDragonModelSha256 = options.lowerDragonModelSha256;
    verify_transitions(transitions);

    const MaterialSpec material = normalized_material(options.orientation);
    PackedFourTable concrete(normalized.path, material);
    if (hex_digest(concrete.sha()) != normalized.sha)
        throw std::runtime_error("normalized Dragon source SHA mismatch");
    ExternalTransitionDatabase database(options.transitionPrefix, material);
    ExternalGhostExtraSolveOptions legacy;
    legacy.scratch = options.scratchPrefix;
    legacy.output = options.outputOverlay;
    legacy.sourceSha256 = normalized.sha;
    legacy.modelSha256 = options.modelSha256;
    legacy.observationSha256 = options.observationSha256;
    legacy.bddLimits.maxNodes = options.maxNodes;
    legacy.bddLimits.uniqueSlots = options.uniqueSlots;
    legacy.compactEvery = options.compactEvery;
    legacy.measureIterations = options.measureIterations;
    gate_external_ghost_extra_solve(options.transitionPrefix, database,
                                    legacy);
    LowerGhostSymbolicSidecar lower(options.lowerGhostSidecar,
      options.lowerGhostSourceSha256, options.lowerGhostModelSha256,
      options.lowerGhostObservationSha256);
    ExtraGeometryDomain domain;
    MaterialSpec constructor;
    constructor.ghostColor = Color::White;
    ExternalGhostExtraFixedPoint solver(database, lower, concrete, domain,
      constructor, legacy);
    solver.material_ = material;
    solver.inherited_lower_mask_self_test();
    const bool proofComplete =
      GhostPublicExtraExact::run_reciprocal_fixed_point(solver, domain);
    SolveCertificate certificate;
    certificate.sourceRemapResidual = normalized.remapResidual;
    certificate.normalizedSourceSha256 = normalized.sha;
    if (!proofComplete)
        return certificate;
    const FreshSummary fresh = report_fresh_roots(solver, original,
      options.orientation, options);
    certificate.structuralResidual += fresh.admissionResidual +
      fresh.groupingResidual + fresh.conservationResidual;
    SolveCertificate sidecar = write_sidecar(options, solver, normalized.sha);
    sidecar.sourceRemapResidual = normalized.remapResidual;
    sidecar.structuralResidual += certificate.structuralResidual;
    ArbitrarySidecarProbe probe(options.outputArbitrary, options);
    prove_sidecar_singletons(probe, solver, options.orientation, sidecar);
    return sidecar;
}

void exact_self_test(const std::string& scratchPrefix) {
    (void)scratchPrefix;
    for (const Orientation orientation : {Orientation::Same,
                                           Orientation::Opposing}) {
        const GhostPublicExtra::MaterialSpec material =
          adapter_material(orientation);
        for (std::uint32_t index = 0; index < StateCount; ++index) {
            const auto original = GhostPublicExtra::decode_index(index,
                                                                  material);
            if (GhostPublicExtra::encode_index(original, material) != index)
                throw std::runtime_error("Dragon/Ghost source codec residual");
            const FourState normalized = original_to_normalized(
              original, orientation);
            if (GhostPublicExtra::encode_index(
                  normalized_to_original(normalized, orientation), material) !=
                index)
                throw std::runtime_error("Dragon/Ghost role remap residual");
        }
    }
    std::cout << "ghost_dragon_exact_self_test codec_states "
              << std::uint64_t(StateCount) * 2
              << " remap_residual 0 belief_cap none\n";
}

std::string verify_source_normalization(
  const std::string& sourceTable, Orientation orientation,
  const std::string& expectedSourceSha256, const std::string& scratchPrefix) {
    const OriginalTable original(sourceTable, orientation);
    if (!expectedSourceSha256.empty() &&
        original.sha() != expectedSourceSha256)
        throw std::runtime_error(
          "Dragon source-normalization input SHA mismatch");
    const NormalizedSource normalized = normalize_source(original,
      orientation, scratchPrefix + ".normalized.uftb");
    if (normalized.remapResidual)
        throw std::runtime_error("Dragon source normalization residual");
    return normalized.sha;
}

class ArbitrarySidecarProbe::Impl {
  public:
    Impl(const std::string& path, const ProbeBindings& bindings)
      : path_(path), orientation_(bindings.orientation) {
        open_and_validate(bindings, nullptr);
    }

    Impl(const std::string& path, const SolveOptions& bindings)
      : path_(path), orientation_(bindings.orientation) {
        ProbeBindings probe;
        probe.orientation = bindings.orientation;
        probe.sourceSha256 = bindings.sourceSha256;
        probe.modelSha256 = bindings.modelSha256;
        probe.observationSha256 = bindings.observationSha256;
        probe.lowerGhostSidecarSha256 = bindings.lowerGhostSidecarSha256;
        probe.lowerDragonFullSha256 = bindings.lowerDragonFullSha256;
        probe.lowerDragonSourceSha256 = bindings.lowerDragonSourceSha256;
        probe.lowerDragonModelSha256 = bindings.lowerDragonModelSha256;
        open_and_validate(probe, &bindings);
    }

    ~Impl() {
        if (data_)
            ::munmap(const_cast<std::uint8_t*>(data_),
                     static_cast<std::size_t>(bytes_));
        if (descriptor_ >= 0) ::close(descriptor_);
    }

    [[nodiscard]] bool query(
      const GhostPublicExtra::ConcreteState& original,
      GhostPublicExtra::GhostMask belief, bool owner) const {
        const FourState normalized = original_to_normalized(original,
                                                             orientation_);
        PublicExtraGeometry raw{
          static_cast<std::uint8_t>(normalized.side), normalized.whiteKing,
          normalized.blackKing, normalized.bishop,
          static_cast<std::uint8_t>(normalized.visible)};
        const CanonicalExtraGeometry canonical = canonical_geometry(raw);
        GhostPublicExtra::GhostMask mapped;
        for (unsigned square = 0; square < Squares; ++square)
            if (belief.test(square))
                mapped.set(rectangle_transform_square(
                  static_cast<std::uint8_t>(square), canonical.transform));
        const unsigned actual = rectangle_transform_square(
          normalized.ghost, canonical.transform);
        const auto [geometry, residual] = domain_.locate(canonical.geometry);
        if (residual || geometry >= header_.geometries)
            throw std::runtime_error("Dragon query geometry residual");
        if (!mapped.count() || !mapped.test(actual))
            throw std::invalid_argument(
              "Dragon belief must be nonempty and contain actual");
        if (canonical.geometry.visible && mapped.count() != 1)
            throw std::invalid_argument(
              "visible Dragon/Ghost query must be singleton");
        const ExternalGeometryMeta& meta = geometries()[geometry];
        const std::uint64_t ownerIndex =
          std::uint64_t(geometry) * Squares + actual;
        if (external_mask_test(meta.terminal, actual)) {
            const bool ownerResult = external_mask_test(meta.terminalOwner,
                                                        actual);
            const bool observerResult = external_mask_test(
              meta.terminalObserver, actual);
            for (unsigned candidate = 0; candidate < Squares; ++candidate)
                if (mapped.test(candidate) &&
                    (!external_mask_test(meta.terminal, candidate) ||
                     external_mask_test(meta.terminalOwner, candidate) !=
                       ownerResult ||
                     external_mask_test(meta.terminalObserver, candidate) !=
                       observerResult))
                    throw std::invalid_argument(
                      "Dragon terminal belief spans observations");
            return owner ? ownerResult : observerResult;
        }
        if (!external_mask_test(meta.live, actual))
            throw std::invalid_argument("Dragon query actual is not live");
        if (canonical.geometry.visible)
            return (owner ? visible_owner() :
                            visible_observer())[ownerIndex] != 0;
        const std::uint32_t stratum = meta.actualStratum[actual];
        if (stratum == NoIndex || stratum >= header_.strata)
            throw std::runtime_error("Dragon query has no decision cell");
        const ExternalMask& allowed = strata()[stratum];
        if ((mapped.low & ~allowed.low) ||
            (mapped.high & ~allowed.high))
            throw std::invalid_argument(
              "Dragon belief spans legal-dot decision cells");
        return evaluate(owner ? owner_roots()[ownerIndex] :
                                observer_roots()[stratum], mapped);
    }

  private:
    template<typename Value>
    [[nodiscard]] const Value* at(std::uint64_t offset,
                                  std::uint64_t count) const {
        if (offset > bytes_ || count > (bytes_ - offset) / sizeof(Value))
            throw std::runtime_error("Dragon sidecar section overflow");
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
        return at<ExternalRobdd::Id>(header_.ownerOffset,
                                     header_.ownerRoots);
    }
    [[nodiscard]] const ExternalRobdd::Id* observer_roots() const {
        return at<ExternalRobdd::Id>(header_.observerOffset,
                                     header_.strata);
    }
    [[nodiscard]] const std::uint8_t* visible_owner() const {
        return at<std::uint8_t>(header_.visibleOwnerOffset,
                                header_.ownerRoots);
    }
    [[nodiscard]] const std::uint8_t* visible_observer() const {
        return at<std::uint8_t>(header_.visibleObserverOffset,
                                header_.ownerRoots);
    }

    [[nodiscard]] bool evaluate(
      ExternalRobdd::Id root,
      const GhostPublicExtra::GhostMask& belief) const {
        while (root > ExternalRobdd::True) {
            if (root >= header_.nodes)
                throw std::runtime_error("Dragon sidecar root out of range");
            const NodeDisk& node = nodes()[root];
            root = belief.test(node.variable) ? node.high : node.low;
        }
        return root == ExternalRobdd::True;
    }

    void open_and_validate(const ProbeBindings& bindings,
                           const SolveOptions* restore) {
        descriptor_ = ::open(path_.c_str(), O_RDONLY);
        if (descriptor_ < 0)
            throw std::runtime_error("cannot open Dragon sidecar");
        struct stat status{};
        if (::fstat(descriptor_, &status) || status.st_size <= 0)
            throw std::runtime_error("cannot stat Dragon sidecar");
        bytes_ = static_cast<std::uint64_t>(status.st_size);
        data_ = static_cast<const std::uint8_t*>(::mmap(nullptr,
          static_cast<std::size_t>(bytes_), PROT_READ, MAP_PRIVATE,
          descriptor_, 0));
        if (data_ == MAP_FAILED) {
            data_ = nullptr;
            throw std::runtime_error("cannot mmap Dragon sidecar");
        }
        if (bytes_ < sizeof(header_))
            throw std::runtime_error("truncated Dragon sidecar");
        std::memcpy(&header_, data_, sizeof(header_));
        validate(bindings, restore);
    }

    void validate(const ProbeBindings& bindings,
                  const SolveOptions* restore) {
        const auto header_hash = [](const std::array<char, 64>& value) {
            return std::string(value.data(), value.size());
        };
        const std::uint64_t nodeOffset = sizeof(header_);
        const std::uint64_t geometryOffset = nodeOffset +
          header_.nodes * sizeof(NodeDisk);
        const std::uint64_t stratumOffset = geometryOffset +
          header_.geometries * sizeof(ExternalGeometryMeta);
        const std::uint64_t ownerOffset = stratumOffset +
          header_.strata * sizeof(ExternalMask);
        const std::uint64_t observerOffset = ownerOffset +
          header_.ownerRoots * sizeof(ExternalRobdd::Id);
        const std::uint64_t visibleOwnerOffset = observerOffset +
          header_.strata * sizeof(ExternalRobdd::Id);
        const std::uint64_t visibleObserverOffset = visibleOwnerOffset +
          header_.ownerRoots;
        const std::uint64_t extent = visibleObserverOffset +
                                     header_.ownerRoots;
        if (header_.magic !=
              std::array<char, 8>{{'U','F','G','D','1','\0','\0','\0'}} ||
            header_.version != 1 || header_.headerBytes != sizeof(header_) ||
            header_.endian != Endian ||
            header_.primary != static_cast<std::uint32_t>(PieceType::Ghost) ||
            header_.secondary !=
              static_cast<std::uint32_t>(PieceType::Dragon) ||
            header_.ghostColor != static_cast<std::uint32_t>(Color::White) ||
            header_.orientation !=
              static_cast<std::uint32_t>(orientation_) ||
            header_.squares != Squares || header_.stateCount != StateCount ||
            header_.nodeBytes != sizeof(NodeDisk) ||
            header_.geometryBytes != sizeof(ExternalGeometryMeta) ||
            header_.maskBytes != sizeof(ExternalMask) ||
            header_.rootBytes != sizeof(ExternalRobdd::Id) ||
            header_.reserved || header_.nodes < 2 ||
            header_.nodes > std::numeric_limits<std::uint32_t>::max() ||
            header_.geometries != 492'960 ||
            header_.strata > header_.geometries * Squares ||
            header_.ownerRoots != header_.geometries * Squares ||
            header_.nodeOffset != nodeOffset ||
            header_.geometryOffset != geometryOffset ||
            header_.stratumOffset != stratumOffset ||
            header_.ownerOffset != ownerOffset ||
            header_.observerOffset != observerOffset ||
            header_.visibleOwnerOffset != visibleOwnerOffset ||
            header_.visibleObserverOffset != visibleObserverOffset ||
            extent != bytes_ ||
            header_.payloadBytes != bytes_ - sizeof(header_) ||
            std::string(header_.sourceSha.data(), 64) !=
              bindings.sourceSha256 ||
            std::string(header_.modelSha.data(), 64) !=
              bindings.modelSha256 ||
            std::string(header_.observationSha.data(), 64) !=
              bindings.observationSha256 ||
            std::string(header_.lowerGhostSha.data(), 64) !=
              bindings.lowerGhostSidecarSha256 ||
            std::string(header_.lowerDragonFullSha.data(), 64) !=
              bindings.lowerDragonFullSha256 ||
            std::string(header_.lowerDragonSourceSha.data(), 64) !=
              bindings.lowerDragonSourceSha256 ||
            std::string(header_.lowerDragonModelSha.data(), 64) !=
              bindings.lowerDragonModelSha256 ||
            std::string(header_.semantics.data(),
                        std::strlen(SidecarSemantics)) != SidecarSemantics ||
            GhostPublicExtraExact::sha256_file(path_, sizeof(header_)) !=
              std::string(header_.payloadSha.data(), 64))
            throw std::runtime_error(
              "Dragon arbitrary sidecar header/binding residual");
        if (!valid_sha(header_hash(header_.sourceSha)) ||
            !valid_sha(header_hash(header_.normalizedSourceSha)) ||
            !valid_sha(header_hash(header_.modelSha)) ||
            !valid_sha(header_hash(header_.observationSha)) ||
            !valid_sha(header_hash(header_.lowerGhostSha)) ||
            !valid_sha(header_hash(header_.lowerDragonFullSha)) ||
            !valid_sha(header_hash(header_.lowerDragonSourceSha)) ||
            !valid_sha(header_hash(header_.lowerDragonModelSha)) ||
            !valid_sha(header_hash(header_.transitionPayloadSha)) ||
            !valid_sha(header_hash(header_.payloadSha)) ||
            std::any_of(header_.transitionSha.begin(),
                        header_.transitionSha.end(),
              [&](const auto& digest) {
                  return !valid_sha(header_hash(digest));
              }))
            throw std::runtime_error("Dragon sidecar has invalid SHA fields");
        const std::string fullSha = GhostPublicExtraExact::sha256_file(path_);
        if (!restore && !valid_sha(bindings.arbitrarySidecarSha256))
            throw std::invalid_argument(
              "probe-only Dragon load requires full UFGD1 SHA-256");
        if (!bindings.arbitrarySidecarSha256.empty() &&
            bindings.arbitrarySidecarSha256 != fullSha)
            throw std::runtime_error("Dragon sidecar full SHA mismatch");
        if (restore) {
            if (GhostPublicExtraExact::sha256_file(restore->sourceTable) !=
                  bindings.sourceSha256 ||
                GhostPublicExtraExact::sha256_file(
                  restore->lowerGhostSidecar) !=
                  bindings.lowerGhostSidecarSha256)
                throw std::runtime_error(
                  "Dragon strict-restore dependency mismatch");
            if (GhostPublicExtraExact::sha256_file(
                  restore->lowerDragonTable) !=
                  bindings.lowerDragonFullSha256 ||
                bindings.lowerDragonFullSha256 !=
                  bindings.lowerDragonSourceSha256 ||
                restore->lowerDragonModelSha256 !=
                  bindings.lowerDragonModelSha256)
                throw std::runtime_error(
                  "Dragon lower-concrete restore mismatch");
            (void)LowerDragonTable(restore->lowerDragonTable);
            const OriginalTable original(restore->sourceTable, orientation_);
            const NormalizedSource normalized = normalize_source(original,
              orientation_, restore->scratchPrefix +
                            ".restore-normalized.uftb");
            if (normalized.sha !=
                std::string(header_.normalizedSourceSha.data(), 64))
                throw std::runtime_error(
                  "Dragon normalized-source restore mismatch");
            std::size_t component = 0;
            for (const char* suffix : {".header", ".meta", ".strata",
                                       ".index", ".blocks", ".verified"})
                if (GhostPublicExtraExact::sha256_file(
                      restore->transitionPrefix + suffix) !=
                    std::string(header_.transitionSha[component++].data(), 64))
                    throw std::runtime_error(
                      "Dragon transition provenance mismatch");
            if (transition_payload_sha(restore->transitionPrefix) !=
                std::string(header_.transitionPayloadSha.data(), 64))
                throw std::runtime_error(
                  "Dragon combined transition provenance mismatch");
        }
        for (std::uint64_t id = 0; id < header_.nodes; ++id) {
            const NodeDisk& node = nodes()[id];
            if (id <= 1) {
                if (node.variable != Squares || node.low != id ||
                    node.high != id)
                    throw std::runtime_error(
                      "Dragon sidecar terminal tuple residual");
            }
            else if (node.variable >= Squares || node.low >= id ||
                     node.high >= id || node.low == node.high ||
                     (node.low > 1 && nodes()[node.low].variable <=
                                      node.variable) ||
                     (node.high > 1 && nodes()[node.high].variable <=
                                       node.variable))
                throw std::runtime_error(
                  "Dragon sidecar ROBDD structural residual");
        }
        if (restore)
            validate_unique_node_tuples(nodes(), header_.nodes);
        for (std::uint64_t id = 0; id < header_.ownerRoots; ++id)
            if (owner_roots()[id] >= header_.nodes ||
                visible_owner()[id] > 1 || visible_observer()[id] > 1)
                throw std::runtime_error("Dragon owner-root residual");
        for (std::uint64_t id = 0; id < header_.strata; ++id)
            if (observer_roots()[id] >= header_.nodes)
                throw std::runtime_error("Dragon observer-root residual");
    }

    std::string path_;
    Orientation orientation_;
    int descriptor_ = -1;
    const std::uint8_t* data_ = nullptr;
    std::uint64_t bytes_ = 0;
    SidecarHeader header_{};
    ExtraGeometryDomain domain_;
};

ArbitrarySidecarProbe::ArbitrarySidecarProbe(
  const std::string& path, const ProbeBindings& bindings)
  : impl_(std::make_unique<Impl>(path, bindings)) {}
ArbitrarySidecarProbe::ArbitrarySidecarProbe(
  const std::string& path, const SolveOptions& bindings)
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

}  // namespace Stockfish::Ultimate::GhostDragonExact
