/* Stateful Devil sidecar probe tests, GPLv3 or later. */

#include "position.h"
#include "tablebases/tablebase_probe.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <cstdlib>
#include <unistd.h>

using namespace Stockfish::Ultimate;

namespace {

constexpr std::uint64_t choose(unsigned n, unsigned k) {
    if (k > n) return 0;
    if (k > n - k) k = n - k;
    std::uint64_t value = 1;
    for (unsigned item = 1; item <= k; ++item)
        value = value * (n - k + item) / item;
    return value;
}

std::uint64_t key(Color side, unsigned whiteKing, unsigned blackKing,
                  bool alive, unsigned cooldown,
                  const std::vector<unsigned>& minions) {
    std::uint64_t ranked = 0;
    for (unsigned smaller = 0; smaller < minions.size(); ++smaller)
        ranked += choose(80, smaller);
    unsigned ordinal = 1;
    for (const unsigned square : minions)
        ranked += choose(square, ordinal++);
    const unsigned blackIndex = blackKing < whiteKing
      ? blackKing : blackKing - 1;
    return ranked | ((whiteKing * 79ULL + blackIndex) << 25) |
      (std::uint64_t{80} << 38) | (std::uint64_t{cooldown} << 45) |
      (std::uint64_t{side == Color::Black} << 47) |
      (std::uint64_t{alive} << 48);
}

#pragma pack(push, 1)
struct Header {
    std::array<char, 8> magic{{'U','F','D','S','V','1','\0','\0'}};
    std::uint32_t version = 1;
    std::uint32_t square = 18;
    std::uint64_t count = 0;
    std::uint32_t recordBytes = 10;
    std::uint32_t reserved = 0;
};
struct Record {
    std::array<std::uint8_t, 7> key{};
    std::uint8_t wdl = 0;
    std::uint16_t dtw = 0;
};
#pragma pack(pop)

Record record(std::uint64_t logical, TablebaseWdl wdl, std::uint16_t dtw) {
    Record result;
    std::memcpy(result.key.data(), &logical, result.key.size());
    result.wdl = static_cast<std::uint8_t>(wdl);
    result.dtw = dtw;
    return result;
}

void moved(Position& position, PieceType type, Color color, const char* square) {
    const int id = position.add_piece(type, color, Position::square_from_name(square));
    position.piece(id).moved = true;
}

}  // namespace

int main() {
    char directoryTemplate[] = "/tmp/ultimate-devil-sidecar-test.XXXXXX";
    const char* directoryRaw = ::mkdtemp(directoryTemplate);
    if (!directoryRaw)
        return 2;
    const std::filesystem::path directory(directoryRaw);
    const std::filesystem::path path = directory / "devil-18.ufds";
    std::vector<Record> records{
      record(key(Color::Black, 0, 79, true, 2, {27}),
             TablebaseWdl::Win, 7),
      record(key(Color::White, 0, 79, false, 0, {59}),
             TablebaseWdl::Loss, 1),
    };
    std::sort(records.begin(), records.end(), [](const auto& first, const auto& second) {
        std::uint64_t a = 0, b = 0;
        std::memcpy(&a, first.key.data(), first.key.size());
        std::memcpy(&b, second.key.data(), second.key.size());
        return a < b;
    });
    Header header;
    header.count = records.size();
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(&header), sizeof(header));
    output.write(reinterpret_cast<const char*>(records.data()),
                 records.size() * sizeof(Record));
    output.close();
    if (!TablebaseProbe::uses_compatible_codec(path.string()))
        return 7;
    ::setenv("ULTIMATE_TABLEBASE_PATH", directory.c_str(), 1);

    Position live;
    moved(live, PieceType::King, Color::White, "a1");
    const int devil = live.add_piece(PieceType::Devil, Color::White,
                                     Position::square_from_name("c3"));
    live.piece(devil).moved = true;
    live.piece(devil).cooldown = 2;
    moved(live, PieceType::Minion, Color::White, "d4");
    moved(live, PieceType::King, Color::Black, "h10");
    live.set_side_to_move(Color::Black);
    const auto liveResult = TablebaseProbe::probe(live);
    if (!liveResult || liveResult->wdl != TablebaseWdl::Win || liveResult->dtw != 7)
        return 3;

    Position colorSwapped;
    moved(colorSwapped, PieceType::King, Color::Black, "a10");
    const int blackDevil = colorSwapped.add_piece(
      PieceType::Devil, Color::Black, Position::square_from_name("c8"));
    colorSwapped.piece(blackDevil).moved = true;
    colorSwapped.piece(blackDevil).cooldown = 2;
    moved(colorSwapped, PieceType::Minion, Color::Black, "d7");
    moved(colorSwapped, PieceType::King, Color::White, "h1");
    colorSwapped.set_side_to_move(Color::White);
    const auto colorSwappedResult = TablebaseProbe::probe(colorSwapped);
    if (!colorSwappedResult || colorSwappedResult->wdl != TablebaseWdl::Win ||
        colorSwappedResult->dtw != 7)
        return 8;

    Position reflected;
    moved(reflected, PieceType::King, Color::White, "h1");
    const int reflectedDevil = reflected.add_piece(
      PieceType::Devil, Color::White, Position::square_from_name("f3"));
    reflected.piece(reflectedDevil).moved = true;
    reflected.piece(reflectedDevil).cooldown = 2;
    moved(reflected, PieceType::Minion, Color::White, "e4");
    moved(reflected, PieceType::King, Color::Black, "a10");
    reflected.set_side_to_move(Color::Black);
    const auto reflectedResult = TablebaseProbe::probe(reflected);
    if (!reflectedResult || reflectedResult->wdl != TablebaseWdl::Win ||
        reflectedResult->dtw != 7)
        return 4;

    Position captured;
    moved(captured, PieceType::King, Color::White, "a1");
    moved(captured, PieceType::Minion, Color::White, "d8");
    moved(captured, PieceType::King, Color::Black, "h10");
    const auto capturedResult = TablebaseProbe::probe(captured);
    if (!capturedResult || capturedResult->wdl != TablebaseWdl::Loss ||
        capturedResult->dtw != 1)
        return 5;

    Position capturedColorSwapped;
    moved(capturedColorSwapped, PieceType::King, Color::Black, "a10");
    moved(capturedColorSwapped, PieceType::Minion, Color::Black, "d3");
    moved(capturedColorSwapped, PieceType::King, Color::White, "h1");
    capturedColorSwapped.set_side_to_move(Color::Black);
    const auto capturedColorSwappedResult = TablebaseProbe::probe(capturedColorSwapped);
    if (!capturedColorSwappedResult ||
        capturedColorSwappedResult->wdl != TablebaseWdl::Loss ||
        capturedColorSwappedResult->dtw != 1)
        return 9;

    Position impossible = captured;
    moved(impossible, PieceType::Minion, Color::White, "a2");
    if (TablebaseProbe::probe(impossible))
        return 6;

    std::filesystem::remove_all(directory);
    std::cout << "ULTIMATE_DEVIL_STATEFUL_SIDECAR_TEST_OK\n";
}
