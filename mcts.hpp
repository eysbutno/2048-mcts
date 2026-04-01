#include "bitboard.hpp"
#include "node.hpp"
#include <chrono>
#include <algorithm>

struct mcts {
    static constexpr int MAX_SCORE = 3'932'100;

    std::mt19937 rng{(uint32_t)std::chrono::steady_clock::now().time_since_epoch().count()};
    std::unique_ptr<node> root;
    bitboard state;
    int num_rollouts;

    mcts() : root(std::make_unique<node>()), state(), num_rollouts(0) {}

    mcts(int max_rollouts) : root(std::make_unique<node>()), state(), num_rollouts(0) {}

    int get_rand(int bound) {
        assert(bound > 0);
        return std::uniform_int_distribution<int>(0, bound - 1)(rng);
    }

    /**
     * @return a selected node (based on UCT), with its respective board state.
     */
    std::pair<node*, bitboard> select() {
        node* cur = root.get();
        auto b = state;

        while (!(cur->ch).empty()) {
            if (cur->is_chance) {
                // transition to a decision node
                auto nxt = (*std::max_element(cur->ch.begin(), cur->ch.end(), 
                    [](const auto &x, const auto &y) -> bool {
                    return x->value() < y->value();
                })).get();

                b.play_move((bitboard::directions) nxt->move);
                cur = nxt;
            } else {
                // transition to a randomly generated tile
                const auto add = b.gen_tile();
                b.play_tile(add);

                for (const auto &nxt : cur->ch) {
                    if (nxt->move == add) {
                        cur = nxt.get();
                        break;
                    }
                }
            }
        }

        return std::make_pair(cur, b);
    }

    std::pair<node*, bitboard> expand(node* cur, bitboard b) {
        if (b.terminal()) {
            return std::make_pair(cur, b);
        }

        if (cur->is_chance) {
            // transition to a decision node
            for (int i = 0; i < 4; i++) {
                auto nxt = b;
                if (nxt.play_move((bitboard::directions) i)) {
                    cur->add_ch(false, i);
                }
            }

            node* chosen = cur->ch[get_rand((cur->ch).size())].get();
            bitboard nxt_b = b;
            nxt_b.play_move((bitboard::directions) chosen->move);
            return std::make_pair(chosen, nxt_b);
        } else {
            // transition to a randomly generated tile
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    if (b.at(i, j) == 0) {
                        cur->add_ch(true, bitboard::tile_repr(i, j, 1));
                        cur->add_ch(true, bitboard::tile_repr(i, j, 2));
                    }
                }
            }


            const auto add = b.gen_tile();
            b.play_tile(add);

            for (const auto &nxt : cur->ch) {
                if (nxt->move == add) {
                    return std::make_pair(nxt.get(), b);
                }
            }
        }

        assert(false);
        return std::make_pair(cur, b);
    }

    long double rollout(bitboard b, bool is_chance) {
        while (!b.terminal()) {
            if (is_chance) {
                // transition to a decision node
                b.play_move(b.gen_move());
            } else {
                // transition to a randomly generated tile
                b.play_tile(b.gen_tile());
            }

            is_chance = !is_chance;
        }

        return (long double) b.score / MAX_SCORE;
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
        if (root->ch.empty()) {
            expand(root.get(), state);
        }

        bool played = state.play_move((bitboard::directions) dir);
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

    void play_tile(bitboard::board_t add) {
        assert(!root->is_chance);
        if (root->ch.empty()) {
            expand(root.get(), state);
        }

        state.play_tile(add);
        for (auto &nxt : root->ch) {
            if (nxt->move == add) {
                auto nxt_root = std::move(nxt);
                nxt_root->par = nullptr;
                root = std::move(nxt_root);
                return;
            }
        }

        assert(false);
    }
};