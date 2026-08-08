/* Ultimate Fish exact endgame tablebase probing, GPLv3 or later. */

#ifndef ULTIMATE_TABLEBASE_PROBE_H_INCLUDED
#define ULTIMATE_TABLEBASE_PROBE_H_INCLUDED

#include "position.h"

#include <cstdint>
#include <optional>

namespace Stockfish::Ultimate {

enum class TablebaseWdl : std::uint8_t { Win = 1, Loss = 2, Draw = 3 };

struct TablebaseResult {
    TablebaseWdl wdl;
    std::uint16_t dtw;
};

class TablebaseProbe {
   public:
    static void preload();
    [[nodiscard]] static std::optional<TablebaseResult> probe(const Position& position);
};

}  // namespace Stockfish::Ultimate

#endif
