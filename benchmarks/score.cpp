#include "bitboard.hpp"
#include "mcts.hpp"
#include "../thread_pool.hpp"
#include <iostream>
#include <future>

int main(int argc, char* argv[]) {
    // argv = {C, open, mono, rollouts, iterations}
    assert(argc == 6);

    config::init(std::atof(argv[1]), std::atof(argv[2]), std::atof(argv[3]));
    int rollouts = std::atoi(argv[4]);
    int num_tries = std::atoi(argv[5]);
    assert(num_tries > 0);
    thread_pool tasks(7); // not tryna set my laptop on fire lol

    std::vector<std::future<int>> wait;
    for (int i = 0; i < num_tries; i++) {
        wait.push_back(tasks.submit([rollouts, num_tries]() -> int {
            mcts solver{};
            bool is_chance = true;
            while (!solver.state.terminal()) {
                if (is_chance) {
                    solver.search_rollouts(rollouts);
                    bitboard::directions move = (bitboard::directions) solver.best_move();
                    solver.play_move(move);
                } else {
                    solver.play_tile(solver.state.gen_tile_idx());
                }

                is_chance = !is_chance;
            }

            return solver.state.score;
        }));
    }

    int sum = 0;
    for (auto &i : wait) sum += i.get();
    sum /= num_tries;

    std::cout << sum << '\n';
}