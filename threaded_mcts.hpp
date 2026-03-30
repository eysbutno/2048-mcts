#include "board.hpp"
#include "threaded_node.hpp"
#include "thread_pool.h"
#include <chrono>
#include <algorithm>

struct threaded_mcts {
    static constexpr int MAX_SCORE = 3'932'100;

    std::unique_ptr<threaded_node> root;
    board state;
    int num_rollouts;
    thread_pool tasks;

    threaded_mcts() : root(std::make_unique<threaded_node>()), state(), 
                      num_rollouts(0), tasks() {
        state.add_tile(state.gen_tile());
        state.add_tile(state.gen_tile());
    }

    threaded_mcts(size_t num_threads) 
        : root(std::make_unique<threaded_node>()), state(), num_rollouts(0), tasks(num_threads) {
        state.add_tile(state.gen_tile());
        state.add_tile(state.gen_tile());
    }

    /**
     * @return a random value in the range [0, bound], provided bound >= 0
     */
    inline int get_rand(int bound) {
        thread_local static std::mt19937 rng{(uint32_t)std::chrono::steady_clock::now().time_since_epoch().count()};
        return std::uniform_int_distribution<int>(0, bound)(rng);
    }

    inline int tile_hash(const std::pair<int, int> &t) {
        return t.first * 4 + t.second / 2;
    }

    /**
     * @return a selected threaded_node (based on UCT), with its respective board state.
     */
    std::pair<threaded_node*, board> select() {
        threaded_node* cur = root.get();
        auto b = state;

        while (cur->has_ch()) {
            if (cur->is_chance) {
                // transition to a decision threaded_node
                auto nxt = (*std::max_element(cur->ch.begin(), cur->ch.end(), 
                    [](auto &x, auto &y) -> bool {
                    return x->value() < y->value();
                })).get();

                b.play_move((board::directions) nxt->move);
                cur = nxt;
            } else {
                // transition to a randomly generated tile
                const auto add = b.gen_tile();
                int hash = tile_hash(add);
                b.add_tile(add);

                for (const auto &nxt : (cur->ch)) {
                    if (nxt->move == hash) {
                        cur = nxt.get();
                        break;
                    }
                }
            }

            // we use virtual loss to avoid reexploring
            cur->upd(-1);
        }

        return std::make_pair(cur, b);
    }

    std::pair<threaded_node*, board> expand(threaded_node* cur, const board &b) {
        if (b.open == 0 || cur->has_ch()) {
            return std::make_pair(cur, b);
        }

        std::lock_guard lock(cur->mtx);

        if (cur->has_ch()) {
            // double check in case lol
            return std::make_pair(cur, b);
        }

        if (cur->is_chance) {
            // transition to a decision threaded_node
            for (int i = 0; i < 4; i++) {
                auto nxt = b;
                if (nxt.play_move((board::directions) i)) {
                    cur->add_ch(false, i);
                }
            }

            threaded_node* chosen = cur->ch[get_rand((cur->ch).size() - 1)].get();
            board nxt_b = b;
            nxt_b.play_move((board::directions) chosen->move);

            return std::make_pair(chosen, nxt_b);
        } else {
            // transition to a randomly generated tile
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    if (b.grid[i][j] == 0) {
                        for (int v : {2, 4}) {
                            cur->add_ch(true, (i * 4 + j) * 4 + v / 2);
                        }
                    }
                }
            }

            board nxt_b = b;
            const std::pair<int, int> add = nxt_b.gen_tile();
            int hash = add.first * 4 + add.second / 2;
            nxt_b.add_tile(add);

            for (auto &nxt : (cur->ch)) {
                if (nxt->move == hash) {
                    // apply virtual loss
                    nxt->upd(-1);
                    return std::make_pair(nxt.get(), nxt_b);
                }
            }
        }

        assert(false);
        return std::make_pair(cur, b);
    }

    double rollout(board b, bool is_chance) {
        while (b.open > 0) {
            if (is_chance) {
                // transition to a decision threaded_node
                int seen = 0;
                for (int i = 0; i < 4; i++) {
                    auto nxt = b;
                    if (nxt.play_move((board::directions) i)) {
                        seen++;
                    }
                }
                
                assert(seen > 0);
                seen = get_rand(seen - 1);
                for (int i = 0; i < 4; i++) {
                    auto nxt = b;
                    if (nxt.play_move((board::directions) i)) {
                        if (seen == 0) {
                            b = nxt;
                            break;
                        }

                        seen--;
                    }
                }
            } else {
                // transition to a randomly generated tile
                b.add_tile(b.gen_tile());
            }

            is_chance = !is_chance;
        }

        return (double) b.score / MAX_SCORE;
    }

    /**
     * Backpropogates res to all parent threaded_nodes. 
     * Because of perspective switch, we need to
     * alternate values properly.
     */
    void backprop(threaded_node* cur, long double res) {
        while (cur != nullptr) {
            cur->upd(res + 1); // +1 is to undo virtual loss
            cur = cur->par;
        }
    }

    /**
     * Searches the game tree by repeating each of the 4 steps.
     * 
     * @param max_rollouts max rollouts allowed to be performed
     */
    void search_rollouts(int max_rollouts) {
        num_rollouts = 0;
        std::vector<std::future<void>> wait;
        while (num_rollouts < max_rollouts) {
            wait.push_back(tasks.submit([&]() -> void {
                auto [cur, b] = select();
                std::tie(cur, b) = expand(cur, b);
                backprop(cur, rollout(b, cur->is_chance));
            }));
            
            num_rollouts++;
        }

        for (auto &i : wait) {
            i.get();
        }
    }

    int best_move() {
        assert(root->is_chance);
        return (*std::max_element(root->ch.begin(), root->ch.end(), 
            [](const auto &x, const auto &y) -> bool {
            return (x->q / x->n) < (y->q / y->n);
        }))->move;
    }

    void play_move(int dir) {
        assert(root->is_chance);
        state.play_move((board::directions) dir);
        for (auto &nxt : root->ch) {
            if (nxt->move == dir) {
                auto nxt_root = std::move(nxt);
                nxt_root->par = nullptr;
                root = std::move(nxt_root);
                return;
            }
        }
    }

    void add_tile(const std::pair<int, int> &t) {
        assert(!root->is_chance);
        if (root->ch.empty()) {
            expand(root.get(), state);
        }

        state.add_tile(t);
        int hash = tile_hash(t);
        for (auto &nxt : root->ch) {
            if (nxt->move == hash) {
                auto nxt_root = std::move(nxt);
                nxt_root->par = nullptr;
                root = std::move(nxt_root);
                return;
            }
        }
    }
};