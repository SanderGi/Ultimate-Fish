/*
  Ultimate Fish - exact K+Ghost/Giant public-information solver
  Copyright (C) 2026 Ultimate Fish contributors
  GPLv3 or later.

  This translation unit is deliberately isolated from the frozen d597 Bishop
  and a88 reciprocal-Bishop source domains. The audited point-piece kernel is
  included under a token-local Giant specialization; no frozen source byte is
  modified and the Giant model receives an independent catalog fingerprint.
*/

#include "ghost_giant_information_solver.h"

#include "external_robdd.h"
#include "ghost_giant_information_model.h"
#include "ghost_information_probe.h"
#include "information.h"
#include "position.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <filesystem>
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

namespace Stockfish::Ultimate {

[[nodiscard]] bool giant_kernel_lower_child(const Position& position) {
    unsigned live = 0;
    unsigned kings = 0;
    unsigned giants = 0;
    for (int id = 0; id < position.piece_count(); ++id) {
        const PieceState& piece = position.piece(id);
        if (!piece.alive || !piece.onBoard)
            continue;
        ++live;
        kings += piece.type == PieceType::King;
        giants += piece.type == PieceType::Giant;
    }
    return live == 3 && kings == 2 && giants == 1;
}

// The audited Bishop compiler's only material-specific escape assumes that a
// lone public extra is an insufficient draw.  Keep its bytes frozen, but make
// that branch reachable for Giant so the isolated post-compiler can replace
// the placeholder with an authenticated K+Giant-v-K result.  No native move
// generation or terminal rule sees this wrapper: only the textual proof
// kernel's child classifier does.
class GiantKernelPosition : public Position {
  public:
    using Position::Position;
    GiantKernelPosition() = default;
    GiantKernelPosition(const Position& position) : Position(position) {}
    GiantKernelPosition(Position&& position) : Position(std::move(position)) {}
    [[nodiscard]] bool game_over() const {
        return giant_kernel_lower_child(*this) || Position::game_over();
    }
    [[nodiscard]] std::optional<Color> winner() const {
        return giant_kernel_lower_child(*this) ? std::nullopt
                                                : Position::winner();
    }
};

}  // namespace Stockfish::Ultimate

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define inherited_lower_mask_self_test()                                  \
    giant_constructor_lower_bypass(); [[maybe_unused]] void                \
      bishop_inherited_lower_mask_test()
#define fresh_root_public_grouping_self_test()                             \
    giant_constructor_grouping_bypass(); [[maybe_unused]] void             \
      bishop_fresh_root_grouping_test()
#define Bishop Giant
#define Position GiantKernelPosition
#define private public
#include "ghost_public_extra_information_solver.cpp"
#undef private
#undef Position
#undef Bishop
#undef fresh_root_public_grouping_self_test
#undef inherited_lower_mask_self_test
#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace Stockfish::Ultimate {
namespace {


// The included fixed-point implementation is reused only for its exact ROBDD
// Bellman kernel. Its constructor samples point-piece Bishop geometry, which
// is not a valid certificate for a four-square Giant. The isolated Giant
// compiler and tests below replace both assertions with exhaustive
// anchor-aware certificates before any solve can begin.
void ExternalGhostExtraFixedPoint::giant_constructor_lower_bypass() {}
void ExternalGhostExtraFixedPoint::giant_constructor_grouping_bypass() {}

}  // namespace
}  // namespace Stockfish::Ultimate

namespace Stockfish::Ultimate::GhostGiantExact {
namespace {

constexpr std::uint32_t Squares = Position::BoardSquares;
constexpr std::uint32_t StateCount = GhostGiant::StateCount;
constexpr std::uint32_t PlacementCount = GhostGiant::PlacementCount;
constexpr std::uint32_t Endian = 0x01020304;
constexpr char SidecarSemantics[] =
  "fresh-maximal-public-view-v2:giant-anchor-v2-ghost";

[[nodiscard]] std::uint32_t giant_geometry_code(
  const GhostGiant::PublicGeometry& geometry) {
    return geometry.side | (std::uint32_t(geometry.whiteKing) << 1) |
           (std::uint32_t(geometry.blackKing) << 8) |
           (std::uint32_t(geometry.giant) << 15) |
           (std::uint32_t(geometry.visible) << 22);
}

class GiantGeometryDomain {
  public:
    GiantGeometryDomain() {
        geometries_.reserve(359'100);
        byCode_.reserve(720'000);
        for (std::uint8_t side = 0; side < 2; ++side)
            for (std::uint8_t whiteKing = 0; whiteKing < Squares;
                 ++whiteKing)
                for (std::uint8_t blackKing = 0; blackKing < Squares;
                     ++blackKing) {
                    if (whiteKing == blackKing)
                        continue;
                    for (std::uint8_t giant = 0; giant < Squares; ++giant) {
                        const Bitboard footprint =
                          GhostGiant::giant_footprint(giant);
                        if (!footprint ||
                            (footprint & (Bitboard(1) << whiteKing)) ||
                            (footprint & (Bitboard(1) << blackKing)))
                            continue;
                        for (std::uint8_t visible = 0; visible < 2;
                             ++visible) {
                            const GhostGiant::PublicGeometry raw{
                              side, whiteKing, blackKing, giant, visible};
                            if (!(GhostGiant::canonical_geometry(raw).geometry ==
                                  raw))
                                continue;
                            const std::uint32_t id =
                              static_cast<std::uint32_t>(geometries_.size());
                            geometries_.push_back(raw);
                            if (!byCode_.emplace(giant_geometry_code(raw), id)
                                   .second)
                                throw std::runtime_error(
                                  "duplicate Giant public geometry");
                        }
                    }
                }
        if (geometries_.size() != 359'100)
            throw std::runtime_error(
              "Giant public geometry quotient count residual");
    }

    [[nodiscard]] std::size_t size() const { return geometries_.size(); }
    [[nodiscard]] const GhostGiant::PublicGeometry& operator[](
      std::uint32_t id) const { return geometries_.at(id); }
    [[nodiscard]] std::pair<std::uint32_t,
                            GhostGiant::RectangleTransform>
    locate(const GhostGiant::PublicGeometry& raw) const {
        const auto canonical = GhostGiant::canonical_geometry(raw);
        const auto found = byCode_.find(
          giant_geometry_code(canonical.geometry));
        if (found == byCode_.end() ||
            !(geometries_.at(found->second) == canonical.geometry))
            throw std::runtime_error(
              "canonical Giant public geometry is not interned");
        return {found->second, canonical.transform};
    }

    // The frozen Bellman block builder needs only side/visible for each custom
    // geometry. Populate its private catalog with exact Giant anchors and one
    // unreachable sentinel so its point-piece fresh reporter stays disabled.
    void install_legacy_view(ExtraGeometryDomain& domain) const {
        domain.geometries_.clear();
        domain.byCode_.clear();
        domain.geometries_.reserve(geometries_.size() + 1);
        for (const auto& geometry : geometries_)
            domain.geometries_.push_back({geometry.side,
              geometry.whiteKing, geometry.blackKing, geometry.giant,
              geometry.visible});
        domain.geometries_.push_back(domain.geometries_.front());
    }

  private:
    std::vector<GhostGiant::PublicGeometry> geometries_;
    std::unordered_map<std::uint32_t, std::uint32_t> byCode_;
};

[[nodiscard]] MaterialSpec normalized_material(Orientation orientation) {
    return {orientation == Orientation::Same ? Color::White : Color::Black};
}

[[nodiscard]] GhostGiant::Orientation model_orientation(
  Orientation orientation) {
    return orientation == Orientation::Same
         ? GhostGiant::Orientation::Same : GhostGiant::Orientation::Opposing;
}

[[nodiscard]] Position make_giant_world(
  const GhostGiant::PublicGeometry& geometry, std::uint8_t ghost,
  Orientation orientation) {
    GhostGiant::NormalizedState state;
    state.side = static_cast<Color>(geometry.side);
    state.whiteKing = geometry.whiteKing;
    state.blackKing = geometry.blackKing;
    state.giant = geometry.giant;
    state.ghost = ghost;
    state.ghostVisible = geometry.visible != 0;
    return GhostGiant::make_position(state, model_orientation(orientation));
}

[[nodiscard]] bool ghost_square_available(
  const GhostGiant::PublicGeometry& geometry, std::uint8_t ghost) {
    return ghost != geometry.whiteKing && ghost != geometry.blackKing &&
           !(GhostGiant::giant_footprint(geometry.giant) &
             (Bitboard(1) << ghost));
}

#pragma pack(push, 1)
struct NodeDisk {
    std::uint8_t variable = Squares;
    std::uint32_t low = 0;
    std::uint32_t high = 0;
};

struct SidecarHeader {
    std::array<char, 8> magic{{'U','F','G','I','1','\0','\0','\0'}};
    std::uint32_t version = 1;
    std::uint32_t headerBytes = sizeof(SidecarHeader);
    std::uint32_t endian = Endian;
    std::uint32_t primary = static_cast<std::uint32_t>(PieceType::Ghost);
    std::uint32_t secondary = static_cast<std::uint32_t>(PieceType::Giant);
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
    std::array<char, 64> lowerGiantFullSha{};
    std::array<char, 64> lowerGiantSourceSha{};
    std::array<char, 64> lowerGiantModelSha{};
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
        throw std::invalid_argument("Giant overlay header SHA is invalid");
    output.write("UFIW2\0\0\0", 8);
    const auto write32 = [&](std::uint32_t value) {
        output.write(reinterpret_cast<const char*>(&value), 4);
    };
    write32(2);
    write32(static_cast<std::uint32_t>(PieceType::Ghost));
    write32(static_cast<std::uint32_t>(PieceType::Giant));
    write32(static_cast<std::uint32_t>(
      orientation == Orientation::Same ? Color::White : Color::Black));
    write32(StateCount);
    write32(2);
    output.write(sourceSha.data(), 64);
    output.write(modelSha.data(), 64);
    if (!output)
        throw std::runtime_error("failed writing Giant overlay header");
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
                throw std::runtime_error("Giant overlay header is truncated");
            std::memcpy(&value, bytes.data() + offset, 4);
            return value;
        };
        const Color giantColor = orientation == Orientation::Same
                                ? Color::White : Color::Black;
        if (bytes.size() != 160 ||
            std::memcmp(bytes.data(), "UFIW2\0\0\0", 8) ||
            word(8) != 2 ||
            word(12) != static_cast<std::uint32_t>(PieceType::Ghost) ||
            word(16) != static_cast<std::uint32_t>(PieceType::Giant) ||
            word(20) != static_cast<std::uint32_t>(giantColor) ||
            word(24) != StateCount || word(28) != 2 ||
            bytes.substr(32, 64) != std::string(64, 'a') ||
            bytes.substr(96, 64) != std::string(64, 'b'))
            throw std::runtime_error("Giant UFIW2 header contract residual");
    }
    std::cout << "ghost_giant_overlay_contract same Ghost/Giant/White"
                 " opposing Ghost/Giant/Black residual 0\n";
}

[[nodiscard]] std::string transition_payload_sha(
  const std::string& prefix) {
    Sha256 hash;
    std::array<std::uint8_t, 1 << 20> buffer{};
    for (const char* suffix : {".header", ".meta", ".strata", ".index",
                               ".blocks", ".verified"}) {
        std::ifstream input(prefix + suffix, std::ios::binary);
        if (!input)
            throw std::runtime_error("missing Giant transition provenance");
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
              "Giant sidecar contains a duplicate ROBDD tuple");
}

[[nodiscard]] GhostGiant::SourceState normalized_to_original(
  const FourState& state, Orientation orientation) {
    const GhostGiant::NormalizedState normalized{state.side, state.whiteKing,
      state.blackKing, state.bishop, state.ghost, state.visible};
    return GhostGiant::denormalize(normalized, model_orientation(orientation));
}

class OriginalTable {
  public:
    OriginalTable(const std::string& path, Orientation orientation) {
        std::ifstream input(path, std::ios::binary);
        if (!input)
            throw std::runtime_error("cannot open Giant/Ghost source table");
        bytes_ = {std::istreambuf_iterator<char>(input),
                  std::istreambuf_iterator<char>()};
        const auto word = [&](std::size_t offset) {
            std::uint32_t value = 0;
            if (offset + 4 > bytes_.size())
                throw std::runtime_error("truncated Giant/Ghost source header");
            std::memcpy(&value, bytes_.data() + offset, 4);
            return value;
        };
        std::uint64_t giantTag = 0;
        if (bytes_.size() >= 64)
            std::memcpy(&giantTag, bytes_.data() + 56, 8);
        constexpr std::uint64_t GiantAnchorV2Tag =
          0x32474e4149474655ULL;
        if (bytes_.size() < 64 ||
            std::memcmp(bytes_.data(), "UFTB1\0\0\0", 8) ||
            word(8) != 7 ||
            word(12) != static_cast<std::uint32_t>(PieceType::Ghost) ||
            word(16) != StateCount || word(24) != 2 ||
            word(28) != PlacementCount / 2 || word(32) != StateCount ||
            word(40) != static_cast<std::uint32_t>(PieceType::Giant) ||
            word(44) != static_cast<std::uint32_t>(orientation ==
              Orientation::Opposing) || giantTag != GiantAnchorV2Tag)
            throw std::runtime_error(
              "Giant/Ghost source header does not match its orientation");
        wdlBytes_ = (std::uint64_t(StateCount) + 3) / 4;
        if (bytes_.size() != 64 + wdlBytes_ + StateCount)
            throw std::runtime_error("Giant/Ghost source extent is invalid");
        Sha256 hash;
        hash.update(bytes_.data(), bytes_.size());
        sha_ = hex_digest(hash.finish());
    }

    [[nodiscard]] std::uint8_t result(std::uint32_t index) const {
        const std::uint8_t value =
          (bytes_.at(64 + index / 4) >> (2 * (index % 4))) & 3;
        if (value < 1 || value > 3)
            throw std::runtime_error("Giant/Ghost source WDL is invalid");
        return value;
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

enum class GiantWdl : std::uint8_t { Win = 1, Loss = 2, Draw = 3 };

class LowerGiantTable {
  public:
    explicit LowerGiantTable(const std::string& path) {
        std::ifstream input(path, std::ios::binary);
        bytes_ = {std::istreambuf_iterator<char>(input),
                  std::istreambuf_iterator<char>()};
        const auto word = [&](std::size_t offset) {
            std::uint32_t value = 0;
            if (offset + 4 > bytes_.size())
                throw std::runtime_error("truncated kgiantk header");
            std::memcpy(&value, bytes_.data() + offset, 4);
            return value;
        };
        if (bytes_.size() < 40 ||
            std::memcmp(bytes_.data(), "UFTB1\0\0\0", 8) ||
            word(8) != 4 ||
            word(12) != static_cast<std::uint32_t>(PieceType::Giant) ||
            word(16) != 985'920 || word(24) != 1 ||
            word(28) != 246'480 || word(32) != 985'920 || word(36) != 0 ||
            bytes_.size() != 40 + 246'480 + 985'920)
            throw std::runtime_error("incompatible authenticated kgiantk");
    }

    [[nodiscard]] GiantWdl probe(const Position& position) const {
        int whiteKing = Position::NoSquare;
        int blackKing = Position::NoSquare;
        int giant = Position::NoSquare;
        for (int id = 0; id < position.piece_count(); ++id) {
            const PieceState& piece = position.piece(id);
            if (!piece.alive || !piece.onBoard)
                continue;
            if (piece.type == PieceType::King && piece.color == Color::White)
                whiteKing = piece.square;
            else if (piece.type == PieceType::King &&
                     piece.color == Color::Black)
                blackKing = piece.square;
            else if (piece.type == PieceType::Giant &&
                     piece.color == Color::White)
                giant = piece.square;
            else
                throw std::runtime_error("kgiantk probe material residual");
        }
        if (whiteKing < 0 || blackKing < 0 || giant < 0 ||
            !GhostGiant::valid_giant_anchor(
              static_cast<std::uint8_t>(giant)) ||
            whiteKing == blackKing ||
            (GhostGiant::giant_footprint(static_cast<std::uint8_t>(giant)) &
             ((Bitboard(1) << whiteKing) | (Bitboard(1) << blackKing))))
            throw std::runtime_error("kgiantk probe placement residual");
        const std::uint32_t blackRank = blackKing -
          (blackKing > whiteKing ? 1u : 0u);
        const int low = std::min(whiteKing, blackKing);
        const int high = std::max(whiteKing, blackKing);
        const std::uint32_t giantRank = giant - (giant > low ? 1u : 0u) -
          (giant > high ? 1u : 0u);
        const std::uint32_t index =
          ((static_cast<std::uint32_t>(position.side_to_move()) * Squares +
             whiteKing) * (Squares - 1) + blackRank) * (Squares - 2) +
          giantRank;
        const std::uint8_t value =
          (bytes_[40 + index / 4] >> (2 * (index % 4))) & 3;
        if (value < 1 || value > 3)
            throw std::runtime_error("kgiantk WDL value is invalid");
        return static_cast<GiantWdl>(value);
    }

  private:
    std::vector<std::uint8_t> bytes_;
};

[[nodiscard]] std::uint8_t lower_giant_force_flags(
  const Position& child, const MaterialSpec& material,
  const LowerGiantTable& lower) {
    if (!giant_kernel_lower_child(child))
        throw std::runtime_error("lower-Giant force requested for wrong class");
    std::optional<Color> winner;
    if (child.game_over())
        winner = child.winner();
    else {
        const GiantWdl result = lower.probe(child);
        if (result == GiantWdl::Win)
            winner = child.side_to_move();
        else if (result == GiantWdl::Loss)
            winner = ~child.side_to_move();
    }
    return static_cast<std::uint8_t>(
      winner && *winner == material.ghostColor ? 1 :
      winner && *winner == material.observer() ? 2 : 0);
}

struct GiantCompileCertificate {
    std::uint64_t geometries = 0;
    std::uint64_t worlds = 0;
    std::uint64_t edges = 0;
    std::uint64_t actionChecks = 0;
    std::uint64_t decisionChecks = 0;
    std::uint64_t observationChecks = 0;
    std::uint64_t childChecks = 0;
    std::uint64_t footprintRejects = 0;
};

[[nodiscard]] std::string giant_action_key_bytes(const Move& move) {
    const auto action = GhostGiant::action_key(move);
    std::string key;
    key.reserve(5 * sizeof(std::uint32_t));
    const auto append = [&](std::uint32_t value) {
        key.append(reinterpret_cast<const char*>(&value), sizeof(value));
    };
    append(action.from);
    append(action.to);
    append(action.auxiliary);
    append(static_cast<std::uint32_t>(action.kind));
    append(static_cast<std::uint32_t>(action.promotion));
    return key;
}

[[nodiscard]] ExternalCompiledEdge encode_giant_child(
  const Position& child, Orientation orientation,
  const GiantGeometryDomain& domain, const MaterialSpec& material,
  const LowerGiantTable& lower, const ExtraGeometryDomain& legacyDomain) {
    if (const auto state = GhostGiant::scan_same_class(
          child, model_orientation(orientation))) {
        const GhostGiant::PublicGeometry raw{
          static_cast<std::uint8_t>(state->side), state->whiteKing,
          state->blackKing, state->giant,
          static_cast<std::uint8_t>(state->ghostVisible)};
        const auto [geometry, transform] = domain.locate(raw);
        ExternalCompiledEdge edge;
        edge.domain = ExternalChildDomain::SameClass;
        edge.child = geometry;
        edge.childActual = GhostGiant::transform_square(
          state->ghost, transform);
        return edge;
    }
    if (giant_kernel_lower_child(child)) {
        ExternalCompiledEdge edge;
        edge.domain = ExternalChildDomain::Exact;
        edge.exact = lower_giant_force_flags(child, material, lower);
        return edge;
    }
    return encode_external_child(child, material, legacyDomain);
}

GiantCompileCertificate compile_giant_raw(
  const TransitionOptions& options, const LowerGiantTable& lower,
  const GiantGeometryDomain& domain) {
    if (options.geometryBegin >= domain.size() || !options.geometryCount ||
        std::uint64_t(options.geometryBegin) + options.geometryCount >
          domain.size())
        throw std::invalid_argument("Giant transition range is outside domain");
    const MaterialSpec material = normalized_material(options.orientation);
    const DisclosureContext observer{material.observer(), false};
    const ExtraGeometryDomain legacyDomain;
    std::ofstream headerFile(options.prefix + ".header",
      std::ios::binary | std::ios::trunc);
    std::ofstream metaFile(options.prefix + ".meta",
      std::ios::binary | std::ios::trunc);
    std::ofstream strataFile(options.prefix + ".strata",
      std::ios::binary | std::ios::trunc);
    std::ofstream indexFile(options.prefix + ".index",
      std::ios::binary | std::ios::trunc);
    std::ofstream blockFile(options.prefix + ".blocks",
      std::ios::binary | std::ios::trunc);
    if (!headerFile || !metaFile || !strataFile || !indexFile || !blockFile)
        throw std::runtime_error("cannot create Giant transition range");
    ExternalTransitionHeader header;
    header.material = static_cast<std::uint32_t>(material.ghostColor);
    header.geometryCount = options.geometryCount;
    header.reserved = options.geometryBegin;
    headerFile.write(reinterpret_cast<const char*>(&header), sizeof(header));

    GiantCompileCertificate certificate;
    ExternalCompileSummary summary;
    for (std::uint32_t local = 0; local < options.geometryCount; ++local) {
        const std::uint32_t geometryId = options.geometryBegin + local;
        const auto& geometry = domain[geometryId];
        ExternalGeometryMeta meta;
        meta.actualStratum.fill(NoIndex);
        std::map<std::string, ExternalMask> decisionBlocks;
        for (std::uint8_t ghost = 0; ghost < Squares; ++ghost) {
            if (!ghost_square_available(geometry, ghost)) {
                ++certificate.footprintRejects;
                continue;
            }
            Position position = make_giant_world(geometry, ghost,
                                                 options.orientation);
            ++certificate.worlds;
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
        std::unordered_map<std::string, std::uint32_t> decisionIds;
        for (const auto& [decision, mask] : decisionBlocks) {
            const std::uint32_t stratum =
              static_cast<std::uint32_t>(summary.strata++);
            decisionIds.emplace(decision, stratum - meta.stratumBase);
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
          actionBijection, decisionBijection, observationBijection;
        const auto intern = [](auto& map, const std::string& text) {
            if (const auto found = map.find(text); found != map.end())
                return found->second;
            const std::uint32_t id = static_cast<std::uint32_t>(map.size());
            if (!map.emplace(text, id).second)
                throw std::runtime_error("Giant transition interner residual");
            return id;
        };
        for (std::uint8_t ghost = 0; ghost < Squares; ++ghost) {
            if (!external_mask_test(meta.live, ghost))
                continue;
            Position position = make_giant_world(geometry, ghost,
                                                 options.orientation);
            const std::vector<Move> moves = position.legal_moves();
            for (const Move& move : moves) {
                Position child = position;
                Undo undo;
                if (!child.make_move(move, undo))
                    throw std::runtime_error("Giant compiler legal move failed");
                ExternalCompiledEdge edge = encode_giant_child(
                  child, options.orientation, domain, material, lower,
                  legacyDomain);
                edge.relation = intern(observations,
                  complete_transition_observation(position, move, child,
                                                  observer));
                edge.action = intern(actions, giant_action_key_bytes(move));
                perSource[ghost].push_back(edge);
                ++certificate.edges;
                ++summary.edges;
                if (edge.domain == ExternalChildDomain::SameClass)
                    ++summary.sameClass;
                else if (edge.domain == ExternalChildDomain::LowerGhost)
                    ++summary.lowerGhost;
                else
                    ++summary.exact;
            }

            for (std::uint8_t value = 1; value < 4; ++value) {
                const auto transform =
                  static_cast<GhostGiant::RectangleTransform>(value);
                const auto pairedGeometry = GhostGiant::transform_geometry(
                  geometry, transform);
                const std::uint8_t pairedGhost =
                  GhostGiant::transform_square(ghost, transform);
                Position paired = make_giant_world(pairedGeometry,
                  pairedGhost, options.orientation);
                const std::vector<Move> pairedMoves = paired.legal_moves();
                if (!geometry.visible &&
                    static_cast<Color>(geometry.side) ==
                      material.observer()) {
                    const auto sourceDecision = decisionIds.find(
                      decision_observation_key(position, observer));
                    if (sourceDecision == decisionIds.end())
                        throw std::runtime_error(
                          "Giant decision cell was not interned");
                    const std::vector<GhostGiant::DecisionMarker> transformed =
                      GhostGiant::transform_markers(position, moves,
                                                    transform);
                    if (transformed != GhostGiant::decision_markers(paired))
                        throw std::runtime_error(
                          "Giant D2 legal-dot partition residual");
                    decisionBijection[value].bind(sourceDecision->second,
                      decision_observation_key(paired, observer),
                      "Giant decision observation");
                    ++certificate.decisionChecks;
                }
                std::map<GhostGiant::ActionKey, std::size_t> pairedByAction;
                for (std::size_t index = 0; index < pairedMoves.size(); ++index)
                    if (!pairedByAction.emplace(
                          GhostGiant::action_key(pairedMoves[index]), index)
                           .second)
                        throw std::runtime_error(
                          "Giant D2 target duplicates an action");
                if (pairedMoves.size() != moves.size())
                    throw std::runtime_error("Giant D2 action-count residual");
                for (std::size_t index = 0; index < moves.size(); ++index) {
                    const auto wanted = GhostGiant::transform_action(
                      position, moves[index], transform);
                    const auto found = pairedByAction.find(wanted);
                    if (found == pairedByAction.end())
                        throw std::runtime_error("Giant D2 lost an action");
                    Position pairedChild = paired;
                    Undo pairedUndo;
                    if (!pairedChild.make_move(pairedMoves[found->second],
                                               pairedUndo))
                        throw std::runtime_error("Giant D2 child failed");
                    const ExternalCompiledEdge pairedEdge = encode_giant_child(
                      pairedChild, options.orientation, domain, material,
                      lower, legacyDomain);
                    verify_external_child_symmetry(perSource[ghost][index],
                                                   pairedEdge);
                    actionBijection[value].bind(
                      perSource[ghost][index].action,
                      giant_action_key_bytes(pairedMoves[found->second]),
                      "Giant action");
                    observationBijection[value].bind(
                      perSource[ghost][index].relation,
                      complete_transition_observation(paired,
                        pairedMoves[found->second], pairedChild, observer),
                      "Giant observation");
                    ++certificate.actionChecks;
                    ++certificate.observationChecks;
                    ++certificate.childChecks;
                    ++summary.symmetryEdges;
                }
                ++summary.symmetryWorlds;
            }
        }
        summary.actionClasses += actions.size();
        summary.observationClasses += observations.size();
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
              static_cast<std::streamsize>(edges.size() * sizeof(edges.front())));
        metaFile.write(reinterpret_cast<const char*>(&meta), sizeof(meta));
        ++certificate.geometries;
        ++summary.geometries;
        header.completedGeometries = local + 1;
        header.edges = summary.edges;
        header.strata = summary.strata;
        header.blockBytes = static_cast<std::uint64_t>(blockFile.tellp());
    }
    const std::uint64_t finalOffset =
      static_cast<std::uint64_t>(blockFile.tellp());
    indexFile.write(reinterpret_cast<const char*>(&finalOffset),
                    sizeof(finalOffset));
    headerFile.seekp(0);
    headerFile.write(reinterpret_cast<const char*>(&header), sizeof(header));
    headerFile.close(); metaFile.close(); strataFile.close();
    indexFile.close(); blockFile.close();
    if (!headerFile || !metaFile || !strataFile || !indexFile || !blockFile ||
        !certificate.actionChecks || !certificate.observationChecks ||
        certificate.actionChecks != certificate.observationChecks ||
        certificate.actionChecks != certificate.childChecks)
        throw std::runtime_error("Giant transition certificate residual");
    std::cout << "ghost_giant_transition_certificate geometries "
              << certificate.geometries << " worlds " << certificate.worlds
              << " edges " << certificate.edges << " action_checks "
              << certificate.actionChecks << " decision_checks "
              << certificate.decisionChecks << " observation_checks "
              << certificate.observationChecks << " child_checks "
              << certificate.childChecks << " footprint_rejects "
              << certificate.footprintRejects << " residual 0\n";
    return certificate;
}
void lower_giant_probe_self_test(const MaterialSpec& material,
                                  const LowerGiantTable& lower) {
    bool found = false;
    std::string witness;
    for (const Color side : {Color::White, Color::Black}) {
        for (std::uint8_t whiteKing = 0; whiteKing < Squares && !found;
             ++whiteKing)
            for (std::uint8_t blackKing = 0; blackKing < Squares && !found;
                 ++blackKing)
                for (std::uint8_t giant = 0; giant < Squares && !found;
                     ++giant) {
                    const Bitboard footprint =
                      GhostGiant::giant_footprint(giant);
                    if (!footprint || whiteKing == blackKing ||
                        (footprint & ((Bitboard(1) << whiteKing) |
                                      (Bitboard(1) << blackKing))))
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
                      PieceType::Giant, Color::White, giant);
                    if (wk == Position::NoPiece || bk == Position::NoPiece ||
                        item == Position::NoPiece)
                        throw std::runtime_error(
                          "K+Giant witness construction residual");
                    position.piece(wk).moved = position.piece(bk).moved =
                      position.piece(item).moved = true;
                    position.set_side_to_move(side);
                    if (position.game_over())
                        continue;
                    const GiantWdl result = lower.probe(position);
                    const bool whiteWins =
                      (result == GiantWdl::Win &&
                       side == Color::White) ||
                      (result == GiantWdl::Loss &&
                       side == Color::Black);
                    if (!whiteWins)
                        continue;
                    const std::uint8_t expected =
                      material.ghostColor == Color::White ? 1 : 2;
                    if (lower_giant_force_flags(position, material, lower) !=
                        expected)
                        throw std::runtime_error(
                          "K+Giant force-role normalization residual");
                    witness = position.upn();
                    found = true;
                }
        if (found) break;
    }
    if (!found)
        throw std::runtime_error("no non-draw K+Giant witness was found");
    std::cout << "ghost_giant_lower_witness orientation "
              << (material.ghostColor == Color::White ? "same" : "opposing")
              << " force "
              << (material.ghostColor == Color::White ? "owner" : "observer")
              << " upn " << witness << '\n';
}

NormalizedSource normalize_source(const OriginalTable& source,
                                  Orientation orientation,
                                  const std::string& path) {
    std::vector<std::uint8_t> wdl((std::uint64_t(StateCount) + 3) / 4, 0);
    std::array<std::uint64_t, 4> originalCounts{};
    std::array<std::uint64_t, 4> normalizedCounts{};
    std::vector<std::uint8_t> seen((StateCount + 7) / 8, 0);
    std::uint64_t residual = 0;
    for (std::uint32_t index = 0; index < StateCount; ++index) {
        const GhostGiant::SourceState original =
          GhostGiant::decode_source(index);
        const GhostGiant::NormalizedState normalized = GhostGiant::normalize(
          original, model_orientation(orientation));
        const GhostGiant::SourceState restored = GhostGiant::denormalize(
          normalized, model_orientation(orientation));
        const std::uint32_t originalIndex = GhostGiant::encode_source(restored);
        const std::uint8_t bit = static_cast<std::uint8_t>(
          1u << (originalIndex % 8));
        residual += (seen[originalIndex / 8] & bit) != 0;
        seen[originalIndex / 8] |= bit;
        residual += originalIndex != index;
        const std::uint8_t value = source.result(originalIndex);
        if (value < 1 || value > 3)
            throw std::runtime_error("Giant/Ghost source has invalid WDL");
        wdl[index / 4] |= static_cast<std::uint8_t>(
          value << (2 * (index % 4)));
        ++originalCounts[value];
        ++normalizedCounts[value];
    }
    residual += std::any_of(seen.begin(), seen.end(),
      [](std::uint8_t byte) { return byte != 0xff; });
    if (residual || originalCounts != normalizedCounts)
        throw std::runtime_error("Giant/Ghost source normalization residual");
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write("UFTB1\0\0\0", 8);
    const auto write32 = [&](std::uint32_t value) {
        output.write(reinterpret_cast<const char*>(&value), 4);
    };
    // The inherited fixed-point object authenticates this private scratch
    // table but never interprets its point-piece codec. Exact singleton/root
    // verification below probes the original GiantAnchorV2 source directly.
    // Version 5 intentionally omits a false Giant-anchor codec promise.
    write32(5);
    write32(static_cast<std::uint32_t>(PieceType::Giant));
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
        throw std::runtime_error("failed writing normalized Giant/Ghost source");
    const std::string sha = GhostPublicExtraExact::sha256_file(path);
    std::cout << "ghost_giant_source_normalization states " << StateCount
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
    const GiantGeometryDomain domain;
    std::vector<std::uint8_t> admitted((StateCount + 7) / 8, 0);
    std::vector<ExternalMask> freshMasks(solver.database_.stratum_count());
    std::array<std::array<std::uint64_t, 4>, 2> unreachable{};
    std::uint64_t admittedCount = 0;
    for (std::uint32_t index = 0; index < StateCount; ++index) {
        const GhostGiant::SourceState state = GhostGiant::decode_source(index);
        const GhostGiant::NormalizedState normalizedState =
          GhostGiant::normalize(state, model_orientation(orientation));
        if (!GhostGiant::fresh_root_admitted(
              normalizedState, model_orientation(orientation))) {
            ++unreachable[static_cast<std::size_t>(state.side)]
                          [original.result(index)];
            continue;
        }
        admitted[index / 8] |= static_cast<std::uint8_t>(1u << (index % 8));
        ++admittedCount;
        if (!state.ghostVisible) {
            const GhostGiant::PublicGeometry raw{
              static_cast<std::uint8_t>(normalizedState.side),
              normalizedState.whiteKing, normalizedState.blackKing,
              normalizedState.giant, 0};
            const auto [geometry, transform] = domain.locate(raw);
            const unsigned actual = GhostGiant::transform_square(
              normalizedState.ghost, transform);
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
        const GhostGiant::SourceState state = GhostGiant::decode_source(index);
        const GhostGiant::NormalizedState normalized = GhostGiant::normalize(
          state, model_orientation(orientation));
        const std::size_t side = static_cast<std::size_t>(state.side);
        const GhostGiant::PublicGeometry raw{
          static_cast<std::uint8_t>(normalized.side), normalized.whiteKing,
          normalized.blackKing, normalized.giant,
          static_cast<std::uint8_t>(normalized.ghostVisible)};
        const auto [geometry, transform] = domain.locate(raw);
        const unsigned actual = GhostGiant::transform_square(
          normalized.ghost, transform);
        const ExternalGeometryMeta& meta = solver.database_.meta(geometry);
        ++realizations[side];
        std::uint64_t publicKey = 0;
        enum class RootKind { Visible, Stratum, Terminal } kind;
        if (normalized.ghostVisible) {
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
                throw std::runtime_error("Giant fresh root lacks stratum");
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
        else if (normalized.ghostVisible) {
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
                  "Giant admitted root crosses decision stratum");
            owner = solver.bdd_->evaluate(
              solver.ownerCurrent_[std::uint64_t(geometry) * Squares + actual],
              freshMasks[stratum].low, freshMasks[stratum].high);
            observer = solver.bdd_->evaluate(solver.observerCurrent_[stratum],
              freshMasks[stratum].low, freshMasks[stratum].high);
        }
        if (owner && observer)
            throw std::runtime_error("Giant fresh root has dual force");
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
        throw std::runtime_error("Giant fresh-root certificate residual");

    std::ofstream output(options.outputOverlay,
      std::ios::binary | std::ios::trunc);
    write_overlay_header(output, orientation, options.sourceSha256,
                         options.modelSha256);
    output.write(reinterpret_cast<const char*>(flags.data()),
                 static_cast<std::streamsize>(flags.size()));
    output.close();
    if (!output)
        throw std::runtime_error("failed writing Giant information overlay");
    std::cout << "ghost_giant_root_conservation admitted " << admittedCount
              << " total " << StateCount
              << " grouping_residual 0 conservation_residual 0\n";
    return certificate;
}

SolveCertificate write_sidecar(const SolveOptions& options,
                               ExternalGhostExtraFixedPoint& solver,
                               const std::string& normalizedSha) {
    if (options.outputArbitrary.empty())
        throw std::invalid_argument("Giant solve requires UFGI1 output");
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
    copy_hash(header.lowerGiantFullSha, options.lowerGiantFullSha256,
              "lower Giant full SHA");
    copy_hash(header.lowerGiantSourceSha, options.lowerGiantSourceSha256,
              "lower Giant source SHA");
    copy_hash(header.lowerGiantModelSha, options.lowerGiantModelSha256,
              "lower Giant model SHA");
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
        throw std::runtime_error("failed writing Giant arbitrary sidecar");
    struct stat status{};
    if (::stat(options.outputArbitrary.c_str(), &status) ||
        static_cast<std::uint64_t>(status.st_size) != extent)
        throw std::runtime_error("Giant arbitrary sidecar extent residual");
    copy_hash(header.payloadSha, GhostPublicExtraExact::sha256_file(
      options.outputArbitrary, sizeof(header)), "payload SHA");
    std::fstream rewrite(options.outputArbitrary,
      std::ios::binary | std::ios::in | std::ios::out);
    write_value(rewrite, header);
    rewrite.close();
    if (!rewrite)
        throw std::runtime_error("cannot finalize Giant sidecar");

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
                      "Giant singleton lacks decision stratum");
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
          "Giant arbitrary singleton reproduction residual");
}

}  // namespace

namespace {

[[nodiscard]] LowerGiantTable authenticate_lower_giant(
  const TransitionOptions& options) {
    if (!valid_sha(options.lowerGiantSha256) ||
        !valid_sha(options.lowerGiantSourceSha256) ||
        !valid_sha(options.lowerGiantModelSha256) ||
        options.lowerGiantSourceSha256 != options.lowerGiantSha256 ||
        GhostPublicExtraExact::sha256_file(options.lowerGiantTable) !=
          options.lowerGiantSha256)
        throw std::runtime_error(
          "Giant transitions require authenticated compatible kgiantk");
    LowerGiantTable lower(options.lowerGiantTable);
    lower_giant_probe_self_test(normalized_material(options.orientation),
                                 lower);
    return lower;
}

void write_giant_marker(const TransitionOptions& options,
                        const GiantCompileCertificate& certificate) {
    if (!valid_sha(options.modelSha256) ||
        !valid_sha(options.observationSha256))
        throw std::invalid_argument(
          "Giant transition model/observation binding is invalid");
    std::ifstream source(options.prefix + ".header", std::ios::binary);
    ExternalTransitionHeader header;
    source.read(reinterpret_cast<char*>(&header), sizeof(header));
    source.close();
    std::ofstream marker(options.prefix + ".verified",
      std::ios::binary | std::ios::trunc);
    marker.write(reinterpret_cast<const char*>(&header), sizeof(header));
    for (const std::string* hash : {&options.lowerGiantSha256,
                                    &options.lowerGiantSourceSha256,
                                    &options.lowerGiantModelSha256})
        marker.write(hash->data(), 64);
    marker.write(options.modelSha256.data(), 64);
    marker.write(options.observationSha256.data(), 64);
    marker.write("UFGTV4\0\0", 8);
    for (const std::uint64_t count : {
           certificate.geometries, certificate.worlds, certificate.edges,
           certificate.actionChecks, certificate.decisionChecks,
           certificate.observationChecks, certificate.childChecks,
           certificate.footprintRejects})
        marker.write(reinterpret_cast<const char*>(&count), sizeof(count));
    for (const char* suffix : {".header", ".meta", ".strata", ".index",
                               ".blocks"}) {
        const std::string digest = GhostPublicExtraExact::sha256_file(
          options.prefix + suffix);
        marker.write(digest.data(), 64);
    }
    marker.close();
    struct stat status{};
    if (::stat((options.prefix + ".verified").c_str(), &status) ||
        status.st_size != static_cast<off_t>(sizeof(header) + 712))
        throw std::runtime_error("Giant transition marker extent residual");
}

[[nodiscard]] GiantCompileCertificate authenticate_giant_marker(
  const TransitionOptions& options) {
    std::ifstream marker(options.prefix + ".verified", std::ios::binary);
    ExternalTransitionHeader header;
    std::array<char, 192> hashes{};
    std::array<char, 128> semanticHashes{};
    std::array<char, 8> magic{};
    std::array<std::uint64_t, 8> counts{};
    std::array<std::array<char, 64>, 5> components{};
    marker.read(reinterpret_cast<char*>(&header), sizeof(header));
    marker.read(hashes.data(), hashes.size());
    marker.read(semanticHashes.data(), semanticHashes.size());
    marker.read(magic.data(), magic.size());
    marker.read(reinterpret_cast<char*>(counts.data()),
                sizeof(counts));
    marker.read(reinterpret_cast<char*>(components.data()),
                sizeof(components));
    std::ifstream storedHeader(options.prefix + ".header", std::ios::binary);
    ExternalTransitionHeader actualHeader;
    storedHeader.read(reinterpret_cast<char*>(&actualHeader),
                      sizeof(actualHeader));
    if (!marker || marker.peek() != std::char_traits<char>::eof() ||
        !storedHeader || std::memcmp(&header, &actualHeader, sizeof(header)) ||
        magic != std::array<char, 8>{{'U','F','G','T','V','4','\0','\0'}} ||
        std::string(hashes.data(), 64) != options.lowerGiantSha256 ||
        std::string(hashes.data() + 64, 64) !=
          options.lowerGiantSourceSha256 ||
        std::string(hashes.data() + 128, 64) !=
          options.lowerGiantModelSha256 ||
        std::string(semanticHashes.data(), 64) != options.modelSha256 ||
        std::string(semanticHashes.data() + 64, 64) !=
          options.observationSha256)
        throw std::runtime_error(
          "Giant transition marker dependency residual");
    std::size_t component = 0;
    for (const char* suffix : {".header", ".meta", ".strata", ".index",
                               ".blocks"})
        if (std::string(components[component++].data(), 64) !=
            GhostPublicExtraExact::sha256_file(options.prefix + suffix))
            throw std::runtime_error(
              "Giant transition marker component SHA residual");
    GiantCompileCertificate certificate{counts[0], counts[1], counts[2],
      counts[3], counts[4], counts[5], counts[6], counts[7]};
    if (certificate.geometries != header.geometryCount ||
        certificate.edges != header.edges ||
        certificate.worlds != certificate.geometries * 74 ||
        certificate.footprintRejects != certificate.geometries * 6 ||
        !certificate.actionChecks ||
        certificate.actionChecks != certificate.observationChecks ||
        certificate.actionChecks != certificate.childChecks)
        throw std::runtime_error("Giant transition marker count residual");
    return certificate;
}

void certify_giant_transitions(const TransitionOptions& options,
                                const LowerGiantTable& lower,
                                const GiantGeometryDomain& domain) {
    TransitionOptions regenerated = options;
    regenerated.prefix = options.prefix + ".regenerated";
    const GiantCompileCertificate certificate = compile_giant_raw(
      regenerated, lower, domain);
    for (const char* suffix : {".header", ".meta", ".strata", ".index",
                               ".blocks"}) {
        const std::string stored = options.prefix + suffix;
        const std::string rebuilt = regenerated.prefix + suffix;
        if (GhostPublicExtraExact::sha256_file(stored) !=
            GhostPublicExtraExact::sha256_file(rebuilt))
            throw std::runtime_error(
              std::string("Giant exhaustive regeneration residual ") +
              suffix);
        std::filesystem::remove(rebuilt);
    }
    write_giant_marker(options, certificate);
    (void)authenticate_giant_marker(options);
}

}  // namespace

void compile_transitions(const TransitionOptions& options) {
    if (options.prefix.empty() || !options.geometryCount)
        throw std::invalid_argument("Giant/Ghost transition range is empty");
    if (!valid_sha(options.modelSha256) ||
        !valid_sha(options.observationSha256))
        throw std::invalid_argument(
          "Giant transition model/observation binding is invalid");
    const LowerGiantTable lower = authenticate_lower_giant(options);
    const GiantGeometryDomain domain;
    (void)compile_giant_raw(options, lower, domain);
    certify_giant_transitions(options, lower, domain);
}

void merge_transitions(const TransitionOptions& output,
                       const std::vector<std::string>& shards,
                       std::uint32_t expectedGeometries) {
    (void)authenticate_lower_giant(output);
    GiantCompileCertificate aggregate;
    for (const std::string& shard : shards) {
        TransitionOptions input = output;
        input.prefix = shard;
        const GiantCompileCertificate certificate =
          authenticate_giant_marker(input);
        aggregate.geometries += certificate.geometries;
        aggregate.worlds += certificate.worlds;
        aggregate.edges += certificate.edges;
        aggregate.actionChecks += certificate.actionChecks;
        aggregate.decisionChecks += certificate.decisionChecks;
        aggregate.observationChecks += certificate.observationChecks;
        aggregate.childChecks += certificate.childChecks;
        aggregate.footprintRejects += certificate.footprintRejects;
    }
    if (aggregate.geometries != expectedGeometries)
        throw std::runtime_error(
          "Giant shard certificate geometry conservation residual");
    merge_external_transition_shards(output.prefix, shards,
      normalized_material(output.orientation), expectedGeometries);
    // Each input marker is emitted only after exhaustive native+D2
    // regeneration. The generic merger independently proves exact gap-free
    // coverage and rebases every geometry/stratum/block offset. Summing those
    // authenticated per-range counts therefore certifies the concatenated
    // database without a redundant single-core full-domain rebuild.
    write_giant_marker(output, aggregate);
    (void)authenticate_giant_marker(output);
}

void verify_transitions(const TransitionOptions& options) {
    const LowerGiantTable lower = authenticate_lower_giant(options);
    (void)authenticate_giant_marker(options);
    std::ifstream headerFile(options.prefix + ".header", std::ios::binary);
    ExternalTransitionHeader header;
    headerFile.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!headerFile || header.magic !=
          std::array<char, 8>{{'U','F','G','X','1','\0','\0','\0'}} ||
        header.material != static_cast<std::uint32_t>(
          normalized_material(options.orientation).ghostColor) ||
        !header.geometryCount)
        throw std::runtime_error("Giant transition header is incompatible");
    TransitionOptions stored = options;
    stored.geometryBegin = header.reserved;
    stored.geometryCount = header.geometryCount;
    const GiantGeometryDomain domain;
    certify_giant_transitions(stored, lower, domain);
}

ResourceEstimate resource_estimate() { return {}; }

std::uint32_t decision_witness_geometry(Orientation orientation) {
    const GiantGeometryDomain domain;
    const Color observer = normalized_material(orientation).observer();
    for (std::uint32_t geometry = 0; geometry < domain.size(); ++geometry)
        if (!domain[geometry].visible &&
            static_cast<Color>(domain[geometry].side) == observer)
            return geometry;
    throw std::runtime_error("cannot construct a Giant decision witness");
}

std::uint32_t lower_capture_witness_geometry(Orientation orientation) {
    const GiantGeometryDomain domain;
    GhostGiant::NormalizedState state;
    state.giant = 36;  // e5, far from either capture lane.
    state.ghostVisible = true;
    if (orientation == Orientation::Same) {
        state.side = Color::Black;
        state.whiteKing = 0;  // a1
        state.blackKing = 2;  // c1
        state.ghost = 3;      // d1, capturable by the Black King.
    }
    else {
        state.side = Color::White;
        state.whiteKing = 0;   // a1
        state.blackKing = 79;  // h10
        state.ghost = 1;       // b1, capturable by the White King.
    }
    Position position = GhostGiant::make_position(
      state, model_orientation(orientation));
    for (const Move& move : position.legal_moves()) {
        Position child = position;
        Undo undo;
        if (child.make_move(move, undo) && giant_kernel_lower_child(child)) {
            const GhostGiant::PublicGeometry raw{
              static_cast<std::uint8_t>(state.side), state.whiteKing,
              state.blackKing, state.giant, 1};
            return domain.locate(raw).first;
        }
    }
    throw std::runtime_error(
      "cannot construct a legal Giant/Ghost capture witness");
}

[[nodiscard]] std::uint64_t verify_giant_singletons(
  ExternalGhostExtraFixedPoint& solver, const OriginalTable& original,
  Orientation orientation, const GiantGeometryDomain& domain) {
    const MaterialSpec material = normalized_material(orientation);
    std::uint64_t checked = 0;
    std::uint64_t residual = 0;
    for (std::uint32_t index = 0; index < StateCount; ++index) {
        const GhostGiant::SourceState source =
          GhostGiant::decode_source(index);
        const GhostGiant::NormalizedState state = GhostGiant::normalize(
          source, model_orientation(orientation));
        if (!GhostGiant::valid_physical_geometry(state))
            continue;
        const GhostGiant::PublicGeometry raw{
          static_cast<std::uint8_t>(state.side), state.whiteKing,
          state.blackKing, state.giant,
          static_cast<std::uint8_t>(state.ghostVisible)};
        const auto [geometry, transform] = domain.locate(raw);
        const unsigned actual = GhostGiant::transform_square(
          state.ghost, transform);
        const ExternalGeometryMeta& meta = solver.database_.meta(geometry);
        bool owner = false;
        bool observer = false;
        if (external_mask_test(meta.terminal, actual)) {
            owner = external_mask_test(meta.terminalOwner, actual);
            observer = external_mask_test(meta.terminalObserver, actual);
        }
        else if (external_mask_test(meta.live, actual)) {
            const std::uint64_t root =
              std::uint64_t(geometry) * Squares + actual;
            if (state.ghostVisible) {
                owner = solver.visibleOwnerCurrent_[root];
                observer = solver.visibleObserverCurrent_[root];
            }
            else {
                const std::uint32_t stratum = meta.actualStratum[actual];
                if (stratum == NoIndex) {
                    ++residual;
                    continue;
                }
                const ExternalMask singleton =
                  external_singleton_mask(actual);
                owner = solver.bdd_->evaluate(solver.ownerCurrent_[root],
                  singleton.low, singleton.high);
                observer = solver.bdd_->evaluate(
                  solver.observerCurrent_[stratum], singleton.low,
                  singleton.high);
            }
        }
        else {
            ++residual;
            continue;
        }
        // A live singleton belief is not a perfect-information subgame:
        // after an unseen Ghost move the observer's successor belief can
        // contain several worlds.  Therefore its information-game value need
        // not equal the concrete source WDL.  Terminal worlds remain exact,
        // while Bellman equality and the serialized-sidecar reproduction
        // certificate prove every live singleton without this false oracle.
        if (external_mask_test(meta.terminal, actual)) {
            const std::uint8_t exact = original.result(index);
            const bool ownerExpected =
              (state.side == material.ghostColor && exact == 1) ||
              (state.side == material.observer() && exact == 2);
            const bool observerExpected =
              (state.side == material.observer() && exact == 1) ||
              (state.side == material.ghostColor && exact == 2);
            residual += owner != ownerExpected ||
                        observer != observerExpected;
        }
        else
            residual += owner && observer;
        ++checked;
    }
    constexpr std::uint64_t ExpectedValidStates =
      359'100ULL * 74 * 2;
    residual += checked != ExpectedValidStates;
    std::cout << "ghost_giant_singleton_certificate checked " << checked
              << " expected " << ExpectedValidStates
              << " residual " << residual << '\n' << std::flush;
    return residual;
}

[[nodiscard]] bool run_giant_fixed_point(
  ExternalGhostExtraFixedPoint& solver, const OriginalTable& original,
  Orientation orientation, const GiantGeometryDomain& domain) {
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
            const ExternalGeometryMeta& meta = solver.database_.meta(geometry);
            for (unsigned actual = 0; actual < Squares; ++actual) {
                const std::uint64_t root =
                  std::uint64_t(geometry) * Squares + actual;
                const ExternalRobdd::Id oldOwner = solver.ownerCurrent_[root];
                const ExternalRobdd::Id newOwner = solver.ownerNext_[root];
                if (!solver.bdd_->implies(oldOwner, newOwner))
                    throw std::runtime_error(
                      "Giant owner least fixed point regressed");
                changedOwner += oldOwner != newOwner;
                if ((solver.visibleOwnerCurrent_[root] &&
                     !solver.visibleOwnerNext_[root]) ||
                    (solver.visibleObserverCurrent_[root] &&
                     !solver.visibleObserverNext_[root]))
                    throw std::runtime_error(
                      "Giant visible least fixed point regressed");
                changedVisible += solver.visibleOwnerCurrent_[root] !=
                                  solver.visibleOwnerNext_[root];
                changedVisible += solver.visibleObserverCurrent_[root] !=
                                  solver.visibleObserverNext_[root];
            }
            for (std::uint32_t local = 0; local < meta.stratumCount; ++local) {
                const std::uint32_t stratum = meta.stratumBase + local;
                const ExternalRobdd::Id oldObserver =
                  solver.observerCurrent_[stratum];
                const ExternalRobdd::Id newObserver =
                  solver.observerNext_[stratum];
                if (!solver.bdd_->implies(oldObserver, newObserver))
                    throw std::runtime_error(
                      "Giant observer least fixed point regressed");
                changedObserver += oldObserver != newObserver;
            }
        }
        solver.swap_force_arrays();
        std::cout << "ghost_giant_iteration " << solver.iteration_
                  << " bdd_nodes " << solver.bdd_->node_count()
                  << " changed_owner " << changedOwner
                  << " changed_observer " << changedObserver
                  << " changed_visible " << changedVisible
                  << " peak_rss_bytes "
                  << peak_rss_bytes()
                  << " elapsed " << std::chrono::duration<double>(
                       std::chrono::steady_clock::now() - started).count()
                  << "s\n" << std::flush;
        if (solver.options_.measureIterations &&
            solver.iteration_ >= solver.options_.measureIterations) {
            std::cout << "ghost_giant_measurement iterations "
                      << solver.iteration_ << " bdd_nodes "
                      << solver.bdd_->node_count() << " peak_rss_bytes "
                      << peak_rss_bytes()
                      << " proof_complete 0 overlay_written 0\n"
                      << std::flush;
            return false;
        }
        if (!changedOwner && !changedObserver && !changedVisible)
            break;
        if (solver.options_.compactEvery &&
            solver.iteration_ % solver.options_.compactEvery == 0)
            solver.compact();
    }

    // Recompute every exact Bellman root once without swapping. ROBDD identity
    // proves equality over the complete powerset, not a sampled belief set.
    for (std::uint32_t geometry = 0;
         geometry < solver.database_.geometry_count(); ++geometry) {
        solver.bdd_->clear_computed_caches();
        solver.bellman_geometry(geometry, build_external_solver_block(
          geometry, solver.database_, solver.lower_, solver.domain_,
          solver.material_));
    }
    std::uint64_t bellmanResidual = 0;
    std::uint64_t monotonicityResidual = 0;
    for (std::uint64_t root = 0; root < solver.ownerCurrent_.size(); ++root) {
        bellmanResidual += solver.ownerCurrent_[root] !=
                           solver.ownerNext_[root];
        bellmanResidual += solver.visibleOwnerCurrent_[root] !=
                           solver.visibleOwnerNext_[root];
        bellmanResidual += solver.visibleObserverCurrent_[root] !=
                           solver.visibleObserverNext_[root];
        const std::uint32_t geometry = static_cast<std::uint32_t>(
          root / Squares);
        const unsigned actual = static_cast<unsigned>(root % Squares);
        const std::uint32_t stratum =
          solver.database_.meta(geometry).actualStratum[actual];
        if (stratum != NoIndex) {
            const ExternalMask& mask = solver.database_.stratum(stratum);
            monotonicityResidual += !solver.bdd_->is_upward_closed(
              solver.ownerCurrent_[root], mask.low, mask.high);
        }
    }
    for (std::uint64_t stratum = 0;
         stratum < solver.observerCurrent_.size(); ++stratum) {
        bellmanResidual += solver.observerCurrent_[stratum] !=
                           solver.observerNext_[stratum];
        const ExternalMask& mask = solver.database_.stratum(
          static_cast<std::uint32_t>(stratum));
        monotonicityResidual += !solver.bdd_->is_downward_closed(
          solver.observerCurrent_[stratum], mask.low, mask.high);
    }
    const std::uint64_t singletonResidual = verify_giant_singletons(
      solver, original, orientation, domain);
    if (bellmanResidual || monotonicityResidual || singletonResidual)
        throw std::runtime_error(
          "Giant exact symbolic certificate has a residual");
    std::cout << "ghost_giant_symbolic_certificate iterations "
              << solver.iteration_ << " bdd_nodes "
              << solver.bdd_->node_count()
              << " bellman_residual 0 monotonicity_residual 0"
              << " singleton_residual 0 compaction_root_residual 0"
              << " belief_cap none powerset_exact 1\n" << std::flush;
    return true;
}

SolveCertificate solve_exact(const SolveOptions& options) {
    if (options.measureIterations == 0 &&
        (options.outputOverlay.empty() || options.outputArbitrary.empty()))
        throw std::invalid_argument(
          "Giant exact solve requires UFIW2 and UFGI1 outputs");
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
    if (!valid_sha(options.lowerGiantFullSha256) ||
        !valid_sha(options.lowerGiantSourceSha256) ||
        !valid_sha(options.lowerGiantModelSha256))
        throw std::invalid_argument("lower Giant binding is invalid");
    if (GhostPublicExtraExact::sha256_file(options.lowerGhostSidecar) !=
        options.lowerGhostSidecarSha256)
        throw std::runtime_error("lower Ghost UFGM full SHA mismatch");
    const OriginalTable original(options.sourceTable, options.orientation);
    if (original.sha() != options.sourceSha256)
        throw std::runtime_error("Giant/Ghost concrete source SHA mismatch");
    TransitionOptions transitions;
    transitions.orientation = options.orientation;
    transitions.prefix = options.transitionPrefix;
    transitions.lowerGiantTable = options.lowerGiantTable;
    transitions.lowerGiantSha256 = options.lowerGiantFullSha256;
    transitions.lowerGiantSourceSha256 = options.lowerGiantSourceSha256;
    transitions.lowerGiantModelSha256 = options.lowerGiantModelSha256;
    transitions.modelSha256 = options.transitionModelSha256.empty()
      ? options.modelSha256 : options.transitionModelSha256;
    transitions.observationSha256 = options.observationSha256;
    (void)authenticate_lower_giant(transitions);
    const GiantCompileCertificate transitionCertificate =
      authenticate_giant_marker(transitions);
    std::ifstream transitionHeaderFile(
      options.transitionPrefix + ".header", std::ios::binary);
    ExternalTransitionHeader transitionHeader;
    transitionHeaderFile.read(reinterpret_cast<char*>(&transitionHeader),
                              sizeof(transitionHeader));
    if (!transitionHeaderFile ||
        (!options.measureIterations &&
         (transitionHeader.reserved != 0 ||
          transitionCertificate.geometries != 359'100)))
        throw std::runtime_error(
          "Giant solve requires an authenticated complete transition domain");
    const NormalizedSource normalized = normalize_source(original,
      options.orientation, options.scratchPrefix + ".normalized.uftb");

    const MaterialSpec material = normalized_material(options.orientation);
    PackedFourTable concrete(normalized.path, material);
    if (hex_digest(concrete.sha()) != normalized.sha)
        throw std::runtime_error("normalized Giant source SHA mismatch");
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
    const GiantGeometryDomain giantDomain;
    giantDomain.install_legacy_view(domain);
    MaterialSpec constructor;
    constructor.ghostColor = Color::White;
    ExternalGhostExtraFixedPoint solver(database, lower, concrete, domain,
      constructor, legacy);
    solver.material_ = material;
    const bool proofComplete = run_giant_fixed_point(
      solver, original, options.orientation, giantDomain);
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
    overlay_header_self_test();
    GhostGiant::exact_model_self_test();
    std::cout << "ghost_giant_exact_self_test codec_states "
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
          "Giant source-normalization input SHA mismatch");
    const NormalizedSource normalized = normalize_source(original,
      orientation, scratchPrefix + ".normalized.uftb");
    if (normalized.remapResidual)
        throw std::runtime_error("Giant source normalization residual");
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
        probe.lowerGiantFullSha256 = bindings.lowerGiantFullSha256;
        probe.lowerGiantSourceSha256 = bindings.lowerGiantSourceSha256;
        probe.lowerGiantModelSha256 = bindings.lowerGiantModelSha256;
        open_and_validate(probe, &bindings);
    }

    ~Impl() {
        if (data_)
            ::munmap(const_cast<std::uint8_t*>(data_),
                     static_cast<std::size_t>(bytes_));
        if (descriptor_ >= 0) ::close(descriptor_);
    }

    [[nodiscard]] bool query(
      const GhostGiant::SourceState& original,
      GhostPublicExtra::GhostMask belief, bool owner) const {
        const GhostGiant::NormalizedState normalized = GhostGiant::normalize(
          original, model_orientation(orientation_));
        const GhostGiant::PublicGeometry raw{
          static_cast<std::uint8_t>(normalized.side), normalized.whiteKing,
          normalized.blackKing, normalized.giant,
          static_cast<std::uint8_t>(normalized.ghostVisible)};
        const auto canonical = GhostGiant::canonical_geometry(raw);
        GhostPublicExtra::GhostMask mapped;
        for (unsigned square = 0; square < Squares; ++square)
            if (belief.test(square))
                mapped.set(GhostGiant::transform_square(
                  static_cast<std::uint8_t>(square), canonical.transform));
        const unsigned actual = GhostGiant::transform_square(
          normalized.ghost, canonical.transform);
        const auto [geometry, residual] = domain_.locate(canonical.geometry);
        if (residual != GhostGiant::RectangleTransform::Identity)
            throw std::runtime_error(
              "canonical Giant query retained a residual transform");
        if (geometry >= header_.geometries)
            throw std::runtime_error("Giant query geometry residual");
        if (!mapped.count() || !mapped.test(actual))
            throw std::invalid_argument(
              "Giant belief must be nonempty and contain actual");
        if (canonical.geometry.visible && mapped.count() != 1)
            throw std::invalid_argument(
              "visible Giant/Ghost query must be singleton");
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
                      "Giant terminal belief spans observations");
            return owner ? ownerResult : observerResult;
        }
        if (!external_mask_test(meta.live, actual))
            throw std::invalid_argument("Giant query actual is not live");
        if (canonical.geometry.visible)
            return (owner ? visible_owner() :
                            visible_observer())[ownerIndex] != 0;
        const std::uint32_t stratum = meta.actualStratum[actual];
        if (stratum == NoIndex || stratum >= header_.strata)
            throw std::runtime_error("Giant query has no decision cell");
        const ExternalMask& allowed = strata()[stratum];
        if ((mapped.low & ~allowed.low) ||
            (mapped.high & ~allowed.high))
            throw std::invalid_argument(
              "Giant belief spans legal-dot decision cells");
        return evaluate(owner ? owner_roots()[ownerIndex] :
                                observer_roots()[stratum], mapped);
    }

  private:
    template<typename Value>
    [[nodiscard]] const Value* at(std::uint64_t offset,
                                  std::uint64_t count) const {
        if (offset > bytes_ || count > (bytes_ - offset) / sizeof(Value))
            throw std::runtime_error("Giant sidecar section overflow");
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
                throw std::runtime_error("Giant sidecar root out of range");
            const NodeDisk& node = nodes()[root];
            root = belief.test(node.variable) ? node.high : node.low;
        }
        return root == ExternalRobdd::True;
    }

    void open_and_validate(const ProbeBindings& bindings,
                           const SolveOptions* restore) {
        descriptor_ = ::open(path_.c_str(), O_RDONLY);
        if (descriptor_ < 0)
            throw std::runtime_error("cannot open Giant sidecar");
        struct stat status{};
        if (::fstat(descriptor_, &status) || status.st_size <= 0)
            throw std::runtime_error("cannot stat Giant sidecar");
        bytes_ = static_cast<std::uint64_t>(status.st_size);
        data_ = static_cast<const std::uint8_t*>(::mmap(nullptr,
          static_cast<std::size_t>(bytes_), PROT_READ, MAP_PRIVATE,
          descriptor_, 0));
        if (data_ == MAP_FAILED) {
            data_ = nullptr;
            throw std::runtime_error("cannot mmap Giant sidecar");
        }
        if (bytes_ < sizeof(header_))
            throw std::runtime_error("truncated Giant sidecar");
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
              std::array<char, 8>{{'U','F','G','I','1','\0','\0','\0'}} ||
            header_.version != 1 || header_.headerBytes != sizeof(header_) ||
            header_.endian != Endian ||
            header_.primary != static_cast<std::uint32_t>(PieceType::Ghost) ||
            header_.secondary !=
              static_cast<std::uint32_t>(PieceType::Giant) ||
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
            header_.geometries != 359'100 ||
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
            std::string(header_.lowerGiantFullSha.data(), 64) !=
              bindings.lowerGiantFullSha256 ||
            std::string(header_.lowerGiantSourceSha.data(), 64) !=
              bindings.lowerGiantSourceSha256 ||
            std::string(header_.lowerGiantModelSha.data(), 64) !=
              bindings.lowerGiantModelSha256 ||
            std::string(header_.semantics.data(),
                        std::strlen(SidecarSemantics)) != SidecarSemantics ||
            GhostPublicExtraExact::sha256_file(path_, sizeof(header_)) !=
              std::string(header_.payloadSha.data(), 64))
            throw std::runtime_error(
              "Giant arbitrary sidecar header/binding residual");
        if (!valid_sha(header_hash(header_.sourceSha)) ||
            !valid_sha(header_hash(header_.normalizedSourceSha)) ||
            !valid_sha(header_hash(header_.modelSha)) ||
            !valid_sha(header_hash(header_.observationSha)) ||
            !valid_sha(header_hash(header_.lowerGhostSha)) ||
            !valid_sha(header_hash(header_.lowerGiantFullSha)) ||
            !valid_sha(header_hash(header_.lowerGiantSourceSha)) ||
            !valid_sha(header_hash(header_.lowerGiantModelSha)) ||
            !valid_sha(header_hash(header_.transitionPayloadSha)) ||
            !valid_sha(header_hash(header_.payloadSha)) ||
            std::any_of(header_.transitionSha.begin(),
                        header_.transitionSha.end(),
              [&](const auto& digest) {
                  return !valid_sha(header_hash(digest));
              }))
            throw std::runtime_error("Giant sidecar has invalid SHA fields");
        const std::string fullSha = GhostPublicExtraExact::sha256_file(path_);
        if (!restore && !valid_sha(bindings.arbitrarySidecarSha256))
            throw std::invalid_argument(
              "probe-only Giant load requires full UFGI1 SHA-256");
        if (!bindings.arbitrarySidecarSha256.empty() &&
            bindings.arbitrarySidecarSha256 != fullSha)
            throw std::runtime_error("Giant sidecar full SHA mismatch");
        if (restore) {
            if (GhostPublicExtraExact::sha256_file(restore->sourceTable) !=
                  bindings.sourceSha256 ||
                GhostPublicExtraExact::sha256_file(
                  restore->lowerGhostSidecar) !=
                  bindings.lowerGhostSidecarSha256)
                throw std::runtime_error(
                  "Giant strict-restore dependency mismatch");
            if (GhostPublicExtraExact::sha256_file(
                  restore->lowerGiantTable) !=
                  bindings.lowerGiantFullSha256 ||
                bindings.lowerGiantFullSha256 !=
                  bindings.lowerGiantSourceSha256 ||
                restore->lowerGiantModelSha256 !=
                  bindings.lowerGiantModelSha256)
                throw std::runtime_error(
                  "Giant lower-concrete restore mismatch");
            (void)LowerGiantTable(restore->lowerGiantTable);
            const OriginalTable original(restore->sourceTable, orientation_);
            const NormalizedSource normalized = normalize_source(original,
              orientation_, restore->scratchPrefix +
                            ".restore-normalized.uftb");
            if (normalized.sha !=
                std::string(header_.normalizedSourceSha.data(), 64))
                throw std::runtime_error(
                  "Giant normalized-source restore mismatch");
            std::size_t component = 0;
            for (const char* suffix : {".header", ".meta", ".strata",
                                       ".index", ".blocks", ".verified"})
                if (GhostPublicExtraExact::sha256_file(
                      restore->transitionPrefix + suffix) !=
                    std::string(header_.transitionSha[component++].data(), 64))
                    throw std::runtime_error(
                      "Giant transition provenance mismatch");
            if (transition_payload_sha(restore->transitionPrefix) !=
                std::string(header_.transitionPayloadSha.data(), 64))
                throw std::runtime_error(
                  "Giant combined transition provenance mismatch");
        }
        for (std::uint64_t id = 0; id < header_.nodes; ++id) {
            const NodeDisk& node = nodes()[id];
            if (id <= 1) {
                if (node.variable != Squares || node.low != id ||
                    node.high != id)
                    throw std::runtime_error(
                      "Giant sidecar terminal tuple residual");
            }
            else if (node.variable >= Squares || node.low >= id ||
                     node.high >= id || node.low == node.high ||
                     (node.low > 1 && nodes()[node.low].variable <=
                                      node.variable) ||
                     (node.high > 1 && nodes()[node.high].variable <=
                                       node.variable))
                throw std::runtime_error(
                  "Giant sidecar ROBDD structural residual");
        }
        if (restore)
            validate_unique_node_tuples(nodes(), header_.nodes);
        for (std::uint64_t id = 0; id < header_.ownerRoots; ++id)
            if (owner_roots()[id] >= header_.nodes ||
                visible_owner()[id] > 1 || visible_observer()[id] > 1)
                throw std::runtime_error("Giant owner-root residual");
        for (std::uint64_t id = 0; id < header_.strata; ++id)
            if (observer_roots()[id] >= header_.nodes)
                throw std::runtime_error("Giant observer-root residual");
    }

    std::string path_;
    Orientation orientation_;
    int descriptor_ = -1;
    const std::uint8_t* data_ = nullptr;
    std::uint64_t bytes_ = 0;
    SidecarHeader header_{};
    GiantGeometryDomain domain_;
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
  const GhostGiant::SourceState& actual,
  const GhostPublicExtra::GhostMask& belief) const {
    return impl_->query(actual, belief, true);
}

bool ArbitrarySidecarProbe::observer_forces(
  const GhostGiant::SourceState& actual,
  const GhostPublicExtra::GhostMask& belief) const {
    return impl_->query(actual, belief, false);
}

}  // namespace Stockfish::Ultimate::GhostGiantExact
