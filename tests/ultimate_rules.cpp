#include "../src/ultimate/position.h"
#include "../src/ultimate/draft.h"
#include "../src/ultimate/tablebases/information.h"
#include "../src/ultimate/search.h"
#include "../src/ultimate/tablebases/tablebase_probe.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>

using namespace Stockfish::Ultimate;

namespace {

int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

Move require_move(const Position& position, const std::string& notation) {
    const auto move = position.move_from_string(notation);
    expect(move.has_value(), "expected legal move " + notation);
    return move.value_or(Move{});
}

void test_roster_and_position_round_trip() {
    expect(Position::BoardFiles == 8 && Position::BoardRanks == 10 &&
           Position::square_from_name("a10") != Position::NoSquare &&
           Position::square_from_name("h11") == Position::NoSquare,
           "native board geometry is eight files by ten ranks");
    for (int value = 0; value < static_cast<int>(PieceType::Count); ++value) {
        const auto type = static_cast<PieceType>(value);
        expect(Position::type_from_name(Position::type_name(type)) == type,
               "piece enum/name round trip " + std::to_string(value));
    }

    Position position;
    position.add_piece(PieceType::King, Color::White, Position::square_from_name("e1"));
    position.add_piece(PieceType::King, Color::Black, Position::square_from_name("e10"));
    const int ghost = position.add_piece(PieceType::Ghost, Color::White, Position::square_from_name("c3"));
    position.piece(ghost).visible = false;
    position.piece(ghost).action = 7;

    Position parsed;
    std::string error;
    expect(parsed.set_upn(position.upn(), &error), "UPN parses: " + error);
    expect(parsed.upn() == position.upn(), "UPN round trip is exact");
    expect(parsed.key() == position.key(), "UPN round trip preserves hash");

    Position linked;
    linked.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    linked.add_piece(PieceType::King, Color::Black, Position::square_from_name("h10"));
    const int disposable = linked.add_piece(PieceType::Pawn, Color::White,
                                             Position::square_from_name("b2"));
    linked.add_piece(PieceType::Copycat, Color::White, Position::square_from_name("c3"));
    linked.remove_piece(disposable);
    Position linkedParsed;
    expect(linkedParsed.set_upn(linked.upn(), &error), "linked UPN with a dead ID parses: " + error);
    const int copycat = linkedParsed.piece_on(Position::square_from_name("c3"));
    expect(copycat != Position::NoPiece && linkedParsed.piece(copycat).link != Position::NoPiece &&
           linkedParsed.piece(linkedParsed.piece(copycat).link).square ==
             Position::square_from_name("f3"),
           "UPN remaps CopyCat links after omitted captured pieces");

    Position singleton;
    const std::string singletonUpn =
      "w;hm=92;fm=33;ep=-;cont=0;forced=-1;epv=-1;win=-;"
      "king,w,a1,0,0,0,0,0,1,-1,1,-1,0;"
      "copycat,w,f2,0,0,0,0,1,1,-1,1,-1,0;"
      "giant,w,c2,0,0,0,0,1,1,-1,1,-1,0;"
      "king,b,c9,0,0,0,0,1,1,-1,1,-1,0";
    expect(singleton.set_upn(singletonUpn, &error),
           "explicit legacy/arbitrary CopyCat singleton parses: " + error);
    expect(singleton.upn() == singletonUpn,
           "explicit CopyCat link=-1 does not regenerate an occupied mirror clone");

    Position recycled;
    recycled.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    recycled.add_piece(PieceType::King, Color::Black, Position::square_from_name("h10"));
    for (int cycle = 0; cycle < Position::MaxPieces * 3; ++cycle) {
        const int generated = recycled.add_piece(PieceType::Minion, Color::White,
                                                  Position::square_from_name("b2"));
        expect(generated != Position::NoPiece,
               "generated-piece slot remains available in a long game");
        recycled.remove_piece(generated);
    }
    expect(recycled.piece_count() == 3,
           "captured generated-piece slots are recycled instead of exhausting storage");

    Position invalid;
    expect(!invalid.set_upn("w;ep=z9;king,w,a1", &error) &&
           error == "invalid en-passant square",
           "custom UPN rejects a malformed en-passant coordinate");
    expect(!invalid.set_upn("w;king,w,a1,0,0,0,0,2", &error) &&
           error == "piece state field is out of range",
           "custom UPN rejects non-Boolean and overflowing state fields");
}

void test_chess_style_move_notation() {
    const auto kings = [](Position& position, std::string_view white,
                          std::string_view black) {
        position.add_piece(PieceType::King, Color::White,
                           Position::square_from_name(white));
        position.add_piece(PieceType::King, Color::Black,
                           Position::square_from_name(black));
    };

    Position queenCapture;
    kings(queenCapture, "a1", "a8");
    queenCapture.add_piece(PieceType::Queen, Color::White,
                           Position::square_from_name("h9"));
    queenCapture.add_piece(PieceType::Pawn, Color::Black,
                           Position::square_from_name("h10"));
    expect(queenCapture.move_to_display_string(
             require_move(queenCapture, "h9-h10")) == "Qxh10",
           "display notation uses the piece letter, capture marker, and destination");

    Position ambiguousRooks;
    kings(ambiguousRooks, "b1", "g10");
    ambiguousRooks.add_piece(PieceType::Rook, Color::White,
                             Position::square_from_name("a2"));
    ambiguousRooks.add_piece(PieceType::Rook, Color::White,
                             Position::square_from_name("h2"));
    expect(ambiguousRooks.move_to_display_string(
             require_move(ambiguousRooks, "a2-d2")) == "Rad2" &&
           ambiguousRooks.move_to_display_string(
             require_move(ambiguousRooks, "h2-d2")) == "Rhd2",
           "display notation uses orthodox file disambiguation");

    Position promotion;
    kings(promotion, "a1", "a8");
    promotion.add_piece(PieceType::Pawn, Color::White,
                        Position::square_from_name("h9"));
    expect(promotion.move_to_display_string(
             require_move(promotion, "h9-h10")) == "h10=Q",
           "auto-queen promotion uses equals-Q notation");

    Position check;
    kings(check, "a1", "e10");
    check.add_piece(PieceType::Rook, Color::White,
                    Position::square_from_name("e2"));
    check.add_piece(PieceType::Pawn, Color::Black,
                    Position::square_from_name("e8"));
    expect(check.move_to_display_string(require_move(check, "e2-e8")) ==
             "Rxe8+",
           "a checking move receives the plus suffix");

    Position mate;
    kings(mate, "c8", "a10");
    mate.add_piece(PieceType::Queen, Color::White,
                   Position::square_from_name("b8"));
    expect(mate.move_to_display_string(require_move(mate, "b8-b9")) ==
             "Qb9#",
           "a mating move receives the hash suffix");

    Position castle;
    kings(castle, "d2", "g10");
    castle.add_piece(PieceType::Rook, Color::White,
                     Position::square_from_name("h2"));
    expect(castle.move_to_display_string(require_move(castle, "d2-f2")) ==
             "0-0",
           "an unambiguous native castle uses zero-zero notation");
    castle.add_piece(PieceType::Rook, Color::White,
                     Position::square_from_name("a2"));
    expect(castle.move_to_display_string(require_move(castle, "d2-f2")) ==
             "0-0h2" &&
           castle.move_to_display_string(require_move(castle, "d2-b2")) ==
             "0-0a2",
           "multiple native castles append the participating Rook square");

    Position special;
    kings(special, "a1", "h10");
    special.add_piece(PieceType::Mage, Color::White,
                      Position::square_from_name("c2"));
    special.add_piece(PieceType::Rook, Color::White,
                      Position::square_from_name("f6"));
    expect(special.move_to_display_string(require_move(special, "c2~f6")) ==
             "Mf6",
           "special actions use the clicked destination without protocol punctuation");

    Position sniper;
    kings(sniper, "a1", "h8");
    sniper.add_piece(PieceType::Sniper, Color::White,
                     Position::square_from_name("d2"));
    sniper.add_piece(PieceType::Rook, Color::Black,
                     Position::square_from_name("d7"));
    expect(sniper.move_to_display_string(require_move(sniper, "d2xd7")) ==
             "SNxd7",
           "Sniper shots render as ordinary piece captures");

    Position royal;
    kings(royal, "h1", "h10");
    royal.add_piece(PieceType::Jester, Color::White,
                    Position::square_from_name("c2"));
    const Move jesterMove = require_move(royal, "c2-d3");
    expect(royal.move_to_display_string(jesterMove) == "Jd3" &&
           royal.move_to_display_string(jesterMove, true) == "Kd3",
           "public notation conceals Jester identity as King");

    Position ghost;
    kings(ghost, "a1", "h10");
    const int hiddenGhost = ghost.add_piece(PieceType::Ghost, Color::White,
                                            Position::square_from_name("c3"));
    ghost.piece(hiddenGhost).visible = false;
    const Move hiddenMove = require_move(ghost, "c3-d4");
    expect(ghost.move_to_display_string(hiddenMove) == "GHd4" &&
           ghost.move_to_display_string(hiddenMove, true) == "GH",
           "public notation conceals an unrevealed Ghost destination");
    ghost.piece(hiddenGhost).visible = true;
    expect(ghost.move_to_display_string(hiddenMove, true) == "GHd4",
           "public notation includes a revealed Ghost destination");
}

void test_bomb_and_undo() {
    Position position;
    position.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    position.add_piece(PieceType::King, Color::Black, Position::square_from_name("h8"));
    position.add_piece(PieceType::Bomb, Color::White, Position::square_from_name("d4"));
    position.add_piece(PieceType::Pawn, Color::Black, Position::square_from_name("d6"));
    position.add_piece(PieceType::Rook, Color::Black, Position::square_from_name("e6"));
    const std::string before = position.upn();

    expect(position.move_from_string("d4-f4").has_value(), "bomb moves two orthogonally");
    expect(!position.move_from_string("d4-f6").has_value(), "bomb does not move two diagonally");

    Undo undo;
    expect(position.make_move(require_move(position, "d4-d6"), undo), "bomb capture applies");
    expect(position.piece_on(Position::square_from_name("d6")) == Position::NoPiece,
           "bomb is removed by its explosion");
    expect(position.piece_on(Position::square_from_name("e6")) == Position::NoPiece,
           "adjacent character is removed by bomb explosion");
    position.undo_move(undo);
    expect(position.upn() == before, "bomb move undo is byte-for-byte equivalent");

    Position bombVictim;
    bombVictim.add_piece(PieceType::King, Color::White,
                         Position::square_from_name("a1"));
    bombVictim.add_piece(PieceType::King, Color::Black,
                         Position::square_from_name("h10"));
    bombVictim.add_piece(PieceType::Queen, Color::White,
                         Position::square_from_name("f1"));
    bombVictim.add_piece(PieceType::Bomb, Color::Black,
                         Position::square_from_name("f8"));
    bombVictim.add_piece(PieceType::Pawn, Color::Black,
                         Position::square_from_name("g8"));
    Undo longRangeBlast;
    expect(bombVictim.make_move(require_move(bombVictim, "f1-f8"), longRangeBlast),
           "long-range capture of a bomb applies");
    expect(bombVictim.piece_on(Position::square_from_name("f1")) == Position::NoPiece &&
           bombVictim.piece_on(Position::square_from_name("f8")) == Position::NoPiece &&
           bombVictim.piece_on(Position::square_from_name("g8")) == Position::NoPiece,
           "captured bomb removes the landing attacker and adjacent characters");

    Position checkerBlast;
    checkerBlast.add_piece(PieceType::King, Color::White,
                           Position::square_from_name("h1"));
    checkerBlast.add_piece(PieceType::King, Color::Black,
                           Position::square_from_name("h10"));
    const int checker = checkerBlast.add_piece(PieceType::Checker, Color::White,
                                               Position::square_from_name("b2"));
    const int jumpedBomb = checkerBlast.add_piece(PieceType::Bomb, Color::Black,
                                                  Position::square_from_name("c3"));
    const int nearJumpedBomb = checkerBlast.add_piece(PieceType::Pawn, Color::Black,
                                                      Position::square_from_name("b3"));
    Undo checkerExplosion;
    expect(checkerBlast.make_move(require_move(checkerBlast, "b2-d4"), checkerExplosion) &&
           !checkerBlast.piece(checker).alive && !checkerBlast.piece(jumpedBomb).alive &&
           !checkerBlast.piece(nearJumpedBomb).alive,
           "Checker capture explodes a jumped Bomb around the Bomb square, not its landing square");

    Position giantBlast;
    giantBlast.add_piece(PieceType::King, Color::White,
                         Position::square_from_name("h1"));
    giantBlast.add_piece(PieceType::King, Color::Black,
                         Position::square_from_name("h10"));
    const int giant = giantBlast.add_piece(PieceType::Giant, Color::White,
                                           Position::square_from_name("a1"));
    const int footprintBomb = giantBlast.add_piece(PieceType::Bomb, Color::Black,
                                                   Position::square_from_name("d2"));
    const int beyondAnchor = giantBlast.add_piece(PieceType::Pawn, Color::Black,
                                                  Position::square_from_name("e3"));
    Undo giantExplosion;
    expect(giantBlast.make_move(require_move(giantBlast, "a1-c1"), giantExplosion) &&
           !giantBlast.piece(giant).alive && !giantBlast.piece(footprintBomb).alive &&
           !giantBlast.piece(beyondAnchor).alive,
           "Giant footprint capture explodes a Bomb around its occupied tile, not the Giant anchor");

    Position chainBlast;
    chainBlast.add_piece(PieceType::King, Color::White,
                         Position::square_from_name("a1"));
    chainBlast.add_piece(PieceType::King, Color::Black,
                         Position::square_from_name("h10"));
    chainBlast.add_piece(PieceType::Queen, Color::White,
                         Position::square_from_name("a3"));
    chainBlast.add_piece(PieceType::Bomb, Color::Black,
                         Position::square_from_name("c3"));
    const int chainedBomb = chainBlast.add_piece(PieceType::Bomb, Color::Black,
                                                 Position::square_from_name("d4"));
    const int chainOnlyVictim = chainBlast.add_piece(PieceType::Pawn, Color::Black,
                                                     Position::square_from_name("e5"));
    Undo chainExplosion;
    expect(chainBlast.make_move(require_move(chainBlast, "a3-c3"), chainExplosion) &&
           !chainBlast.piece(chainedBomb).alive && !chainBlast.piece(chainOnlyVictim).alive,
           "a Bomb killed by another Bomb triggers its own radius-one chain explosion");

    // Live Unranked regression: a Sniper on f10 shot a Bomb on f2, survived at
    // its origin, reloaded, and later shot the Penguin on f8. Unlike ordinary
    // capturers, the Sniper never enters the remote Bomb's blast radius.
    Position rangedBomb;
    rangedBomb.add_piece(PieceType::King, Color::White,
                         Position::square_from_name("a1"));
    const int remoteBomb = rangedBomb.add_piece(PieceType::Bomb, Color::White,
                                                Position::square_from_name("f2"));
    rangedBomb.add_piece(PieceType::King, Color::Black,
                         Position::square_from_name("h10"));
    const int remoteSniper = rangedBomb.add_piece(PieceType::Sniper, Color::Black,
                                                  Position::square_from_name("f10"));
    rangedBomb.set_side_to_move(Color::Black);
    Undo rangedExplosion;
    expect(rangedBomb.make_move(require_move(rangedBomb, "f10xf2"), rangedExplosion) &&
           !rangedBomb.piece(remoteBomb).alive && rangedBomb.piece(remoteSniper).alive &&
           rangedBomb.piece(remoteSniper).square == Position::square_from_name("f10"),
           "a distant Sniper survives the radius-one blast of its Bomb target");

    Position penguinShot;
    penguinShot.add_piece(PieceType::King, Color::White,
                          Position::square_from_name("a1"));
    penguinShot.add_piece(PieceType::Penguin, Color::White,
                          Position::square_from_name("f8"));
    penguinShot.add_piece(PieceType::King, Color::Black,
                          Position::square_from_name("h10"));
    penguinShot.add_piece(PieceType::Sniper, Color::Black,
                          Position::square_from_name("f10"));
    penguinShot.set_side_to_move(Color::Black);
    expect(penguinShot.move_from_string("f10xf8").has_value(),
           "Sniper can shoot the first visible Penguin on its forward file");
}

void test_bomb_check_legality() {
    // Regression from the 2026-08-05 Unranked loss.  The apparent King escape
    // b1-c2 is illegal: Black's Rook can capture the Bomb on d1, whose radius-
    // one blast removes the real King on c2.
    Position position;
    position.add_piece(PieceType::King, Color::White,
                       Position::square_from_name("b1"));
    position.add_piece(PieceType::Bomb, Color::White,
                       Position::square_from_name("d1"));
    position.add_piece(PieceType::Rook, Color::Black,
                       Position::square_from_name("a1"));
    position.add_piece(PieceType::King, Color::Black,
                       Position::square_from_name("h10"));

    expect(!position.move_from_string("b1-c2").has_value(),
           "King cannot escape onto a square killed by a captured Bomb");

    Position control;
    control.add_piece(PieceType::King, Color::White,
                      Position::square_from_name("b1"));
    control.add_piece(PieceType::Ghost, Color::White,
                      Position::square_from_name("d1"));
    control.add_piece(PieceType::Bomb, Color::White,
                      Position::square_from_name("f1"));
    control.add_piece(PieceType::Rook, Color::Black,
                      Position::square_from_name("a1"));
    control.add_piece(PieceType::King, Color::Black,
                      Position::square_from_name("h10"));
    expect(control.move_from_string("b1-c2").has_value(),
           "same King escape survives when d1 is a Ghost rather than a Bomb");

    // Search check detection uses a geometric prefilter before invoking the
    // full native simulator. Exercise every ordinary attack geometry so a
    // future piece change cannot make that performance shortcut drop checks.
    const auto expectOrdinaryCheck = [](PieceType type, const char* square,
                                        const char* description) {
        Position checked;
        checked.add_piece(PieceType::King, Color::White,
                          Position::square_from_name("e5"));
        checked.add_piece(PieceType::Pawn, Color::White,
                          Position::square_from_name("a2"));
        if (type != PieceType::King)
            checked.add_piece(PieceType::King, Color::Black,
                              Position::square_from_name("h10"));
        checked.add_piece(type, Color::Black, Position::square_from_name(square));
        expect(!checked.move_from_string("a2-a3").has_value(), description);
    };
    expectOrdinaryCheck(PieceType::King, "e6", "King geometry survives threat prefilter");
    expectOrdinaryCheck(PieceType::Jester, "e6", "Jester geometry survives threat prefilter");
    expectOrdinaryCheck(PieceType::Parasite, "e6", "Parasite geometry survives threat prefilter");
    expectOrdinaryCheck(PieceType::Prince, "e6", "Prince geometry survives threat prefilter");
    expectOrdinaryCheck(PieceType::Knight, "c4", "Knight geometry survives threat prefilter");
    expectOrdinaryCheck(PieceType::Pawn, "d6", "Pawn geometry survives threat prefilter");
    expectOrdinaryCheck(PieceType::Queen, "e9", "Queen geometry survives threat prefilter");
    expectOrdinaryCheck(PieceType::Rook, "e9", "Rook geometry survives threat prefilter");
    expectOrdinaryCheck(PieceType::Bishop, "b8", "Bishop geometry survives threat prefilter");
    expectOrdinaryCheck(PieceType::Berserker, "d6",
                        "Berserker geometry survives threat prefilter");
    expectOrdinaryCheck(PieceType::Ninja, "e8", "Ninja geometry survives threat prefilter");
    expectOrdinaryCheck(PieceType::Turtle, "e6", "Turtle geometry survives threat prefilter");
    expectOrdinaryCheck(PieceType::Sniper, "e9", "Sniper geometry survives threat prefilter");
    expectOrdinaryCheck(PieceType::Dragon, "b8", "Dragon geometry survives threat prefilter");

    // Royal threats are not limited to an attacker landing on the King's
    // square. Keep the optimized threat generator honest for every native
    // action in which a quiet-looking move knocks the King out indirectly.
    Position mageGiant;
    mageGiant.add_piece(PieceType::King, Color::White,
                        Position::square_from_name("e5"));
    mageGiant.add_piece(PieceType::Pawn, Color::White,
                        Position::square_from_name("a2"));
    mageGiant.add_piece(PieceType::King, Color::Black,
                        Position::square_from_name("h10"));
    mageGiant.add_piece(PieceType::Mage, Color::Black,
                        Position::square_from_name("d4"));
    mageGiant.add_piece(PieceType::Giant, Color::Black,
                        Position::square_from_name("f6"));
    expect(!mageGiant.move_from_string("a2-a3").has_value(),
           "Mage/Giant relocation that knocks out the King counts as check");

    Position fishermanGiant;
    fishermanGiant.add_piece(PieceType::King, Color::White,
                             Position::square_from_name("d5"));
    fishermanGiant.add_piece(PieceType::Pawn, Color::White,
                             Position::square_from_name("a2"));
    fishermanGiant.add_piece(PieceType::King, Color::Black,
                             Position::square_from_name("h10"));
    fishermanGiant.add_piece(PieceType::Fisherman, Color::Black,
                             Position::square_from_name("c3"));
    fishermanGiant.add_piece(PieceType::Giant, Color::Black,
                             Position::square_from_name("f6"));
    expect(!fishermanGiant.move_from_string("a2-a3").has_value(),
           "Fisherman/Giant pull that knocks out the King counts as check");

    Position copycatPartner;
    copycatPartner.add_piece(PieceType::King, Color::White,
                             Position::square_from_name("e5"));
    copycatPartner.add_piece(PieceType::Pawn, Color::White,
                             Position::square_from_name("a2"));
    copycatPartner.add_piece(PieceType::King, Color::Black,
                             Position::square_from_name("h10"));
    copycatPartner.add_piece(PieceType::Copycat, Color::Black,
                             Position::square_from_name("c6"));
    expect(!copycatPartner.move_from_string("a2-a3").has_value(),
           "quiet Copycat half with a capturing mirrored partner counts as check");

    // Captured Ranked evidence: an opponent that still owned a Jester legally
    // moved a linked CopyCat while its real King remained attacked. Native
    // play allows either royal silhouette to be in check until the Jester is
    // gone; only actual real-King capture ends the game.
    Position hiddenRoyal;
    hiddenRoyal.add_piece(PieceType::King, Color::White,
                          Position::square_from_name("b1"));
    hiddenRoyal.add_piece(PieceType::Jester, Color::White,
                          Position::square_from_name("a1"));
    hiddenRoyal.add_piece(PieceType::Copycat, Color::White,
                          Position::square_from_name("c2"));
    hiddenRoyal.add_piece(PieceType::King, Color::Black,
                          Position::square_from_name("h10"));
    hiddenRoyal.add_piece(PieceType::Rook, Color::Black,
                          Position::square_from_name("b10"));
    expect(hiddenRoyal.move_from_string("c2-d3").has_value(),
           "a side owning a Jester may make the captured linked Copycat move "
           "while its real King remains in check");

    Position revealedRoyal;
    revealedRoyal.add_piece(PieceType::King, Color::White,
                            Position::square_from_name("b1"));
    revealedRoyal.add_piece(PieceType::Copycat, Color::White,
                            Position::square_from_name("c2"));
    revealedRoyal.add_piece(PieceType::King, Color::Black,
                            Position::square_from_name("h10"));
    revealedRoyal.add_piece(PieceType::Rook, Color::Black,
                            Position::square_from_name("b10"));
    expect(!revealedRoyal.move_from_string("c2-d3").has_value(),
           "ordinary check legality resumes when the Jester is absent");

    Position protectedRoyal;
    protectedRoyal.add_piece(PieceType::King, Color::White,
                             Position::square_from_name("e5"));
    protectedRoyal.add_piece(PieceType::Angel, Color::White,
                             Position::square_from_name("b2"));
    protectedRoyal.add_piece(PieceType::Pawn, Color::White,
                             Position::square_from_name("a2"));
    protectedRoyal.add_piece(PieceType::King, Color::Black,
                             Position::square_from_name("h10"));
    protectedRoyal.add_piece(PieceType::Rook, Color::Black,
                             Position::square_from_name("e10"));
    Undo royalProtection;
    expect(protectedRoyal.make_move(
               require_move(protectedRoyal, "b2&e5"), royalProtection),
           "Angel attachment can resolve an otherwise direct royal threat");
    protectedRoyal.set_side_to_move(Color::White);
    expect(protectedRoyal.move_from_string("a2-a3").has_value(),
           "direct-threat fast path still simulates an Angel-protected King");

    Position disappearingDecoy;
    disappearingDecoy.add_piece(PieceType::King, Color::White,
                                 Position::square_from_name("e1"));
    disappearingDecoy.add_piece(PieceType::Jester, Color::White,
                                 Position::square_from_name("b2"));
    disappearingDecoy.add_piece(PieceType::Bomb, Color::White,
                                 Position::square_from_name("a2"));
    disappearingDecoy.add_piece(PieceType::King, Color::Black,
                                 Position::square_from_name("h10"));
    disappearingDecoy.add_piece(PieceType::Rook, Color::Black,
                                 Position::square_from_name("e10"));
    disappearingDecoy.add_piece(PieceType::Pawn, Color::Black,
                                 Position::square_from_name("a3"));
    expect(!disappearingDecoy.move_from_string("a2-a3").has_value(),
           "check filtering resumes when the candidate move blasts the Jester");
}

void test_native_castling() {
    Position position;
    const int king = position.add_piece(PieceType::King, Color::White,
                                        Position::square_from_name("d2"));
    const int rook = position.add_piece(PieceType::Rook, Color::White,
                                        Position::square_from_name("h2"));
    position.add_piece(PieceType::King, Color::Black,
                       Position::square_from_name("a10"));
    const std::string before = position.upn();
    const Move castle = require_move(position, "d2-f2");
    expect(castle.kind == MoveKind::Castle && castle.auxiliary == rook,
           "King castle records the native Rook submove");
    Undo undo;
    expect(position.make_move(castle, undo) &&
           position.piece(king).square == Position::square_from_name("f2") &&
           position.piece(rook).square == Position::square_from_name("e2") &&
           position.piece(king).moved && position.piece(rook).moved,
           "King moves two files and Rook lands on the passed square");
    position.undo_move(undo);
    expect(position.upn() == before, "castling undo restores moved state and placement");

    Position jesterCastle;
    jesterCastle.add_piece(PieceType::King, Color::White,
                           Position::square_from_name("h1"));
    const int jester = jesterCastle.add_piece(PieceType::Jester, Color::White,
                                              Position::square_from_name("d2"));
    const int leftRook = jesterCastle.add_piece(PieceType::Rook, Color::White,
                                                Position::square_from_name("a2"));
    jesterCastle.add_piece(PieceType::King, Color::Black,
                           Position::square_from_name("h10"));
    Undo jesterUndo;
    expect(jesterCastle.make_move(require_move(jesterCastle, "d2-b2"), jesterUndo) &&
           jesterCastle.piece(jester).square == Position::square_from_name("b2") &&
           jesterCastle.piece(leftRook).square == Position::square_from_name("c2"),
           "Jester uses SimulatedKing castling in either direction");

    Position blocked;
    blocked.add_piece(PieceType::King, Color::White, Position::square_from_name("d2"));
    blocked.add_piece(PieceType::Pawn, Color::White, Position::square_from_name("f2"));
    blocked.add_piece(PieceType::Rook, Color::White, Position::square_from_name("h2"));
    blocked.add_piece(PieceType::King, Color::Black, Position::square_from_name("a10"));
    expect(!blocked.move_from_string("d2-f2").has_value(),
           "an occupied corridor blocks castling");

    Position tooClose;
    tooClose.add_piece(PieceType::King, Color::White, Position::square_from_name("d2"));
    tooClose.add_piece(PieceType::Rook, Color::White, Position::square_from_name("f2"));
    tooClose.add_piece(PieceType::King, Color::Black, Position::square_from_name("a10"));
    expect(!tooClose.move_from_string("d2-f2").has_value(),
           "a Rook only two files away is too close to castle");

    Position movedKing;
    const int usedKing = movedKing.add_piece(PieceType::King, Color::White,
                                             Position::square_from_name("d2"));
    movedKing.add_piece(PieceType::Rook, Color::White, Position::square_from_name("h2"));
    movedKing.add_piece(PieceType::King, Color::Black, Position::square_from_name("a10"));
    movedKing.piece(usedKing).moved = true;
    expect(!movedKing.move_from_string("d2-f2").has_value(),
           "a previously moved royal cannot castle");

    Position movedRook;
    movedRook.add_piece(PieceType::King, Color::White, Position::square_from_name("d2"));
    const int usedRook = movedRook.add_piece(PieceType::Rook, Color::White,
                                             Position::square_from_name("h2"));
    movedRook.add_piece(PieceType::King, Color::Black, Position::square_from_name("a10"));
    movedRook.piece(usedRook).moved = true;
    expect(!movedRook.move_from_string("d2-f2").has_value(),
           "a previously moved Rook cannot castle");

    Position throughCheck;
    throughCheck.add_piece(PieceType::King, Color::White,
                           Position::square_from_name("d2"));
    throughCheck.add_piece(PieceType::Rook, Color::White,
                           Position::square_from_name("h2"));
    throughCheck.add_piece(PieceType::King, Color::Black,
                           Position::square_from_name("a10"));
    throughCheck.add_piece(PieceType::Rook, Color::Black,
                           Position::square_from_name("e10"));
    expect(throughCheck.move_from_string("d2-f2").has_value(),
           "native castle may cross an attacked square");

    Position outOfCheck;
    outOfCheck.add_piece(PieceType::King, Color::White,
                         Position::square_from_name("d2"));
    outOfCheck.add_piece(PieceType::Rook, Color::White,
                         Position::square_from_name("h2"));
    outOfCheck.add_piece(PieceType::King, Color::Black,
                         Position::square_from_name("a10"));
    outOfCheck.add_piece(PieceType::Rook, Color::Black,
                         Position::square_from_name("d10"));
    expect(outOfCheck.move_from_string("d2-f2").has_value(),
           "native castle may move a royal out of check");

    Position intoCheck;
    intoCheck.add_piece(PieceType::King, Color::White,
                        Position::square_from_name("d2"));
    intoCheck.add_piece(PieceType::Rook, Color::White,
                        Position::square_from_name("h2"));
    intoCheck.add_piece(PieceType::King, Color::Black,
                        Position::square_from_name("a10"));
    intoCheck.add_piece(PieceType::Rook, Color::Black,
                        Position::square_from_name("f10"));
    expect(!intoCheck.move_from_string("d2-f2").has_value(),
           "the completed castle remains illegal when the real King is threatened");

    Position enemyRook;
    enemyRook.add_piece(PieceType::King, Color::White,
                        Position::square_from_name("d2"));
    const int foreignRook = enemyRook.add_piece(PieceType::Rook, Color::Black,
                                                Position::square_from_name("h2"));
    enemyRook.add_piece(PieceType::King, Color::Black,
                        Position::square_from_name("a10"));
    // Keep the foreign Rook from making the completed position check the
    // King so this assertion isolates ownership. Committing the native dot
    // advances board.turn without handing control to Player 2, forcing that
    // player to lose on their action clock.
    enemyRook.piece(foreignRook).cooldown = 2;
    const auto foreignCastle = enemyRook.move_from_string("d2-f2");
    expect(foreignCastle && foreignCastle->kind == MoveKind::Castle &&
           foreignCastle->auxiliary == foreignRook,
           "native castle scan exposes an enemy-owned Rook");
    const std::string foreignBefore = enemyRook.upn();
    Undo foreignUndo;
    expect(enemyRook.make_move(*foreignCastle, foreignUndo) &&
           enemyRook.game_over() && enemyRook.winner() == Color::White &&
           enemyRook.terminal_reason() == TerminalReason::ForcedTimeout &&
           enemyRook.legal_moves().empty(),
           "enemy-Rook castle is a terminal forced-timeout win for its mover");
    Position serializedForeign;
    std::string foreignError;
    expect(serializedForeign.set_upn(enemyRook.upn(), &foreignError) &&
           serializedForeign.game_over() &&
           serializedForeign.winner() == Color::White,
           "forced-timeout winner survives lossless UPN serialization");
    enemyRook.undo_move(foreignUndo);
    expect(enemyRook.upn() == foreignBefore,
           "enemy-Rook castle undo restores the nonterminal position");
    Search timeoutSearch(1);
    SearchLimits timeoutLimits;
    timeoutLimits.depth = 2;
    const SearchResult timeoutResult = timeoutSearch.think(enemyRook, timeoutLimits);
    expect(timeoutResult.bestMove &&
           enemyRook.move_to_string(*timeoutResult.bestMove) == "d2-f2" &&
           timeoutResult.score > 29000,
           "search recognizes the enemy-Rook castle as a forced win");
}

void test_ninja_and_mage() {
    Position position;
    position.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    position.add_piece(PieceType::King, Color::Black, Position::square_from_name("h8"));
    position.add_piece(PieceType::Ninja, Color::White, Position::square_from_name("b2"));
    position.add_piece(PieceType::Pawn, Color::White, Position::square_from_name("c3"));
    expect(position.move_from_string("b2-e5").has_value(), "ninja passes through a character");

    Position swap;
    swap.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    swap.add_piece(PieceType::King, Color::Black, Position::square_from_name("h8"));
    swap.add_piece(PieceType::Mage, Color::White, Position::square_from_name("c2"));
    swap.add_piece(PieceType::Rook, Color::White, Position::square_from_name("g6"));
    Undo undo;
    expect(swap.make_move(require_move(swap, "c2~g6"), undo), "mage swap applies");
    expect(swap.piece(swap.piece_on(Position::square_from_name("g6"))).type == PieceType::Mage,
           "mage occupies target square after swap");
    expect(swap.piece(swap.piece_on(Position::square_from_name("c2"))).type == PieceType::Rook,
           "ally occupies mage square after swap");

    Position pawnStateSwap;
    pawnStateSwap.add_piece(PieceType::King, Color::White,
                            Position::square_from_name("a1"));
    pawnStateSwap.add_piece(PieceType::King, Color::Black,
                            Position::square_from_name("h10"));
    pawnStateSwap.add_piece(PieceType::Mage, Color::White,
                            Position::square_from_name("h2"));
    const int displacedPawn = pawnStateSwap.add_piece(
        PieceType::Pawn, Color::White, Position::square_from_name("d4"));
    Undo pawnStateUndo;
    expect(pawnStateSwap.make_move(require_move(pawnStateSwap, "h2~d4"), pawnStateUndo),
           "Mage can displace an unmoved Pawn");
    expect(!pawnStateSwap.piece(displacedPawn).moved,
           "Mage displacement preserves the target's first-move state");
    pawnStateSwap.set_side_to_move(Color::White);
    expect(pawnStateSwap.move_from_string("h2-h4").has_value(),
           "Mage-displaced unmoved Pawn retains its native double step");

    Position promotionSwap;
    promotionSwap.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    promotionSwap.add_piece(PieceType::King, Color::Black, Position::square_from_name("h10"));
    promotionSwap.add_piece(PieceType::Mage, Color::White, Position::square_from_name("d10"));
    promotionSwap.add_piece(PieceType::Pawn, Color::White, Position::square_from_name("d9"));
    Undo promotionUndo;
    expect(promotionSwap.make_move(require_move(promotionSwap, "d10~d9"), promotionUndo) &&
           promotionSwap.piece(promotionSwap.piece_on(Position::square_from_name("d10"))).type ==
             PieceType::Queen,
           "Mage relocation invokes the native promotion check on the swapped ally");

    Position giantSwap;
    giantSwap.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    giantSwap.add_piece(PieceType::King, Color::Black, Position::square_from_name("h10"));
    const int mage = giantSwap.add_piece(PieceType::Mage, Color::White,
                                         Position::square_from_name("c2"));
    const int giant = giantSwap.add_piece(PieceType::Giant, Color::White,
                                          Position::square_from_name("f6"));
    const int ally = giantSwap.add_piece(PieceType::Pawn, Color::White,
                                         Position::square_from_name("d3"));
    const int enemy = giantSwap.add_piece(PieceType::Pawn, Color::Black,
                                          Position::square_from_name("c3"));
    expect(giantSwap.move_from_string("c2~f6").has_value() &&
           giantSwap.move_from_string("c2~g7").has_value(),
           "mage can select any Giant footprint tile that yields an in-bounds swap");
    Undo giantSwapUndo;
    expect(giantSwap.make_move(require_move(giantSwap, "c2~f6"), giantSwapUndo),
           "mage/Giant swap applies");
    expect(giantSwap.piece(mage).square == Position::square_from_name("f6") &&
           giantSwap.piece(giant).square == Position::square_from_name("c2"),
           "selected Giant tile and translated Giant anchor exchange correctly");
    const Bitboard mageTile = Bitboard(1) << Position::square_from_name("f6");
    expect((giantSwap.occupied(Color::White) & mageTile) != 0 &&
           (giantSwap.pieces(Color::White, PieceType::Mage) & mageTile) != 0,
           "Mage/Giant swap retains the Mage in incremental occupancy bitboards");
    expect(!giantSwap.piece(ally).alive && !giantSwap.piece(enemy).alive,
           "a Mage-swapped Giant knocks out both teams in its destination footprint");

    // Coevolution regression: when the Mage starts beside the selected Giant
    // tile, the translated 2x2 footprint overlaps the Mage's destination.
    // Native MakeMoveTurnSkip sees the already-placed Mage and knocks it out.
    Position overlappingSwap;
    overlappingSwap.add_piece(PieceType::King, Color::White,
                               Position::square_from_name("a1"));
    overlappingSwap.add_piece(PieceType::King, Color::Black,
                               Position::square_from_name("h10"));
    const int overlappingMage = overlappingSwap.add_piece(
      PieceType::Mage, Color::White, Position::square_from_name("f1"));
    const int overlappingGiant = overlappingSwap.add_piece(
      PieceType::Giant, Color::White, Position::square_from_name("g1"));
    Undo overlappingUndo;
    expect(overlappingSwap.make_move(require_move(overlappingSwap, "f1~g1"), overlappingUndo) &&
           !overlappingSwap.piece(overlappingMage).alive &&
           overlappingSwap.piece(overlappingGiant).alive &&
           overlappingSwap.piece(overlappingGiant).square == Position::square_from_name("f1"),
           "Mage is knocked out when the translated Giant footprint overlaps its target tile");
    Position overlappingRoundTrip;
    std::string overlappingError;
    expect(overlappingRoundTrip.set_upn(overlappingSwap.upn(), &overlappingError),
           "overlapping Mage/Giant swap remains losslessly round-trippable: " +
             overlappingError);

    Position savedGiant;
    savedGiant.add_piece(PieceType::King, Color::White,
                         Position::square_from_name("h1"));
    savedGiant.add_piece(PieceType::King, Color::Black,
                         Position::square_from_name("h10"));
    savedGiant.add_piece(PieceType::Angel, Color::White,
                         Position::square_from_name("a2"));
    const int savedCollision = savedGiant.add_piece(
      PieceType::Checker, Color::White, Position::square_from_name("b2"));
    const int protectedGiant = savedGiant.add_piece(
      PieceType::Giant, Color::White, Position::square_from_name("e3"));
    savedGiant.add_piece(PieceType::Prince, Color::Black,
                         Position::square_from_name("d5"));
    Undo giantAttach;
    expect(savedGiant.make_move(require_move(savedGiant, "a2&e3"), giantAttach),
           "Angel can attach to a Giant before a collision save");
    Undo princeFirst;
    expect(savedGiant.make_move(require_move(savedGiant, "d5-e5"), princeFirst),
           "Prince starts the Giant-capture sequence");
    Undo princeCapture;
    expect(savedGiant.make_move(require_move(savedGiant, "e5-f4"), princeCapture),
           "Prince captures an Angel-protected Giant on its second move");
    expect(savedGiant.piece(protectedGiant).alive &&
           savedGiant.piece(protectedGiant).square == Position::square_from_name("a2") &&
           !savedGiant.piece(savedCollision).alive,
           "Angel-saved Giant uses native forced relocation and clears its full halo footprint");
    Position savedRoundTrip;
    std::string savedError;
    expect(savedRoundTrip.set_upn(savedGiant.upn(), &savedError),
           "Angel-saved Giant position remains losslessly round-trippable: " + savedError);

    Position edgeSwap;
    edgeSwap.add_piece(PieceType::King, Color::White, Position::square_from_name("h1"));
    edgeSwap.add_piece(PieceType::King, Color::Black, Position::square_from_name("h10"));
    edgeSwap.add_piece(PieceType::Mage, Color::White, Position::square_from_name("a1"));
    edgeSwap.add_piece(PieceType::Giant, Color::White, Position::square_from_name("f6"));
    expect(edgeSwap.move_from_string("a1~f6").has_value() &&
           !edgeSwap.move_from_string("a1~g7").has_value(),
           "mage excludes Giant-tile swaps whose translated footprint leaves the board");
}

void test_checker_king_long_diagonals() {
    const auto position = [](const char* extra, Color side = Color::White) {
        Position result;
        std::string error;
        expect(result.set_upn(std::string("w;king,w,a1;king,b,h10;rook,w,b1;pawn,w,h1;") +
                              extra, &error), "Checker King fixture parses: " + error);
        result.set_side_to_move(side);
        return result;
    };
    for (const Color side : {Color::White, Color::Black}) {
        Position quiet = position(side == Color::White ? "checkerKing,w,d5"
                                                       : "checkerKing,b,d5", side);
        for (const char* to : {"a2", "a8", "h9", "h1"}) {
            // h1 is occupied; every other diagonal reaches its edge.
            if (std::string(to) != "h1")
                expect(quiet.move_from_string(std::string("d5-") + to).has_value(),
                       "Checker King slides in both forward and backward directions");
        }
        expect(quiet.move_from_string("d5-g2").has_value() &&
                 !quiet.move_from_string("d5-h1").has_value() &&
                 !quiet.move_from_string("d5-d8").has_value(),
               "Checker King stops at occupancy and never moves orthogonally");
        const std::string before = quiet.upn();
        const auto key = quiet.key();
        Undo undo;
        expect(quiet.make_move(require_move(quiet, "d5-a8"), undo),
               "long quiet Checker King move applies");
        quiet.undo_move(undo);
        expect(quiet.upn() == before && quiet.key() == key,
               "long quiet move restores exact UPN and hash");
    }
    Position chain = position("checkerKing,w,b2;pawn,b,e5;pawn,b,d8");
    const std::string original = chain.upn();
    const Move jump = require_move(chain, "b2-f6");
    expect(jump.auxiliary == Position::square_from_name("e5") && chain.is_capture(jump),
           "long jump identifies the distant victim, not the midpoint");
    expect(!chain.move_from_string("b2-e5") && !chain.move_from_string("b2-g7"),
           "jump lands exactly one square beyond the victim");
    Undo first, second;
    expect(chain.make_move(jump, first) && chain.piece_on(Position::square_from_name("e5")) ==
             Position::NoPiece && chain.has_forced_action(),
           "long capture removes its victim and starts the distant continuation");
    expect(chain.legal_moves().size() == 1 &&
             chain.make_move(require_move(chain, "f6-c9"), second) &&
             chain.side_to_move() == Color::Black,
           "long backward continuation captures and ends the turn");
    chain.undo_move(second);
    chain.undo_move(first);
    expect(chain.upn() == original, "long capture chain is fully reversible");

    for (const char* blockers : {
           "checkerKing,w,b2;pawn,w,d4;pawn,b,e5",
           "checkerKing,w,b2;pawn,b,e5;pawn,b,f6",
           "checkerKing,w,b2;pawn,b,e5;pawn,w,f6"}) {
        Position blocked = position(blockers);
        expect(!blocked.move_from_string("b2-f6") && !blocked.move_from_string("b2-g7"),
               "friendly intervening pieces and either color on the landing block jumps");
    }
    Position threat;
    std::string error;
    expect(threat.set_upn("w;king,w,a1;king,b,e5;checkerKing,w,b2", &error) &&
             !threat.ordinary_predecessor_king_safe(),
           "a distant Checker King jump threatens the King");
    threat.add_piece(PieceType::Pawn, Color::Black, Position::square_from_name("f6"));
    expect(threat.ordinary_predecessor_king_safe(),
           "blocked jump landing removes the royal threat");

    Position promotion = position("checker,w,b8;pawn,b,c9;pawn,b,f8");
    const std::string beforePromotion = promotion.upn();
    Undo promote, finish;
    expect(promotion.make_move(require_move(promotion, "b8-d10"), promote) &&
             promotion.has_forced_action(), "promotion immediately enables a long backward jump");
    expect(promotion.make_move(require_move(promotion, "d10-g7"), finish),
           "newly promoted Checker King jumps the distant piece in the same turn");
    promotion.undo_move(finish);
    promotion.undo_move(promote);
    expect(promotion.upn() == beforePromotion, "promotion and long continuation undo exactly");

    Position hidden = position("checkerKing,w,b2;ghost,b,d4,0,0,0,0,0,0");
    const int checker = hidden.piece_on(Position::square_from_name("b2"));
    expect(hidden.move_from_string("b2-d4") && hidden.move_from_string("b2-f6") &&
             !hidden.is_capture(require_move(hidden, "b2-f6")),
           "hidden Ghost is an apparent quiet endpoint, transparent to the ray, not a jump");
    const std::string beforeHidden = hidden.upn();
    Undo collision;
    expect(hidden.make_move(require_move(hidden, "b2-d4"), collision) &&
             !hidden.piece(checker).alive, "long blind landing retains mutual knockout");
    hidden.undo_move(collision);
    expect(hidden.upn() == beforeHidden, "blind long-range collision undoes exactly");
    Position ordinary = position("checker,w,b2");
    expect(ordinary.move_from_string("b2-c3") && !ordinary.move_from_string("b2-d4") &&
             !ordinary.move_from_string("b2-c1"), "unpromoted Checker remains short-range");
}

void test_stale_checker_tablebase_codec() {
    const auto path = std::filesystem::temp_directory_path() /
      "ultimatefish-stale-checker-codec.uftb";
    std::array<char, 48> header{};
    std::memcpy(header.data(), "UFTB1\0\0\0", 8);
    const auto put = [&](int offset, std::uint32_t value) {
        std::memcpy(header.data() + offset, &value, sizeof(value));
    };
    put(8, 5); put(16, 4); put(24, 1); put(28, 1); put(32, 4);
    const auto check = [&](PieceType first, PieceType second) {
        put(12, static_cast<std::uint32_t>(first));
        put(40, static_cast<std::uint32_t>(second));
        { std::ofstream file(path, std::ios::binary); file.write(header.data(), header.size()); }
        return TablebaseProbe::uses_compatible_codec(path.string());
    };
    expect(check(PieceType::Rook, PieceType::Bishop),
           "unaffected legacy material codec remains available");
    expect(!check(PieceType::Checker, PieceType::Rook) &&
             !check(PieceType::Rook, PieceType::Checker) &&
             !check(PieceType::CheckerKing, PieceType::Rook),
           "legacy Checker codecs are rejected in either material slot, including promotions");
    for (const PieceType stale : {PieceType::Devil, PieceType::Sniper})
        expect(!check(stale, PieceType::Rook) && !check(PieceType::Rook, stale),
               "pre-turn-start check codecs are rejected in either material slot");
    std::filesystem::remove(path);
}

void test_checker_chain_and_prince_turns() {
    Position checker;
    checker.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    checker.add_piece(PieceType::King, Color::Black, Position::square_from_name("h8"));
    checker.add_piece(PieceType::Checker, Color::White, Position::square_from_name("b2"));
    checker.add_piece(PieceType::Pawn, Color::Black, Position::square_from_name("c3"));
    checker.add_piece(PieceType::Pawn, Color::Black, Position::square_from_name("e5"));
    expect(!checker.has_forced_action(),
           "an available checker jump does not suppress other characters");
    Undo first;
    expect(checker.make_move(require_move(checker, "b2-d4"), first), "first checker jump applies");
    expect(checker.has_forced_action(), "checker continuation remains compulsory");
    expect(checker.side_to_move() == Color::White, "checker chain retains the turn");
    expect(checker.continuation() == Continuation::CheckerJump, "checker chain is forced");
    expect(checker.legal_moves().size() == 1, "only the continued checker jump is legal");
    Undo second;
    expect(checker.make_move(require_move(checker, "d4-f6"), second), "second checker jump applies");
    expect(checker.side_to_move() == Color::Black, "checker chain ends the turn");

    Position hiddenCheckerCollision;
    hiddenCheckerCollision.add_piece(PieceType::King, Color::White,
                                     Position::square_from_name("a1"));
    hiddenCheckerCollision.add_piece(PieceType::King, Color::Black,
                                     Position::square_from_name("h10"));
    const int blindChecker = hiddenCheckerCollision.add_piece(
      PieceType::Checker, Color::White, Position::square_from_name("b2"));
    const int adjacentHiddenGhost = hiddenCheckerCollision.add_piece(
      PieceType::Ghost, Color::Black, Position::square_from_name("c3"));
    hiddenCheckerCollision.piece(adjacentHiddenGhost).visible = false;
    expect(hiddenCheckerCollision.move_from_string("b2-c3").has_value() &&
             !hiddenCheckerCollision.move_from_string("b2-d4").has_value(),
           "Checker collides with an adjacent hidden Ghost instead of jumping it");
    Undo checkerGhostCollision;
    expect(hiddenCheckerCollision.make_move(
             require_move(hiddenCheckerCollision, "b2-c3"),
             checkerGhostCollision) &&
             !hiddenCheckerCollision.piece(blindChecker).alive &&
             !hiddenCheckerCollision.piece(adjacentHiddenGhost).alive,
           "adjacent Checker/Ghost blind collision knocks out both characters");

    Position hiddenCheckerKingCollision;
    hiddenCheckerKingCollision.add_piece(PieceType::King, Color::White,
                                         Position::square_from_name("a1"));
    hiddenCheckerKingCollision.add_piece(PieceType::King, Color::Black,
                                         Position::square_from_name("h10"));
    const int blindCheckerKing = hiddenCheckerKingCollision.add_piece(
      PieceType::CheckerKing, Color::White, Position::square_from_name("b4"));
    const int backwardHiddenGhost = hiddenCheckerKingCollision.add_piece(
      PieceType::Ghost, Color::Black, Position::square_from_name("c3"));
    hiddenCheckerKingCollision.piece(backwardHiddenGhost).visible = false;
    Undo checkerKingGhostCollision;
    expect(hiddenCheckerKingCollision.make_move(
             require_move(hiddenCheckerKingCollision, "b4-c3"),
             checkerKingGhostCollision) &&
             !hiddenCheckerKingCollision.piece(blindCheckerKing).alive &&
             !hiddenCheckerKingCollision.piece(backwardHiddenGhost).alive,
           "promoted Checker uses the same backward blind-Ghost collision callback");

    Position hiddenCheckerLanding;
    hiddenCheckerLanding.add_piece(PieceType::King, Color::White,
                                   Position::square_from_name("a1"));
    hiddenCheckerLanding.add_piece(PieceType::King, Color::Black,
                                   Position::square_from_name("h10"));
    const int landingChecker = hiddenCheckerLanding.add_piece(
      PieceType::Checker, Color::White, Position::square_from_name("b2"));
    const int jumpedPawn = hiddenCheckerLanding.add_piece(
      PieceType::Pawn, Color::Black, Position::square_from_name("c3"));
    const int landingGhost = hiddenCheckerLanding.add_piece(
      PieceType::Ghost, Color::Black, Position::square_from_name("d4"));
    hiddenCheckerLanding.piece(landingGhost).visible = false;
    Undo checkerLandsOnGhost;
    expect(hiddenCheckerLanding.make_move(
             require_move(hiddenCheckerLanding, "b2-d4"),
             checkerLandsOnGhost),
           "Checker jump may land blindly on an invisible enemy Ghost");
    expect(!hiddenCheckerLanding.piece(landingChecker).alive &&
             !hiddenCheckerLanding.piece(jumpedPawn).alive &&
             !hiddenCheckerLanding.piece(landingGhost).alive,
           "Checker captures the jumped piece before the hidden landing collision kills both");

    Position optionalChecker;
    std::string checkerError;
    expect(optionalChecker.set_upn(
      "b;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;king,w,a1;queen,w,c8;"
      "king,b,b10;bishop,b,b9;checker,b,d9",
      &checkerError),
      "live optional-checker regression parses: " + checkerError);
    expect(optionalChecker.move_from_string("d9-b7").has_value(),
           "checker jump remains available to that checker");
    expect(optionalChecker.move_from_string("b9-c8").has_value(),
           "checker jump does not suppress a native Bishop recapture");

    Position quietChecker;
    quietChecker.add_piece(PieceType::King, Color::White,
                           Position::square_from_name("a1"));
    quietChecker.add_piece(PieceType::King, Color::Black,
                           Position::square_from_name("h10"));
    quietChecker.add_piece(PieceType::Rook, Color::Black,
                           Position::square_from_name("g10"));
    quietChecker.add_piece(PieceType::Checker, Color::Black,
                           Position::square_from_name("b8"));
    quietChecker.set_side_to_move(Color::Black);
    Undo quietCheckerMove;
    expect(quietChecker.make_move(require_move(quietChecker, "b8-c7"), quietCheckerMove),
           "live quiet Checker move applies");
    expect(quietChecker.has_real_king(Color::White) && !quietChecker.game_over(),
           "quiet Checker auxiliary sentinel never captures the King on a1");

    Position prince;
    prince.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    prince.add_piece(PieceType::King, Color::Black, Position::square_from_name("h8"));
    prince.add_piece(PieceType::Prince, Color::White, Position::square_from_name("d4"));
    prince.add_piece(PieceType::Pawn, Color::Black, Position::square_from_name("d6"));
    Undo move;
    expect(prince.make_move(require_move(prince, "d4-d5"), move), "prince first move applies");
    expect(prince.side_to_move() == Color::White, "non-capturing prince first move retains turn");
    expect(prince.continuation() == Continuation::PrinceSecondMove, "prince second move is forced");
    expect(require_move(prince, "d5-d6").from == Position::square_from_name("d5"),
           "prince second move may attack");
    expect(require_move(prince, "d5-c5").from == Position::square_from_name("d5"),
           "native prince continuation may also be quiet");
    Undo quietSecond;
    expect(prince.make_move(require_move(prince, "d5-c5"), quietSecond),
           "quiet prince second move applies");
    expect(prince.side_to_move() == Color::Black,
           "prince turn ends after its queued second move");
}

void test_sludge_and_victory() {
    Position position;
    position.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    position.add_piece(PieceType::King, Color::Black, Position::square_from_name("h8"));
    position.add_piece(PieceType::Sludge, Color::White, Position::square_from_name("c2"));
    position.add_piece(PieceType::Pawn, Color::Black, Position::square_from_name("b7"));
    Undo undo;
    expect(position.make_move(require_move(position, "c2-c4"), undo), "sludge moves two orthogonally");
    const int goop = position.piece_on(Position::square_from_name("c2"));
    expect(goop != Position::NoPiece && position.piece(goop).type == PieceType::Goop,
           "sludge leaves goop on its previous square");
    expect(position.piece(position.piece_on(Position::square_from_name("c3"))).type == PieceType::Goop,
           "two-square sludge move also leaves goop on the intervening square");

    Position trailCollision;
    trailCollision.add_piece(PieceType::King, Color::White,
                             Position::square_from_name("a1"));
    trailCollision.add_piece(PieceType::King, Color::Black,
                             Position::square_from_name("h10"));
    const int passingSludge = trailCollision.add_piece(
        PieceType::Sludge, Color::White, Position::square_from_name("c2"));
    const int trailGhost = trailCollision.add_piece(
        PieceType::Ghost, Color::Black, Position::square_from_name("c3"));
    trailCollision.piece(trailGhost).visible = false;
    Undo trailUndo;
    expect(trailCollision.make_move(
               require_move(trailCollision, "c2-c4"), trailUndo),
           "Sludge can move through an unseen Ghost on its intermediate cell");
    expect(trailCollision.piece(passingSludge).alive &&
               trailCollision.piece(passingSludge).square ==
                   Position::square_from_name("c4") &&
               !trailCollision.piece(trailGhost).alive,
           "intermediate trail Goop knocks out the hidden Ghost while Sludge continues");
    expect(trailCollision.piece_on(Position::square_from_name("c3")) ==
               Position::NoPiece &&
               trailCollision.piece_on(Position::square_from_name("c2")) !=
                   Position::NoPiece &&
               trailCollision.piece(
                   trailCollision.piece_on(Position::square_from_name("c2"))).type ==
                   PieceType::Goop,
           "exploding intermediate Goop disappears while origin Goop remains");

    Position blindCollision;
    blindCollision.add_piece(PieceType::King, Color::White,
                             Position::square_from_name("a1"));
    blindCollision.add_piece(PieceType::King, Color::Black,
                             Position::square_from_name("h10"));
    blindCollision.add_piece(
        PieceType::Sludge, Color::White, Position::square_from_name("c4"));
    const int hiddenGhost = blindCollision.add_piece(
        PieceType::Ghost, Color::Black, Position::square_from_name("c5"));
    blindCollision.piece(hiddenGhost).visible = false;
    Undo blindUndo;
    expect(blindCollision.make_move(
               require_move(blindCollision, "c4-c5"), blindUndo),
           "Sludge can blindly enter an unseen enemy Ghost cell");
    expect(blindCollision.pieces(Color::White, PieceType::Sludge) == 0 &&
               blindCollision.pieces(Color::Black, PieceType::Ghost) == 0,
           "blind Sludge and hidden Ghost collision knocks out both pieces");
    const int blindGoop = blindCollision.piece_on(
        Position::square_from_name("c4"));
    expect(blindGoop != Position::NoPiece &&
               blindCollision.piece(blindGoop).type == PieceType::Goop,
           "a Sludge killed by a hidden Ghost still leaves Goop behind");

    Position knockout;
    knockout.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    knockout.add_piece(PieceType::King, Color::Black, Position::square_from_name("b2"));
    knockout.add_piece(PieceType::Pawn, Color::White, Position::square_from_name("h2"));
    Undo capture;
    expect(knockout.make_move(require_move(knockout, "a1-b2"), capture), "real king can be knocked out");
    expect(knockout.game_over(), "real king knockout ends the game");
    expect(knockout.winner() == Color::White, "surviving real king wins");
}

void test_giant_rechecks_angel_rescue_footprint() {
    Position setup;
    setup.add_piece(PieceType::King, Color::White,
                    Position::square_from_name("a1"));
    setup.add_piece(PieceType::Angel, Color::White,
                    Position::square_from_name("b1"));
    setup.add_piece(PieceType::King, Color::Black,
                    Position::square_from_name("h10"));
    setup.add_piece(PieceType::Giant, Color::Black,
                    Position::square_from_name("c5"));
    Undo link;
    expect(setup.make_move(require_move(setup, "b1&a1"), link),
           "Angel can protect a King inside the Giant's next footprint");

    // Relocating the opposing Giant beside the already linked King constructs
    // the exact state discovered by draft self-play without first asking the
    // checked side to make an illegal setup move.
    std::string collisionUpn = setup.upn();
    const std::string oldGiant = "giant,b,c5";
    const auto giant = collisionUpn.find(oldGiant);
    expect(giant != std::string::npos, "collision fixture locates its Giant");
    if (giant != std::string::npos)
        collisionUpn.replace(giant, oldGiant.size(), "giant,b,c1");
    Position position;
    std::string error;
    expect(position.set_upn(collisionUpn, &error),
           "Angel/Giant collision fixture parses: " + error);

    Undo collision;
    expect(position.make_move(require_move(position, "c1-a1"), collision),
           "Giant can enter a footprint containing an Angel-protected enemy");
    expect(!position.has_real_king(Color::White) &&
               position.winner() == Color::Black,
           "Giant strikes a King again when its Angel rescues it onto another footprint cell");

    Position roundTrip;
    expect(roundTrip.set_upn(position.upn(), &error),
           "Angel/Giant collision remains a valid round-trippable state: " + error);

    Position rescuedGiant;
    rescuedGiant.add_piece(PieceType::King, Color::White,
                            Position::square_from_name("a1"));
    rescuedGiant.add_piece(PieceType::Angel, Color::White,
                            Position::square_from_name("c3"));
    rescuedGiant.add_piece(PieceType::Giant, Color::White,
                            Position::square_from_name("d3"));
    rescuedGiant.add_piece(PieceType::King, Color::Black,
                            Position::square_from_name("a10"));
    rescuedGiant.add_piece(PieceType::Rook, Color::Black,
                            Position::square_from_name("d8"));
    Undo protectGiant;
    expect(rescuedGiant.make_move(
               require_move(rescuedGiant, "c3&d3"), protectGiant),
           "Angel can protect a Giant from outside its current footprint");
    Undo rookAttack;
    expect(rescuedGiant.make_move(
               require_move(rescuedGiant, "d8-d4"), rookAttack),
           "Rook can attack an Angel-protected Giant footprint cell");
    expect(rescuedGiant.pieces(Color::White, PieceType::Giant) != 0 &&
               rescuedGiant.pieces(Color::Black, PieceType::Rook) == 0,
           "rescued Giant's relocated footprint knocks out the already-landed attacker");
    expect(roundTrip.set_upn(rescuedGiant.upn(), &error),
           "rescued Giant/attacker collision round trips: " + error);
}

void test_parasite_goop_and_angel_interactions() {
    Position parasiteAttack;
    parasiteAttack.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    parasiteAttack.add_piece(PieceType::King, Color::Black, Position::square_from_name("h8"));
    const int parasite = parasiteAttack.add_piece(PieceType::Parasite, Color::White,
                                                   Position::square_from_name("d4"));
    const int rook = parasiteAttack.add_piece(PieceType::Rook, Color::Black,
                                               Position::square_from_name("e5"));
    Undo attack;
    expect(parasiteAttack.make_move(require_move(parasiteAttack, "d4-e5"), attack),
           "parasite attack applies");
    expect(!parasiteAttack.piece(parasite).alive && parasiteAttack.piece(rook).alive &&
           parasiteAttack.piece(rook).color == Color::White,
           "attacking parasite possesses instead of killing its target");

    Position jesterPossession;
    jesterPossession.add_piece(PieceType::King, Color::White,
                               Position::square_from_name("a1"));
    const int jesterParasite = jesterPossession.add_piece(
        PieceType::Parasite, Color::White, Position::square_from_name("c3"));
    jesterPossession.add_piece(PieceType::King, Color::Black,
                               Position::square_from_name("h10"));
    const int possessedJester = jesterPossession.add_piece(
        PieceType::Jester, Color::Black, Position::square_from_name("d4"));
    const std::string beforeJesterPossession = jesterPossession.upn();
    Undo possessJester;
    expect(jesterPossession.make_move(
               require_move(jesterPossession, "c3-d4"), possessJester),
           "Parasite possession of an opposing Jester applies");
    expect(!jesterPossession.piece(jesterParasite).alive &&
               jesterPossession.piece(possessedJester).alive &&
               jesterPossession.piece(possessedJester).type == PieceType::Jester &&
               jesterPossession.piece(possessedJester).color == Color::White,
           "Jester possession changes control without replacing its fixed "
           "piece identity");
    jesterPossession.undo_move(possessJester);
    expect(jesterPossession.upn() == beforeJesterPossession,
           "undo restores the exact pre-possession Jester identity and team");

    Position rangedParasiteDefense;
    rangedParasiteDefense.add_piece(PieceType::King, Color::White,
                                     Position::square_from_name("a1"));
    const int rangedParasiteSniper = rangedParasiteDefense.add_piece(
        PieceType::Sniper, Color::White, Position::square_from_name("f3"));
    const int rangedDefendingParasite = rangedParasiteDefense.add_piece(
        PieceType::Parasite, Color::Black, Position::square_from_name("f6"));
    rangedParasiteDefense.add_piece(PieceType::King, Color::Black,
                                     Position::square_from_name("h10"));
    Undo shootParasite;
    expect(rangedParasiteDefense.make_move(
               require_move(rangedParasiteDefense, "f3xf6"), shootParasite),
           "ranged attack on a defending Parasite applies");
    expect(rangedParasiteDefense.piece(rangedParasiteSniper).alive &&
               rangedParasiteDefense.piece(rangedParasiteSniper).color ==
                   Color::White &&
               !rangedParasiteDefense.piece(rangedDefendingParasite).alive,
           "ranged attacker kills a Parasite without being possessed");

    Position specialTarget;
    specialTarget.add_piece(PieceType::King, Color::White,
                            Position::square_from_name("a1"));
    const int attackingParasite = specialTarget.add_piece(
        PieceType::Parasite, Color::White, Position::square_from_name("d4"));
    const int possessedBomb = specialTarget.add_piece(
        PieceType::Bomb, Color::Black, Position::square_from_name("e5"));
    specialTarget.add_piece(PieceType::King, Color::Black,
                            Position::square_from_name("h10"));
    specialTarget.add_piece(PieceType::Pawn, Color::Black,
                            Position::square_from_name("f5"));
    Undo possessBomb;
    expect(specialTarget.make_move(
               require_move(specialTarget, "d4-e5"), possessBomb),
           "Parasite attack on a Bomb applies");
    expect(!specialTarget.piece(attackingParasite).alive &&
               specialTarget.piece(possessedBomb).alive &&
               specialTarget.piece(possessedBomb).color == Color::White &&
               specialTarget.piece_on(Position::square_from_name("f5")) !=
                   Position::NoPiece,
           "Parasite possesses a Bomb without invoking its explosion");

    Position possessedGoop;
    possessedGoop.add_piece(PieceType::King, Color::White,
                            Position::square_from_name("a1"));
    const int goopParasite = possessedGoop.add_piece(
        PieceType::Parasite, Color::White, Position::square_from_name("d4"));
    const int goopTarget = possessedGoop.add_piece(
        PieceType::Goop, Color::Black, Position::square_from_name("e5"));
    possessedGoop.add_piece(PieceType::King, Color::Black,
                            Position::square_from_name("h10"));
    Undo possessGoop;
    expect(possessedGoop.make_move(
               require_move(possessedGoop, "d4-e5"), possessGoop),
           "Parasite attack on Goop applies");
    expect(!possessedGoop.piece(goopParasite).alive &&
               possessedGoop.piece(goopTarget).alive &&
               possessedGoop.piece(goopTarget).color == Color::White,
           "Parasite possesses Goop without invoking melee retaliation");

    Position attachedAttacker;
    attachedAttacker.add_piece(PieceType::King, Color::White,
                               Position::square_from_name("a1"));
    const int attackerAngel = attachedAttacker.add_piece(
        PieceType::Angel, Color::White, Position::square_from_name("b2"));
    const int protectedParasite = attachedAttacker.add_piece(
        PieceType::Parasite, Color::White, Position::square_from_name("d4"));
    const int takeoverTarget = attachedAttacker.add_piece(
        PieceType::Rook, Color::Black, Position::square_from_name("e5"));
    attachedAttacker.add_piece(PieceType::King, Color::Black,
                               Position::square_from_name("h10"));
    attachedAttacker.add_piece(PieceType::Pawn, Color::Black,
                               Position::square_from_name("h9"));
    Undo attachToParasite;
    expect(attachedAttacker.make_move(
               require_move(attachedAttacker, "b2&d4"), attachToParasite),
           "Angel can attach to an attacking Parasite");
    Undo waitForParasite;
    expect(attachedAttacker.make_move(
               require_move(attachedAttacker, "h9-h8"), waitForParasite),
           "opponent wait returns the turn to the attached Parasite");
    Undo attachedPossession;
    expect(attachedAttacker.make_move(
               require_move(attachedAttacker, "d4-e5"), attachedPossession),
           "Angel-attached Parasite possession applies");
    expect(!attachedAttacker.piece(protectedParasite).alive &&
               attachedAttacker.piece(attackerAngel).alive &&
               attachedAttacker.piece(attackerAngel).host == takeoverTarget &&
               attachedAttacker.piece(takeoverTarget).color == Color::White,
           "attacking Parasite bypasses rescue and transfers its Angel to the possessed host");

    Position attachedVictim;
    attachedVictim.add_piece(PieceType::King, Color::White,
                             Position::square_from_name("a1"));
    const int victimAngel = attachedVictim.add_piece(
        PieceType::Angel, Color::White, Position::square_from_name("b2"));
    const int defendedParasite = attachedVictim.add_piece(
        PieceType::Parasite, Color::White, Position::square_from_name("d4"));
    const int possessedKnight = attachedVictim.add_piece(
        PieceType::Knight, Color::Black, Position::square_from_name("c6"));
    attachedVictim.add_piece(PieceType::King, Color::Black,
                             Position::square_from_name("h10"));
    Undo attachToVictim;
    expect(attachedVictim.make_move(
               require_move(attachedVictim, "b2&d4"), attachToVictim),
           "Angel can attach to a defending Parasite");
    Undo attackAttachedParasite;
    expect(attachedVictim.make_move(
               require_move(attachedVictim, "c6-d4"), attackAttachedParasite),
           "melee attack on an Angel-attached Parasite applies");
    expect(!attachedVictim.piece(defendedParasite).alive &&
               attachedVictim.piece(possessedKnight).alive &&
               attachedVictim.piece(possessedKnight).color == Color::White &&
               attachedVictim.piece(victimAngel).alive &&
               attachedVictim.piece(victimAngel).host == possessedKnight,
           "defending Parasite bypasses rescue and transfers its Angel to the possessed attacker");

    Position copycatPossession;
    copycatPossession.add_piece(PieceType::King, Color::White,
                                Position::square_from_name("a1"));
    copycatPossession.add_piece(PieceType::King, Color::Black,
                                Position::square_from_name("h10"));
    const int copycat = copycatPossession.add_piece(
        PieceType::Copycat, Color::White, Position::square_from_name("e4"));
    const int copycatClone = copycatPossession.piece(copycat).link;
    const int blackParasite = copycatPossession.add_piece(
        PieceType::Parasite, Color::Black, Position::square_from_name("d5"));
    copycatPossession.set_side_to_move(Color::Black);
    Undo possessClone;
    expect(copycatPossession.make_move(
               require_move(copycatPossession, "d5-d4"), possessClone),
           "parasite possession of CopyCat clone applies");
    expect(!copycatPossession.piece(blackParasite).alive &&
               copycatPossession.piece(copycat).color == Color::Black &&
               copycatPossession.piece(copycatClone).color == Color::Black,
           "possessing either CopyCat half transfers the linked pair");
    expect(copycatPossession.material_points(Color::White) == 0 &&
               copycatPossession.material_points(Color::Black) == 5,
           "CopyCat possession transfers all five native material points");

    Position retaliation;
    retaliation.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    retaliation.add_piece(PieceType::King, Color::Black, Position::square_from_name("h8"));
    const int knight = retaliation.add_piece(PieceType::Knight, Color::White,
                                              Position::square_from_name("c3"));
    const int goop = retaliation.add_piece(PieceType::Goop, Color::Black,
                                            Position::square_from_name("d5"));
    retaliation.add_piece(PieceType::Pawn, Color::Black, Position::square_from_name("g7"));
    Undo hit;
    expect(retaliation.make_move(require_move(retaliation, "c3-d5"), hit), "melee attack on goop applies");
    expect(!retaliation.piece(knight).alive && !retaliation.piece(goop).alive,
           "goop and its melee attacker both die");

    Position rangedGoop;
    rangedGoop.add_piece(PieceType::King, Color::White,
                          Position::square_from_name("a1"));
    const int goopSniper = rangedGoop.add_piece(
        PieceType::Sniper, Color::White, Position::square_from_name("f3"));
    const int rangedGoopTarget = rangedGoop.add_piece(
        PieceType::Goop, Color::Black, Position::square_from_name("f6"));
    rangedGoop.add_piece(PieceType::King, Color::Black,
                          Position::square_from_name("h10"));
    Undo shootGoop;
    expect(rangedGoop.make_move(require_move(rangedGoop, "f3xf6"), shootGoop),
           "ranged attack on Goop applies");
    expect(rangedGoop.piece(goopSniper).alive &&
               !rangedGoop.piece(rangedGoopTarget).alive,
           "ranged attacker survives Goop retaliation");


    Position rescuedAttacker;
    rescuedAttacker.add_piece(PieceType::King, Color::White,
                              Position::square_from_name("h1"));
    rescuedAttacker.add_piece(PieceType::King, Color::Black,
                              Position::square_from_name("h10"));
    const int rescueAngel = rescuedAttacker.add_piece(
        PieceType::Angel, Color::White, Position::square_from_name("a2"));
    const int rescuedKnight = rescuedAttacker.add_piece(
        PieceType::Knight, Color::White, Position::square_from_name("b2"));
    const int rescueGoop = rescuedAttacker.add_piece(
        PieceType::Goop, Color::Black, Position::square_from_name("c4"));
    rescuedAttacker.add_piece(PieceType::Pawn, Color::Black,
                              Position::square_from_name("h9"));
    Undo rescueLink;
    expect(rescuedAttacker.make_move(
               require_move(rescuedAttacker, "a2&b2"), rescueLink),
           "angel can attach to a future melee attacker");
    Undo rescueWaitingMove;
    expect(rescuedAttacker.make_move(
               require_move(rescuedAttacker, "h9-h8"), rescueWaitingMove),
           "opponent waiting move returns the turn to the protected attacker");
    Undo rescuedAttack;
    expect(rescuedAttacker.make_move(
               require_move(rescuedAttacker, "b2-c4"), rescuedAttack),
           "angel-protected melee attack on Goop applies");
    expect(rescuedAttacker.piece(rescuedKnight).alive &&
               rescuedAttacker.piece(rescuedKnight).square ==
                   Position::square_from_name("a2") &&
               !rescuedAttacker.piece(rescueAngel).alive &&
               !rescuedAttacker.piece(rescueGoop).alive,
           "Goop kills the attacker, then Angel rescue leaves it on the halo");
    expect(rescuedAttacker.piece_on(Position::square_from_name("c4")) ==
               Position::NoPiece,
           "generic move placement does not overwrite an Angel rescue");

    Position angel;
    angel.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    angel.add_piece(PieceType::King, Color::Black, Position::square_from_name("h10"));
    const int angelId = angel.add_piece(PieceType::Angel, Color::White,
                                        Position::square_from_name("b2"));
    const int host = angel.add_piece(PieceType::Rook, Color::White,
                                     Position::square_from_name("c3"));
    const int attacker = angel.add_piece(PieceType::Rook, Color::Black,
                                         Position::square_from_name("c8"));
    Undo link;
    expect(angel.make_move(require_move(angel, "b2&c3"), link), "angel link applies");
    expect(angel.piece_on(Position::square_from_name("b2")) != Position::NoPiece &&
           angel.piece(angel.piece_on(Position::square_from_name("b2"))).type == PieceType::Halo,
           "angel leaves its halo at the origin");
    expect(!angel.piece(angelId).onBoard && angel.piece(angelId).host == host,
           "angel becomes attached state without occupying its host square");
    Position linkedRoundTrip;
    std::string linkError;
    expect(linkedRoundTrip.set_upn(angel.upn(), &linkError),
           "UPN accepts an off-board angel sharing its host coordinate: " + linkError);
    expect(linkedRoundTrip.upn() == angel.upn(),
           "UPN round trip preserves attached angel/host/halo state");
    Undo saved;
    expect(angel.make_move(require_move(angel, "c8-c3"), saved),
           "lethal host hit invokes the attached angel");
    expect(angel.piece(host).alive && angel.piece(host).square == Position::square_from_name("b2") &&
           !angel.piece(angelId).alive && angel.piece(attacker).square == Position::square_from_name("c3"),
           "angel is consumed and returns its surviving host to the halo");

    Position onyxGiantRescue;
    onyxGiantRescue.add_piece(PieceType::King, Color::White,
                              Position::square_from_name("a1"));
    onyxGiantRescue.add_piece(PieceType::King, Color::Black,
                              Position::square_from_name("a10"));
    onyxGiantRescue.add_piece(PieceType::Sniper, Color::White,
                              Position::square_from_name("f3"));
    const int onyxAngel = onyxGiantRescue.add_piece(
      PieceType::Angel, Color::Black, Position::square_from_name("d8"));
    const int onyxGiant = onyxGiantRescue.add_piece(
      PieceType::Giant, Color::Black, Position::square_from_name("e9"));
    onyxGiantRescue.set_side_to_move(Color::Black);
    Undo onyxLink;
    expect(onyxGiantRescue.make_move(
             require_move(onyxGiantRescue, "d8&e9"), onyxLink),
           "Onyx Angel can attach to a Giant");
    expect(onyxGiantRescue.move_from_string("f3xf9").has_value() &&
             !onyxGiantRescue.move_from_string("f3xe9").has_value(),
           "Sniper targets the Giant footprint cell on its file, not the off-axis anchor");
    Undo onyxShot;
    expect(onyxGiantRescue.make_move(
             require_move(onyxGiantRescue, "f3xf9"), onyxShot),
           "Sniper can trigger an Onyx Giant's Angel rescue");
    expect(onyxGiantRescue.piece(onyxGiant).alive &&
             onyxGiantRescue.piece(onyxGiant).square ==
               Position::square_from_name("c7") &&
             !onyxGiantRescue.piece(onyxAngel).alive,
           "Onyx Giant rescue converts the Halo's upper-right corner to the canonical anchor");

    Position nestedAngels;
    nestedAngels.add_piece(PieceType::King, Color::White,
                           Position::square_from_name("a1"));
    nestedAngels.add_piece(PieceType::King, Color::Black,
                           Position::square_from_name("h10"));
    const int firstAngel = nestedAngels.add_piece(
      PieceType::Angel, Color::White, Position::square_from_name("b2"));
    const int dependentAngel = nestedAngels.add_piece(
      PieceType::Angel, Color::White, Position::square_from_name("c2"));
    const int layeredHost = nestedAngels.add_piece(
      PieceType::Rook, Color::White, Position::square_from_name("d3"));
    nestedAngels.add_piece(PieceType::Rook, Color::Black,
                           Position::square_from_name("d8"));
    nestedAngels.add_piece(PieceType::Rook, Color::Black,
                           Position::square_from_name("b8"));
    Undo nestAngel;
    expect(nestedAngels.make_move(
             require_move(nestedAngels, "c2&b2"), nestAngel),
           "an Angel can protect another unlinked Angel");
    Undo blackWait;
    expect(nestedAngels.make_move(
             require_move(nestedAngels, "h10-h9"), blackWait),
           "opponent wait preserves the nested Angel graph");
    Undo reparentAngels;
    expect(nestedAngels.make_move(
             require_move(nestedAngels, "b2&d3"), reparentAngels),
           "a protected Angel can attach to a final host");
    expect(nestedAngels.piece(firstAngel).host == layeredHost &&
             nestedAngels.piece(dependentAngel).host == layeredHost,
           "attaching an Angel reparents its dependent Angels to the host");
    Undo firstLayer;
    expect(nestedAngels.make_move(
             require_move(nestedAngels, "d8-d3"), firstLayer),
           "the first attack consumes the front Angel layer");
    expect(nestedAngels.piece(layeredHost).alive &&
             nestedAngels.piece(layeredHost).square ==
               Position::square_from_name("b2") &&
             !nestedAngels.piece(firstAngel).alive &&
             nestedAngels.piece(dependentAngel).alive,
           "nested Angel order returns the host to the front Halo first");
    Undo whiteWait;
    expect(nestedAngels.make_move(
             require_move(nestedAngels, "a1-a2"), whiteWait),
           "waiting move exposes the remaining Angel layer");
    Undo secondLayer;
    expect(nestedAngels.make_move(
             require_move(nestedAngels, "b8-b2"), secondLayer),
           "the second attack consumes the dependent Angel layer");
    expect(nestedAngels.piece(layeredHost).alive &&
             nestedAngels.piece(layeredHost).square ==
               Position::square_from_name("c2") &&
             !nestedAngels.piece(dependentAngel).alive,
           "the second nested Angel returns the host to its own Halo");
}

void test_native_giant_and_copycat_footprints() {
    Position giant;
    giant.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    giant.add_piece(PieceType::King, Color::Black, Position::square_from_name("h8"));
    const int giantId = giant.add_piece(PieceType::Giant, Color::White,
                                        Position::square_from_name("c3"));
    const int victimA = giant.add_piece(PieceType::Pawn, Color::Black,
                                        Position::square_from_name("c5"));
    const int victimB = giant.add_piece(PieceType::Rook, Color::Black,
                                        Position::square_from_name("d6"));
    expect(!giant.move_from_string("c3-c4").has_value(),
           "native giant does not move one square");
    expect(giant.move_from_string("c3-c5").has_value(),
           "native giant moves exactly one 2x2 footprint orthogonally");
    Undo giantMove;
    expect(giant.make_move(require_move(giant, "c3-c5"), giantMove), "giant move applies");
    expect(!giant.piece(victimA).alive && !giant.piece(victimB).alive,
           "giant captures every enemy in its destination footprint");
    expect(giant.piece_on(Position::square_from_name("c5")) == giantId &&
           giant.piece_on(Position::square_from_name("d6")) == giantId,
           "giant occupies all four destination squares");

    Position hiddenGiantTarget;
    hiddenGiantTarget.add_piece(PieceType::King, Color::White,
                                Position::square_from_name("a1"));
    const int ghostCrusher = hiddenGiantTarget.add_piece(
        PieceType::Giant, Color::White, Position::square_from_name("c3"));
    const int crushedGhost = hiddenGiantTarget.add_piece(
        PieceType::Ghost, Color::Black, Position::square_from_name("d5"));
    hiddenGiantTarget.piece(crushedGhost).visible = false;
    hiddenGiantTarget.add_piece(PieceType::King, Color::Black,
                                Position::square_from_name("h10"));
    Undo crushHiddenGhost;
    expect(hiddenGiantTarget.make_move(
               require_move(hiddenGiantTarget, "c3-c5"), crushHiddenGhost),
           "Giant can translate onto a hidden enemy Ghost footprint");
    expect(hiddenGiantTarget.piece(ghostCrusher).alive &&
               !hiddenGiantTarget.piece(crushedGhost).alive &&
               hiddenGiantTarget.piece_on(Position::square_from_name("d5")) ==
                   ghostCrusher,
           "Giant captures a hidden enemy Ghost as ordinary footprint occupancy");

    Position hiddenGiantAlly;
    hiddenGiantAlly.add_piece(PieceType::King, Color::White,
                              Position::square_from_name("a1"));
    hiddenGiantAlly.add_piece(PieceType::Giant, Color::White,
                              Position::square_from_name("c3"));
    const int alliedGiantGhost = hiddenGiantAlly.add_piece(
        PieceType::Ghost, Color::White, Position::square_from_name("d5"));
    hiddenGiantAlly.piece(alliedGiantGhost).visible = false;
    hiddenGiantAlly.add_piece(PieceType::King, Color::Black,
                              Position::square_from_name("h10"));
    expect(!hiddenGiantAlly.move_from_string("c3-c5").has_value(),
           "an allied hidden Ghost blocks an ordinary Giant translation");

    Position copycat;
    copycat.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    copycat.add_piece(PieceType::King, Color::Black, Position::square_from_name("h8"));
    const int original = copycat.add_piece(PieceType::Copycat, Color::White,
                                           Position::square_from_name("c3"));
    const int clone = copycat.piece(original).link;
    expect(clone != Position::NoPiece && copycat.piece(clone).type == PieceType::CopycatClone &&
           copycat.piece(clone).square == Position::square_from_name("f3"),
           "copycat construction creates the native file-mirrored clone");
    const int leftVictim = copycat.add_piece(PieceType::Pawn, Color::Black,
                                             Position::square_from_name("d4"));
    const int rightVictim = copycat.add_piece(PieceType::Pawn, Color::Black,
                                              Position::square_from_name("e4"));
    Undo pairedMove;
    expect(copycat.make_move(require_move(copycat, "c3-d4"), pairedMove),
           "copycat paired capture applies");
    expect(!copycat.piece(leftVictim).alive && !copycat.piece(rightVictim).alive,
           "copycat and clone capture both mirrored targets");
    expect(copycat.piece(original).square == Position::square_from_name("d4") &&
           copycat.piece(clone).square == Position::square_from_name("e4"),
           "available copycat clone follows the mirrored displacement");

    Position frozenCopycat;
    frozenCopycat.add_piece(PieceType::King, Color::White,
                            Position::square_from_name("a1"));
    frozenCopycat.add_piece(PieceType::King, Color::Black,
                            Position::square_from_name("h10"));
    const int thawedHalf = frozenCopycat.add_piece(
      PieceType::Copycat, Color::White, Position::square_from_name("c3"));
    const int frozenHalf = frozenCopycat.piece(thawedHalf).link;
    frozenCopycat.piece(frozenHalf).freezeCount = 1;
    Undo frozenPartnerMove;
    expect(frozenCopycat.make_move(
             require_move(frozenCopycat, "c3-d4"), frozenPartnerMove),
           "CopyCat can move while its linked partner is unavailable");
    expect(frozenCopycat.piece(thawedHalf).square == Position::square_from_name("d4") &&
             frozenCopycat.piece(frozenHalf).square == Position::square_from_name("f3"),
           "frozen CopyCat partner remains stationary and the UPN link stays exact");

    Position dyingCopycat;
    dyingCopycat.add_piece(PieceType::King, Color::White,
                           Position::square_from_name("a1"));
    const int doomedHost = dyingCopycat.add_piece(
        PieceType::Copycat, Color::White, Position::square_from_name("c3"));
    const int doomedClone = dyingCopycat.piece(doomedHost).link;
    const int lethalGoop = dyingCopycat.add_piece(
        PieceType::Goop, Color::Black, Position::square_from_name("d4"));
    const int queuedVictim = dyingCopycat.add_piece(
        PieceType::Rook, Color::Black, Position::square_from_name("e4"));
    dyingCopycat.add_piece(PieceType::King, Color::Black,
                           Position::square_from_name("h10"));
    Undo dyingPairMove;
    expect(dyingCopycat.make_move(
               require_move(dyingCopycat, "c3-d4"), dyingPairMove),
           "CopyCat paired attack with a lethal first target applies");
    expect(!dyingCopycat.piece(lethalGoop).alive &&
               !dyingCopycat.piece(doomedHost).alive &&
               !dyingCopycat.piece(doomedClone).alive &&
               !dyingCopycat.piece(queuedVictim).alive,
           "queued CopyCat half resolves its target after linked Goop death");

    Position rescuedCopycat;
    rescuedCopycat.add_piece(PieceType::King, Color::White,
                             Position::square_from_name("a1"));
    const int copycatAngel = rescuedCopycat.add_piece(
        PieceType::Angel, Color::White, Position::square_from_name("b2"));
    const int rescuedHost = rescuedCopycat.add_piece(
        PieceType::Copycat, Color::White, Position::square_from_name("c3"));
    const int movingClone = rescuedCopycat.piece(rescuedHost).link;
    const int copycatGoop = rescuedCopycat.add_piece(
        PieceType::Goop, Color::Black, Position::square_from_name("d4"));
    rescuedCopycat.add_piece(PieceType::King, Color::Black,
                             Position::square_from_name("h10"));
    rescuedCopycat.add_piece(PieceType::Pawn, Color::Black,
                             Position::square_from_name("h9"));
    Undo protectCopycat;
    expect(rescuedCopycat.make_move(
               require_move(rescuedCopycat, "b2&c3"), protectCopycat),
           "Angel can attach to one CopyCat half");
    Undo waitForCopycat;
    expect(rescuedCopycat.make_move(
               require_move(rescuedCopycat, "h9-h8"), waitForCopycat),
           "opponent wait returns the turn to the protected CopyCat");
    Undo rescuedPairMove;
    expect(rescuedCopycat.make_move(
               require_move(rescuedCopycat, "c3-d4"), rescuedPairMove),
           "Angel-protected CopyCat attack on Goop applies");
    expect(rescuedCopycat.piece(rescuedHost).alive &&
               rescuedCopycat.piece(rescuedHost).square ==
                   Position::square_from_name("b2") &&
               rescuedCopycat.piece_on(Position::square_from_name("b2")) ==
                   rescuedHost &&
               rescuedCopycat.piece_on(Position::square_from_name("d4")) ==
                   Position::NoPiece &&
               !rescuedCopycat.piece(copycatAngel).alive &&
               !rescuedCopycat.piece(copycatGoop).alive &&
               rescuedCopycat.piece(movingClone).square ==
                   Position::square_from_name("e4"),
           "CopyCat retains its Angel relocation while the paired half completes");

    Position partnerProtectedCopycat;
    partnerProtectedCopycat.add_piece(PieceType::King, Color::White,
                                      Position::square_from_name("a1"));
    const int partnerAngel = partnerProtectedCopycat.add_piece(
      PieceType::Angel, Color::White, Position::square_from_name("b2"));
    const int unprotectedHalf = partnerProtectedCopycat.add_piece(
      PieceType::Copycat, Color::White, Position::square_from_name("c3"));
    const int protectedPartner = partnerProtectedCopycat.piece(unprotectedHalf).link;
    partnerProtectedCopycat.add_piece(PieceType::Rook, Color::Black,
                                      Position::square_from_name("c8"));
    partnerProtectedCopycat.add_piece(PieceType::King, Color::Black,
                                      Position::square_from_name("h10"));
    Undo protectPartnerOnly;
    expect(partnerProtectedCopycat.make_move(
             require_move(partnerProtectedCopycat, "b2&f3"),
             protectPartnerOnly),
           "Angel can attach specifically to the other CopyCat half");
    Undo killUnprotectedHalf;
    expect(partnerProtectedCopycat.make_move(
             require_move(partnerProtectedCopycat, "c8-c3"),
             killUnprotectedHalf),
           "the unprotected CopyCat half can be struck first");
    expect(!partnerProtectedCopycat.piece(unprotectedHalf).alive &&
             !partnerProtectedCopycat.piece(protectedPartner).alive &&
             partnerProtectedCopycat.piece(partnerAngel).alive &&
             !partnerProtectedCopycat.piece(partnerAngel).onBoard,
           "linked death bypasses the partner's Angel and never creates a singleton CopyCat");

    Position hiddenPrimary;
    hiddenPrimary.add_piece(PieceType::King, Color::White,
                            Position::square_from_name("a1"));
    hiddenPrimary.add_piece(PieceType::King, Color::Black,
                            Position::square_from_name("h10"));
    const int hiddenHost = hiddenPrimary.add_piece(
      PieceType::Copycat, Color::White, Position::square_from_name("c3"));
    const int hiddenClone = hiddenPrimary.piece(hiddenHost).link;
    const int primaryGhost = hiddenPrimary.add_piece(
      PieceType::Ghost, Color::Black, Position::square_from_name("d4"));
    hiddenPrimary.piece(primaryGhost).visible = false;
    Undo hiddenPrimaryMove;
    expect(hiddenPrimary.make_move(
             require_move(hiddenPrimary, "c3-d4"), hiddenPrimaryMove),
           "CopyCat can target a hidden enemy Ghost");
    expect(!hiddenPrimary.piece(primaryGhost).alive &&
             hiddenPrimary.piece(hiddenHost).alive &&
             hiddenPrimary.piece(hiddenHost).square ==
               Position::square_from_name("d4") &&
             hiddenPrimary.piece(hiddenClone).square ==
               Position::square_from_name("e4"),
           "CopyCat captures a hidden Ghost while both halves complete their move");

    Position hiddenMirror;
    hiddenMirror.add_piece(PieceType::King, Color::White,
                           Position::square_from_name("a1"));
    hiddenMirror.add_piece(PieceType::King, Color::Black,
                           Position::square_from_name("h10"));
    const int mirrorHost = hiddenMirror.add_piece(
      PieceType::Copycat, Color::White, Position::square_from_name("c3"));
    const int mirrorClone = hiddenMirror.piece(mirrorHost).link;
    const int mirrorGhost = hiddenMirror.add_piece(
      PieceType::Ghost, Color::Black, Position::square_from_name("e4"));
    hiddenMirror.piece(mirrorGhost).visible = false;
    Undo hiddenMirrorMove;
    expect(hiddenMirror.make_move(
             require_move(hiddenMirror, "c3-d4"), hiddenMirrorMove),
           "CopyCat move can send its clone onto a hidden enemy Ghost");
    expect(!hiddenMirror.piece(mirrorGhost).alive &&
             hiddenMirror.piece(mirrorHost).square ==
               Position::square_from_name("d4") &&
             hiddenMirror.piece(mirrorClone).alive &&
             hiddenMirror.piece(mirrorClone).square ==
               Position::square_from_name("e4"),
           "the mirrored CopyCat half captures its hidden Ghost normally");

    Position hiddenAlly;
    hiddenAlly.add_piece(PieceType::King, Color::White,
                         Position::square_from_name("a1"));
    hiddenAlly.add_piece(PieceType::King, Color::Black,
                         Position::square_from_name("h10"));
    hiddenAlly.add_piece(PieceType::Copycat, Color::White,
                         Position::square_from_name("c3"));
    const int alliedCopycatGhost = hiddenAlly.add_piece(
      PieceType::Ghost, Color::White, Position::square_from_name("d4"));
    hiddenAlly.piece(alliedCopycatGhost).visible = false;
    expect(!hiddenAlly.move_from_string("c3-d4").has_value(),
           "an allied hidden Ghost blocks its CopyCat half as friendly occupancy");

    Position hiddenPrince;
    hiddenPrince.add_piece(PieceType::King, Color::White,
                           Position::square_from_name("a1"));
    hiddenPrince.add_piece(PieceType::King, Color::Black,
                           Position::square_from_name("h10"));
    const int prince = hiddenPrince.add_piece(
      PieceType::Prince, Color::White, Position::square_from_name("d4"));
    const int princeGhost = hiddenPrince.add_piece(
      PieceType::Ghost, Color::Black, Position::square_from_name("e5"));
    hiddenPrince.piece(princeGhost).visible = false;
    Undo princeCapture;
    expect(hiddenPrince.make_move(
             require_move(hiddenPrince, "d4-e5"), princeCapture),
           "Prince can target a hidden enemy Ghost");
    expect(hiddenPrince.piece(prince).alive &&
             hiddenPrince.piece(prince).square ==
               Position::square_from_name("e5") &&
             !hiddenPrince.piece(princeGhost).alive &&
             hiddenPrince.side_to_move() == Color::Black,
           "Prince captures a hidden Ghost normally and receives no quiet second step");

    Position hiddenBerserker;
    hiddenBerserker.add_piece(PieceType::King, Color::White,
                              Position::square_from_name("a1"));
    hiddenBerserker.add_piece(PieceType::King, Color::Black,
                              Position::square_from_name("h10"));
    const int berserker = hiddenBerserker.add_piece(
      PieceType::Berserker, Color::White, Position::square_from_name("d4"));
    const int berserkerGhost = hiddenBerserker.add_piece(
      PieceType::Ghost, Color::Black, Position::square_from_name("e5"));
    hiddenBerserker.piece(berserkerGhost).visible = false;
    Undo berserkerCapture;
    expect(hiddenBerserker.make_move(
             require_move(hiddenBerserker, "d4-e5"), berserkerCapture),
           "Berserker can target a hidden enemy Ghost");
    expect(hiddenBerserker.piece(berserker).alive &&
             hiddenBerserker.piece(berserker).square ==
               Position::square_from_name("e5") &&
             hiddenBerserker.piece(berserker).power == 1 &&
             !hiddenBerserker.piece(berserkerGhost).alive,
           "Berserker captures a hidden Ghost normally and gains power");

    Position hiddenSniper;
    hiddenSniper.add_piece(PieceType::King, Color::White,
                           Position::square_from_name("a1"));
    hiddenSniper.add_piece(PieceType::King, Color::Black,
                           Position::square_from_name("h10"));
    const int sniper = hiddenSniper.add_piece(
      PieceType::Sniper, Color::White, Position::square_from_name("d4"));
    const int sniperGhost = hiddenSniper.add_piece(
      PieceType::Ghost, Color::Black, Position::square_from_name("e4"));
    hiddenSniper.piece(sniperGhost).visible = false;
    Undo sniperCollision;
    expect(hiddenSniper.make_move(
             require_move(hiddenSniper, "d4-e4"), sniperCollision),
           "Sniper may side-step into a hidden enemy Ghost");
    expect(!hiddenSniper.piece(sniper).alive &&
             !hiddenSniper.piece(sniperGhost).alive,
           "Sniper and hidden Ghost mutually knock out on a blind lateral collision");
}

void test_native_fisherman_rays() {
    Position position;
    position.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    position.add_piece(PieceType::King, Color::Black, Position::square_from_name("h8"));
    const int fisherman = position.add_piece(PieceType::Fisherman, Color::White,
                                              Position::square_from_name("d4"));
    const int ally = position.add_piece(PieceType::Rook, Color::White,
                                        Position::square_from_name("d7"));
    expect(position.move_from_string("d4-d5").has_value() &&
           position.move_from_string("d4-d6").has_value(),
           "fisherman has queen-like quiet ray moves before the first character");
    expect(position.move_from_string("d4!d7").has_value(),
           "fisherman can hook a distant allied character");
    Undo pull;
    expect(position.make_move(require_move(position, "d4!d7"), pull),
           "fisherman pull applies");
    expect(position.piece(fisherman).square == Position::square_from_name("d4") &&
           position.piece(ally).square == Position::square_from_name("d5"),
           "fisherman stays put and pulls the target to its adjacent ray square");

    Position adjacent;
    adjacent.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    adjacent.add_piece(PieceType::King, Color::Black, Position::square_from_name("h8"));
    adjacent.add_piece(PieceType::Fisherman, Color::White, Position::square_from_name("d4"));
    adjacent.add_piece(PieceType::Pawn, Color::Black, Position::square_from_name("e5"));
    expect(!adjacent.move_from_string("d4!e5").has_value() &&
           !adjacent.move_from_string("d4-e5").has_value(),
           "fisherman cannot hook or capture an adjacent visible character");

    Position blindGhost;
    blindGhost.add_piece(PieceType::King, Color::White,
                         Position::square_from_name("a1"));
    blindGhost.add_piece(PieceType::King, Color::Black,
                         Position::square_from_name("h10"));
    const int blindFisherman = blindGhost.add_piece(
      PieceType::Fisherman, Color::White, Position::square_from_name("d2"));
    const int unseenGhost = blindGhost.add_piece(
      PieceType::Ghost, Color::Black, Position::square_from_name("d5"));
    blindGhost.piece(unseenGhost).visible = false;
    Undo blindCollision;
    expect(blindGhost.make_move(
             require_move(blindGhost, "d2-d5"), blindCollision),
           "Fisherman may blindly enter an invisible enemy Ghost square");
    expect(!blindGhost.piece(blindFisherman).alive &&
             !blindGhost.piece(unseenGhost).alive,
           "native Fisherman/Ghost blind collision knocks out both characters");

    Position ghostRay;
    ghostRay.add_piece(PieceType::King, Color::White,
                       Position::square_from_name("a1"));
    ghostRay.add_piece(PieceType::King, Color::Black,
                       Position::square_from_name("h10"));
    const int rayFisherman = ghostRay.add_piece(
      PieceType::Fisherman, Color::White, Position::square_from_name("d2"));
    const int rayGhost = ghostRay.add_piece(
      PieceType::Ghost, Color::Black, Position::square_from_name("d5"));
    ghostRay.piece(rayGhost).visible = false;
    const int rayRook = ghostRay.add_piece(
      PieceType::Rook, Color::Black, Position::square_from_name("d8"));
    expect(ghostRay.move_from_string("d2-d6").has_value() &&
             ghostRay.move_from_string("d2!d8").has_value(),
           "Fisherman ray continues through a hidden Ghost to empty and hook cells");
    Undo throughGhost;
    expect(ghostRay.make_move(
             require_move(ghostRay, "d2!d8"), throughGhost),
           "Fisherman hooks the first visible target beyond a hidden Ghost");
    expect(ghostRay.piece(rayFisherman).square ==
             Position::square_from_name("d2") &&
             ghostRay.piece(rayRook).square ==
             Position::square_from_name("d3") &&
             ghostRay.piece(rayGhost).alive &&
             ghostRay.piece(rayGhost).square ==
             Position::square_from_name("d5"),
           "Fisherman pull through a hidden Ghost leaves that Ghost untouched");

    Position forcedBlindCollision;
    std::string forcedBlindError;
    expect(forcedBlindCollision.set_upn(
             "b;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;"
             "king,w,d1,0,0,0,0,0,1,-1,1,-1,0;"
             "ghost,w,d7,0,0,0,0,0,0,-1,1,-1,0;"
             "fisherman,b,d8,0,0,0,0,0,1,-1,1,-1,0;"
             "king,b,d10,0,0,0,0,0,1,-1,1,-1,0",
             &forcedBlindError),
           "Fisherman forced hidden-collision fixture parses: " +
             forcedBlindError);
    int pulledKing = Position::NoPiece;
    int collisionGhost = Position::NoPiece;
    for (int candidate = 0; candidate < forcedBlindCollision.piece_count(); ++candidate) {
        if (forcedBlindCollision.piece(candidate).type == PieceType::King &&
            forcedBlindCollision.piece(candidate).color == Color::White)
            pulledKing = candidate;
        if (forcedBlindCollision.piece(candidate).type == PieceType::Ghost)
            collisionGhost = candidate;
    }
    Undo forcedBlindPull;
    expect(forcedBlindCollision.make_move(
             require_move(forcedBlindCollision, "d8!d1"), forcedBlindPull),
           "Fisherman can pull a character into its own hidden Ghost");
    expect(pulledKing != Position::NoPiece &&
             collisionGhost != Position::NoPiece &&
             !forcedBlindCollision.piece(pulledKing).alive &&
             !forcedBlindCollision.piece(collisionGhost).alive,
           "forced Fisherman collision dispatches death to both the dragged King and hidden Ghost");

    Position protectedDraggedKing;
    protectedDraggedKing.add_piece(PieceType::King, Color::White,
                                    Position::square_from_name("d1"));
    const int kingAngel = protectedDraggedKing.add_piece(
      PieceType::Angel, Color::White, Position::square_from_name("a2"));
    const int savedDraggedKing = protectedDraggedKing.piece_on(
      Position::square_from_name("d1"));
    const int doomedCollisionGhost = protectedDraggedKing.add_piece(
      PieceType::Ghost, Color::White, Position::square_from_name("d7"));
    protectedDraggedKing.piece(doomedCollisionGhost).visible = false;
    protectedDraggedKing.add_piece(PieceType::Fisherman, Color::Black,
                                    Position::square_from_name("d8"));
    protectedDraggedKing.add_piece(PieceType::King, Color::Black,
                                    Position::square_from_name("h10"));
    Undo attachKingAngel;
    expect(protectedDraggedKing.make_move(
             require_move(protectedDraggedKing, "a2&d1"), attachKingAngel),
           "Angel can protect the character dragged into a hidden Ghost");
    Undo pullProtectedKing;
    expect(protectedDraggedKing.make_move(
             require_move(protectedDraggedKing, "d8!d1"), pullProtectedKing),
           "Fisherman collision invokes the dragged character's Angel callback");
    expect(protectedDraggedKing.piece(savedDraggedKing).alive &&
             protectedDraggedKing.piece(savedDraggedKing).square ==
               Position::square_from_name("a2") &&
             !protectedDraggedKing.piece(doomedCollisionGhost).alive &&
             !protectedDraggedKing.piece(kingAngel).alive,
           "dragged King Angel rescue does not suppress the Ghost's independent death");

    Position protectedCollisionGhost;
    const int doomedDraggedKing = protectedCollisionGhost.add_piece(
      PieceType::King, Color::White, Position::square_from_name("d1"));
    const int ghostAngel = protectedCollisionGhost.add_piece(
      PieceType::Angel, Color::White, Position::square_from_name("a2"));
    const int savedCollisionGhost = protectedCollisionGhost.add_piece(
      PieceType::Ghost, Color::White, Position::square_from_name("d7"));
    protectedCollisionGhost.piece(savedCollisionGhost).visible = false;
    protectedCollisionGhost.add_piece(PieceType::Jester, Color::White,
                                       Position::square_from_name("b1"));
    protectedCollisionGhost.add_piece(PieceType::Fisherman, Color::Black,
                                       Position::square_from_name("d8"));
    protectedCollisionGhost.add_piece(PieceType::King, Color::Black,
                                       Position::square_from_name("h10"));
    Undo attachGhostAngel;
    expect(protectedCollisionGhost.make_move(
             require_move(protectedCollisionGhost, "a2&d7"),
             attachGhostAngel),
           "Angel can protect the hidden Ghost in a Fisherman collision");
    Undo pullIntoProtectedGhost;
    expect(protectedCollisionGhost.make_move(
             require_move(protectedCollisionGhost, "d8!d1"),
             pullIntoProtectedGhost),
           "Fisherman collision invokes the hidden Ghost's Angel callback");
    expect(!protectedCollisionGhost.piece(doomedDraggedKing).alive &&
             protectedCollisionGhost.piece(savedCollisionGhost).alive &&
             protectedCollisionGhost.piece(savedCollisionGhost).square ==
               Position::square_from_name("a2") &&
             !protectedCollisionGhost.piece(ghostAngel).alive,
           "Ghost Angel rescue does not suppress the dragged King's independent death");

    Position explosiveCollision;
    explosiveCollision.add_piece(PieceType::King, Color::White,
                                  Position::square_from_name("a1"));
    const int pulledBomb = explosiveCollision.add_piece(
      PieceType::Bomb, Color::White, Position::square_from_name("d1"));
    const int explodedGhost = explosiveCollision.add_piece(
      PieceType::Ghost, Color::White, Position::square_from_name("d7"));
    explosiveCollision.piece(explodedGhost).visible = false;
    const int explodedFisherman = explosiveCollision.add_piece(
      PieceType::Fisherman, Color::Black, Position::square_from_name("d8"));
    explosiveCollision.add_piece(PieceType::King, Color::Black,
                                  Position::square_from_name("h10"));
    explosiveCollision.set_side_to_move(Color::Black);
    Undo pullBombIntoGhost;
    expect(explosiveCollision.make_move(
             require_move(explosiveCollision, "d8!d1"), pullBombIntoGhost),
           "Fisherman can drag a Bomb into a hidden Ghost collision");
    expect(!explosiveCollision.piece(pulledBomb).alive &&
             !explosiveCollision.piece(explodedGhost).alive &&
             !explosiveCollision.piece(explodedFisherman).alive,
           "dragged Bomb callback explodes the Ghost and adjacent Fisherman");

    Position forcedBlindThreat;
    expect(forcedBlindThreat.set_upn(
             "w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;"
             "king,w,d1,0,0,0,0,0,1,-1,1,-1,0;"
             "ghost,w,d7,0,0,0,0,0,0,-1,1,-1,0;"
             "fisherman,b,d8,0,0,0,0,0,1,-1,1,-1,0;"
             "king,b,d10,0,0,0,0,0,1,-1,1,-1,0",
             &forcedBlindError),
           "Fisherman forced-collision check fixture parses: " +
             forcedBlindError);
    forcedBlindThreat.add_piece(PieceType::Pawn, Color::White,
                                Position::square_from_name("a2"));
    expect(!forcedBlindThreat.move_from_string("a2-a3").has_value(),
           "a Fisherman pull that would drag the King into its own hidden Ghost filters unrelated replies as check");

    Position giantPull;
    giantPull.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    giantPull.add_piece(PieceType::King, Color::Black, Position::square_from_name("h8"));
    giantPull.add_piece(PieceType::Fisherman, Color::White, Position::square_from_name("c3"));
    const int giant = giantPull.add_piece(PieceType::Giant, Color::Black,
                                          Position::square_from_name("f6"));
    const int alliedCollision = giantPull.add_piece(PieceType::Pawn, Color::Black,
                                                     Position::square_from_name("d5"));
    const int enemyCollision = giantPull.add_piece(PieceType::Pawn, Color::White,
                                                    Position::square_from_name("e4"));
    Undo hook;
    expect(giantPull.make_move(require_move(giantPull, "c3!f6"), hook),
           "fisherman can hook a distant giant footprint");
    expect(giantPull.piece(giant).square == Position::square_from_name("d4"),
           "giant pull translates its anchor so the hooked footprint square lands adjacent");
    expect(!giantPull.piece(alliedCollision).alive && !giantPull.piece(enemyCollision).alive,
           "a forcibly pulled giant knocks out allies and enemies across its destination footprint");

    Position protectedPull;
    protectedPull.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    protectedPull.add_piece(PieceType::King, Color::Black, Position::square_from_name("h10"));
    protectedPull.add_piece(PieceType::Fisherman, Color::White,
                            Position::square_from_name("d4"));
    const int angel = protectedPull.add_piece(PieceType::Angel, Color::Black,
                                               Position::square_from_name("d8"));
    const int protectedRook = protectedPull.add_piece(PieceType::Rook, Color::Black,
                                                       Position::square_from_name("d7"));
    protectedPull.set_side_to_move(Color::Black);
    Undo attach;
    expect(protectedPull.make_move(require_move(protectedPull, "d8&d7"), attach),
           "opposing angel can attach before a fisherman pull");
    Undo protectedHook;
    expect(protectedPull.make_move(require_move(protectedPull, "d4!d7"), protectedHook),
           "fisherman pulls an angel-protected host");
    expect(protectedPull.piece(protectedRook).square == Position::square_from_name("d5") &&
           !protectedPull.piece(angel).onBoard &&
           protectedPull.piece(angel).square == Position::square_from_name("d5"),
           "attached angels follow a fisherman-relocated host without consuming the save");
}

void test_ghost_visibility_transitions() {
    Position hide;
    hide.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    hide.add_piece(PieceType::King, Color::Black, Position::square_from_name("h8"));
    const int hiddenGhost = hide.add_piece(PieceType::Ghost, Color::White,
                                            Position::square_from_name("d4"));
    Undo quiet;
    expect(hide.make_move(require_move(hide, "d4-e4"), quiet), "ghost quiet move applies");
    expect(!hide.piece(hiddenGhost).visible,
           "ghost becomes hidden after a quiet move away from an enemy royal");

    Position attack;
    attack.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    attack.add_piece(PieceType::King, Color::Black, Position::square_from_name("h8"));
    const int attackingGhost = attack.add_piece(PieceType::Ghost, Color::White,
                                                 Position::square_from_name("d4"));
    attack.add_piece(PieceType::Pawn, Color::Black, Position::square_from_name("e5"));
    Undo capture;
    expect(attack.make_move(require_move(attack, "d4-e5"), capture), "ghost attack applies");
    expect(attack.piece(attackingGhost).visible, "ghost reveals when it attacks");

    Position approach;
    approach.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    approach.add_piece(PieceType::King, Color::Black, Position::square_from_name("e6"));
    const int approachedGhost = approach.add_piece(PieceType::Ghost, Color::White,
                                                    Position::square_from_name("d4"));
    approach.piece(approachedGhost).visible = false;
    approach.set_side_to_move(Color::Black);
    Undo kingMove;
    expect(approach.make_move(require_move(approach, "e6-e5"), kingMove),
           "enemy king approaches hidden ghost");
    expect(approach.piece(approachedGhost).visible,
           "a King stepping beside a hidden Ghost reveals it in network play");

    Position jesterApproach;
    jesterApproach.add_piece(PieceType::King, Color::White,
                             Position::square_from_name("a1"));
    jesterApproach.add_piece(PieceType::King, Color::Black,
                             Position::square_from_name("h10"));
    jesterApproach.add_piece(PieceType::Jester, Color::Black,
                             Position::square_from_name("e6"));
    const int jesterApproachedGhost = jesterApproach.add_piece(
      PieceType::Ghost, Color::White, Position::square_from_name("d4"));
    jesterApproach.piece(jesterApproachedGhost).visible = false;
    jesterApproach.set_side_to_move(Color::Black);
    Undo jesterMove;
    expect(jesterApproach.make_move(
             require_move(jesterApproach, "e6-e5"), jesterMove) &&
             jesterApproach.piece(jesterApproachedGhost).visible,
           "a Jester stepping beside a hidden Ghost reveals it in network play");

    for (const PieceType royal : {PieceType::King, PieceType::Jester}) {
        Position ghostApproach;
        std::string rebuildError;
        expect(ghostApproach.set_upn(
          royal == PieceType::King
            ? "w;king,w,a1;king,b,e6;ghost,w,d4,0,0,0,0,0,0,-1,1,-1,0"
            : "w;king,w,a1;king,b,h10;jester,b,e6;ghost,w,d4,0,0,0,0,0,0,-1,1,-1,0",
          &rebuildError), "royal-range Ghost fixture parses: " + rebuildError);
        int ghostId = Position::NoPiece;
        for (int id = 0; id < ghostApproach.piece_count(); ++id)
            if (ghostApproach.piece(id).type == PieceType::Ghost)
                ghostId = id;
        Undo ghostMove;
        expect(ghostApproach.make_move(
                 require_move(ghostApproach, "d4-d5"), ghostMove) &&
                 ghostApproach.piece(ghostId).visible,
               std::string("a Ghost reveals when it steps into ") +
                 (royal == PieceType::King ? "King" : "Jester") + " range");
    }

    Position ghostAttacksParasite;
    ghostAttacksParasite.add_piece(
      PieceType::King, Color::White, Position::square_from_name("a1"));
    const int retaliatingGhost = ghostAttacksParasite.add_piece(
      PieceType::Ghost, Color::White, Position::square_from_name("c3"));
    ghostAttacksParasite.add_piece(
      PieceType::Parasite, Color::Black, Position::square_from_name("d4"));
    ghostAttacksParasite.add_piece(
      PieceType::King, Color::Black, Position::square_from_name("h10"));
    ghostAttacksParasite.piece(retaliatingGhost).visible = false;
    Undo parasiteRetaliation;
    expect(ghostAttacksParasite.make_move(
             require_move(ghostAttacksParasite, "c3-d4"),
             parasiteRetaliation) &&
             ghostAttacksParasite.piece(retaliatingGhost).color == Color::Black &&
             ghostAttacksParasite.piece(retaliatingGhost).parasiteTracked &&
             ghostAttacksParasite.piece(retaliatingGhost).visible,
           "a defending Parasite also leaves its permanent marker on the "
           "Ghost attacker it possesses");
    ghostAttacksParasite.undo_move(parasiteRetaliation);
    expect(ghostAttacksParasite.piece(retaliatingGhost).color == Color::White &&
             !ghostAttacksParasite.piece(retaliatingGhost).parasiteTracked &&
             !ghostAttacksParasite.piece(retaliatingGhost).visible,
           "undo restores Ghost visibility before Parasite retaliation");

    Position blindPawn;
    blindPawn.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    blindPawn.add_piece(PieceType::King, Color::Black, Position::square_from_name("h10"));
    const int pawn = blindPawn.add_piece(PieceType::Pawn, Color::White,
                                         Position::square_from_name("d4"));
    const int blockingGhost = blindPawn.add_piece(PieceType::Ghost, Color::Black,
                                                   Position::square_from_name("d5"));
    blindPawn.piece(blockingGhost).visible = false;
    Undo collision;
    expect(blindPawn.make_move(require_move(blindPawn, "d4-d5"), collision),
           "pawn can blindly advance into an unseen opposing ghost");
    expect(!blindPawn.piece(pawn).alive && !blindPawn.piece(blockingGhost).alive,
           "pawn and unseen ghost knock each other out on a blind forward collision");

    Position pawnThroughEnemy;
    pawnThroughEnemy.add_piece(PieceType::King, Color::White,
                               Position::square_from_name("a1"));
    pawnThroughEnemy.add_piece(PieceType::King, Color::Black,
                               Position::square_from_name("h10"));
    const int passingPawn = pawnThroughEnemy.add_piece(
      PieceType::Pawn, Color::White, Position::square_from_name("d2"));
    const int passedGhost = pawnThroughEnemy.add_piece(
      PieceType::Ghost, Color::Black, Position::square_from_name("d3"));
    pawnThroughEnemy.piece(passedGhost).visible = false;
    expect(pawnThroughEnemy.move_from_string("d2-d3").has_value() &&
             pawnThroughEnemy.move_from_string("d2-d4").has_value(),
           "an unmoved Pawn can stop on or double-step through a hidden enemy Ghost");
    Undo passThrough;
    expect(pawnThroughEnemy.make_move(
             require_move(pawnThroughEnemy, "d2-d4"), passThrough),
           "Pawn double-step through a hidden enemy Ghost applies");
    expect(pawnThroughEnemy.piece(passingPawn).alive &&
             pawnThroughEnemy.piece(passingPawn).square ==
               Position::square_from_name("d4") &&
             pawnThroughEnemy.piece(passedGhost).alive &&
             pawnThroughEnemy.piece(passedGhost).square ==
               Position::square_from_name("d3"),
           "a passed hidden Ghost remains on its intermediate square");
    pawnThroughEnemy.undo_move(passThrough);
    expect(pawnThroughEnemy.piece(passingPawn).square ==
             Position::square_from_name("d2") &&
             pawnThroughEnemy.piece(passedGhost).square ==
               Position::square_from_name("d3"),
           "undo restores a Pawn double-step through a hidden Ghost");

    Position pawnThroughAlly;
    pawnThroughAlly.add_piece(PieceType::King, Color::White,
                              Position::square_from_name("a1"));
    pawnThroughAlly.add_piece(PieceType::King, Color::Black,
                              Position::square_from_name("h10"));
    pawnThroughAlly.add_piece(PieceType::Pawn, Color::White,
                              Position::square_from_name("d2"));
    const int alliedGhost = pawnThroughAlly.add_piece(
      PieceType::Ghost, Color::White, Position::square_from_name("d3"));
    pawnThroughAlly.piece(alliedGhost).visible = false;
    expect(!pawnThroughAlly.move_from_string("d2-d3").has_value() &&
             pawnThroughAlly.move_from_string("d2-d4").has_value(),
           "a Pawn cannot stop on but can double-step through an allied hidden Ghost");

    Position pawnDoubleCollision;
    pawnDoubleCollision.add_piece(PieceType::King, Color::White,
                                  Position::square_from_name("a1"));
    pawnDoubleCollision.add_piece(PieceType::King, Color::Black,
                                  Position::square_from_name("h10"));
    const int collidingPawn = pawnDoubleCollision.add_piece(
      PieceType::Pawn, Color::White, Position::square_from_name("d2"));
    const int landingGhost = pawnDoubleCollision.add_piece(
      PieceType::Ghost, Color::Black, Position::square_from_name("d4"));
    pawnDoubleCollision.piece(landingGhost).visible = false;
    Undo doubleCollision;
    expect(pawnDoubleCollision.make_move(
             require_move(pawnDoubleCollision, "d2-d4"), doubleCollision),
           "Pawn may double-step onto a hidden enemy Ghost");
    expect(!pawnDoubleCollision.piece(collidingPawn).alive &&
             !pawnDoubleCollision.piece(landingGhost).alive,
           "Pawn and hidden Ghost mutually knock out on a double-step landing");

    Position hiddenRay;
    hiddenRay.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    hiddenRay.add_piece(PieceType::King, Color::Black, Position::square_from_name("h10"));
    hiddenRay.add_piece(PieceType::Rook, Color::White, Position::square_from_name("c4"));
    const int rayGhost = hiddenRay.add_piece(PieceType::Ghost, Color::Black,
                                             Position::square_from_name("d4"));
    hiddenRay.piece(rayGhost).visible = false;
    expect(hiddenRay.move_from_string("c4-d4").has_value() &&
           hiddenRay.move_from_string("c4-e4").has_value(),
           "an unseen opposing Ghost is a legal blind endpoint and transparent to a sliding ray");
    Undo blindRookCapture;
    expect(hiddenRay.make_move(
             require_move(hiddenRay, "c4-d4"), blindRookCapture),
           "an ordinary slider may enter an apparently empty enemy Ghost cell");
    expect(!hiddenRay.piece(rayGhost).alive &&
             hiddenRay.piece_on(Position::square_from_name("d4")) !=
               Position::NoPiece,
           "the ordinary slider survives while the hidden enemy Ghost is captured");

    Position friendlyHiddenRay;
    friendlyHiddenRay.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    friendlyHiddenRay.add_piece(PieceType::King, Color::Black, Position::square_from_name("h10"));
    friendlyHiddenRay.add_piece(PieceType::Rook, Color::White, Position::square_from_name("c4"));
    const int friendlyGhost = friendlyHiddenRay.add_piece(PieceType::Ghost, Color::White,
                                                           Position::square_from_name("d4"));
    friendlyHiddenRay.piece(friendlyGhost).visible = false;
    expect(!friendlyHiddenRay.move_from_string("c4-d4").has_value() &&
           friendlyHiddenRay.move_from_string("c4-e4").has_value(),
           "native sliding rays also pass through a hidden friendly ghost");

    Position hiddenDiagonal;
    hiddenDiagonal.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    hiddenDiagonal.add_piece(PieceType::King, Color::Black, Position::square_from_name("h10"));
    hiddenDiagonal.add_piece(PieceType::Pawn, Color::White, Position::square_from_name("d4"));
    const int diagonalGhost = hiddenDiagonal.add_piece(PieceType::Ghost, Color::Black,
                                                        Position::square_from_name("e5"));
    hiddenDiagonal.piece(diagonalGhost).visible = false;
    expect(!hiddenDiagonal.move_from_string("d4-e5").has_value(),
           "a pawn cannot deliberately capture an unseen ghost diagonally");

    struct BlindCaptureCase {
        PieceType type;
        const char* from;
        const char* to;
        bool actorSurvives;
        bool ghostSurvives;
    };
    const std::array<BlindCaptureCase, 11> ordinaryBlindCaptures = {{
      {PieceType::Queen, "c4", "d4", true, false},
      {PieceType::Rook, "c4", "d4", true, false},
      {PieceType::Bishop, "c3", "d4", true, false},
      {PieceType::Knight, "c3", "d5", true, false},
      {PieceType::Ninja, "c3", "d4", true, false},
      {PieceType::Turtle, "c3", "d3", true, false},
      {PieceType::Penguin, "c3", "d4", false, false},
      {PieceType::Ghost, "c3", "d4", true, false},
      {PieceType::Dragon, "c3", "d4", true, false},
      {PieceType::Bomb, "c3", "d3", false, false},
      {PieceType::Parasite, "c3", "d4", false, true},
    }};
    for (const BlindCaptureCase& fixture : ordinaryBlindCaptures) {
        Position candidate;
        candidate.add_piece(PieceType::King, Color::White,
                            Position::square_from_name("a1"));
        candidate.add_piece(PieceType::King, Color::Black,
                            Position::square_from_name("h10"));
        const int actor = candidate.add_piece(
          fixture.type, Color::White,
          Position::square_from_name(fixture.from));
        const int targetGhost = candidate.add_piece(
          PieceType::Ghost, Color::Black,
          Position::square_from_name(fixture.to));
        candidate.piece(targetGhost).visible = false;
        const std::string moveText =
          std::string(fixture.from) + '-' + fixture.to;
        Undo blindCapture;
        expect(candidate.make_move(require_move(candidate, moveText), blindCapture),
               std::string(Position::type_name(fixture.type)) +
                 " has its native hidden-Ghost endpoint");
        expect(candidate.piece(actor).alive == fixture.actorSurvives &&
                 candidate.piece(targetGhost).alive == fixture.ghostSurvives,
               std::string(Position::type_name(fixture.type)) +
                 " resolves its own death/capture callback on a hidden Ghost");
        if (fixture.type == PieceType::Parasite) {
            expect(candidate.piece(targetGhost).color == Color::White,
                   "Parasite possesses a hidden enemy Ghost instead of killing it");
            expect(candidate.piece(targetGhost).parasiteTracked &&
                     candidate.piece(targetGhost).visible,
                   "a Parasite-possessed Ghost carries a permanent public tracker");
            expect(!candidate.tablebase_substate(
                     targetGhost, PieceType::Ghost).has_value(),
                   "the two-state Ghost tablebase codec rejects a tracked Ghost");
            Position roundTrip;
            std::string roundTripError;
            const int trackedSquare = Position::square_from_name("d4");
            expect(roundTrip.set_upn(candidate.upn(), &roundTripError) &&
                     roundTrip.piece_on(trackedSquare) != Position::NoPiece &&
                     roundTrip.piece(roundTrip.piece_on(trackedSquare)).parasiteTracked &&
                     roundTrip.piece(roundTrip.piece_on(trackedSquare)).visible,
                   "Parasite-tracked Ghost state survives UPN round trip: " +
                     roundTripError);
            Undo reply;
            expect(candidate.make_move(
                     require_move(candidate, "h10-h9"), reply),
                   "opponent can reply before the possessed Ghost moves");
            Undo trackedMove;
            expect(candidate.make_move(
                     require_move(candidate, "d4-e4"), trackedMove) &&
                     candidate.piece(targetGhost).parasiteTracked &&
                     candidate.piece(targetGhost).visible,
                   "Parasite-tracked Ghost stays visible after a quiet move");
            candidate.undo_move(trackedMove);
            candidate.undo_move(reply);
            candidate.undo_move(blindCapture);
            expect(candidate.piece(actor).alive &&
                     candidate.piece(targetGhost).alive &&
                     !candidate.piece(targetGhost).parasiteTracked &&
                     !candidate.piece(targetGhost).visible,
                   "undo restores an untracked hidden Ghost before possession");
        }
    }

    Position royalBlindCapture;
    const int blindKing = royalBlindCapture.add_piece(
      PieceType::King, Color::White, Position::square_from_name("c3"));
    royalBlindCapture.add_piece(PieceType::King, Color::Black,
                                Position::square_from_name("h10"));
    const int royalGhost = royalBlindCapture.add_piece(
      PieceType::Ghost, Color::Black, Position::square_from_name("d4"));
    royalBlindCapture.piece(royalGhost).visible = false;
    Undo royalBlindMove;
    expect(royalBlindCapture.make_move(
             require_move(royalBlindCapture, "c3-d4"), royalBlindMove) &&
             royalBlindCapture.piece(blindKing).alive &&
             !royalBlindCapture.piece(royalGhost).alive,
           "King uses the ordinary native blind-capture path for a hidden Ghost");

    Position jesterBlindCapture;
    jesterBlindCapture.add_piece(PieceType::King, Color::White,
                                 Position::square_from_name("a1"));
    const int blindJester = jesterBlindCapture.add_piece(
      PieceType::Jester, Color::White, Position::square_from_name("c3"));
    jesterBlindCapture.add_piece(PieceType::King, Color::Black,
                                 Position::square_from_name("h10"));
    const int jesterGhost = jesterBlindCapture.add_piece(
      PieceType::Ghost, Color::Black, Position::square_from_name("d4"));
    jesterBlindCapture.piece(jesterGhost).visible = false;
    Undo jesterBlindMove;
    expect(jesterBlindCapture.make_move(
             require_move(jesterBlindCapture, "c3-d4"), jesterBlindMove) &&
             jesterBlindCapture.piece(blindJester).alive &&
             !jesterBlindCapture.piece(jesterGhost).alive,
           "Jester uses the same ordinary blind-capture path as the King silhouette");
}

void test_search_and_perft_regressions() {
    Position tactical;
    tactical.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    tactical.add_piece(PieceType::King, Color::Black, Position::square_from_name("b2"));
    tactical.add_piece(PieceType::Pawn, Color::White, Position::square_from_name("h2"));
    Search search(1);
    SearchLimits limits;
    limits.depth = 3;
    const SearchResult first = search.think(tactical, limits);
    const SearchResult cached = search.think(tactical, limits);
    expect(first.bestMove && tactical.move_to_string(*first.bestMove) == "a1-b2",
           "search chooses an immediate real-king knockout");
    expect(cached.bestMove && tactical.move_to_string(*cached.bestMove) == "a1-b2",
           "root transposition hit retains a playable principal variation");

    Position rootChoice;
    rootChoice.add_piece(PieceType::King, Color::White,
                         Position::square_from_name("a1"));
    rootChoice.add_piece(PieceType::King, Color::Black,
                         Position::square_from_name("h10"));
    rootChoice.add_piece(PieceType::Pawn, Color::White,
                         Position::square_from_name("h2"));
    search.think(rootChoice, limits);
    SearchLimits restricted = limits;
    restricted.rootMoves = {require_move(rootChoice, "h2-h3")};
    const SearchResult forcedRoot = search.think(rootChoice, restricted);
    expect(forcedRoot.bestMove && rootChoice.move_to_string(*forcedRoot.bestMove) == "h2-h3",
           "restricted root search ignores an unrestricted transposition hit");

    Position fixture;
    std::string error;
    const std::string upn =
      "w;hm=0;fm=1;ep=-;cont=0;forced=-1;king,w,e1;jester,w,d1;ninja,w,b2;"
      "penguin,w,c2;devil,w,f2;sniper,w,g2;checker,w,h2;sludge,w,a2;king,b,e10;"
      "jester,b,d10;ninja,b,b9;penguin,b,c9;devil,b,f9;sniper,b,g9;checker,b,h9;"
      "sludge,b,a9";
    expect(fixture.set_upn(upn, &error), "perft fixture parses: " + error);
    expect(fixture.perft(1) == 44 && fixture.perft(2) == 1936 && fixture.perft(3) == 74045,
           "mixed-roster perft remains stable at depths one through three");

    const auto legal = fixture.legal_moves();
    const auto forcing = fixture.legal_forcing_moves();
    const auto expectedForcing = [&fixture](const Move& move) {
        if (fixture.is_capture(move))
            return true;
        if (move.kind == MoveKind::Pull) {
            const int actor = fixture.piece_on(move.from);
            const int target = fixture.piece_on(move.to);
            return actor != Position::NoPiece && target != Position::NoPiece &&
                   fixture.piece(actor).color != fixture.piece(target).color;
        }
        return move.kind == MoveKind::Swap &&
               move.auxiliary < fixture.piece_count() &&
               fixture.piece(move.auxiliary).type == PieceType::Giant;
    };
    for (const Move& move : legal)
        expect((std::find(forcing.begin(), forcing.end(), move) != forcing.end()) ==
                 expectedForcing(move),
               "forcing frontier exactly matches the former full-frontier filter");
    expect(std::all_of(forcing.begin(), forcing.end(), [&legal](const Move& move) {
               return std::find(legal.begin(), legal.end(), move) != legal.end();
           }),
           "every forcing-frontier action remains fully legal");

    // A live relocation self-play exposed a reversible Fisherman/check line
    // whose quiescence search could recurse until the process stack overflowed.
    // Prime the same persistent history over the preceding position sequence,
    // then require the formerly crashing search to stop at its node budget.
    Position cycle;
    expect(cycle.set_upn(
      "w;hm=0;fm=1;ep=-;cont=0;forced=-1;king,w,e1;mage,w,c2;giant,w,f2;"
      "fisherman,w,a2;rook,w,d3;king,b,e10;mage,b,c9;giant,b,f8;"
      "fisherman,b,a9;rook,b,d8", &error),
      "relocation quiescence-cycle fixture parses: " + error);
    Search cycleSearch(1);
    SearchLimits cycleLimits;
    cycleLimits.depth = 20;
    cycleLimits.nodes = 50'000;
    const std::array<std::pair<const char*, const char*>, 7> prelude = {{
      {"a2-d5", "d8-e8"}, {"e1-f1", "c9~f9"},
      {"c2~d5", "a9-a2"}, {"d5~g2", "c8-c6"},
      {"d3-d6", "e8-e3"}, {"c2-c6", "a2-c2"},
      {"c6!f9", "c2!c6"},
    }};
    auto apply = [](Position& position, const char* notation) {
        Undo undo;
        const auto move = position.move_from_string(notation);
        return move && position.make_move(*move, undo);
    };
    for (const auto& [whiteMove, blackMove] : prelude) {
        expect(apply(cycle, whiteMove),
               std::string("cycle prelude accepts ") + whiteMove);
        const SearchResult priming = cycleSearch.think(cycle, cycleLimits);
        expect(priming.bestMove.has_value(),
               "persistent cycle search returns a move while priming history");
        expect(apply(cycle, blackMove),
               std::string("cycle prelude accepts ") + blackMove);
    }
    expect(apply(cycle, "c3-c7"), "cycle trigger position accepts c3-c7");
    const SearchResult boundedCycle = cycleSearch.think(cycle, cycleLimits);
    expect(boundedCycle.bestMove.has_value() && boundedCycle.nodes <= cycleLimits.nodes,
           "forcing quiescence cycles respect the hard ply/node bounds");

    Position fishermanGiantMate;
    expect(fishermanGiantMate.set_upn(
      "b;hm=1;fm=1;ep=-;cont=0;forced=-1;epv=-1;"
      "fisherman,w,e10,0,0,0,0,0,1,-1,1,-1,0;"
      "king,b,e9,0,0,0,0,0,1,-1,1,-1,0;"
      "giant,w,a7,0,0,0,0,0,1,-1,1,-1,0;"
      "king,w,f7,0,0,0,0,1,1,-1,1,-1,0", &error),
      "Fisherman/Giant native-mate regression parses: " + error);
    SearchLimits nativeMateLimits;
    nativeMateLimits.depth = 16;
    nativeMateLimits.useTablebases = false;
    Search nativeMateSearch(8);
    const SearchResult nativeMate = nativeMateSearch.think(
      fishermanGiantMate, nativeMateLimits);
    expect(nativeMate.score < -29'900 &&
             nativeMate.principalVariation.size() == 14,
           "native search reports the complete fourteen-action mate proof");
    Position mateLine = fishermanGiantMate;
    std::map<std::uint64_t, int> mateOccurrences{{mateLine.key(), 1}};
    bool legalMateLine = true;
    bool crossedThreefold = false;
    for (const Move& move : nativeMate.principalVariation) {
        Undo undo;
        legalMateLine = legalMateLine && mateLine.make_move(move, undo);
        if (!legalMateLine)
            break;
        crossedThreefold = crossedThreefold || ++mateOccurrences[mateLine.key()] >= 3;
    }
    expect(legalMateLine && !crossedThreefold && mateLine.game_over() &&
             mateLine.winner() == Color::White,
           "the native mate PV is legal, repetition-safe, and terminal");
    const SearchResult cachedNativeMate = nativeMateSearch.think(
      fishermanGiantMate, nativeMateLimits);
    expect(cachedNativeMate.score == nativeMate.score &&
             cachedNativeMate.principalVariation.size() == 14 &&
             cachedNativeMate.nodes < nativeMate.nodes,
           "a warm TT returns the complete mate PV without repeating its proof search");
}

void test_exact_tablebase_probing() {
    const auto moved = [](Position& position, PieceType type, Color color,
                          const char* square) {
        const int id = position.add_piece(type, color, Position::square_from_name(square));
        position.piece(id).moved = true;
        return id;
    };
    const std::filesystem::path ownerGiant =
      std::filesystem::path("../tablebases/kjestergiantk.uftb");
    expect(TablebaseProbe::uses_compatible_codec(ownerGiant.string()),
           "corrected folded-Giant tablebase carries the v7 anchor marker");
    const bool queenGiantInstalled = TablebaseProbe::uses_compatible_codec(
      "../tablebases/kqueengiantk.uftb");
    {
        std::array<char, 64> header{};
        std::ifstream source(ownerGiant, std::ios::binary);
        source.read(header.data(), header.size());
        expect(source.gcount() == static_cast<std::streamsize>(header.size()),
               "folded-Giant v7 header is available for corruption tests");
        const std::filesystem::path temporary =
          std::filesystem::temp_directory_path() /
          "ultimatefish-malformed-giant-codec.uftb";
        const auto write = [&](const std::array<char, 64>& bytes) {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            output.write(bytes.data(), bytes.size());
        };
        auto malformed = header;
        malformed[56] ^= 1;
        write(malformed);
        expect(!TablebaseProbe::uses_compatible_codec(temporary.string()),
               "runtime rejects a malformed GiantAnchorV2 marker");
        auto stale = header;
        const std::uint32_t version5 = 5;
        std::memcpy(stale.data() + 8, &version5, sizeof(version5));
        write(stale);
        expect(!TablebaseProbe::uses_compatible_codec(temporary.string()),
               "runtime rejects a pre-v7 folded-Giant payload");
        auto nonGiant = header;
        const std::uint32_t bishop = static_cast<std::uint32_t>(PieceType::Bishop);
        std::memcpy(nonGiant.data() + 40, &bishop, sizeof(bishop));
        write(nonGiant);
        expect(!TablebaseProbe::uses_compatible_codec(temporary.string()),
               "runtime rejects v7 on a non-Giant material class");
        std::error_code removeError;
        std::filesystem::remove(temporary, removeError);
    }
    Position staleGiantQueen;
    moved(staleGiantQueen, PieceType::King, Color::White, "a1");
    moved(staleGiantQueen, PieceType::Queen, Color::White, "c3");
    moved(staleGiantQueen, PieceType::Giant, Color::White, "f4");
    moved(staleGiantQueen, PieceType::King, Color::Black, "h10");
    expect(TablebaseProbe::probe(staleGiantQueen).has_value() ==
             queenGiantInstalled,
           "runtime indexes a folded-Giant payload iff it authenticates as v7");
    Position queen;
    moved(queen, PieceType::King, Color::White, "a1");
    moved(queen, PieceType::Queen, Color::White, "b1");
    moved(queen, PieceType::King, Color::Black, "h10");
    const auto white = TablebaseProbe::probe(queen);
    expect(white && white->wdl == TablebaseWdl::Win && white->dtw > 0,
           "bundled KQK tablebase returns an exact moved-state win");

    Position mirroredColors;
    moved(mirroredColors, PieceType::King, Color::Black, "a1");
    moved(mirroredColors, PieceType::Queen, Color::Black, "b1");
    moved(mirroredColors, PieceType::King, Color::White, "h10");
    mirroredColors.set_side_to_move(Color::Black);
    const auto black = TablebaseProbe::probe(mirroredColors);
    expect(white && black && black->wdl == white->wdl && black->dtw == white->dtw,
           "tablebase color canonicalization preserves exact WDL and DTW");

    Position whiteSniper;
    moved(whiteSniper, PieceType::King, Color::White, "b1");
    moved(whiteSniper, PieceType::Sniper, Color::White, "b2");
    moved(whiteSniper, PieceType::King, Color::Black, "a8");
    whiteSniper.set_side_to_move(Color::Black);
    const auto whiteSniperResult = TablebaseProbe::probe(whiteSniper);
    Position blackSniper;
    moved(blackSniper, PieceType::King, Color::Black, "b10");
    moved(blackSniper, PieceType::Sniper, Color::Black, "b9");
    moved(blackSniper, PieceType::King, Color::White, "a3");
    blackSniper.set_side_to_move(Color::White);
    const auto blackSniperResult = TablebaseProbe::probe(blackSniper);
    expect(!whiteSniperResult && !blackSniperResult,
           "pre-turn-start Sniper tablebases are quarantined for both colors");

    Position unmovedBishopDragon;
    std::string unmovedBishopDragonError;
    expect(unmovedBishopDragon.set_upn(
      "w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;"
      "king,w,d1,0,0,0,0,0,1,-1,1,-1,0;"
      "king,b,d10,0,0,0,0,0,1,-1,1,-1,0;"
      "dragon,w,d3,0,0,0,0,0,1,-1,1,-1,0;"
      "bishop,b,f9,0,0,0,0,0,1,-1,1,-1,0",
      &unmovedBishopDragonError),
      "unmoved Bishop/Dragon regression parses: " +
        unmovedBishopDragonError);
    const auto unmovedBishopDragonResult =
      TablebaseProbe::probe(unmovedBishopDragon);
    expect(unmovedBishopDragonResult &&
             unmovedBishopDragonResult->wdl == TablebaseWdl::Win &&
             unmovedBishopDragonResult->dtw == 129,
           "inert unmoved flags reuse the exact Bishop/Dragon tablebase");
    Position movedBishopDragon = unmovedBishopDragon;
    for (int id = 0; id < movedBishopDragon.piece_count(); ++id)
        movedBishopDragon.piece(id).moved = true;
    const auto movedBishopDragonResult =
      TablebaseProbe::probe(movedBishopDragon);
    expect(unmovedBishopDragonResult && movedBishopDragonResult &&
             unmovedBishopDragonResult->wdl == movedBishopDragonResult->wdl &&
             unmovedBishopDragonResult->dtw == movedBishopDragonResult->dtw,
           "Bishop, Dragon, and no-Rook King moved bits are outcome-inert");

    Position rookState;
    const int whiteRookKing = moved(
      rookState, PieceType::King, Color::White, "d1");
    const int whiteRook = moved(
      rookState, PieceType::Rook, Color::White, "h1");
    moved(rookState, PieceType::King, Color::Black, "d10");
    const auto movedRookResult = TablebaseProbe::probe(rookState);
    expect(movedRookResult.has_value(),
           "bundled moved-state KRK tablebase is probeable");
    Position inertUnmovedRook = rookState;
    inertUnmovedRook.piece(whiteRook).moved = false;
    const auto inertUnmovedRookResult = TablebaseProbe::probe(inertUnmovedRook);
    expect(movedRookResult && inertUnmovedRookResult &&
             movedRookResult->wdl == inertUnmovedRookResult->wdl &&
             movedRookResult->dtw == inertUnmovedRookResult->dtw,
           "an unmoved Rook is inert when every royal has moved");
    Position inertUnmovedKing = rookState;
    inertUnmovedKing.piece(whiteRookKing).moved = false;
    const auto inertUnmovedKingResult = TablebaseProbe::probe(inertUnmovedKing);
    expect(movedRookResult && inertUnmovedKingResult &&
             movedRookResult->wdl == inertUnmovedKingResult->wdl &&
             movedRookResult->dtw == inertUnmovedKingResult->dtw,
           "an unmoved royal is inert when every Rook has moved");
    Position liveCastle = rookState;
    liveCastle.piece(whiteRookKing).moved = false;
    liveCastle.piece(whiteRook).moved = false;
    const std::vector<Move> liveCastleMoves = liveCastle.legal_moves();
    const bool advertisesCastle = std::any_of(
      liveCastleMoves.begin(), liveCastleMoves.end(),
      [](const Move& move) { return move.kind == MoveKind::Castle; });
    expect(advertisesCastle && !TablebaseProbe::probe(liveCastle),
           "a live King/Rook castling privilege stays outside the no-castling table");

    for (const auto [type, square] : {
           std::pair{PieceType::Jester, "b1"},
           std::pair{PieceType::Bomb, "b1"},
           std::pair{PieceType::Parasite, "b1"},
           std::pair{PieceType::Giant, "b1"},
         }) {
        Position special;
        moved(special, PieceType::King, Color::White, "a4");
        moved(special, type, Color::White, square);
        moved(special, PieceType::King, Color::Black, "h10");
        expect(TablebaseProbe::probe(special).has_value(),
               std::string("bundled K+") + std::string(Position::type_name(type)) +
                 "+K tablebase is probeable");
    }

    for (const PieceType type : {PieceType::Pawn, PieceType::Ghost,
                                 PieceType::Sniper,
                                 PieceType::Prince, PieceType::Penguin}) {
        Position stateful;
        moved(stateful, PieceType::King, Color::White, "a1");
        const int extra = moved(stateful, type, Color::White, "c3");
        moved(stateful, PieceType::King, Color::Black, "h10");
        if (type == PieceType::Pawn)
            stateful.piece(extra).moved = false;
        else if (type == PieceType::Ghost)
            stateful.piece(extra).visible = false;
        else if (type == PieceType::Sniper)
            stateful.piece(extra).cooldown = 3;
        expect(TablebaseProbe::probe(stateful).has_value() == (type != PieceType::Sniper),
               std::string("stateful K+") + std::string(Position::type_name(type)) +
                 "+K tablebase is probeable unless its check-rule seeds are stale");
    }

    Position devilWithoutStatefulSidecar;
    moved(devilWithoutStatefulSidecar, PieceType::King, Color::White, "a1");
    const int devil = moved(devilWithoutStatefulSidecar, PieceType::Devil,
                            Color::White, "c3");
    moved(devilWithoutStatefulSidecar, PieceType::King, Color::Black, "h10");
    devilWithoutStatefulSidecar.piece(devil).cooldown = 3;
    expect(!TablebaseProbe::probe(devilWithoutStatefulSidecar),
           "minion-free Devil entry projection cannot replace the stateful sidecar");

    Position fabricatedPenguinCooldown;
    moved(fabricatedPenguinCooldown, PieceType::King, Color::White, "a1");
    const int penguin = moved(fabricatedPenguinCooldown, PieceType::Penguin,
                              Color::White, "c3");
    moved(fabricatedPenguinCooldown, PieceType::King, Color::Black, "h10");
    fabricatedPenguinCooldown.piece(penguin).cooldown = 5;
    expect(!TablebaseProbe::probe(fabricatedPenguinCooldown),
           "fabricated Penguin cooldown state is outside the exact table domain");

    Position princeMate;
    std::string princeError;
    expect(princeMate.set_upn(
      "w;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;win=-;"
      "king,w,a1,0,0,0,0,1,1,-1,1,-1,0;"
      "king,b,c1,0,0,0,0,1,1,-1,1,-1,0;"
      "prince,w,e1,0,0,0,0,1,1,-1,1,-1,0",
      &princeError),
      "same-turn Prince tablebase regression parses: " + princeError);
    const auto princeResult = TablebaseProbe::probe(princeMate);
    expect(princeResult && princeResult->wdl == TablebaseWdl::Win &&
             princeResult->dtw == 2,
           "Prince e1-d1 followed by forced d1xc1 is an exact two-action win");
    Search princeSearch(1);
    SearchLimits princeLimits;
    princeLimits.depth = 4;
    const SearchResult princeAnalysis =
      princeSearch.think(princeMate, princeLimits);
    expect(princeAnalysis.principalVariation.size() == 2,
           "tablebase PV follows a same-side continuation through terminal knockout");
    Position princeLine = princeMate;
    for (const Move& move : princeAnalysis.principalVariation) {
        Undo undo;
        expect(princeLine.make_move(move, undo),
               "every reconstructed tablebase action is legal");
    }
    expect(princeLine.game_over() && princeLine.winner() == Color::White,
           "reconstructed tablebase PV stops at its terminal result");

    Position copycat;
    moved(copycat, PieceType::King, Color::White, "a1");
    const int cat = moved(copycat, PieceType::Copycat, Color::White, "c3");
    copycat.piece(copycat.piece(cat).link).moved = true;
    moved(copycat, PieceType::King, Color::Black, "h10");
    expect(TablebaseProbe::probe(copycat).has_value(),
           "linked Copycat mirror-pair tablebase is probeable");

    Position copycatBishop;
    moved(copycatBishop, PieceType::King, Color::White, "a1");
    const int compoundCat = moved(
      copycatBishop, PieceType::Copycat, Color::White, "c3");
    const int compoundClone = copycatBishop.piece(compoundCat).link;
    copycatBishop.piece(compoundClone).moved = true;
    moved(copycatBishop, PieceType::King, Color::Black, "h10");
    moved(copycatBishop, PieceType::Bishop, Color::Black, "d8");
    const auto compoundWhite = TablebaseProbe::probe(copycatBishop);
    expect(compoundWhite.has_value(),
           "linked compound Copycat versus Bishop tablebase is probeable");

    Position copycatBishopSwapped;
    moved(copycatBishopSwapped, PieceType::King, Color::Black, "a1");
    const int swappedCat = moved(
      copycatBishopSwapped, PieceType::Copycat, Color::Black, "c3");
    copycatBishopSwapped.piece(
      copycatBishopSwapped.piece(swappedCat).link).moved = true;
    moved(copycatBishopSwapped, PieceType::King, Color::White, "h10");
    moved(copycatBishopSwapped, PieceType::Bishop, Color::White, "d8");
    copycatBishopSwapped.set_side_to_move(Color::Black);
    const auto compoundBlack = TablebaseProbe::probe(copycatBishopSwapped);
    expect(compoundBlack && compoundWhite &&
             compoundBlack->wdl == compoundWhite->wdl &&
             compoundBlack->dtw == compoundWhite->dtw,
           "compound Copycat tablebase canonicalizes material-owner color");

    Position singletonCopycatBishop = copycatBishop;
    singletonCopycatBishop.piece(compoundCat).link = Position::NoPiece;
    singletonCopycatBishop.piece(compoundClone).link = Position::NoPiece;
    expect(singletonCopycatBishop.remove_piece(compoundClone),
           "Copycat tablebase regression constructs one surviving split half");
    expect(!TablebaseProbe::probe(singletonCopycatBishop),
           "a split Copycat singleton never probes an unsplit linked-pair table");

    copycatBishop.piece(compoundClone).square =
      Position::square_from_name("e3");
    expect(!TablebaseProbe::probe(copycatBishop),
           "independently displaced Copycat halves remain outside the exact domain");

    Position dragonPenguin;
    moved(dragonPenguin, PieceType::King, Color::White, "a1");
    moved(dragonPenguin, PieceType::Dragon, Color::White, "c3");
    moved(dragonPenguin, PieceType::King, Color::Black, "h10");
    moved(dragonPenguin, PieceType::Penguin, Color::Black, "d8");
    expect(TablebaseProbe::probe(dragonPenguin).has_value(),
           "Dragon versus inactive Penguin tablebase is probeable");

    Position twoKnights;
    moved(twoKnights, PieceType::King, Color::White, "a1");
    moved(twoKnights, PieceType::Knight, Color::White, "c3");
    moved(twoKnights, PieceType::Knight, Color::White, "f4");
    moved(twoKnights, PieceType::King, Color::Black, "h10");
    expect(TablebaseProbe::probe(twoKnights).has_value(),
           "horizontally canonical identical-extra K+NN+K tablebase is probeable");

    for (const Color giantColor : {Color::White, Color::Black}) {
        Position giantJester;
        moved(giantJester, PieceType::King, Color::White, "a1");
        moved(giantJester, PieceType::King, Color::Black, "c1");
        moved(giantJester, PieceType::Jester, Color::White, "f1");
        moved(giantJester, PieceType::Giant, giantColor, "g1");
        const auto left = TablebaseProbe::probe(giantJester);

        Position mirroredGiantJester;
        moved(mirroredGiantJester, PieceType::King, Color::White, "h1");
        moved(mirroredGiantJester, PieceType::King, Color::Black, "f1");
        moved(mirroredGiantJester, PieceType::Jester, Color::White, "c1");
        moved(mirroredGiantJester, PieceType::Giant, giantColor, "a1");
        const auto right = TablebaseProbe::probe(mirroredGiantJester);
        expect(left && right && left->wdl == right->wdl &&
                 left->dtw == right->dtw,
               std::string("Giant/Jester tablebase reflects the 2x2 anchor for ") +
                 (giantColor == Color::White ? "owner" : "opponent") +
                 " material");
    }

    Position sameColorBishops;
    moved(sameColorBishops, PieceType::King, Color::White, "a1");
    moved(sameColorBishops, PieceType::Bishop, Color::White, "c3");
    moved(sameColorBishops, PieceType::Bishop, Color::White, "e5");
    moved(sameColorBishops, PieceType::King, Color::Black, "h10");
    const auto sameColor = TablebaseProbe::probe(sameColorBishops);
    expect(sameColor && sameColor->wdl == TablebaseWdl::Draw,
           "same-color Bishop pair is a terminal insufficient-material tablebase draw");

    Position longDragonWin;
    std::string longDragonError;
    expect(longDragonWin.set_upn(
      "b;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;win=-;"
      "king,w,c2,0,0,0,0,1,1,-1,1,-1,0;"
      "king,b,a1,0,0,0,0,1,1,-1,1,-1,0;"
      "bishop,w,h1,0,0,0,0,1,1,-1,1,-1,0;"
      "dragon,b,c1,0,0,0,0,1,1,-1,1,-1,0",
      &longDragonError),
      "long Dragon tablebase regression parses: " + longDragonError);
    const auto longDragonResult = TablebaseProbe::probe(longDragonWin);
    expect(longDragonResult && longDragonResult->wdl == TablebaseWdl::Win &&
             longDragonResult->dtw == 143,
           "Dragon-side root probes as an exact 143-action win");
    Search longDragonSearch(1);
    SearchLimits longDragonLimits;
    longDragonLimits.depth = 16;
    const SearchResult longDragonAnalysis =
      longDragonSearch.think(longDragonWin, longDragonLimits);
    expect(longDragonAnalysis.mateActions && *longDragonAnalysis.mateActions == 143,
           "analysis preserves a long tablebase DTW as an exact action count");
    expect(longDragonAnalysis.completedDepth == 1,
           "an exact root tablebase result stops after selecting its optimal action");
    expect(longDragonAnalysis.principalVariation.size() == 16,
           "a long exact tablebase PV is reconstructed to the requested depth limit");

    queen.piece(1).moved = false;
    const auto displacedPromotion = TablebaseProbe::probe(queen);
    expect(displacedPromotion && white &&
             displacedPromotion->wdl == white->wdl &&
             displacedPromotion->dtw == white->dtw,
           "an unmoved forced-promotion Queen reuses the exact ordinary table");

    Position promotedPawnMarker;
    std::string promotedMarkerError;
    expect(promotedPawnMarker.set_upn(
      "b;hm=1;fm=1;ep=a9;cont=0;forced=-1;epv=3;win=-;"
      "king,w,a1,0,0,0,0,1,1,-1,1,-1,0;"
      "king,b,b1,0,0,0,0,1,1,-1,1,-1,0;"
      "jester,w,c1,0,0,0,0,1,1,-1,1,-1,0;"
      "queen,w,a10,0,0,0,0,1,1,-1,1,-1,0",
      &promotedMarkerError),
      "promoted-Pawn inert-marker regression parses: " + promotedMarkerError);
    Position normalizedPromotion;
    std::string normalizedPromotionError;
    expect(normalizedPromotion.set_upn(
      "b;hm=1;fm=1;ep=-;cont=0;forced=-1;epv=-1;win=-;"
      "king,w,a1,0,0,0,0,1,1,-1,1,-1,0;"
      "king,b,b1,0,0,0,0,1,1,-1,1,-1,0;"
      "jester,w,c1,0,0,0,0,1,1,-1,1,-1,0;"
      "queen,w,a10,0,0,0,0,1,1,-1,1,-1,0",
      &normalizedPromotionError),
      "promoted-Pawn normalized regression parses: " + normalizedPromotionError);
    const auto markerResult = TablebaseProbe::probe(promotedPawnMarker);
    const auto normalizedResult = TablebaseProbe::probe(normalizedPromotion);
    expect(markerResult && normalizedResult &&
             markerResult->wdl == normalizedResult->wdl &&
             markerResult->dtw == normalizedResult->dtw,
           "a promoted two-step Pawn's inert en-passant marker reuses the exact lower table");
}

void test_native_information_set_search() {
    auto ghostFixture = [](int ghostSquare) {
        Position position;
        position.add_piece(PieceType::King, Color::White,
                           Position::square_from_name("a1"));
        position.add_piece(PieceType::Rook, Color::White,
                           Position::square_from_name("b2"));
        position.add_piece(PieceType::Queen, Color::White,
                           Position::square_from_name("d4"));
        position.add_piece(PieceType::King, Color::Black,
                           Position::square_from_name("e10"));
        position.add_piece(PieceType::Pawn, Color::Black,
                           Position::square_from_name("e5"));
        const int ghost = position.add_piece(PieceType::Ghost, Color::Black, ghostSquare);
        position.piece(ghost).visible = false;
        return position;
    };

    Position unsafe = ghostFixture(Position::square_from_name("f6"));
    Position safe = ghostFixture(Position::square_from_name("a9"));
    SearchLimits limits;
    limits.depth = 3;
    Search exact(2);
    const SearchResult safeOnly = exact.think(safe, limits);
    expect(safeOnly.bestMove && safe.move_to_string(*safeOnly.bestMove) == "d4-e5",
           "a concrete safe world takes the exposed pawn with a continuing attack");

    PublicBeliefState retained({Color::White, false});
    std::string beliefError;
    expect(retained.add(unsafe, &beliefError) &&
             retained.add(safe, &beliefError),
           "native information-set fixture forms one public view: " +
             beliefError);
    Search informationSet(2);
    const BeliefSearchResult robust = informationSet.think_beliefs(retained, limits);
    expect(robust.bestMove && *robust.bestMove != "d4-e5",
           "native information-set search rejects a queen move lost to one possible Ghost");
    expect(robust.beliefs == 2 && robust.deepBeliefs == 2 && robust.commonMoves > 1,
           "native information-set result reports complete belief/root coverage");

    const auto drawingCapture = safe.move_from_string("d4-e5");
    SearchLimits repetitionLimits;
    repetitionLimits.depth = 3;
    repetitionLimits.rootMoves = {*drawingCapture};
    repetitionLimits.rootDrawMoveStrings = {"d4-e5"};
    Search repetitionSearch(2);
    const SearchResult repetition = repetitionSearch.think(safe, repetitionLimits);
    expect(repetition.bestMove && safe.move_to_string(*repetition.bestMove) == "d4-e5" &&
             repetition.score == 0,
           "a root completing public threefold repetition scores as a draw in native search");

    Position repeatedChild = safe;
    Undo repeatedUndo;
    expect(repeatedChild.make_move(*drawingCapture, repeatedUndo),
           "history-aware repetition fixture applies its root action");
    SearchLimits historyRepetitionLimits;
    historyRepetitionLimits.depth = 3;
    historyRepetitionLimits.rootMoves = {*drawingCapture};
    historyRepetitionLimits.repetitionHistory = {
      {repeatedChild.key()}, {repeatedChild.key()}, {safe.key()}};
    Search historyRepetitionSearch(2);
    const SearchResult historyRepetition = historyRepetitionSearch.think(
      safe, historyRepetitionLimits);
    expect(historyRepetition.bestMove && historyRepetition.score == 0,
           "native search derives a third-occurrence draw from full history");

    const auto repetitionPosition = [](const char* upn) {
        Position position;
        std::string error;
        if (!position.set_upn(upn, &error))
            throw std::runtime_error("invalid repetition fixture: " + error);
        return position;
    };
    Position inertMoved = repetitionPosition(
      "w;king,w,a1;bishop,w,c3;king,b,h10");
    Position movedBishop = inertMoved;
    movedBishop.piece(1).moved = true;
    expect(inertMoved.key() == movedBishop.key(),
           "position identity ignores a semantically inert Bishop moved bit");
    Position castlingState = repetitionPosition(
      "w;king,w,a1;rook,w,h1;king,b,h10");
    Position lostCastling = castlingState;
    lostCastling.piece(0).moved = true;
    expect(castlingState.key() != lostCastling.key(),
           "position identity retains a moved bit while it controls castling");
    Position pawnState = repetitionPosition(
      "w;king,w,a1;pawn,w,c3;king,b,h10");
    Position movedPawn = pawnState;
    movedPawn.piece(1).moved = true;
    expect(pawnState.key() != movedPawn.key(),
           "position identity retains the Pawn double-step state");

    auto winningFixture = [](int ghostSquare) {
        Position position;
        position.add_piece(PieceType::King, Color::White,
                           Position::square_from_name("a1"));
        position.add_piece(PieceType::Queen, Color::White,
                           Position::square_from_name("h9"));
        position.add_piece(PieceType::King, Color::Black,
                           Position::square_from_name("h10"));
        const int ghost = position.add_piece(PieceType::Ghost, Color::Black, ghostSquare);
        position.piece(ghost).visible = false;
        return position;
    };
    Position adjacentOne = winningFixture(Position::square_from_name("g9"));
    Position adjacentTwo = winningFixture(Position::square_from_name("g10"));
    PublicBeliefState adjacentBeliefs({Color::White, false});
    expect(adjacentBeliefs.add(adjacentOne, &beliefError) &&
             adjacentBeliefs.add(adjacentTwo, &beliefError),
           "forced-win worlds form one exact public belief: " + beliefError);
    Search forcedWin(2);
    const BeliefSearchResult mate = forcedWin.think_beliefs(
      adjacentBeliefs, limits, false);
    expect(mate.bestMove && *mate.bestMove == "h9-h10" && mate.score >= 29900,
           "Ghost uncertainty never hard-filters a high-value move that captures the real king");
}

void test_public_belief_state_core() {
    auto hiddenGhostWorld = [](int ghostSquare) {
        Position position;
        position.add_piece(PieceType::King, Color::White,
                           Position::square_from_name("a1"));
        position.add_piece(PieceType::Rook, Color::White,
                           Position::square_from_name("c1"));
        position.add_piece(PieceType::King, Color::Black,
                           Position::square_from_name("h10"));
        const int ghost = position.add_piece(PieceType::Ghost, Color::Black,
                                             ghostSquare);
        position.piece(ghost).visible = false;
        return position;
    };

    const std::set<std::string> excluded = {
      "a1", "a2", "a3", "b1", "b2", "b3", "c1", "d1", "h10"};
    std::vector<Position> worlds;
    for (int square = 0; square < Position::BoardSquares; ++square)
        if (!excluded.count(Position::square_name(square)))
            worlds.push_back(hiddenGhostWorld(square));
    expect(worlds.size() > 64,
           "uncapped belief regression contains more than 64 concrete worlds");

    PublicBeliefState forward({Color::White, false});
    PublicBeliefState reverse({Color::White, false});
    std::string error;
    for (const Position& world : worlds)
        expect(forward.add(world, &error),
               "forward hidden-Ghost world is one public view: " + error);
    for (auto iterator = worlds.rbegin(); iterator != worlds.rend(); ++iterator)
        expect(reverse.add(*iterator, &error),
               "reverse hidden-Ghost world is one public view: " + error);
    expect(forward.size() == worlds.size() && reverse.size() == worlds.size(),
           "public belief retains every world without a 64-world cap");
    const std::vector<Position> forwardPositions = forward.positions();
    const std::vector<Position> reversePositions = reverse.positions();
    expect(forwardPositions.size() == reversePositions.size() &&
             std::equal(forwardPositions.begin(), forwardPositions.end(),
               reversePositions.begin(), reversePositions.end(),
               [](const Position& first, const Position& second) {
                   return first.upn() == second.upn();
               }),
           "public belief canonical order is insertion-permutation invariant");
    SearchLimits benchmarkLimits;
    benchmarkLimits.depth = 1;
    const std::vector<BeliefDecisionBucket> forwardCells =
      forward.decision_cells();
    const std::vector<BeliefDecisionBucket> reverseCells =
      reverse.decision_cells();
    std::size_t cellWorlds = 0;
    for (const BeliefDecisionBucket& cell : forwardCells)
        cellWorlds += cell.worlds.size();
    expect(forwardCells.size() == 1 &&
             forwardCells.size() == reverseCells.size() &&
             cellWorlds == worlds.size() &&
             std::equal(forwardCells.begin(), forwardCells.end(),
               reverseCells.begin(),
               [](const BeliefDecisionBucket& first,
                  const BeliefDecisionBucket& second) {
                   if (first.observation != second.observation ||
                       first.worlds.size() != second.worlds.size())
                       return false;
                   return std::equal(first.worlds.begin(), first.worlds.end(),
                     second.worlds.begin(),
                     [](const Position& left, const Position& right) {
                         return left.upn() == right.upn();
                     });
               }),
           "hidden enemy Ghost coordinates do not leak through legal dots and decision cells conserve worlds");
    const auto renderedMarkers = [](const Position& position) {
        std::vector<std::string> markers;
        for (const Move& move : position.legal_moves())
            markers.push_back(move.kind == MoveKind::Pass
              ? "pass"
              : Position::square_name(move.from) + ">" +
                Position::square_name(move.to));
        return markers;
    };
    PublicBeliefState observed = forward;
    const std::size_t observedBefore = observed.size();
    std::vector<std::string> firstMarkers = renderedMarkers(
      forwardCells.front().worlds.front());
    if (!firstMarkers.empty())
        firstMarkers.push_back(firstMarkers.front());
    expect(observed.condition_on_decision_markers(firstMarkers, &error) &&
             observed.size() == forwardCells.front().worlds.size() &&
             observed.decision_partitions() == 1,
           "rendered legal-dot markers select one exact cell without sampling: " +
             error);
    const std::size_t observedAfter = observed.size();
    expect(!observed.condition_on_decision_markers({"h1>h2"}, &error) &&
             observed.size() == observedAfter && observedBefore >= observedAfter,
           "unknown legal-dot markers fail without mutating retained worlds");
    Search mergedSearch(1);
    const BeliefSearchResult mergedResult =
      mergedSearch.think_beliefs(forward, benchmarkLimits);
    expect(mergedResult.validInformationCell && mergedResult.bestMove &&
             mergedResult.beliefs == worlds.size(),
           "exact search accepts the unified legal-dot cell for hidden enemy Ghost coordinates");
    Search mergedConservative(1);
    const BeliefSearchResult mergedConservativeResult =
      mergedConservative.think_beliefs(
        forward, benchmarkLimits, false);
    expect(mergedConservativeResult.validInformationCell &&
             mergedConservativeResult.bestMove &&
             mergedConservativeResult.beliefs == worlds.size() &&
             mergedConservativeResult.historyPreservingPlies == 1,
           "no-dot search retains all decision cells and chooses a common action");
    for (const BeliefDecisionBucket& bucket : forwardCells) {
        PublicBeliefState cell({Color::White, false});
        for (const Position& world : bucket.worlds)
            expect(cell.add(world, &error),
                   "decision-cell world retains its public view: " + error);
        Search cellSearch(1);
        const BeliefSearchResult cellResult =
          cellSearch.think_beliefs(cell, benchmarkLimits);
        expect(cell.decision_partitions() == 1 &&
                 cellResult.validInformationCell &&
                 cellResult.historyPreservingPlies == 1,
               "each exact private-dot cell searches with preserved history");
    }
    PublicBeliefState drawCell({Color::White, false});
    for (const Position& world : forwardCells.front().worlds)
        expect(drawCell.add(world, &error),
               "public repetition fixture retains its decision cell: " + error);
    const std::string drawingAction = drawCell.common_moves().front();
    const auto drawingMove = drawCell.concrete_worlds().begin()->second
                               .move_from_string(drawingAction);
    expect(bool(drawingMove), "public repetition action parses in its cell");
    SearchLimits publicRepetitionLimits;
    publicRepetitionLimits.depth = 2;
    publicRepetitionLimits.rootMoves = {*drawingMove};
    publicRepetitionLimits.rootDrawMoveStrings = {drawingAction};
    Search publicRepetition(1);
    const BeliefSearchResult publicDraw = publicRepetition.think_beliefs(
      drawCell, publicRepetitionLimits);
    expect(publicDraw.bestMove && *publicDraw.bestMove == drawingAction &&
             publicDraw.score == 0,
           "public threefold result remains a draw in history-preserving search");
    const std::size_t beforeDuplicate = forward.size();
    expect(forward.add(worlds.front(), &error) &&
             forward.size() == beforeDuplicate,
           "exact duplicate beliefs are deduplicated without hashing loss");

    Position visible = worlds.front();
    for (int id = 0; id < visible.piece_count(); ++id)
        if (visible.piece(id).type == PieceType::Ghost)
            visible.piece(id).visible = true;
    expect(!forward.add(visible, &error) &&
             error.find("public view differs") != std::string::npos,
           "distinguishable Ghost visibility cannot enter one belief state");
    expect(!forward.set_disclosure({Color::Black, false}, &error),
           "belief disclosure cannot change underneath retained worlds");

    const std::vector<std::string> common = reverse.common_moves();
    const std::set<std::string> commonSet(common.begin(), common.end());
    bool everyMoveIsCommon = true;
    for (const Position& world : worlds) {
        for (const Move& move : world.legal_moves()) {
            const std::string text = world.move_to_string(move);
            if (!commonSet.count(text)) {
                everyMoveIsCommon = false;
                break;
            }
        }
        if (!everyMoveIsCommon)
            break;
    }
    expect(everyMoveIsCommon,
           "ordinary apparent moves remain legal in every hidden-enemy-Ghost world");
    const std::vector<std::string> safePrefix =
      reverse.observation_safe_prefix({"c1-d1", "h10-g10", "d1-e1"});
    expect(safePrefix ==
             std::vector<std::string>({"c1-d1", "h10-g10", "d1-e1"}),
           "belief PV follows an observable opponent reply after conditioning incompatible worlds");
    expect(reverse.observation_safe_prefix({"h10-g10"}).empty(),
           "belief PV never exposes a root action that is not common to every world");

    Position blockedOpponent = hiddenGhostWorld(
      Position::square_from_name("a7"));
    Position openOpponent = hiddenGhostWorld(
      Position::square_from_name("f8"));
    for (Position* world : {&blockedOpponent, &openOpponent}) {
        world->add_piece(PieceType::Pawn, Color::Black,
                         Position::square_from_name("a9"));
        world->set_side_to_move(Color::Black);
    }
    PublicBeliefState opponentLine({Color::White, false});
    expect(opponentLine.add(blockedOpponent, &error) &&
             opponentLine.add(openOpponent, &error),
           "opponent-line worlds retain one public hidden-Ghost view: " + error);
    expect(opponentLine.observation_safe_prefix({"a9-a7"}) ==
             std::vector<std::string>{"a9-a7"},
           "a visible opponent action may condition worlds where its destination was blocked");
    expect(opponentLine.observation_safe_prefix({"f8-g8"}).empty(),
           "an opponent PV never exposes a hidden Ghost source coordinate");
    const std::vector<std::string> projectedGhost =
      opponentLine.observation_safe_prefix({"a7-a6"}, &openOpponent);
    expect(projectedGhost.size() == 1 &&
             projectedGhost.front().rfind("f8-", 0) == 0,
           "a hidden opponent action projects onto an observation-equivalent move in the actual world");

    const std::size_t retained = forward.size();
    const BeliefTransitionResult applied = forward.apply_known("c1-d1", &error);
    expect(applied.applied && applied.before == retained &&
             applied.after == retained && applied.observations == 1 &&
             forward.size() == retained,
           "uniform known action preserves the complete hidden-Ghost set");

    std::vector<Position> blackToMoveWorlds;
    for (const Position& source : worlds) {
        Position world = source;
        const int blackRook = world.add_piece(
          PieceType::Rook, Color::Black,
          Position::square_from_name("f10"));
        if (blackRook == Position::NoPiece ||
            world.piece_on(Position::square_from_name("e10")) !=
              Position::NoPiece)
            continue;
        world.set_side_to_move(Color::Black);
        blackToMoveWorlds.push_back(std::move(world));
    }
    PublicBeliefState blackToMove({Color::White, false});
    for (const Position& world : blackToMoveWorlds)
        expect(blackToMove.add(world, &error),
               "Black-to-move hidden-Ghost world is one public view: " +
                 error);
    const std::string splitAction = "f10-e10";
    const BeliefSuccessorPartitions exactPartitions =
      blackToMove.successor_partitions(splitAction);
    expect(exactPartitions.before == blackToMove.size() &&
             exactPartitions.incompatible == 0 &&
             exactPartitions.buckets.size() == 1,
           "opponent Ghost coordinates do not split the observer's successor legal dots");
    const BeliefSuccessorPartitions conservativePartitions =
      blackToMove.successor_partitions(splitAction, false);
    expect(conservativePartitions.before == blackToMove.size() &&
             conservativePartitions.incompatible == 0 &&
             conservativePartitions.buckets.size() == exactPartitions.buckets.size(),
           "exact and conservative successor observations agree without a legal-dot leak");
    const BeliefSuccessorPartitions adversarialPartitions =
      blackToMove.adversarial_successor_partitions(false);
    expect(adversarialPartitions.before == blackToMove.size() &&
             adversarialPartitions.incompatible == 0 &&
             std::any_of(adversarialPartitions.buckets.begin(),
                         adversarialPartitions.buckets.end(),
                         [](const BeliefSuccessorBucket& bucket) {
                             return bucket.actions.size() > 1 &&
                                    bucket.worlds.size() > 1;
                         }),
           "indistinguishable opponent actions share one observation bucket");
    PublicBeliefState appliedBlackToMove = blackToMove;
    const std::size_t decisionBefore = appliedBlackToMove.size();
    const BeliefTransitionResult decisionResult = appliedBlackToMove.apply_known(
      splitAction, &error);
    expect(decisionResult.applied && decisionResult.before == decisionBefore &&
             decisionResult.after == decisionBefore &&
             decisionResult.observations == 1 &&
             appliedBlackToMove.size() == decisionBefore,
           "known successor retains every world when hidden Ghost squares do not alter legal dots");

    SearchLimits splitLimits;
    splitLimits.depth = 2;
    const auto forcedSplit = blackToMoveWorlds.front().move_from_string(splitAction);
    expect(bool(forcedSplit), "private-dot split action parses at the belief root");
    splitLimits.rootMoves = {*forcedSplit};
    Search exactHistory(1);
    const BeliefSearchResult exactHistoryResult = exactHistory.think_beliefs(
      blackToMove, splitLimits, true);
    Search conservativeHistory(1);
    const BeliefSearchResult conservativeHistoryResult =
      conservativeHistory.think_beliefs(
        blackToMove, splitLimits, false);
    expect(exactHistoryResult.bestMove &&
             *exactHistoryResult.bestMove == splitAction &&
             exactHistoryResult.historyPreservingPlies == 2 &&
             conservativeHistoryResult.bestMove &&
             *conservativeHistoryResult.bestMove == splitAction &&
             conservativeHistoryResult.historyPreservingPlies == 2,
           "two-ply belief search propagates exact and no-dot histories");

    Position kingTarget;
    kingTarget.add_piece(PieceType::King, Color::White,
                         Position::square_from_name("c3"));
    kingTarget.add_piece(PieceType::Jester, Color::White,
                         Position::square_from_name("d3"));
    kingTarget.add_piece(PieceType::King, Color::Black,
                         Position::square_from_name("h10"));
    kingTarget.add_piece(PieceType::Rook, Color::Black,
                         Position::square_from_name("c10"));
    kingTarget.set_side_to_move(Color::Black);
    Position jesterTarget;
    jesterTarget.add_piece(PieceType::Jester, Color::White,
                           Position::square_from_name("c3"));
    jesterTarget.add_piece(PieceType::King, Color::White,
                           Position::square_from_name("d3"));
    jesterTarget.add_piece(PieceType::King, Color::Black,
                           Position::square_from_name("h10"));
    jesterTarget.add_piece(PieceType::Rook, Color::Black,
                           Position::square_from_name("c10"));
    jesterTarget.set_side_to_move(Color::Black);
    PublicBeliefState royal({Color::Black, false});
    expect(royal.add(kingTarget, &error) && royal.add(jesterTarget, &error),
           "royal silhouettes form one ordinary public belief: " + error);
    const BeliefTransitionResult split = royal.apply_known("c10-c3", &error);
    expect(!split.applied && split.before == 2 && split.after == 0 &&
             split.observations == 2 && royal.size() == 2,
           "distinguishable terminal/continuing outcomes never merge histories");
}

void test_public_history_reconstruction() {
    const auto parse = [](std::string_view upn) {
        Position result;
        std::string error;
        expect(result.set_upn(upn, &error),
               "public-history fixture parses: " + error);
        return result;
    };
    const auto hiddenGhost = [](std::string_view side,
                                std::string_view square) {
        Position result;
        std::string error;
        const std::string upn = std::string(side) +
          ";king,w,a1;ghost,w," + std::string(square) +
          ",0,0,0,0,0,0,-1,1,-1,0;king,b,h10";
        expect(result.set_upn(upn, &error),
               "hidden-Ghost history fixture parses: " + error);
        return result;
    };
    PublicHistoryState fromC3;
    PublicHistoryState fromF3;
    std::string error;
    expect(fromC3.start(hiddenGhost("b", "c3"), {Color::Black, false},
                        &error),
           "public history accepts a private authoritative Ghost start: " +
             error);
    expect(fromF3.start(hiddenGhost("b", "f3"), {Color::Black, false},
                        &error),
           "public history accepts an indistinguishable Ghost start: " +
             error);
    const std::vector<Position> c3Worlds = fromC3.beliefs().positions();
    const std::vector<Position> f3Worlds = fromF3.beliefs().positions();
    expect(c3Worlds.size() == 75 && c3Worlds.size() == f3Worlds.size() &&
             std::equal(c3Worlds.begin(), c3Worlds.end(), f3Worlds.begin(),
               [](const Position& left, const Position& right) {
                   return left.upn() == right.upn();
               }),
           "initial history forgets the leaked Ghost coordinate and retains "
           "every publicly possible board world");
    expect(fromC3.beliefs().decision_partitions() == 1,
           "the mover's initial legal dots select one exact information cell");
    expect(!fromC3.beliefs().piece_location_known(1) &&
             fromC3.beliefs().piece_location_candidates(1).size() == 75,
           "per-piece knowledge reports every still-possible hidden Ghost square");
    PublicHistoryState deploymentRoot;
    expect(deploymentRoot.start(
             hiddenGhost("b", "c3"), {Color::Black, false}, true, &error) &&
             deploymentRoot.beliefs().size() == 23,
           "a known freshly deployed root limits hidden Ghosts to their home "
           "zone: " + error);

    Position twoGhosts;
    twoGhosts.add_piece(PieceType::King, Color::White,
                        Position::square_from_name("a1"));
    const int firstGhost = twoGhosts.add_piece(
      PieceType::Ghost, Color::White, Position::square_from_name("c2"));
    const int secondGhost = twoGhosts.add_piece(
      PieceType::Ghost, Color::White, Position::square_from_name("d2"));
    twoGhosts.piece(firstGhost).visible = false;
    twoGhosts.piece(secondGhost).visible = false;
    twoGhosts.add_piece(PieceType::King, Color::Black,
                        Position::square_from_name("h10"));
    twoGhosts.set_side_to_move(Color::Black);
    PublicHistoryState interchangeableGhosts;
    expect(interchangeableGhosts.start(
             twoGhosts, {Color::Black, false}, &error) &&
             interchangeableGhosts.beliefs().size() == 2775,
           "two identical hidden Ghosts use all board-cell combinations "
           "without duplicate identity permutations: " + error);
    twoGhosts.piece(secondGhost).action = 1;
    PublicHistoryState statefulGhosts;
    expect(statefulGhosts.start(
             twoGhosts, {Color::Black, false}, &error) &&
             statefulGhosts.beliefs().size() == 5550,
           "distinguishable hidden Ghost state retains both square "
           "assignments: " + error);

    PublicHistoryState afterHiddenMove;
    expect(afterHiddenMove.start(
             hiddenGhost("w", "c3"), {Color::Black, false}, &error) &&
             afterHiddenMove.apply_actual("c3-d3", &error),
           "public history replays an invisible enemy Ghost action: " + error);
    expect(afterHiddenMove.actual_position().piece_on(
             Position::square_from_name("d3")) != Position::NoPiece &&
             afterHiddenMove.beliefs().size() > 1 &&
             afterHiddenMove.beliefs().decision_partitions() == 1,
           "an invisible Ghost action advances the private cursor without "
           "collapsing the observer's information set");
    expect(afterHiddenMove.repetition_history().size() == 2 &&
             !afterHiddenMove.repetition_history().front().empty() &&
             !afterHiddenMove.repetition_history().back().empty(),
           "public history records every observed position set for native repetition search");

    PublicHistoryState preparedHiddenMove;
    PublicHistoryState directHiddenMove;
    expect(preparedHiddenMove.start(
             hiddenGhost("w", "c3"), {Color::Black, false}, &error) &&
             directHiddenMove.start(
               hiddenGhost("w", "c3"), {Color::Black, false}, &error) &&
             preparedHiddenMove.prepare_opponent_transition(&error) &&
             preparedHiddenMove.apply_actual("c3-d3", &error) &&
             directHiddenMove.apply_actual("c3-d3", &error),
           "an idle-prepared hidden transition applies successfully: " + error);
    const std::vector<Position> preparedWorlds =
      preparedHiddenMove.beliefs().positions();
    const std::vector<Position> directWorlds =
      directHiddenMove.beliefs().positions();
    expect(preparedWorlds.size() == directWorlds.size() &&
             std::equal(preparedWorlds.begin(), preparedWorlds.end(),
               directWorlds.begin(), [](const Position& left,
                                        const Position& right) {
                   return left.upn() == right.upn();
               }),
           "idle preparation preserves the exact observed successor belief");

    const Position visibleRoyalMove = parse(
      "w;king,w,a1;jester,w,b1;king,b,h10");
    PublicHistoryState preparedVisibleMove;
    PublicHistoryState directVisibleMove;
    expect(preparedVisibleMove.start(
             visibleRoyalMove, {Color::Black, false}, &error) &&
             directVisibleMove.start(
               visibleRoyalMove, {Color::Black, false}, &error) &&
             preparedVisibleMove.prepare_opponent_transition(&error) &&
             preparedVisibleMove.apply_actual("a1-a2", &error) &&
             directVisibleMove.apply_actual("a1-a2", &error),
           "an observation-filtered visible transition applies: " + error);
    const std::vector<Position> preparedVisibleWorlds =
      preparedVisibleMove.beliefs().positions();
    const std::vector<Position> directVisibleWorlds =
      directVisibleMove.beliefs().positions();
    expect(preparedVisibleWorlds.size() == directVisibleWorlds.size() &&
             std::equal(
               preparedVisibleWorlds.begin(), preparedVisibleWorlds.end(),
               directVisibleWorlds.begin(),
               [](const Position& left, const Position& right) {
                   return left.upn() == right.upn();
               }),
           "visible action filtering retains the exact full-partition belief");

    Position visibleGhost = hiddenGhost("w", "c3");
    visibleGhost.piece(1).visible = true;
    PublicBeliefState knownGhost({Color::Black, false});
    expect(knownGhost.add(visibleGhost, &error) && knownGhost.size() == 1,
           "a currently visible enemy Ghost begins as one concrete world: " +
             error);
    SearchLimits reexpansionLimits;
    reexpansionLimits.depth = 4;
    Search reexpansionSearch(1);
    const BeliefSearchResult reexpansion = reexpansionSearch.think_beliefs(
      knownGhost, reexpansionLimits);
    expect(reexpansion.singletonHandoffs == 0 &&
             reexpansion.peakBeliefs > 1,
           "singleton Ghost knowledge remains in exact belief search and "
           "re-expands only after a later move begun while hidden");

    const Position ambiguousRoyal = parse(
      "b;king,w,a1;jester,w,b1;king,b,h10");
    PublicHistoryState concealedRoyal;
    PublicHistoryState revealedRoyal;
    expect(concealedRoyal.start(
             ambiguousRoyal, {Color::Black, false}, &error) &&
             concealedRoyal.beliefs().size() == 2 &&
             !concealedRoyal.beliefs().enemy_king_known(),
           "pre-reveal history branches over enemy King/Jester identity: " +
             error);
    expect(revealedRoyal.start(
             ambiguousRoyal, {Color::Black, true}, &error) &&
             revealedRoyal.beliefs().size() == 1 &&
             revealedRoyal.beliefs().enemy_king_known(),
           "post-first-pick history preserves the now-public enemy King "
           "identity: " + error);

    // Parasite possession changes team, not object identity.  The former
    // owner therefore keeps knowing that its old royal was the Jester, while
    // the possessing side learns the same fact from the public continuation:
    // the alternative world where the target was the real King terminated.
    const Position parasiteJester = parse(
      "w;king,w,a1;parasite,w,c3;king,b,h10;jester,b,d4");
    PublicHistoryState parasiteJesterForIvory;
    PublicHistoryState parasiteJesterForOnyx;
    expect(parasiteJesterForIvory.start(
             parasiteJester, {Color::White, false}, &error) &&
             parasiteJesterForIvory.beliefs().size() == 2 &&
             !parasiteJesterForIvory.beliefs().piece_type_known(
               3, PieceType::Jester),
           "the possessing side initially sees two enemy royal assignments: " +
             error);
    expect(parasiteJesterForOnyx.start(
             parasiteJester, {Color::Black, false}, &error) &&
             parasiteJesterForOnyx.beliefs().size() == 1 &&
             parasiteJesterForOnyx.beliefs().piece_type_known(
               3, PieceType::Jester),
           "a royal's original owner knows its Jester identity before "
           "possession: " + error);
    expect(parasiteJesterForIvory.apply_actual("c3-d4", &error) &&
             parasiteJesterForIvory.beliefs().size() == 1 &&
             parasiteJesterForIvory.beliefs().piece_type_known(
               3, PieceType::Jester) &&
             parasiteJesterForIvory.actual_position().piece(3).color ==
               Color::White,
           "continuing after Parasite possession proves the transferred "
           "royal was the Jester: " + error);
    expect(parasiteJesterForOnyx.apply_actual("c3-d4", &error) &&
             parasiteJesterForOnyx.beliefs().size() == 1 &&
             parasiteJesterForOnyx.beliefs().piece_type_known(
               3, PieceType::Jester) &&
             parasiteJesterForOnyx.actual_position().piece(3).color ==
               Color::White,
           "Parasite possession preserves the former owner's exact Jester "
           "provenance after control changes: " + error);
    SearchLimits royalOracleLimits;
    royalOracleLimits.depth = 6;
    royalOracleLimits.factoredBeliefs = false;
    Search royalOracleSearch(2);
    const BeliefSearchResult royalOracle = royalOracleSearch.think_beliefs(
      concealedRoyal.beliefs(), royalOracleLimits);
    royalOracleLimits.factoredBeliefs = true;
    Search royalFactoredSearch(2);
    const BeliefSearchResult royalFactored = royalFactoredSearch.think_beliefs(
      concealedRoyal.beliefs(), royalOracleLimits);
    expect(royalFactored.score == royalOracle.score &&
             royalFactored.nodes < royalOracle.nodes,
           "lazy royal candidate search matches its enumerated oracle while "
           "visiting fewer nodes");

    PublicHistoryState combinedHiddenState;
    expect(combinedHiddenState.start(parse(
             "b;king,w,a1;jester,w,b1;"
             "ghost,w,c3,0,0,0,0,0,0,-1,1,-1,0;king,b,h10"),
             {Color::Black, false}, &error) &&
             combinedHiddenState.beliefs().size() == 148,
           "combined royal/Ghost fixture retains its exact correlated domain: " +
             error);
    SearchLimits combinedOracleLimits;
    combinedOracleLimits.depth = 2;
    combinedOracleLimits.factoredBeliefs = false;
    Search combinedOracleSearch(2);
    const BeliefSearchResult combinedOracle =
      combinedOracleSearch.think_beliefs(
        combinedHiddenState.beliefs(), combinedOracleLimits);
    combinedOracleLimits.factoredBeliefs = true;
    Search combinedFactoredSearch(2);
    const BeliefSearchResult combinedFactored =
      combinedFactoredSearch.think_beliefs(
        combinedHiddenState.beliefs(), combinedOracleLimits);
    expect(combinedFactored.score == combinedOracle.score &&
             combinedFactored.bestMove == combinedOracle.bestMove,
           "joint Ghost/royal tuples preserve oracle score and root policy");

    PublicHistoryState mixedGhostMaterial;
    expect(mixedGhostMaterial.start(parse(
             "b;king,w,a1;rook,w,c1;bishop,w,e3;"
             "ghost,w,c2,0,0,0,0,0,0,-1,1,-1,0;"
             "ghost,w,d2,0,0,0,0,0,0,-1,1,-1,0;"
             "king,b,h10;rook,b,h8"),
             {Color::Black, false}, &error) &&
             mixedGhostMaterial.beliefs().size() > 1,
           "arbitrary-material correlated Ghost fixture reconstructs: " + error);
    SearchLimits mixedOracleLimits;
    mixedOracleLimits.depth = 1;
    mixedOracleLimits.factoredBeliefs = false;
    Search mixedOracleSearch(2);
    const BeliefSearchResult mixedOracle = mixedOracleSearch.think_beliefs(
      mixedGhostMaterial.beliefs(), mixedOracleLimits);
    mixedOracleLimits.factoredBeliefs = true;
    Search mixedFactoredSearch(2);
    const BeliefSearchResult mixedFactored = mixedFactoredSearch.think_beliefs(
      mixedGhostMaterial.beliefs(), mixedOracleLimits);
    expect(mixedFactored.searchPath == "correlated-tuples" &&
             mixedFactored.score == mixedOracle.score &&
             mixedFactored.bestMove == mixedOracle.bestMove,
           "general correlated tuples match arbitrary-material oracle policy");

    PublicHistoryState tacticalGhostMaterial;
    expect(tacticalGhostMaterial.start(parse(
             "w;king,w,a1;rook,w,b2;queen,w,d4;king,b,e10;rook,b,h8;"
             "pawn,b,e5;ghost,b,a9,0,0,0,0,0,0,-1,1,-1,0"),
             {Color::White, false}, &error) &&
             tacticalGhostMaterial.beliefs().size() == 72,
           "mixed tactical Ghost fixture reconstructs its exact domain: " +
             error);
    SearchLimits tacticalOracleLimits;
    tacticalOracleLimits.depth = 3;
    tacticalOracleLimits.factoredBeliefs = false;
    Search tacticalOracleSearch(16);
    const BeliefSearchResult tacticalOracle =
      tacticalOracleSearch.think_beliefs(
        tacticalGhostMaterial.beliefs(), tacticalOracleLimits);
    tacticalOracleLimits.factoredBeliefs = true;
    Search tacticalFactoredSearch(16);
    const BeliefSearchResult tacticalFactored =
      tacticalFactoredSearch.think_beliefs(
        tacticalGhostMaterial.beliefs(), tacticalOracleLimits);
    expect(tacticalOracle.score == tacticalFactored.score &&
             tacticalOracle.bestMove == tacticalFactored.bestMove,
           "Ghost belief TT ordering cannot change the unrestricted exact "
           "root result");

    const Position chronologicalRoyals = parse(
      "b;king,w,a1;jester,w,b1;jester,w,c1;king,b,h10");
    PublicHistoryState firstGroupRoyal;
    expect(firstGroupRoyal.start(
             chronologicalRoyals, {Color::Black, false}, false,
             {Position::square_from_name("a1"),
              Position::square_from_name("b1")}, &error) &&
             firstGroupRoyal.beliefs().size() == 2 &&
             firstGroupRoyal.beliefs().enemy_king_candidate_squares() ==
               std::vector<int>{Position::square_from_name("a1"),
                                Position::square_from_name("b1")} &&
             !firstGroupRoyal.beliefs().enemy_king_known(),
           "only first-pick royal silhouettes remain King candidates while "
           "a later Jester is public: " + error);
    expect(!firstGroupRoyal.beliefs().piece_type_known(1, PieceType::Jester) &&
             firstGroupRoyal.beliefs().piece_type_known(2, PieceType::Jester),
           "per-piece knowledge distinguishes an ambiguous first-group Jester "
           "from a known later-group Jester");
    PublicHistoryState oneFirstGroupRoyal;
    expect(oneFirstGroupRoyal.start(
             chronologicalRoyals, {Color::Black, false}, false,
             {Position::square_from_name("a1")}, &error) &&
             oneFirstGroupRoyal.beliefs().size() == 1 &&
             oneFirstGroupRoyal.beliefs().enemy_king_known(),
           "a singleton first-group candidate discloses the King: " + error);
    PublicHistoryState impossibleRoyalChronology;
    expect(!impossibleRoyalChronology.start(
             chronologicalRoyals, {Color::Black, false}, false,
             {Position::square_from_name("b1"),
              Position::square_from_name("c1")}, &error),
           "the authoritative King must remain inside the supplied public "
           "candidate set");
    PublicHistoryState contradictoryRoyalDisclosure;
    expect(!contradictoryRoyalDisclosure.start(
             chronologicalRoyals, {Color::Black, true}, false,
             {Position::square_from_name("a1"),
              Position::square_from_name("b1")}, &error),
           "known-King disclosure cannot retain multiple royal candidates");

    PublicHistoryState midgameSnapshot;
    expect(midgameSnapshot.start(parse(
             "b;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;"
             "king,w,d8,0,0,0,0,0,1,-1,1,-1,0;"
             "king,b,d10,0,0,0,0,0,1,-1,1,-1,0;"
             "ghost,w,b10,0,0,0,0,0,0,-1,1,-1,0"),
             {Color::Black, false}, &error) &&
             midgameSnapshot.beliefs().size() == 73 &&
             midgameSnapshot.beliefs().common_moves().size() == 2,
           "a history root outside the deployment zone is conditioned by its "
           "two known legal moves: " + error);
    SearchLimits historyLimits;
    historyLimits.depth = 1;
    Search historySearch(1);
    const BeliefSearchResult historyChoice = historySearch.think_beliefs(
      midgameSnapshot.beliefs(), historyLimits);
    expect(historyChoice.bestMove &&
             (*historyChoice.bestMove == "d10-c10" ||
              *historyChoice.bestMove == "d10-e10") &&
             midgameSnapshot.apply_actual(*historyChoice.bestMove, &error),
           "every displayed legal King move remains a searchable and "
           "replayable belief action: " + error);
}

void test_belief_terminal_and_observer_scoring() {
    const std::string matingUpn =
      "b;hm=3;fm=2;ep=-;cont=0;forced=-1;epv=-1;"
      "king,w,a1,0,0,0,0,1,1,-1,1,-1,0;"
      "jester,w,h1,0,0,0,0,0,1,-1,1,-1,0;"
      "rook,w,d1,0,0,0,0,0,1,-1,1,-1,0;"
      "rook,w,d8,0,0,0,0,1,1,-1,1,-1,0;"
      "ninja,w,b2,0,0,0,0,0,1,-1,1,-1,0;"
      "ghost,w,g2,0,0,0,0,0,0,-1,1,-1,0;"
      "rook,w,e1,0,0,0,0,0,1,-1,1,-1,0;"
      "pawn,w,f2,0,0,0,0,0,1,-1,1,-1,0;"
      "king,b,d10,0,0,0,0,0,1,-1,1,-1,0;"
      "giant,b,e9,0,0,0,0,0,1,-1,1,-1,0;"
      "giant,b,b9,0,0,0,0,0,1,-1,1,-1,0;"
      "prince,b,g10,0,0,0,0,0,1,-1,1,-1,0;"
      "prince,b,a10,0,0,0,0,0,1,-1,1,-1,0;"
      "checker,b,h10,0,0,0,0,0,1,-1,1,-1,0;"
      "giant,b,g8,0,0,0,0,0,1,-1,1,-1,0;"
      "prince,b,a9,0,0,0,0,0,1,-1,1,-1,0;"
      "knight,b,e8,0,0,0,0,0,1,-1,1,-1,0;"
      "checker,b,c8,0,0,0,0,0,1,-1,1,-1,0;"
      "checker,b,f8,0,0,0,0,0,1,-1,1,-1,0";
    Position mating;
    std::string error;
    expect(mating.set_upn(matingUpn, &error) && mating.game_over() &&
             mating.winner() == Color::White && mating.legal_moves().empty(),
           "reported drafted mating position is a terminal Ivory win: " + error);

    PublicHistoryState hiddenHistory;
    expect(hiddenHistory.start(
             mating, {Color::Black, false}, false,
             {Position::square_from_name("a1"),
              Position::square_from_name("h1")}, &error) &&
             hiddenHistory.beliefs().size() > 1,
           "reported mate reconstructs a nontrivial Ghost/royal belief: " + error);
    SearchLimits limits;
    limits.depth = 1;
    Search hiddenSearch(1);
    const BeliefSearchResult hiddenMate = hiddenSearch.think_beliefs(
      hiddenHistory.beliefs(), limits);
    expect(!hiddenMate.bestMove && hiddenMate.score <= -29900,
           "every terminal world scores as mate, never as a centipawn leaf");

    PublicBeliefState singleton({Color::White, true});
    expect(singleton.add(mating, &error),
           "singleton observer-perspective fixture forms a belief: " + error);
    Search singletonSearch(1);
    const BeliefSearchResult observerMate = singletonSearch.think_beliefs(
      singleton, limits);
    expect(observerMate.score >= 29900,
           "singleton belief scores and mate distance are converted from the "
           "side to move to the configured observer");
}

void test_turn_start_check() {
    const auto parse = [](const char* upn) {
        Position position;
        std::string error;
        expect(position.set_upn(upn, &error), "turn-start fixture: " + error);
        return position;
    };
    Position mate = parse(
      "b;king,w,b8;king,b,a10;devil,w,a1;minion,w,a9;minion,w,b9");
    const auto original = mate.upn();
    const auto originalKey = mate.key();
    expect(mate.in_check() && mate.legal_moves().empty() &&
             mate.terminal_reason() == TerminalReason::Checkmate &&
             mate.winner() == Color::White,
           "automatic Minion advance is checkmate, not stalemate");
    expect(!TablebaseProbe::probe(mate),
           "stale Devil/Minion tables cannot override turn-start checkmate");
    SearchLimits limits;
    limits.depth = 2;
    limits.useTablebases = false;
    Search search(1);
    const auto result = search.think(mate, limits);
    expect(!result.bestMove && result.score <= -29900,
           "concrete search scores automatic Minion mate without tablebases");
    PublicBeliefState belief({Color::White, true});
    std::string error;
    expect(belief.add(mate, &error), "Minion mate belief: " + error);
    Search beliefSearch(1);
    const auto beliefResult = beliefSearch.think_beliefs(belief, limits);
    expect(!beliefResult.bestMove && beliefResult.score >= 29900,
           "belief search scores Minion mate from the observer perspective");
    expect(mate.upn() == original && mate.key() == originalKey,
           "check/terminal/search queries do not mutate Minions or position state");

    Position mirrored = parse(
      "w;king,b,g3;king,w,h1;devil,b,h10;minion,b,h2;minion,b,g2");
    expect(mirrored.in_check() && mirrored.winner() == Color::Black &&
             mirrored.terminal_reason() == TerminalReason::Checkmate,
           "Onyx Minions give the color/rank-reflected checkmate");

    Position setup = parse(
      "w;king,w,c8;king,b,a10;devil,w,a1;minion,w,a9;minion,w,b9");
    const Move matingMove = require_move(setup, "c8-b8");
    expect(setup.move_to_display_string(matingMove) == "Kb8#",
           "a King move completing a Minion net is displayed as mate");
    Search setupSearch(1);
    const auto setupResult = setupSearch.think(setup, limits);
    expect(setupResult.bestMove && setupResult.score >= 29900,
           "search propagates automatic mate through quiescence");
    if (setupResult.bestMove) {
        Position chosen = setup;
        Undo undo;
        expect(chosen.make_move(*setupResult.bestMove, undo) &&
                 chosen.winner() == Color::White,
               "reported mating move actually ends the game");
    }

    Position escaping = parse("b;king,w,h1;king,b,a10;minion,w,a9");
    expect(escaping.in_check() && !escaping.game_over(),
           "a Minion threat with a legal escape is check, not mate");
    const auto escapingUpn = escaping.upn();
    Undo escape;
    expect(escaping.make_move(require_move(escaping, "a10-b10"), escape) &&
             escaping.piece_on(Position::square_from_name("a10")) != Position::NoPiece,
           "a legal escape advances the Minion exactly once, not off the edge");
    escaping.undo_move(escape);
    expect(escaping.upn() == escapingUpn, "Minion escape is fully reversible");
    Position checkingMove = parse("w;king,w,h1;king,b,a10;minion,w,a9");
    expect(checkingMove.move_to_display_string(require_move(checkingMove, "h1-h2")) == "Kh2+",
           "a nonmating Minion threat receives the check suffix");

    Position stalemate = parse(
      "b;king,w,b8;king,b,a10;pawn,w,a9;minion,w,b9");
    expect(!stalemate.in_check() && stalemate.legal_moves().empty() &&
             stalemate.terminal_reason() == TerminalReason::Stalemate &&
             !stalemate.winner(),
           "Minions restricting escape squares do not make every stalemate mate");

    Position cooling = parse("b;king,w,h1;king,b,a10;minion,w,a9");
    const int minion = cooling.piece_on(Position::square_from_name("a9"));
    cooling.piece(minion).cooldown = 2;
    expect(!cooling.in_check(), "Minion cooldown two survives the hypothetical turn");
    cooling.piece(minion).cooldown = 1;
    expect(cooling.in_check(), "Minion cooldown one expires before automatic movement");
    cooling.piece(minion).freezeCount = 1;
    expect(!cooling.in_check(), "a frozen Minion does not advance into the King");

    Position train = parse("b;king,w,h1;king,b,a10;minion,w,a8;minion,w,a9");
    expect(train.in_check(), "an automatic Minion train threatens through its front member");
    Position edge = parse("b;king,w,h1;king,b,a9;minion,w,a10");
    expect(!edge.in_check(), "far-edge Minions disappear instead of attacking backward");
    Position revealedRay = parse("b;king,w,h1;king,b,h10;rook,w,b10;minion,w,f10");
    expect(revealedRay.in_check(),
           "far-edge disappearance can uncover another character's checking ray");
    Position blockedRay = parse("b;king,w,h1;king,b,h6;rook,w,a6;minion,w,b5");
    expect(!blockedRay.in_check(),
           "automatic movement can block a ray that was checking before the turn phase");

    Position protectedKing = parse("b;king,w,h1;king,b,a10;angel,b,h8;minion,w,a8");
    Undo attach;
    expect(protectedKing.make_move(require_move(protectedKing, "h8&a10"), attach),
           "attach an Angel before the approaching Minion reaches the King");
    protectedKing.set_side_to_move(Color::Black);
    const auto protectedUpn = protectedKing.upn();
    expect(!protectedKing.in_check() && protectedKing.upn() == protectedUpn,
           "hypothetical Minion collision respects Angel rescue without consuming it");

    // The Minion's Bomb capture kills both nearby Kings.
    Position mutual = parse("b;king,w,c9;king,b,a10;bomb,b,b10;minion,w,b9");
    expect(!mutual.in_check(),
           "an automatic simultaneous royal knockout is not an opposing mate threat");

    Position cooldownAttack = parse("w;king,w,a1;king,b,h10;sniper,b,a8;rook,w,h2");
    const int sniper = cooldownAttack.piece_on(Position::square_from_name("a8"));
    cooldownAttack.piece(sniper).cooldown = 1;
    expect(cooldownAttack.in_check(),
           "turn-start check also recognizes an opponent Sniper becoming ready");
    expect(!cooldownAttack.move_from_string("h2-h3"),
           "post-action legality rejects a quiet move leaving the readied Sniper's check");
    cooldownAttack.piece(sniper).cooldown = 2;
    expect(!cooldownAttack.in_check(), "a still-cooling Sniper does not give check");
    Position sniperMate = parse(
      "b;king,w,b8;king,b,a10;sniper,w,a9,0,1;knight,w,c8");
    expect(sniperMate.in_check() && sniperMate.legal_moves().empty() &&
             sniperMate.terminal_reason() == TerminalReason::Checkmate &&
             !TablebaseProbe::probe(sniperMate),
           "a ready-next-turn Sniper mates and cannot use stale tablebase seeds");

    Position disguised = mate;
    disguised.add_piece(PieceType::Jester, Color::Black, Position::square_from_name("h10"));
    expect(!disguised.in_check() && !disguised.game_over(),
           "King/Jester ambiguity still suspends check and checkmate");
    Position ghost = parse("b;king,w,h1;king,b,a10;ghost,w,a9");
    expect(!ghost.in_check(), "Ghost attacks still do not give check");
}

void test_native_insufficient_material() {
    Position bare;
    bare.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    bare.add_piece(PieceType::King, Color::Black, Position::square_from_name("h8"));
    expect(!bare.is_checkmate_possible() && bare.game_over() && !bare.winner() &&
             bare.terminal_reason() == TerminalReason::InsufficientMaterial,
           "bare real kings are an insufficient-material draw");

    Position oneMinor = bare;
    oneMinor.add_piece(PieceType::Knight, Color::White, Position::square_from_name("b1"));
    expect(!oneMinor.is_checkmate_possible(), "one knight remains insufficient");

    Position mixed = oneMinor;
    mixed.add_piece(PieceType::Bishop, Color::White, Position::square_from_name("c1"));
    expect(mixed.team_has_sufficient_material(Color::White) && mixed.is_checkmate_possible(),
           "native table accepts one minor plus one color-bound character");

    Position support = bare;
    support.add_piece(PieceType::Mage, Color::White, Position::square_from_name("b1"));
    expect(!support.is_checkmate_possible(), "mage alone remains insufficient");
    support.add_piece(PieceType::Checker, Color::White, Position::square_from_name("c2"));
    expect(support.is_checkmate_possible(),
           "mage plus a checker satisfies the native support/color-bound rule");

    Position devil = bare;
    devil.add_piece(PieceType::Devil, Color::White,
                    Position::square_from_name("b2"));
    expect(devil.team_has_sufficient_material(Color::White) &&
             devil.is_checkmate_possible() && !devil.game_over(),
           "Devil is sufficient because it can spawn mating Minions");

    Position spawnedMinion = bare;
    spawnedMinion.add_piece(PieceType::Minion, Color::White,
                            Position::square_from_name("a8"));
    expect(spawnedMinion.team_has_sufficient_material(Color::White) &&
             spawnedMinion.is_checkmate_possible() && !spawnedMinion.game_over(),
           "a spawned Minion remains sufficient after its Devil is captured");
}

void test_native_terminal_reasons() {
    Position stalemate;
    std::string stalemateError;
    expect(stalemate.set_upn(
      "b;hm=0;fm=1;ep=-;cont=0;forced=-1;epv=-1;"
      "king,w,h8,0,0,0,0,1,1,-1,1,-1,0;"
      "king,b,g10,0,0,0,0,0,1,-1,1,-1,0;"
      "dragon,w,g8,0,0,0,0,0,1,-1,1,-1,0",
      &stalemateError),
      "stalemate reason regression parses: " + stalemateError);
    expect(stalemate.game_over() && !stalemate.winner() &&
             stalemate.terminal_reason() == TerminalReason::Stalemate,
           "a safe royal with no legal action is classified as stalemate");

    Position checkmate;
    checkmate.add_piece(PieceType::King, Color::White,
                        Position::square_from_name("c8"));
    checkmate.add_piece(PieceType::Queen, Color::White,
                        Position::square_from_name("b9"));
    checkmate.add_piece(PieceType::King, Color::Black,
                        Position::square_from_name("a10"));
    checkmate.set_side_to_move(Color::Black);
    expect(checkmate.game_over() && checkmate.winner() == Color::White &&
             checkmate.terminal_reason() == TerminalReason::Checkmate,
           "a threatened royal with no legal action is classified as checkmate");

    Position captured;
    captured.add_piece(PieceType::King, Color::White,
                       Position::square_from_name("a1"));
    expect(captured.game_over() && captured.winner() == Color::White &&
             captured.terminal_reason() == TerminalReason::KingCaptured,
           "one surviving real King is classified as a King capture");

    Position simultaneous;
    simultaneous.clear();
    expect(simultaneous.game_over() && !simultaneous.winner() &&
             simultaneous.terminal_reason() ==
               TerminalReason::SimultaneousKingCapture,
           "two absent real Kings are classified as simultaneous capture");
}

void test_pawn_en_passant_lifetime() {
    Position position;
    position.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    position.add_piece(PieceType::King, Color::Black, Position::square_from_name("h8"));
    const int whitePawn = position.add_piece(PieceType::Pawn, Color::White,
                                              Position::square_from_name("e2"));
    const int blackPawn = position.add_piece(PieceType::Pawn, Color::Black,
                                              Position::square_from_name("d4"));
    Undo push;
    expect(position.make_move(require_move(position, "e2-e4"), push), "pawn double-step applies");
    expect(position.en_passant_square() == Position::square_from_name("e3") &&
           position.move_from_string("d4-e3").has_value(),
           "en-passant target survives through the opponent response");
    Position reloaded;
    std::string error;
    expect(reloaded.set_upn(position.upn(), &error) &&
           reloaded.move_from_string("d4-e3").has_value(),
           "lossless UPN preserves the en-passant victim across analysis requests");
    Undo capture;
    expect(position.make_move(require_move(position, "d4-e3"), capture),
           "en-passant capture applies");
    expect(!position.piece(whitePawn).alive && position.piece(blackPawn).square == Position::square_from_name("e3") &&
           position.en_passant_square() == Position::NoSquare,
           "en-passant removes the bypassed pawn and then expires");

    Position deployed;
    deployed.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    deployed.add_piece(PieceType::King, Color::Black, Position::square_from_name("h10"));
    deployed.add_piece(PieceType::Pawn, Color::White, Position::square_from_name("c3"));
    expect(deployed.move_from_string("c3-c5").has_value(),
           "an unmoved pawn deployed on the third home rank retains its native double step");

    auto hiddenEnPassantFixture = [](Color ghostColor) {
        Position fixture;
        fixture.add_piece(PieceType::King, Color::White,
                          Position::square_from_name("a1"));
        fixture.add_piece(PieceType::King, Color::Black,
                          Position::square_from_name("h10"));
        fixture.add_piece(PieceType::Pawn, Color::White,
                          Position::square_from_name("e2"));
        fixture.add_piece(PieceType::Pawn, Color::Black,
                          Position::square_from_name("d4"));
        const int ghost = fixture.add_piece(
          PieceType::Ghost, ghostColor, Position::square_from_name("e3"));
        fixture.piece(ghost).visible = false;
        return fixture;
    };
    for (const Color ghostColor : {Color::White, Color::Black}) {
        Position collision = hiddenEnPassantFixture(ghostColor);
        const int bypassingPawn = collision.piece_on(Position::square_from_name("e2"));
        const int capturingPawn = collision.piece_on(Position::square_from_name("d4"));
        const int destinationGhost = collision.piece_on(Position::square_from_name("e3"));
        Undo hiddenPush;
        expect(collision.make_move(require_move(collision, "e2-e4"), hiddenPush),
               "double-step through an en-passant square's hidden Ghost applies");
        expect(collision.piece(destinationGhost).alive &&
                 collision.move_from_string("d4-e3").has_value(),
               "en-passant remains available when its destination contains a hidden Ghost");
        Undo hiddenCapture;
        expect(collision.make_move(require_move(collision, "d4-e3"), hiddenCapture),
               "en-passant onto a hidden Ghost applies");
        expect(collision.piece(bypassingPawn).alive &&
                 collision.piece(bypassingPawn).square ==
                   Position::square_from_name("e4") &&
                 !collision.piece(capturingPawn).alive &&
                 !collision.piece(destinationGhost).alive,
               "hidden Ghost and en-passant attacker mutually knock out while bypassing Pawn survives");
    }
}

void test_cooldowns_minions_and_freeze_stacking() {
    Position hiddenEnemyMinionCollision;
    hiddenEnemyMinionCollision.add_piece(PieceType::King, Color::White,
                                         Position::square_from_name("a1"));
    const int blindMinion = hiddenEnemyMinionCollision.add_piece(
        PieceType::Minion, Color::White, Position::square_from_name("d4"));
    const int hiddenEnemyGhost = hiddenEnemyMinionCollision.add_piece(
        PieceType::Ghost, Color::Black, Position::square_from_name("d5"));
    hiddenEnemyMinionCollision.piece(hiddenEnemyGhost).visible = false;
    hiddenEnemyMinionCollision.add_piece(PieceType::King, Color::Black,
                                         Position::square_from_name("h10"));
    hiddenEnemyMinionCollision.add_piece(PieceType::Rook, Color::Black,
                                         Position::square_from_name("g9"));
    hiddenEnemyMinionCollision.set_side_to_move(Color::Black);
    Undo triggerEnemyCollision;
    expect(hiddenEnemyMinionCollision.make_move(
               require_move(hiddenEnemyMinionCollision, "g9-g8"),
               triggerEnemyCollision),
           "opponent waiting move triggers a Minion hidden-Ghost collision");
    expect(hiddenEnemyMinionCollision.piece(blindMinion).alive &&
               hiddenEnemyMinionCollision.piece(blindMinion).square ==
                   Position::square_from_name("d5") &&
               !hiddenEnemyMinionCollision.piece(hiddenEnemyGhost).alive,
           "automatic Minion captures an enemy hidden Ghost normally");

    Position hiddenAlliedMinionCollision;
    hiddenAlliedMinionCollision.add_piece(PieceType::King, Color::White,
                                          Position::square_from_name("a1"));
    const int alliedBlindMinion = hiddenAlliedMinionCollision.add_piece(
        PieceType::Minion, Color::White, Position::square_from_name("d4"));
    const int hiddenAlliedGhost = hiddenAlliedMinionCollision.add_piece(
        PieceType::Ghost, Color::White, Position::square_from_name("d5"));
    hiddenAlliedMinionCollision.piece(hiddenAlliedGhost).visible = false;
    hiddenAlliedMinionCollision.add_piece(PieceType::King, Color::Black,
                                          Position::square_from_name("h10"));
    hiddenAlliedMinionCollision.add_piece(PieceType::Rook, Color::Black,
                                          Position::square_from_name("g9"));
    hiddenAlliedMinionCollision.set_side_to_move(Color::Black);
    Undo triggerAlliedCollision;
    expect(hiddenAlliedMinionCollision.make_move(
               require_move(hiddenAlliedMinionCollision, "g9-g8"),
               triggerAlliedCollision),
           "opponent waiting move triggers an allied Minion collision");
    expect(hiddenAlliedMinionCollision.piece(alliedBlindMinion).alive &&
               hiddenAlliedMinionCollision.piece(alliedBlindMinion).square ==
                   Position::square_from_name("d5") &&
               !hiddenAlliedMinionCollision.piece(hiddenAlliedGhost).alive,
           "automatic Minion knocks out an allied hidden non-Minion");

    Position blindDevil;
    blindDevil.add_piece(PieceType::King, Color::White,
                         Position::square_from_name("a1"));
    blindDevil.add_piece(PieceType::King, Color::Black,
                         Position::square_from_name("h10"));
    const int blindDevilId = blindDevil.add_piece(
      PieceType::Devil, Color::White, Position::square_from_name("d2"));
    const int devilGhost = blindDevil.add_piece(
      PieceType::Ghost, Color::Black, Position::square_from_name("e4"));
    blindDevil.piece(devilGhost).visible = false;
    const int friendlyGhost = blindDevil.add_piece(
      PieceType::Ghost, Color::White, Position::square_from_name("c4"));
    blindDevil.piece(friendlyGhost).visible = false;
    expect(blindDevil.move_from_string("d2@e4").has_value() &&
             !blindDevil.move_from_string("d2@c4").has_value(),
           "Devil may blindly target an enemy hidden Ghost but not an allied one");
    Undo blindSpawn;
    expect(blindDevil.make_move(
             require_move(blindDevil, "d2@e4"), blindSpawn),
           "Devil blind hidden-Ghost spawn applies");
    expect(!blindDevil.piece(devilGhost).alive &&
             blindDevil.piece_on(Position::square_from_name("e4")) ==
               Position::NoPiece &&
             blindDevil.piece(blindDevilId).cooldown == 2,
           "blind Devil spawn kills the Ghost, creates no Minion, and starts cooldown");

    Position devil;
    devil.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    devil.add_piece(PieceType::King, Color::Black, Position::square_from_name("h8"));
    const int devilId = devil.add_piece(PieceType::Devil, Color::White,
                                        Position::square_from_name("d2"));
    devil.add_piece(PieceType::Pawn, Color::White, Position::square_from_name("b2"));
    Undo spawn;
    expect(devil.make_move(require_move(devil, "d2@d4"), spawn), "devil spawn applies");
    const int minion = devil.piece_on(Position::square_from_name("d4"));
    expect(minion != Position::NoPiece && devil.piece(minion).type == PieceType::Minion &&
           devil.piece(devilId).cooldown == 2,
           "devil creates a minion and enters the native three-change cooldown");
    Undo blackMove;
    expect(devil.make_move(require_move(devil, "h8-h7"), blackMove), "opponent reply applies");
    expect(devil.piece(minion).square == Position::square_from_name("d5") &&
           devil.piece(devilId).cooldown == 1,
           "minion advances automatically at its side's turn start");

    Position frozenMinion;
    frozenMinion.add_piece(PieceType::King, Color::White,
                           Position::square_from_name("a1"));
    frozenMinion.add_piece(PieceType::King, Color::Black,
                           Position::square_from_name("h10"));
    frozenMinion.add_piece(PieceType::Devil, Color::White,
                           Position::square_from_name("d2"));
    frozenMinion.add_piece(PieceType::Penguin, Color::Black,
                           Position::square_from_name("c5"));
    Undo spawnFrozenMinion;
    expect(frozenMinion.make_move(
             require_move(frozenMinion, "d2@d4"), spawnFrozenMinion),
           "Devil spawns the future frozen Minion");
    const int stoppedMinion = frozenMinion.piece_on(
      Position::square_from_name("d4"));
    Undo freezeMinion;
    expect(frozenMinion.make_move(
             require_move(frozenMinion, "c5-c4"), freezeMinion),
           "Penguin moves beside a Minion before its automatic phase");
    expect(frozenMinion.piece(stoppedMinion).alive &&
             frozenMinion.piece(stoppedMinion).square ==
               Position::square_from_name("d4") &&
             frozenMinion.piece(stoppedMinion).freezeCount == 1,
           "a frozen Minion skips its native turn-start automatic advance");

    Position angelAtEdge;
    angelAtEdge.add_piece(PieceType::King, Color::White,
                          Position::square_from_name("h1"));
    angelAtEdge.add_piece(PieceType::King, Color::Black,
                          Position::square_from_name("h10"));
    const int edgeAngel = angelAtEdge.add_piece(
      PieceType::Angel, Color::White, Position::square_from_name("b2"));
    const int edgeMinion = angelAtEdge.add_piece(
      PieceType::Minion, Color::White, Position::square_from_name("a10"));
    angelAtEdge.add_piece(PieceType::Pawn, Color::Black,
                          Position::square_from_name("g9"));
    Undo attachEdgeAngel;
    expect(angelAtEdge.make_move(
             require_move(angelAtEdge, "b2&a10"), attachEdgeAngel),
           "Angel can protect a Minion already at the far edge");
    const int survivingHalo = angelAtEdge.piece_on(
      Position::square_from_name("b2"));
    Undo triggerEdgeMinion;
    expect(angelAtEdge.make_move(
             require_move(angelAtEdge, "g9-g8"), triggerEdgeMinion),
           "opponent move starts the protected Minion's automatic phase");
    expect(!angelAtEdge.piece(edgeMinion).alive &&
             angelAtEdge.piece(edgeAngel).alive &&
             !angelAtEdge.piece(edgeAngel).onBoard &&
             survivingHalo != Position::NoPiece &&
             angelAtEdge.piece(survivingHalo).alive,
           "far-edge Minion death bypasses Angel rescue but preserves Angel and Halo");
    Position edgeAngelRoundTrip;
    std::string edgeAngelError;
    expect(edgeAngelRoundTrip.set_upn(angelAtEdge.upn(), &edgeAngelError),
           "orphaned edge Angel/Halo state remains losslessly parseable: " +
             edgeAngelError);

    Position minionTrain;
    minionTrain.add_piece(PieceType::King, Color::White,
                          Position::square_from_name("h1"));
    minionTrain.add_piece(PieceType::King, Color::Black,
                          Position::square_from_name("h10"));
    // Add the rear Minion first so ID iteration alone would try to overwrite
    // the front one. Native same-team Undead suppression must still advance
    // the pair as a train.
    const int rearMinion = minionTrain.add_piece(
      PieceType::Minion, Color::White, Position::square_from_name("d4"));
    const int frontMinion = minionTrain.add_piece(
      PieceType::Minion, Color::White, Position::square_from_name("d5"));
    minionTrain.add_piece(PieceType::Pawn, Color::Black,
                          Position::square_from_name("g9"));
    minionTrain.set_side_to_move(Color::Black);
    Undo startTrain;
    expect(minionTrain.make_move(
             require_move(minionTrain, "g9-g8"), startTrain),
           "opponent move starts a two-Minion train");
    expect(minionTrain.piece(rearMinion).alive &&
             minionTrain.piece(rearMinion).square ==
               Position::square_from_name("d5") &&
             minionTrain.piece(frontMinion).alive &&
             minionTrain.piece(frontMinion).square ==
               Position::square_from_name("d6"),
           "unstunned allied Minions advance as an order-independent train");

    Position alliedCollision;
    alliedCollision.add_piece(PieceType::King, Color::White,
                              Position::square_from_name("h1"));
    alliedCollision.add_piece(PieceType::King, Color::Black,
                              Position::square_from_name("h10"));
    const int alliedMinion = alliedCollision.add_piece(
      PieceType::Minion, Color::White, Position::square_from_name("d4"));
    const int alliedRook = alliedCollision.add_piece(
      PieceType::Rook, Color::White, Position::square_from_name("d5"));
    alliedCollision.add_piece(PieceType::Pawn, Color::Black,
                              Position::square_from_name("g9"));
    alliedCollision.set_side_to_move(Color::Black);
    Undo collideWithAlly;
    expect(alliedCollision.make_move(
             require_move(alliedCollision, "g9-g8"), collideWithAlly),
           "opponent move starts an allied Minion collision");
    expect(alliedCollision.piece(alliedMinion).alive &&
             alliedCollision.piece(alliedMinion).square ==
               Position::square_from_name("d5") &&
             !alliedCollision.piece(alliedRook).alive,
           "automatic Minion knocks out a non-Minion ally in its path");

    Position stunnedTrain;
    stunnedTrain.add_piece(PieceType::King, Color::White,
                           Position::square_from_name("h1"));
    stunnedTrain.add_piece(PieceType::King, Color::Black,
                           Position::square_from_name("h10"));
    const int movingRear = stunnedTrain.add_piece(
      PieceType::Minion, Color::White, Position::square_from_name("d4"));
    const int stunnedFront = stunnedTrain.add_piece(
      PieceType::Minion, Color::White, Position::square_from_name("d5"));
    stunnedTrain.piece(stunnedFront).cooldown = 2;
    stunnedTrain.add_piece(PieceType::Pawn, Color::Black,
                           Position::square_from_name("g9"));
    stunnedTrain.set_side_to_move(Color::Black);
    Undo hitStunnedMinion;
    expect(stunnedTrain.make_move(
             require_move(stunnedTrain, "g9-g8"), hitStunnedMinion),
           "opponent move starts a train behind a stunned Minion");
    expect(stunnedTrain.piece(movingRear).alive &&
             stunnedTrain.piece(movingRear).square ==
               Position::square_from_name("d5") &&
             !stunnedTrain.piece(stunnedFront).alive,
           "automatic Minion knocks out a stunned allied Minion instead of forming a train");

    Position freeze;
    freeze.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    freeze.add_piece(PieceType::King, Color::Black, Position::square_from_name("h8"));
    const int first = freeze.add_piece(PieceType::Penguin, Color::White,
                                       Position::square_from_name("c2"));
    const int second = freeze.add_piece(PieceType::Penguin, Color::Black,
                                        Position::square_from_name("e2"));
    const int rook = freeze.add_piece(PieceType::Rook, Color::White,
                                      Position::square_from_name("d3"));
    expect(freeze.piece(rook).freezeCount == 0,
           "a deployed Penguin has no freeze aura before it moves");
    Undo firstFreeze;
    expect(freeze.make_move(require_move(freeze, "c2-c3"), firstFreeze),
           "first Penguin move applies");
    Undo secondFreeze;
    expect(freeze.make_move(require_move(freeze, "e2-e3"), secondFreeze),
           "second Penguin move applies");
    expect(freeze.piece(rook).freezeCount == 2 && freeze.piece(first).freezeCount == 0 &&
           freeze.piece(second).freezeCount == 0,
           "move-triggered Penguin freezes stack while Penguins remain immune");
    freeze.remove_piece(first);
    expect(freeze.piece(rook).freezeCount == 1, "removing one Penguin removes only its freeze layer");
    freeze.remove_piece(second);
    expect(freeze.piece(rook).freezeCount == 0, "removing the final Penguin fully unfreezes its neighbor");

    Position giantFreeze;
    giantFreeze.add_piece(PieceType::King, Color::White,
                          Position::square_from_name("a1"));
    giantFreeze.add_piece(PieceType::King, Color::Black,
                          Position::square_from_name("h10"));
    const int giantPenguin = giantFreeze.add_piece(
      PieceType::Penguin, Color::White, Position::square_from_name("c2"));
    const int frozenGiant = giantFreeze.add_piece(
      PieceType::Giant, Color::Black, Position::square_from_name("d3"));
    const std::string giantFreezeBefore = giantFreeze.upn();
    Undo giantFreezeMove;
    expect(giantFreeze.make_move(require_move(giantFreeze, "c2-c3"), giantFreezeMove),
           "Penguin can move beside two tiles of one Giant");
    expect(giantFreeze.piece(frozenGiant).freezeCount == 1,
           "one Penguin contributes one HashSet freeze layer to a multi-tile Giant");
    giantFreeze.remove_piece(giantPenguin);
    expect(giantFreeze.piece(frozenGiant).freezeCount == 0,
           "removing that Penguin thaws a multi-tile Giant exactly once");
    Position giantFreezeUndo;
    std::string giantFreezeError;
    expect(giantFreezeUndo.set_upn(giantFreezeBefore, &giantFreezeError),
           "multi-tile freeze setup round trips");
    Undo giantFreezeUndoMove;
    expect(giantFreezeUndo.make_move(
             require_move(giantFreezeUndo, "c2-c3"), giantFreezeUndoMove),
           "multi-tile freeze applies before undo");
    giantFreezeUndo.undo_move(giantFreezeUndoMove);
    expect(giantFreezeUndo.upn() == giantFreezeBefore,
           "multi-tile Penguin freeze undo restores every set-derived field");

    Position mageThaw;
    mageThaw.add_piece(PieceType::King, Color::White,
                       Position::square_from_name("a1"));
    mageThaw.add_piece(PieceType::King, Color::Black,
                       Position::square_from_name("h10"));
    mageThaw.add_piece(PieceType::Mage, Color::White,
                       Position::square_from_name("a2"));
    const int swappedRook = mageThaw.add_piece(
      PieceType::Rook, Color::White, Position::square_from_name("d3"));
    const int freezingPenguin = mageThaw.add_piece(
      PieceType::Penguin, Color::Black, Position::square_from_name("c4"));
    mageThaw.set_side_to_move(Color::Black);
    Undo establishMageFreeze;
    expect(mageThaw.make_move(
             require_move(mageThaw, "c4-c3"), establishMageFreeze),
           "enemy Penguin establishes a Mage-target freeze");
    expect(mageThaw.piece(swappedRook).freezeCount == 1,
           "Mage swap target starts frozen");
    const std::string frozenMageState = mageThaw.upn();
    Undo mageSwapThaw;
    expect(mageThaw.make_move(require_move(mageThaw, "a2~d3"), mageSwapThaw),
           "Mage may forcibly swap a frozen ally");
    expect(mageThaw.piece(swappedRook).square == Position::square_from_name("a2") &&
             mageThaw.piece(swappedRook).freezeCount == 0 &&
             mageThaw.piece(freezingPenguin).action == 0,
           "Mage displacement removes the target from native Penguin freeze sets");
    mageThaw.undo_move(mageSwapThaw);
    expect(mageThaw.upn() == frozenMageState,
           "undoing a Mage thaw restores Penguin membership and freeze count");

    Position fishermanThaw;
    fishermanThaw.add_piece(PieceType::King, Color::White,
                            Position::square_from_name("a1"));
    fishermanThaw.add_piece(PieceType::King, Color::Black,
                            Position::square_from_name("h10"));
    fishermanThaw.add_piece(PieceType::Fisherman, Color::White,
                            Position::square_from_name("d1"));
    const int pulledPenguin = fishermanThaw.add_piece(
      PieceType::Penguin, Color::Black, Position::square_from_name("d5"));
    const int auraVictim = fishermanThaw.add_piece(
      PieceType::Rook, Color::White, Position::square_from_name("e4"));
    fishermanThaw.set_side_to_move(Color::Black);
    Undo establishFishermanFreeze;
    expect(fishermanThaw.make_move(
             require_move(fishermanThaw, "d5-d4"), establishFishermanFreeze),
           "Penguin establishes an aura before being hooked");
    expect(fishermanThaw.piece(auraVictim).freezeCount == 1,
           "hook target Penguin owns one active freeze layer");
    const std::string activePenguinState = fishermanThaw.upn();
    Undo fishermanPullThaw;
    expect(fishermanThaw.make_move(
             require_move(fishermanThaw, "d1!d4"), fishermanPullThaw),
           "Fisherman can hook an active enemy Penguin");
    expect(fishermanThaw.piece(pulledPenguin).square ==
             Position::square_from_name("d2") &&
             fishermanThaw.piece(pulledPenguin).action == 0 &&
             fishermanThaw.piece(auraVictim).freezeCount == 0,
           "hooking a Penguin extinguishes its old aura without creating a new one");
    fishermanThaw.undo_move(fishermanPullThaw);
    expect(fishermanThaw.upn() == activePenguinState,
           "undoing a Penguin hook restores its old aura exactly");

    Position fishermanPromotion;
    fishermanPromotion.add_piece(PieceType::King, Color::White,
                                  Position::square_from_name("a1"));
    fishermanPromotion.add_piece(PieceType::King, Color::Black,
                                  Position::square_from_name("h1"));
    fishermanPromotion.add_piece(PieceType::Fisherman, Color::White,
                                  Position::square_from_name("a10"));
    const int hookedPawn = fishermanPromotion.add_piece(
      PieceType::Pawn, Color::White, Position::square_from_name("c10"));
    Undo fishermanPromotes;
    expect(fishermanPromotion.make_move(
             require_move(fishermanPromotion, "a10!c10"), fishermanPromotes),
           "Fisherman can hook an allied Pawn along the promotion rank");
    expect(fishermanPromotion.piece(hookedPawn).square ==
             Position::square_from_name("b10") &&
             fishermanPromotion.piece(hookedPawn).type == PieceType::Queen,
           "Fisherman forced relocation invokes the Pawn promotion hook");

    Position angelThaw;
    angelThaw.add_piece(PieceType::King, Color::White,
                        Position::square_from_name("a1"));
    angelThaw.add_piece(PieceType::King, Color::Black,
                        Position::square_from_name("h10"));
    angelThaw.add_piece(PieceType::Angel, Color::White,
                        Position::square_from_name("a2"));
    const int rescuedRook = angelThaw.add_piece(
      PieceType::Rook, Color::White, Position::square_from_name("d3"));
    const int rescuePenguin = angelThaw.add_piece(
      PieceType::Penguin, Color::Black, Position::square_from_name("c4"));
    angelThaw.add_piece(PieceType::Queen, Color::Black,
                        Position::square_from_name("d5"));
    angelThaw.add_piece(PieceType::Pawn, Color::White,
                        Position::square_from_name("h2"));
    Undo attachAngel;
    expect(angelThaw.make_move(require_move(angelThaw, "a2&d3"), attachAngel),
           "Angel attaches before forced-relocation freeze test");
    Undo freezeAngelHost;
    expect(angelThaw.make_move(require_move(angelThaw, "c4-c3"), freezeAngelHost),
           "Penguin freezes an Angel-protected host");
    expect(angelThaw.piece(rescuedRook).freezeCount == 1,
           "protected host is frozen before rescue");
    Undo whiteWaitingMove;
    expect(angelThaw.make_move(require_move(angelThaw, "h2-h3"), whiteWaitingMove),
           "white waiting move exposes the protected host to capture");
    Undo rescueThaw;
    expect(angelThaw.make_move(require_move(angelThaw, "d5-d3"), rescueThaw),
           "capturing a protected frozen host triggers Angel rescue");
    expect(angelThaw.piece(rescuedRook).alive &&
             angelThaw.piece(rescuedRook).square == Position::square_from_name("a2") &&
             angelThaw.piece(rescuedRook).freezeCount == 0 &&
             angelThaw.piece(rescuePenguin).action == 0,
           "Angel rescue detaches the host from its old Penguin freeze set");

    Position angelPromotion;
    angelPromotion.add_piece(PieceType::King, Color::White,
                             Position::square_from_name("a1"));
    angelPromotion.add_piece(PieceType::King, Color::Black,
                             Position::square_from_name("h8"));
    angelPromotion.add_piece(PieceType::Angel, Color::White,
                             Position::square_from_name("a10"));
    const int rescuedPawn = angelPromotion.add_piece(
      PieceType::Pawn, Color::White, Position::square_from_name("d3"));
    angelPromotion.add_piece(PieceType::Rook, Color::Black,
                             Position::square_from_name("d5"));
    Undo attachPromotionAngel;
    expect(angelPromotion.make_move(
             require_move(angelPromotion, "a10&d3"), attachPromotionAngel),
           "Angel links a Pawn to a promotion-rank Halo");
    Undo promoteOnRescue;
    expect(angelPromotion.make_move(
             require_move(angelPromotion, "d5-d3"), promoteOnRescue),
           "capturing the protected Pawn triggers promotion-rank rescue");
    expect(angelPromotion.piece(rescuedPawn).alive &&
             angelPromotion.piece(rescuedPawn).square ==
               Position::square_from_name("a10") &&
             angelPromotion.piece(rescuedPawn).type == PieceType::Queen,
           "Angel rescue invokes the protected Pawn's promotion hook");

    Position forcedGhostVisibility;
    forcedGhostVisibility.add_piece(PieceType::King, Color::White,
                                     Position::square_from_name("h1"));
    forcedGhostVisibility.add_piece(PieceType::King, Color::Black,
                                     Position::square_from_name("b2"));
    forcedGhostVisibility.add_piece(PieceType::Mage, Color::White,
                                     Position::square_from_name("a2"));
    const int displacedGhost = forcedGhostVisibility.add_piece(
      PieceType::Ghost, Color::White, Position::square_from_name("d3"));
    forcedGhostVisibility.piece(displacedGhost).visible = false;
    Undo forceGhostBesideRoyal;
    expect(forcedGhostVisibility.make_move(
             require_move(forcedGhostVisibility, "a2~d3"),
             forceGhostBesideRoyal),
           "Mage can displace a hidden allied Ghost beside an enemy royal");
    expect(forcedGhostVisibility.piece(displacedGhost).square ==
             Position::square_from_name("a2") &&
             !forcedGhostVisibility.piece(displacedGhost).visible,
           "forced Mage displacement does not invoke the Ghost move-reveal hook");

    Position forcedRoyalVisibility;
    forcedRoyalVisibility.add_piece(PieceType::Mage, Color::White,
                                     Position::square_from_name("a2"));
    forcedRoyalVisibility.add_piece(PieceType::King, Color::White,
                                     Position::square_from_name("d3"));
    forcedRoyalVisibility.add_piece(PieceType::King, Color::Black,
                                     Position::square_from_name("h10"));
    const int unrevealedGhost = forcedRoyalVisibility.add_piece(
      PieceType::Ghost, Color::Black, Position::square_from_name("b2"));
    forcedRoyalVisibility.piece(unrevealedGhost).visible = false;
    Undo forceRoyalBesideGhost;
    expect(forcedRoyalVisibility.make_move(
             require_move(forcedRoyalVisibility, "a2~d3"),
             forceRoyalBesideGhost),
           "Mage can displace its allied King beside a hidden enemy Ghost");
    expect(!forcedRoyalVisibility.piece(unrevealedGhost).visible,
           "forced royal displacement does not invoke the normal royal reveal hook");

    Position penguinMovement;
    penguinMovement.add_piece(PieceType::King, Color::White,
                              Position::square_from_name("a1"));
    penguinMovement.add_piece(PieceType::King, Color::Black,
                              Position::square_from_name("h10"));
    penguinMovement.add_piece(PieceType::Penguin, Color::White,
                              Position::square_from_name("h1"));
    penguinMovement.add_piece(PieceType::Dragon, Color::Black,
                              Position::square_from_name("h2"));
    expect(!penguinMovement.move_from_string("h1-h2").has_value(),
           "penguin cannot attack an occupied adjacent square");

    Position dormantPenguin;
    dormantPenguin.add_piece(PieceType::King, Color::White,
                             Position::square_from_name("a1"));
    dormantPenguin.add_piece(PieceType::King, Color::Black,
                             Position::square_from_name("h10"));
    const int bomb = dormantPenguin.add_piece(PieceType::Bomb, Color::White,
                                              Position::square_from_name("g1"));
    const int dormant = dormantPenguin.add_piece(PieceType::Penguin, Color::White,
                                                 Position::square_from_name("h1"));
    const int dragon = dormantPenguin.add_piece(PieceType::Dragon, Color::Black,
                                                Position::square_from_name("h2"));
    dormantPenguin.set_side_to_move(Color::Black);
    const auto bombCapture = dormantPenguin.move_from_string("h2-g1");
    expect(bombCapture.has_value(),
           "an unmoved adjacent Penguin does not freeze an enemy Dragon");
    Undo explosion;
    expect(bombCapture && dormantPenguin.make_move(*bombCapture, explosion) &&
           !dormantPenguin.piece(bomb).alive && !dormantPenguin.piece(dormant).alive &&
           !dormantPenguin.piece(dragon).alive,
           "Dragon capture of a dormant Penguin's Bomb resolves the full explosion");
}

void test_sniper_berserker_and_dragon() {
    Position sniper;
    sniper.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    sniper.add_piece(PieceType::King, Color::Black, Position::square_from_name("h8"));
    const int sniperId = sniper.add_piece(PieceType::Sniper, Color::White,
                                          Position::square_from_name("d2"));
    const int target = sniper.add_piece(PieceType::Rook, Color::Black,
                                        Position::square_from_name("d7"));
    Undo shot;
    expect(sniper.make_move(require_move(sniper, "d2xd7"), shot), "sniper forward shot applies");
    expect(!sniper.piece(target).alive && sniper.piece(sniperId).square == Position::square_from_name("d2") &&
           sniper.piece(sniperId).cooldown == 2,
           "sniper stays put after shooting and begins its reload cooldown");

    Position hiddenShot;
    hiddenShot.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    hiddenShot.add_piece(PieceType::King, Color::Black, Position::square_from_name("h10"));
    hiddenShot.add_piece(PieceType::Sniper, Color::White, Position::square_from_name("d2"));
    const int sniperGhost = hiddenShot.add_piece(PieceType::Ghost, Color::Black,
                                                  Position::square_from_name("d5"));
    hiddenShot.piece(sniperGhost).visible = false;
    hiddenShot.add_piece(PieceType::Rook, Color::Black, Position::square_from_name("d8"));
    hiddenShot.add_piece(PieceType::Pawn, Color::Black, Position::square_from_name("e2"));
    expect(!hiddenShot.move_from_string("d2xd5").has_value() &&
           hiddenShot.move_from_string("d2xd8").has_value(),
           "sniper fire passes through an unseen ghost to the first visible target");
    expect(!hiddenShot.move_from_string("d2-e2").has_value(),
           "sniper cannot attack an occupied sideways square");
    expect(hiddenShot.move_from_string("d2-c2").has_value(),
           "sniper can make a quiet one-square sideways move");

    Position hiddenFriendlyGhost;
    hiddenFriendlyGhost.add_piece(PieceType::King, Color::White,
                                   Position::square_from_name("a1"));
    const int friendlyGhost = hiddenFriendlyGhost.add_piece(
        PieceType::Ghost, Color::White, Position::square_from_name("a3"));
    hiddenFriendlyGhost.piece(friendlyGhost).visible = false;
    hiddenFriendlyGhost.add_piece(PieceType::Prince, Color::White,
                                   Position::square_from_name("h3"));
    hiddenFriendlyGhost.add_piece(PieceType::King, Color::Black,
                                   Position::square_from_name("h10"));
    hiddenFriendlyGhost.add_piece(PieceType::Sniper, Color::Black,
                                   Position::square_from_name("a8"));
    expect(!hiddenFriendlyGhost.move_from_string("h3-g4").has_value(),
           "an own Ghost hidden from the enemy does not shield the King from Sniper check");

    Position berserker;
    berserker.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    berserker.add_piece(PieceType::King, Color::Black, Position::square_from_name("h8"));
    const int berserkerId = berserker.add_piece(PieceType::Berserker, Color::White,
                                                Position::square_from_name("d4"));
    berserker.add_piece(PieceType::Pawn, Color::Black, Position::square_from_name("e5"));
    Undo knockout;
    expect(berserker.make_move(require_move(berserker, "d4-e5"), knockout),
           "berserker knockout applies");
    expect(berserker.piece(berserkerId).power == 1 &&
           berserker.material_points(berserkerId) == 30,
           "berserker radius and dynamic native material grow after a knockout");
    berserker.set_side_to_move(Color::White);
    expect(berserker.move_from_string("e5-g6").has_value(),
           "powered berserker reaches every square in its Chebyshev-radius box");

    Position veteranBerserker;
    veteranBerserker.add_piece(PieceType::King, Color::White,
                               Position::square_from_name("a1"));
    veteranBerserker.add_piece(PieceType::King, Color::Black,
                               Position::square_from_name("h10"));
    const int veteran = veteranBerserker.add_piece(
      PieceType::Berserker, Color::White, Position::square_from_name("d4"));
    veteranBerserker.piece(veteran).power = 7;
    veteranBerserker.add_piece(PieceType::Pawn, Color::Black,
                               Position::square_from_name("h8"));
    Undo eighthAttack;
    expect(veteranBerserker.make_move(
             require_move(veteranBerserker, "d4-h8"), eighthAttack),
           "Berserker can attack after its useful radius reaches the board limit");
    expect(veteranBerserker.piece(veteran).power == 8 &&
             veteranBerserker.material_points(veteran) == 135,
           "native Berserker power and dynamic material continue beyond level eight");

    Position verticalVeteran;
    verticalVeteran.add_piece(PieceType::King, Color::White,
                               Position::square_from_name("h1"));
    verticalVeteran.add_piece(PieceType::King, Color::Black,
                               Position::square_from_name("h10"));
    const int vertical = verticalVeteran.add_piece(
      PieceType::Berserker, Color::White, Position::square_from_name("a1"));
    verticalVeteran.piece(vertical).power = 7;
    expect(!verticalVeteran.move_from_string("a1-a10").has_value(),
           "level-eight Berserker cannot yet span all ten ranks");
    verticalVeteran.piece(vertical).power = 8;
    expect(verticalVeteran.move_from_string("a1-a10").has_value(),
           "level-nine Berserker reaches rank ten without an eight-file clamp");

    Position hiddenLeap;
    hiddenLeap.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    hiddenLeap.add_piece(PieceType::King, Color::Black, Position::square_from_name("h10"));
    hiddenLeap.add_piece(PieceType::Knight, Color::White, Position::square_from_name("d4"));
    const int hiddenGhost = hiddenLeap.add_piece(PieceType::Ghost, Color::Black,
                                                  Position::square_from_name("f5"));
    hiddenLeap.piece(hiddenGhost).visible = false;
    Undo hiddenLeapCapture;
    expect(hiddenLeap.make_move(
             require_move(hiddenLeap, "d4-f5"), hiddenLeapCapture),
           "an ordinary leaper may enter an apparently empty enemy Ghost cell");
    expect(!hiddenLeap.piece(hiddenGhost).alive,
           "the ordinary leaper captures the hidden enemy Ghost normally");

    Position dragon;
    dragon.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    dragon.add_piece(PieceType::King, Color::Black, Position::square_from_name("h8"));
    dragon.add_piece(PieceType::Dragon, Color::White, Position::square_from_name("d4"));
    dragon.add_piece(PieceType::Pawn, Color::White, Position::square_from_name("e5"));
    expect(!dragon.move_from_string("d4-f6").has_value() &&
           dragon.move_from_string("d4-f5").has_value(),
           "dragon diagonals are blocked while its knight component leaps");
}

void test_native_draft_windows_and_costs() {
    expect(Position::info(PieceType::Jester).draftCost == 10, "native jester cost is recovered");
    expect(Position::info(PieceType::Giant).draftCost == 1, "native giant cost is recovered");
    expect(Position::info(PieceType::Dragon).draftCost == 15, "native dragon cost is recovered");

    DraftState draft;
    expect(draft.window().action == DraftAction::Ban && draft.window().player == Color::White,
           "draft opens with ivory ban");
    std::string error;
    expect(draft.choose(PieceType::Queen, &error) && draft.commit(&error), "first ban commits");
    expect(draft.choose(PieceType::Ninja, &error) && draft.commit(&error), "second ban commits");
    expect(draft.window().action == DraftAction::Pick &&
           draft.window().minimumPoints == 15 && draft.window().maximumPoints == 40,
           "first pick group adds at least 15 under the native 40 point cap");
    expect(!draft.choose(PieceType::Queen, &error), "banned character cannot be picked");
    expect(draft.choose(PieceType::Jester, &error) &&
           draft.choose(PieceType::Sniper, &error) &&
           draft.points(Color::White) == 27 &&
           !draft.choose(PieceType::Dragon, &error) && draft.commit(&error),
           "opening pick may lock 27 points but may not cross 40");

    expect(draft.choose(PieceType::Jester, &error) &&
           draft.choose(PieceType::Dragon, &error) && draft.commit(&error),
           "onyx protects a 25 point opening group above the same floor");
    expect(draft.choose(PieceType::Bomb, &error) && draft.commit(&error) &&
           draft.choose(PieceType::Parasite, &error) && draft.commit(&error),
           "second alternating ban pair commits");
    expect(draft.window().phase == 6 && draft.window().minimumPoints == 42 &&
           draft.window().maximumPoints == 80 &&
           !draft.unchoose(PieceType::Sniper) &&
           draft.choose(PieceType::Dragon, &error) && draft.commit(&error) &&
           draft.points(Color::White) == 42,
           "ivory adds another 15 without being able to remove its locked group");
    expect(draft.window().phase == 7 && draft.window().minimumPoints == 65 &&
           draft.window().maximumPoints == 90,
           "onyx must add 40 points under its asymmetric 90 point cap");
    for (int copy = 0; copy < 3; ++copy)
        expect(draft.choose(PieceType::Dragon, &error), "onyx middle Dragon locks");
    expect(draft.commit(&error) && draft.points(Color::Black) == 70,
           "onyx middle group may exceed its 40 point addition minimum");
    expect(draft.choose(PieceType::Sniper, &error) && draft.commit(&error) &&
           draft.choose(PieceType::Prince, &error) && draft.commit(&error),
           "final alternating ban pair commits");
    expect(draft.window().minimumPoints == 42 && draft.window().maximumPoints == 100,
           "final Ivory group has no required addition under the 100 point cap");

    DraftState underBudget = draft;
    expect(underBudget.commit(&error) && underBudget.points(Color::White) == 42,
           "Ivory may finish Ranked below the 100 point ceiling");
    expect(underBudget.commit(&error) && underBudget.points(Color::Black) == 70 &&
           underBudget.complete(),
           "Onyx may also finish Ranked below the 100 point ceiling");

    for (int copy = 0; copy < 4; ++copy)
        expect(draft.choose(PieceType::Rook, &error), "ivory final Rook locks");
    expect(draft.choose(PieceType::Pawn, &error) && draft.commit(&error),
           "ivory final pick reaches exactly 100 points");
    for (int copy = 0; copy < 2; ++copy)
        expect(draft.choose(PieceType::Dragon, &error), "onyx final Dragon locks");
    expect(draft.commit(&error) && draft.complete(),
           "both test teams may voluntarily fill their final groups to 100");

    DraftState automatic;
    std::vector<PieceType> automaticChoices;
    expect(automatic.suggest() == PieceType::Penguin,
           "evolved Ranked policy opens by denying the strongest uncommitted threat");
    for (int phase = 0; phase < DraftState::PhaseCount; ++phase) {
        expect(automatic.autoplay(automaticChoices, &error),
               "draft AI satisfies every ranked window: " + error);
        if (phase == 6 || phase == 7 || phase == 10 || phase == 11)
            expect(std::find(automaticChoices.begin(), automaticChoices.end(),
                             PieceType::Jester) == automaticChoices.end(),
                   "draft AI does not buy Jester after its King was publicly locked");
        expect(automatic.deployment_slots(Color::White) <= 24 &&
               automatic.deployment_slots(Color::Black) <= 24,
               "draft AI never exceeds either three-rank deployment zone");
        if (phase == 2)
            expect(automatic.points(Color::White) >= 15 &&
                   automatic.points(Color::White) <= 40,
                   "draft AI respects Ivory's opening 15-to-40 range");
        if (phase == 3)
            expect(automatic.points(Color::Black) >= 15 &&
                   automatic.points(Color::Black) <= 40,
                   "draft AI respects Onyx's opening 15-to-40 range");
    }
    expect(automatic.complete() && automatic.points(Color::White) == 100 &&
           automatic.points(Color::Black) == 100,
           "draft AI preserves the evolved finalist's exact full-budget rosters");
    for (const PieceType denied : {
           PieceType::Penguin, PieceType::Prince, PieceType::Jester,
           PieceType::Queen, PieceType::Ninja, PieceType::Rook})
        expect(automatic.banned(denied),
               "evolved policy preserves its validated adaptive ban sequence");
}

void test_safe_ordinary_static_exchange() {
    const auto exchange = [](std::string_view upn, std::string_view notation) {
        Position position;
        std::string error;
        expect(position.set_upn(upn, &error), "SEE fixture parses: " + error);
        const auto move = position.move_from_string(notation);
        expect(bool(move), "SEE fixture move parses");
        return move ? position.static_exchange(*move) : std::optional<int>{};
    };
    const auto freePawn = exchange(
      "w;king,w,a1;queen,w,d4;king,b,h10;pawn,b,d5", "d4-d5");
    expect(freePawn && *freePawn == Position::material_value(PieceType::Pawn),
           "ordinary SEE values an undefended Pawn capture");

    const auto defendedPawn = exchange(
      "w;king,w,a1;queen,w,d4;king,b,h10;pawn,b,d5;rook,b,d8", "d4-d5");
    expect(defendedPawn && *defendedPawn < 0,
           "ordinary SEE classifies Queen-for-Pawn into a Rook recapture as losing");

    expect(!exchange("w;king,w,a1;queen,w,d4;king,b,h10;bomb,b,d5", "d4-d5"),
           "SEE declines Bomb death and blast semantics");
    expect(!exchange(
      "w;king,w,a1;queen,w,d4;angel,w,b2;king,b,h10;pawn,b,d5", "d4-d5"),
      "SEE declines positions containing Angel attachment semantics");
}

void test_turn_boundary_reachability() {
    const auto parses = [](std::string_view upn) {
        Position position;
        std::string error;
        expect(position.set_upn(upn, &error), "reachability fixture parses: " + error);
        return position;
    };
    const Position bombArtifact = parses(
      "w;king,w,a1;king,b,c1;bomb,w,b1;bomb,w,b2");
    expect(!bombArtifact.ordinary_predecessor_king_safe(),
           "a nonmoving King cannot have been left vulnerable to a Bomb blast");

    const Position hiddenRoyal = parses(
      "w;king,w,a1;king,b,c1;jester,b,h10;bomb,w,b1;bomb,w,b2");
    expect(hiddenRoyal.ordinary_predecessor_king_safe(),
           "the previous mover's live Jester suspends ordinary King safety");

    const Position wrongJester = parses(
      "w;king,w,a1;jester,w,h1;king,b,c1;bomb,w,b1;bomb,w,b2");
    expect(!wrongJester.ordinary_predecessor_king_safe(),
           "a Jester owned by the current mover does not protect the previous mover");
}

void test_public_information_projection() {
    const auto parses = [](std::string_view upn) {
        Position position;
        std::string error;
        expect(position.set_upn(upn, &error),
               "public-information fixture parses: " + error);
        return position;
    };

    const Position firstRoyal = parses(
      "w;king,w,a1;jester,w,b1;king,b,h10");
    const Position swappedRoyal = parses(
      "w;king,w,b1;jester,w,a1;king,b,h10");
    const DisclosureContext ivory{Color::White, false};
    const DisclosureContext onyx{Color::Black, false};
    const DisclosureContext disclosedOnyx{Color::Black, true};
    expect(view_key(firstRoyal, onyx) == view_key(swappedRoyal, onyx),
           "enemy King/Jester assignments share one royal-silhouette view");
    expect(compact_view_key(firstRoyal, onyx) ==
             compact_view_key(swappedRoyal, onyx),
           "compact projection preserves royal-silhouette equivalence");
    Position swappedInPlace = firstRoyal;
    expect(swappedInPlace.swap_royal_roles(0, 1) &&
             swappedInPlace.piece(0).type == PieceType::Jester &&
             swappedInPlace.piece(1).type == PieceType::King &&
             swappedInPlace.pieces(Color::White, PieceType::King) ==
               (Bitboard{1} << Position::square_from_name("b1")) &&
             swappedInPlace.pieces(Color::White, PieceType::Jester) ==
               (Bitboard{1} << Position::square_from_name("a1")),
           "incremental royal-role swap updates stable piece identities and bitboards");
    expect(swappedInPlace.swap_royal_roles(0, 1) &&
             swappedInPlace.upn() == firstRoyal.upn() &&
             swappedInPlace.key() == firstRoyal.key(),
           "royal-role swap is exactly reversible");
    expect(view_key(firstRoyal, ivory) != view_key(swappedRoyal, ivory),
           "a player retains its own concrete King/Jester identity");
    expect(view_key(firstRoyal, disclosedOnyx) !=
             view_key(swappedRoyal, disclosedOnyx),
           "draft/history disclosure makes the enemy King assignment public");

    // Native legal-dot oracle: the mover may select/cancel pieces repeatedly
    // before committing an action. With the Onyx King on e7, capturing the
    // e6 silhouette is displayed when it is the real King (the capture ends
    // play), but omitted when it is the Jester because landing on e6 would be
    // adjacent to the real King on e5. The board presentation remains the
    // same while the mover-private pre-decision observation distinguishes it.
    const Position dotKingE6 = parses(
      "b;king,b,e7;king,w,e6;jester,w,e5");
    const Position dotJesterE6 = parses(
      "b;king,b,e7;jester,w,e6;king,w,e5");
    expect(view_key(dotKingE6, onyx) == view_key(dotJesterE6, onyx),
           "legal-dot witness keeps one ordinary royal-silhouette view");
    expect(dotKingE6.move_from_string("e7-e6").has_value() &&
             !dotJesterE6.move_from_string("e7-e6").has_value(),
           "native King/Jester assignment changes the displayed e7-e6 dot");
    expect(decision_observation_key(dotKingE6, onyx) !=
             decision_observation_key(dotJesterE6, onyx),
           "mover-private legal dots disclose the hidden royal assignment");
    expect(compact_decision_markers(dotKingE6, onyx) !=
             compact_decision_markers(dotJesterE6, onyx),
           "compact legal-dot markers preserve royal disclosure");
    bool rejectedNonMoverDots = false;
    try {
        (void)decision_observation_key(dotKingE6, ivory);
    }
    catch (const std::invalid_argument&) {
        rejectedNonMoverDots = true;
    }
    expect(rejectedNonMoverDots,
           "legal-dot frontier is never exposed to the non-moving observer");

    const Position hiddenC3 = parses(
      "w;king,w,a1;ghost,w,c3,0,0,0,0,0,0,-1,1,-1,0;king,b,h10");
    const Position hiddenF6 = parses(
      "w;king,w,a1;ghost,w,f6,0,0,0,0,0,0,-1,1,-1,0;king,b,h10");
    expect(view_key(hiddenC3, onyx) == view_key(hiddenF6, onyx),
           "an invisible enemy Ghost square is absent from the public view");
    expect(compact_view_key(hiddenC3, onyx) ==
             compact_view_key(hiddenF6, onyx),
           "compact projection conceals an invisible enemy Ghost square");
    expect(view_key(hiddenC3, ivory) != view_key(hiddenF6, ivory),
           "a Ghost owner retains its exact private square");

    const Position penguinGhostF5 = parses(
      "b;king,w,b3;king,b,f2;penguin,w,e5,8,0,0,0,1,1,-1,1,-1,0;"
      "ghost,w,f5,0,0,1,0,1,0,-1,1,-1,0");
    const Position penguinGhostF6 = parses(
      "b;king,w,b3;king,b,f2;penguin,w,e5,32,0,0,0,1,1,-1,1,-1,0;"
      "ghost,w,f6,0,0,1,0,1,0,-1,1,-1,0");
    expect(view_key(penguinGhostF5, onyx) ==
             view_key(penguinGhostF6, onyx) &&
             compact_view_key(penguinGhostF5, onyx) ==
             compact_view_key(penguinGhostF6, onyx),
           "Penguin direction metadata conceals an invisible frozen enemy Ghost");
    expect(view_key(penguinGhostF5, ivory) !=
             view_key(penguinGhostF6, ivory),
           "the Ghost owner retains Penguin-frozen Ghost coordinates");

    Position visibleC3 = hiddenC3;
    Position visibleF6 = hiddenF6;
    visibleC3.piece(1).visible = true;
    visibleF6.piece(1).visible = true;
    expect(view_key(visibleC3, onyx) != view_key(visibleF6, onyx),
           "a visible enemy Ghost square is public");
    Position trackedC3 = visibleC3;
    trackedC3.piece(1).parasiteTracked = true;
    expect(view_key(trackedC3, onyx) != view_key(visibleC3, onyx) &&
             !(compact_view_key(trackedC3, onyx) ==
               compact_view_key(visibleC3, onyx)),
           "a floating Parasite tracker is persistent public Ghost state");

    const Move firstQuiet = require_move(hiddenC3, "c3-d4");
    const Move secondQuiet = require_move(hiddenF6, "f6-e5");
    Position afterFirst = hiddenC3;
    Position afterSecond = hiddenF6;
    Undo firstUndo, secondUndo;
    expect(afterFirst.make_move(firstQuiet, firstUndo) &&
           afterSecond.make_move(secondQuiet, secondUndo),
           "quiet hidden-Ghost observation fixtures apply");
    expect(transition_observation_key(hiddenC3, firstQuiet, afterFirst, onyx) ==
             transition_observation_key(hiddenF6, secondQuiet, afterSecond, onyx),
           "quiet invisible-Ghost observations expose neither endpoint");
    expect(compact_transition_observation_key(
             hiddenC3, firstQuiet, afterFirst, onyx, false) ==
             compact_transition_observation_key(
               hiddenF6, secondQuiet, afterSecond, onyx, false),
           "compact quiet-Ghost observations expose neither endpoint");

    const Move firstVisibleQuiet = require_move(visibleC3, "c3-d4");
    const Move secondVisibleQuiet = require_move(visibleF6, "f6-e5");
    Position afterVisibleFirst = visibleC3;
    Position afterVisibleSecond = visibleF6;
    Undo visibleFirstUndo, visibleSecondUndo;
    expect(afterVisibleFirst.make_move(firstVisibleQuiet, visibleFirstUndo) &&
           afterVisibleSecond.make_move(secondVisibleQuiet, visibleSecondUndo),
           "visible-Ghost observation fixtures apply");
    expect(transition_observation_key(
             visibleC3, firstVisibleQuiet, afterVisibleFirst, onyx) !=
           transition_observation_key(
             visibleF6, secondVisibleQuiet, afterVisibleSecond, onyx),
           "a visible Ghost transition preserves its public source");

    // The visible model animates its direction before fading. Thus the first
    // quiet move from a revealed square exposes both endpoints, even though
    // the resulting ordinary board view conceals the Ghost again.
    const Move visibleToD4 = require_move(visibleC3, "c3-d4");
    const Move visibleToC4 = require_move(visibleC3, "c3-c4");
    Position afterVisibleD4 = visibleC3;
    Position afterVisibleC4 = visibleC3;
    Undo visibleD4Undo, visibleC4Undo;
    expect(afterVisibleD4.make_move(visibleToD4, visibleD4Undo) &&
           afterVisibleC4.make_move(visibleToC4, visibleC4Undo),
           "same-source visible-Ghost quiet fixtures apply");
    expect(transition_observation_key(
             visibleC3, visibleToD4, afterVisibleD4, onyx) !=
           transition_observation_key(
             visibleC3, visibleToC4, afterVisibleC4, onyx),
           "a revealed Ghost's quiet destination remains known from its animation");
    expect(!(compact_transition_observation_key(
             visibleC3, visibleToD4, afterVisibleD4, onyx, false) ==
             compact_transition_observation_key(
               visibleC3, visibleToC4, afterVisibleC4, onyx, false)),
           "compact observations retain a revealed Ghost's movement direction");
    const std::string visibleDirection = transition_observation_key(
      visibleC3, visibleToD4, afterVisibleD4, onyx);
    expect(visibleDirection.find("|from=c3|to=d4|") != std::string::npos,
           "the revealed-Ghost transition spells both public endpoints");

    Position afterPublicReply = afterVisibleD4;
    Undo publicReply;
    expect(afterPublicReply.make_move(
             require_move(afterPublicReply, "h10-h9"), publicReply),
           "opponent reply reaches the next hidden-Ghost turn");
    PublicBeliefState knownHidden(onyx);
    std::string knownError;
    expect(knownHidden.add(afterPublicReply, &knownError) &&
             knownHidden.size() == 1 && knownHidden.piece_location_known(1),
           "a Ghost that just faded still has one publicly known square: " +
             knownError);
    const BeliefSuccessorPartitions hiddenAgain =
      knownHidden.adversarial_successor_partitions(false);
    expect(std::any_of(hiddenAgain.buckets.begin(), hiddenAgain.buckets.end(),
             [](const BeliefSuccessorBucket& bucket) {
                 return bucket.actions.size() > 1 && bucket.worlds.size() > 1;
             }),
           "only the following move begun while hidden re-expands Ghost locations");

    const Position hiddenCaptureLeft = parses(
      "w;king,w,a1;ghost,w,d4,0,0,0,0,1,0,-1,1,-1,0;"
      "king,b,h10;rook,b,e5");
    const Position hiddenCaptureRight = parses(
      "w;king,w,a1;ghost,w,f4,0,0,0,0,1,0,-1,1,-1,0;"
      "king,b,h10;rook,b,e5");
    const Move captureFromLeft = require_move(hiddenCaptureLeft, "d4-e5");
    const Move captureFromRight = require_move(hiddenCaptureRight, "f4-e5");
    Position afterCaptureLeft = hiddenCaptureLeft;
    Position afterCaptureRight = hiddenCaptureRight;
    Undo captureLeftUndo, captureRightUndo;
    expect(afterCaptureLeft.make_move(captureFromLeft, captureLeftUndo) &&
           afterCaptureRight.make_move(captureFromRight, captureRightUndo),
           "hidden-Ghost reveal-on-capture fixtures apply");
    const std::string captureObservation = transition_observation_key(
      hiddenCaptureLeft, captureFromLeft, afterCaptureLeft, onyx);
    expect(captureObservation == transition_observation_key(
             hiddenCaptureRight, captureFromRight, afterCaptureRight, onyx),
           "a hidden Ghost attack conceals its source and exposes its destination");
    expect(captureObservation.find("|from=?|to=e5|") != std::string::npos,
           "the hidden-Ghost capture observation spells the public reveal boundary");

    const Move firstRoyalMove = require_move(firstRoyal, "a1-a2");
    const Move swappedRoyalMove = require_move(swappedRoyal, "a1-a2");
    Position afterFirstRoyal = firstRoyal;
    Position afterSwappedRoyal = swappedRoyal;
    Undo firstRoyalUndo, swappedRoyalUndo;
    expect(afterFirstRoyal.make_move(firstRoyalMove, firstRoyalUndo) &&
           afterSwappedRoyal.make_move(swappedRoyalMove, swappedRoyalUndo),
           "royal-silhouette action fixtures apply");
    expect(transition_observation_key(
             firstRoyal, firstRoyalMove, afterFirstRoyal, onyx) ==
           transition_observation_key(
             swappedRoyal, swappedRoyalMove, afterSwappedRoyal, onyx),
           "a royal action does not disclose whether its actor is King or Jester");

    const Position kingOnTarget = parses(
      "b;king,w,c3;jester,w,d3;king,b,h10;rook,b,c10");
    const Position jesterOnTarget = parses(
      "b;jester,w,c3;king,w,d3;king,b,h10;rook,b,c10");
    expect(view_key(kingOnTarget, onyx) == view_key(jesterOnTarget, onyx),
           "a capturable royal target begins as one Onyx observation");
    const Move captureKing = require_move(kingOnTarget, "c10-c3");
    const Move captureJester = require_move(jesterOnTarget, "c10-c3");
    Position afterKingCapture = kingOnTarget;
    Position afterJesterCapture = jesterOnTarget;
    Undo kingCaptureUndo, jesterCaptureUndo;
    expect(afterKingCapture.make_move(captureKing, kingCaptureUndo) &&
           afterJesterCapture.make_move(captureJester, jesterCaptureUndo),
           "royal-capture terminal-observation fixtures apply");
    expect(afterKingCapture.game_over() && !afterJesterCapture.game_over(),
           "capturing the King ends play while capturing its Jester does not");
    expect(view_key(afterKingCapture, onyx) != view_key(afterJesterCapture, onyx),
           "the public terminal announcement distinguishes royal-capture outcomes");
    expect(transition_observation_key(
             kingOnTarget, captureKing, afterKingCapture, onyx) !=
           transition_observation_key(
             jesterOnTarget, captureJester, afterJesterCapture, onyx),
           "continuation after a royal capture eliminates the King-on-target world");
    expect(!(compact_transition_observation_key(
               kingOnTarget, captureKing, afterKingCapture, onyx, false) ==
             compact_transition_observation_key(
               jesterOnTarget, captureJester, afterJesterCapture, onyx, false)),
           "compact terminal observations split King and Jester captures");

    // Exact K+Jester-v-K information index 492966: capturing a1 wins if a1 is
    // the King, but that dot is absent if a1 is the Jester because the Black
    // King would land next to the real King on a2. The v2 information model
    // splits these worlds before Black chooses; they are not a uniform-action
    // pair or a soft lock.
    const Position royalA1 = parses(
      "b;king,w,a1;king,b,b1;jester,w,a2");
    const Position royalA2 = parses(
      "b;jester,w,a1;king,b,b1;king,w,a2");
    expect(view_key(royalA1, onyx) == view_key(royalA2, onyx),
           "nonuniform-legality witness starts in one public royal view");
    const auto action_strings = [](const Position& position) {
        std::set<std::string> actions;
        for (const Move& move : position.legal_moves())
            actions.insert(position.move_to_string(move));
        return actions;
    };
    const auto actualActions = action_strings(royalA1);
    const auto swappedActions = action_strings(royalA2);
    expect(actualActions.count("b1-a1") == 1 &&
             swappedActions.count("b1-a1") == 0,
           "a hidden royal assignment can remove a concretely winning action");
    expect(decision_observation_key(royalA1, onyx) !=
             decision_observation_key(royalA2, onyx),
           "index 492966 is split by the mover's private legal dots");

    const Position onlyCaptureA1 = parses(
      "b;king,w,a1;king,b,b1;jester,w,b2");
    const Position onlyCaptureB2 = parses(
      "b;jester,w,a1;king,b,b1;king,w,b2");
    expect(view_key(onlyCaptureA1, onyx) == view_key(onlyCaptureB2, onyx),
           "former soft-lock witness starts in one ordinary public view");
    const auto captureA1Actions = action_strings(onlyCaptureA1);
    const auto captureB2Actions = action_strings(onlyCaptureB2);
    expect(!captureA1Actions.empty() && !captureB2Actions.empty() &&
             decision_observation_key(onlyCaptureA1, onyx) !=
               decision_observation_key(onlyCaptureB2, onyx),
           "private legal dots eliminate the obsolete uniform-action soft lock");
}

}  // namespace

int main(int argc, char** argv) {
    bool externalTablebases = true;
    for (int index = 1; index < argc; ++index) {
        if (std::string_view(argv[index]) == "--skip-external-tablebases")
            externalTablebases = false;
        else {
            std::cerr << "unknown test option: " << argv[index] << '\n';
            return EXIT_FAILURE;
        }
    }
    test_roster_and_position_round_trip();
    test_chess_style_move_notation();
    test_bomb_and_undo();
    test_bomb_check_legality();
    test_native_castling();
    test_ninja_and_mage();
    test_checker_king_long_diagonals();
    test_stale_checker_tablebase_codec();
    test_checker_chain_and_prince_turns();
    test_sludge_and_victory();
    test_giant_rechecks_angel_rescue_footprint();
    test_parasite_goop_and_angel_interactions();
    test_native_giant_and_copycat_footprints();
    test_native_fisherman_rays();
    test_ghost_visibility_transitions();
    test_search_and_perft_regressions();
    if (externalTablebases)
        test_exact_tablebase_probing();
    test_native_information_set_search();
    test_public_belief_state_core();
    test_public_history_reconstruction();
    test_belief_terminal_and_observer_scoring();
    test_native_terminal_reasons();
    test_turn_start_check();
    test_native_insufficient_material();
    test_pawn_en_passant_lifetime();
    test_cooldowns_minions_and_freeze_stacking();
    test_sniper_berserker_and_dragon();
    test_safe_ordinary_static_exchange();
    test_turn_boundary_reachability();
    test_public_information_projection();
    test_native_draft_windows_and_costs();
    if (failures) {
        std::cerr << failures << " Ultimate rules test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "Ultimate rules reference tests passed\n";
    return EXIT_SUCCESS;
}
