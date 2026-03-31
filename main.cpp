#include "board.hpp"
#include "mcts.hpp"
#include "threaded_mcts.hpp"
#include <iostream>
#include <chrono>

int main() {
    mcts solver{};
    bool is_chance = true;

    /*
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            std::cout << solver.state.grid[i][j] << ' ';
        }
        
        std::cout << "\n";
    }
    */

    int moves = 0;
    auto start = std::chrono::steady_clock::now();
    while (!solver.state.terminal()) {
        if (is_chance) {
            solver.search_rollouts(500);
            board::directions move = (board::directions) solver.best_move();
            solver.play_move(move);
            moves++;
        } else {
            solver.add_tile(solver.state.gen_tile());
        }

        is_chance = !is_chance;
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);
    int seconds = duration.count();
    std::cout << (double) moves / duration.count() << '\n';
    std::cout << seconds << " " << solver.state.score << '\n';

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            std::cout << solver.state.grid[i][j] << ' ';
        }
        
        std::cout << "\n";
    }
}