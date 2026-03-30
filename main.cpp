#include "board.hpp"
#include "mcts.hpp"
#include <iostream>

int main() {
    mcts solver{};
    bool is_chance = false;
    while (solver.state.open > 0) {
        if (is_chance) {
            solver.search_rollouts(5000);
            board::directions move = (board::directions) solver.best_move();
            solver.play_move(move);
            std::cout << move << ' ' << solver.num_rollouts << ' ' << solver.state.score << '\n';
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