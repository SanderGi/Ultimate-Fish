/* Ultimate Fish exact endgame tablebase probing, GPLv3 or later. */

#include "tablebase_probe.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace Stockfish::Ultimate {
namespace {

constexpr std::uint32_t SquareCount = Position::BoardSquares;
constexpr std::uint32_t StateCount =
  2 * SquareCount * (SquareCount - 1) * (SquareCount - 2);

struct Database {
    PieceType attacker = PieceType::Count;
    std::vector<TablebaseResult> records;
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

std::vector<std::string> paths() {
    if (const char* configured = std::getenv("ULTIMATE_TABLEBASE_PATH")) {
        std::vector<std::string> result;
        std::string text(configured);
        std::size_t begin = 0;
        while (begin <= text.size()) {
            const std::size_t end = text.find(':', begin);
            const std::string path = text.substr(begin, end - begin);
            if (!path.empty())
                result.push_back(path);
            if (end == std::string::npos)
                break;
            begin = end + 1;
        }
        return result;
    }
    return {
      "tablebases/krk.uftb", "tablebases/kqk.uftb",
      "tablebases/kninjak.uftb", "tablebases/kdragonk.uftb",
      "../tablebases/krk.uftb", "../tablebases/kqk.uftb",
      "../tablebases/kninjak.uftb", "../tablebases/kdragonk.uftb",
    };
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
        if (!stream || magic != expected || version != 2 || count != StateCount ||
            piece >= static_cast<std::uint32_t>(PieceType::Count))
            continue;
        Database database;
        database.attacker = static_cast<PieceType>(piece);
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
        if (stream && database.records.size() == count)
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
    if (position.continuation_ != Continuation::None ||
        position.forcedPiece_ != Position::NoPiece ||
        position.enPassantSquare_ != Position::NoSquare ||
        position.enPassantVictim_ != Position::NoPiece ||
        position.forcedTimeoutWinner_ >= 0)
        return std::nullopt;

    int whiteKing = Position::NoPiece;
    int blackKing = Position::NoPiece;
    int attacker = Position::NoPiece;
    int alive = 0;
    for (int id = 0; id < position.pieceCount_; ++id) {
        const PieceState& piece = position.pieces_[id];
        if (!piece.alive)
            continue;
        if (!piece.onBoard || piece.cooldown || piece.freezeCount || piece.action ||
            piece.power || piece.link != Position::NoPiece ||
            piece.host != Position::NoPiece || !piece.moved)
            return std::nullopt;
        ++alive;
        if (piece.type == PieceType::King)
            (piece.color == Color::White ? whiteKing : blackKing) = id;
        else if (attacker == Position::NoPiece)
            attacker = id;
        else
            return std::nullopt;
    }
    if (alive != 3 || whiteKing == Position::NoPiece ||
        blackKing == Position::NoPiece || attacker == Position::NoPiece)
        return std::nullopt;

    const PieceState& extra = position.pieces_[attacker];
    for (const Database& database : databases()) {
        if (database.attacker != extra.type)
            continue;
        const bool swapColors = extra.color == Color::Black;
        const Color side = swapColors ? ~position.sideToMove_ : position.sideToMove_;
        const int canonicalWhite = swapColors ? blackKing : whiteKing;
        const int canonicalBlack = swapColors ? whiteKing : blackKing;
        const std::uint32_t index = encode(
          side, position.pieces_[canonicalWhite].square,
          position.pieces_[canonicalBlack].square, extra.square);
        return database.records[index];
    }
    return std::nullopt;
}

}  // namespace Stockfish::Ultimate
