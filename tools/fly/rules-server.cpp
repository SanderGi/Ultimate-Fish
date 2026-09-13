// Stateless native referee for the public fly page. GPL-3.0-or-later.
// Reuse the exact roster, feature encoder and adjudication used in training.
#include "rules.cpp"

int main() {
    int preset, reflection, count;
    if (!(std::cin >> preset >> reflection >> count) || preset < 0 || preset > 2
        || reflection < 0 || reflection > 1 || count < 0 || count > 400) {
        std::cout << "{\"error\":\"Invalid game history\"}\n";
        return 0;
    }
    reset(preset, unsigned(reflection));
    for (int ply = 0; ply < count; ++ply) {
        int index;
        const auto moves = game.legal_moves();
        if (!(std::cin >> index) || index < 0 || size_t(index) >= moves.size()
            || game.game_over() || exhibition_draw()) {
            std::cout << "{\"error\":\"Illegal action in game history\"}\n";
            return 0;
        }
        history.push_back(game);
        Undo undo;
        game.make_move(moves[index], undo);
        ++ticks;
    }
    // Features are computed once, for the final position, rather than per replay.
    std::cout << state() << '\n';
}
