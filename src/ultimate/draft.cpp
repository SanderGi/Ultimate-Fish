/* Ultimate Fish drafting state, GPLv3 or later. */

#include "draft.h"

#include <algorithm>
#include <limits>

namespace Stockfish::Ultimate {
DraftWindow DraftState::window() const {
    if (complete())
        return {phase_, Color::White, DraftAction::Complete, 0, 0, false};
    const Color player = (phase_ & 1) ? Color::Black : Color::White;
    if (phase_ == 0 || phase_ == 1 || phase_ == 4 || phase_ == 5 || phase_ == 8 || phase_ == 9)
        return {phase_, player, DraftAction::Ban, 0, 0, false};
    if (phase_ == 2 || phase_ == 3)
        return {phase_, player, DraftAction::Pick, 15, 15, false};
    if (phase_ == 6)
        return {phase_, player, DraftAction::Pick, 15, 30, false};
    if (phase_ == 7)
        return {phase_, player, DraftAction::Pick, 15, 40, false};
    return {phase_, player, DraftAction::Pick, 0, TotalPoints, true};
}

const std::vector<PieceType>& DraftState::team(Color color) const { return teams_[index(color)]; }

int DraftState::points(Color color) const {
    int total = 0;
    for (const PieceType type : team(color))
        total += Position::info(type).draftCost;
    return total;
}

bool DraftState::banned(PieceType type) const { return banned_[static_cast<std::size_t>(type)]; }

std::vector<PieceType> DraftState::legal_choices() const {
    std::vector<PieceType> choices;
    if (complete())
        return choices;
    const DraftWindow current = window();
    for (int value = 0; value < static_cast<int>(PieceType::Count); ++value) {
        const PieceType type = static_cast<PieceType>(value);
        const PieceInfo& info = Position::info(type);
        if (!info.selectable || banned(type))
            continue;
        if (current.action == DraftAction::Ban) {
            if (!pendingBan_)
                choices.push_back(type);
        }
        else if (points(current.player) + info.draftCost <= current.maximumPoints)
            choices.push_back(type);
    }
    return choices;
}

bool DraftState::choose(PieceType type, std::string* error) {
    const auto choices = legal_choices();
    if (std::find(choices.begin(), choices.end(), type) == choices.end()) {
        if (error)
            *error = "piece is not legal in the current draft window";
        return false;
    }
    const DraftWindow current = window();
    if (current.action == DraftAction::Ban)
        pendingBan_ = type;
    else
        teams_[index(current.player)].push_back(type);
    return true;
}

bool DraftState::unchoose(PieceType type) {
    if (complete())
        return false;
    const DraftWindow current = window();
    if (current.action == DraftAction::Ban) {
        if (pendingBan_ == type) {
            pendingBan_.reset();
            return true;
        }
        return false;
    }
    auto& pieces = teams_[index(current.player)];
    const auto found = std::find(pieces.rbegin(), pieces.rend(), type);
    if (found == pieces.rend() || type == PieceType::King)
        return false;
    pieces.erase(std::next(found).base());
    return true;
}

bool DraftState::can_commit() const {
    if (complete())
        return false;
    const DraftWindow current = window();
    if (current.action == DraftAction::Ban)
        return pendingBan_.has_value();
    const int total = points(current.player);
    return total >= current.minimumPoints && total <= current.maximumPoints;
}

bool DraftState::commit(std::string* error) {
    if (!can_commit()) {
        if (error)
            *error = "draft window has not satisfied its native point/selection constraint";
        return false;
    }
    if (pendingBan_) {
        banned_[static_cast<std::size_t>(*pendingBan_)] = true;
        pendingBan_.reset();
    }
    ++phase_;
    return true;
}

std::optional<PieceType> DraftState::suggest() const {
    const auto choices = legal_choices();
    if (choices.empty())
        return std::nullopt;
    const DraftWindow current = window();
    int bestScore = std::numeric_limits<int>::min();
    PieceType best = choices.front();
    for (const PieceType type : choices) {
        const int strength = Position::material_value(type);
        const int cost = std::max(1, int(Position::info(type).draftCost));
        int score = current.action == DraftAction::Ban ? strength : (strength * 16) / cost;
        if (type == PieceType::Jester && current.action == DraftAction::Pick)
            score += 120;
        if (score > bestScore) {
            bestScore = score;
            best = type;
        }
    }
    return best;
}

}  // namespace Stockfish::Ultimate
