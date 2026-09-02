/*
  Convert an opposed extra-primary Ghost information result to the canonical
  Ghost-primary color-swapped representation.

  The conversion is deliberately exhaustive: every dense source state is
  mapped, the corresponding packed concrete WDL entries are compared, and a
  target-coverage bitset proves that the permutation is bijective.  No game
  result is inferred from material symmetry alone.
*/

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t Squares = 80;
constexpr std::uint32_t Files = 8;
constexpr std::uint32_t ExtraSubstates = 4;
constexpr std::uint32_t CombinedSubstates = 2 * ExtraSubstates;
constexpr std::uint32_t PlacementCount =
  2 * (Squares / 2) * (Squares - 1) * (Squares - 2) * (Squares - 3);
constexpr std::uint32_t StateCount = PlacementCount * CombinedSubstates;
constexpr std::uint32_t Ghost = 11;
constexpr std::uint32_t Checker = 21;

std::uint32_t word(const std::vector<std::uint8_t>& bytes,
                   std::size_t offset) {
    if (offset + 4 > bytes.size())
        throw std::runtime_error("truncated input header");
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.data() + offset, 4);
    return value;
}

void put_word(std::vector<std::uint8_t>& bytes, std::size_t offset,
              std::uint32_t value) {
    if (offset + 4 > bytes.size())
        throw std::runtime_error("truncated output header");
    std::memcpy(bytes.data() + offset, &value, 4);
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("cannot open " + path.string());
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

void write_file(const std::filesystem::path& path,
                const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!output)
        throw std::runtime_error("cannot write " + path.string());
}

std::size_t concrete_header_bytes(std::uint32_t version) {
    return 40 + (version >= 5 ? 8 : 0) + (version >= 6 ? 8 : 0) +
           (version >= 7 ? 8 : 0);
}

struct ConcreteTable {
    std::vector<std::uint8_t> bytes;
    std::size_t wdlOffset = 0;

    ConcreteTable(const std::filesystem::path& path, std::uint32_t primary,
                  std::uint32_t secondary) : bytes(read_file(path)) {
        if (bytes.size() < 48 ||
            std::memcmp(bytes.data(), "UFTB1\0\0\0", 8) ||
            word(bytes, 12) != primary || word(bytes, 16) != StateCount ||
            word(bytes, 24) != CombinedSubstates ||
            word(bytes, 28) != (StateCount + 3) / 4 ||
            word(bytes, 32) != StateCount || word(bytes, 40) != secondary ||
            word(bytes, 44) != 1)
            throw std::runtime_error("concrete table header residual");
        const std::uint32_t version = word(bytes, 8);
        if (version < 5 || version > 9)
            throw std::runtime_error("unsupported concrete table version");
        wdlOffset = concrete_header_bytes(version);
        if (wdlOffset + (StateCount + 3) / 4 > bytes.size())
            throw std::runtime_error("truncated concrete WDL plane");
    }

    std::uint8_t result(std::uint32_t index) const {
        return (bytes[wdlOffset + index / 4] >> (2 * (index % 4))) & 3;
    }
};

std::uint8_t unrank(std::uint32_t rank,
                    std::initializer_list<std::uint8_t> occupied) {
    std::array<std::uint8_t, 3> sorted{};
    std::copy(occupied.begin(), occupied.end(), sorted.begin());
    std::sort(sorted.begin(), sorted.begin() + occupied.size());
    std::uint32_t square = rank;
    for (std::size_t i = 0; i < occupied.size(); ++i)
        if (square >= sorted[i])
            ++square;
    return static_cast<std::uint8_t>(square);
}

std::uint32_t rank(std::uint8_t square,
                   std::initializer_list<std::uint8_t> occupied) {
    return square - std::count_if(occupied.begin(), occupied.end(),
      [square](std::uint8_t value) { return value < square; });
}

std::uint8_t reflect(std::uint8_t square) {
    return static_cast<std::uint8_t>(
      (square / Files) * Files + (Files - 1 - square % Files));
}

std::uint8_t color_swap_square(std::uint8_t square) {
    // Color exchange reverses forward movement.  A mere ownership swap is
    // sufficient for symmetric pieces but is wrong for an unpromoted Checker;
    // reflect ranks as well.  The ordinary horizontal canonicalizer below may
    // independently mirror files to put the new White king in the left half.
    return static_cast<std::uint8_t>(
      (Squares / Files - 1 - square / Files) * Files + square % Files);
}

struct Placement {
    std::uint32_t side = 0;
    std::uint8_t whiteKing = 0, blackKing = 0, first = 0, second = 0;
};

Placement decode_placement(std::uint32_t index) {
    const std::uint32_t secondRank = index % (Squares - 3);
    index /= Squares - 3;
    const std::uint32_t firstRank = index % (Squares - 2);
    index /= Squares - 2;
    const std::uint32_t blackRank = index % (Squares - 1);
    index /= Squares - 1;
    const std::uint32_t whiteRank = index % (Squares / 2);
    const std::uint32_t side = index / (Squares / 2);
    const std::uint8_t whiteKing = static_cast<std::uint8_t>(
      (whiteRank / (Files / 2)) * Files + whiteRank % (Files / 2));
    const std::uint8_t blackKing = unrank(blackRank, {whiteKing});
    const std::uint8_t first = unrank(firstRank, {whiteKing, blackKing});
    const std::uint8_t second =
      unrank(secondRank, {whiteKing, blackKing, first});
    return {side, whiteKing, blackKing, first, second};
}

std::uint32_t encode_placement(Placement state) {
    if (state.whiteKing % Files >= Files / 2) {
        state.whiteKing = reflect(state.whiteKing);
        state.blackKing = reflect(state.blackKing);
        state.first = reflect(state.first);
        state.second = reflect(state.second);
    }
    const std::uint32_t whiteRank =
      (state.whiteKing / Files) * (Files / 2) + state.whiteKing % Files;
    return ((((state.side * (Squares / 2) + whiteRank) * (Squares - 1) +
               rank(state.blackKing, {state.whiteKing})) * (Squares - 2) +
              rank(state.first, {state.whiteKing, state.blackKing})) *
             (Squares - 3) +
            rank(state.second,
                 {state.whiteKing, state.blackKing, state.first}));
}

std::uint8_t swap_force_bits(std::uint8_t value) {
    return static_cast<std::uint8_t>((value & ~3u) | ((value & 1u) << 1) |
                                     ((value & 2u) >> 1));
}

void bind_hash(std::vector<std::uint8_t>& bytes, std::size_t offset,
               const std::string& hash) {
    if (hash.size() != 64 || offset + 64 > bytes.size())
        throw std::runtime_error("invalid SHA-256 binding");
    std::copy(hash.begin(), hash.end(), bytes.begin() + offset);
}

}  // namespace

int main(int argc, char** argv) try {
    if (argc != 9) {
        std::cerr << "usage: canonicalize EXTRA_TABLE CANONICAL_TABLE "
                     "SOURCE_UFIW SOURCE_UFGD CANONICAL_SHA OUT_UFIW "
                     "OUT_UFGD CERTIFICATE\n";
        return 2;
    }
    const ConcreteTable extra(argv[1], Checker, Ghost);
    const ConcreteTable canonical(argv[2], Ghost, Checker);
    const auto sourceOverlay = read_file(argv[3]);
    if (sourceOverlay.size() != 160 + StateCount ||
        std::memcmp(sourceOverlay.data(), "UFIW2\0\0\0", 8) ||
        word(sourceOverlay, 12) != Checker ||
        word(sourceOverlay, 16) != Ghost || word(sourceOverlay, 20) != 1 ||
        word(sourceOverlay, 24) != StateCount ||
        word(sourceOverlay, 28) != CombinedSubstates)
        throw std::runtime_error("source information overlay residual");

    std::vector<std::uint8_t> output(160 + StateCount, 0);
    std::copy(sourceOverlay.begin(), sourceOverlay.begin() + 160,
              output.begin());
    put_word(output, 12, Ghost);
    put_word(output, 16, Checker);
    bind_hash(output, 32, argv[5]);
    std::vector<std::uint8_t> seen((StateCount + 7) / 8, 0);
    std::uint64_t mappingResidual = 0, wdlResidual = 0, coverage = 0;
    std::array<std::array<std::array<std::array<std::uint64_t, 4>, 4>,
                          ExtraSubstates>, 2> resultPairs{};

    for (std::uint32_t sourcePlacement = 0;
         sourcePlacement < PlacementCount; ++sourcePlacement) {
        const Placement source = decode_placement(sourcePlacement);
        Placement target{1u - source.side,
                         color_swap_square(source.blackKing),
                         color_swap_square(source.whiteKing),
                         color_swap_square(source.second),
                         color_swap_square(source.first)};
        const std::uint32_t targetPlacement = encode_placement(target);
        for (std::uint32_t substate = 0; substate < ExtraSubstates;
             ++substate) {
            for (std::uint32_t visible = 0; visible < 2; ++visible) {
                const std::uint32_t sourceIndex =
                  sourcePlacement * CombinedSubstates + substate * 2 + visible;
                const std::uint32_t targetIndex =
                  targetPlacement * CombinedSubstates + substate * 2 + visible;
                const std::uint32_t targetPhysical =
                  targetPlacement * CombinedSubstates +
                  visible * ExtraSubstates + substate;
                if (seen[targetIndex / 8] & (1u << (targetIndex % 8)))
                    ++mappingResidual;
                else {
                    seen[targetIndex / 8] |=
                      static_cast<std::uint8_t>(1u << (targetIndex % 8));
                    ++coverage;
                }
                const std::uint8_t sourceResult = extra.result(sourceIndex);
                const std::uint8_t targetResult =
                  canonical.result(targetPhysical);
                ++resultPairs[source.side][substate][sourceResult][targetResult];
                wdlResidual += sourceResult != targetResult;
                output[160 + targetIndex] =
                  swap_force_bits(sourceOverlay[160 + sourceIndex]);
            }
        }
    }
    mappingResidual += coverage != StateCount;
    std::cout << "ghost_extra_primary_canonicalization_probe states "
              << StateCount << " coverage " << coverage
              << " mapping_residual " << mappingResidual
              << " wdl_residual " << wdlResidual << '\n';
    if (wdlResidual)
        for (std::size_t side = 0; side < 2; ++side)
            for (std::size_t substate = 0; substate < ExtraSubstates;
                 ++substate)
                for (std::size_t sourceResult = 0; sourceResult < 4;
                     ++sourceResult)
                    for (std::size_t targetResult = 0; targetResult < 4;
                         ++targetResult)
                        if (sourceResult != targetResult &&
                            resultPairs[side][substate][sourceResult]
                                       [targetResult])
                            std::cout << "wdl_pair source_side " << side
                                      << " substate " << substate
                                      << " source " << sourceResult
                                      << " target " << targetResult
                                      << " count "
                                      << resultPairs[side][substate]
                                                    [sourceResult]
                                                    [targetResult] << '\n';
    if (mappingResidual || wdlResidual)
        throw std::runtime_error("canonical conversion proof residual");
    write_file(argv[6], output);

    auto arbitrary = read_file(argv[4]);
    if (arbitrary.size() < 1248 ||
        std::memcmp(arbitrary.data(), "UFGD1\0\0\0", 8) ||
        word(arbitrary, 12) != 1248 || word(arbitrary, 20) != Checker ||
        word(arbitrary, 24) != Ghost || word(arbitrary, 32) != 1 ||
        word(arbitrary, 40) != StateCount)
        throw std::runtime_error("source arbitrary sidecar residual");
    put_word(arbitrary, 20, Ghost);
    put_word(arbitrary, 24, Checker);
    put_word(arbitrary, 28, 0);  // Canonical Ghost is White.
    bind_hash(arbitrary, 160, argv[5]);
    write_file(argv[7], arbitrary);

    std::ofstream certificate(argv[8]);
    certificate << "{\n"
                << "  \"schema\": \"ultimatefish-ghost-extra-primary-"
                   "canonicalization-v1\",\n"
                << "  \"states\": " << StateCount << ",\n"
                << "  \"mapping_residual\": 0,\n"
                << "  \"coverage_residual\": 0,\n"
                << "  \"concrete_wdl_residual\": 0,\n"
                << "  \"force_bit_color_swap\": true,\n"
                << "  \"side_summary_swap_required\": true\n"
                << "}\n";
    if (!certificate)
        throw std::runtime_error("cannot write conversion certificate");
    std::cout << "ghost_extra_primary_canonicalization states " << StateCount
              << " mapping_residual 0 coverage_residual 0 wdl_residual 0\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
