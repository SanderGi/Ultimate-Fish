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
constexpr std::uint32_t CompoundCopycatStateCount =
  2 * SquareCount * (SquareCount - 1) * (SquareCount - 2) * (SquareCount - 3);
constexpr std::uint32_t IdenticalCompoundCopycatStateCount =
  CompoundCopycatStateCount / 2;
constexpr std::uint64_t GiantAnchorV2Tag = 0x32474e4149474655ULL;
constexpr std::uint64_t TrackedGhostV1Tag = 0x3154534f48474655ULL;
constexpr std::uint64_t AngelGraphV1Tag = 0x314c45474e414655ULL;
constexpr std::uint64_t AngelGiantGraphV1Tag = 0x314741474e414655ULL;
constexpr std::uint64_t LinkedCopycatPairV1Tag = 0x314b4e4c43434655ULL;
constexpr std::uint64_t AngelCopycatGraphV1Tag = 0x3152504343414655ULL;

bool compatible_codec(std::uint32_t version, PieceType primary,
                      PieceType secondary, std::uint64_t codecTag) {
    if (version < 2 || version > 10)
        return false;
    const bool angel = primary == PieceType::Angel ||
      secondary == PieceType::Angel;
    const bool foldedGiant = secondary != PieceType::Count &&
      (primary == PieceType::Giant || secondary == PieceType::Giant);
    if (version == 10)
        return (primary == PieceType::Copycat &&
                secondary == PieceType::CopycatClone &&
                codecTag == LinkedCopycatPairV1Tag) ||
               (primary == PieceType::Copycat &&
                secondary == PieceType::Angel &&
                codecTag == AngelCopycatGraphV1Tag);
    if (version == 9)
        return angel && !(primary == PieceType::Angel &&
                          secondary == PieceType::Angel) &&
          codecTag == (foldedGiant ? AngelGiantGraphV1Tag
                                   : AngelGraphV1Tag);
    if (angel)
        return false;
    if (version == 8)
        return primary == PieceType::Ghost && secondary == PieceType::Count &&
               codecTag == TrackedGhostV1Tag;
    // v7 is reserved for a folded four-model Giant payload whose horizontal
    // symmetry reflects the lower-left 2x2 anchor. Earlier versions cannot
    // authenticate that codec and are rejected before they are indexed.
    return version == 7 ? foldedGiant && codecTag == GiantAnchorV2Tag
                        : !foldedGiant;
}

bool compatible_secondary_color(std::uint32_t version, PieceType primary,
                                PieceType secondary, Color secondaryColor) {
    // AngelGraphV1 represents a Copycat compound only when the Angel is the
    // opponent. A same-team Angel may attach to either linked half, and a hit
    // on the other half can leave a live off-board orphan; that larger graph
    // is deliberately not authenticated by this codec.
    if (version == 10)
        return primary == PieceType::Copycat &&
          ((secondary == PieceType::CopycatClone &&
            secondaryColor == Color::White) ||
           (secondary == PieceType::Angel &&
            secondaryColor == Color::White));
    return version != 9 || primary != PieceType::Copycat ||
           secondary != PieceType::Angel || secondaryColor == Color::Black;
}

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
    bool trackedGhost = false;
    bool linkedCopycatPair = false;
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

std::uint8_t reflect_giant_anchor_horizontal(std::uint8_t square) {
    const int file = square % 8;
    return file == 7 ? square
                     : static_cast<std::uint8_t>((square / 8) * 8 + 6 - file);
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
                          std::uint8_t second, bool firstGiant,
                          bool secondGiant) {
    if (whiteKing % 8 >= 4) {
        whiteKing = reflect_horizontal(whiteKing);
        blackKing = reflect_horizontal(blackKing);
        first = firstGiant ? reflect_giant_anchor_horizontal(first)
                           : reflect_horizontal(first);
        second = secondGiant ? reflect_giant_anchor_horizontal(second)
                             : reflect_horizontal(second);
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

std::uint32_t encode_compound_copycat(Color side, std::uint8_t whiteKing,
                                      std::uint8_t blackKing,
                                      std::uint8_t copycat,
                                      std::uint8_t secondary) {
    const std::uint32_t blackRank = rank_excluding(blackKing, {whiteKing});
    const std::uint32_t copycatRank = rank_excluding(
      copycat, {whiteKing, blackKing});
    const std::uint32_t secondaryRank = rank_excluding(
      secondary, {whiteKing, blackKing, copycat});
    return ((((static_cast<std::uint32_t>(side) * SquareCount + whiteKing)
               * (SquareCount - 1) + blackRank)
              * (SquareCount - 2) + copycatRank)
             * (SquareCount - 3) + secondaryRank);
}

std::uint32_t encode_identical_compound_copycat(
  Color side, std::uint8_t whiteKing, std::uint8_t blackKing,
  std::uint8_t first, std::uint8_t second) {
    const std::uint32_t blackRank = rank_excluding(blackKing, {whiteKing});
    std::uint32_t firstRank = rank_excluding(first, {whiteKing, blackKing});
    std::uint32_t secondRank = rank_excluding(second, {whiteKing, blackKing});
    if (firstRank > secondRank)
        std::swap(firstRank, secondRank);
    constexpr std::uint32_t remaining = SquareCount - 2;
    constexpr std::uint32_t pairs = remaining * (remaining - 1) / 2;
    const std::uint32_t pairRank =
      firstRank * (2 * remaining - firstRank - 1) / 2 +
      secondRank - firstRank - 1;
    return ((static_cast<std::uint32_t>(side) * SquareCount + whiteKing) *
            (SquareCount - 1) + blackRank) * pairs + pairRank;
}

std::uint32_t encode_identical_four(Color side, std::uint8_t whiteKing,
                                    std::uint8_t blackKing, std::uint8_t first,
                                    std::uint8_t second, bool giant) {
    if (whiteKing % 8 >= 4) {
        whiteKing = reflect_horizontal(whiteKing);
        blackKing = reflect_horizontal(blackKing);
        first = giant ? reflect_giant_anchor_horizontal(first)
                      : reflect_horizontal(first);
        second = giant ? reflect_giant_anchor_horizontal(second)
                       : reflect_horizontal(second);
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

std::uint32_t represented_substates(PieceType type, bool fourModels = false,
                                    PieceType other = PieceType::Count,
                                    Color typeColor = Color::White,
                                    Color otherColor = Color::White) {
    switch (type) {
    case PieceType::Berserker: return 10;
    case PieceType::Ghost: return 2;
    case PieceType::Devil: return 4;
    case PieceType::Sniper: return 4;
    case PieceType::Prince: return 2;
    case PieceType::Checker: return 4;
    case PieceType::Pawn: return 2;
    case PieceType::Penguin:
        return !fourModels || other == PieceType::Penguin ? 4 : 8;
    case PieceType::Angel:
        return 2 + (fourModels && typeColor == otherColor
          ? other == PieceType::Copycat ? 2 : 1 : 0);
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
        if (!stream || magic != expected || (version < 2 || version > 10) ||
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
                if (secondary > static_cast<std::uint32_t>(PieceType::Count) ||
                    (secondary == static_cast<std::uint32_t>(PieceType::Count) &&
                     version != 8 && version != 9) ||
                    secondaryColor > static_cast<std::uint32_t>(Color::Black)) {
                    database.count = 0;
                    continue;
                }
                database.secondary = static_cast<PieceType>(secondary);
                database.secondaryColor = static_cast<Color>(secondaryColor);
                const bool copycat = database.attacker == PieceType::Copycat &&
                                     database.secondary == PieceType::CopycatClone;
                const bool compoundCopycat = database.attacker == PieceType::Copycat &&
                                             database.secondary != PieceType::CopycatClone;
                const bool identicalCompoundCopycats = compoundCopycat &&
                  database.secondary == PieceType::Copycat &&
                  database.secondaryColor == Color::White;
                const bool identical = database.attacker == database.secondary &&
                                       database.secondaryColor == Color::White;
                const std::uint64_t placementCount =
                  database.secondary == PieceType::Count ? StateCount
                  : copycat ? (version == 10 ? FourStateCount : StateCount)
                  : identicalCompoundCopycats ? IdenticalCompoundCopycatStateCount
                  : compoundCopycat ? CompoundCopycatStateCount
                  : identical ? IdenticalFourStateCount : FourStateCount;
                if (std::uint64_t(count) != placementCount * substates) {
                    database.count = 0;
                    continue;
                }
            }
            if (version >= 6) {
                std::uint64_t exactEdges = 0;
                stream.read(reinterpret_cast<char*>(&exactEdges), sizeof(exactEdges));
                if (version == 6 &&
                    exactEdges <= std::numeric_limits<std::uint32_t>::max()) {
                    database.count = 0;
                    continue;
                }
            }
            std::uint64_t codecTag = 0;
            if (version >= 7)
                stream.read(reinterpret_cast<char*>(&codecTag), sizeof(codecTag));
            database.trackedGhost = codecTag == TrackedGhostV1Tag;
            database.linkedCopycatPair =
              codecTag == LinkedCopycatPairV1Tag;
            if (!compatible_codec(version, database.attacker,
                                  database.secondary, codecTag) ||
                !compatible_secondary_color(
                  version, database.attacker, database.secondary,
                  database.secondaryColor)) {
                database.count = 0;
                continue;
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
            if (!compatible_codec(version, database.attacker,
                                  PieceType::Count, 0))
                continue;
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

void TablebaseProbe::preload() { (void) databases(); }

bool TablebaseProbe::uses_compatible_codec(const std::string& path) {
    const std::string logicalPath = materialize_shards(path);
    std::ifstream stream(logicalPath, std::ios::binary);
    if (!stream)
        return false;
    std::array<char, 8> magic{};
    std::uint32_t version = 0, piece = 0, count = 0, edges = 0;
    stream.read(magic.data(), magic.size());
    stream.read(reinterpret_cast<char*>(&version), sizeof(version));
    stream.read(reinterpret_cast<char*>(&piece), sizeof(piece));
    stream.read(reinterpret_cast<char*>(&count), sizeof(count));
    stream.read(reinterpret_cast<char*>(&edges), sizeof(edges));
    const std::array<char, 8> expected{{'U','F','T','B','1','\0','\0','\0'}};
    if (!stream || magic != expected || version < 2 || version > 10 ||
        piece >= static_cast<std::uint32_t>(PieceType::Count))
        return false;
    std::uint32_t substates = 1;
    if (version >= 3)
        stream.read(reinterpret_cast<char*>(&substates), sizeof(substates));
    PieceType secondary = PieceType::Count;
    Color secondaryColor = Color::White;
    std::uint64_t codecTag = 0;
    if (version >= 4) {
        std::uint32_t wdlBytes = 0, dtwBytes = 0, exceptionCount = 0;
        stream.read(reinterpret_cast<char*>(&wdlBytes), sizeof(wdlBytes));
        stream.read(reinterpret_cast<char*>(&dtwBytes), sizeof(dtwBytes));
        stream.read(reinterpret_cast<char*>(&exceptionCount), sizeof(exceptionCount));
        if (version >= 5) {
            std::uint32_t encodedSecondary = 0, encodedSecondaryColor = 0;
            stream.read(reinterpret_cast<char*>(&encodedSecondary), sizeof(encodedSecondary));
            stream.read(reinterpret_cast<char*>(&encodedSecondaryColor),
                        sizeof(encodedSecondaryColor));
            if (encodedSecondary > static_cast<std::uint32_t>(PieceType::Count) ||
                (encodedSecondary == static_cast<std::uint32_t>(PieceType::Count) &&
                 version != 8 && version != 9) ||
                encodedSecondaryColor > static_cast<std::uint32_t>(Color::Black))
                return false;
            secondary = static_cast<PieceType>(encodedSecondary);
            secondaryColor = static_cast<Color>(encodedSecondaryColor);
        }
        if (version >= 6) {
            std::uint64_t exactEdges = 0;
            stream.read(reinterpret_cast<char*>(&exactEdges), sizeof(exactEdges));
            if (version == 6 &&
                exactEdges <= std::numeric_limits<std::uint32_t>::max())
                return false;
        }
        if (version >= 7)
            stream.read(reinterpret_cast<char*>(&codecTag), sizeof(codecTag));
        if (!stream || !substates || wdlBytes != (count + 3) / 4 ||
            dtwBytes != count)
            return false;
    }
    return stream && compatible_codec(version, static_cast<PieceType>(piece),
                                      secondary, codecTag) &&
           compatible_secondary_color(
             version, static_cast<PieceType>(piece), secondary,
             secondaryColor);
}

std::optional<TablebaseResult> TablebaseProbe::probe(const Position& position) {
    if (position.forcedTimeoutWinner_ >= 0)
        return std::nullopt;

    // Generated tables represent the closed no-castling state class. Most
    // pieces record whether they have moved even though that bit changes no
    // rule. It is relevant only to a Pawn's double step and to the joint
    // King/Jester + Rook castling privilege. Castling can use either team's
    // Rook, so fail closed whenever both unmoved halves coexist; otherwise an
    // unmoved non-Pawn bit is semantically inert and maps to the stored class.
    bool hasUnmovedRoyal = false;
    bool hasUnmovedRook = false;
    for (int id = 0; id < position.pieceCount_; ++id) {
        const PieceState& piece = position.pieces_[id];
        if (!piece.alive || !piece.onBoard || piece.moved)
            continue;
        hasUnmovedRoyal = hasUnmovedRoyal ||
          piece.type == PieceType::King || piece.type == PieceType::Jester;
        hasUnmovedRook = hasUnmovedRook || piece.type == PieceType::Rook;
    }
    if (hasUnmovedRoyal && hasUnmovedRook)
        return std::nullopt;

    int whiteKing = Position::NoPiece;
    int blackKing = Position::NoPiece;
    std::array<int, 4> extras{Position::NoPiece, Position::NoPiece,
                              Position::NoPiece, Position::NoPiece};
    int extraCount = 0;
    int liveHalos = 0;
    int attachedAngels = 0;
    for (int id = 0; id < position.pieceCount_; ++id) {
        const PieceState& piece = position.pieces_[id];
        if (!piece.alive)
            continue;
        if ((!piece.onBoard && piece.type != PieceType::Angel) ||
            (piece.host != Position::NoPiece &&
             piece.type != PieceType::Angel) ||
            (!piece.visible && piece.type != PieceType::Ghost))
            return std::nullopt;
        if (piece.type == PieceType::Halo) {
            ++liveHalos;
            continue;
        }
        if (piece.type == PieceType::King) {
            if (!piece.onBoard || piece.host != Position::NoPiece)
                return std::nullopt;
            (piece.color == Color::White ? whiteKing : blackKing) = id;
        }
        else if (extraCount < static_cast<int>(extras.size()))
            extras[extraCount++] = id;
        else return std::nullopt;
    }
    if (extraCount < 1 || extraCount > 4 ||
        whiteKing == Position::NoPiece || blackKing == Position::NoPiece)
        return std::nullopt;
    for (int slot = 0; slot < extraCount; ++slot) {
        const int id = extras[slot];
        const PieceState& angel = position.pieces_[id];
        if (angel.type != PieceType::Angel)
            continue;
        if (angel.onBoard) {
            if (angel.link != Position::NoPiece ||
                angel.host != Position::NoPiece || angel.attachmentOrder)
                return std::nullopt;
            continue;
        }
        ++attachedAngels;
        if (angel.link < 0 || angel.link >= position.pieceCount_ ||
            angel.host < 0 || angel.host >= position.pieceCount_ ||
            !angel.attachmentOrder || angel.freezeCount ||
            !position.pieces_[angel.link].alive ||
            !position.pieces_[angel.link].onBoard ||
            position.pieces_[angel.link].type != PieceType::Halo ||
            position.pieces_[angel.link].color != angel.color ||
            position.pieces_[angel.link].link != id ||
            !position.pieces_[angel.host].alive ||
            !position.pieces_[angel.host].onBoard ||
            position.pieces_[angel.host].type == PieceType::Halo ||
            position.pieces_[angel.host].color != angel.color ||
            angel.square != position.pieces_[angel.host].square)
            return std::nullopt;
    }
    if (liveHalos != attachedAngels)
        return std::nullopt;
    for (int id = 0; id < position.pieceCount_; ++id) {
        const PieceState& halo = position.pieces_[id];
        if (!halo.alive || halo.type != PieceType::Halo)
            continue;
        if (halo.link < 0 || halo.link >= position.pieceCount_ ||
            !position.pieces_[halo.link].alive ||
            position.pieces_[halo.link].type != PieceType::Angel ||
            position.pieces_[halo.link].onBoard ||
            position.pieces_[halo.link].link != id)
            return std::nullopt;
    }

    // A native Pawn can use its first-move two-step directly onto the
    // promotion rank.  The resulting Queen temporarily remains the nominal
    // en-passant victim.  If no legal Pawn can use that marker, it changes no
    // move or outcome and is exactly representable by the ordinary lower
    // Queen table.  Keep the marker distinct whenever a real capture exists,
    // which is essential for the later two-Pawn dependency wave.
    const auto actionableEnPassant = [&] {
        if (position.enPassantSquare_ == Position::NoSquare ||
            position.enPassantVictim_ == Position::NoPiece)
            return false;
        for (const Move& move : position.legal_moves()) {
            const int actor = move.from >= 0 && move.from < Position::BoardSquares
              ? position.board_[move.from] : Position::NoPiece;
            if (move.to == position.enPassantSquare_ &&
                actor != Position::NoPiece &&
                position.pieces_[actor].type == PieceType::Pawn)
                return true;
        }
        return false;
    };

    // Once no Pawn has a legal capture onto the stored square, the marker is
    // observationally and strategically inert. Normalize it before material
    // dispatch so every codec gets the same exact equivalence; the older
    // per-codec checks accidentally rejected promoted Jester/Queen children
    // even though their legal move sets were identical to the stored class.
    if (position.enPassantSquare_ != Position::NoSquare &&
        !actionableEnPassant()) {
        Position normalized = position;
        normalized.enPassantSquare_ = Position::NoSquare;
        normalized.enPassantVictim_ = Position::NoPiece;
        return probe(normalized);
    }

    // A promoted two-step victim with a genuinely available en-passant
    // capture is not a persistent tablebase state: every legal reply clears
    // the marker.  Resolve that one-ply frontier exactly from the ordinary
    // lower-material tables instead of either discarding the legal capture or
    // requiring a separate transient-state file.
    if (position.enPassantVictim_ != Position::NoPiece &&
        position.enPassantVictim_ < position.pieceCount_ &&
        position.pieces_[position.enPassantVictim_].alive &&
        position.pieces_[position.enPassantVictim_].type != PieceType::Pawn &&
        actionableEnPassant()) {
        const auto moves = position.legal_moves();
        if (moves.empty()) {
            const auto winner = position.winner();
            return TablebaseResult{
              winner && *winner != position.sideToMove_
                ? TablebaseWdl::Loss : TablebaseWdl::Draw,
              0};
        }

        bool hasDraw = false;
        bool hasWin = false;
        std::uint16_t shortestWin = std::numeric_limits<std::uint16_t>::max();
        std::uint16_t longestLoss = 0;
        for (const Move& move : moves) {
            Position child = position;
            if (!child.apply_move_unchecked(move))
                return std::nullopt;

            TablebaseWdl outcome = TablebaseWdl::Draw;
            std::uint16_t distance = 0;
            if (child.forced_timeout_winner() ||
                !child.has_real_king(Color::White) ||
                !child.has_real_king(Color::Black) ||
                !child.is_checkmate_possible()) {
                const auto winner = child.winner();
                if (winner)
                    outcome = *winner == position.sideToMove_
                      ? TablebaseWdl::Win : TablebaseWdl::Loss;
            }
            else {
                const auto result = probe(child);
                if (!result)
                    return std::nullopt;
                const bool sameSide = child.sideToMove_ == position.sideToMove_;
                outcome = sameSide || result->wdl == TablebaseWdl::Draw
                  ? result->wdl
                  : result->wdl == TablebaseWdl::Win
                      ? TablebaseWdl::Loss : TablebaseWdl::Win;
                distance = result->dtw;
            }

            if (outcome == TablebaseWdl::Win) {
                hasWin = true;
                shortestWin = std::min<std::uint16_t>(
                  shortestWin, static_cast<std::uint16_t>(distance + 1));
            }
            else if (outcome == TablebaseWdl::Draw)
                hasDraw = true;
            else
                longestLoss = std::max<std::uint16_t>(
                  longestLoss, static_cast<std::uint16_t>(distance + 1));
        }
        if (hasWin)
            return TablebaseResult{TablebaseWdl::Win, shortestWin};
        if (hasDraw)
            return TablebaseResult{TablebaseWdl::Draw, 0};
        return TablebaseResult{TablebaseWdl::Loss, longestLoss};
    }

    const auto typeMatches = [](PieceType represented, PieceType actual) {
        return represented == PieceType::Checker
          ? actual == PieceType::Checker || actual == PieceType::CheckerKing
          : represented == actual;
    };
    const auto angelSubstate = [&](int angel, int other)
      -> std::optional<std::uint32_t> {
        if (angel < 0 || angel >= position.pieceCount_)
            return std::nullopt;
        const PieceState& item = position.pieces_[angel];
        if (!item.alive || item.type != PieceType::Angel || item.action ||
            item.cooldown || item.power || !item.visible ||
            item.parasiteTracked)
            return std::nullopt;
        if (item.onBoard)
            return item.link == Position::NoPiece &&
                   item.host == Position::NoPiece && !item.attachmentOrder
              ? std::optional<std::uint32_t>(0) : std::nullopt;
        if (item.link < 0 || item.link >= position.pieceCount_ ||
            item.host < 0 || item.host >= position.pieceCount_ ||
            !item.attachmentOrder || item.freezeCount)
            return std::nullopt;
        const PieceState& halo = position.pieces_[item.link];
        const PieceState& host = position.pieces_[item.host];
        if (!halo.alive || !halo.onBoard || halo.type != PieceType::Halo ||
            halo.color != item.color || halo.link != angel ||
            halo.host != Position::NoPiece || !host.alive || !host.onBoard ||
            host.type == PieceType::Halo || host.color != item.color ||
            item.square != host.square)
            return std::nullopt;
        const int ownKing = item.color == Color::White ? whiteKing : blackKing;
        if (item.host == ownKing)
            return 1;
        if (other != Position::NoPiece && item.host == other &&
            position.pieces_[other].color == item.color)
            return 2;
        if (other != Position::NoPiece && other >= 0 &&
            other < position.pieceCount_ &&
            position.pieces_[other].type == PieceType::Copycat) {
            const int clone = position.pieces_[other].link;
            if (clone != Position::NoPiece && clone >= 0 &&
                clone < position.pieceCount_ && item.host == clone &&
                position.pieces_[clone].alive &&
                position.pieces_[clone].type == PieceType::CopycatClone &&
                position.pieces_[clone].color == item.color)
                return 3;
        }
        return std::nullopt;
    };
    const auto boardProxy = [&](int id, PieceType represented, int other) {
        if (represented != PieceType::Angel)
            return id;
        const auto substate = angelSubstate(id, other);
        return !substate ? Position::NoPiece
          : *substate ? static_cast<int>(position.pieces_[id].link) : id;
    };
    const auto slotSquare = [&](int id, PieceType represented, int other)
      -> std::optional<std::uint8_t> {
        const int proxy = boardProxy(id, represented, other);
        return proxy == Position::NoPiece
          ? std::nullopt
          : std::optional<std::uint8_t>(position.pieces_[proxy].square);
    };
    constexpr std::array<std::array<int, 2>, 8> penguinDirections{{
      {{1, 0}}, {{-1, 0}}, {{0, 1}}, {{0, -1}},
      {{1, 1}}, {{-1, 1}}, {{1, -1}}, {{-1, -1}}
    }};
    const auto penguinDirectionBit = [](int deltaFile, int deltaRank) {
        if (deltaFile == 0 && deltaRank == 1) return std::uint8_t{1};
        if (deltaFile == 0 && deltaRank == -1) return std::uint8_t{2};
        if (deltaFile == -1 && deltaRank == 0) return std::uint8_t{4};
        if (deltaFile == 1 && deltaRank == 0) return std::uint8_t{8};
        if (deltaFile == -1 && deltaRank == 1) return std::uint8_t{16};
        if (deltaFile == 1 && deltaRank == 1) return std::uint8_t{32};
        if (deltaFile == -1 && deltaRank == -1) return std::uint8_t{64};
        if (deltaFile == 1 && deltaRank == -1) return std::uint8_t{128};
        return std::uint8_t{0};
    };
    const auto penguinTargetFlag = [&](int target, int other) {
        if (target == whiteKing) return 1u;
        if (target == blackKing) return 2u;
        if (other != Position::NoPiece && target == other) return 4u;
        return 0u;
    };
    const auto penguinSubstate = [&](int penguin, int other)
      -> std::optional<std::uint32_t> {
        const PieceState& item = position.pieces_[penguin];
        if (item.type != PieceType::Penguin || item.link != Position::NoPiece ||
            item.cooldown || item.power)
            return std::nullopt;
        const int file = item.square % Position::BoardFiles;
        const int rank = item.square / Position::BoardFiles;
        std::uint32_t substate = 0;
        for (const auto& direction : penguinDirections) {
            const std::uint8_t directionBit = penguinDirectionBit(
              direction[0], direction[1]);
            if (!(item.action & directionBit))
                continue;
            const int targetFile = file + direction[0];
            const int targetRank = rank + direction[1];
            if (targetFile < 0 || targetFile >= Position::BoardFiles ||
                targetRank < 0 || targetRank >= Position::BoardRanks)
                return std::nullopt;
            const int target = position.board_[
              targetRank * Position::BoardFiles + targetFile];
            const std::uint32_t targetFlag = penguinTargetFlag(target, other);
            if (!targetFlag || position.pieces_[target].type == PieceType::Penguin)
                return std::nullopt;
            substate |= targetFlag;
        }
        std::uint8_t expectedAction = 0;
        for (const auto& direction : penguinDirections) {
            const int targetFile = file + direction[0];
            const int targetRank = rank + direction[1];
            if (targetFile < 0 || targetFile >= Position::BoardFiles ||
                targetRank < 0 || targetRank >= Position::BoardRanks)
                continue;
            const int target = position.board_[
              targetRank * Position::BoardFiles + targetFile];
            const std::uint32_t targetFlag = penguinTargetFlag(target, other);
            if (targetFlag && (substate & targetFlag))
                expectedAction |= penguinDirectionBit(direction[0], direction[1]);
        }
        return expectedAction == item.action
          ? std::optional<std::uint32_t>(substate) : std::nullopt;
    };
    const auto fullPenguinSubstate = [&](int penguin, int other) {
        const PieceState& item = position.pieces_[penguin];
        const int file = item.square % Position::BoardFiles;
        const int rank = item.square / Position::BoardFiles;
        std::uint32_t result = 0;
        for (const auto& direction : penguinDirections) {
            const int targetFile = file + direction[0];
            const int targetRank = rank + direction[1];
            if (targetFile < 0 || targetFile >= Position::BoardFiles ||
                targetRank < 0 || targetRank >= Position::BoardRanks)
                continue;
            const int target = position.board_[
              targetRank * Position::BoardFiles + targetFile];
            const std::uint32_t targetFlag = penguinTargetFlag(target, other);
            if (targetFlag && position.pieces_[target].type != PieceType::Penguin)
                result |= targetFlag;
        }
        return result;
    };
    const auto penguinFreezeMatches = [&](int first, int firstOther,
                                          std::uint32_t firstSubstate,
                                          int second, int secondOther,
                                          std::uint32_t secondSubstate) {
        Position expected = position;
        for (int id = 0; id < expected.pieceCount_; ++id) {
            expected.pieces_[id].freezeCount = 0;
            if (expected.pieces_[id].type == PieceType::Penguin)
                expected.pieces_[id].action = 0;
        }
        const auto apply = [&](int penguin, int other, std::uint32_t substate) {
            if (penguin == Position::NoPiece)
                return substate == 0;
            if ((other == Position::NoPiece && (substate & ~3u)) ||
                (other != Position::NoPiece &&
                 expected.pieces_[other].type == PieceType::Penguin &&
                 (substate & ~3u)) || (substate & ~7u))
                return false;
            PieceState& item = expected.pieces_[penguin];
            std::array<bool, Position::MaxPieces> frozen{};
            std::uint32_t found = 0;
            const int file = item.square % Position::BoardFiles;
            const int rank = item.square / Position::BoardFiles;
            for (const auto& direction : penguinDirections) {
                const int targetFile = file + direction[0];
                const int targetRank = rank + direction[1];
                if (targetFile < 0 || targetFile >= Position::BoardFiles ||
                    targetRank < 0 || targetRank >= Position::BoardRanks)
                    continue;
                const int target = expected.board_[
                  targetRank * Position::BoardFiles + targetFile];
                const std::uint32_t targetFlag = penguinTargetFlag(target, other);
                if (!targetFlag || !(substate & targetFlag))
                    continue;
                found |= targetFlag;
                item.action |= penguinDirectionBit(direction[0], direction[1]);
                if (!frozen[target]) {
                    frozen[target] = true;
                    ++expected.pieces_[target].freezeCount;
                }
            }
            return found == substate;
        };
        if (!apply(first, firstOther, firstSubstate) ||
            !apply(second, secondOther, secondSubstate))
            return false;
        for (int id = 0; id < expected.pieceCount_; ++id) {
            if (!expected.pieces_[id].alive)
                continue;
            if (expected.pieces_[id].freezeCount != position.pieces_[id].freezeCount)
                return false;
            if (expected.pieces_[id].type == PieceType::Penguin &&
                expected.pieces_[id].action != position.pieces_[id].action)
                return false;
        }
        return true;
    };
    const auto extractSubstate = [&](int id, PieceType represented,
                                     int other, bool& continuationMatched)
      -> std::optional<std::uint32_t> {
        const PieceState& item = position.pieces_[id];
        if (!typeMatches(represented, item.type))
            return std::nullopt;
        if (represented == PieceType::Angel)
            return angelSubstate(id, other);
        if (!item.onBoard || item.link != Position::NoPiece)
            return std::nullopt;
        switch (represented) {
        case PieceType::Berserker:
            if (item.cooldown || item.action) return std::nullopt;
            return std::min<std::uint32_t>(item.power, 9);
        case PieceType::Ghost:
            if (item.cooldown || item.power || item.action ||
                item.parasiteTracked)
                return std::nullopt;
            return item.visible ? 1u : 0u;
        case PieceType::Devil:
            if (item.cooldown > 3 || item.power || item.action)
                return std::nullopt;
            return item.cooldown;
        case PieceType::Sniper:
            if (item.cooldown > 3 || item.power || item.action) return std::nullopt;
            return item.cooldown;
        case PieceType::Prince: {
            if (item.cooldown || item.power || item.action) return std::nullopt;
            const bool forced =
              position.continuation_ == Continuation::PrinceSecondMove &&
              position.forcedPiece_ == id;
            continuationMatched = continuationMatched || forced;
            return forced ? 1u : 0u;
        }
        case PieceType::Checker: {
            if (item.cooldown || item.power || item.action) return std::nullopt;
            const bool forced =
              position.continuation_ == Continuation::CheckerJump &&
              position.forcedPiece_ == id;
            continuationMatched = continuationMatched || forced;
            return (item.type == PieceType::CheckerKing ? 2u : 0u) +
                   (forced ? 1u : 0u);
        }
        case PieceType::Pawn:
            if (item.cooldown || item.power || item.action) return std::nullopt;
            return item.moved ? 1u : 0u;
        case PieceType::Penguin:
            return penguinSubstate(id, other);
        default:
            if (item.cooldown || item.power || item.action) return std::nullopt;
            return 0u;
        }
    };

    if (extraCount == 4) {
        std::array<int, 2> copycats{Position::NoPiece, Position::NoPiece};
        int copycatCount = 0;
        std::array<bool, Position::MaxPieces> consumed{};
        for (int slot = 0; slot < extraCount; ++slot) {
            const int id = extras[slot];
            if (position.pieces_[id].type != PieceType::Copycat)
                continue;
            if (copycatCount == static_cast<int>(copycats.size()))
                return std::nullopt;
            const int clone = position.pieces_[id].link;
            if (clone == Position::NoPiece || clone >= position.pieceCount_ ||
                !position.pieces_[clone].alive ||
                position.pieces_[clone].type != PieceType::CopycatClone ||
                position.pieces_[clone].link != id ||
                position.pieces_[clone].color != position.pieces_[id].color ||
                position.pieces_[clone].square !=
                  reflect_horizontal(position.pieces_[id].square))
                return std::nullopt;
            copycats[copycatCount++] = id;
            consumed[id] = consumed[clone] = true;
        }
        if (copycatCount != 2)
            return std::nullopt;
        for (int slot = 0; slot < extraCount; ++slot) {
            const int id = extras[slot];
            if (!consumed[id])
                return std::nullopt;
        }
        if (position.continuation_ != Continuation::None ||
            position.forcedPiece_ != Position::NoPiece ||
            position.enPassantVictim_ != Position::NoPiece ||
            position.enPassantSquare_ != Position::NoSquare)
            return std::nullopt;
        for (int id = 0; id < position.pieceCount_; ++id) {
            const PieceState& item = position.pieces_[id];
            if (item.alive && (item.cooldown || item.power || item.action ||
                               item.freezeCount))
                return std::nullopt;
        }
        for (const Database& database : databases()) {
            if (database.attacker != PieceType::Copycat ||
                database.secondary != PieceType::Copycat ||
                database.substates != 1)
                continue;
            for (int order = 0; order < 2; ++order) {
                const PieceState& primary = position.pieces_[copycats[order]];
                const PieceState& secondary = position.pieces_[copycats[1 - order]];
                const Color expectedSecondary =
                  database.secondaryColor == Color::White
                    ? primary.color : ~primary.color;
                if (secondary.color != expectedSecondary)
                    continue;
                const bool swapColors = primary.color == Color::Black;
                const Color side = swapColors ? ~position.sideToMove_
                                              : position.sideToMove_;
                const int canonicalWhite = swapColors ? blackKing : whiteKing;
                const int canonicalBlack = swapColors ? whiteKing : blackKing;
                const std::uint32_t placement =
                  database.secondaryColor == Color::White
                    ? encode_identical_compound_copycat(
                        side, position.pieces_[canonicalWhite].square,
                        position.pieces_[canonicalBlack].square,
                        primary.square, secondary.square)
                    : encode_compound_copycat(
                        side, position.pieces_[canonicalWhite].square,
                        position.pieces_[canonicalBlack].square,
                        primary.square, secondary.square);
                if (placement < database.count)
                    return database.at(placement);
            }
        }
        return std::nullopt;
    }

    if (extraCount == 3) {
        int copycat = Position::NoPiece;
        int clone = Position::NoPiece;
        int material = Position::NoPiece;
        for (int slot = 0; slot < extraCount; ++slot) {
            const int id = extras[slot];
            if (position.pieces_[id].type == PieceType::Copycat)
                copycat = id;
            else if (position.pieces_[id].type == PieceType::CopycatClone)
                clone = id;
            else
                material = id;
        }
        if (copycat == Position::NoPiece || clone == Position::NoPiece ||
            material == Position::NoPiece)
            return std::nullopt;
        const PieceState& primary = position.pieces_[copycat];
        const PieceState& linked = position.pieces_[clone];
        const PieceState& secondary = position.pieces_[material];
        if (primary.link != clone || linked.link != copycat ||
            linked.type != PieceType::CopycatClone || linked.color != primary.color ||
            linked.square != reflect_horizontal(primary.square) ||
            (secondary.type != PieceType::Angel &&
             secondary.link != Position::NoPiece) || primary.cooldown ||
            primary.power || primary.action || primary.freezeCount ||
            linked.cooldown || linked.power || linked.action || linked.freezeCount)
            return std::nullopt;
        for (const Database& database : databases()) {
            if (database.attacker != PieceType::Copycat ||
                database.secondary == PieceType::Count ||
                database.secondary == PieceType::CopycatClone ||
                database.secondary == PieceType::Copycat ||
                database.secondary != secondary.type)
                continue;
            const Color expectedSecondary = database.secondaryColor == Color::White
              ? primary.color : ~primary.color;
            if (secondary.color != expectedSecondary)
                continue;
            bool continuationMatched =
              position.continuation_ == Continuation::None &&
              position.forcedPiece_ == Position::NoPiece;
            const auto secondarySubstate = extractSubstate(
              material, database.secondary, copycat, continuationMatched);
            const std::uint32_t exactSecondarySubstates = represented_substates(
              database.secondary, true, database.attacker,
              database.secondaryColor, Color::White);
            if (!secondarySubstate || !continuationMatched ||
                *secondarySubstate >= exactSecondarySubstates ||
                database.substates != exactSecondarySubstates)
                continue;
            if (position.enPassantVictim_ != Position::NoPiece) {
                if (position.enPassantVictim_ != material ||
                    (secondary.type != PieceType::Pawn &&
                     actionableEnPassant()))
                    continue;
            }
            else if (position.enPassantSquare_ != Position::NoSquare)
                continue;
            bool hasUnrepresentedFreeze = false;
            for (int id = 0; id < position.pieceCount_; ++id)
                hasUnrepresentedFreeze = hasUnrepresentedFreeze ||
                  (position.pieces_[id].alive && position.pieces_[id].freezeCount);
            if (hasUnrepresentedFreeze)
                continue;
            const bool swapColors = primary.color == Color::Black;
            const Color side = swapColors ? ~position.sideToMove_ : position.sideToMove_;
            const int canonicalWhite = swapColors ? blackKing : whiteKing;
            const int canonicalBlack = swapColors ? whiteKing : blackKing;
            const auto secondarySquare = slotSquare(
              material, database.secondary, copycat);
            if (!secondarySquare)
                continue;
            const std::uint64_t index = std::uint64_t(encode_compound_copycat(
              side, position.pieces_[canonicalWhite].square,
              position.pieces_[canonicalBlack].square, primary.square,
              *secondarySquare)) * database.substates + *secondarySubstate;
            if (index < database.count)
                return database.at(static_cast<std::uint32_t>(index));
        }
        return std::nullopt;
    }

    if (extraCount == 2) {
        for (const Database& database : databases()) {
            if (database.secondary == PieceType::Count)
                continue;
            for (int order = 0; order < 2; ++order) {
                const int first = extras[order];
                const int second = extras[1 - order];
                const PieceState& primary = position.pieces_[first];
                const PieceState& secondary = position.pieces_[second];
                if (!typeMatches(database.attacker, primary.type) ||
                    !typeMatches(database.secondary, secondary.type))
                    continue;
                const bool swapColors = primary.color == Color::Black;
                const Color expectedSecondary = database.secondaryColor == Color::White
                  ? primary.color : ~primary.color;
                if (secondary.color != expectedSecondary)
                    continue;
                const int firstProxy = boardProxy(
                  first, database.attacker, second);
                const int secondProxy = boardProxy(
                  second, database.secondary, first);
                if (firstProxy == Position::NoPiece ||
                    secondProxy == Position::NoPiece)
                    continue;
                const bool copycat = primary.type == PieceType::Copycat &&
                                     secondary.type == PieceType::CopycatClone;
                const bool containsCopycat =
                  primary.type == PieceType::Copycat ||
                  primary.type == PieceType::CopycatClone ||
                  secondary.type == PieceType::Copycat ||
                  secondary.type == PieceType::CopycatClone;
                // Every concrete Copycat table starts with the complete
                // linked horizontal-mirror pair.  A later split/singleton
                // state may otherwise have the same two non-King piece types
                // as a compound Copycat table and accidentally reuse its
                // placement index.  Only the dedicated intact-pair shape is
                // representable in this two-extra branch.
                if (containsCopycat && !copycat)
                    continue;
                if (copycat ? !(primary.link == second && secondary.link == first)
                            : !((database.attacker == PieceType::Angel ||
                                 primary.link == Position::NoPiece) &&
                                (database.secondary == PieceType::Angel ||
                                 secondary.link == Position::NoPiece)))
                    continue;
                if (copycat && !database.linkedCopycatPair &&
                    secondary.square != reflect_horizontal(primary.square))
                    continue;
                if (copycat && (primary.cooldown || primary.power ||
                                secondary.cooldown || secondary.power))
                    continue;
                bool continuationMatched = position.continuation_ == Continuation::None &&
                                           position.forcedPiece_ == Position::NoPiece;
                std::optional<std::uint32_t> primarySubstate = copycat
                  ? std::optional<std::uint32_t>(0)
                  : extractSubstate(
                      first, database.attacker,
                      database.attacker == PieceType::Angel
                        ? second : secondProxy,
                      continuationMatched);
                std::optional<std::uint32_t> secondarySubstate = copycat
                  ? std::optional<std::uint32_t>(0)
                  : extractSubstate(
                      second, database.secondary,
                      database.secondary == PieceType::Angel
                        ? first : firstProxy,
                      continuationMatched);
                if (!primarySubstate || !secondarySubstate || !continuationMatched)
                    continue;
                if (position.enPassantVictim_ != Position::NoPiece) {
                    const int victim = position.enPassantVictim_;
                    if ((victim != first && victim != second) ||
                        (position.pieces_[victim].type != PieceType::Pawn &&
                         actionableEnPassant()))
                        continue;
                }
                else if (position.enPassantSquare_ != Position::NoSquare)
                    continue;

                if (database.attacker == PieceType::Penguin ||
                    database.secondary == PieceType::Penguin) {
                    const int firstPenguin = database.attacker == PieceType::Penguin
                      ? first : second;
                    const int firstOther = firstPenguin == first
                      ? secondProxy : firstProxy;
                    const std::uint32_t firstPenguinSubstate =
                      firstPenguin == first ? *primarySubstate : *secondarySubstate;
                    const int secondPenguin =
                      database.attacker == PieceType::Penguin &&
                      database.secondary == PieceType::Penguin
                        ? second : Position::NoPiece;
                    if (!penguinFreezeMatches(
                          firstPenguin, firstOther, firstPenguinSubstate,
                          secondPenguin, first,
                          secondPenguin == Position::NoPiece
                            ? 0 : *secondarySubstate))
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
                const auto primarySquare = slotSquare(
                  first, database.attacker, second);
                const auto secondarySquare = slotSquare(
                  second, database.secondary, first);
                if (!primarySquare || !secondarySquare)
                    continue;
                const bool identical = database.attacker == database.secondary &&
                                       database.secondaryColor == Color::White;
                std::uint32_t placement = 0;
                bool identicalSwapped = false;
                if (copycat)
                    placement = database.linkedCopycatPair
                      ? encode_four(
                          side, position.pieces_[canonicalWhite].square,
                          position.pieces_[canonicalBlack].square,
                          primary.square, secondary.square, false, false)
                      : encode(side,
                          position.pieces_[canonicalWhite].square,
                          position.pieces_[canonicalBlack].square,
                          primary.square);
                else if (identical) {
                    std::uint8_t wk = position.pieces_[canonicalWhite].square;
                    std::uint8_t bk = position.pieces_[canonicalBlack].square;
                    std::uint8_t firstSquare = *primarySquare;
                    std::uint8_t secondSquare = *secondarySquare;
                    if (wk % 8 >= 4) {
                        wk = reflect_horizontal(wk);
                        bk = reflect_horizontal(bk);
                        const bool giant = database.attacker == PieceType::Giant;
                        firstSquare = giant
                          ? reflect_giant_anchor_horizontal(firstSquare)
                          : reflect_horizontal(firstSquare);
                        secondSquare = giant
                          ? reflect_giant_anchor_horizontal(secondSquare)
                          : reflect_horizontal(secondSquare);
                    }
                    if (rank_excluding(firstSquare, {wk, bk}) >
                        rank_excluding(secondSquare, {wk, bk})) {
                        std::swap(*primarySubstate, *secondarySubstate);
                        identicalSwapped = true;
                    }
                    placement = encode_identical_four(
                      side, position.pieces_[canonicalWhite].square,
                      position.pieces_[canonicalBlack].square, *primarySquare,
                      *secondarySquare, database.attacker == PieceType::Giant);
                }
                else placement = encode_four(
                  side, position.pieces_[canonicalWhite].square,
                  position.pieces_[canonicalBlack].square, *primarySquare,
                  *secondarySquare, database.attacker == PieceType::Giant,
                  database.secondary == PieceType::Giant);
                const std::uint32_t exactPrimaryFactor = represented_substates(
                  database.attacker, true, database.secondary, Color::White,
                  database.secondaryColor);
                const std::uint32_t exactSecondaryFactor = represented_substates(
                  database.secondary, true, database.attacker,
                  database.secondaryColor, Color::White);
                std::uint32_t primaryFactor = exactPrimaryFactor;
                std::uint32_t secondaryFactor = exactSecondaryFactor;
                if (database.substates != exactPrimaryFactor * exactSecondaryFactor) {
                    const std::uint32_t legacyPrimaryFactor =
                      database.attacker == PieceType::Penguin
                        ? 2 : exactPrimaryFactor;
                    const std::uint32_t legacySecondaryFactor =
                      database.secondary == PieceType::Penguin
                        ? 2 : exactSecondaryFactor;
                    if (database.substates !=
                        legacyPrimaryFactor * legacySecondaryFactor)
                        continue;
                    const auto legacyPenguin = [&](PieceType type, int id,
                                                   int other,
                                                   std::uint32_t exact)
                      -> std::optional<std::uint32_t> {
                        if (type != PieceType::Penguin)
                            return exact;
                        if (!exact)
                            return 0;
                        const std::uint32_t full = fullPenguinSubstate(id, other);
                        return full && exact == full
                          ? std::optional<std::uint32_t>(1) : std::nullopt;
                    };
                    const int encodedPrimary = identicalSwapped ? second : first;
                    const int encodedSecondary = identicalSwapped ? first : second;
                    const auto legacyPrimary = legacyPenguin(
                      database.attacker, encodedPrimary, encodedSecondary,
                      *primarySubstate);
                    const auto legacySecondary = legacyPenguin(
                      database.secondary, encodedSecondary, encodedPrimary,
                      *secondarySubstate);
                    if (!legacyPrimary || !legacySecondary)
                        continue;
                    *primarySubstate = *legacyPrimary;
                    *secondarySubstate = *legacySecondary;
                    primaryFactor = legacyPrimaryFactor;
                    secondaryFactor = legacySecondaryFactor;
                }
                if (*primarySubstate >= primaryFactor ||
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
    // There is no K+singleton-Copycat domain: Copycat tablebases always encode
    // both linked halves, even when no other non-King model is present.
    if (extra.type == PieceType::Copycat ||
        extra.type == PieceType::CopycatClone)
        return std::nullopt;
    if (extra.type != PieceType::Angel &&
        extra.link != Position::NoPiece)
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
        // A promoted Checker King is represented by the corresponding
        // Checker's promoted substates, just as it is in compound tables.
        const PieceType represented = database.attacker;
        if (!typeMatches(represented, extra.type))
            continue;
        if (represented == PieceType::Ghost &&
            database.trackedGhost != extra.parasiteTracked)
            continue;
        if (represented != PieceType::Penguin && has_unrepresented_freeze())
            continue;
        std::uint32_t substate = 0;
        switch (represented) {
        case PieceType::Berserker:
            if (extra.cooldown)
                continue;
            substate = std::min<std::uint32_t>(extra.power, 9);
            break;
        case PieceType::Ghost:
            if (extra.cooldown || extra.power ||
                (extra.parasiteTracked && !database.trackedGhost))
                continue;
            substate = database.trackedGhost ? 0 : extra.visible ? 1 : 0;
            break;
        case PieceType::Devil:
            if (extra.cooldown > 3 || extra.power)
                continue;
            substate = extra.cooldown;
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
        case PieceType::Checker: {
            if (extra.cooldown || extra.power)
                continue;
            const bool forced = position.continuation_ == Continuation::CheckerJump &&
              position.forcedPiece_ == attacker;
            if (!forced && (position.continuation_ != Continuation::None ||
                            position.forcedPiece_ != Position::NoPiece))
                continue;
            substate = (extra.type == PieceType::CheckerKing ? 2u : 0u) +
              (forced ? 1u : 0u);
            break;
        }
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
            if (extra.cooldown || extra.power ||
                position.continuation_ != Continuation::None ||
                position.forcedPiece_ != Position::NoPiece)
                continue;
            const auto exact = penguinSubstate(attacker, Position::NoPiece);
            if (!exact || !penguinFreezeMatches(
                  attacker, Position::NoPiece, *exact,
                  Position::NoPiece, Position::NoPiece, 0))
                continue;
            substate = *exact;
            break;
        }
        case PieceType::Angel: {
            const auto exact = angelSubstate(attacker, Position::NoPiece);
            if (!exact || position.continuation_ != Continuation::None ||
                position.forcedPiece_ != Position::NoPiece)
                continue;
            substate = *exact;
            break;
        }
        default:
            if (extra.cooldown || extra.power || has_unrepresented_freeze() ||
                position.continuation_ != Continuation::None ||
                position.forcedPiece_ != Position::NoPiece)
                continue;
            break;
        }
        const std::uint32_t exactSubstates = database.trackedGhost
          ? 1 : represented_substates(represented);
        if (database.substates != exactSubstates) {
            if (represented != PieceType::Penguin || database.substates != 2)
                continue;
            if (substate) {
                const std::uint32_t full = fullPenguinSubstate(
                  attacker, Position::NoPiece);
                if (!full || substate != full)
                    continue;
                substate = 1;
            }
        }
        if (substate >= database.substates)
            continue;
        const bool swapColors = extra.color == Color::Black;
        const Color side = swapColors ? ~position.sideToMove_ : position.sideToMove_;
        const int canonicalWhite = swapColors ? blackKing : whiteKing;
        const int canonicalBlack = swapColors ? whiteKing : blackKing;
        const auto attackerSquare = slotSquare(
          attacker, represented, Position::NoPiece);
        if (!attackerSquare)
            continue;
        const std::uint32_t index = encode(
          side, position.pieces_[canonicalWhite].square,
          position.pieces_[canonicalBlack].square, *attackerSquare) *
          database.substates + substate;
        return database.at(index);
    }
    return std::nullopt;
}

}  // namespace Stockfish::Ultimate
