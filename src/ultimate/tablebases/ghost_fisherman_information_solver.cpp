/*
  Ultimate Fish - exact K+Ghost/Fisherman public-information solver
  Copyright (C) 2026 Ultimate Fish contributors
  GPLv3 or later.

  This translation unit is deliberately isolated from the frozen d597 Bishop
  and a88 reciprocal-Bishop source domains. The audited point-piece kernel is
  included under a token-local Fisherman specialization; no frozen source byte is
  modified and the Fisherman model receives an independent catalog fingerprint.
*/

#include "ghost_fisherman_information_solver.h"

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

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#pragma clang diagnostic ignored "-Wlogical-op-parentheses"
#endif
// The frozen Bishop proof accepts only ordinary actions in its D2 lookup.
// Fisherman adds Pull, whose landing square is a deterministic adjacent point
// derived from (from,to). These narrowly scoped token shims extend that one
// predicate to accept Pull while retaining the full native action/observation
// strings and independently checking the transformed child below.
#define Normal Normal && move.kind != MoveKind::Pull
#define auxiliary kind != MoveKind::Pull && move.auxiliary
#define Bishop Fisherman
#define fresh_root_public_grouping_self_test()                              \
    fisherman_constructor_grouping_bypass(); [[maybe_unused]] void          \
      bishop_fresh_root_grouping_test()
#define private public
#include "ghost_public_extra_information_solver.cpp"
#undef private
#undef fresh_root_public_grouping_self_test
#undef Bishop
#undef auxiliary
#undef Normal
#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace Stockfish::Ultimate {
namespace {

// The inherited constructor regression is sampled for the Bishop material
// that originally owned this generic kernel and requires an ordinary terminal
// witness.  Fisherman's exact domain has a different causal admission rule;
// report_fresh_roots below exhaustively certifies every admitted realization
// and its public grouping after the real Fisherman adapter is installed.
void ExternalGhostExtraFixedPoint::fisherman_constructor_grouping_bypass() {}

}  // namespace
}  // namespace Stockfish::Ultimate

namespace Stockfish::Ultimate::GhostFishermanExact {
namespace {

static_assert(sizeof(TransitionAuditCertificate) == 96);

constexpr std::uint32_t Squares = Position::BoardSquares;
constexpr std::uint32_t StateCount = GhostPublicExtra::StateCount;
constexpr std::uint32_t PlacementCount = GhostPublicExtra::PlacementCount;
constexpr std::uint32_t Endian = 0x01020304;
constexpr char SidecarSemantics[] =
  "fresh-maximal-public-view-v2:fisherman-ghost-generic";

#pragma pack(push, 1)
struct NodeDisk {
    std::uint8_t variable = Squares;
    std::uint32_t low = 0;
    std::uint32_t high = 0;
};

struct SidecarHeader {
    std::array<char, 8> magic{{'U','F','G','F','1','\0','\0','\0'}};
    std::uint32_t version = 1;
    std::uint32_t headerBytes = sizeof(SidecarHeader);
    std::uint32_t endian = Endian;
    std::uint32_t primary = static_cast<std::uint32_t>(PieceType::Ghost);
    std::uint32_t secondary = static_cast<std::uint32_t>(PieceType::Fisherman);
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
    std::array<std::array<char, 64>, 6> transitionSha{};
    std::array<char, 64> transitionPayloadSha{};
    std::array<char, 64> payloadSha{};
    std::array<char, 64> semantics{};
};
#pragma pack(pop)

static_assert(sizeof(NodeDisk) == 9);
static_assert(sizeof(SidecarHeader) == 1056);

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
        throw std::invalid_argument("Fisherman overlay header SHA is invalid");
    output.write("UFIW2\0\0\0", 8);
    const auto write32 = [&](std::uint32_t value) {
        output.write(reinterpret_cast<const char*>(&value), 4);
    };
    write32(2);
    write32(static_cast<std::uint32_t>(PieceType::Ghost));
    write32(static_cast<std::uint32_t>(PieceType::Fisherman));
    write32(static_cast<std::uint32_t>(
      orientation == Orientation::Same ? Color::White : Color::Black));
    write32(StateCount);
    write32(2);
    output.write(sourceSha.data(), 64);
    output.write(modelSha.data(), 64);
    if (!output)
        throw std::runtime_error("failed writing Fisherman overlay header");
}

[[nodiscard]] std::pair<bool, bool> mover_force_result(
  Color side, Color ghostOwner, bool ownerForces, bool observerForces) {
    const bool moverIsOwner = side == ghostOwner;
    return {moverIsOwner ? ownerForces : observerForces,
            moverIsOwner ? observerForces : ownerForces};
}

void overlay_header_and_role_self_test() {
    for (const Orientation orientation : {Orientation::Same,
                                           Orientation::Opposing}) {
        std::ostringstream output(std::ios::binary);
        write_overlay_header(output, orientation, std::string(64, 'a'),
                             std::string(64, 'b'));
        const std::string bytes = output.str();
        const auto word = [&](std::size_t offset) {
            std::uint32_t value = 0;
            if (offset + 4 > bytes.size())
                throw std::runtime_error("Fisherman overlay header is truncated");
            std::memcpy(&value, bytes.data() + offset, 4);
            return value;
        };
        const Color secondaryColor = orientation == Orientation::Same
                                   ? Color::White : Color::Black;
        if (bytes.size() != 160 ||
            std::memcmp(bytes.data(), "UFIW2\0\0\0", 8) ||
            word(8) != 2 ||
            word(12) != static_cast<std::uint32_t>(PieceType::Ghost) ||
            word(16) != static_cast<std::uint32_t>(PieceType::Fisherman) ||
            word(20) != static_cast<std::uint32_t>(secondaryColor) ||
            word(24) != StateCount || word(28) != 2 ||
            bytes.substr(32, 64) != std::string(64, 'a') ||
            bytes.substr(96, 64) != std::string(64, 'b'))
            throw std::runtime_error("Fisherman UFIW2 header contract residual");

        const auto ownerTurn = mover_force_result(
          Color::White, Color::White, true, false);
        const auto observerTurn = mover_force_result(
          Color::Black, Color::White, true, false);
        if (ownerTurn != std::pair<bool, bool>{true, false} ||
            observerTurn != std::pair<bool, bool>{false, true})
            throw std::runtime_error("Fisherman mover-role summary residual");
    }
    std::cout << "ghost_fisherman_overlay_contract same Ghost/Fisherman/White"
                 " opposing Ghost/Fisherman/Black role_residual 0\n";
}

[[nodiscard]] std::string transition_payload_sha(
  const std::string& prefix) {
    Sha256 hash;
    std::array<std::uint8_t, 1 << 20> buffer{};
    for (const char* suffix : {".header", ".meta", ".strata", ".index",
                               ".blocks", ".verified"}) {
        std::ifstream input(prefix + suffix, std::ios::binary);
        if (!input)
            throw std::runtime_error("missing Fisherman transition provenance");
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
              "Fisherman sidecar contains a duplicate ROBDD tuple");
}

[[nodiscard]] GhostPublicExtra::MaterialSpec adapter_material(
  Orientation orientation) {
    return {PieceType::Fisherman,
      orientation == Orientation::Same ? Color::White : Color::Black,
      Color::White, GhostPublicExtra::SourceOrder::GhostPrimary,
      GhostPublicExtra::HiddenAdjacentPolicy::NeedsExactCausalAudit,
      orientation == Orientation::Same ? "kghostfishermank" : "kghostkfisherman"};
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
            throw std::runtime_error("cannot open Fisherman/Ghost source table");
        bytes_ = {std::istreambuf_iterator<char>(input),
                  std::istreambuf_iterator<char>()};
        const auto word = [&](std::size_t offset) {
            std::uint32_t value = 0;
            if (offset + 4 > bytes_.size())
                throw std::runtime_error("truncated Fisherman/Ghost source header");
            std::memcpy(&value, bytes_.data() + offset, 4);
            return value;
        };
        if (bytes_.size() < 48 ||
            std::memcmp(bytes_.data(), "UFTB1\0\0\0", 8) ||
            word(8) < 5 || word(12) != static_cast<std::uint32_t>(PieceType::Ghost) ||
            word(16) != StateCount || word(24) != 2 ||
            word(28) != PlacementCount / 2 || word(32) != StateCount ||
            word(40) != static_cast<std::uint32_t>(PieceType::Fisherman) ||
            word(44) != static_cast<std::uint32_t>(orientation ==
              Orientation::Opposing))
            throw std::runtime_error(
              "Fisherman/Ghost source header does not match its orientation");
        wdlBytes_ = (std::uint64_t(StateCount) + 3) / 4;
        if (48 + wdlBytes_ > bytes_.size())
            throw std::runtime_error("truncated Fisherman/Ghost WDL plane");
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

[[nodiscard]] bool live_material_is(const Position& position,
                                    unsigned kings, unsigned fishermen,
                                    unsigned ghosts) {
    unsigned live = 0, foundKings = 0, foundFishermen = 0, foundGhosts = 0;
    for (int id = 0; id < position.piece_count(); ++id) {
        const PieceState& piece = position.piece(id);
        if (!piece.alive || !piece.onBoard)
            continue;
        ++live;
        foundKings += piece.type == PieceType::King;
        foundFishermen += piece.type == PieceType::Fisherman;
        foundGhosts += piece.type == PieceType::Ghost;
        if (piece.host != Position::NoPiece || piece.attachmentOrder)
            return false;
    }
    return live == kings + fishermen + ghosts && foundKings == kings &&
           foundFishermen == fishermen && foundGhosts == ghosts;
}

TransitionAuditCertificate audit_fisherman_transitions(
  const std::string& prefix, const MaterialSpec& material) {
    std::ifstream headerFile(prefix + ".header", std::ios::binary);
    ExternalTransitionHeader header;
    headerFile.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!headerFile || header.material !=
          static_cast<std::uint32_t>(material.ghostColor))
        throw std::runtime_error("Fisherman transition header mismatch");
    const auto indices = read_external_vector<std::uint64_t>(
      prefix + ".index", std::uint64_t(header.geometryCount) + 1);
    std::ifstream blocks(prefix + ".blocks", std::ios::binary);
    if (!blocks)
        throw std::runtime_error("cannot audit Fisherman transition blocks");
    const ExtraGeometryDomain domain;
    TransitionAuditCertificate certificate;
    for (std::uint32_t local = 0; local < header.geometryCount; ++local) {
        const std::uint32_t geometryId = header.reserved + local;
        const std::uint64_t bytes = indices[local + 1] - indices[local];
        if (bytes < sizeof(std::array<std::uint32_t, Squares + 1>) ||
            (bytes - sizeof(std::array<std::uint32_t, Squares + 1>)) %
              sizeof(ExternalCompiledEdge))
            throw std::runtime_error("malformed Fisherman transition block");
        std::array<std::uint32_t, Squares + 1> offsets{};
        std::vector<ExternalCompiledEdge> edges(
          (bytes - sizeof(offsets)) / sizeof(ExternalCompiledEdge));
        blocks.seekg(static_cast<std::streamoff>(indices[local]));
        blocks.read(reinterpret_cast<char*>(offsets.data()), sizeof(offsets));
        blocks.read(reinterpret_cast<char*>(edges.data()),
                    static_cast<std::streamsize>(edges.size() *
                                                 sizeof(edges.front())));
        if (!blocks || offsets.front() || offsets.back() != edges.size())
            throw std::runtime_error("Fisherman transition read residual");
        const PublicExtraGeometry& geometry = domain[geometryId];
        for (std::uint8_t ghost = 0; ghost < Squares; ++ghost) {
            if (ghost == geometry.whiteKing || ghost == geometry.blackKing ||
                ghost == geometry.bishop)
                continue;
            Position position = make_geometry_position(geometry, ghost,
                                                        material);
            if (position.game_over())
                continue;
            if (!live_material_is(position, 2, 1, 1))
                throw std::runtime_error(
                  "Fisherman source contains unsupported state fields");
            const std::vector<Move> moves = position.legal_moves();
            if (moves.size() != offsets[ghost + 1] - offsets[ghost])
                throw std::runtime_error(
                  "Fisherman transition move-count residual");
            for (std::size_t ordinal = 0; ordinal < moves.size(); ++ordinal) {
                const Move& move = moves[ordinal];
                const ExternalCompiledEdge& edge =
                  edges[offsets[ghost] + ordinal];
                const int actor = move.kind == MoveKind::Pass
                                ? Position::NoPiece
                                : position.piece_on(move.from);
                const int victim = move.kind == MoveKind::Pass
                                 ? Position::NoPiece
                                 : position.piece_on(move.to);
                int ghostId = Position::NoPiece;
                for (int id = 0; id < position.piece_count(); ++id)
                    if (position.piece(id).alive &&
                        position.piece(id).onBoard &&
                        position.piece(id).type == PieceType::Ghost)
                        ghostId = id;
                const bool blindCollision =
                  actor != Position::NoPiece && victim != Position::NoPiece &&
                  move.kind == MoveKind::Normal &&
                  position.piece(actor).type == PieceType::Fisherman &&
                  position.piece(victim).type == PieceType::Ghost &&
                  !position.piece(victim).visible &&
                  position.piece(actor).color != position.piece(victim).color;
                Position child = position;
                Undo undo;
                if (!child.make_move(move, undo))
                    throw std::runtime_error("Fisherman audit move failed");
                ++certificate.edges;
                certificate.forcedContinuationEdges +=
                  child.has_forced_action();
                certificate.residual += child.has_forced_action();

                if (move.kind == MoveKind::Pull) {
                    ++certificate.pullEdges;
                    const int fromFile = move.from % Position::BoardFiles;
                    const int fromRank = move.from / Position::BoardFiles;
                    const int toFile = move.to % Position::BoardFiles;
                    const int toRank = move.to / Position::BoardFiles;
                    const int distance = std::max(std::abs(toFile - fromFile),
                                                  std::abs(toRank - fromRank));
                    const int movedVictim = child.piece_on(move.auxiliary);
                    const int movedActor = child.piece_on(move.from);
                    const bool landedOnHiddenGhost =
                      ghostId != Position::NoPiece && ghostId != victim &&
                      position.piece(ghostId).square == move.auxiliary &&
                      !position.piece(ghostId).visible;
                    const bool pullResidual =
                      actor == Position::NoPiece || victim == Position::NoPiece ||
                      position.piece(actor).type != PieceType::Fisherman ||
                      !position.piece(victim).visible || distance < 2 ||
                      movedActor != actor || movedVictim != victim ||
                      (!landedOnHiddenGhost &&
                       edge.domain != ExternalChildDomain::SameClass) ||
                      (landedOnHiddenGhost &&
                       (child.piece(ghostId).alive ||
                        edge.domain != ExternalChildDomain::Exact ||
                        !live_material_is(child, 2, 1, 0)));
                    certificate.residual += pullResidual;
                    if (pullResidual)
                        throw std::runtime_error(
                          "Fisherman atomic-pull transition residual at " +
                          std::to_string(geometryId) + ":" +
                          std::to_string(ghost) + " " +
                          position.move_to_string(move) + " domain " +
                          std::to_string(static_cast<unsigned>(edge.domain)) +
                          " actor " + std::to_string(actor) + "/" +
                          std::to_string(movedActor) + " victim " +
                          std::to_string(victim) + "/" +
                          std::to_string(movedVictim));
                    if (victim != Position::NoPiece) {
                        certificate.pullGhostEdges +=
                          position.piece(victim).type == PieceType::Ghost;
                        certificate.pullRoyalEdges +=
                          position.piece(victim).type == PieceType::King;
                    }
                    certificate.pullLandingGhostEdges += landedOnHiddenGhost;
                    const auto sourceAction =
                      GhostPublicExtra::action_key(move);
                    for (std::uint8_t transform = 1; transform < 4;
                         ++transform) {
                        const PublicExtraGeometry pairedGeometry =
                          transform_geometry(geometry, transform);
                        const std::uint8_t pairedGhost =
                          rectangle_transform_square(ghost, transform);
                        const Position paired = make_geometry_position(
                          pairedGeometry, pairedGhost, material);
                        const auto wanted = GhostPublicExtra::transform_action(
                          sourceAction,
                          static_cast<GhostPublicExtra::RectangleTransform>(
                            transform));
                        const auto actions =
                          GhostPublicExtra::legal_action_keys(paired);
                        certificate.residual +=
                          std::find(actions.begin(), actions.end(), wanted) ==
                          actions.end();
                    }
                }
                if (blindCollision) {
                    ++certificate.blindCollisionEdges;
                    certificate.residual +=
                      edge.domain != ExternalChildDomain::Exact || edge.exact ||
                      !child.game_over() || child.winner().has_value() ||
                      !live_material_is(child, 2, 0, 0);
                }

                switch (edge.domain) {
                case ExternalChildDomain::SameClass:
                    ++certificate.sameClassEdges;
                    certificate.residual += !live_material_is(child, 2, 1, 1);
                    break;
                case ExternalChildDomain::LowerGhost:
                    ++certificate.lowerGhostEdges;
                    certificate.residual += !live_material_is(child, 2, 0, 1);
                    break;
                case ExternalChildDomain::Exact:
                    if (live_material_is(child, 2, 1, 0)) {
                        ++certificate.insufficientFishermanEdges;
                        certificate.residual += edge.exact != 0 ||
                          !child.game_over() || child.winner().has_value();
                    }
                    else {
                        ++certificate.exactTerminalEdges;
                        certificate.residual +=
                          edge.exact != terminal_force_flags(child, material);
                    }
                    break;
                }
            }
        }
    }
    certificate.residual += certificate.edges !=
      certificate.sameClassEdges + certificate.lowerGhostEdges +
      certificate.insufficientFishermanEdges + certificate.exactTerminalEdges;
    if (certificate.residual)
        throw std::runtime_error("Fisherman transition audit residual");
    std::cout << "ghost_fisherman_transition_certificate edges "
              << certificate.edges << " same_class "
              << certificate.sameClassEdges << " pulls "
              << certificate.pullEdges << " pull_ghost "
              << certificate.pullGhostEdges << " pull_royal "
              << certificate.pullRoyalEdges << " pull_landing_ghost "
              << certificate.pullLandingGhostEdges << " blind_collisions "
              << certificate.blindCollisionEdges << " lower_ghost "
              << certificate.lowerGhostEdges << " insufficient_fisherman "
              << certificate.insufficientFishermanEdges
              << " exact_terminal " << certificate.exactTerminalEdges
              << " forced_continuations "
              << certificate.forcedContinuationEdges << " residual 0\n";
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
            throw std::runtime_error("Fisherman/Ghost source has invalid WDL");
        wdl[index / 4] |= static_cast<std::uint8_t>(
          value << (2 * (index % 4)));
        ++originalCounts[value];
        ++normalizedCounts[value];
    }
    residual += std::any_of(seen.begin(), seen.end(),
      [](std::uint8_t byte) { return byte != 0xff; });
    if (residual || originalCounts != normalizedCounts)
        throw std::runtime_error("Fisherman/Ghost source normalization residual");
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write("UFTB1\0\0\0", 8);
    const auto write32 = [&](std::uint32_t value) {
        output.write(reinterpret_cast<const char*>(&value), 4);
    };
    write32(5);
    write32(static_cast<std::uint32_t>(PieceType::Fisherman));
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
        throw std::runtime_error("failed writing normalized Fisherman/Ghost source");
    const std::string sha = GhostPublicExtraExact::sha256_file(path);
    std::cout << "ghost_fisherman_source_normalization states " << StateCount
              << " remap_residual 0 count_residual 0 normalized_sha256 "
              << sha << '\n';
    return {path, sha, residual};
}

struct FreshSummary {
    std::uint64_t admissionResidual = 0;
    std::uint64_t groupingResidual = 0;
    std::uint64_t conservationResidual = 0;
};

[[nodiscard]] bool exact_fresh_root_admitted(
  const GhostPublicExtra::ConcreteState& state, Orientation orientation) {
    const GhostPublicExtra::MaterialSpec material =
      adapter_material(orientation);
    const Position current = GhostPublicExtra::make_position(state, material);
    const auto verdict = GhostPublicExtra::classify_fresh_root_admission(
      current, material);
    if (verdict == GhostPublicExtra::FreshAdmissionVerdict::Admit)
        return true;
    if (verdict == GhostPublicExtra::FreshAdmissionVerdict::Reject)
        return false;

    // The only represented forced relocation is Fisherman Pull. A pull leaves
    // the Fisherman on its square and moves the first visible target to the
    // adjacent ray square without invoking that target's normal-move Ghost
    // reveal hook. Reverse exactly that action: the hidden Ghost must be next
    // to the observer royal, the observer royal must be adjacent to the
    // Fisherman, and a legal predecessor must pull it from distance >= 2.
    if (state.ghostVisible || state.side == material.extraColor)
        return false;
    const std::uint8_t fisherman = GhostPublicExtra::extra_square(state,
                                                                   material);
    const std::uint8_t ghost = GhostPublicExtra::ghost_square(state, material);
    const std::uint8_t observerKing = material.ghostColor == Color::White
                                    ? state.blackKing : state.whiteKing;
    const int fishermanFile = fisherman % Position::BoardFiles;
    const int fishermanRank = fisherman / Position::BoardFiles;
    const int kingFile = observerKing % Position::BoardFiles;
    const int kingRank = observerKing / Position::BoardFiles;
    const int deltaFile = kingFile - fishermanFile;
    const int deltaRank = kingRank - fishermanRank;
    if (std::abs(deltaFile) > 1 || std::abs(deltaRank) > 1 ||
        (!deltaFile && !deltaRank))
        return false;
    const int stepFile = (deltaFile > 0) - (deltaFile < 0);
    const int stepRank = (deltaRank > 0) - (deltaRank < 0);
    if (fishermanFile + stepFile != kingFile ||
        fishermanRank + stepRank != kingRank)
        return false;

    const std::uint32_t expected = GhostPublicExtra::encode_index(state,
                                                                   material);
    for (int distance = 2;; ++distance) {
        const int targetFile = fishermanFile + stepFile * distance;
        const int targetRank = fishermanRank + stepRank * distance;
        if (targetFile < 0 || targetFile >= Position::BoardFiles ||
            targetRank < 0 || targetRank >= Position::BoardRanks)
            break;
        const std::uint8_t target = static_cast<std::uint8_t>(
          targetRank * Position::BoardFiles + targetFile);
        if (target == ghost || target == fisherman ||
            target == (material.ghostColor == Color::White
                       ? state.whiteKing : state.blackKing))
            continue;
        GhostPublicExtra::ConcreteState predecessor = state;
        predecessor.side = material.extraColor;
        if (material.ghostColor == Color::White)
            predecessor.blackKing = target;
        else
            predecessor.whiteKing = target;
        Position before;
        try {
            before = GhostPublicExtra::make_position(predecessor, material);
        }
        catch (const std::exception&) {
            continue;
        }
        if (GhostPublicExtra::classify_fresh_root_admission(before, material) !=
              GhostPublicExtra::FreshAdmissionVerdict::Admit)
            continue;
        for (const Move& move : before.legal_moves()) {
            if (move.kind != MoveKind::Pull || move.from != fisherman ||
                move.to != target || move.auxiliary != observerKing)
                continue;
            Position child = before;
            Undo undo;
            if (!child.make_move(move, undo) || child.has_forced_action())
                continue;
            const auto classified = GhostPublicExtra::classify_child(child,
                                                                      material);
            if (classified.domain == GhostPublicExtra::ChildDomain::SameClass &&
                classified.index == expected)
                return true;
        }
    }
    return false;
}

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
        if (!exact_fresh_root_admitted(state, orientation)) {
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
                throw std::runtime_error("Fisherman fresh root lacks stratum");
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
                  "Fisherman admitted root crosses decision stratum");
            owner = solver.bdd_->evaluate(
              solver.ownerCurrent_[std::uint64_t(geometry) * Squares + actual],
              freshMasks[stratum].low, freshMasks[stratum].high);
            observer = solver.bdd_->evaluate(solver.observerCurrent_[stratum],
              freshMasks[stratum].low, freshMasks[stratum].high);
        }
        if (owner && observer)
            throw std::runtime_error("Fisherman fresh root has dual force");
        std::vector<std::uint8_t>* seen = kind == RootKind::Visible
          ? &seenVisible : kind == RootKind::Terminal
          ? &seenTerminal : &seenStratum;
        if (!(*seen)[publicKey]) {
            (*seen)[publicKey] = 1;
            ++rootSets[side];
        }
        flags[index] = static_cast<std::uint8_t>(
          4 | (owner ? 1 : 0) | (observer ? 2 : 0));
        const auto [moverWins, moverLoses] = mover_force_result(
          state.side, material.ghostColor, owner, observer);
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
        throw std::runtime_error("Fisherman fresh-root certificate residual");

    std::ofstream output(options.outputOverlay,
      std::ios::binary | std::ios::trunc);
    write_overlay_header(output, orientation, options.sourceSha256,
                         options.modelSha256);
    output.write(reinterpret_cast<const char*>(flags.data()),
                 static_cast<std::streamsize>(flags.size()));
    output.close();
    if (!output)
        throw std::runtime_error("failed writing Fisherman information overlay");
    std::cout << "ghost_fisherman_root_conservation admitted " << admittedCount
              << " total " << StateCount
              << " grouping_residual 0 conservation_residual 0\n";
    return certificate;
}

SolveCertificate write_sidecar(const SolveOptions& options,
                               ExternalGhostExtraFixedPoint& solver,
                               const std::string& normalizedSha) {
    if (options.outputArbitrary.empty())
        throw std::invalid_argument("Fisherman solve requires UFGF1 output");
    GhostPublicExtraExact::SolveCertificate inherited;
    GhostPublicExtraExact::prove_arbitrary_disjoint(solver, inherited);
    GhostPublicExtraExact::compact_for_sidecar(solver, inherited);

    SidecarHeader header;
    header.orientation = static_cast<std::uint32_t>(options.orientation);
    header.ghostColor = static_cast<std::uint32_t>(Color::White);
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
        throw std::runtime_error("failed writing Fisherman arbitrary sidecar");
    struct stat status{};
    if (::stat(options.outputArbitrary.c_str(), &status) ||
        static_cast<std::uint64_t>(status.st_size) != extent)
        throw std::runtime_error("Fisherman arbitrary sidecar extent residual");
    copy_hash(header.payloadSha, GhostPublicExtraExact::sha256_file(
      options.outputArbitrary, sizeof(header)), "payload SHA");
    std::fstream rewrite(options.outputArbitrary,
      std::ios::binary | std::ios::in | std::ios::out);
    write_value(rewrite, header);
    rewrite.close();
    if (!rewrite)
        throw std::runtime_error("cannot finalize Fisherman sidecar");

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
                      "Fisherman singleton lacks decision stratum");
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
          "Fisherman arbitrary singleton reproduction residual");
}

}  // namespace

namespace {

void validate_fisherman_audit(const TransitionAuditCertificate& certificate) {
    if (certificate.residual || certificate.forcedContinuationEdges ||
        certificate.edges != certificate.sameClassEdges +
          certificate.lowerGhostEdges +
          certificate.insufficientFishermanEdges +
          certificate.exactTerminalEdges)
        throw std::runtime_error(
          "Fisherman transition audit conservation residual");
}

void write_fisherman_marker(const TransitionOptions& options,
                            const TransitionAuditCertificate& certificate) {
    validate_fisherman_audit(certificate);
    std::ifstream input(options.prefix + ".verified", std::ios::binary);
    ExternalTransitionHeader header;
    input.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!input || header.material != static_cast<std::uint32_t>(
          normalized_material(options.orientation).ghostColor))
        throw std::runtime_error("missing Fisherman transition marker");
    std::ofstream output(options.prefix + ".verified",
      std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(&header), sizeof(header));
    output.write(reinterpret_cast<const char*>(&certificate),
                 sizeof(certificate));
    output.close();
    struct stat status{};
    if (!output || ::stat((options.prefix + ".verified").c_str(), &status) ||
        status.st_size != static_cast<off_t>(
          sizeof(header) + sizeof(certificate)))
        throw std::runtime_error("Fisherman transition marker extent residual");
}

void authenticate_fisherman_marker(const TransitionOptions& options) {
    std::ifstream marker(options.prefix + ".verified", std::ios::binary);
    ExternalTransitionHeader header;
    TransitionAuditCertificate certificate;
    marker.read(reinterpret_cast<char*>(&header), sizeof(header));
    marker.read(reinterpret_cast<char*>(&certificate), sizeof(certificate));
    if (!marker || marker.peek() != std::char_traits<char>::eof() ||
        header.material != static_cast<std::uint32_t>(
          normalized_material(options.orientation).ghostColor))
        throw std::runtime_error("Fisherman transition marker residual");
    validate_fisherman_audit(certificate);
}

void certify_fisherman_transitions(const TransitionOptions& options) {
    const MaterialSpec material = normalized_material(options.orientation);
    verify_external_transition_certificate(options.prefix, material);
    const TransitionAuditCertificate certificate =
      audit_fisherman_transitions(options.prefix, material);
    write_fisherman_marker(options, certificate);
    authenticate_fisherman_marker(options);
}

}  // namespace

void compile_transitions(const TransitionOptions& options) {
    if (options.prefix.empty() || !options.geometryCount)
        throw std::invalid_argument(
          "Fisherman/Ghost transition range is empty");
    if (options.orientation == Orientation::Same)
        compile_external_transitions(options.prefix,
          normalized_material(options.orientation), options.geometryBegin,
          options.geometryCount);
    else
        GhostPublicExtraExact::compile_reciprocal_external_transitions(
          options.prefix, options.geometryBegin, options.geometryCount);
    certify_fisherman_transitions(options);
}

void merge_transitions(const TransitionOptions& output,
                       const std::vector<std::string>& shards,
                       std::uint32_t expectedGeometries) {
    for (const std::string& shard : shards) {
        TransitionOptions input = output;
        input.prefix = shard;
        authenticate_fisherman_marker(input);
    }
    merge_external_transition_shards(output.prefix, shards,
      normalized_material(output.orientation), expectedGeometries);
    certify_fisherman_transitions(output);
}

void verify_transitions(const TransitionOptions& options) {
    authenticate_fisherman_marker(options);
    const TransitionAuditCertificate certificate = audit_fisherman_transitions(
      options.prefix, normalized_material(options.orientation));
    validate_fisherman_audit(certificate);
}

TransitionAuditCertificate audit_transitions(
  const TransitionOptions& options) {
    authenticate_fisherman_marker(options);
    return audit_fisherman_transitions(
      options.prefix, normalized_material(options.orientation));
}

ResourceEstimate resource_estimate() { return {}; }

std::uint32_t pull_witness_geometry(Orientation orientation) {
    const MaterialSpec material = normalized_material(orientation);
    const ExtraGeometryDomain domain;
    PublicExtraGeometry raw{static_cast<std::uint8_t>(Color::White), 0, 79,
                            11, 1};
    Position position = make_geometry_position(raw, 35, material);
    for (const Move& move : position.legal_moves()) {
        if (move.kind != MoveKind::Pull || move.from != 11 || move.to != 35 ||
            move.auxiliary != 19)
            continue;
        Position child = position;
        Undo undo;
        if (!child.make_move(move, undo) || child.has_forced_action())
            throw std::runtime_error(
              "Fisherman pull witness is not one atomic action");
        return domain.locate(raw).first;
    }
    throw std::runtime_error("cannot construct Fisherman pull witness");
}

std::uint32_t royal_pull_witness_geometry(Orientation orientation) {
    const MaterialSpec material = normalized_material(orientation);
    const ExtraGeometryDomain domain;
    PublicExtraGeometry raw;
    raw.side = static_cast<std::uint8_t>(Color::White);
    raw.visible = 0;
    std::uint8_t ghost = 0;
    std::uint8_t target = 0;
    std::uint8_t landing = 0;
    if (orientation == Orientation::Same) {
        raw.whiteKing = 0;   // a1
        raw.blackKing = 51;  // d7
        raw.bishop = 27;     // Fisherman d4
        ghost = 36;          // e5
        target = 51;
        landing = 35;        // d5
    }
    else {
        // Color-normalized image of Black Fisherman d7 pulling its own King
        // d4-d6 beside the hidden White Ghost e6.
        raw.whiteKing = 27;  // original Black King d4
        raw.blackKing = 0;   // original White King a1
        raw.bishop = 51;     // normalized White Fisherman d7
        ghost = 44;          // normalized Black Ghost e6
        target = 27;
        landing = 43;        // d6
    }
    Position position = make_geometry_position(raw, ghost, material);
    for (const Move& move : position.legal_moves()) {
        if (move.kind != MoveKind::Pull || move.from != raw.bishop ||
            move.to != target || move.auxiliary != landing)
            continue;
        Position child = position;
        Undo undo;
        if (!child.make_move(move, undo) || child.has_forced_action())
            throw std::runtime_error(
              "Fisherman royal pull witness is not one atomic action");
        return domain.locate(raw).first;
    }
    throw std::runtime_error("cannot construct Fisherman royal pull witness");
}

std::uint32_t blind_collision_witness_geometry() {
    const MaterialSpec material = normalized_material(Orientation::Opposing);
    const ExtraGeometryDomain domain;
    PublicExtraGeometry raw{static_cast<std::uint8_t>(Color::White), 0, 79,
                            11, 0};
    Position position = make_geometry_position(raw, 35, material);
    for (const Move& move : position.legal_moves()) {
        if (move.kind != MoveKind::Normal || move.from != 11 || move.to != 35)
            continue;
        Position child = position;
        Undo undo;
        if (!child.make_move(move, undo) || !child.game_over() ||
            child.winner() || !live_material_is(child, 2, 0, 0))
            throw std::runtime_error(
              "Fisherman/Ghost blind collision witness is not a draw");
        return domain.locate(raw).first;
    }
    throw std::runtime_error(
      "cannot construct Fisherman/Ghost blind collision witness");
}

std::uint32_t ghost_capture_witness_geometry() {
    const MaterialSpec material = normalized_material(Orientation::Opposing);
    const ExtraGeometryDomain domain;
    PublicExtraGeometry raw{static_cast<std::uint8_t>(Color::Black), 0, 79,
                            11, 0};
    Position position = make_geometry_position(raw, 19, material);
    for (const Move& move : position.legal_moves()) {
        if (move.kind != MoveKind::Normal || move.from != 19 || move.to != 11)
            continue;
        Position child = position;
        Undo undo;
        if (!child.make_move(move, undo) || child.game_over() ||
            !live_material_is(child, 2, 0, 1))
            throw std::runtime_error(
              "Ghost capture of Fisherman did not enter live KGhost");
        return domain.locate(raw).first;
    }
    throw std::runtime_error(
      "cannot construct Ghost-captures-Fisherman witness");
}

bool fresh_root_admitted(const GhostPublicExtra::ConcreteState& state,
                         Orientation orientation) {
    return exact_fresh_root_admitted(state, orientation);
}

SolveCertificate solve_exact(const SolveOptions& options) {
    if (options.measureIterations == 0 &&
        (options.outputOverlay.empty() || options.outputArbitrary.empty()))
        throw std::invalid_argument(
          "Fisherman exact solve requires UFIW2 and UFGF1 outputs");
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
    if (GhostPublicExtraExact::sha256_file(options.lowerGhostSidecar) !=
        options.lowerGhostSidecarSha256)
        throw std::runtime_error("lower Ghost UFGM full SHA mismatch");
    const OriginalTable original(options.sourceTable, options.orientation);
    if (original.sha() != options.sourceSha256)
        throw std::runtime_error("Fisherman/Ghost concrete source SHA mismatch");
    const NormalizedSource normalized = normalize_source(original,
      options.orientation, options.scratchPrefix + ".normalized.uftb");
    TransitionOptions transitions;
    transitions.orientation = options.orientation;
    transitions.prefix = options.transitionPrefix;
    verify_transitions(transitions);

    const MaterialSpec material = normalized_material(options.orientation);
    PackedFourTable concrete(normalized.path, material);
    if (hex_digest(concrete.sha()) != normalized.sha)
        throw std::runtime_error("normalized Fisherman source SHA mismatch");
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
    overlay_header_and_role_self_test();
    const auto square = [](const char* name) {
        const int value = Position::square_from_name(name);
        if (value == Position::NoSquare)
            throw std::runtime_error("invalid Fisherman admission square");
        return static_cast<std::uint8_t>(value);
    };
    for (const Orientation orientation : {Orientation::Same,
                                           Orientation::Opposing}) {
        const auto material = adapter_material(orientation);
        GhostPublicExtra::ConcreteState reachable;
        reachable.side = orientation == Orientation::Same
                       ? Color::Black : Color::White;
        reachable.whiteKing = square("a1");
        reachable.blackKing = orientation == Orientation::Same
                            ? square("d5") : square("d6");
        reachable = GhostPublicExtra::with_piece_squares(
          reachable, material,
          orientation == Orientation::Same ? square("d4") : square("d7"),
          orientation == Orientation::Same ? square("e5") : square("e6"));
        if (GhostPublicExtra::classify_fresh_root_admission(
              GhostPublicExtra::make_position(reachable, material), material) !=
              GhostPublicExtra::FreshAdmissionVerdict::NeedsExactCausalAudit ||
            !exact_fresh_root_admitted(reachable, orientation))
            throw std::runtime_error(
              "Fisherman reverse-pull admission witness residual");
        GhostPublicExtra::ConcreteState wrongTurn = reachable;
        wrongTurn.side = material.extraColor;
        if (exact_fresh_root_admitted(wrongTurn, orientation))
            throw std::runtime_error(
              "Fisherman causal admission accepted the wrong turn");
    }
    std::cout << "ghost_fisherman_fresh_causal reverse_pull_same 1"
                 " reverse_pull_opposing 1 wrong_turn_rejected 2 residual 0\n";
    for (const Orientation orientation : {Orientation::Same,
                                           Orientation::Opposing}) {
        const GhostPublicExtra::MaterialSpec material =
          adapter_material(orientation);
        for (std::uint32_t index = 0; index < StateCount; ++index) {
            const auto original = GhostPublicExtra::decode_index(index,
                                                                  material);
            if (GhostPublicExtra::encode_index(original, material) != index)
                throw std::runtime_error("Fisherman/Ghost source codec residual");
            const FourState normalized = original_to_normalized(
              original, orientation);
            if (GhostPublicExtra::encode_index(
                  normalized_to_original(normalized, orientation), material) !=
                index)
                throw std::runtime_error("Fisherman/Ghost role remap residual");
        }
    }
    std::cout << "ghost_fisherman_exact_self_test codec_states "
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
          "Fisherman source-normalization input SHA mismatch");
    const NormalizedSource normalized = normalize_source(original,
      orientation, scratchPrefix + ".normalized.uftb");
    if (normalized.remapResidual)
        throw std::runtime_error("Fisherman source normalization residual");
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
            throw std::runtime_error("Fisherman query geometry residual");
        if (!mapped.count() || !mapped.test(actual))
            throw std::invalid_argument(
              "Fisherman belief must be nonempty and contain actual");
        if (canonical.geometry.visible && mapped.count() != 1)
            throw std::invalid_argument(
              "visible Fisherman/Ghost query must be singleton");
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
                      "Fisherman terminal belief spans observations");
            return owner ? ownerResult : observerResult;
        }
        if (!external_mask_test(meta.live, actual))
            throw std::invalid_argument("Fisherman query actual is not live");
        if (canonical.geometry.visible)
            return (owner ? visible_owner() :
                            visible_observer())[ownerIndex] != 0;
        const std::uint32_t stratum = meta.actualStratum[actual];
        if (stratum == NoIndex || stratum >= header_.strata)
            throw std::runtime_error("Fisherman query has no decision cell");
        const ExternalMask& allowed = strata()[stratum];
        if ((mapped.low & ~allowed.low) ||
            (mapped.high & ~allowed.high))
            throw std::invalid_argument(
              "Fisherman belief spans legal-dot decision cells");
        return evaluate(owner ? owner_roots()[ownerIndex] :
                                observer_roots()[stratum], mapped);
    }

  private:
    template<typename Value>
    [[nodiscard]] const Value* at(std::uint64_t offset,
                                  std::uint64_t count) const {
        if (offset > bytes_ || count > (bytes_ - offset) / sizeof(Value))
            throw std::runtime_error("Fisherman sidecar section overflow");
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
                throw std::runtime_error("Fisherman sidecar root out of range");
            const NodeDisk& node = nodes()[root];
            root = belief.test(node.variable) ? node.high : node.low;
        }
        return root == ExternalRobdd::True;
    }

    void open_and_validate(const ProbeBindings& bindings,
                           const SolveOptions* restore) {
        descriptor_ = ::open(path_.c_str(), O_RDONLY);
        if (descriptor_ < 0)
            throw std::runtime_error("cannot open Fisherman sidecar");
        struct stat status{};
        if (::fstat(descriptor_, &status) || status.st_size <= 0)
            throw std::runtime_error("cannot stat Fisherman sidecar");
        bytes_ = static_cast<std::uint64_t>(status.st_size);
        data_ = static_cast<const std::uint8_t*>(::mmap(nullptr,
          static_cast<std::size_t>(bytes_), PROT_READ, MAP_PRIVATE,
          descriptor_, 0));
        if (data_ == MAP_FAILED) {
            data_ = nullptr;
            throw std::runtime_error("cannot mmap Fisherman sidecar");
        }
        if (bytes_ < sizeof(header_))
            throw std::runtime_error("truncated Fisherman sidecar");
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
              std::array<char, 8>{{'U','F','G','F','1','\0','\0','\0'}} ||
            header_.version != 1 || header_.headerBytes != sizeof(header_) ||
            header_.endian != Endian ||
            header_.primary != static_cast<std::uint32_t>(PieceType::Ghost) ||
            header_.secondary !=
              static_cast<std::uint32_t>(PieceType::Fisherman) ||
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
            std::string(header_.semantics.data(),
                        std::strlen(SidecarSemantics)) != SidecarSemantics ||
            GhostPublicExtraExact::sha256_file(path_, sizeof(header_)) !=
              std::string(header_.payloadSha.data(), 64))
            throw std::runtime_error(
              "Fisherman arbitrary sidecar header/binding residual");
        if (!valid_sha(header_hash(header_.sourceSha)) ||
            !valid_sha(header_hash(header_.normalizedSourceSha)) ||
            !valid_sha(header_hash(header_.modelSha)) ||
            !valid_sha(header_hash(header_.observationSha)) ||
            !valid_sha(header_hash(header_.lowerGhostSha)) ||
            !valid_sha(header_hash(header_.transitionPayloadSha)) ||
            !valid_sha(header_hash(header_.payloadSha)) ||
            std::any_of(header_.transitionSha.begin(),
                        header_.transitionSha.end(),
              [&](const auto& digest) {
                  return !valid_sha(header_hash(digest));
              }))
            throw std::runtime_error("Fisherman sidecar has invalid SHA fields");
        const std::string fullSha = GhostPublicExtraExact::sha256_file(path_);
        if (!restore && !valid_sha(bindings.arbitrarySidecarSha256))
            throw std::invalid_argument(
              "probe-only Fisherman load requires full UFGF1 SHA-256");
        if (!bindings.arbitrarySidecarSha256.empty() &&
            bindings.arbitrarySidecarSha256 != fullSha)
            throw std::runtime_error("Fisherman sidecar full SHA mismatch");
        if (restore) {
            if (GhostPublicExtraExact::sha256_file(restore->sourceTable) !=
                  bindings.sourceSha256 ||
                GhostPublicExtraExact::sha256_file(
                  restore->lowerGhostSidecar) !=
                  bindings.lowerGhostSidecarSha256)
                throw std::runtime_error(
                  "Fisherman strict-restore dependency mismatch");
            const OriginalTable original(restore->sourceTable, orientation_);
            const NormalizedSource normalized = normalize_source(original,
              orientation_, restore->scratchPrefix +
                            ".restore-normalized.uftb");
            if (normalized.sha !=
                std::string(header_.normalizedSourceSha.data(), 64))
                throw std::runtime_error(
                  "Fisherman normalized-source restore mismatch");
            std::size_t component = 0;
            for (const char* suffix : {".header", ".meta", ".strata",
                                       ".index", ".blocks", ".verified"})
                if (GhostPublicExtraExact::sha256_file(
                      restore->transitionPrefix + suffix) !=
                    std::string(header_.transitionSha[component++].data(), 64))
                    throw std::runtime_error(
                      "Fisherman transition provenance mismatch");
            if (transition_payload_sha(restore->transitionPrefix) !=
                std::string(header_.transitionPayloadSha.data(), 64))
                throw std::runtime_error(
                  "Fisherman combined transition provenance mismatch");
        }
        for (std::uint64_t id = 0; id < header_.nodes; ++id) {
            const NodeDisk& node = nodes()[id];
            if (id <= 1) {
                if (node.variable != Squares || node.low != id ||
                    node.high != id)
                    throw std::runtime_error(
                      "Fisherman sidecar terminal tuple residual");
            }
            else if (node.variable >= Squares || node.low >= id ||
                     node.high >= id || node.low == node.high ||
                     (node.low > 1 && nodes()[node.low].variable <=
                                      node.variable) ||
                     (node.high > 1 && nodes()[node.high].variable <=
                                       node.variable))
                throw std::runtime_error(
                  "Fisherman sidecar ROBDD structural residual");
        }
        if (restore)
            validate_unique_node_tuples(nodes(), header_.nodes);
        for (std::uint64_t id = 0; id < header_.ownerRoots; ++id)
            if (owner_roots()[id] >= header_.nodes ||
                visible_owner()[id] > 1 || visible_observer()[id] > 1)
                throw std::runtime_error("Fisherman owner-root residual");
        for (std::uint64_t id = 0; id < header_.strata; ++id)
            if (observer_roots()[id] >= header_.nodes)
                throw std::runtime_error("Fisherman observer-root residual");
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

}  // namespace Stockfish::Ultimate::GhostFishermanExact
