/*
  Ultimate Fish - exact crossed K+Jester versus K+Ghost information model
  Copyright (C) 2026 Ultimate Fish contributors

  This file is part of Ultimate Fish and is licensed under GPLv3 or later.
*/

#include "crossed_jester_ghost_information_model.h"

#include "information.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace Stockfish::Ultimate::CrossedJesterGhostInformation {
namespace {

constexpr std::uint8_t Squares = Position::BoardSquares;

[[nodiscard]] std::uint8_t horizontal_reflection(std::uint8_t square) {
    return static_cast<std::uint8_t>(
      (square / Position::BoardFiles) * Position::BoardFiles +
      Position::BoardFiles - 1 - square % Position::BoardFiles);
}

[[nodiscard]] std::uint8_t vertical_reflection(std::uint8_t square) {
    return static_cast<std::uint8_t>(
      (Position::BoardRanks - 1 - square / Position::BoardFiles) *
      Position::BoardFiles + square % Position::BoardFiles);
}

void validate_side(Color side) {
    if (side != Color::White && side != Color::Black)
        throw std::invalid_argument("invalid crossed-information side");
}

void validate_square(std::uint8_t square, const char* field) {
    if (square >= Squares)
        throw std::invalid_argument(std::string(field) +
                                    " is outside the Ultimate board");
}

[[nodiscard]] bool distinct(std::initializer_list<std::uint8_t> squares) {
    for (auto first = squares.begin(); first != squares.end(); ++first)
        for (auto second = first + 1; second != squares.end(); ++second)
            if (*first == *second)
                return false;
    return true;
}

[[nodiscard]] std::uint32_t rank_excluding(
  std::uint8_t square, std::initializer_list<std::uint8_t> occupied) {
    std::uint32_t rank = square;
    for (const std::uint8_t used : occupied)
        rank -= used < square;
    return rank;
}

[[nodiscard]] std::uint8_t unrank_excluding(
  std::uint32_t rank, std::initializer_list<std::uint8_t> occupied) {
    for (std::uint8_t square = 0; square < Squares; ++square) {
        bool used = false;
        for (const std::uint8_t item : occupied)
            used = used || item == square;
        if (!used && rank-- == 0)
            return square;
    }
    throw std::runtime_error("crossed-information square rank is invalid");
}

[[nodiscard]] std::uint32_t pair_rank(std::uint32_t first,
                                      std::uint32_t second,
                                      std::uint32_t count) {
    if (first >= second || second >= count)
        throw std::invalid_argument("invalid crossed public royal pair");
    return first * (2 * count - first - 1) / 2 + second - first - 1;
}

[[nodiscard]] std::pair<std::uint32_t, std::uint32_t> pair_unrank(
  std::uint32_t rank, std::uint32_t count) {
    for (std::uint32_t first = 0; first + 1 < count; ++first) {
        const std::uint32_t width = count - first - 1;
        if (rank < width)
            return {first, first + 1 + rank};
        rank -= width;
    }
    throw std::runtime_error("crossed public royal pair index is invalid");
}

void validate_source(const ConcreteState& state) {
    validate_side(state.side);
    validate_square(state.whiteKing, "White King square");
    validate_square(state.blackKing, "Black King square");
    validate_square(state.jester, "Jester square");
    validate_square(state.ghost, "Ghost square");
    if (!distinct(
          {state.whiteKing, state.blackKing, state.jester, state.ghost}))
        throw std::invalid_argument("overlapping crossed source state");
}

[[nodiscard]] ConcreteState horizontal_canonical(ConcreteState state) {
    validate_source(state);
    if (state.whiteKing % Position::BoardFiles >= Position::BoardFiles / 2) {
        state.whiteKing = horizontal_reflection(state.whiteKing);
        state.blackKing = horizontal_reflection(state.blackKing);
        state.jester = horizontal_reflection(state.jester);
        state.ghost = horizontal_reflection(state.ghost);
    }
    return state;
}

void validate_frame(const PublicFrame& frame) {
    validate_side(frame.side);
    validate_square(frame.blackKing, "Black King square");
    validate_square(frame.royalFirst, "first royal silhouette");
    validate_square(frame.royalSecond, "second royal silhouette");
    if (frame.visibleGhost)
        validate_square(*frame.visibleGhost, "visible Ghost square");
    if (frame.royalFirst >= frame.royalSecond ||
        !distinct({frame.blackKing, frame.royalFirst, frame.royalSecond}) ||
        (frame.visibleGhost &&
         !distinct({frame.blackKing, frame.royalFirst, frame.royalSecond,
                    *frame.visibleGhost})))
        throw std::invalid_argument("invalid crossed public frame");
}

void validate_world(const PublicFrame& frame, const ProductWorld& world) {
    validate_frame(frame);
    validate_square(world.ghost, "product Ghost square");
    if (!distinct({frame.blackKing, frame.royalFirst, frame.royalSecond,
                   world.ghost}) ||
        (frame.visibleGhost && *frame.visibleGhost != world.ghost))
        throw std::invalid_argument("invalid crossed product world");
}

void validate_transform(RectangleTransform transform) {
    if (static_cast<std::uint8_t>(transform) >
        static_cast<std::uint8_t>(RectangleTransform::Both))
        throw std::invalid_argument("invalid Ultimate rectangle transform");
}

[[nodiscard]] std::tuple<unsigned, unsigned, unsigned, unsigned, unsigned,
                         unsigned>
frame_key(const PublicFrame& frame) {
    return {static_cast<unsigned>(frame.side), frame.blackKing,
      frame.royalFirst, frame.royalSecond,
      frame.visibleGhost ? 1u : 0u,
      frame.visibleGhost ? *frame.visibleGhost : Squares};
}

[[nodiscard]] bool action_auxiliary_is_square(const ActionKey& action) {
    if (action.kind == MoveKind::Pull)
        return true;
    if (action.kind == MoveKind::Normal && action.auxiliary != 0)
        throw std::invalid_argument(
          "nonzero ordinary auxiliary needs a material action adapter");
    return false;
}

[[nodiscard]] std::optional<Move> locate_action(
  const Position& position, const ActionKey& action) {
    std::optional<Move> result;
    for (const Move& move : position.legal_moves())
        if (action_key(move) == action) {
            if (result)
                throw std::runtime_error(
                  "one complete action maps to multiple legal moves");
            result = move;
        }
    return result;
}

struct LiveMaterial {
    int whiteKing = Position::NoSquare;
    int blackKing = Position::NoSquare;
    int jester = Position::NoSquare;
    int ghost = Position::NoSquare;
    bool ghostVisible = false;
    int live = 0;
    bool unexpected = false;
};

[[nodiscard]] LiveMaterial scan_live(const Position& position) {
    LiveMaterial result;
    for (int id = 0; id < position.piece_count(); ++id) {
        const PieceState& piece = position.piece(id);
        if (!piece.alive || !piece.onBoard)
            continue;
        ++result.live;
        if (piece.type == PieceType::King && piece.color == Color::White &&
            result.whiteKing == Position::NoSquare)
            result.whiteKing = piece.square;
        else if (piece.type == PieceType::King &&
                 piece.color == Color::Black &&
                 result.blackKing == Position::NoSquare)
            result.blackKing = piece.square;
        else if (piece.type == PieceType::Jester &&
                 piece.color == Color::White &&
                 result.jester == Position::NoSquare)
            result.jester = piece.square;
        else if (piece.type == PieceType::Ghost &&
                 piece.color == Color::Black &&
                 result.ghost == Position::NoSquare) {
            result.ghost = piece.square;
            result.ghostVisible = piece.visible;
        }
        else
            result.unexpected = true;
    }
    return result;
}

[[nodiscard]] bool adjacent(std::uint8_t first, std::uint8_t second) {
    return std::abs(int(first % Position::BoardFiles) -
                    int(second % Position::BoardFiles)) <= 1 &&
           std::abs(int(first / Position::BoardFiles) -
                    int(second / Position::BoardFiles)) <= 1;
}

void normalize_partition(KnowledgePartition& partition) {
    for (KnowledgeCell& cell : partition.cells) {
        std::sort(cell.begin(), cell.end());
        if (cell.empty() ||
            std::adjacent_find(cell.begin(), cell.end()) != cell.end())
            throw std::invalid_argument(
              "knowledge partition has an empty or duplicate cell");
    }
    std::sort(partition.cells.begin(), partition.cells.end());
}

[[nodiscard]] KnowledgePartition build_owned_partition(
  const std::vector<HistoryAtom>& atoms, Color player) {
    std::map<unsigned, KnowledgeCell> grouped;
    for (std::uint32_t atom = 0; atom < atoms.size(); ++atom) {
        const ProductWorld& world = atoms[atom].world;
        const unsigned key = player == Color::White
                           ? unsigned(world.kingAtFirst) : world.ghost;
        grouped[key].push_back(atom);
    }
    KnowledgePartition result;
    for (auto& [key, cell] : grouped) {
        (void)key;
        result.cells.push_back(std::move(cell));
    }
    normalize_partition(result);
    return result;
}

[[nodiscard]] WorldMask projected_worlds(
  const PublicFrame& frame, const std::vector<HistoryAtom>& atoms) {
    WorldMask result;
    for (const HistoryAtom& atom : atoms)
        result.set(world_variable(frame, atom.world));
    return result;
}

[[nodiscard]] std::string common_position_key(const Position& position) {
    const LiveMaterial live = scan_live(position);
    std::ostringstream out;
    out << "UFCROSSVIEW1|turn=" << int(position.side_to_move())
        << "|terminal=" << int(position.game_over()) << "|winner=";
    if (const std::optional<Color> winner = position.winner())
        out << int(*winner);
    else
        out << '-';
    out << "|blackKing=";
    if (live.blackKing != Position::NoSquare)
        out << Position::square_name(live.blackKing);
    else
        out << '-';
    if (live.whiteKing != Position::NoSquare &&
        live.jester != Position::NoSquare) {
        const int first = std::min(live.whiteKing, live.jester);
        const int second = std::max(live.whiteKing, live.jester);
        out << "|royals=" << Position::square_name(first) << ','
            << Position::square_name(second);
    }
    else if (live.whiteKing != Position::NoSquare)
        out << "|whiteKing=" << Position::square_name(live.whiteKing);
    else if (live.jester != Position::NoSquare)
        out << "|whiteJester=" << Position::square_name(live.jester);
    else
        out << "|whiteRoyal=-";
    out << "|ghost=";
    if (live.ghost == Position::NoSquare)
        out << '-';
    else if (!live.ghostVisible)
        out << "hidden";
    else
        out << Position::square_name(live.ghost);
    return out.str();
}

[[nodiscard]] std::string common_transition_key(
  const Position& before, const Move& move, const Position& after) {
    if (move.kind == MoveKind::Pass)
        return "UFCROSSTRANS1|actor=none|kind=pass|from=-|to=-|view=" +
               common_position_key(after);
    const int actor = before.piece_on(move.from);
    if (actor == Position::NoPiece)
        throw std::runtime_error("crossed transition has no actor");
    const PieceState& piece = before.piece(actor);
    const bool hiddenGhost = piece.type == PieceType::Ghost && !piece.visible;
    bool destinationKnown = !hiddenGhost || before.is_capture(move);
    if (hiddenGhost && actor < after.piece_count()) {
        const PieceState& childActor = after.piece(actor);
        destinationKnown = destinationKnown ||
          (childActor.alive && childActor.onBoard && childActor.visible);
    }
    const char* actorName = piece.type == PieceType::Ghost ? "ghost"
                          : piece.color == Color::White &&
                            (piece.type == PieceType::King ||
                             piece.type == PieceType::Jester) ? "royal"
                          : "king";
    std::ostringstream out;
    out << "UFCROSSTRANS1|actor=" << actorName
        << "|kind=" << int(move.kind)
        << "|from=" << (hiddenGhost ? "?" : Position::square_name(move.from))
        << "|to=" << (destinationKnown ? Position::square_name(move.to) : "?")
        << "|promotion=" << int(move.promotion)
        << "|view=" << common_position_key(after);
    return out.str();
}

[[nodiscard]] KnowledgePartition successor_partition(
  const KnowledgeState& source, Color player,
  const std::vector<TransitionAtom>& atoms) {
    std::map<std::pair<std::uint32_t, std::string>, KnowledgeCell> grouped;
    for (std::uint32_t childAtom = 0; childAtom < atoms.size(); ++childAtom) {
        const TransitionAtom& transition = atoms[childAtom];
        const std::uint32_t priorCell = cell_for_atom(
          source, player, transition.sourceAtom);
        const std::string& observation = player == Color::White
                                       ? transition.whiteObservation
                                       : transition.blackObservation;
        grouped[{priorCell, observation}].push_back(childAtom);
    }
    KnowledgePartition result;
    for (auto& [key, cell] : grouped) {
        (void)key;
        result.cells.push_back(std::move(cell));
    }
    normalize_partition(result);
    return result;
}

}  // namespace

std::uint32_t encode_public_frame(const PublicFrame& frame) {
    validate_frame(frame);
    constexpr std::uint32_t PairCount =
      (Squares - 1) * (Squares - 2) / 2;
    constexpr std::uint32_t VisibilityCount = Squares - 2;
    const std::uint32_t first = rank_excluding(
      frame.royalFirst, {frame.blackKing});
    const std::uint32_t second = rank_excluding(
      frame.royalSecond, {frame.blackKing});
    std::uint32_t visibility = 0;
    if (frame.visibleGhost)
        visibility = 1 + rank_excluding(
          *frame.visibleGhost,
          {frame.blackKing, frame.royalFirst, frame.royalSecond});
    return (((static_cast<std::uint32_t>(frame.side) * Squares +
              frame.blackKing) * PairCount +
             pair_rank(first, second, Squares - 1)) *
            VisibilityCount + visibility);
}

PublicFrame decode_public_frame(std::uint32_t index) {
    if (index >= RawPublicFrameCount)
        throw std::out_of_range("crossed public frame index");
    constexpr std::uint32_t PairCount =
      (Squares - 1) * (Squares - 2) / 2;
    constexpr std::uint32_t VisibilityCount = Squares - 2;
    const std::uint32_t original = index;
    const std::uint32_t visibility = index % VisibilityCount;
    index /= VisibilityCount;
    const auto [firstRank, secondRank] = pair_unrank(index % PairCount,
                                                     Squares - 1);
    index /= PairCount;
    const std::uint8_t blackKing = static_cast<std::uint8_t>(index % Squares);
    const Color side = static_cast<Color>(index / Squares);
    const std::uint8_t first = unrank_excluding(firstRank, {blackKing});
    const std::uint8_t second = unrank_excluding(secondRank, {blackKing});
    PublicFrame frame{side, blackKing, std::min(first, second),
                      std::max(first, second), {}};
    if (visibility)
        frame.visibleGhost = unrank_excluding(
          visibility - 1,
          {blackKing, frame.royalFirst, frame.royalSecond});
    if (encode_public_frame(frame) != original)
        throw std::runtime_error("crossed public frame codec is not involutive");
    return frame;
}

std::uint32_t encode_source(const ConcreteState& source) {
    const ConcreteState state = horizontal_canonical(source);
    const std::uint32_t whiteRank =
      (state.whiteKing / Position::BoardFiles) *
      (Position::BoardFiles / 2) +
      state.whiteKing % (Position::BoardFiles / 2);
    const std::uint32_t blackRank = rank_excluding(
      state.blackKing, {state.whiteKing});
    const std::uint32_t jesterRank = rank_excluding(
      state.jester, {state.whiteKing, state.blackKing});
    const std::uint32_t ghostRank = rank_excluding(
      state.ghost, {state.whiteKing, state.blackKing, state.jester});
    const std::uint32_t placement =
      ((((static_cast<std::uint32_t>(state.side) * (Squares / 2) + whiteRank) *
          (Squares - 1) + blackRank) *
         (Squares - 2) + jesterRank) *
        (Squares - 3) + ghostRank);
    return placement * 2 + (state.ghostVisible ? 1u : 0u);
}

ConcreteState decode_source(std::uint32_t index) {
    if (index >= StateCount)
        throw std::out_of_range("crossed source index is out of range");
    const bool visible = index % 2 != 0;
    index /= 2;
    const std::uint32_t ghostRank = index % (Squares - 3);
    index /= Squares - 3;
    const std::uint32_t jesterRank = index % (Squares - 2);
    index /= Squares - 2;
    const std::uint32_t blackRank = index % (Squares - 1);
    index /= Squares - 1;
    const std::uint32_t whiteRank = index % (Squares / 2);
    const Color side = static_cast<Color>(index / (Squares / 2));
    const std::uint8_t whiteKing = static_cast<std::uint8_t>(
      (whiteRank / (Position::BoardFiles / 2)) * Position::BoardFiles +
      whiteRank % (Position::BoardFiles / 2));
    const std::uint8_t blackKing = unrank_excluding(
      blackRank, {whiteKing});
    const std::uint8_t jester = unrank_excluding(
      jesterRank, {whiteKing, blackKing});
    const std::uint8_t ghost = unrank_excluding(
      ghostRank, {whiteKing, blackKing, jester});
    return {side, whiteKing, blackKing, jester, ghost, visible};
}

FramedWorld source_to_product(const ConcreteState& state) {
    validate_source(state);
    const std::uint8_t first = std::min(state.whiteKing, state.jester);
    const std::uint8_t second = std::max(state.whiteKing, state.jester);
    PublicFrame frame{state.side, state.blackKing, first, second, {}};
    if (state.ghostVisible)
        frame.visibleGhost = state.ghost;
    const ProductWorld world{state.whiteKing == first, state.ghost};
    validate_world(frame, world);
    return {frame, world};
}

ConcreteState product_to_source(const PublicFrame& frame,
                                const ProductWorld& world) {
    validate_world(frame, world);
    const std::uint8_t whiteKing = world.kingAtFirst
                                 ? frame.royalFirst : frame.royalSecond;
    const std::uint8_t jester = world.kingAtFirst
                              ? frame.royalSecond : frame.royalFirst;
    return {frame.side, whiteKing, frame.blackKing, jester, world.ghost,
            frame.visibleGhost.has_value()};
}

Position make_position(const PublicFrame& frame, const ProductWorld& world) {
    const ConcreteState state = product_to_source(frame, world);
    Position position;
    position.clear();
    const int whiteKing = position.add_piece(
      PieceType::King, Color::White, state.whiteKing);
    const int blackKing = position.add_piece(
      PieceType::King, Color::Black, state.blackKing);
    const int jester = position.add_piece(
      PieceType::Jester, Color::White, state.jester);
    const int ghost = position.add_piece(
      PieceType::Ghost, Color::Black, state.ghost);
    if (whiteKing == Position::NoPiece || blackKing == Position::NoPiece ||
        jester == Position::NoPiece || ghost == Position::NoPiece)
        throw std::runtime_error("cannot construct crossed product world");
    for (const int id : {whiteKing, blackKing, jester, ghost})
        position.piece(id).moved = true;
    position.piece(ghost).visible = state.ghostVisible;
    position.set_side_to_move(state.side);
    return position;
}

void WorldMask::set(unsigned variable) {
    if (variable >= ProductVariables)
        throw std::out_of_range("crossed product variable is invalid");
    words[variable / 64] |= std::uint64_t(1) << (variable % 64);
}

bool WorldMask::test(unsigned variable) const {
    return variable < ProductVariables &&
           ((words[variable / 64] >> (variable % 64)) & 1u);
}

unsigned WorldMask::count() const {
    return static_cast<unsigned>(
      __builtin_popcountll(words[0]) + __builtin_popcountll(words[1]) +
      __builtin_popcountll(words[2]));
}

unsigned world_variable(const PublicFrame& frame,
                        const ProductWorld& world) {
    validate_world(frame, world);
    return (world.kingAtFirst ? 0u : Squares) + world.ghost;
}

ProductWorld decode_world_variable(const PublicFrame& frame,
                                   unsigned variable) {
    if (variable >= ProductVariables)
        throw std::out_of_range("crossed product variable is invalid");
    const ProductWorld world{variable < Squares,
      static_cast<std::uint8_t>(variable % Squares)};
    validate_world(frame, world);
    return world;
}

std::vector<ProductWorld> geometric_worlds(const PublicFrame& frame) {
    validate_frame(frame);
    std::vector<ProductWorld> result;
    if (frame.visibleGhost) {
        result.push_back({true, *frame.visibleGhost});
        result.push_back({false, *frame.visibleGhost});
        return result;
    }
    result.reserve(2 * (Squares - 3));
    for (const bool assignment : {true, false})
        for (std::uint8_t ghost = 0; ghost < Squares; ++ghost) {
            ProductWorld world{assignment, ghost};
            try {
                validate_world(frame, world);
                result.push_back(world);
            }
            catch (const std::invalid_argument&) {
            }
        }
    if (result.size() != 2 * (Squares - 3))
        throw std::runtime_error("crossed hidden product is not 2*77");
    return result;
}

void validate_knowledge_state(const KnowledgeState& state) {
    validate_frame(state.frame);
    if (state.atoms.empty())
        throw std::invalid_argument("crossed knowledge state is empty");
    const WorldMask projection = projected_worlds(state.frame, state.atoms);
    if (!(projection == state.worlds))
        throw std::invalid_argument(
          "crossed physical world projection is stale");
    for (const HistoryAtom& atom : state.atoms)
        validate_world(state.frame, atom.world);
    for (const Color player : {Color::White, Color::Black}) {
        const KnowledgePartition& partition = partition_for(state, player);
        KnowledgePartition canonical = partition;
        normalize_partition(canonical);
        if (!(canonical == partition))
            throw std::invalid_argument(
              "knowledge partition is not in canonical order");
        std::vector<unsigned> coverage(state.atoms.size());
        for (const KnowledgeCell& cell : partition.cells) {
            if (cell.empty())
                throw std::invalid_argument("knowledge partition has empty cell");
            std::optional<unsigned> owned;
            for (const std::uint32_t atom : cell) {
                if (atom >= state.atoms.size() || ++coverage[atom] != 1)
                    throw std::invalid_argument(
                      "knowledge partition is not an exact atom cover");
                const ProductWorld& world = state.atoms[atom].world;
                const unsigned value = player == Color::White
                                     ? unsigned(world.kingAtFirst) : world.ghost;
                if (owned && *owned != value)
                    throw std::invalid_argument(
                      "knowledge cell merges its owner's private fact");
                owned = value;
            }
        }
        if (std::any_of(coverage.begin(), coverage.end(),
              [](unsigned value) { return value != 1; }))
            throw std::invalid_argument(
              "knowledge partition does not cover every history atom");
    }
}

std::optional<KnowledgeState> admitted_fresh_state(const PublicFrame& frame) {
    KnowledgeState result;
    result.frame = frame;
    for (const ProductWorld& world : geometric_worlds(frame))
        if (fresh_world_admission(frame, world) == AdmissionVerdict::Admit)
            result.atoms.push_back({world});
    if (result.atoms.empty())
        return std::nullopt;
    result.worlds = projected_worlds(frame, result.atoms);
    result.white = build_owned_partition(result.atoms, Color::White);
    result.black = build_owned_partition(result.atoms, Color::Black);
    validate_knowledge_state(result);
    return result;
}

KnowledgeState fresh_state(const PublicFrame& frame) {
    std::optional<KnowledgeState> result = admitted_fresh_state(frame);
    if (!result)
        throw std::invalid_argument(
          "crossed public frame has no admitted fresh world");
    return std::move(*result);
}

const KnowledgePartition& partition_for(const KnowledgeState& state,
                                        Color player) {
    validate_side(player);
    return player == Color::White ? state.white : state.black;
}

std::uint32_t cell_for_atom(const KnowledgeState& state, Color player,
                            std::uint32_t atom) {
    if (atom >= state.atoms.size())
        throw std::out_of_range("history atom is outside knowledge state");
    const KnowledgePartition& partition = partition_for(state, player);
    for (std::uint32_t cell = 0; cell < partition.cells.size(); ++cell)
        if (std::find(partition.cells[cell].begin(),
                      partition.cells[cell].end(), atom) !=
            partition.cells[cell].end())
            return cell;
    throw std::runtime_error("history atom has no knowledge cell");
}

std::uint8_t transform_square(std::uint8_t square,
                              RectangleTransform transform) {
    validate_square(square, "rectangle-transform square");
    validate_transform(transform);
    if (static_cast<std::uint8_t>(transform) & 1u)
        square = horizontal_reflection(square);
    if (static_cast<std::uint8_t>(transform) & 2u)
        square = vertical_reflection(square);
    return square;
}

FramedWorld transform_world(const PublicFrame& frame,
                            const ProductWorld& world,
                            RectangleTransform transform) {
    validate_world(frame, world);
    const std::uint8_t physicalKing = world.kingAtFirst
                                    ? frame.royalFirst : frame.royalSecond;
    const std::uint8_t first = transform_square(
      frame.royalFirst, transform);
    const std::uint8_t second = transform_square(
      frame.royalSecond, transform);
    PublicFrame mapped;
    mapped.side = frame.side;
    mapped.blackKing = transform_square(frame.blackKing, transform);
    mapped.royalFirst = std::min(first, second);
    mapped.royalSecond = std::max(first, second);
    if (frame.visibleGhost)
        mapped.visibleGhost = transform_square(
          *frame.visibleGhost, transform);
    const std::uint8_t mappedKing = transform_square(
      physicalKing, transform);
    ProductWorld mappedWorld{
      mappedKing == mapped.royalFirst,
      transform_square(world.ghost, transform)};
    validate_world(mapped, mappedWorld);
    return {mapped, mappedWorld};
}

KnowledgeState transform_state(const KnowledgeState& state,
                               RectangleTransform transform) {
    validate_knowledge_state(state);
    KnowledgeState result;
    for (const HistoryAtom& atom : state.atoms) {
        const FramedWorld mapped = transform_world(
          state.frame, atom.world, transform);
        if (result.atoms.empty())
            result.frame = mapped.frame;
        else if (!(result.frame == mapped.frame))
            throw std::runtime_error(
              "one crossed state transformed into multiple public frames");
        result.atoms.push_back({mapped.world});
    }
    result.worlds = projected_worlds(result.frame, result.atoms);
    result.white = state.white;
    result.black = state.black;
    validate_knowledge_state(result);
    return result;
}

CanonicalState canonicalize_state(const KnowledgeState& state) {
    CanonicalState result{
      transform_state(state, RectangleTransform::Identity),
      RectangleTransform::Identity};
    for (std::uint8_t raw = 1; raw < 4; ++raw) {
        const auto transform = static_cast<RectangleTransform>(raw);
        KnowledgeState candidate = transform_state(state, transform);
        if (frame_key(candidate.frame) < frame_key(result.value.frame))
            result = {std::move(candidate), transform};
    }
    return result;
}

std::vector<std::uint8_t> serialize_state(const KnowledgeState& state) {
    validate_knowledge_state(state);
    constexpr std::array<std::uint8_t, 12> magic{{
      'U', 'F', 'C', 'R', 'O', 'S', 'S', 'S', 'T', 'A', 'T', '1'}};
    std::vector<std::uint8_t> result(magic.begin(), magic.end());
    const auto byte = [&](std::uint8_t value) {
        result.push_back(value);
    };
    const auto word = [&](std::uint32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8)
            result.push_back(static_cast<std::uint8_t>(value >> shift));
    };
    byte(static_cast<std::uint8_t>(state.frame.side));
    byte(state.frame.blackKing);
    byte(state.frame.royalFirst);
    byte(state.frame.royalSecond);
    byte(state.frame.visibleGhost ? 1 : 0);
    byte(state.frame.visibleGhost ? *state.frame.visibleGhost : 0xff);
    word(static_cast<std::uint32_t>(state.atoms.size()));
    for (const HistoryAtom& atom : state.atoms) {
        byte(atom.world.kingAtFirst ? 1 : 0);
        byte(atom.world.ghost);
    }
    const auto partition = [&](const KnowledgePartition& value) {
        word(static_cast<std::uint32_t>(value.cells.size()));
        for (const KnowledgeCell& cell : value.cells) {
            word(static_cast<std::uint32_t>(cell.size()));
            for (const std::uint32_t atom : cell)
                word(atom);
        }
    };
    partition(state.white);
    partition(state.black);
    return result;
}

KnowledgeState deserialize_state(const std::vector<std::uint8_t>& bytes) {
    constexpr std::array<std::uint8_t, 12> magic{{
      'U', 'F', 'C', 'R', 'O', 'S', 'S', 'S', 'T', 'A', 'T', '1'}};
    if (bytes.size() < magic.size() ||
        !std::equal(magic.begin(), magic.end(), bytes.begin()))
        throw std::invalid_argument("invalid crossed-state magic");
    std::size_t cursor = magic.size();
    const auto byte = [&]() -> std::uint8_t {
        if (cursor == bytes.size())
            throw std::invalid_argument("truncated crossed-state byte");
        return bytes[cursor++];
    };
    const auto word = [&]() -> std::uint32_t {
        if (bytes.size() - cursor < 4)
            throw std::invalid_argument("truncated crossed-state word");
        std::uint32_t value = 0;
        for (unsigned shift = 0; shift < 32; shift += 8)
            value |= static_cast<std::uint32_t>(bytes[cursor++]) << shift;
        return value;
    };
    KnowledgeState result;
    result.frame.side = static_cast<Color>(byte());
    result.frame.blackKing = byte();
    result.frame.royalFirst = byte();
    result.frame.royalSecond = byte();
    const std::uint8_t visible = byte();
    const std::uint8_t visibleSquare = byte();
    if (visible > 1 || (!visible && visibleSquare != 0xff))
        throw std::invalid_argument("invalid crossed-state visibility field");
    if (visible)
        result.frame.visibleGhost = visibleSquare;
    const std::uint32_t atoms = word();
    if (atoms > (bytes.size() - cursor) / 2)
        throw std::invalid_argument("crossed-state atom extent exceeds payload");
    result.atoms.reserve(atoms);
    for (std::uint32_t atom = 0; atom < atoms; ++atom) {
        const std::uint8_t royal = byte();
        const std::uint8_t ghost = byte();
        if (royal > 1)
            throw std::invalid_argument("invalid crossed-state royal bit");
        result.atoms.push_back({{royal != 0, ghost}});
    }
    const auto partition = [&](KnowledgePartition& value) {
        const std::uint32_t cells = word();
        if (cells > atoms)
            throw std::invalid_argument(
              "crossed-state partition has too many cells");
        value.cells.reserve(cells);
        for (std::uint32_t cellIndex = 0; cellIndex < cells; ++cellIndex) {
            const std::uint32_t members = word();
            if (!members || members > atoms ||
                members > (bytes.size() - cursor) / 4)
                throw std::invalid_argument(
                  "invalid crossed-state partition cell extent");
            KnowledgeCell cell;
            cell.reserve(members);
            for (std::uint32_t member = 0; member < members; ++member)
                cell.push_back(word());
            value.cells.push_back(std::move(cell));
        }
    };
    partition(result.white);
    partition(result.black);
    if (cursor != bytes.size())
        throw std::invalid_argument("crossed-state payload has trailing bytes");
    result.worlds = projected_worlds(result.frame, result.atoms);
    validate_knowledge_state(result);
    return result;
}

bool operator<(const ActionKey& lhs, const ActionKey& rhs) {
    return std::tie(lhs.from, lhs.to, lhs.auxiliary, lhs.kind, lhs.promotion) <
           std::tie(rhs.from, rhs.to, rhs.auxiliary, rhs.kind, rhs.promotion);
}

ActionKey action_key(const Move& move) {
    return {move.from, move.to, move.auxiliary, move.kind, move.promotion};
}

ActionKey transform_action(ActionKey action, RectangleTransform transform) {
    validate_transform(transform);
    if (action.kind == MoveKind::Pass)
        return action;
    if (action.from >= Squares || action.to >= Squares)
        throw std::invalid_argument("action endpoint is outside the board");
    action.from = transform_square(action.from, transform);
    action.to = transform_square(action.to, transform);
    if (action_auxiliary_is_square(action)) {
        if (action.auxiliary >= Squares)
            throw std::invalid_argument(
              "square-valued action auxiliary is outside the board");
        action.auxiliary = transform_square(action.auxiliary, transform);
    }
    return action;
}

std::vector<ActionKey> legal_actions(const Position& position) {
    std::vector<ActionKey> result;
    for (const Move& move : position.legal_moves())
        result.push_back(action_key(move));
    std::sort(result.begin(), result.end());
    if (std::adjacent_find(result.begin(), result.end()) != result.end())
        throw std::runtime_error(
          "legal move generator produced duplicate complete actions");
    return result;
}

KnowledgeState refine_mover_decisions(const KnowledgeState& state) {
    validate_knowledge_state(state);
    KnowledgeState result = state;
    KnowledgePartition refined;
    const KnowledgePartition& source = partition_for(
      state, state.frame.side);
    for (const KnowledgeCell& cell : source.cells) {
        std::map<std::string, KnowledgeCell> grouped;
        for (const std::uint32_t atom : cell) {
            const Position position = make_position(
              state.frame, state.atoms[atom].world);
            grouped[decision_observation_key(
              position, {state.frame.side, false})].push_back(atom);
        }
        for (auto& [observation, split] : grouped) {
            (void)observation;
            refined.cells.push_back(std::move(split));
        }
    }
    normalize_partition(refined);
    if (state.frame.side == Color::White)
        result.white = std::move(refined);
    else
        result.black = std::move(refined);
    validate_knowledge_state(result);
    return result;
}

std::vector<ActionKey> uniform_actions(const KnowledgeState& state,
                                       std::uint32_t moverCell) {
    validate_knowledge_state(state);
    const KnowledgePartition& partition = partition_for(
      state, state.frame.side);
    if (moverCell >= partition.cells.size())
        throw std::out_of_range("mover cell is outside its partition");
    const KnowledgeCell& cell = partition.cells[moverCell];
    std::optional<std::string> decision;
    std::set<ActionKey> common;
    bool first = true;
    for (const std::uint32_t atom : cell) {
        const Position position = make_position(
          state.frame, state.atoms[atom].world);
        const std::string observation = decision_observation_key(
          position, {state.frame.side, false});
        if (decision && *decision != observation)
            throw std::invalid_argument(
              "mover cell crosses a private legal-dot observation");
        decision = observation;
        const std::vector<ActionKey> actions = legal_actions(position);
        const std::set<ActionKey> available(actions.begin(), actions.end());
        if (first) {
            common = available;
            first = false;
        }
        else {
            std::set<ActionKey> intersection;
            std::set_intersection(
              common.begin(), common.end(), available.begin(), available.end(),
              std::inserter(intersection, intersection.begin()));
            common = std::move(intersection);
        }
    }
    return {common.begin(), common.end()};
}

std::uint32_t encode_lower_jester(const LowerJesterState& state) {
    validate_side(state.side);
    validate_square(state.whiteKing, "lower White King square");
    validate_square(state.blackKing, "lower Black King square");
    validate_square(state.jester, "lower Jester square");
    if (!distinct({state.whiteKing, state.blackKing, state.jester}))
        throw std::invalid_argument("overlapping lower Jester placement");
    const std::uint32_t blackRank = rank_excluding(
      state.blackKing, {state.whiteKing});
    const std::uint32_t jesterRank = rank_excluding(
      state.jester, {state.whiteKing, state.blackKing});
    return ((static_cast<std::uint32_t>(state.side) * Squares +
             state.whiteKing) * (Squares - 1) + blackRank) *
             (Squares - 2) + jesterRank;
}

LowerJesterState decode_lower_jester(std::uint32_t index) {
    if (index >= LowerJesterStateCount)
        throw std::out_of_range("lower Jester index is out of range");
    const std::uint32_t jesterRank = index % (Squares - 2);
    index /= Squares - 2;
    const std::uint32_t blackRank = index % (Squares - 1);
    index /= Squares - 1;
    const std::uint8_t whiteKing = static_cast<std::uint8_t>(index % Squares);
    const Color side = static_cast<Color>(index / Squares);
    const std::uint8_t blackKing = unrank_excluding(
      blackRank, {whiteKing});
    const std::uint8_t jester = unrank_excluding(
      jesterRank, {whiteKing, blackKing});
    return {side, whiteKing, blackKing, jester};
}

std::uint32_t encode_lower_ghost(const LowerGhostState& state) {
    if (state.side != Role::GhostOwner && state.side != Role::Observer)
        throw std::invalid_argument("invalid lower Ghost side");
    validate_square(state.ownerKing, "lower Ghost-owner King square");
    validate_square(state.observerKing, "lower observer King square");
    validate_square(state.ghost, "lower Ghost square");
    if (!distinct({state.ownerKing, state.observerKing, state.ghost}))
        throw std::invalid_argument("overlapping lower Ghost placement");
    const std::uint32_t observerRank = rank_excluding(
      state.observerKing, {state.ownerKing});
    const std::uint32_t ghostRank = rank_excluding(
      state.ghost, {state.ownerKing, state.observerKing});
    const std::uint32_t placement =
      ((static_cast<std::uint32_t>(state.side) * Squares + state.ownerKing) *
       (Squares - 1) + observerRank) * (Squares - 2) + ghostRank;
    return placement * 2 + (state.visible ? 1u : 0u);
}

LowerGhostState decode_lower_ghost(std::uint32_t index) {
    if (index >= LowerGhostStateCount)
        throw std::out_of_range("lower Ghost index is out of range");
    const bool visible = index % 2 != 0;
    index /= 2;
    const std::uint32_t ghostRank = index % (Squares - 2);
    index /= Squares - 2;
    const std::uint32_t observerRank = index % (Squares - 1);
    index /= Squares - 1;
    const std::uint8_t ownerKing = static_cast<std::uint8_t>(index % Squares);
    const Role side = static_cast<Role>(index / Squares);
    const std::uint8_t observerKing = unrank_excluding(
      observerRank, {ownerKing});
    const std::uint8_t ghost = unrank_excluding(
      ghostRank, {ownerKing, observerKing});
    return {side, ownerKing, observerKing, ghost, visible};
}

ClassifiedChild classify_child(const Position& position) {
    const LiveMaterial live = scan_live(position);
    if (!live.unexpected && live.live == 4 &&
        live.whiteKing != Position::NoSquare &&
        live.blackKing != Position::NoSquare &&
        live.jester != Position::NoSquare &&
        live.ghost != Position::NoSquare) {
        return {ChildDomain::SameClass,
          encode_source({position.side_to_move(),
            static_cast<std::uint8_t>(live.whiteKing),
            static_cast<std::uint8_t>(live.blackKing),
            static_cast<std::uint8_t>(live.jester),
            static_cast<std::uint8_t>(live.ghost), live.ghostVisible}), {}};
    }
    if (!live.unexpected && live.live == 3 &&
        live.whiteKing != Position::NoSquare &&
        live.blackKing != Position::NoSquare &&
        live.jester != Position::NoSquare &&
        live.ghost == Position::NoSquare) {
        return {ChildDomain::LowerJester,
          encode_lower_jester({position.side_to_move(),
            static_cast<std::uint8_t>(live.whiteKing),
            static_cast<std::uint8_t>(live.blackKing),
            static_cast<std::uint8_t>(live.jester)}), {}};
    }
    if (!live.unexpected && live.live == 3 &&
        live.whiteKing != Position::NoSquare &&
        live.blackKing != Position::NoSquare &&
        live.jester == Position::NoSquare &&
        live.ghost != Position::NoSquare) {
        return {ChildDomain::LowerGhost,
          encode_lower_ghost({
            position.side_to_move() == Color::Black
              ? Role::GhostOwner : Role::Observer,
            static_cast<std::uint8_t>(live.blackKing),
            static_cast<std::uint8_t>(live.whiteKing),
            static_cast<std::uint8_t>(live.ghost), live.ghostVisible}), {}};
    }
    if (position.game_over())
        return {ChildDomain::ExactTerminal, 0, position.winner()};
    return {ChildDomain::Invalid, 0, {}};
}

std::optional<FramedWorld> same_class_product(const Position& position) {
    const LiveMaterial live = scan_live(position);
    if (live.unexpected || live.live != 4 ||
        live.whiteKing == Position::NoSquare ||
        live.blackKing == Position::NoSquare ||
        live.jester == Position::NoSquare ||
        live.ghost == Position::NoSquare)
        return std::nullopt;
    return source_to_product({position.side_to_move(),
      static_cast<std::uint8_t>(live.whiteKing),
      static_cast<std::uint8_t>(live.blackKing),
      static_cast<std::uint8_t>(live.jester),
      static_cast<std::uint8_t>(live.ghost), live.ghostVisible});
}

LowerJesterSet inherited_lower_jester_set(
  const std::vector<ClassifiedChild>& children) {
    if (children.empty())
        throw std::invalid_argument(
          "lower Jester set must contain an assignment");
    std::set<std::uint32_t> unique;
    std::optional<LowerJesterSet> result;
    for (const ClassifiedChild& child : children) {
        if (child.domain != ChildDomain::LowerJester)
            throw std::invalid_argument(
              "lower Jester set contains another material class");
        unique.insert(child.index);
        const LowerJesterState state = decode_lower_jester(child.index);
        const std::uint8_t first = std::min(state.whiteKing, state.jester);
        const std::uint8_t second = std::max(state.whiteKing, state.jester);
        if (!result)
            result = LowerJesterSet{
              0, state.side, state.blackKing, first, second, {}};
        else if (result->side != state.side ||
                 result->blackKing != state.blackKing ||
                 result->royalFirst != first ||
                 result->royalSecond != second)
            throw std::invalid_argument(
              "one lower Jester observation mixes public frames");
    }
    if (unique.size() > 2)
        throw std::invalid_argument("lower Jester image exceeds royal pair");
    result->cardinality = static_cast<std::uint8_t>(unique.size());
    std::copy(unique.begin(), unique.end(), result->concrete.begin());
    if (result->cardinality == 2) {
        const LowerJesterState first =
          decode_lower_jester(result->concrete[0]);
        const LowerJesterState second =
          decode_lower_jester(result->concrete[1]);
        if (first.whiteKing != second.jester ||
            first.jester != second.whiteKing)
            throw std::invalid_argument(
              "lower Jester children are not swapped assignments");
    }
    return *result;
}

void GhostMask::set(unsigned square) {
    if (square >= Squares)
        throw std::out_of_range("Ghost mask square is outside the board");
    if (square < 64)
        low |= std::uint64_t(1) << square;
    else
        high |= std::uint16_t(1) << (square - 64);
}

bool GhostMask::test(unsigned square) const {
    if (square >= Squares)
        return false;
    return square < 64 ? (low >> square) & 1u
                       : (high >> (square - 64)) & 1u;
}

unsigned GhostMask::count() const {
    return static_cast<unsigned>(
      __builtin_popcountll(low) + __builtin_popcount(high));
}

LowerGhostImage inherited_lower_ghost_image(
  const std::vector<ClassifiedChild>& children) {
    if (children.empty() ||
        children.front().domain != ChildDomain::LowerGhost)
        throw std::invalid_argument("lower Ghost image is empty or invalid");
    const LowerGhostState first = decode_lower_ghost(
      children.front().index);
    LowerGhostImage result{
      first.side, first.ownerKing, first.observerKing, first.visible, {}};
    for (const ClassifiedChild& child : children) {
        if (child.domain != ChildDomain::LowerGhost)
            throw std::invalid_argument(
              "lower Ghost image contains another material class");
        const LowerGhostState state = decode_lower_ghost(child.index);
        if (state.side != result.side ||
            state.ownerKing != result.ownerKing ||
            state.observerKing != result.observerKing ||
            state.visible != result.visible)
            throw std::invalid_argument(
              "one lower Ghost observation mixes public frames");
        result.locations.set(state.ghost);
    }
    return result;
}

CompleteTransitions enumerate_complete_transitions(
  const KnowledgeState& state) {
    CompleteTransitions result;
    result.decisionState = refine_mover_decisions(state);
    const KnowledgeState& decision = result.decisionState;
    const Color mover = decision.frame.side;
    const KnowledgePartition& moverPartition = partition_for(
      decision, mover);

    struct Candidate {
        std::uint32_t action = 0;
        std::string publicObservation;
        TransitionAtom transition;
    };
    std::vector<Candidate> candidates;
    for (std::uint32_t cell = 0; cell < moverPartition.cells.size(); ++cell) {
        for (const ActionKey& action : uniform_actions(decision, cell)) {
            const std::uint32_t actionIndex =
              static_cast<std::uint32_t>(result.actions.size());
            result.actions.push_back({{cell, action}, {}});
            for (const std::uint32_t atom : moverPartition.cells[cell]) {
                const Position position = make_position(
                  decision.frame, decision.atoms[atom].world);
                const std::optional<Move> move = locate_action(
                  position, action);
                if (!move)
                    throw std::runtime_error(
                      "uniform crossed action disappeared in one history");
                Position child = position;
                Undo undo;
                if (!child.make_move(*move, undo))
                    throw std::runtime_error(
                      "uniform crossed transition failed to apply");
                const ClassifiedChild classified = classify_child(child);
                if (classified.domain == ChildDomain::Invalid)
                    throw std::runtime_error(
                      "complete crossed transition left the certified domains");
                std::optional<FramedWorld> physical;
                if (classified.domain == ChildDomain::SameClass) {
                    physical = same_class_product(child);
                    if (!physical)
                        throw std::runtime_error(
                          "same-class crossed child lost physical coordinate");
                }
                candidates.push_back({actionIndex,
                  common_transition_key(position, *move, child),
                  {atom, decision.atoms[atom].world, classified, physical,
                   transition_observation_key(
                     position, *move, child, {Color::White, false}),
                   transition_observation_key(
                     position, *move, child, {Color::Black, false})}});
            }
        }
    }

    std::map<std::string, std::vector<std::uint32_t>> grouped;
    for (std::uint32_t candidate = 0; candidate < candidates.size(); ++candidate)
        grouped[candidates[candidate].publicObservation].push_back(candidate);

    for (const auto& [observation, members] : grouped) {
        const std::uint32_t bucketIndex =
          static_cast<std::uint32_t>(result.buckets.size());
        SuccessorBucket bucket;
        bucket.publicObservation = observation;
        bucket.domain = candidates[members.front()].transition.child.domain;
        for (const std::uint32_t candidate : members) {
            const Candidate& item = candidates[candidate];
            if (item.transition.child.domain != bucket.domain)
                throw std::runtime_error(
                  "one complete crossed observation mixes child domains");
            const std::uint32_t childAtom =
              static_cast<std::uint32_t>(bucket.atoms.size());
            bucket.atoms.push_back(item.transition);
            result.actions[item.action].outcomes.push_back({
              item.transition.sourceAtom, bucketIndex, childAtom,
              item.transition.child});
        }
        bucket.white = successor_partition(
          decision, Color::White, bucket.atoms);
        bucket.black = successor_partition(
          decision, Color::Black, bucket.atoms);
        if (bucket.domain == ChildDomain::SameClass) {
            KnowledgeState child;
            for (const TransitionAtom& transition : bucket.atoms) {
                if (!transition.sameClassProduct)
                    throw std::runtime_error(
                      "same-class complete transition lacks a physical child");
                if (child.atoms.empty())
                    child.frame = transition.sameClassProduct->frame;
                else if (!(child.frame == transition.sameClassProduct->frame))
                    throw std::runtime_error(
                      "one complete observation mixes physical child frames");
                child.atoms.push_back({transition.sameClassProduct->world});
            }
            child.worlds = projected_worlds(child.frame, child.atoms);
            child.white = bucket.white;
            child.black = bucket.black;
            validate_knowledge_state(child);
            bucket.sameClass = std::move(child);
        }
        result.buckets.push_back(std::move(bucket));
    }

    for (const CellActionOutcomes& action : result.actions) {
        const KnowledgeCell& cell = moverPartition.cells[action.choice.cell];
        if (action.outcomes.size() != cell.size())
            throw std::runtime_error(
              "complete crossed cell/action does not conserve histories");
        std::vector<std::uint32_t> sources;
        sources.reserve(action.outcomes.size());
        for (const AtomOutcome& outcome : action.outcomes)
            sources.push_back(outcome.sourceAtom);
        std::sort(sources.begin(), sources.end());
        if (sources != cell)
            throw std::runtime_error(
              "complete crossed cell/action source coverage residual");
    }
    return result;
}

std::vector<SuccessorBucket> apply_uniform_policy(
  const KnowledgeState& decisionState,
  const std::vector<CellAction>& policy) {
    validate_knowledge_state(decisionState);
    const Color mover = decisionState.frame.side;
    const KnowledgePartition& moverPartition = partition_for(
      decisionState, mover);
    if (policy.size() != moverPartition.cells.size())
        throw std::invalid_argument(
          "uniform policy must cover every mover cell exactly once");
    std::vector<std::optional<ActionKey>> byCell(moverPartition.cells.size());
    for (const CellAction& choice : policy) {
        if (choice.cell >= byCell.size() || byCell[choice.cell])
            throw std::invalid_argument(
              "uniform policy has an invalid or duplicate cell");
        const std::vector<ActionKey> common = uniform_actions(
          decisionState, choice.cell);
        if (!std::binary_search(common.begin(), common.end(), choice.action))
            throw std::invalid_argument(
              "policy action is not legal throughout its private cell");
        byCell[choice.cell] = choice.action;
    }

    std::map<std::string, std::vector<TransitionAtom>> grouped;
    for (std::uint32_t atom = 0; atom < decisionState.atoms.size(); ++atom) {
        const std::uint32_t cell = cell_for_atom(
          decisionState, mover, atom);
        const Position position = make_position(
          decisionState.frame, decisionState.atoms[atom].world);
        const std::optional<Move> move = locate_action(
          position, *byCell[cell]);
        if (!move)
            throw std::runtime_error(
              "validated uniform action disappeared in one history");
        Position child = position;
        Undo undo;
        if (!child.make_move(*move, undo))
            throw std::runtime_error(
              "validated crossed transition failed to apply");
        const ClassifiedChild classified = classify_child(child);
        std::optional<FramedWorld> physical;
        if (classified.domain == ChildDomain::SameClass) {
            physical = same_class_product(child);
            if (!physical)
                throw std::runtime_error(
                  "same-class crossed child lost physical coordinate");
        }
        const std::string common = common_transition_key(
          position, *move, child);
        grouped[common].push_back({
          atom, decisionState.atoms[atom].world, classified, physical,
          transition_observation_key(
            position, *move, child, {Color::White, false}),
          transition_observation_key(
            position, *move, child, {Color::Black, false})});
    }

    std::vector<SuccessorBucket> result;
    for (auto& [observation, transitions] : grouped) {
        SuccessorBucket bucket;
        bucket.publicObservation = observation;
        bucket.domain = transitions.front().child.domain;
        if (std::any_of(transitions.begin(), transitions.end(),
              [&](const TransitionAtom& transition) {
                  return transition.child.domain != bucket.domain;
              }))
            throw std::runtime_error(
              "one public observation mixes child material domains");
        bucket.atoms = std::move(transitions);
        bucket.white = successor_partition(
          decisionState, Color::White, bucket.atoms);
        bucket.black = successor_partition(
          decisionState, Color::Black, bucket.atoms);
        if (bucket.domain == ChildDomain::SameClass) {
            KnowledgeState child;
            for (const TransitionAtom& transition : bucket.atoms) {
                if (!transition.sameClassProduct)
                    throw std::runtime_error(
                      "same-class transition has no physical child");
                if (child.atoms.empty())
                    child.frame = transition.sameClassProduct->frame;
                else if (!(child.frame ==
                           transition.sameClassProduct->frame))
                    throw std::runtime_error(
                      "one public observation mixes physical child frames");
                child.atoms.push_back(
                  {transition.sameClassProduct->world});
            }
            child.worlds = projected_worlds(child.frame, child.atoms);
            child.white = bucket.white;
            child.black = bucket.black;
            validate_knowledge_state(child);
            bucket.sameClass = std::move(child);
        }
        result.push_back(std::move(bucket));
    }
    return result;
}

AdmissionVerdict fresh_world_admission(const PublicFrame& frame,
                                       const ProductWorld& world) {
    const Position position = make_position(frame, world);
    if (position.has_forced_action() ||
        !position.ordinary_predecessor_king_safe())
        return AdmissionVerdict::Reject;
    if (!frame.visibleGhost &&
        (adjacent(world.ghost, frame.royalFirst) ||
         adjacent(world.ghost, frame.royalSecond)))
        return AdmissionVerdict::Reject;
    return AdmissionVerdict::Admit;
}

}  // namespace Stockfish::Ultimate::CrossedJesterGhostInformation
