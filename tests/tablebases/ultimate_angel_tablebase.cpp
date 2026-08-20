/* Exact Angel/Halo tablebase codec and runtime-probe regression, GPLv3+. */

#include "position.h"
#include "tablebase_probe.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace Stockfish::Ultimate;

namespace {

void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

int moved_piece(Position& position, PieceType type, Color color,
                const char* square) {
    const int id = position.add_piece(
      type, color, Position::square_from_name(square));
    require(id != Position::NoPiece, "could not add Angel probe fixture piece");
    position.piece(id).moved = true;
    return id;
}

void require_draw(const Position& position, const char* message) {
    const auto result = TablebaseProbe::probe(position);
    require(result && result->wdl == TablebaseWdl::Draw && !result->dtw,
            message);
}

template<typename Value>
void write_value(std::ofstream& stream, Value value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

std::string create_rook_angel_fixture() {
    constexpr std::uint32_t squares = Position::BoardSquares;
    constexpr std::uint32_t placements =
      2 * (squares / 2) * (squares - 1) * (squares - 2) * (squares - 3);
    constexpr std::uint32_t substates = 3;
    constexpr std::uint32_t count = placements * substates;
    constexpr std::uint32_t wdlBytes = (count + 3) / 4;
    constexpr std::uint64_t angelGraphV1Tag = 0x314c45474e414655ULL;
    const std::string path =
      (std::filesystem::temp_directory_path() /
       "ultimatefish-rook-angel-probe-test.uftb").string();
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    require(bool(stream), "could not create synthetic Rook/Angel tablebase");
    const std::array<char, 8> magic{{'U','F','T','B','1','\0','\0','\0'}};
    stream.write(magic.data(), magic.size());
    write_value<std::uint32_t>(stream, 9);
    write_value<std::uint32_t>(stream,
                               static_cast<std::uint32_t>(PieceType::Rook));
    write_value<std::uint32_t>(stream, count);
    write_value<std::uint32_t>(stream, 0);  // legacy edge count
    write_value<std::uint32_t>(stream, substates);
    write_value<std::uint32_t>(stream, wdlBytes);
    write_value<std::uint32_t>(stream, count);  // one DTW byte per state
    write_value<std::uint32_t>(stream, 0);      // no DTW exceptions
    write_value<std::uint32_t>(stream,
                               static_cast<std::uint32_t>(PieceType::Angel));
    write_value<std::uint32_t>(stream,
                               static_cast<std::uint32_t>(Color::White));
    write_value<std::uint64_t>(stream, 0);  // exact edge count
    write_value<std::uint64_t>(stream, angelGraphV1Tag);

    std::array<char, 1 << 20> draws{};
    draws.fill(static_cast<char>(0xff));
    std::uint32_t remaining = wdlBytes;
    while (remaining) {
        const std::uint32_t chunk = std::min<std::uint32_t>(remaining, draws.size());
        stream.write(draws.data(), chunk);
        remaining -= chunk;
    }
    // DTW is all zero.  Keep that plane sparse while retaining the exact
    // authenticated file length required by the runtime loader.
    stream.seekp(static_cast<std::streamoff>(count - 1), std::ios::cur);
    stream.put('\0');
    require(bool(stream), "could not finish synthetic Rook/Angel tablebase");
    return path;
}

std::string create_copycat_angel_fixture(Color angelColor,
                                         bool includePayload,
                                         bool exactSameCodec = false) {
    constexpr std::uint32_t squares = Position::BoardSquares;
    constexpr std::uint32_t placements =
      2 * squares * (squares - 1) * (squares - 2) * (squares - 3);
    const std::uint32_t substates = exactSameCodec ? 4 : 2;
    const std::uint32_t count = placements * substates;
    const std::uint32_t wdlBytes = (count + 3) / 4;
    constexpr std::uint64_t angelGraphV1Tag = 0x314c45474e414655ULL;
    constexpr std::uint64_t angelCopycatGraphV1Tag = 0x3152504343414655ULL;
    const std::string suffix = angelColor == Color::Black ? "opposed" : "same";
    const std::string path =
      (std::filesystem::temp_directory_path() /
       ("ultimatefish-copycat-angel-" + suffix + "-probe-test.uftb")).string();
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    require(bool(stream), "could not create synthetic Copycat/Angel tablebase");
    const std::array<char, 8> magic{{'U','F','T','B','1','\0','\0','\0'}};
    stream.write(magic.data(), magic.size());
    write_value<std::uint32_t>(stream, exactSameCodec ? 10 : 9);
    write_value<std::uint32_t>(stream,
                               static_cast<std::uint32_t>(PieceType::Copycat));
    write_value<std::uint32_t>(stream, count);
    write_value<std::uint32_t>(stream, 0);
    write_value<std::uint32_t>(stream, substates);
    write_value<std::uint32_t>(stream, wdlBytes);
    write_value<std::uint32_t>(stream, count);
    write_value<std::uint32_t>(stream, 0);
    write_value<std::uint32_t>(stream,
                               static_cast<std::uint32_t>(PieceType::Angel));
    write_value<std::uint32_t>(stream,
                               static_cast<std::uint32_t>(angelColor));
    write_value<std::uint64_t>(stream, 0);
    write_value<std::uint64_t>(stream, exactSameCodec
      ? angelCopycatGraphV1Tag : angelGraphV1Tag);
    if (!includePayload)
        return path;
    std::array<char, 1 << 20> draws{};
    draws.fill(static_cast<char>(0xff));
    std::uint32_t remaining = wdlBytes;
    while (remaining) {
        const std::uint32_t chunk = std::min<std::uint32_t>(remaining, draws.size());
        stream.write(draws.data(), chunk);
        remaining -= chunk;
    }
    stream.seekp(static_cast<std::streamoff>(count - 1), std::ios::cur);
    stream.put('\0');
    require(bool(stream), "could not finish synthetic Copycat/Angel tablebase");
    return path;
}

std::string create_linked_copycat_pair_fixture() {
    constexpr std::uint32_t squares = Position::BoardSquares;
    constexpr std::uint32_t count =
      2 * (squares / 2) * (squares - 1) * (squares - 2) * (squares - 3);
    constexpr std::uint32_t wdlBytes = (count + 3) / 4;
    constexpr std::uint64_t linkedCopycatPairV1Tag = 0x314b4e4c43434655ULL;
    const std::string path =
      (std::filesystem::temp_directory_path() /
       "ultimatefish-linked-copycat-pair-probe-test.uftb").string();
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    require(bool(stream), "could not create synthetic linked Copycat tablebase");
    const std::array<char, 8> magic{{'U','F','T','B','1','\0','\0','\0'}};
    stream.write(magic.data(), magic.size());
    write_value<std::uint32_t>(stream, 10);
    write_value<std::uint32_t>(stream,
                               static_cast<std::uint32_t>(PieceType::Copycat));
    write_value<std::uint32_t>(stream, count);
    write_value<std::uint32_t>(stream, 0);
    write_value<std::uint32_t>(stream, 1);
    write_value<std::uint32_t>(stream, wdlBytes);
    write_value<std::uint32_t>(stream, count);
    write_value<std::uint32_t>(stream, 0);
    write_value<std::uint32_t>(stream,
                               static_cast<std::uint32_t>(PieceType::CopycatClone));
    write_value<std::uint32_t>(stream,
                               static_cast<std::uint32_t>(Color::White));
    write_value<std::uint64_t>(stream, 0);
    write_value<std::uint64_t>(stream, linkedCopycatPairV1Tag);
    std::array<char, 1 << 20> draws{};
    draws.fill(static_cast<char>(0xff));
    std::uint32_t remaining = wdlBytes;
    while (remaining) {
        const std::uint32_t chunk = std::min<std::uint32_t>(remaining, draws.size());
        stream.write(draws.data(), chunk);
        remaining -= chunk;
    }
    stream.seekp(static_cast<std::streamoff>(count - 1), std::ios::cur);
    stream.put('\0');
    require(bool(stream), "could not finish synthetic linked Copycat tablebase");
    return path;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 2, "usage: ultimate-angel-tablebase-test TABLEBASE");
        require(TablebaseProbe::uses_compatible_codec(argv[1]),
                "runtime rejected the authenticated AngelGraphV1 codec");
        const std::string pairPath = create_rook_angel_fixture();
        require(TablebaseProbe::uses_compatible_codec(pairPath),
                "runtime rejected the two-piece AngelGraphV1 codec");
        const std::string copycatAngelPath =
          create_copycat_angel_fixture(Color::Black, true);
        require(TablebaseProbe::uses_compatible_codec(copycatAngelPath),
                "runtime rejected opposed Copycat/Angel graph codec");
        const std::string sameCopycatAngelPath =
          create_copycat_angel_fixture(Color::White, false);
        require(!TablebaseProbe::uses_compatible_codec(sameCopycatAngelPath),
                "runtime accepted legacy same-team Copycat/Angel graph");
        const std::string exactSameCopycatAngelPath =
          create_copycat_angel_fixture(Color::White, true, true);
        require(TablebaseProbe::uses_compatible_codec(exactSameCopycatAngelPath),
                "runtime rejected exact same-team Copycat/Angel graph");
        const std::string linkedCopycatPairPath =
          create_linked_copycat_pair_fixture();
        require(TablebaseProbe::uses_compatible_codec(linkedCopycatPairPath),
                "runtime rejected arbitrary linked Copycat-pair codec");
        const std::string paths = std::string(argv[1]) + ":" + pairPath +
                                  ":" + copycatAngelPath + ":" +
                                  exactSameCopycatAngelPath + ":" +
                                  linkedCopycatPairPath;
        require(setenv("ULTIMATE_TABLEBASE_PATH", paths.c_str(), 1) == 0,
                "could not bind Angel tablebase path");

        Position deployed;
        moved_piece(deployed, PieceType::King, Color::White, "a1");
        moved_piece(deployed, PieceType::King, Color::Black, "h10");
        moved_piece(deployed, PieceType::Angel, Color::White, "b2");
        require_draw(deployed, "deployed Angel did not probe exactly");

        Position attached;
        std::string error;
        require(attached.set_upn(
          "b;hm=1;fm=1;ep=-;cont=0;forced=-1;epv=-1;win=-;"
          "king,w,a1,0,0,0,0,1,1,-1,1,-1,0;"
          "king,b,h10,0,0,0,0,1,1,-1,1,-1,0;"
          "angel,w,a1,0,0,0,0,1,1,3,0,0,1;"
          "halo,w,b2,0,0,0,0,0,1,2,1,-1,0", &error),
          ("could not parse attached Angel fixture: " + error).c_str());
        constexpr int angel = 2;
        require_draw(attached, "attached Angel/Halo did not probe exactly");

        Position malformed = attached;
        malformed.piece(malformed.piece(angel).link).link = Position::NoPiece;
        require(!TablebaseProbe::probe(malformed),
                "runtime accepted a non-reciprocal Angel/Halo link");

        Position black;
        moved_piece(black, PieceType::King, Color::White, "h10");
        moved_piece(black, PieceType::King, Color::Black, "a1");
        moved_piece(black, PieceType::Angel, Color::Black, "b2");
        black.set_side_to_move(Color::Black);
        require_draw(black, "Angel codec did not canonicalize owner color");

        Position pair;
        moved_piece(pair, PieceType::King, Color::White, "a1");
        moved_piece(pair, PieceType::King, Color::Black, "h10");
        moved_piece(pair, PieceType::Rook, Color::White, "c3");
        moved_piece(pair, PieceType::Angel, Color::White, "b2");
        require_draw(pair, "deployed Rook/Angel pair did not probe exactly");

        Position linkedPair;
        require(linkedPair.set_upn(
          "b;hm=1;fm=1;ep=-;cont=0;forced=-1;epv=-1;win=-;"
          "king,w,a1,0,0,0,0,1,1,-1,1,-1,0;"
          "king,b,h10,0,0,0,0,1,1,-1,1,-1,0;"
          "rook,w,c3,0,0,0,0,1,1,-1,1,-1,0;"
          "angel,w,c3,0,0,0,0,1,1,4,0,2,1;"
          "halo,w,b2,0,0,0,0,0,1,3,1,-1,0", &error),
          ("could not parse Rook-hosted Angel fixture: " + error).c_str());
        require_draw(linkedPair,
                     "Rook-hosted Angel/Halo pair did not probe exactly");

        Position copycatAngel;
        moved_piece(copycatAngel, PieceType::King, Color::White, "a1");
        moved_piece(copycatAngel, PieceType::King, Color::Black, "h10");
        const int copycat = moved_piece(
          copycatAngel, PieceType::Copycat, Color::White, "c3");
        copycatAngel.piece(copycatAngel.piece(copycat).link).moved = true;
        moved_piece(copycatAngel, PieceType::Angel, Color::Black, "g9");
        require_draw(copycatAngel,
                     "deployed opposed Copycat/Angel did not probe exactly");

        Position attachedCopycatAngel;
        require(attachedCopycatAngel.set_upn(
          "b;hm=1;fm=1;ep=-;cont=0;forced=-1;epv=-1;win=-;"
          "king,w,a1,0,0,0,0,1,1,-1,1,-1,0;"
          "king,b,h10,0,0,0,0,1,1,-1,1,-1,0;"
          "copycat,w,c3,0,0,0,0,1,1,3,1,-1,0;"
          "copycatClone,w,f3,0,0,0,0,1,1,2,1,-1,0;"
          "angel,b,h10,0,0,0,0,1,1,5,0,1,1;"
          "halo,b,g9,0,0,0,0,0,1,4,1,-1,0", &error),
          ("could not parse attached Copycat/Angel fixture: " + error).c_str());
        require_draw(attachedCopycatAngel,
                     "attached opposed Copycat/Angel did not probe exactly");

        Position sameCopycatAngel;
        moved_piece(sameCopycatAngel, PieceType::King, Color::White, "a1");
        moved_piece(sameCopycatAngel, PieceType::King, Color::Black, "h10");
        const int sameCopycat = moved_piece(
          sameCopycatAngel, PieceType::Copycat, Color::White, "c3");
        sameCopycatAngel.piece(
          sameCopycatAngel.piece(sameCopycat).link).moved = true;
        moved_piece(sameCopycatAngel, PieceType::Angel, Color::White, "b2");
        require_draw(sameCopycatAngel,
                     "deployed same-team Copycat/Angel did not probe exactly");

        Position cloneHostedAngel;
        require(cloneHostedAngel.set_upn(
          "b;hm=1;fm=1;ep=-;cont=0;forced=-1;epv=-1;win=-;"
          "king,w,a1,0,0,0,0,1,1,-1,1,-1,0;"
          "king,b,h10,0,0,0,0,1,1,-1,1,-1,0;"
          "copycat,w,c3,0,0,0,0,1,1,3,1,-1,0;"
          "copycatClone,w,f3,0,0,0,0,1,1,2,1,-1,0;"
          "angel,w,f3,0,0,0,0,1,1,5,0,3,1;"
          "halo,w,b2,0,0,0,0,0,1,4,1,-1,0", &error),
          ("could not parse clone-hosted Angel fixture: " + error).c_str());
        require_draw(cloneHostedAngel,
                     "clone-hosted same-team Angel did not probe exactly");

        Position displacedPair;
        require(displacedPair.set_upn(
          "w;hm=1;fm=1;ep=-;cont=0;forced=-1;epv=-1;win=-;"
          "king,w,a1,0,0,0,0,1,1,-1,1,-1,0;"
          "king,b,h10,0,0,0,0,1,1,-1,1,-1,0;"
          "copycat,w,b2,0,0,0,0,1,1,3,1,-1,0;"
          "copycatClone,w,f3,0,0,0,0,1,1,2,1,-1,0", &error),
          ("could not parse displaced linked Copycat fixture: " + error).c_str());
        require_draw(displacedPair,
                     "displaced linked Copycat pair did not probe exactly");

        std::cout << "ultimate Angel tablebase probe tests passed\n";
    }
    catch (const std::exception& error) {
        std::cerr << "ultimate Angel tablebase probe test failed: "
                  << error.what() << '\n';
        return 1;
    }
    return 0;
}
