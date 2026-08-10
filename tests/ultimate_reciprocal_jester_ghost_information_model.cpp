/* Exact reciprocal Jester/Ghost information-model regression. GPLv3+. */

#include "reciprocal_jester_ghost_information_model.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Stockfish::Ultimate;
using namespace Stockfish::Ultimate::ReciprocalJesterGhostInformation;

namespace {

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

PublicFrame frame(Color side) {
    return {side,
      static_cast<std::uint8_t>(Position::square_from_name("h10")),
      static_cast<std::uint8_t>(Position::square_from_name("a1")),
      static_cast<std::uint8_t>(Position::square_from_name("c1")), {}};
}

void test_material_and_lowering() {
    const PublicFrame publicFrame = frame(Color::White);
    const ProductWorld world{true,
      static_cast<std::uint8_t>(Position::square_from_name("e7"))};
    Position position =
      ReciprocalJesterGhostInformation::make_position(publicFrame, world);
    require(position.piece_on(Position::square_from_name("a1")) !=
              Position::NoPiece &&
            position.piece_on(Position::square_from_name("c1")) !=
              Position::NoPiece &&
            position.piece_on(Position::square_from_name("e7")) !=
              Position::NoPiece,
            "reciprocal material placement is incomplete");
    const int ghost = position.piece_on(Position::square_from_name("e7"));
    require(position.piece(ghost).type == PieceType::Ghost &&
            position.piece(ghost).color == Color::Black &&
            !position.piece(ghost).visible,
            "reciprocal Ghost must be hidden and Black");
    require(classify_child(position).domain == ChildDomain::SameClass,
            "complete reciprocal material is same-class");

    Position lowerJester;
    lowerJester.add_piece(PieceType::King, Color::White,
                          Position::square_from_name("a1"));
    lowerJester.add_piece(PieceType::Jester, Color::White,
                          Position::square_from_name("c1"));
    lowerJester.add_piece(PieceType::King, Color::Black,
                          Position::square_from_name("h10"));
    require(classify_child(lowerJester).domain == ChildDomain::LowerJester,
            "Ghost capture lowers to exact K+Jester-v-K");

    Position lowerGhost;
    lowerGhost.add_piece(PieceType::King, Color::White,
                         Position::square_from_name("a1"));
    lowerGhost.add_piece(PieceType::King, Color::Black,
                         Position::square_from_name("h10"));
    const int lowerGhostId = lowerGhost.add_piece(
      PieceType::Ghost, Color::Black, Position::square_from_name("e7"));
    lowerGhost.piece(lowerGhostId).visible = false;
    lowerGhost.set_side_to_move(Color::Black);
    const ClassifiedChild classified = classify_child(lowerGhost);
    require(classified.domain == ChildDomain::LowerGhost,
            "Jester capture lowers to exact opposing K+Ghost");
    const LowerGhostState decoded = decode_lower_ghost(classified.index);
    require(decoded.side == Role::GhostOwner &&
            decoded.ownerKing == Position::square_from_name("h10") &&
            decoded.observerKing == Position::square_from_name("a1") &&
            decoded.ghost == Position::square_from_name("e7"),
            "opposing lower Ghost roles are normalized to its Black owner");
}

void test_owned_private_partitions() {
    const std::vector<ProductWorld> worlds{
      {true, static_cast<std::uint8_t>(Position::square_from_name("e7"))},
      {true, static_cast<std::uint8_t>(Position::square_from_name("f7"))},
      {false, static_cast<std::uint8_t>(Position::square_from_name("e7"))},
      {false, static_cast<std::uint8_t>(Position::square_from_name("f7"))}};
    for (const Color mover : {Color::White, Color::Black}) {
        const PublicFrame publicFrame = frame(mover);
        const auto cells =
          ReciprocalJesterGhostInformation::decision_partition(
            publicFrame, worlds);
        unsigned conserved = 0;
        for (const DecisionBucket& cell : cells) {
            conserved += cell.worlds.count();
            bool firstRoyal = false;
            bool secondRoyal = false;
            int ghostSquare = Position::NoSquare;
            for (const ProductWorld& world :
                 JesterGhostInformation::decode_product_mask(
                   publicFrame, cell.worlds)) {
                firstRoyal = firstRoyal || world.kingAtFirst;
                secondRoyal = secondRoyal || !world.kingAtFirst;
                if (ghostSquare == Position::NoSquare)
                    ghostSquare = world.ghost;
                if (mover == Color::Black)
                    require(ghostSquare == world.ghost,
                            "Black private cell merged owned Ghost squares");
            }
            if (mover == Color::White)
                require(!(firstRoyal && secondRoyal),
                        "White private cell merged royal assignments");
        }
        require(conserved == worlds.size(),
                "reciprocal private cells do not conserve the relation");
    }
}

void test_transition_private_facts_and_admission() {
    PublicFrame publicFrame = frame(Color::White);
    const std::vector<ProductWorld> worlds{
      {true, static_cast<std::uint8_t>(Position::square_from_name("e7"))},
      {true, static_cast<std::uint8_t>(Position::square_from_name("f7"))},
      {false, static_cast<std::uint8_t>(Position::square_from_name("e7"))},
      {false, static_cast<std::uint8_t>(Position::square_from_name("f7"))}};
    const Position first = ReciprocalJesterGhostInformation::make_position(
      publicFrame, worlds.front());
    require(!first.legal_moves().empty(), "reciprocal fixture has no action");
    const ActionKey action = action_key(first.legal_moves().front());
    for (const Color observer : {Color::White, Color::Black}) {
        const auto buckets =
          ReciprocalJesterGhostInformation::transition_partition(
            publicFrame, worlds, action, observer);
        for (const auto& bucket : buckets) {
            bool firstRoyal = false;
            bool secondRoyal = false;
            int ghostSquare = Position::NoSquare;
            for (const TransitionWorld& item : bucket) {
                firstRoyal = firstRoyal || item.source.kingAtFirst;
                secondRoyal = secondRoyal || !item.source.kingAtFirst;
                if (ghostSquare == Position::NoSquare)
                    ghostSquare = item.source.ghost;
                if (observer == Color::Black)
                    require(ghostSquare == item.source.ghost,
                            "Black transition merged owned Ghost squares");
            }
            if (observer == Color::White)
                require(!(firstRoyal && secondRoyal),
                        "White transition merged owned royal assignments");
        }
    }

    ProductWorld adjacentToWhite{true,
      static_cast<std::uint8_t>(Position::square_from_name("b1"))};
    ProductWorld adjacentToBlack{true,
      static_cast<std::uint8_t>(Position::square_from_name("g10"))};
    require(ReciprocalJesterGhostInformation::fresh_world_admission(
              publicFrame, adjacentToWhite) ==
              AdmissionVerdict::Reject,
            "hidden Ghost beside the White observer King must be rejected");
    require(ReciprocalJesterGhostInformation::fresh_world_admission(
              publicFrame, adjacentToBlack) ==
              AdmissionVerdict::Admit,
            "hidden Ghost beside its own Black King remains causally possible");
}

}  // namespace

int main() {
    try {
        test_material_and_lowering();
        test_owned_private_partitions();
        test_transition_private_facts_and_admission();
        std::cout << "Reciprocal Jester/Ghost information model tests passed\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Reciprocal Jester/Ghost model test failed: "
                  << error.what() << '\n';
        return 1;
    }
}
