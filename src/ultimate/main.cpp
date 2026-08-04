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
}

}  // namespace

int main() {
    std::cout.setf(std::ios::unitbuf);
    Position position;
    Search search;
    DraftState draft;
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
            search.clear();
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
            SearchLimits limits;
            std::istringstream input(line);
            std::string token;
            input >> token;
            while (input >> token) {
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
            const SearchResult result = search.think(position, limits);
            std::cout << "info depth " << result.completedDepth << " score cp " << result.score
                      << " nodes " << result.nodes << " time " << result.elapsed.count() << " pv";
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
