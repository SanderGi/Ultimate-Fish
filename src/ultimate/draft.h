/* Ultimate Fish drafting state, GPLv3 or later. */

#ifndef ULTIMATE_DRAFT_H_INCLUDED
#define ULTIMATE_DRAFT_H_INCLUDED

#include "position.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace Stockfish::Ultimate {

enum class DraftAction : std::uint8_t { Ban, Pick, Complete };

struct DraftWindow {
    int phase;
    Color player;
    DraftAction action;
    int minimumPoints;
    int maximumPoints;
    bool lastPick;
};

class DraftState {
   public:
    static constexpr int PhaseCount = 12;
    static constexpr int TotalPoints = 100;
    static constexpr int InitialMinimum = 15;
    static constexpr int DraftIncrement = 40;

    [[nodiscard]] int phase() const { return phase_; }
    [[nodiscard]] bool complete() const { return phase_ >= PhaseCount; }
    [[nodiscard]] DraftWindow window() const;
    [[nodiscard]] const std::vector<PieceType>& team(Color color) const;
    [[nodiscard]] int points(Color color) const;
    [[nodiscard]] int deployment_slots(Color color) const;
    [[nodiscard]] bool banned(PieceType type) const;
    [[nodiscard]] std::vector<PieceType> legal_choices() const;
    [[nodiscard]] bool can_commit() const;

    bool choose(PieceType type, std::string* error = nullptr);
    bool unchoose(PieceType type);
    bool commit(std::string* error = nullptr);
    [[nodiscard]] std::optional<PieceType> suggest() const;
    bool autoplay(std::vector<PieceType>& choices, std::string* error = nullptr);

   private:
    static std::size_t index(Color color) { return static_cast<std::size_t>(color); }

    int phase_ = 0;
    std::array<std::vector<PieceType>, 2> teams_{{{PieceType::King}, {PieceType::King}}};
    std::array<std::size_t, 2> lockedSizes_{{1, 1}};
    std::array<int, 2> lockedPoints_{{0, 0}};
    std::array<bool, static_cast<std::size_t>(PieceType::Count)> banned_{};
    std::optional<PieceType> pendingBan_;
};

}  // namespace Stockfish::Ultimate

#endif
