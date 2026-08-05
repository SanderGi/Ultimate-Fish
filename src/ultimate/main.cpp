/*
  Ultimate Fish - Chess Ultimate 5.731 command-line engine
  GPLv3 or later
*/

#include "position.h"
#include "search.h"
#include "draft.h"

#include <charconv>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace Stockfish::Ultimate;

namespace {

bool parse_int(const std::string& text, int& value) {
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size();
}

void print_position(const Position& position) {
    std::cout << "upn " << position.upn() << '\n';
    std::cout << "moves";
    for (const Move& move : position.legal_moves())
        std::cout << ' ' << position.move_to_string(move);
    std::cout << '\n';
    std::cout << "material white " << position.material_points(Color::White)
              << " black " << position.material_points(Color::Black) << '\n';
    if (!position.game_over())
        std::cout << "result ongoing\n";
    else if (const auto winner = position.winner()) {
        const bool kingCaptured = !position.has_real_king(Color::White) ||
                                  !position.has_real_king(Color::Black);
        std::cout << "result " << (*winner == Color::White ? "white" : "black")
                  << (kingCaptured ? " king-captured\n" : " checkmate\n");
    }
    else if (position.has_real_king(Color::White) && position.has_real_king(Color::Black))
        std::cout << "result draw insufficient-material\n";
    else
        std::cout << "result draw simultaneous-king-capture\n";
}

void print_score(int score) {
    if (std::abs(score) >= 30000 - 128) {
        const int plies = 30000 - std::abs(score);
        const int moves = std::max(1, (plies + 1) / 2);
        std::cout << "mate " << (score < 0 ? -moves : moves);
    }
    else
        std::cout << "cp " << score;
}

SearchLimits parse_limits(std::istringstream& input, const Position* position = nullptr) {
    SearchLimits limits;
    std::string token;
    while (input >> token) {
        if (token == "drawmoves") {
            std::string notation;
            while (input >> notation)
                limits.rootDrawMoveStrings.push_back(notation);
            break;
        }
        if (token == "searchmoves" && position) {
            std::string notation;
            while (input >> notation)
                if (const auto move = position->move_from_string(notation))
                    limits.rootMoves.push_back(*move);
            break;
        }
        std::string value;
        if (!(input >> value))
            break;
        int parsed = 0;
        if (!parse_int(value, parsed))
            continue;
        if (token == "depth")
            limits.depth = parsed;
        else if (token == "movetime")
            limits.moveTime = std::chrono::milliseconds(parsed);
        else if (token == "nodes")
            limits.nodes = static_cast<std::uint64_t>(std::max(0, parsed));
    }
    return limits;
}

}  // namespace

int main() {
    std::cout.setf(std::ios::unitbuf);
    Position position;
    Search search;
    DraftState draft;
    std::vector<Position> beliefs;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit")
            break;
        if (line == "uci") {
            std::cout << "id name Ultimate Fish 0.1\n"
                         "id author Ultimate Fish contributors\n"
                         "option name Hash type spin default 64 min 1 max 4096\n"
                         "uciok\n";
            continue;
        }
        if (line == "isready") {
            std::cout << "readyok\n";
            continue;
        }
        if (line == "ucinewgame") {
            position.clear();
            beliefs.clear();
            search.clear();
            continue;
        }
        if (line == "belief clear") {
            beliefs.clear();
            std::cout << "beliefok\n";
            continue;
        }
        if (line == "belief count") {
            std::cout << "beliefcount " << beliefs.size() << '\n';
            continue;
        }
        if (line.rfind("belief add ", 0) == 0) {
            Position belief;
            std::string error;
            if (!belief.set_upn(line.substr(11), &error))
                std::cout << "info string invalid belief " << error << '\n';
            else if (!beliefs.empty() && belief.side_to_move() != beliefs.front().side_to_move())
                std::cout << "info string invalid belief side-to-move differs\n";
            else {
                beliefs.push_back(std::move(belief));
                std::cout << "beliefok\n";
            }
            continue;
        }
        if (line == "belief go" || line.rfind("belief go ", 0) == 0) {
            std::istringstream input(line);
            std::string token;
            input >> token >> token;
            const SearchLimits limits = parse_limits(input);
            const BeliefSearchResult result = search.think_beliefs(beliefs, limits);
            std::cout << "info depth " << result.completedDepth << " score ";
            print_score(result.score);
            std::cout << " nodes " << result.nodes << " time " << result.elapsed.count()
                      << " beliefs " << result.beliefs << " deepbeliefs " << result.deepBeliefs
                      << " common " << result.commonMoves << " candidates " << result.candidates
                      << " beliefworst " << result.worstScore
                      << " beliefmean " << result.meanScore << " pv";
            for (const std::string& move : result.principalVariation)
                std::cout << ' ' << move;
            std::cout << '\n';
            std::cout << "bestmove " << (result.bestMove ? *result.bestMove : "(none)") << '\n';
            continue;
        }
        if (line == "draft new") {
            draft = DraftState{};
            std::cout << "draftok\n";
            continue;
        }
        if (line == "draft status") {
            const DraftWindow window = draft.window();
            std::cout << "draft phase " << window.phase << " player "
                      << (window.player == Color::White ? "white" : "black") << " action "
                      << (window.action == DraftAction::Ban ? "ban" :
                          window.action == DraftAction::Pick ? "pick" : "complete")
                      << " min " << window.minimumPoints << " max " << window.maximumPoints
                      << " white " << draft.points(Color::White)
                      << " black " << draft.points(Color::Black) << '\n';
            continue;
        }
        if (line == "draft suggest") {
            const auto suggestion = draft.suggest();
            std::cout << "draftsuggest "
                      << (suggestion ? Position::type_name(*suggestion) : "(none)") << '\n';
            continue;
        }
        if (line == "draft auto") {
            std::vector<PieceType> choices;
            std::string error;
            if (!draft.autoplay(choices, &error))
                std::cout << "info string draft error " << error << '\n';
            else {
                std::cout << "draftauto";
                for (const PieceType type : choices)
                    std::cout << ' ' << Position::type_name(type);
                std::cout << '\n';
            }
            continue;
        }
        if (line.rfind("draft choose ", 0) == 0) {
            const auto type = Position::type_from_name(line.substr(13));
            std::string error;
            if (!type || !draft.choose(*type, &error))
                std::cout << "info string draft error " << (type ? error : "unknown piece") << '\n';
            else
                std::cout << "draftok\n";
            continue;
        }
        if (line == "draft commit") {
            std::string error;
            if (!draft.commit(&error))
                std::cout << "info string draft error " << error << '\n';
            else
                std::cout << "draftok\n";
            continue;
        }
        if (line == "d" || line == "moves") {
            print_position(position);
            continue;
        }
        if (line.rfind("position upn ", 0) == 0) {
            std::string error;
            if (!position.set_upn(line.substr(13), &error))
                std::cout << "info string invalid upn " << error << '\n';
            else
                std::cout << "positionok\n";
            continue;
        }
        if (line.rfind("move ", 0) == 0) {
            const auto move = position.move_from_string(line.substr(5));
            Undo undo;
            if (!move || !position.make_move(*move, undo))
                std::cout << "illegalmove\n";
            else
                std::cout << "position " << position.upn() << '\n';
            continue;
        }
        if (line.rfind("perft ", 0) == 0) {
            int depth = 0;
            if (parse_int(line.substr(6), depth) && depth >= 0)
                std::cout << "nodes " << position.perft(depth) << '\n';
            continue;
        }
        if (line == "go" || line.rfind("go ", 0) == 0) {
            std::istringstream input(line);
            std::string token;
            input >> token;
            const SearchLimits limits = parse_limits(input, &position);
            const SearchResult result = search.think(position, limits);
            std::cout << "info depth " << result.completedDepth << " score ";
            print_score(result.score);
            std::cout << " nodes " << result.nodes << " time " << result.elapsed.count() << " pv";
            for (const Move& move : result.principalVariation)
                std::cout << ' ' << position.move_to_string(move);
            std::cout << '\n';
            std::cout << "bestmove "
                      << (result.bestMove ? position.move_to_string(*result.bestMove) : "(none)") << '\n';
            continue;
        }
        std::cout << "info string unknown command\n";
    }
    return 0;
}
