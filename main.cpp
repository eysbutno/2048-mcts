#include "board.hpp"
#include "threaded_mcts.hpp"
#include <iostream>

int main() {
    threaded_mcts solver{};
    bool is_chance = true;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            std::cout << solver.state.grid[i][j] << ' ';
        }
        
        std::cout << "\n";
    }

    while (solver.state.open > 0) {
        if (is_chance) {
            solver.search_rollouts(2000);
            board::directions move = (board::directions) solver.best_move();
            solver.play_move(move);
            std::cout << move << ' ' << solver.state.open << ' ' << solver.state.score << '\n';
        } else {
            solver.add_tile(solver.state.gen_tile());
        }

        is_chance = !is_chance;
    }

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            std::cout << solver.state.grid[i][j] << ' ';
        }
        
        std::cout << "\n";
    }
}