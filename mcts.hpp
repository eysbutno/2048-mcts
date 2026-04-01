#include "board.hpp"
#include "node.hpp"
#include <chrono>
#include <algorithm>

struct mcts {
    static constexpr int MAX_SCORE = 3'932'100;

    std::mt19937 rng{(uint32_t)std::chrono::steady_clock::now().time_since_epoch().count()};
    std::unique_ptr<node> root;
    board state;
    int num_rollouts;

    mcts() : root(std::make_unique<node>()), state(), num_rollouts(0) {
        state.add_tile(state.gen_tile());
        state.add_tile(state.gen_tile());
    }

    mcts(int max_rollouts) : root(std::make_unique<node>()), state(), num_rollouts(0) {
        state.add_tile(state.gen_tile());
        state.add_tile(state.gen_tile());
    }

    /**
     * @return a random value in the range [0, bound], provided bound >= 0
     */
    inline int get_rand(int bound) {
        return std::uniform_int_distribution<int>(0, bound)(rng);
    }

    inline int tile_hash(const std::pair<int, int> &t) {
        return t.first * 4 + t.second / 2;
    }

    /**
     * @return a selected node (based on UCT), with its respective board state.
     */
    std::pair<node*, board> select() {
        node* cur = root.get();
        auto b = state;

        while (!(cur->ch).empty()) {
            if (cur->is_chance) {
                // transition to a decision node
                auto nxt = (*std::max_element(cur->ch.begin(), cur->ch.end(), 
                    [](const auto &x, const auto &y) -> bool {
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
        }

        return std::make_pair(cur, b);
    }

    std::pair<node*, board> expand(node* cur, const board &b) {
        if (b.terminal()) {
            return std::make_pair(cur, b);
        }

        if (cur->is_chance) {
            // transition to a decision node
            for (int i = 0; i < 4; i++) {
                if (b.can_play((board::directions) i)) {
                    cur->add_ch(false, i);
                }
            }

            node* chosen = cur->ch[get_rand((cur->ch).size() - 1)].get();
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

            for (const auto &nxt : (cur->ch)) {
                if (nxt->move == hash) {
                    return std::make_pair(nxt.get(), nxt_b);
                }
            }
        }

        assert(false);
        return std::make_pair(cur, b);
    }

    double rollout(board b, bool is_chance) {
        while (!b.terminal()) {
            if (is_chance) {
                // transition to a decision node
                int seen = 0;
                for (int i = 0; i < 4; i++) {
                    if (b.can_play((board::directions) i)) {
                        seen++;
                    }
                }
                
                assert(seen > 0);
                seen = get_rand(seen - 1);
                for (int i = 0; i < 4; i++) {
                    auto nxt = b;
                    if (b.can_play((board::directions) i)) {
                        if (seen == 0) {
                            auto nxt = b;
                            nxt.play_move((board::directions) i);
                            b = std::move(nxt);
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
     * Backpropogates res to all parent nodes. 
     * Because of perspective switch, we need to
     * alternate values properly.
     */
    void backprop(node* cur, long double res) {
        while (cur != nullptr) {
            cur->n++;
            cur->q += res;
            cur = cur->par;
        }
    }

    /**
     * Searches the game tree by repeating each of the 4 steps.
     * NOTE: Because of std::unique_ptr, each run may take longer (use arena/similar to fix)
     * 
     * @param tl_ms time limit in ms
     */
    void search_tl(int tl_ms) {
        using clock = std::chrono::steady_clock;

        auto deadline = clock::now() + std::chrono::milliseconds(tl_ms);

        num_rollouts = 0;
        while (clock::now() < deadline) {
            auto [cur, b] = select();
            std::tie(cur, b) = expand(cur, b);
            backprop(cur, rollout(b, cur->is_chance));
            num_rollouts++;
        }
    }

    /**
     * Searches the game tree by repeating each of the 4 steps.
     * 
     * @param max_rollouts max rollouts allowed to be performed
     */
    void search_rollouts(int max_rollouts) {
        num_rollouts = 0;
        while (num_rollouts < max_rollouts) {
            auto [cur, b] = select();
            std::tie(cur, b) = expand(cur, b);
            backprop(cur, rollout(b, cur->is_chance));
            num_rollouts++;
        }
    }

    int best_move() {
        assert(root->is_chance);
        return (*std::max_element(root->ch.begin(), root->ch.end(), 
            [](const auto &x, const auto &y) -> bool {
            return x->q / x->n < y->q / y->n;
        }))->move;
    }

    void play_move(int dir) {
        assert(root->is_chance);
        assert(state.can_play((board::directions) dir));
        bool played = state.play_move((board::directions) dir);
        assert(played);

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

        assert(false);
    }
};