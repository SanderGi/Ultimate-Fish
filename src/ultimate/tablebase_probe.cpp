/* Ultimate Fish exact endgame tablebase probing, GPLv3 or later. */

#include "tablebase_probe.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
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

struct PackedStorage {
    std::string path;
    std::uint64_t wdlOffset = 0;
    std::uint32_t wdlBytes = 0;
    std::uint32_t dtwBytes = 0;
    std::uint32_t count = 0;
    mutable std::once_flag loadOnce;
    mutable std::vector<std::uint8_t> wdl;
    mutable std::vector<std::uint8_t> dtw;

    void load() const {
        std::call_once(loadOnce, [&] {
            std::ifstream stream(path, std::ios::binary);
            stream.seekg(static_cast<std::streamoff>(wdlOffset));
            wdl.resize(wdlBytes);
            dtw.resize(dtwBytes);
            stream.read(reinterpret_cast<char*>(wdl.data()), wdl.size());
            stream.read(reinterpret_cast<char*>(dtw.data()), dtw.size());
            if (!stream)
                throw std::runtime_error("truncated packed Ultimate tablebase: " + path);
            for (std::uint32_t index = 0; index < count; ++index) {
                const std::uint8_t value = (wdl[index / 4] >> ((index % 4) * 2)) & 3;
                if (value < static_cast<std::uint8_t>(TablebaseWdl::Win) ||
                    value > static_cast<std::uint8_t>(TablebaseWdl::Draw))
                    throw std::runtime_error("invalid packed Ultimate WDL value: " + path);
            }
        });
    }
};

struct Database {
    PieceType attacker = PieceType::Count;
    PieceType secondary = PieceType::Count;
    Color secondaryColor = Color::White;
    std::uint32_t substates = 1;
    std::uint32_t count = 0;
    std::vector<TablebaseResult> records;
    std::shared_ptr<PackedStorage> packed;
    std::vector<std::pair<std::uint32_t, std::uint16_t>> exceptions;

    TablebaseResult at(std::uint32_t index) const {
        if (packed) {
            packed->load();
            const auto wdl = static_cast<TablebaseWdl>(
              (packed->wdl[index / 4] >> ((index % 4) * 2)) & 3);
            std::uint16_t distance = packed->dtw[index];
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

std::uint32_t represented_substates(PieceType type) {
    switch (type) {
    case PieceType::Berserker: return 10;
    case PieceType::Ghost: return 2;
    case PieceType::Sniper: return 4;
    case PieceType::Prince: return 2;
    case PieceType::Checker: return 4;
    case PieceType::Pawn: return 2;
    case PieceType::Penguin: return 12;
    default: return 1;
    }
}

std::string materialize_shards(const std::string& path) {
    std::ifstream source(path, std::ios::binary);
    const std::array<char, 8> shardMagic{{'U','F','T','B','S','1','\0','\0'}};
    std::array<char, 8> prefix{};
    source.read(prefix.data(), prefix.size());
    if (!source || prefix != shardMagic)
        return path;
    source.clear();
    source.seekg(0);
    const std::string data((std::istreambuf_iterator<char>(source)),
                           std::istreambuf_iterator<char>());
    std::istringstream manifest(data, std::ios::in | std::ios::binary);
    std::array<char, 8> magic{};
    std::uint32_t version = 0, count = 0;
    std::uint64_t total = 0;
    manifest.read(magic.data(), magic.size());
    manifest.read(reinterpret_cast<char*>(&version), sizeof(version));
    manifest.read(reinterpret_cast<char*>(&count), sizeof(count));
    manifest.read(reinterpret_cast<char*>(&total), sizeof(total));
    if (!manifest || magic != shardMagic || version != 1 || !count)
        return {};
    struct Part { std::string name; std::uint64_t size; };
    std::vector<Part> parts;
    std::uint64_t summed = 0;
    for (std::uint32_t item = 0; item < count; ++item) {
        std::uint16_t nameLength = 0;
        std::uint64_t size = 0;
        std::array<char, 32> digest{};
        manifest.read(reinterpret_cast<char*>(&nameLength), sizeof(nameLength));
        manifest.read(reinterpret_cast<char*>(&size), sizeof(size));
        manifest.read(digest.data(), digest.size());
        std::string name(nameLength, '\0');
        manifest.read(name.data(), name.size());
        if (!manifest || name.empty() || std::filesystem::path(name).filename() != name)
            return {};
        parts.push_back({std::move(name), size});
        summed += size;
    }
    if (manifest.peek() != std::char_traits<char>::eof() || summed != total)
        return {};

    std::error_code error;
    const std::string key = std::filesystem::absolute(path, error).string() + data;
    const auto cache = std::filesystem::temp_directory_path(error) /
      ("ultimatefish-tb-" + std::to_string(std::hash<std::string>{}(key)) + ".uftb");
    if (!error && std::filesystem::file_size(cache, error) == total && !error)
        return cache.string();
    error.clear();
    const auto temporary = cache.string() + ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output)
        return {};
    const auto directory = std::filesystem::path(path).parent_path();
    std::array<char, 1 << 20> buffer{};
    for (const Part& part : parts) {
        std::ifstream input(directory / part.name, std::ios::binary);
        std::uint64_t copied = 0;
        while (input && copied < part.size) {
            const std::size_t wanted = static_cast<std::size_t>(
              std::min<std::uint64_t>(buffer.size(), part.size - copied));
            input.read(buffer.data(), wanted);
            const std::streamsize received = input.gcount();
            if (received <= 0)
                break;
            output.write(buffer.data(), received);
            copied += static_cast<std::uint64_t>(received);
        }
        if (copied != part.size)
            return {};
    }
    output.close();
    std::filesystem::rename(temporary, cache, error);
    if (error)
        return {};
    return cache.string();
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
        const std::string logicalPath = materialize_shards(path);
        std::ifstream stream(logicalPath, std::ios::binary);
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
        if (!stream || magic != expected || (version < 2 || version > 6) ||
            !substates || (version < 5 && count != StateCount * substates) ||
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
            if (version >= 5) {
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
                const bool copycat = database.attacker == PieceType::Copycat &&
                                     database.secondary == PieceType::CopycatClone;
                const bool identical = database.attacker == database.secondary &&
                                       database.secondaryColor == Color::White;
                const std::uint64_t placementCount = copycat ? StateCount
                  : identical ? IdenticalFourStateCount : FourStateCount;
                if (std::uint64_t(count) != placementCount * substates) {
                    database.count = 0;
                    continue;
                }
            }
            if (version >= 6) {
                std::uint64_t exactEdges = 0;
                stream.read(reinterpret_cast<char*>(&exactEdges), sizeof(exactEdges));
                if (exactEdges <= std::numeric_limits<std::uint32_t>::max()) {
                    database.count = 0;
                    continue;
                }
            }
            if (!stream || wdlBytes != (count + 3) / 4 || dtwBytes != count) {
                database.count = 0;
                continue;
            }
            const std::streamoff planeOffset = stream.tellg();
            if (planeOffset < 0) {
                database.count = 0;
                continue;
            }
            database.packed = std::make_shared<PackedStorage>();
            database.packed->path = logicalPath;
            database.packed->wdlOffset = static_cast<std::uint64_t>(planeOffset);
            database.packed->wdlBytes = wdlBytes;
            database.packed->dtwBytes = dtwBytes;
            database.packed->count = count;
            const std::uint64_t exceptionOffset =
              database.packed->wdlOffset + wdlBytes + dtwBytes;
            stream.seekg(static_cast<std::streamoff>(exceptionOffset));
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
            std::error_code sizeError;
            const std::uint64_t expectedSize = exceptionOffset +
              std::uint64_t(exceptionCount) * (sizeof(std::uint32_t) + sizeof(std::uint16_t));
            if (std::filesystem::file_size(logicalPath, sizeError) != expectedSize || sizeError)
                database.count = 0;
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
            (database.records.size() == count || database.packed))
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
        for (const Database& database : databases()) {
            if (database.secondary == PieceType::Count)
                continue;
            for (int order = 0; order < 2; ++order) {
                const int first = extras[order];
                const int second = extras[1 - order];
                const PieceState& primary = position.pieces_[first];
                const PieceState& secondary = position.pieces_[second];
                const auto typeMatches = [](PieceType represented, PieceType actual) {
                    return represented == PieceType::Checker
                      ? actual == PieceType::Checker || actual == PieceType::CheckerKing
                      : represented == actual;
                };
                if (!typeMatches(database.attacker, primary.type) ||
                    !typeMatches(database.secondary, secondary.type))
                    continue;
                const bool swapColors = primary.color == Color::Black;
                const Color expectedSecondary = database.secondaryColor == Color::White
                  ? primary.color : ~primary.color;
                if (secondary.color != expectedSecondary)
                    continue;
                const bool copycat = primary.type == PieceType::Copycat &&
                                     secondary.type == PieceType::CopycatClone;
                if (copycat ? !(primary.link == second && secondary.link == first)
                            : !(primary.link == Position::NoPiece &&
                                secondary.link == Position::NoPiece))
                    continue;
                if (copycat && (primary.cooldown || primary.power ||
                                secondary.cooldown || secondary.power))
                    continue;
                bool continuationMatched = position.continuation_ == Continuation::None &&
                                           position.forcedPiece_ == Position::NoPiece;
                const auto extractSubstate = [&](int id, PieceType represented)
                  -> std::optional<std::uint32_t> {
                    const PieceState& item = position.pieces_[id];
                    if (!typeMatches(represented, item.type) || item.link != Position::NoPiece)
                        return std::nullopt;
                    switch (represented) {
                    case PieceType::Berserker:
                        if (item.cooldown || item.action) return std::nullopt;
                        return std::min<std::uint32_t>(item.power, 9);
                    case PieceType::Ghost:
                        if (item.cooldown || item.power || item.action) return std::nullopt;
                        return item.visible ? 1u : 0u;
                    case PieceType::Sniper:
                        if (item.cooldown > 3 || item.power || item.action) return std::nullopt;
                        return item.cooldown;
                    case PieceType::Prince: {
                        if (item.cooldown || item.power || item.action) return std::nullopt;
                        const bool forced = position.continuation_ == Continuation::PrinceSecondMove &&
                                            position.forcedPiece_ == id;
                        continuationMatched = continuationMatched || forced;
                        return forced ? 1u : 0u;
                    }
                    case PieceType::Checker: {
                        if (item.cooldown || item.power || item.action) return std::nullopt;
                        const bool forced = position.continuation_ == Continuation::CheckerJump &&
                                            position.forcedPiece_ == id;
                        continuationMatched = continuationMatched || forced;
                        return (item.type == PieceType::CheckerKing ? 2u : 0u) +
                               (forced ? 1u : 0u);
                    }
                    case PieceType::Pawn:
                        if (item.cooldown || item.power || item.action) return std::nullopt;
                        return item.moved ? 1u : 0u;
                    case PieceType::Penguin:
                        if (item.cooldown > 5 || item.power) return std::nullopt;
                        return item.cooldown * 2 + (item.action ? 1u : 0u);
                    default:
                        if (item.cooldown || item.power || item.action) return std::nullopt;
                        return 0u;
                    }
                };
                std::optional<std::uint32_t> primarySubstate = copycat
                  ? std::optional<std::uint32_t>(0) : extractSubstate(first, database.attacker);
                std::optional<std::uint32_t> secondarySubstate = copycat
                  ? std::optional<std::uint32_t>(0) : extractSubstate(second, database.secondary);
                if (!primarySubstate || !secondarySubstate || !continuationMatched)
                    continue;
                if (position.enPassantVictim_ != Position::NoPiece) {
                    const int victim = position.enPassantVictim_;
                    if ((victim != first && victim != second) ||
                        position.pieces_[victim].type != PieceType::Pawn)
                        continue;
                }
                else if (position.enPassantSquare_ != Position::NoSquare)
                    continue;

                std::array<int, 2> representedPenguins{Position::NoPiece, Position::NoPiece};
                int penguinCount = 0;
                if (database.attacker == PieceType::Penguin)
                    representedPenguins[penguinCount++] = first;
                if (database.secondary == PieceType::Penguin)
                    representedPenguins[penguinCount++] = second;
                if (penguinCount) {
                    Position expected = position;
                    for (int id = 0; id < expected.pieceCount_; ++id) {
                        expected.pieces_[id].freezeCount = 0;
                        expected.pieces_[id].action = 0;
                    }
                    for (int slot = 0; slot < penguinCount; ++slot) {
                        const int id = representedPenguins[slot];
                        if (position.pieces_[id].action)
                            expected.apply_penguin_freeze(id);
                    }
                    bool freezeMatches = true;
                    for (int id = 0; id < position.pieceCount_; ++id)
                        if (position.pieces_[id].alive)
                            freezeMatches = freezeMatches &&
                              position.pieces_[id].freezeCount == expected.pieces_[id].freezeCount &&
                              position.pieces_[id].action == expected.pieces_[id].action;
                    if (!freezeMatches)
                        continue;
                }
                else {
                    bool hasFreeze = false;
                    for (int id = 0; id < position.pieceCount_; ++id)
                        hasFreeze = hasFreeze || (position.pieces_[id].alive &&
                          (position.pieces_[id].freezeCount || position.pieces_[id].action));
                    if (hasFreeze)
                        continue;
                }
                const Color side = swapColors ? ~position.sideToMove_ : position.sideToMove_;
                const int canonicalWhite = swapColors ? blackKing : whiteKing;
                const int canonicalBlack = swapColors ? whiteKing : blackKing;
                const bool identical = database.attacker == database.secondary &&
                                       database.secondaryColor == Color::White;
                std::uint32_t placement = 0;
                if (copycat)
                    placement = encode(side, position.pieces_[canonicalWhite].square,
                                       position.pieces_[canonicalBlack].square, primary.square);
                else if (identical) {
                    std::uint8_t wk = position.pieces_[canonicalWhite].square;
                    std::uint8_t bk = position.pieces_[canonicalBlack].square;
                    std::uint8_t firstSquare = primary.square;
                    std::uint8_t secondSquare = secondary.square;
                    if (wk % 8 >= 4) {
                        wk = reflect_horizontal(wk);
                        bk = reflect_horizontal(bk);
                        firstSquare = reflect_horizontal(firstSquare);
                        secondSquare = reflect_horizontal(secondSquare);
                    }
                    if (rank_excluding(firstSquare, {wk, bk}) >
                        rank_excluding(secondSquare, {wk, bk}))
                        std::swap(*primarySubstate, *secondarySubstate);
                    placement = encode_identical_four(
                      side, position.pieces_[canonicalWhite].square,
                      position.pieces_[canonicalBlack].square, primary.square, secondary.square);
                }
                else placement = encode_four(
                  side, position.pieces_[canonicalWhite].square,
                  position.pieces_[canonicalBlack].square, primary.square, secondary.square);
                const std::uint32_t secondaryFactor = represented_substates(database.secondary);
                if (*primarySubstate >= represented_substates(database.attacker) ||
                    *secondarySubstate >= secondaryFactor)
                    continue;
                const std::uint64_t index64 = std::uint64_t(placement) * database.substates +
                  *primarySubstate * secondaryFactor + *secondarySubstate;
                if (index64 >= database.count)
                    continue;
                const std::uint32_t index = static_cast<std::uint32_t>(index64);
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
