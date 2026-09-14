/* Ultimate Fish tablebase rules compatibility, GPLv3 or later. */
#ifndef ULTIMATE_TABLEBASE_RULES_REVISION_H
#define ULTIMATE_TABLEBASE_RULES_REVISION_H

#include "position.h"
#include <cstdint>

namespace Stockfish::Ultimate {
// UFTB v12 retains the 64-byte header and authenticates both the underlying
// layout tag and the September 2026 Checker/ChangeTurn rules. Zero denotes
// the ordinary dense layout; special layouts retain their own tag via XOR.
constexpr std::uint64_t CorrectedRulesTag = 0x3132393036524655ULL;
constexpr bool corrected_tablebase_rules(PieceType piece) {
    return piece == PieceType::Checker || piece == PieceType::CheckerKing ||
      piece == PieceType::Sniper || piece == PieceType::Devil;
}
}
#endif
