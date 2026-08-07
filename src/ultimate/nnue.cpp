/* Ultimate Fish sparse neural evaluation, GPLv3 or later. */

#include "nnue.h"
#include "position.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <type_traits>
#include <vector>

namespace Stockfish::Ultimate {
namespace {

constexpr std::array<char, 8> Magic{{'U','F','N','N','U','E','1','\0'}};
constexpr std::uint32_t Version = 1;
static_assert(UltimateNnue::PieceTypes == static_cast<std::size_t>(PieceType::Count));

struct Network {
    std::uint32_t activationScale = 0;
    std::uint32_t outputScale = 0;
    std::vector<std::int16_t> featureWeights;
    std::array<std::int32_t, UltimateNnue::HiddenDimensions> hiddenBias{};
    std::array<std::array<std::int16_t, UltimateNnue::HiddenDimensions>, 2> outputWeights{};
    std::int32_t outputBias = 0;
    bool valid = false;
};

template<typename T>
bool read(std::ifstream& stream, T& value) {
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    return bool(stream);
}

Network load_network() {
    Network network;
    const char* configured = std::getenv("ULTIMATE_NNUE_FILE");
    if (!configured || !*configured)
        return network;
    std::ifstream stream(configured, std::ios::binary);
    if (!stream)
        return network;
    std::array<char, 8> magic{};
    std::uint32_t version = 0, inputs = 0, hidden = 0;
    stream.read(magic.data(), magic.size());
    if (!read(stream, version) || !read(stream, inputs) || !read(stream, hidden) ||
        !read(stream, network.activationScale) || !read(stream, network.outputScale) ||
        magic != Magic || version != Version ||
        inputs != UltimateNnue::InputDimensions ||
        hidden != UltimateNnue::HiddenDimensions ||
        !network.activationScale || !network.outputScale)
        return Network{};
    network.featureWeights.resize(
      UltimateNnue::InputDimensions * UltimateNnue::HiddenDimensions);
    stream.read(reinterpret_cast<char*>(network.featureWeights.data()),
                static_cast<std::streamsize>(network.featureWeights.size() * sizeof(std::int16_t)));
    stream.read(reinterpret_cast<char*>(network.hiddenBias.data()),
                sizeof(network.hiddenBias));
    stream.read(reinterpret_cast<char*>(network.outputWeights.data()),
                sizeof(network.outputWeights));
    if (!read(stream, network.outputBias) || stream.peek() != std::ifstream::traits_type::eof())
        return Network{};
    network.valid = true;
    return network;
}

const Network& network() {
    static std::once_flag once;
    static Network loaded;
    std::call_once(once, [] { loaded = load_network(); });
    return loaded;
}

std::size_t relative_piece(const PieceState& piece, Color perspective) {
    return (piece.color == perspective ? 0 : UltimateNnue::PieceTypes) +
           static_cast<std::size_t>(piece.type);
}

std::size_t state_feature(std::size_t relativePiece, std::size_t group) {
    return UltimateNnue::LocationFeatures +
           relativePiece * UltimateNnue::PerStateFeatureGroups + group;
}

template<typename Callback>
void each_piece_feature(const Position& position, int id, Color perspective,
                        const PieceState& piece, Callback&& callback) {
    if (!piece.alive)
        return;
    const std::size_t relative = relative_piece(piece, perspective);
    const std::size_t square = piece.onBoard
      ? static_cast<std::size_t>(perspective == Color::White
          ? piece.square : Position::BoardSquares - 1 - piece.square)
      : Position::BoardSquares;
    callback(relative * UltimateNnue::LocationSquares + square);

    std::size_t group = 0;
    if (piece.moved)
        callback(state_feature(relative, group));
    ++group;
    if (!piece.visible)
        callback(state_feature(relative, group));
    ++group;
    const auto bucket = [](std::uint8_t value) {
        return std::min<std::size_t>(value, UltimateNnue::StateBuckets) - 1;
    };
    if (piece.cooldown)
        callback(state_feature(relative, group + bucket(piece.cooldown)));
    group += UltimateNnue::StateBuckets;
    if (piece.freezeCount)
        callback(state_feature(relative, group + bucket(piece.freezeCount)));
    group += UltimateNnue::StateBuckets;
    if (piece.power)
        callback(state_feature(relative, group + bucket(piece.power)));
    group += UltimateNnue::StateBuckets;
    for (std::size_t bit = 0; bit < 8; ++bit)
        if (piece.action & (1u << bit))
            callback(state_feature(relative, group + bit));
    group += 8;
    if (piece.link != Position::NoPiece)
        callback(state_feature(relative, group));
    ++group;
    if (piece.host != Position::NoPiece)
        callback(state_feature(relative, group));
    ++group;
    if (id == position.forced_piece())
        callback(state_feature(relative, group));
}

template<typename Callback>
void each_global_feature(const Position& position, Color perspective, Callback&& callback) {
    const std::size_t global = UltimateNnue::LocationFeatures +
      2 * UltimateNnue::PieceTypes * UltimateNnue::PerStateFeatureGroups;
    if (position.side_to_move() == perspective)
        callback(global);
    if (position.continuation() == Continuation::CheckerJump)
        callback(global + 1);
    else if (position.continuation() == Continuation::PrinceSecondMove)
        callback(global + 2);
    if (position.en_passant_square() != Position::NoSquare) {
        const int square = perspective == Color::White
          ? position.en_passant_square()
          : Position::BoardSquares - 1 - position.en_passant_square();
        callback(global + 3 + square);
    }
}

template<typename Callback>
void each_feature(const Position& position, Color perspective, Callback&& callback) {
    for (int id = 0; id < position.piece_count(); ++id)
        each_piece_feature(position, id, perspective, position.piece(id), callback);
    each_global_feature(position, perspective, callback);
}

bool same_piece(const PieceState& lhs, const PieceState& rhs) {
    static_assert(std::is_trivially_copyable_v<PieceState>);
    // This is a conservative dirty detector, not PieceState equality. A
    // padding-byte mismatch merely performs a redundant remove/add update;
    // equal object representations necessarily contain equal public state.
    return std::memcmp(&lhs, &rhs, sizeof(PieceState)) == 0;
}

void add_feature(const Network& net, std::size_t feature, int sign,
                 std::array<std::int32_t, UltimateNnue::HiddenDimensions>& accumulator) {
    const std::int16_t* weights =
      net.featureWeights.data() + feature * UltimateNnue::HiddenDimensions;
    for (std::size_t hidden = 0; hidden < UltimateNnue::HiddenDimensions; ++hidden)
        accumulator[hidden] += sign * weights[hidden];
}

}  // namespace

std::optional<int> UltimateNnue::evaluate(const Position& position) {
    const Network& net = network();
    if (!net.valid)
        return std::nullopt;
    Accumulator accumulator;
    refresh(position, accumulator);
    return correction(position, accumulator);
}

bool UltimateNnue::enabled() { return network().valid; }

void UltimateNnue::refresh(const Position& position, Accumulator& accumulator) {
    const Network& net = network();
    for (Color perspective : {Color::White, Color::Black}) {
        auto& values = accumulator.values[static_cast<std::size_t>(perspective)];
        values = net.hiddenBias;
        each_feature(position, perspective, [&](std::size_t feature) {
            add_feature(net, feature, 1, values);
        });
    }
}

void UltimateNnue::update(const Position& parent, const Position& child,
                          const Accumulator& parentAccumulator,
                          Accumulator& childAccumulator) {
    const Network& net = network();
    childAccumulator = parentAccumulator;
    for (Color perspective : {Color::White, Color::Black}) {
        auto& values = childAccumulator.values[static_cast<std::size_t>(perspective)];
        const int count = std::max(parent.piece_count(), child.piece_count());
        for (int id = 0; id < count; ++id) {
            const PieceState empty{};
            const PieceState& before = id < parent.piece_count() ? parent.piece(id) : empty;
            const PieceState& after = id < child.piece_count() ? child.piece(id) : empty;
            if (same_piece(before, after) &&
                (id != parent.forced_piece()) == (id != child.forced_piece()))
                continue;
            each_piece_feature(parent, id, perspective, before,
              [&](std::size_t feature) { add_feature(net, feature, -1, values); });
            each_piece_feature(child, id, perspective, after,
              [&](std::size_t feature) { add_feature(net, feature, 1, values); });
        }
        each_global_feature(parent, perspective,
          [&](std::size_t feature) { add_feature(net, feature, -1, values); });
        each_global_feature(child, perspective,
          [&](std::size_t feature) { add_feature(net, feature, 1, values); });
    }
}

int UltimateNnue::correction(const Position& position, const Accumulator& accumulator) {
    const Network& net = network();
    std::int64_t output = net.outputBias;
    for (std::size_t perspective = 0; perspective < 2; ++perspective)
        for (std::size_t hidden = 0; hidden < HiddenDimensions; ++hidden) {
            const std::int32_t activation = std::clamp(
              accumulator.values[perspective][hidden], 0,
              static_cast<std::int32_t>(net.activationScale));
            output += std::int64_t(activation) * net.outputWeights[perspective][hidden];
        }
    const int whiteScore = static_cast<int>(output / net.outputScale);
    return position.side_to_move() == Color::White ? whiteScore : -whiteScore;
}

}  // namespace Stockfish::Ultimate
