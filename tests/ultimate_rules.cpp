#include "../src/ultimate/position.h"
#include "../src/ultimate/draft.h"
#include "../src/ultimate/search.h"

#include <cstdlib>
#include <iostream>
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
    // King; castling itself ignores this cooldown and still drags the Rook.
    enemyRook.piece(foreignRook).cooldown = 2;
    const auto foreignCastle = enemyRook.move_from_string("d2-f2");
    expect(foreignCastle && foreignCastle->kind == MoveKind::Castle &&
           foreignCastle->auxiliary == foreignRook,
           "native castle scan does not filter a Rook by team");
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
           "copycat clone follows the horizontally mirrored displacement");
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
           "enemy king or jester adjacency reveals a ghost");

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

    Position hiddenRay;
    hiddenRay.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    hiddenRay.add_piece(PieceType::King, Color::Black, Position::square_from_name("h10"));
    hiddenRay.add_piece(PieceType::Rook, Color::White, Position::square_from_name("c4"));
    const int rayGhost = hiddenRay.add_piece(PieceType::Ghost, Color::Black,
                                             Position::square_from_name("d4"));
    hiddenRay.piece(rayGhost).visible = false;
    expect(!hiddenRay.move_from_string("c4-d4").has_value() &&
           hiddenRay.move_from_string("c4-e4").has_value(),
           "an unseen opposing ghost is unavailable but transparent to a sliding ray");

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
    expect(fixture.perft(1) == 44 && fixture.perft(2) == 1936 && fixture.perft(3) == 74088,
           "mixed-roster perft remains stable at depths one through three");
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

    Search informationSet(2);
    const BeliefSearchResult robust = informationSet.think_beliefs({unsafe, safe}, limits);
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
    Search forcedWin(2);
    const BeliefSearchResult mate = forcedWin.think_beliefs(
      {adjacentOne, adjacentTwo}, limits);
    expect(mate.bestMove && *mate.bestMove == "h9-h10" && mate.score >= 29900,
           "Ghost uncertainty never hard-filters a high-value move that captures the real king");
}

void test_native_insufficient_material() {
    Position bare;
    bare.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    bare.add_piece(PieceType::King, Color::Black, Position::square_from_name("h8"));
    expect(!bare.is_checkmate_possible() && bare.game_over() && !bare.winner(),
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
}

void test_cooldowns_minions_and_freeze_stacking() {
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

    Position hiddenLeap;
    hiddenLeap.add_piece(PieceType::King, Color::White, Position::square_from_name("a1"));
    hiddenLeap.add_piece(PieceType::King, Color::Black, Position::square_from_name("h10"));
    hiddenLeap.add_piece(PieceType::Knight, Color::White, Position::square_from_name("d4"));
    const int hiddenGhost = hiddenLeap.add_piece(PieceType::Ghost, Color::Black,
                                                  Position::square_from_name("f5"));
    hiddenLeap.piece(hiddenGhost).visible = false;
    expect(!hiddenLeap.move_from_string("d4-f5").has_value(),
           "ordinary leapers cannot deliberately target an unrevealed Ghost");

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
    for (int phase = 0; phase < DraftState::PhaseCount; ++phase) {
        expect(automatic.autoplay(automaticChoices, &error),
               "draft AI satisfies every ranked window: " + error);
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
           "draft AI completes all twelve ranked windows at the full budget");
}

}  // namespace

int main() {
    test_roster_and_position_round_trip();
    test_bomb_and_undo();
    test_bomb_check_legality();
    test_native_castling();
    test_ninja_and_mage();
    test_checker_chain_and_prince_turns();
    test_sludge_and_victory();
    test_parasite_goop_and_angel_interactions();
    test_native_giant_and_copycat_footprints();
    test_native_fisherman_rays();
    test_ghost_visibility_transitions();
    test_search_and_perft_regressions();
    test_native_information_set_search();
    test_native_insufficient_material();
    test_pawn_en_passant_lifetime();
    test_cooldowns_minions_and_freeze_stacking();
    test_sniper_berserker_and_dragon();
    test_native_draft_windows_and_costs();
    if (failures) {
        std::cerr << failures << " Ultimate rules test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "Ultimate rules reference tests passed\n";
    return EXIT_SUCCESS;
}
