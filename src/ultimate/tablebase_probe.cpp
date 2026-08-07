/* Ultimate Fish exact endgame tablebase probing, GPLv3 or later. */

#include "tablebase_probe.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace Stockfish::Ultimate {
namespace {

constexpr std::uint32_t SquareCount = Position::BoardSquares;
constexpr std::uint32_t StateCount =
  2 * SquareCount * (SquareCount - 1) * (SquareCount - 2);
constexpr std::uint32_t FourStateCount =
  2 * (SquareCount / 2) * (SquareCount - 1) * (SquareCount - 2) * (SquareCount - 3);
constexpr std::uint32_t IdenticalFourStateCount = FourStateCount / 2;

struct Database {
    PieceType attacker = PieceType::Count;
    PieceType secondary = PieceType::Count;
    Color secondaryColor = Color::White;
    std::uint32_t substates = 1;
    std::uint32_t count = 0;
    std::vector<TablebaseResult> records;
    std::vector<std::uint8_t> wdlPlane;
    std::vector<std::uint8_t> dtwPlane;
    std::vector<std::pair<std::uint32_t, std::uint16_t>> exceptions;

    TablebaseResult at(std::uint32_t index) const {
        if (!wdlPlane.empty()) {
            const auto wdl = static_cast<TablebaseWdl>(
              (wdlPlane[index / 4] >> ((index % 4) * 2)) & 3);
            std::uint16_t distance = dtwPlane[index];
            if (distance == 255) {
                const auto found = std::lower_bound(
                  exceptions.begin(), exceptions.end(), index,
                  [](const auto& item, std::uint32_t target) { return item.first < target; });
                if (found != exceptions.end() && found->first == index)
                    distance = found->second;
            }
            return {wdl, distance};
        }
        return records[index];
    }
};

std::uint32_t encode(Color side, std::uint8_t whiteKing,
                     std::uint8_t blackKing, std::uint8_t attacker) {
    const std::uint32_t blackRank = blackKing - (blackKing > whiteKing);
    const std::uint8_t low = std::min(whiteKing, blackKing);
    const std::uint8_t high = std::max(whiteKing, blackKing);
    const std::uint32_t attackerRank = attacker - (attacker > low) - (attacker > high);
    return (((static_cast<std::uint32_t>(side) * SquareCount + whiteKing)
             * (SquareCount - 1) + blackRank)
            * (SquareCount - 2) + attackerRank);
}

std::uint8_t reflect_horizontal(std::uint8_t square) {
    return static_cast<std::uint8_t>((square / 8) * 8 + 7 - square % 8);
}

std::uint32_t rank_excluding(std::uint8_t square,
                             std::initializer_list<std::uint8_t> used) {
    std::uint32_t rank = square;
    for (const std::uint8_t occupied : used)
        rank -= occupied < square;
    return rank;
}

std::uint32_t encode_four(Color side, std::uint8_t whiteKing,
                          std::uint8_t blackKing, std::uint8_t first,
                          std::uint8_t second) {
    if (whiteKing % 8 >= 4) {
        whiteKing = reflect_horizontal(whiteKing);
        blackKing = reflect_horizontal(blackKing);
        first = reflect_horizontal(first);
        second = reflect_horizontal(second);
    }
    const std::uint32_t whiteRank = (whiteKing / 8) * 4 + whiteKing % 8;
    const std::uint32_t blackRank = rank_excluding(blackKing, {whiteKing});
    const std::uint32_t firstRank = rank_excluding(first, {whiteKing, blackKing});
    const std::uint32_t secondRank = rank_excluding(second, {whiteKing, blackKing, first});
    return ((((static_cast<std::uint32_t>(side) * (SquareCount / 2) + whiteRank)
               * (SquareCount - 1) + blackRank)
              * (SquareCount - 2) + firstRank)
             * (SquareCount - 3) + secondRank);
}

std::uint32_t encode_identical_four(Color side, std::uint8_t whiteKing,
                                    std::uint8_t blackKing, std::uint8_t first,
                                    std::uint8_t second) {
    if (whiteKing % 8 >= 4) {
        whiteKing = reflect_horizontal(whiteKing);
        blackKing = reflect_horizontal(blackKing);
        first = reflect_horizontal(first);
        second = reflect_horizontal(second);
    }
    const std::uint32_t whiteRank = (whiteKing / 8) * 4 + whiteKing % 8;
    const std::uint32_t blackRank = rank_excluding(blackKing, {whiteKing});
    std::uint32_t firstRank = rank_excluding(first, {whiteKing, blackKing});
    std::uint32_t secondRank = rank_excluding(second, {whiteKing, blackKing});
    if (firstRank > secondRank)
        std::swap(firstRank, secondRank);
    constexpr std::uint32_t remaining = SquareCount - 2;
    constexpr std::uint32_t pairs = remaining * (remaining - 1) / 2;
    const std::uint32_t pairRank =
      firstRank * (2 * remaining - firstRank - 1) / 2 + secondRank - firstRank - 1;
    return ((static_cast<std::uint32_t>(side) * (SquareCount / 2) + whiteRank)
             * (SquareCount - 1) + blackRank) * pairs + pairRank;
}

std::vector<std::string> paths() {
    const auto expand = [](const std::vector<std::string>& configured) {
        std::vector<std::string> result;
        for (const std::string& item : configured) {
            std::error_code error;
            if (std::filesystem::is_directory(item, error)) {
                for (const auto& entry : std::filesystem::directory_iterator(item, error))
                    if (!error && entry.path().extension() == ".uftb")
                        result.push_back(entry.path().string());
            }
            else result.push_back(item);
        }
        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    };
    if (const char* configured = std::getenv("ULTIMATE_TABLEBASE_PATH")) {
        std::vector<std::string> entries;
        std::string text(configured);
        std::size_t begin = 0;
        while (begin <= text.size()) {
            const std::size_t end = text.find(':', begin);
            const std::string path = text.substr(begin, end - begin);
            if (!path.empty())
                entries.push_back(path);
            if (end == std::string::npos)
                break;
            begin = end + 1;
        }
        return expand(entries);
    }
    return expand({"tablebases", "../tablebases"});
}

std::vector<Database> load_databases() {
    std::vector<Database> result;
    for (const std::string& path : paths()) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
            continue;
        std::array<char, 8> magic{};
        std::uint32_t version = 0, piece = 0, count = 0, edges = 0;
        stream.read(magic.data(), magic.size());
        stream.read(reinterpret_cast<char*>(&version), sizeof(version));
        stream.read(reinterpret_cast<char*>(&piece), sizeof(piece));
        stream.read(reinterpret_cast<char*>(&count), sizeof(count));
        stream.read(reinterpret_cast<char*>(&edges), sizeof(edges));
        const std::array<char, 8> expected{{'U','F','T','B','1','\0','\0','\0'}};
        std::uint32_t substates = 1;
        if (version >= 3)
            stream.read(reinterpret_cast<char*>(&substates), sizeof(substates));
        if (!stream || magic != expected || (version < 2 || version > 5) ||
            !substates ||
            count != (version == 5 ? (count == FourStateCount ? FourStateCount
                                      : count == IdenticalFourStateCount ? IdenticalFourStateCount
                                      : StateCount)
                                   : StateCount * substates) ||
            piece >= static_cast<std::uint32_t>(PieceType::Count))
            continue;
        Database database;
        database.attacker = static_cast<PieceType>(piece);
        database.substates = substates;
        database.count = count;
        if (version >= 4) {
            std::uint32_t wdlBytes = 0, dtwBytes = 0, exceptionCount = 0;
            stream.read(reinterpret_cast<char*>(&wdlBytes), sizeof(wdlBytes));
            stream.read(reinterpret_cast<char*>(&dtwBytes), sizeof(dtwBytes));
            stream.read(reinterpret_cast<char*>(&exceptionCount), sizeof(exceptionCount));
            if (version == 5) {
                std::uint32_t secondary = 0, secondaryColor = 0;
                stream.read(reinterpret_cast<char*>(&secondary), sizeof(secondary));
                stream.read(reinterpret_cast<char*>(&secondaryColor), sizeof(secondaryColor));
                if (secondary >= static_cast<std::uint32_t>(PieceType::Count) ||
                    secondaryColor > static_cast<std::uint32_t>(Color::Black)) {
                    database.count = 0;
                    continue;
                }
                database.secondary = static_cast<PieceType>(secondary);
                database.secondaryColor = static_cast<Color>(secondaryColor);
            }
            if (!stream || wdlBytes != (count + 3) / 4 || dtwBytes != count) {
                database.count = 0;
                continue;
            }
            database.wdlPlane.resize(wdlBytes);
            database.dtwPlane.resize(dtwBytes);
            stream.read(reinterpret_cast<char*>(database.wdlPlane.data()), database.wdlPlane.size());
            stream.read(reinterpret_cast<char*>(database.dtwPlane.data()), database.dtwPlane.size());
            for (std::uint32_t index = 0; index < count; ++index) {
                const std::uint8_t wdl =
                  (database.wdlPlane[index / 4] >> ((index % 4) * 2)) & 3;
                if (wdl < static_cast<std::uint8_t>(TablebaseWdl::Win) ||
                    wdl > static_cast<std::uint8_t>(TablebaseWdl::Draw)) {
                    database.count = 0;
                    break;
                }
            }
            for (std::uint32_t item = 0; item < exceptionCount; ++item) {
                std::uint32_t index = 0;
                std::uint16_t distance = 0;
                stream.read(reinterpret_cast<char*>(&index), sizeof(index));
                stream.read(reinterpret_cast<char*>(&distance), sizeof(distance));
                if (!stream || index >= database.count) {
                    database.count = 0;
                    break;
                }
                database.exceptions.emplace_back(index, distance);
            }
        }
        else {
            database.records.resize(count);
            for (TablebaseResult& record : database.records) {
                std::uint16_t packed = 0;
                stream.read(reinterpret_cast<char*>(&packed), sizeof(packed));
                const std::uint8_t wdl = packed >> 14;
                record.dtw = packed & ((1u << 14) - 1);
                if (wdl < static_cast<std::uint8_t>(TablebaseWdl::Win) ||
                    wdl > static_cast<std::uint8_t>(TablebaseWdl::Draw)) {
                    database.records.clear();
                    break;
                }
                record.wdl = static_cast<TablebaseWdl>(wdl);
            }
        }
        if (stream && database.count == count &&
            (database.records.size() == count || database.dtwPlane.size() == count))
            result.push_back(std::move(database));
    }
    return result;
}

const std::vector<Database>& databases() {
    static std::once_flag once;
    static std::vector<Database> loaded;
    std::call_once(once, [] { loaded = load_databases(); });
    return loaded;
}

}  // namespace

std::optional<TablebaseResult> TablebaseProbe::probe(const Position& position) {
    if (position.forcedTimeoutWinner_ >= 0)
        return std::nullopt;

    int whiteKing = Position::NoPiece;
    int blackKing = Position::NoPiece;
    std::array<int, 2> extras{Position::NoPiece, Position::NoPiece};
    int extraCount = 0;
    int alive = 0;
    for (int id = 0; id < position.pieceCount_; ++id) {
        const PieceState& piece = position.pieces_[id];
        if (!piece.alive)
            continue;
        if (!piece.onBoard || piece.host != Position::NoPiece ||
            (!piece.moved && piece.type != PieceType::Pawn))
            return std::nullopt;
        ++alive;
        if (piece.type == PieceType::King)
            (piece.color == Color::White ? whiteKing : blackKing) = id;
        else if (extraCount < 2)
            extras[extraCount++] = id;
        else return std::nullopt;
    }
    if ((alive != 3 && alive != 4) || whiteKing == Position::NoPiece ||
        blackKing == Position::NoPiece || extraCount != alive - 2)
        return std::nullopt;

    if (extraCount == 2) {
        if (position.continuation_ != Continuation::None ||
            position.forcedPiece_ != Position::NoPiece ||
            position.enPassantVictim_ != Position::NoPiece ||
            position.enPassantSquare_ != Position::NoSquare)
            return std::nullopt;
        for (const Database& database : databases()) {
            if (database.secondary == PieceType::Count)
                continue;
            for (int order = 0; order < 2; ++order) {
                const int first = extras[order];
                const int second = extras[1 - order];
                const PieceState& primary = position.pieces_[first];
                const PieceState& secondary = position.pieces_[second];
                if (primary.type != database.attacker || secondary.type != database.secondary)
                    continue;
                const bool swapColors = primary.color == Color::Black;
                const Color expectedSecondary = database.secondaryColor == Color::White
                  ? primary.color : ~primary.color;
                if (secondary.color != expectedSecondary)
                    continue;
                bool scalarOk = true;
                for (int id : {whiteKing, blackKing, first, second}) {
                    const PieceState& item = position.pieces_[id];
                    scalarOk = scalarOk && !item.cooldown && !item.freezeCount &&
                               !item.action && !item.power;
                }
                const bool copycat = primary.type == PieceType::Copycat &&
                                     secondary.type == PieceType::CopycatClone;
                if (copycat)
                    scalarOk = scalarOk && primary.link == second && secondary.link == first;
                else
                    scalarOk = scalarOk && primary.link == Position::NoPiece &&
                               secondary.link == Position::NoPiece;
                if (!scalarOk)
                    continue;
                const Color side = swapColors ? ~position.sideToMove_ : position.sideToMove_;
                const int canonicalWhite = swapColors ? blackKing : whiteKing;
                const int canonicalBlack = swapColors ? whiteKing : blackKing;
                const bool identical = database.attacker == database.secondary &&
                                       database.secondaryColor == Color::White;
                const std::uint32_t index = copycat
                  ? encode(side, position.pieces_[canonicalWhite].square,
                           position.pieces_[canonicalBlack].square, primary.square)
                  : identical
                  ? encode_identical_four(side, position.pieces_[canonicalWhite].square,
                                          position.pieces_[canonicalBlack].square,
                                          primary.square, secondary.square)
                  : encode_four(side, position.pieces_[canonicalWhite].square,
                                position.pieces_[canonicalBlack].square,
                                primary.square, secondary.square);
                return database.at(index);
            }
        }
        return std::nullopt;
    }

    const int attacker = extras[0];
    const PieceState& extra = position.pieces_[attacker];
    if (extra.link != Position::NoPiece)
        return std::nullopt;
    const auto has_unrepresented_freeze = [&] {
        for (int id = 0; id < position.pieceCount_; ++id)
            if (position.pieces_[id].alive &&
                (position.pieces_[id].freezeCount || position.pieces_[id].action))
                return true;
        return false;
    };
    for (const Database& database : databases()) {
        if (database.secondary != PieceType::Count)
            continue;
        if (database.attacker != extra.type)
            continue;
        if (extra.type != PieceType::Penguin && has_unrepresented_freeze())
            continue;
        std::uint32_t substate = 0;
        switch (extra.type) {
        case PieceType::Berserker:
            if (extra.cooldown)
                continue;
            substate = std::min<std::uint32_t>(extra.power, 9);
            break;
        case PieceType::Ghost:
            if (extra.cooldown || extra.power)
                continue;
            substate = extra.visible ? 1 : 0;
            break;
        case PieceType::Sniper:
            if (extra.cooldown > 3 || extra.power)
                continue;
            substate = extra.cooldown;
            break;
        case PieceType::Prince:
            if (extra.cooldown || extra.power)
                continue;
            if (position.continuation_ == Continuation::PrinceSecondMove &&
                position.forcedPiece_ == attacker)
                substate = 1;
            else if (position.continuation_ != Continuation::None ||
                     position.forcedPiece_ != Position::NoPiece)
                continue;
            break;
        case PieceType::Pawn:
            if (extra.cooldown || extra.power ||
                position.continuation_ != Continuation::None ||
                position.forcedPiece_ != Position::NoPiece)
                continue;
            // With no second non-King model, en passant is not actionable and
            // is therefore an exact state equivalence after a double step.
            if (position.enPassantVictim_ != Position::NoPiece &&
                position.enPassantVictim_ != attacker)
                continue;
            substate = extra.moved ? 1 : 0;
            break;
        case PieceType::Penguin: {
            if (extra.cooldown > 5 || extra.power ||
                position.continuation_ != Continuation::None ||
                position.forcedPiece_ != Position::NoPiece)
                continue;
            Position expected = position;
            for (int id = 0; id < expected.pieceCount_; ++id) {
                expected.pieces_[id].freezeCount = 0;
                expected.pieces_[id].action = 0;
            }
            expected.apply_penguin_freeze(attacker);
            if (extra.action) {
                bool matches = extra.action == expected.pieces_[attacker].action;
                for (int id : {whiteKing, blackKing})
                    matches = matches && position.pieces_[id].freezeCount ==
                                           expected.pieces_[id].freezeCount;
                if (!matches)
                    continue;
            }
            else if (has_unrepresented_freeze())
                continue;
            substate = extra.cooldown * 2 + (extra.action ? 1 : 0);
            break;
        }
        default:
            if (extra.cooldown || extra.power || has_unrepresented_freeze() ||
                position.continuation_ != Continuation::None ||
                position.forcedPiece_ != Position::NoPiece)
                continue;
            break;
        }
        if (substate >= database.substates)
            continue;
        const bool swapColors = extra.color == Color::Black;
        const Color side = swapColors ? ~position.sideToMove_ : position.sideToMove_;
        const int canonicalWhite = swapColors ? blackKing : whiteKing;
        const int canonicalBlack = swapColors ? whiteKing : blackKing;
        const std::uint32_t index = encode(
          side, position.pieces_[canonicalWhite].square,
          position.pieces_[canonicalBlack].square, extra.square) * database.substates + substate;
        return database.at(index);
    }
    return std::nullopt;
}

}  // namespace Stockfish::Ultimate
