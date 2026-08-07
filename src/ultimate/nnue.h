/* Ultimate Fish sparse neural evaluation, GPLv3 or later. */

#ifndef ULTIMATE_NNUE_H_INCLUDED
#define ULTIMATE_NNUE_H_INCLUDED

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace Stockfish::Ultimate {

class Position;

class UltimateNnue {
   public:
    static constexpr std::size_t HiddenDimensions = 32;
    static constexpr std::size_t LocationSquares = 81;
    static constexpr std::size_t StateBuckets = 8;
    static constexpr std::size_t PieceTypes = 30;
    static constexpr std::size_t LocationFeatures = 2 * PieceTypes * LocationSquares;
    static constexpr std::size_t PerStateFeatureGroups = 37;
    static constexpr std::size_t GlobalFeatures = 84;
    static constexpr std::size_t InputDimensions =
      LocationFeatures + 2 * PieceTypes * PerStateFeatureGroups + GlobalFeatures;

    struct Accumulator {
        std::array<std::array<std::int32_t, HiddenDimensions>, 2> values{};
    };

    // Returns no value when ULTIMATE_NNUE_FILE is unset or its network fails
    // strict format validation. The caller then uses handcrafted evaluation.
    static std::optional<int> evaluate(const Position& position);
    static bool enabled();
    static void refresh(const Position& position, Accumulator& accumulator);
    static void update(const Position& parent, const Position& child,
                       const Accumulator& parentAccumulator,
                       Accumulator& childAccumulator);
    static int correction(const Position& position, const Accumulator& accumulator);
};

}  // namespace Stockfish::Ultimate

#endif
