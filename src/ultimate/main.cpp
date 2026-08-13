/*
  Ultimate Fish - Chess Ultimate 5.731 command-line engine
  GPLv3 or later
*/

#include "position.h"
#include "search.h"
#include "draft.h"

#include <charconv>
#include <iostream>
#include <set>
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

void print_score(int score, const std::optional<int>& exactMateActions = std::nullopt) {
    if (exactMateActions) {
        std::cout << "mate " << *exactMateActions;
    }
    else if (std::abs(score) >= 30000 - 128) {
        const int actions = std::max(1, 30000 - std::abs(score));
        std::cout << "mate " << (score < 0 ? -actions : actions);
    }
    else
        std::cout << "cp " << score;
}

void print_search_info(const Position& position, const SearchResult& result) {
    Position line = position;
    std::vector<std::string> display;
    std::vector<std::string> publicDisplay;
    for (const Move& move : result.principalVariation) {
        display.push_back(line.move_to_display_string(move));
        publicDisplay.push_back(line.move_to_display_string(move, true));
        Undo undo;
        if (!line.make_move(move, undo))
            break;
    }
    std::cout << "displaypv";
    for (const std::string& notation : display)
        std::cout << ' ' << notation;
    std::cout << '\n';
    std::cout << "publicpv";
    for (const std::string& notation : publicDisplay)
        std::cout << ' ' << notation;
    std::cout << '\n';
    std::cout << "info depth " << result.completedDepth << " score ";
    print_score(result.score, result.mateActions);
    std::cout << " nodes " << result.nodes << " time " << result.elapsed.count() << " pv";
    for (const Move& move : result.principalVariation)
        std::cout << ' ' << position.move_to_string(move);
    std::cout << '\n';
}

void print_belief_search_info(const PublicBeliefState& beliefs,
                              const BeliefSearchResult& result,
                              bool conservativeMergedCells) {
    if (!result.validInformationCell) {
        std::cout << "info string invalid belief decision cell spans "
                  << beliefs.decision_partitions()
                  << " privately distinguishable observations\n"
                  << "bestmove (none)\n";
        return;
    }
    std::cout << "info depth " << result.completedDepth << " score ";
    print_score(result.score, result.mateActions);
    std::cout << " nodes " << result.nodes << " time " << result.elapsed.count()
              << " beliefs " << result.beliefs << " deepbeliefs " << result.deepBeliefs
              << " common " << result.commonMoves << " candidates " << result.candidates
              << " beliefmode history-preserving historyplies "
              << result.historyPreservingPlies
              << " decisionmode "
              << (conservativeMergedCells
                    ? "merged-conservative" : "exact-cell")
              << " decisionpartitions " << beliefs.decision_partitions()
              << " beliefworst " << result.worstScore
              << " beliefmean " << result.meanScore << " pv";
    for (const std::string& move : result.principalVariation)
        std::cout << ' ' << move;
    std::cout << '\n';
    // Keep the original fixed belief-analysis line backward compatible with
    // local bridge clients. Extensible diagnostics use separate info-string
    // records so older parsers can continue matching the metadata contract.
    std::cout << "info string searchpath " << result.searchPath << '\n';
    std::cout << "bestmove " << (result.bestMove ? *result.bestMove : "(none)") << '\n';
}

void print_piece_knowledge(const Position& representative,
                           const PublicBeliefState& beliefs) {
    for (int id = 0; id < representative.piece_count(); ++id) {
        const PieceState& piece = representative.piece(id);
        if (!piece.alive || !piece.onBoard)
            continue;
        if (piece.type == PieceType::Ghost) {
            std::vector<int> equivalentIds{id};
            // Canonical initial beliefs intentionally do not retain factorial
            // identity permutations for identical hidden enemy Ghosts. Their
            // diagnostic location is therefore the union for the whole
            // indistinguishable group, not an arbitrary canonical id subset.
            if (piece.color != beliefs.disclosure().observer && !piece.visible) {
                equivalentIds.clear();
                for (int candidateId = 0;
                     candidateId < representative.piece_count(); ++candidateId) {
                    const PieceState& candidate = representative.piece(candidateId);
                    if (candidate.alive && candidate.onBoard &&
                        candidate.type == PieceType::Ghost &&
                        candidate.color == piece.color &&
                        candidate.visible == piece.visible &&
                        candidate.action == piece.action &&
                        candidate.cooldown == piece.cooldown &&
                        candidate.freezeCount == piece.freezeCount &&
                        candidate.power == piece.power &&
                        candidate.moved == piece.moved &&
                        candidate.link == piece.link &&
                        candidate.host == piece.host &&
                        candidate.attachmentOrder == piece.attachmentOrder)
                        equivalentIds.push_back(candidateId);
                }
            }
            std::set<int> candidateSet;
            for (int candidateId : equivalentIds) {
                const std::vector<int> locations =
                  beliefs.piece_location_candidates(candidateId);
                candidateSet.insert(locations.begin(), locations.end());
            }
            const std::vector<int> candidates(
              candidateSet.begin(), candidateSet.end());
            const bool known = equivalentIds.size() == 1 &&
              beliefs.piece_location_known(equivalentIds.front());
            std::cout << "knowledge ghost "
                      << Position::square_name(piece.square) << ' '
                      << int(known) << ' ';
            if (candidates.empty())
                std::cout << '-';
            else
                for (std::size_t index = 0; index < candidates.size(); ++index) {
                    if (index)
                        std::cout << ',';
                    std::cout << Position::square_name(candidates[index]);
                }
            std::cout << '\n';
        }
        else if (piece.type == PieceType::Jester)
            std::cout << "knowledge jester "
                      << Position::square_name(piece.square) << ' '
                      << int(beliefs.piece_type_known(id, PieceType::Jester))
                      << '\n';
    }
    std::cout << "knowledgeend\n";
}

SearchLimits parse_limits(std::istringstream& input, const Position* position = nullptr,
                          int moveOverhead = 50) {
    SearchLimits limits;
    limits.moveOverhead = std::chrono::milliseconds(moveOverhead);
    int whiteTime = 0;
    int blackTime = 0;
    int whiteIncrement = 0;
    int blackIncrement = 0;
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
        else if (token == "wtime")
            whiteTime = std::max(0, parsed);
        else if (token == "btime")
            blackTime = std::max(0, parsed);
        else if (token == "winc")
            whiteIncrement = std::max(0, parsed);
        else if (token == "binc")
            blackIncrement = std::max(0, parsed);
        else if (token == "movestogo")
            limits.movesToGo = std::max(0, parsed);
        else if (token == "overhead")
            limits.moveOverhead = std::chrono::milliseconds(std::max(0, parsed));
        else if (token == "factored")
            limits.factoredBeliefs = parsed != 0;
    }
    if (position) {
        const bool white = position->side_to_move() == Color::White;
        limits.remainingTime = std::chrono::milliseconds(white ? whiteTime : blackTime);
        limits.increment = std::chrono::milliseconds(
          white ? whiteIncrement : blackIncrement);
    }
    return limits;
}

}  // namespace

int main() {
    std::cout.setf(std::ios::unitbuf);
    Position position;
    Search search;
    DraftState draft;
    PublicBeliefState beliefs;
    PublicHistoryState history;
    int configuredMoveOverhead = 50;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit")
            break;
        if (line == "uci") {
            std::cout << "id name Ultimate Fish 0.1\n"
                         "id author Ultimate Fish contributors\n"
                         "option name Hash type spin default 64 min 1 max 4096\n"
                         "option name Move Overhead type spin default 50 min 0 max 5000\n"
                         "uciok\n";
            continue;
        }
        if (line == "isready") {
            std::cout << "readyok\n";
            continue;
        }
        constexpr std::string_view overheadPrefix = "setoption name Move Overhead value ";
        if (line.rfind(overheadPrefix, 0) == 0) {
            int value = 50;
            if (parse_int(line.substr(overheadPrefix.size()), value))
                configuredMoveOverhead = std::clamp(value, 0, 5000);
            continue;
        }
        if (line == "ucinewgame") {
            position.clear();
            beliefs = PublicBeliefState{};
            history = PublicHistoryState{};
            search.clear();
            continue;
        }
        if (line == "history clear") {
            history = PublicHistoryState{};
            std::cout << "historyok\n";
            continue;
        }
        if (line.rfind("history start ", 0) == 0) {
            std::istringstream input(line);
            std::string historyToken, startToken, colorToken, knownToken;
            std::string deploymentToken;
            input >> historyToken >> startToken >> colorToken >> knownToken
                  >> deploymentToken;
            int enemyKingKnown = 0;
            int initialDeploymentKnown = 0;
            std::string upn;
            if (parse_int(deploymentToken, initialDeploymentKnown) &&
                (initialDeploymentKnown == 0 || initialDeploymentKnown == 1))
                std::getline(input >> std::ws, upn);
            else {
                // Backward-compatible spelling: the old protocol omitted the
                // root-provenance bit and therefore means arbitrary snapshot.
                upn = deploymentToken;
                initialDeploymentKnown = 0;
            }
            bool hasKingCandidates = false;
            std::vector<int> kingCandidates;
            if (upn.rfind("kc=", 0) == 0) {
                const std::size_t separator = upn.find(' ');
                const std::string candidateToken = upn.substr(
                  3, separator == std::string::npos
                     ? std::string::npos : separator - 3);
                upn = separator == std::string::npos
                    ? std::string() : upn.substr(separator + 1);
                hasKingCandidates = true;
                std::size_t begin = 0;
                while (begin <= candidateToken.size()) {
                    const std::size_t comma = candidateToken.find(',', begin);
                    const std::string square = candidateToken.substr(
                      begin, comma == std::string::npos
                           ? std::string::npos : comma - begin);
                    const int parsed = Position::square_from_name(square);
                    if (parsed == Position::NoSquare) {
                        kingCandidates.clear();
                        break;
                    }
                    kingCandidates.push_back(parsed);
                    if (comma == std::string::npos)
                        break;
                    begin = comma + 1;
                }
            }
            const bool validColor = colorToken == "white" || colorToken == "black";
            const bool validKnown = parse_int(knownToken, enemyKingKnown) &&
              (enemyKingKnown == 0 || enemyKingKnown == 1);
            Position initial;
            std::string error;
            if (!validColor || !validKnown || upn.empty() ||
                (hasKingCandidates && kingCandidates.empty()))
                error = "expected history start white|black 0|1 [0|1] "
                        "[kc=square,...] <upn>";
            else if (!initial.set_upn(upn, &error))
                error = "invalid initial UPN: " + error;
            else {
                const DisclosureContext disclosure{
                  colorToken == "black" ? Color::Black : Color::White,
                  enemyKingKnown != 0};
                const bool started = hasKingCandidates
                  ? history.start(std::move(initial), disclosure,
                                  initialDeploymentKnown != 0,
                                  kingCandidates, &error)
                  : history.start(std::move(initial), disclosure,
                                  initialDeploymentKnown != 0, &error);
                if (started) {
                    std::cout << "historyok beliefs "
                              << history.beliefs().size() << '\n';
                    continue;
                }
            }
            std::cout << "info string invalid public history " << error << '\n';
            continue;
        }
        if (line.rfind("history move ", 0) == 0) {
            std::string error;
            if (!history.apply_actual(line.substr(13), &error))
                std::cout << "info string invalid public history " << error << '\n';
            else
                std::cout << "historyok beliefs " << history.beliefs().size()
                          << '\n';
            continue;
        }
        if (line == "history count") {
            if (!history.initialized())
                std::cout << "info string invalid public history not initialized\n";
            else {
                const std::vector<int> kingCandidates =
                  history.beliefs().enemy_king_candidate_squares();
                std::cout << "historycount " << history.beliefs().size()
                          << " observer "
                          << (history.beliefs().disclosure().observer == Color::White
                                ? "white" : "black")
                          << " enemykingknown "
                          << int(history.beliefs().disclosure().enemyKingKnown)
                          << " decisionpartitions "
                          << history.beliefs().decision_partitions()
                          << " royalknown "
                          << int(history.beliefs().enemy_king_known())
                          << " kingcandidates ";
                if (kingCandidates.empty())
                    std::cout << '-';
                else
                    for (std::size_t index = 0;
                         index < kingCandidates.size(); ++index) {
                        if (index)
                            std::cout << ',';
                        std::cout << Position::square_name(
                          kingCandidates[index]);
                    }
                std::cout << '\n';
            }
            continue;
        }
        if (line == "history d") {
            if (!history.initialized())
                std::cout << "info string invalid public history not initialized\n";
            else
                print_position(history.actual_position());
            continue;
        }
        if (line == "history knowledge") {
            if (!history.initialized())
                std::cout << "info string invalid public history not initialized\n";
            else
                print_piece_knowledge(history.actual_position(), history.beliefs());
            continue;
        }
        if (line == "history dump") {
            if (!history.initialized()) {
                std::cout << "info string invalid public history not initialized\n";
                continue;
            }
            for (const Position& world : history.beliefs().positions())
                std::cout << "historybelief " << world.upn() << '\n';
            std::cout << "historyend " << history.beliefs().size() << '\n';
            continue;
        }
        if (line == "history go" || line.rfind("history go ", 0) == 0) {
            if (!history.initialized()) {
                std::cout << "info string invalid public history not initialized\n"
                             "bestmove (none)\n";
                continue;
            }
            std::istringstream input(line);
            std::string token;
            input >> token >> token;
            SearchLimits limits = parse_limits(
              input, nullptr, configuredMoveOverhead);
            const bool streamIterations =
              line.find(" stream") != std::string::npos;
            if (streamIterations)
                limits.onBeliefIteration = [&](const BeliefSearchResult& iteration) {
                    print_belief_search_info(
                      history.beliefs(), iteration, false);
                    std::cout.flush();
                };
            const BeliefSearchResult result = search.think_beliefs(
              history.beliefs(), limits, true);
            if (!streamIterations)
                print_belief_search_info(history.beliefs(), result, false);
            continue;
        }
        if (line == "belief clear") {
            beliefs.clear();
            std::cout << "beliefok\n";
            continue;
        }
        if (line.rfind("belief observer ", 0) == 0) {
            std::istringstream input(line);
            std::string beliefToken, observerToken, colorToken;
            int enemyKingKnown = 0;
            input >> beliefToken >> observerToken >> colorToken;
            std::string knownToken;
            const bool hasKnown = bool(input >> knownToken);
            const bool parsedKnown = !hasKnown ||
              parse_int(knownToken, enemyKingKnown);
            const bool validColor = colorToken == "white" || colorToken == "black";
            const bool validKnown = parsedKnown &&
              (enemyKingKnown == 0 || enemyKingKnown == 1);
            std::string extra;
            const bool hasExtra = bool(input >> extra);
            std::string error;
            if (!validColor || !validKnown || hasExtra ||
                !beliefs.set_disclosure(
                  {colorToken == "black" ? Color::Black : Color::White,
                   enemyKingKnown != 0}, &error))
                std::cout << "info string invalid belief disclosure "
                          << (error.empty() ? "expected white|black [0|1]" : error)
                          << '\n';
            else
                std::cout << "beliefok\n";
            continue;
        }
        if (line == "belief count") {
            std::cout << "beliefcount " << beliefs.size() << " observer "
                      << (beliefs.disclosure().observer == Color::White
                            ? "white" : "black")
                      << " enemykingknown "
                      << int(beliefs.disclosure().enemyKingKnown)
                      << " mode exact-uncapped\n";
            continue;
        }
        if (line.rfind("belief add ", 0) == 0) {
            Position belief;
            std::string error;
            if (!belief.set_upn(line.substr(11), &error))
                std::cout << "info string invalid belief " << error << '\n';
            else if (!beliefs.add(std::move(belief), &error))
                std::cout << "info string invalid belief " << error << '\n';
            else
                std::cout << "beliefok\n";
            continue;
        }
        if (line == "belief knowledge" ||
            line.rfind("belief knowledge ", 0) == 0) {
            if (beliefs.empty()) {
                std::cout << "info string invalid belief no retained worlds\n";
                continue;
            }
            Position representative;
            std::string error;
            if (line == "belief knowledge")
                representative = beliefs.positions().front();
            else if (!representative.set_upn(line.substr(17), &error)) {
                std::cout << "info string invalid belief representative "
                          << error << '\n';
                continue;
            }
            print_piece_knowledge(representative, beliefs);
            continue;
        }
        if (line == "belief observe" || line.rfind("belief observe ", 0) == 0) {
            std::istringstream input(line);
            std::string token;
            input >> token >> token;
            std::vector<std::string> markers;
            while (input >> token)
                markers.push_back(token);
            const std::size_t before = beliefs.size();
            const std::size_t partitions = beliefs.decision_partitions();
            std::string error;
            if (!beliefs.condition_on_decision_markers(markers, &error))
                std::cout << "info string invalid belief observation "
                          << error << '\n';
            else
                std::cout << "beliefok before " << before
                          << " after " << beliefs.size()
                          << " partitions " << partitions << '\n';
            continue;
        }
        if (line.rfind("belief apply ", 0) == 0) {
            std::string error;
            const BeliefTransitionResult applied = beliefs.apply_known(
              line.substr(13), &error);
            if (!applied.applied)
                std::cout << "info string invalid belief transition " << error
                          << '\n';
            else
                std::cout << "beliefok before " << applied.before
                          << " after " << applied.after << " observations "
                          << applied.observations << '\n';
            continue;
        }
        if (line == "belief go" || line.rfind("belief go ", 0) == 0) {
            const bool conservativeMergedCells =
              line == "belief go conservative" ||
              line.rfind("belief go conservative ", 0) == 0;
            std::istringstream input(line);
            std::string token;
            input >> token >> token;
            if (conservativeMergedCells)
                input >> token;
            const SearchLimits limits = parse_limits(input, nullptr, configuredMoveOverhead);
            // A controller which has not yet mapped the app's highlighted
            // legal dots to an exact decision cell may deliberately ignore
            // that private information. Searching the union is conservative:
            // one move must be legal across every retained cell, and no world
            // is sampled or discarded. Ordinary `belief go` remains strict.
            const BeliefSearchResult result = search.think_beliefs(
              beliefs, limits, !conservativeMergedCells);
            print_belief_search_info(
              beliefs, result, conservativeMergedCells);
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
        if (line == "draft preview" || line.rfind("draft preview ", 0) == 0) {
            std::istringstream input(line);
            std::string token;
            input >> token >> token;
            std::vector<PieceType> excluded;
            bool valid = true;
            while (input >> token) {
                const auto type = Position::type_from_name(token);
                if (!type) {
                    valid = false;
                    break;
                }
                excluded.push_back(*type);
            }
            DraftState preview = draft;
            std::vector<PieceType> choices;
            std::string error;
            if (!valid)
                std::cout << "info string draft error unknown excluded piece\n";
            else if (!preview.autoplay(choices, &error, excluded))
                std::cout << "info string draft error " << error << '\n';
            else {
                std::cout << "draftpreview";
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
        if (line == "eval") {
            std::cout << "eval cp " << position.static_evaluate() << '\n';
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
        if (line.rfind("notation ", 0) == 0) {
            const auto move = position.move_from_string(line.substr(9));
            if (!move)
                std::cout << "illegalmove\n";
            else
                std::cout << "notation " << position.move_to_display_string(*move)
                          << " public " << position.move_to_display_string(*move, true)
                          << '\n';
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
            SearchLimits limits = parse_limits(input, &position, configuredMoveOverhead);
            const bool streamIterations = line.find(" stream") != std::string::npos;
            if (streamIterations)
                limits.onIteration = [&position](const SearchResult& iteration) {
                    print_search_info(position, iteration);
                    std::cout.flush();
                };
            const SearchResult result = search.think(position, limits);
            if (!streamIterations)
                print_search_info(position, result);
            std::cout << "bestmove "
                      << (result.bestMove ? position.move_to_string(*result.bestMove) : "(none)") << '\n';
            continue;
        }
        std::cout << "info string unknown command\n";
    }
    return 0;
}
