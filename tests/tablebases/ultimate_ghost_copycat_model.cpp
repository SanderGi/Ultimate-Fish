/* Ultimate Fish exact Copycat/Ghost adapter tests. GPLv3+. */

#include "ghost_public_extra_model.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

using namespace Stockfish::Ultimate;
using namespace Stockfish::Ultimate::GhostPublicExtra;

int main() {
    const MaterialSpec material{PieceType::Copycat, Color::White, Color::Black,
      SourceOrder::ExtraPrimary,
      HiddenAdjacentPolicy::ImpossibleWithoutForcedRelocation,
      "kcopycatkghost"};
    if (PlacementCount != 75'915'840 || StateCount != 151'831'680)
        throw std::runtime_error("compound Copycat/Ghost codec size drift");

    ConcreteState state{Color::White, 0, 79, 1, 20, 0, false};
    const std::uint32_t index = encode_index(state, material);
    const ConcreteState decoded = decode_index(index, material);
    if (decoded.whiteKing != state.whiteKing ||
        decoded.blackKing != state.blackKing || decoded.first != state.first ||
        decoded.second != state.second || decoded.side != state.side ||
        !valid_concrete_world(decoded, material))
        throw std::runtime_error("compound Copycat/Ghost codec residual");

    ConcreteState sentinel = state;
    sentinel.second = 6;  // Copycat at b1 derives its clone at g1.
    if (valid_concrete_world(sentinel, material))
        throw std::runtime_error("Copycat clone/Ghost collision was admitted");
    sentinel = state;
    sentinel.whiteKing = 6;
    if (valid_concrete_world(sentinel, material))
        throw std::runtime_error("Copycat clone/King collision was admitted");

    const Position position = make_position(state, material);
    if (position.piece_count() != 5 ||
        classify_child(position, material).domain != ChildDomain::SameClass)
        throw std::runtime_error("Copycat clone escaped same-class scan");

    const auto actions = legal_action_keys(position);
    ConcreteState reflected = transform_state(
      state, RectangleTransform::Horizontal);
    const auto reflectedActions = legal_action_keys(
      make_position(reflected, material));
    for (const ActionKey& action : actions) {
        if (action.from != state.first && action.from != 6)
            continue;
        if (action.auxiliary == 0)
            continue;  // The context-free helper cannot distinguish a1 from
                       // the ordinary auxiliary sentinel; the exact compiler
                       // binds this case using the Copycat actor square.
        const ActionKey transformed = transform_action(
          action, RectangleTransform::Horizontal);
        if (!std::binary_search(reflectedActions.begin(),
                                reflectedActions.end(), transformed))
            throw std::runtime_error(
              "Copycat semantic auxiliary did not transform as a square: " +
              std::to_string(action.from) + "," +
              std::to_string(action.to) + "," +
              std::to_string(action.auxiliary) + " -> " +
              std::to_string(transformed.from) + "," +
              std::to_string(transformed.to) + "," +
              std::to_string(transformed.auxiliary));
    }
    std::cout << "ghost_copycat_model codec_states " << StateCount
              << " physical_pieces 5 sentinel_residual 0 action_residual 0\n";
}
