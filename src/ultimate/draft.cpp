/* Ultimate Fish drafting state, GPLv3 or later. */

#include "draft.h"

#include <algorithm>
#include <iterator>
#include <limits>

namespace Stockfish::Ultimate {
DraftWindow DraftState::window() const {
    if (complete())
        return {phase_, Color::White, DraftAction::Complete, 0, 0, false};
    const Color player = (phase_ & 1) ? Color::Black : Color::White;
    if (phase_ == 0 || phase_ == 1 || phase_ == 4 || phase_ == 5 || phase_ == 8 || phase_ == 9)
        return {phase_, player, DraftAction::Ban, 0, 0, false};
    // The native constraints apply to each newly placed, immutable group while
    // the upper bound is cumulative. SetTotalMaxPoints(100, draft=true)
    // recovers INIT_MIN=15 and AMT_TO_ADD_PER_DRAFT=40. Consequently both
    // opening groups add at least 15 under a 40-point cap; Ivory's middle group
    // adds 15 under 80, while Onyx's adds 40 under 90. The final group has no
    // required addition and only the global 100-point ceiling.
    const int locked = lockedPoints_[index(player)];
    if (phase_ == 2 || phase_ == 3)
        return {phase_, player, DraftAction::Pick, locked + InitialMinimum, 40, false};
    if (phase_ == 6)
        return {phase_, player, DraftAction::Pick, locked + InitialMinimum, 80, false};
    if (phase_ == 7)
        return {phase_, player, DraftAction::Pick, locked + DraftIncrement, 90, false};
    return {phase_, player, DraftAction::Pick, locked, TotalPoints, true};
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
    const auto firstPending = pieces.begin() + static_cast<std::ptrdiff_t>(lockedSizes_[index(current.player)]);
    const auto found = std::find(std::make_reverse_iterator(pieces.end()),
                                 std::make_reverse_iterator(firstPending), type);
    if (found == std::make_reverse_iterator(firstPending) || type == PieceType::King)
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
    const DraftWindow current = window();
    if (pendingBan_) {
        banned_[static_cast<std::size_t>(*pendingBan_)] = true;
        pendingBan_.reset();
    }
    else {
        const Color player = current.player;
        lockedSizes_[index(player)] = teams_[index(player)].size();
        lockedPoints_[index(player)] = points(player);
    }
    ++phase_;
    return true;
}

std::optional<PieceType> DraftState::suggest(const std::vector<PieceType>& excluded) const {
    const auto choices = legal_choices();
    const auto allowed = [&](PieceType type) {
        return std::find(excluded.begin(), excluded.end(), type) == excluded.end();
    };
    if (std::none_of(choices.begin(), choices.end(), allowed))
        return std::nullopt;
    const DraftWindow current = window();
    // The 2026-08-07 native-conditional CopyCat league evolved independent
    // pick, repeat, and global-ban weights. Its strongest generation scored
    // 18W-11D-1L and the deeper finalist pool converged on the same roster
    // later validated 3/3 in live Unranked play. Native costs remain the
    // legality constraint; these values express roster interaction.
    static constexpr std::array<int, static_cast<std::size_t>(PieceType::Count)> Pick = {{
      0, 1068, 267, -411, 1663, 1095, 702, 134, 827, 1274,
      -1148, 632, 1557, 0, 1210, 866, -543, 0, 39, 1014,
      2218, -350, 0, -596, 775, 0, 943, 0, 701, -112,
    }};
    static constexpr std::array<int, static_cast<std::size_t>(PieceType::Count)> Repeat = {{
      0, 298, -218, -234, 55, 46, 214, 132, 247, -368,
      301, 100, 169, 0, 331, 300, -217, 0, 225, -133,
      -28, 289, 0, 123, 194, 0, 229, 0, -318, 163,
    }};
    static constexpr std::array<int, static_cast<std::size_t>(PieceType::Count)> Ban = {{
      0, 1358, -2, 635, 1240, 380, 154, 678, 1539, -456,
      -910, 1001, 1901, 0, 452, 240, -248, 0, 337, 937,
      2024, -291, 0, -502, 344, 0, 342, 0, 439, -190,
    }};
    constexpr int OpponentDeny = -89;
    constexpr int SelfPreserve = 364;
    constexpr int FinalPenguin = 103;
    // Once this player's opening group has been revealed, the opponent has
    // already learned which royal is the fixed King. A newly drafted Jester
    // can no longer create royal-location ambiguity, so do not spend later
    // immutable groups on that now-redundant deception piece.
    constexpr int RevealedKingJester = -100000;
    int bestScore = std::numeric_limits<int>::min();
    PieceType best = choices.front();
    for (const PieceType type : choices) {
        if (!allowed(type))
            continue;
        const std::size_t typeIndex = static_cast<std::size_t>(type);
        const int copies = static_cast<int>(std::count(team(current.player).begin(),
                                                        team(current.player).end(), type));
        int score = Pick[typeIndex] + Repeat[typeIndex] * copies;
        if (current.action == DraftAction::Ban) {
            const Color opponent = current.player == Color::White ? Color::Black : Color::White;
            const int opponentCopies = static_cast<int>(std::count(
              team(opponent).begin(), team(opponent).end(), type));
            score = Ban[typeIndex] + OpponentDeny * opponentCopies - SelfPreserve * copies;
        }
        else if (type == PieceType::Jester && lockedSizes_[index(current.player)] > 1)
            score += RevealedKingJester;
        else if (current.lastPick && type == PieceType::Penguin)
            score += FinalPenguin;
        if (score > bestScore) {
            bestScore = score;
            best = type;
        }
    }
    return best;
}

bool DraftState::autoplay(std::vector<PieceType>& choices, std::string* error,
                          const std::vector<PieceType>& excluded) {
    choices.clear();
    if (complete()) {
        if (error)
            *error = "draft is already complete";
        return false;
    }
    const DraftWindow current = window();
    if (current.action == DraftAction::Ban) {
        const auto choice = suggest(excluded);
        if (!choice || !choose(*choice, error))
            return false;
        choices.push_back(*choice);
        return commit(error);
    }

    // Lock as much legal playing strength as the current native cap permits
    // before the following ban. A choice that cannot fill the final remainder
    // is skipped naturally when legal_choices() excludes it.
    const int target = current.maximumPoints;
    // The legal-choice filter enforces the native 100-point budget and finite
    // 8x3 deployment capacity, including multi-cell pairs.
    while (points(current.player) < target) {
        const auto choice = suggest(excluded);
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
