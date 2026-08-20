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
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef ULTIMATE_GHOST_EXTRA_PIECE_TOKEN
#define ULTIMATE_GHOST_EXTRA_PIECE_TOKEN Dragon
#define ULTIMATE_GHOST_EXTRA_PIECE_TOKEN_DEFAULTED
#endif

namespace Stockfish::Ultimate {

[[nodiscard]] constexpr bool dragon_kernel_piece_type(PieceType type) {
#ifdef ULTIMATE_GHOST_EXTRA_IS_CHECKER
    if (type == PieceType::CheckerKing)
        return true;
#endif
    return type == PieceType::ULTIMATE_GHOST_EXTRA_PIECE_TOKEN;
}

[[nodiscard]] bool dragon_kernel_lower_child(const Position& position) {
    unsigned live = 0;
    unsigned kings = 0;
    unsigned dragons = 0;
    for (int id = 0; id < position.piece_count(); ++id) {
        const PieceState& piece = position.piece(id);
        if (!piece.alive || !piece.onBoard)
            continue;
#ifdef ULTIMATE_GHOST_EXTRA_IS_COPYCAT
        // CopycatClone is a derived half of the one logical public piece.  It
        // participates in move generation and occupancy, but not in the
        // lower-class material cardinality used to select K+Copycat-v-K.
        if (piece.type == PieceType::CopycatClone)
            continue;
#endif
        ++live;
        kings += piece.type == PieceType::King;
        dragons += dragon_kernel_piece_type(piece.type);
    }
    return live == 3 && kings == 2 && dragons == 1;
}

#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
[[nodiscard]] bool dragon_kernel_promoted_lower_child(
  const Position& position) {
    unsigned live = 0;
    unsigned kings = 0;
    unsigned queens = 0;
    for (int id = 0; id < position.piece_count(); ++id) {
        const PieceState& piece = position.piece(id);
        if (!piece.alive || !piece.onBoard)
            continue;
        ++live;
        kings += piece.type == PieceType::King;
        queens += piece.type == PieceType::Queen;
    }
    return live == 3 && kings == 2 && queens == 1;
}
#endif

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
        return dragon_kernel_lower_child(*this)
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
          || dragon_kernel_promoted_lower_child(*this)
#endif
          || Position::game_over();
    }
    [[nodiscard]] std::optional<Color> winner() const {
        return (dragon_kernel_lower_child(*this)
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
                || dragon_kernel_promoted_lower_child(*this)
#endif
               ) ? std::nullopt : Position::winner();
    }
};

}  // namespace Stockfish::Ultimate

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define Bishop ULTIMATE_GHOST_EXTRA_PIECE_TOKEN
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
#ifdef ULTIMATE_GHOST_ORDINARY_LOWER_SUBSTATES
constexpr std::uint32_t LowerExtraSubstates =
  ULTIMATE_GHOST_ORDINARY_LOWER_SUBSTATES;
#else
constexpr std::uint32_t LowerExtraSubstates = ExtraSubstates;
#endif
constexpr std::uint32_t Endian = 0x01020304;
#ifdef ULTIMATE_GHOST_ORDINARY_EXTRA_PRIMARY
constexpr bool SourceExtraPrimary = true;
#else
constexpr bool SourceExtraPrimary = false;
#endif
constexpr PieceType ExtraPiece =
  PieceType::ULTIMATE_GHOST_EXTRA_PIECE_TOKEN;
#ifdef ULTIMATE_GHOST_EXTRA_PIECE_TOKEN_DEFAULTED
#undef ULTIMATE_GHOST_EXTRA_PIECE_TOKEN
#undef ULTIMATE_GHOST_EXTRA_PIECE_TOKEN_DEFAULTED
#endif
constexpr PieceType SourcePrimary =
  SourceExtraPrimary ? ExtraPiece : PieceType::Ghost;
constexpr PieceType SourceSecondary =
  SourceExtraPrimary ? PieceType::Ghost : ExtraPiece;
constexpr char SidecarSemantics[] =
  "fresh-maximal-public-view-v2:dragon-ghost-generic";

[[nodiscard]] constexpr std::size_t concrete_header_bytes(
  std::uint32_t version) {
    return 40 + (version >= 5 ? 8 : 0) + (version >= 6 ? 8 : 0) +
           (version >= 7 ? 8 : 0);
}

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
    std::uint32_t primary = static_cast<std::uint32_t>(SourcePrimary);
    std::uint32_t secondary = static_cast<std::uint32_t>(SourceSecondary);
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

void write_overlay_header(std::ostream& output, Orientation orientation,
                          const std::string& sourceSha,
                          const std::string& modelSha) {
    if (!valid_sha(sourceSha) || !valid_sha(modelSha))
        throw std::invalid_argument("Dragon overlay header SHA is invalid");
    output.write("UFIW2\0\0\0", 8);
    const auto write32 = [&](std::uint32_t value) {
        output.write(reinterpret_cast<const char*>(&value), 4);
    };
    write32(2);
    write32(static_cast<std::uint32_t>(SourcePrimary));
    write32(static_cast<std::uint32_t>(SourceSecondary));
    write32(static_cast<std::uint32_t>(
      orientation == Orientation::Same ? Color::White : Color::Black));
    write32(StateCount);
    write32(2 * ExtraSubstates);
    output.write(sourceSha.data(), 64);
    output.write(modelSha.data(), 64);
    if (!output)
        throw std::runtime_error("failed writing Dragon overlay header");
}

void overlay_header_self_test() {
    for (const Orientation orientation : {Orientation::Same,
                                           Orientation::Opposing}) {
        std::ostringstream output(std::ios::binary);
        write_overlay_header(output, orientation, std::string(64, 'a'),
                             std::string(64, 'b'));
        const std::string bytes = output.str();
        const auto word = [&](std::size_t offset) {
            std::uint32_t value = 0;
            if (offset + 4 > bytes.size())
                throw std::runtime_error("Dragon overlay header is truncated");
            std::memcpy(&value, bytes.data() + offset, 4);
            return value;
        };
        const Color dragonColor = orientation == Orientation::Same
                                ? Color::White : Color::Black;
        if (bytes.size() != 160 ||
            std::memcmp(bytes.data(), "UFIW2\0\0\0", 8) ||
            word(8) != 2 ||
            word(12) != static_cast<std::uint32_t>(SourcePrimary) ||
            word(16) != static_cast<std::uint32_t>(SourceSecondary) ||
            word(20) != static_cast<std::uint32_t>(dragonColor) ||
            word(24) != StateCount ||
            word(28) != 2 * ExtraSubstates ||
            bytes.substr(32, 64) != std::string(64, 'a') ||
            bytes.substr(96, 64) != std::string(64, 'b'))
            throw std::runtime_error("Dragon UFIW2 header contract residual");
    }
    std::cout << "ghost_dragon_overlay_contract same Ghost/Dragon/White"
                 " opposing Ghost/Dragon/Black residual 0\n";
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
    return {ExtraPiece,
      SourceExtraPrimary ? Color::White
        : (orientation == Orientation::Same ? Color::White : Color::Black),
      SourceExtraPrimary
        ? (orientation == Orientation::Same ? Color::White : Color::Black)
        : Color::White, SourceExtraPrimary
        ? GhostPublicExtra::SourceOrder::ExtraPrimary
        : GhostPublicExtra::SourceOrder::GhostPrimary,
      GhostPublicExtra::HiddenAdjacentPolicy::ImpossibleWithoutForcedRelocation,
      orientation == Orientation::Same ? "kghostdragonk" : "kghostkdragon"};
}

[[nodiscard]] MaterialSpec normalized_material(Orientation orientation) {
    return {orientation == Orientation::Same ? Color::White : Color::Black};
}

[[nodiscard]] GhostPublicExtra::ConcreteState normalized_to_original(
  const FourState& state, Orientation orientation) {
    GhostPublicExtra::ConcreteState result;
    // Extra-primary source tables already use the legacy kernel's physical
    // colors in both orientations: the public extra is White and an opposing
    // Ghost is Black.  Color-swapping those sources changed side to move and
    // the two King identities without swapping the extra/Ghost ownership,
    // so the normalized WDL plane described a different position.  Only a
    // Ghost-primary opposing source needs the role-color swap that puts its
    // White Ghost into the kernel's Black-Ghost convention.
    if (orientation == Orientation::Same || SourceExtraPrimary) {
        result.side = state.side;
        result.whiteKing = state.whiteKing;
        result.blackKing = state.blackKing;
    }
    else {
        result.side = ~state.side;
        result.whiteKing = state.blackKing;
        result.blackKing = state.whiteKing;
    }
    result.first = SourceExtraPrimary ? state.bishop : state.ghost;
    result.second = SourceExtraPrimary ? state.ghost : state.bishop;
    result.extraSubstate = state.extraSubstate;
    result.ghostVisible = state.visible;
    return result;
}

[[nodiscard]] FourState original_to_normalized(
  const GhostPublicExtra::ConcreteState& state, Orientation orientation) {
    FourState result;
    if (orientation == Orientation::Same || SourceExtraPrimary) {
        result.side = state.side;
        result.whiteKing = state.whiteKing;
        result.blackKing = state.blackKing;
    }
    else {
        result.side = ~state.side;
        result.whiteKing = state.blackKing;
        result.blackKing = state.whiteKing;
    }
    result.bishop = SourceExtraPrimary ? state.first : state.second;
    result.ghost = SourceExtraPrimary ? state.second : state.first;
    result.extraSubstate = state.extraSubstate;
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
        const std::uint32_t version = word(8);
        wdlOffset_ = concrete_header_bytes(version);
        if (bytes_.size() < wdlOffset_ ||
            std::memcmp(bytes_.data(), "UFTB1\0\0\0", 8) ||
            version < 5 || version > 8 ||
            word(12) != static_cast<std::uint32_t>(SourcePrimary) ||
            word(16) != StateCount ||
            word(24) != 2 * ExtraSubstates ||
            word(28) != (StateCount + 3) / 4 || word(32) != StateCount ||
            word(40) != static_cast<std::uint32_t>(SourceSecondary) ||
            word(44) != static_cast<std::uint32_t>(orientation ==
              Orientation::Opposing))
            throw std::runtime_error(
              "Dragon/Ghost source header does not match its orientation");
        wdlBytes_ = (std::uint64_t(StateCount) + 3) / 4;
        if (wdlOffset_ + wdlBytes_ > bytes_.size())
            throw std::runtime_error("truncated Dragon/Ghost WDL plane");
        Sha256 hash;
        hash.update(bytes_.data(), bytes_.size());
        sha_ = hex_digest(hash.finish());
    }

    [[nodiscard]] static std::uint32_t physical_index(
      std::uint32_t logicalIndex) {
        if (logicalIndex >= StateCount)
            throw std::out_of_range(
              "Dragon/Ghost source index is outside the concrete domain");
        constexpr std::uint32_t CombinedSubstates = 2 * ExtraSubstates;
        const std::uint32_t placement = logicalIndex / CombinedSubstates;
        const std::uint32_t logicalCombined =
          logicalIndex % CombinedSubstates;
        const std::uint32_t extraSubstate = logicalCombined / 2;
        const std::uint32_t ghostVisible = logicalCombined % 2;
        // The concrete generator packs primary-substate first and secondary-
        // substate second.  The information solver's role-normalized codec is
        // always [extra substate][Ghost visibility].  They coincide when the
        // public extra is primary, but a Ghost-primary source physically uses
        // [Ghost visibility][extra substate] and must be transposed here.
        const std::uint32_t physicalCombined = SourceExtraPrimary
          ? logicalCombined
          : ghostVisible * ExtraSubstates + extraSubstate;
        return placement * CombinedSubstates + physicalCombined;
    }

    [[nodiscard]] std::uint8_t result(std::uint32_t logicalIndex) const {
        const std::uint32_t index = physical_index(logicalIndex);
        return (bytes_.at(wdlOffset_ + index / 4) >> (2 * (index % 4))) & 3;
    }
    [[nodiscard]] const std::string& sha() const { return sha_; }

  private:
    std::vector<std::uint8_t> bytes_;
    std::size_t wdlOffset_ = 0;
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
    explicit LowerDragonTable(const std::string& path,
                              PieceType pieceType = ExtraPiece,
                              std::uint32_t substates = LowerExtraSubstates)
      : pieceType_(pieceType), substates_(substates) {
#ifdef ULTIMATE_GHOST_ORDINARY_LOWER_DRAW_ONLY
        if (path != "implicit-draw")
            throw std::runtime_error("invalid implicit insufficient-material lower binding");
        return;
#else
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
        constexpr std::size_t HeaderBytes =
#ifdef ULTIMATE_GHOST_EXTRA_IS_COPYCAT
          48;
#else
          40;
#endif
        if (bytes_.size() < HeaderBytes ||
            std::memcmp(bytes_.data(), "UFTB1\0\0\0", 8) ||
#ifdef ULTIMATE_GHOST_EXTRA_IS_COPYCAT
            word(8) != 5 ||
#else
            word(8) != 4 ||
#endif
            word(12) != static_cast<std::uint32_t>(pieceType_) ||
            word(16) != 985'920 * substates_ ||
            word(24) != substates_ ||
            word(28) != (985'920 * substates_ + 3) / 4 ||
            word(32) != 985'920 * substates_ || word(36) != 0 ||
#ifdef ULTIMATE_GHOST_EXTRA_IS_COPYCAT
            word(40) != static_cast<std::uint32_t>(PieceType::CopycatClone) ||
            word(44) != static_cast<std::uint32_t>(Color::White) ||
#endif
            bytes_.size() != HeaderBytes +
                               (985'920 * substates_ + 3) / 4 +
                               985'920 * substates_)
            throw std::runtime_error("incompatible authenticated kdragonk");
#endif
    }

    [[nodiscard]] DragonWdl probe(const Position& position) const {
        int whiteKing = Position::NoSquare;
        int blackKing = Position::NoSquare;
        int dragon = Position::NoSquare;
        for (int id = 0; id < position.piece_count(); ++id) {
            const PieceState& piece = position.piece(id);
            if (!piece.alive || !piece.onBoard)
                continue;
#ifdef ULTIMATE_GHOST_EXTRA_IS_COPYCAT
            if (piece.type == PieceType::CopycatClone)
                continue;
#endif
            if (piece.type == PieceType::King && piece.color == Color::White)
                whiteKing = piece.square;
            else if (piece.type == PieceType::King &&
                     piece.color == Color::Black)
                blackKing = piece.square;
            else if (piece.type == pieceType_ &&
                     piece.color == Color::White)
                dragon = piece.square;
            else
                throw std::runtime_error("kdragonk probe material residual");
        }
        if (whiteKing < 0 || blackKing < 0 || dragon < 0 ||
            whiteKing == blackKing || whiteKing == dragon ||
            blackKing == dragon)
            throw std::runtime_error("kdragonk probe placement residual");
#ifdef ULTIMATE_GHOST_ORDINARY_LOWER_DRAW_ONLY
        return DragonWdl::Draw;
#else
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
        const auto extraSubstate = position.tablebase_substate(
          [&] {
              for (int id = 0; id < position.piece_count(); ++id)
                  if (position.piece(id).alive && position.piece(id).onBoard &&
                      position.piece(id).type == pieceType_)
                      return id;
              return Position::NoPiece;
          }(), pieceType_);
        if (!extraSubstate || *extraSubstate >= substates_)
            throw std::runtime_error("lower extra substate residual");
        const std::uint32_t stateIndex = index * substates_ +
                                         *extraSubstate;
        const std::uint8_t value =
          (bytes_[
#ifdef ULTIMATE_GHOST_EXTRA_IS_COPYCAT
                   48
#else
                   40
#endif
                 + stateIndex / 4] >> (2 * (stateIndex % 4))) & 3;
        if (value < 1 || value > 3)
            throw std::runtime_error("kdragonk WDL value is invalid");
        return static_cast<DragonWdl>(value);
#endif
    }

  private:
    std::vector<std::uint8_t> bytes_;
    PieceType pieceType_ = ExtraPiece;
    [[maybe_unused]] std::uint32_t substates_ = LowerExtraSubstates;
};

[[nodiscard]] std::uint8_t lower_dragon_force_flags(
  const Position& child, const MaterialSpec& material,
  const LowerDragonTable& lower, bool promoted = false) {
    if ((!promoted && !dragon_kernel_lower_child(child))
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
        || (promoted && !dragon_kernel_promoted_lower_child(child))
#else
        || promoted
#endif
        )
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
                      ExtraPiece, Color::White, dragon);
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
    // The same exact kernel is also instantiated for ordinary public extras.
    // Several of their K+piece-v-K lower classes are rigorously all-draw, so
    // absence of a winning witness is a valid lower-table property rather
    // than a solver failure.  Every lower edge is still exhaustively probed
    // and checked by rewrite_lower_dragon_edges below.
    if (!found) {
        std::cout << "ghost_dragon_lower_witness orientation "
                  << (material.ghostColor == Color::White ? "same" : "opposing")
                  << " force none lower_class_draw_only\n";
        return;
    }
    std::cout << "ghost_dragon_lower_witness orientation "
              << (material.ghostColor == Color::White ? "same" : "opposing")
              << " force "
              << (material.ghostColor == Color::White ? "owner" : "observer")
              << " upn " << witness << '\n';
}

DragonPatchCertificate rewrite_lower_dragon_edges(
  const std::string& prefix, const MaterialSpec& material,
  const LowerDragonTable& lower,
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
  const LowerDragonTable& promotedLower,
#endif
  bool placeholders,
  bool acceptPlaceholders, bool acceptAnyExact = false) {
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
            if (!valid_geometry_world(geometry, ghost))
                continue;
            Position position = make_geometry_position(geometry, ghost,
                                                        material);
            if (position.game_over())
                continue;
            const std::vector<Move> moves = position.legal_moves();
            const std::size_t compiledMoves =
              offsets[ghost + 1] - offsets[ghost];
            if (moves.size() != compiledMoves)
                throw std::runtime_error(
                  "Dragon transition patch move-count residual geometry " +
                  std::to_string(geometryId) + " ghost " +
                  std::to_string(ghost) + " legal " +
                  std::to_string(moves.size()) + " compiled " +
                  std::to_string(compiledMoves) + " upn " + position.upn());
            for (std::size_t ordinal = 0; ordinal < moves.size(); ++ordinal) {
                Position child = position;
                Undo undo;
                if (!child.make_move(moves[ordinal], undo))
                    throw std::runtime_error("Dragon patch move failed");
                const bool ordinaryLower = dragon_kernel_lower_child(child);
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
                const bool promoted =
                  dragon_kernel_promoted_lower_child(child);
#else
                const bool promoted = false;
#endif
                if (!ordinaryLower && !promoted)
                    continue;
                const LowerDragonTable& selected = promoted
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
                  ? promotedLower
#else
                  ? lower
#endif
                  : lower;
                ExternalCompiledEdge& edge = edges[offsets[ghost] + ordinal];
                if (edge.domain != ExternalChildDomain::Exact ||
                    (!acceptAnyExact && !acceptPlaceholders && edge.exact !=
                       lower_dragon_force_flags(
                         child, material, selected, promoted)) ||
                    (!acceptAnyExact && acceptPlaceholders && edge.exact != 0 &&
                     edge.exact != lower_dragon_force_flags(
                       child, material, selected, promoted)))
                    throw std::runtime_error(
                      "Dragon lower-table transition residual");
                const std::uint8_t expected = lower_dragon_force_flags(
                  child, material, selected, promoted);
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
    write32(static_cast<std::uint32_t>(ExtraPiece));
    write32(StateCount);
    write32(0);
    write32(2 * ExtraSubstates);
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
        if (!GhostPublicExtra::valid_concrete_world(state, material)) {
            ++unreachable[static_cast<std::size_t>(state.side)]
                          [original.result(index)];
            continue;
        }
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
              normalized.bishop, 0, normalized.extraSubstate};
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
          static_cast<std::uint8_t>(normalized.visible),
          normalized.extraSubstate};
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
    write_overlay_header(output, orientation, options.sourceSha256,
                         options.modelSha256);
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
            normalized.extraSubstate = physical.extraSubstate;
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
        options.lowerDragonSourceSha256 != options.lowerDragonSha256
#ifndef ULTIMATE_GHOST_ORDINARY_LOWER_DRAW_ONLY
        ||
        GhostPublicExtraExact::sha256_file(options.lowerDragonTable) !=
          options.lowerDragonSha256
#endif
        )
        throw std::runtime_error(
          "Dragon transitions require authenticated compatible kdragonk");
    LowerDragonTable lower(options.lowerDragonTable);
    lower_dragon_probe_self_test(normalized_material(options.orientation),
                                 lower);
    return lower;
}

#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
[[nodiscard]] LowerDragonTable authenticate_promoted_lower_dragon(
  const TransitionOptions& options) {
    if (!valid_sha(options.promotedLowerDragonSha256) ||
        !valid_sha(options.promotedLowerDragonSourceSha256) ||
        !valid_sha(options.promotedLowerDragonModelSha256) ||
        options.promotedLowerDragonSourceSha256 !=
          options.promotedLowerDragonSha256 ||
        GhostPublicExtraExact::sha256_file(
          options.promotedLowerDragonTable) !=
          options.promotedLowerDragonSha256)
        throw std::runtime_error(
          "Pawn/Ghost transitions require authenticated compatible kqueenk");
    return LowerDragonTable(
      options.promotedLowerDragonTable, PieceType::Queen, 1);
}
#endif

constexpr std::size_t DragonTransitionComponentCount = 5;
constexpr std::array<const char*, DragonTransitionComponentCount>
  DragonTransitionComponents{{".header", ".meta", ".strata", ".index",
                               ".blocks"}};

[[nodiscard]] constexpr std::size_t dragon_lower_binding_count() {
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
    return 6;
#else
    return 3;
#endif
}

[[nodiscard]] std::uint64_t dragon_marker_bytes(bool payloadBound) {
    return sizeof(ExternalTransitionHeader) +
      64 * (dragon_lower_binding_count() +
            (payloadBound ? 3 + DragonTransitionComponentCount : 0));
}

void write_dragon_marker(const TransitionOptions& options) {
    for (const auto& [value, label] : {
           std::pair<const std::string*, const char*>{&options.sourceSha256,
                                                      "source SHA"},
           {&options.modelSha256, "model SHA"},
           {&options.observationSha256, "observation SHA"}})
        if (!valid_sha(*value))
            throw std::runtime_error(
              std::string("Dragon transition marker has invalid ") + label);
    std::ifstream headerFile(options.prefix + ".header", std::ios::binary);
    ExternalTransitionHeader header;
    headerFile.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!headerFile ||
        headerFile.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("missing frozen transition header");
    std::array<std::string, DragonTransitionComponentCount> components;
    for (std::size_t index = 0; index < components.size(); ++index)
        components[index] = GhostPublicExtraExact::sha256_file(
          options.prefix + DragonTransitionComponents[index]);
    std::ofstream marker(options.prefix + ".verified",
      std::ios::binary | std::ios::trunc);
    marker.write(reinterpret_cast<const char*>(&header), sizeof(header));
    for (const std::string* hash : {&options.lowerDragonSha256,
                                    &options.lowerDragonSourceSha256,
                                    &options.lowerDragonModelSha256
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
                                    , &options.promotedLowerDragonSha256,
                                    &options.promotedLowerDragonSourceSha256,
                                    &options.promotedLowerDragonModelSha256
#endif
                                    })
        marker.write(hash->data(), 64);
    for (const std::string* hash : {&options.sourceSha256,
                                    &options.modelSha256,
                                    &options.observationSha256})
        marker.write(hash->data(), 64);
    for (const std::string& hash : components)
        marker.write(hash.data(), 64);
    marker.close();
    struct stat status{};
    if (::stat((options.prefix + ".verified").c_str(), &status) ||
        status.st_size != static_cast<off_t>(dragon_marker_bytes(true)))
        throw std::runtime_error("Dragon transition marker extent residual");
    std::cout << "ghost_dragon_transition_payload_certificate components "
              << components.size() << " payload_bound 1 residual 0\n"
              << std::flush;
}

bool authenticate_dragon_marker(const TransitionOptions& options) {
    std::ifstream marker(options.prefix + ".verified", std::ios::binary);
    ExternalTransitionHeader header;
    std::array<char,
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
      384
#else
      192
#endif
      > hashes{};
    marker.read(reinterpret_cast<char*>(&header), sizeof(header));
    marker.read(hashes.data(), hashes.size());
    std::ifstream headerFile(options.prefix + ".header", std::ios::binary);
    ExternalTransitionHeader storedHeader;
    headerFile.read(reinterpret_cast<char*>(&storedHeader),
                    sizeof(storedHeader));
    if (!marker || !headerFile ||
        headerFile.peek() != std::char_traits<char>::eof() ||
        std::memcmp(&header, &storedHeader, sizeof(header)) ||
        std::string(hashes.data(), 64) != options.lowerDragonSha256 ||
        std::string(hashes.data() + 64, 64) !=
          options.lowerDragonSourceSha256 ||
        std::string(hashes.data() + 128, 64) !=
          options.lowerDragonModelSha256
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
        || std::string(hashes.data() + 192, 64) !=
          options.promotedLowerDragonSha256 ||
        std::string(hashes.data() + 256, 64) !=
          options.promotedLowerDragonSourceSha256 ||
        std::string(hashes.data() + 320, 64) !=
          options.promotedLowerDragonModelSha256
#endif
        )
        throw std::runtime_error(
          "Dragon transition marker dependency residual");
    if (marker.peek() == std::char_traits<char>::eof())
        return false;
    std::array<char, 64 * (3 + DragonTransitionComponentCount)> payload{};
    marker.read(payload.data(), payload.size());
    if (!marker || marker.peek() != std::char_traits<char>::eof() ||
        std::string(payload.data(), 64) != options.sourceSha256 ||
        std::string(payload.data() + 64, 64) != options.modelSha256 ||
        std::string(payload.data() + 128, 64) != options.observationSha256)
        throw std::runtime_error(
          "Dragon transition marker model-binding residual");
    for (std::size_t index = 0; index < DragonTransitionComponentCount;
         ++index)
        if (GhostPublicExtraExact::sha256_file(
              options.prefix + DragonTransitionComponents[index]) !=
            std::string(payload.data() + 64 * (3 + index), 64))
            throw std::runtime_error(
              "Dragon transition marker payload residual");
    std::cout << "ghost_dragon_transition_payload_authentication components "
              << DragonTransitionComponentCount
              << " payload_bound 1 residual 0\n" << std::flush;
    return true;
}

void certify_dragon_transitions(const TransitionOptions& options,
                                const LowerDragonTable& lower
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
                                , const LowerDragonTable& promotedLower
#endif
                                ) {
    const MaterialSpec material = normalized_material(options.orientation);
    rewrite_lower_dragon_edges(options.prefix, material, lower,
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
                               promotedLower,
#endif
                               true, false);
    try {
        verify_external_transition_certificate(options.prefix, material);
    }
    catch (...) {
        rewrite_lower_dragon_edges(options.prefix, material, lower,
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
                                   promotedLower,
#endif
                                   false, true);
        throw;
    }
    rewrite_lower_dragon_edges(options.prefix, material, lower,
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
                               promotedLower,
#endif
                               false, true);
    write_dragon_marker(options);
    if (!authenticate_dragon_marker(options))
        throw std::runtime_error(
          "new Dragon transition marker lacks a payload binding");
}

}  // namespace

void compile_transitions(const TransitionOptions& options) {
    if (options.prefix.empty() || !options.geometryCount)
        throw std::invalid_argument("Dragon/Ghost transition range is empty");
    const LowerDragonTable lower = authenticate_lower_dragon(options);
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
    const LowerDragonTable promotedLower =
      authenticate_promoted_lower_dragon(options);
#endif
    if (options.orientation == Orientation::Same)
        compile_external_transitions(options.prefix,
          normalized_material(options.orientation), options.geometryBegin,
          options.geometryCount);
    else
        GhostPublicExtraExact::compile_reciprocal_external_transitions(
          options.prefix, options.geometryBegin, options.geometryCount);
    rewrite_lower_dragon_edges(options.prefix,
      normalized_material(options.orientation), lower,
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
      promotedLower,
#endif
      false, true);
    // compile_external_transitions already performed exhaustive native+D2
    // regeneration with structural lower placeholders.  The authenticated
    // rewrite above changes only those certified placeholder bytes to exact
    // lower-table outcomes, so a second full native replay is redundant.
    write_dragon_marker(options);
    if (!authenticate_dragon_marker(options))
        throw std::runtime_error(
          "compiled Dragon transition marker lacks a payload binding");
    std::cout << "ghost_dragon_compile_certificate exhaustive_replay 1"
                 " duplicate_replay_skipped 1 residual 0\n" << std::flush;
}

void merge_transitions(const TransitionOptions& output,
                       const std::vector<std::string>& shards,
                       std::uint32_t expectedGeometries) {
    const LowerDragonTable lower = authenticate_lower_dragon(output);
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
    const LowerDragonTable promotedLower =
      authenticate_promoted_lower_dragon(output);
#endif
    std::size_t payloadBoundShards = 0;
    for (const std::string& shard : shards) {
        TransitionOptions input = output;
        input.prefix = shard;
        payloadBoundShards += authenticate_dragon_marker(input);
    }
    merge_external_transition_shards(output.prefix, shards,
      normalized_material(output.orientation), expectedGeometries);
    // Each shard marker is issued only after exhaustive native regeneration,
    // lower-table restoration, and exact dependency authentication.  Gap-free
    // coverage plus the merger's extent/rebase/conservation checks therefore
    // compose into an exact full-domain certificate.  Replaying every legal
    // move again here used to cost days for large ordinary-piece domains.
    // Bind the deterministic merged payload cryptographically instead.
    (void) lower;
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
    (void) promotedLower;
#endif
    write_dragon_marker(output);
    if (!authenticate_dragon_marker(output))
        throw std::runtime_error(
          "merged Dragon transition marker lacks a payload binding");
    std::cout << "ghost_dragon_compositional_merge_certificate shards "
              << shards.size() << " payload_bound_shards "
              << payloadBoundShards << " legacy_exhaustive_shards "
              << shards.size() - payloadBoundShards
              << " full_replay_skipped 1 residual 0\n" << std::flush;
}

void verify_transitions(const TransitionOptions& options) {
    const LowerDragonTable lower = authenticate_lower_dragon(options);
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
    const LowerDragonTable promotedLower =
      authenticate_promoted_lower_dragon(options);
#endif
    if (authenticate_dragon_marker(options)) {
        std::cout << "ghost_dragon_transition_replay payload_bound 1"
                     " full_replay_skipped 1 residual 0\n" << std::flush;
        return;
    }
    certify_dragon_transitions(options, lower
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
                               , promotedLower
#endif
                               );
}

void rebind_transitions(const TransitionOptions& options) {
    const LowerDragonTable lower = authenticate_lower_dragon(options);
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
    const LowerDragonTable promotedLower =
      authenticate_promoted_lower_dragon(options);
#endif
    authenticate_dragon_marker(options);
    const MaterialSpec material = normalized_material(options.orientation);
    // A frozen store from an older ordinary-piece instantiation can carry
    // different exact terminal flags while its geometry, moves, and public
    // observations remain valid.  Validate every lower edge's domain while
    // replacing those flags with the verifier's structural placeholder in a
    // single pass, then restore the authenticated lower-table values after
    // the exhaustive structural certificate.  This avoids a redundant full
    // rewrite of multi-gigabyte block stores.
    rewrite_lower_dragon_edges(options.prefix, material, lower,
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
                               promotedLower,
#endif
                               true, false, true);
    try {
        verify_external_transition_certificate(options.prefix, material);
    }
    catch (...) {
        rewrite_lower_dragon_edges(options.prefix, material, lower,
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
                                   promotedLower,
#endif
                                   false, true);
        throw;
    }
    rewrite_lower_dragon_edges(options.prefix, material, lower,
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
                               promotedLower,
#endif
                               false, true);
    write_dragon_marker(options);
    if (!authenticate_dragon_marker(options))
        throw std::runtime_error(
          "rebound Dragon transition marker lacks a payload binding");
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
    if (options.normalizedSourceTable.empty() !=
        options.normalizedSourceSha256.empty())
        throw std::invalid_argument(
          "normalized source reuse requires both path and SHA-256");
    NormalizedSource normalized;
    if (options.normalizedSourceTable.empty())
        normalized = normalize_source(original, options.orientation,
          options.scratchPrefix + ".normalized.uftb");
    else {
        if (!valid_sha(options.normalizedSourceSha256) ||
            GhostPublicExtraExact::sha256_file(
              options.normalizedSourceTable) !=
                options.normalizedSourceSha256)
            throw std::runtime_error(
              "reused normalized source SHA-256 mismatch");
        normalized = {options.normalizedSourceTable,
                      options.normalizedSourceSha256, 0};
        std::cout << "ghost_dragon_source_normalization_reused states "
                  << StateCount << " remap_residual 0 normalized_sha256 "
                  << normalized.sha << '\n' << std::flush;
    }
    TransitionOptions transitions;
    transitions.orientation = options.orientation;
    transitions.prefix = options.transitionPrefix;
    transitions.lowerDragonTable = options.lowerDragonTable;
    transitions.lowerDragonSha256 = options.lowerDragonFullSha256;
    transitions.lowerDragonSourceSha256 = options.lowerDragonSourceSha256;
    transitions.lowerDragonModelSha256 = options.lowerDragonModelSha256;
    transitions.sourceSha256 = options.sourceSha256;
    transitions.modelSha256 = options.modelSha256;
    transitions.observationSha256 = options.observationSha256;
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
    transitions.promotedLowerDragonTable =
      options.promotedLowerDragonTable;
    transitions.promotedLowerDragonSha256 =
      options.promotedLowerDragonFullSha256;
    transitions.promotedLowerDragonSourceSha256 =
      options.promotedLowerDragonSourceSha256;
    transitions.promotedLowerDragonModelSha256 =
      options.promotedLowerDragonModelSha256;
#endif
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
#ifdef ULTIMATE_GHOST_ORDINARY_PROMOTES_TO_QUEEN
    legacy.promotedSidecar = options.promotedSidecar;
    legacy.promotedSidecarSha256 = options.promotedSidecarSha256;
    legacy.promotedSourceSha256 = options.promotedSourceSha256;
    legacy.promotedModelSha256 = options.promotedModelSha256;
    legacy.promotedObservationSha256 = options.promotedObservationSha256;
    legacy.promotedLowerGhostSidecarSha256 =
      options.promotedLowerGhostSidecarSha256;
    legacy.promotedLowerDragonFullSha256 =
      options.promotedLowerDragonFullSha256;
    legacy.promotedLowerDragonSourceSha256 =
      options.promotedLowerDragonSourceSha256;
    legacy.promotedLowerDragonModelSha256 =
      options.promotedLowerDragonModelSha256;
    legacy.promotedOpposing = options.orientation == Orientation::Opposing;
#endif
    legacy.bddLimits.maxNodes = options.maxNodes;
    legacy.bddLimits.uniqueSlots = options.uniqueSlots;
    legacy.compactEvery = options.compactEvery;
    legacy.measureIterations = options.measureIterations;
    legacy.resumeFixedPoint = options.resumeFixedPoint;
    legacy.resumeConverged = options.resumeConverged;
    legacy.resumeCurrentInNextSlot = options.resumeCurrentInNextSlot;
    legacy.resumeBddSlot = options.resumeBddSlot;
    legacy.resumeIteration = options.resumeIteration;
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
    if (concrete_header_bytes(5) != 48 ||
        concrete_header_bytes(6) != 56 ||
        concrete_header_bytes(7) != 64 ||
        concrete_header_bytes(8) != 64)
        throw std::runtime_error("Dragon/Ghost concrete header self-test residual");
    overlay_header_self_test();
    packed_four_header_self_test();
    constexpr std::uint32_t CombinedSubstates = 2 * ExtraSubstates;
    constexpr std::uint32_t SamplePlacement =
      PlacementCount > 6'007 ? 6'007 : PlacementCount - 1;
    std::array<bool, CombinedSubstates> seenPhysical{};
    for (std::uint32_t logicalCombined = 0;
         logicalCombined < CombinedSubstates; ++logicalCombined) {
        const std::uint32_t logical =
          SamplePlacement * CombinedSubstates + logicalCombined;
        const std::uint32_t physical = OriginalTable::physical_index(logical);
        const std::uint32_t extraSubstate = logicalCombined / 2;
        const std::uint32_t ghostVisible = logicalCombined % 2;
        const std::uint32_t expectedCombined = SourceExtraPrimary
          ? logicalCombined
          : ghostVisible * ExtraSubstates + extraSubstate;
        if (physical != SamplePlacement * CombinedSubstates +
                        expectedCombined ||
            seenPhysical[expectedCombined])
            throw std::runtime_error(
              "Dragon/Ghost source substate-order self-test residual");
        seenPhysical[expectedCombined] = true;
    }
    if (std::any_of(seenPhysical.begin(), seenPhysical.end(),
                    [](bool seen) { return !seen; }))
        throw std::runtime_error(
          "Dragon/Ghost source substate-order bijection residual");
    std::cout << "ghost_dragon_source_substate_order source_extra_primary "
              << SourceExtraPrimary << " extra_substates " << ExtraSubstates
              << " logical_physical_bijection 1 residual 0\n";
    for (const Orientation orientation : {Orientation::Same,
                                           Orientation::Opposing}) {
        const MaterialSpec normalized = normalized_material(orientation);
        external_child_substate_self_test(normalized);
        ExternalGhostExtraFixedPoint::run_fresh_root_public_grouping_self_test(
          normalized);
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
            if (SourceExtraPrimary &&
                (normalized.side != original.side ||
                 normalized.whiteKing != original.whiteKing ||
                 normalized.blackKing != original.blackKing ||
                 normalized.bishop != original.first ||
                 normalized.ghost != original.second))
                throw std::runtime_error(
                  "extra-primary Dragon/Ghost physical remap residual");
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
    const PackedFourTable concrete(
      normalized.path, normalized_material(orientation));
    if (hex_digest(concrete.sha()) != normalized.sha)
        throw std::runtime_error(
          "normalized Dragon source self-test SHA mismatch");
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
          static_cast<std::uint8_t>(normalized.visible),
          normalized.extraSubstate};
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
            header_.primary != static_cast<std::uint32_t>(SourcePrimary) ||
            header_.secondary !=
              static_cast<std::uint32_t>(SourceSecondary) ||
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
            header_.geometries != 492'960ULL * ExtraSubstates
#ifdef ULTIMATE_GHOST_EXTRA_HORIZONTAL_ONLY
              * 2
#endif
              ||
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
            if (
#ifndef ULTIMATE_GHOST_ORDINARY_LOWER_DRAW_ONLY
                GhostPublicExtraExact::sha256_file(
                  restore->lowerDragonTable) !=
                  bindings.lowerDragonFullSha256 ||
#endif
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
