#include "bitboard.hpp"
#include "node.hpp"
#include <chrono>
#include <algorithm>
#include <iostream>

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

        while (!b.terminal() && !cur->ch.empty()) {
            if (cur->is_chance) {
                node* nxt = nullptr;
                int move = -1;
                for (int i = 0; i < 4; i++) {
                    if (cur->ch[i] != nullptr && (nxt == nullptr || nxt->value() < cur->ch[i]->value())) {
                        nxt = cur->ch[i].get();
                        move = i;
                    }
                }

                b.play_move((bitboard::directions) move);
                cur = nxt;
            } else {
                // transition to a randomly generated tile
                int add = b.gen_tile_idx();
                b.play_tile(bitboard::to_tile(add));
                cur = cur->ch[add].get();
            }
        }

        return std::make_pair(cur, b);
    }

    std::pair<node*, bitboard> expand(node* cur, bitboard b) {
        if (b.terminal()) {
            return std::make_pair(cur, b);
        }

        cur->alloc();

        if (cur->is_chance) {
            // transition to a decision node
            for (int i = 0; i < 4; i++) {
                auto nxt = b;
                if (nxt.play_move((bitboard::directions) i)) {
                    cur->add_ch(i, false);
                }
            }

            int move = b.gen_move();
            assert("move must be in list" && (cur->ch[move] != nullptr));
            node* chosen = cur->ch[move].get();
            b.play_move((bitboard::directions) move);
            return std::make_pair(chosen, b);
        } else {
            // transition to a randomly generated tile
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    if (b.at(i, j) == 0) {
                        for (int v : {1, 2}) {
                            int idx = 4 * (i * 4 + j) + v;
                            cur->add_ch(idx, true);
                        }
                    }
                }
            }

            int add = b.gen_tile_idx();
            b.play_tile(bitboard::to_tile(add));
            assert("add must be in list" && (cur->ch[add] != nullptr));
            return std::make_pair(cur->ch[add].get(), b);
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
        node* nxt = nullptr;
        int move = -1;
        for (int i = 0; i < 4; i++) {
            if (root->ch[i] != nullptr && (nxt == nullptr || (nxt->q / nxt->n) < (root->ch[i]->q / root->ch[i]->n))) {
                nxt = root->ch[i].get();
                move = i;
            }
        }

        return move;
    }

    void play_move(int dir) {
        assert(root->is_chance);
        if (root->ch.empty()) {
            expand(root.get(), state);
        }

        bool played = state.play_move((bitboard::directions) dir);
        assert(played);
        auto nxt_root = std::move(root->ch[dir]);
        nxt_root->par = nullptr;
        root = std::move(nxt_root);
    }

    void play_tile(int move) {
        assert(!root->is_chance);
        if (root->ch.empty()) {
            expand(root.get(), state);
        }

        state.play_tile(bitboard::to_tile(move));
        auto nxt_root = std::move(root->ch[move]);
        nxt_root->par = nullptr;
        root = std::move(nxt_root);
    }
};