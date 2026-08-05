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
    // Ranked pick windows are cumulative material *floors*, not ceilings.
    // The shipping UI names the remaining value ``minpts`` and permits a
    // player to protect more than the current milestone before the next ban.
    // The live 5.731 client, for example, accepted a 27-point opening group in
    // the 15-point window.  Only the army's global 100-point budget is a cap.
    if (phase_ == 2 || phase_ == 3)
        return {phase_, player, DraftAction::Pick, 15, TotalPoints, false};
    if (phase_ == 6)
        return {phase_, player, DraftAction::Pick, 30, TotalPoints, false};
    if (phase_ == 7)
        return {phase_, player, DraftAction::Pick, 40, TotalPoints, false};
    return {phase_, player, DraftAction::Pick, TotalPoints, TotalPoints, true};
}

const std::vector<PieceType>& DraftState::team(Color color) const { return teams_[index(color)]; }

int DraftState::points(Color color) const {
    int total = 0;
    for (const PieceType type : team(color))
        total += Position::info(type).draftCost;
    return total;
}

int DraftState::deployment_slots(Color color) const {
    int total = 0;
    for (const PieceType type : team(color))
        total += type == PieceType::Giant ? 4 : type == PieceType::Copycat ? 2 : 1;
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
        else {
            const int footprint = type == PieceType::Giant ? 4 : type == PieceType::Copycat ? 2 : 1;
            if (points(current.player) + info.draftCost <= current.maximumPoints &&
                deployment_slots(current.player) + footprint <= 24)
            choices.push_back(type);
        }
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
    // Search-derived draft priors. They deliberately are not a cost table:
    // native costs already constrain the choice and these values express
    // tactical reach, forced-action potential, king pressure, and synergy.
    static constexpr std::array<int, static_cast<std::size_t>(PieceType::Count)> DraftStrength = {{
      0, 680, 430, 300, 880, 700, 540, 820, 860, 980,
      310, 900, 470, 0, 790, 800, 760, 0, 560, 920,
      940, 190, 220, 120, 720, 0, 700, 0, 610, 900,
    }};
    int bestScore = std::numeric_limits<int>::min();
    PieceType best = choices.front();
    for (const PieceType type : choices) {
        const int strength = DraftStrength[static_cast<std::size_t>(type)];
        const int copies = static_cast<int>(std::count(team(current.player).begin(),
                                                        team(current.player).end(), type));
        // The draft window already constrains cost. Ranking raw playing
        // strength avoids exhausting the 24-cell deployment zone with cheap
        // Giants before a required point minimum can be reached.
        int score = strength;
        if (current.action == DraftAction::Pick) {
            // Deterministic draft-roster matches showed a large late-window
            // edge for a Penguin wall: it swept the former mixed final four
            // as both colors at 10k and 30k nodes per move.  Preserve early
            // tactical diversity, but exploit that measured stacking synergy
            // in the unrestricted final window.
            if (current.lastPick && type == PieceType::Penguin)
                score += 500;
            else
                score -= copies * 90;
            if (type == PieceType::Jester && copies == 0)
                score += 180;
            if (type == PieceType::Angel && team(current.player).size() > 2)
                score += 90;
            if (type == PieceType::Mage &&
                std::find(team(current.player).begin(), team(current.player).end(), PieceType::Giant)
                  != team(current.player).end())
                score += 80;
        }
        if (score > bestScore) {
            bestScore = score;
            best = type;
        }
    }
    return best;
}

bool DraftState::autoplay(std::vector<PieceType>& choices, std::string* error) {
    choices.clear();
    if (complete()) {
        if (error)
            *error = "draft is already complete";
        return false;
    }
    const DraftWindow current = window();
    if (current.action == DraftAction::Ban) {
        const auto choice = suggest();
        if (!choice || !choose(*choice, error))
            return false;
        choices.push_back(*choice);
        return commit(error);
    }

    // Early picks may intentionally exceed the current floor so valuable
    // types are locked before the next ban.  Protect a strong 15-point piece
    // plus the Jester in the opening group, then satisfy the later cumulative
    // milestones without prematurely exhausting the full roster budget.
    const int target = current.lastPick ? current.maximumPoints
                     : current.phase <= 3 ? current.minimumPoints + 10
                                          : current.minimumPoints;
    // The legal-choice filter enforces the native 100-point budget and finite
    // 8x3 deployment capacity, including multi-cell pairs.
    while (points(current.player) < target) {
        const auto choice = suggest();
        if (!choice || !choose(*choice, error))
            break;
        choices.push_back(*choice);
    }
    if (!can_commit()) {
        if (error && error->empty())
            *error = "AI could not satisfy this draft window";
        return false;
    }
    return commit(error);
}

}  // namespace Stockfish::Ultimate
