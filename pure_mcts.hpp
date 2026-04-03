#include "bitboard.hpp"
#include "thread_pool.hpp"

struct pure_mcts {
    static inline thread_local std::mt19937 rng{(uint32_t)std::chrono::steady_clock::now().time_since_epoch().count()};
    thread_pool tasks;
    bitboard state;

    pure_mcts() : tasks(1), state() {}
    pure_mcts(int num_threads) : tasks(num_threads), state() {}

    static int get_rand(int bound) {
        assert(bound > 0);
        return std::uniform_int_distribution<int>(0, bound - 1)(rng);
    }

    static int rollout(bitboard b, bool is_chance = false) {
        while (!b.terminal()) {
            if (is_chance) {
                // transition to a decision node
                // b.play_move(b.gen_move());
                b.play_move(b.gen_heuristic_move());
            } else {
                // transition to a randomly generated tile
                b.play_tile(b.gen_tile());
            }

            is_chance = !is_chance;
        }

        return b.score;
    }

    bitboard::directions search(int num_rollouts) {
        std::vector<std::pair<int, std::future<int>>> wait;
        std::array<int, 4> sums{}, nums{};
        for (int i = 0; i < num_rollouts; i++) {
            auto nxt = state;
            auto dir = nxt.gen_move();
            nxt.play_move(dir);
            wait.push_back({(int) dir, tasks.submit([nxt]() -> int {
                return rollout(nxt, false);
            })});
        }

        for (auto &[i, j] : wait) {
            sums[i] += j.get();
            nums[i]++;
        }

        double best = 0;
        bitboard::directions res = bitboard::directions::NONE;
        for (int i = 0; i < 4; i++) {
            if (nums[i] > 0 && (double) sums[i] / nums[i] > best) {
                best = (double) sums[i] / nums[i];
                res = (bitboard::directions) i;
            }
        }

        return res;
    }

    void play_move(int dir) {
        bool played = state.play_move((bitboard::directions) dir);
        assert(played);
    }

    void play_tile(int move) {
        state.play_tile(bitboard::to_tile(move));
    }
};