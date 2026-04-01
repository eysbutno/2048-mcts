#pragma once

#include <cstdint>
#include <random>
#include <chrono>
#include <cassert>
#include <array>

struct bitboard {
    using board_t = uint64_t;
    enum directions {
        LEFT, RIGHT, UP, DOWN, NONE
    };

    static constexpr int ROW_MASK = (1 << 16) - 1;

    enum table_type {
        TABLE_LEFT, TABLE_RIGHT, TABLE_LEFT_SCORE, TABLE_RIGHT_SCORE
    };

    static const std::array<int, 1 << 16> precompute_table(table_type t) {
        std::array<int, 1 << 16> res{};
        for (int i = 0; i < (1 << 16); i++) {
            std::array<int, 4> cur;
            if (t == TABLE_LEFT || t == TABLE_LEFT_SCORE) {
                cur = std::array<int, 4>{i & 0xF, (i >> 4) & 0xF, (i >> 8) & 0xF, (i >> 12) & 0xF};
            } else {
                cur = std::array<int, 4>{(i >> 12) & 0xF, (i >> 8) & 0xF, (i >> 4) & 0xF, i & 0xF};
            }

            int ptr = 0;
            std::array<int, 4> tmp{};
            for (int j = 0; j < 4; j++) {
                if (cur[j] > 0) tmp[ptr++] = cur[j];
            }

            std::array<int, 4> line{};
            int lptr = 0;
            for (int j = 0; j < ptr; ) {
                if (j + 1 < ptr && tmp[j] == tmp[j + 1] && tmp[j] != 0xF) {
                    if (t == TABLE_LEFT_SCORE || t == TABLE_RIGHT_SCORE) {
                        res[i] += 1 << (tmp[j] + 1);
                    } else {
                        line[lptr++] = tmp[j] + 1;
                    }

                    j += 2;
                } else {
                    if (t == TABLE_LEFT || t == TABLE_RIGHT) {
                        line[lptr++] = tmp[j];
                    }

                    j++;
                }
            }

            if (t == TABLE_LEFT || t == TABLE_RIGHT) {
                if (t == TABLE_LEFT) {
                    for (int j = 3; j >= 0; j--) {
                        res[i] <<= 4;
                        res[i] ^= line[j];
                    }
                } else {
                    for (int j = 0; j < 4; j++) {
                        res[i] <<= 4;
                        res[i] ^= line[j];
                    }
                }

                res[i] ^= i;
            }
        }

        return res;
    }

    static inline const auto left_table = precompute_table(TABLE_LEFT);
    static inline const auto right_table = precompute_table(TABLE_RIGHT);
    static inline const auto scores_left = precompute_table(TABLE_LEFT_SCORE);
    static inline const auto scores_right = precompute_table(TABLE_RIGHT_SCORE);

    board_t mask;
    int score;

    bitboard() : mask(0), score(0) {
        play_tile(gen_tile());
        play_tile(gen_tile());
    }

    static int count_open(board_t x) {
        x |= (x >> 2) & 0x3333333333333333ULL;
        x |= (x >> 1);
        x = ~x & 0x1111111111111111ULL;
        x += x >> 32;
        x += x >> 16;
        x += x >>  8;
        x += x >>  4;
        return x & 0xf;
    }

    static board_t transpose(board_t x) {
        board_t a1 = x & 0xF0F00F0FF0F00F0FULL;
        board_t a2 = x & 0x0000F0F00000F0F0ULL;
        board_t a3 = x & 0x0F0F00000F0F0000ULL;
        board_t a = a1 | (a2 << 12) | (a3 >> 12);
        board_t b1 = a & 0xFF00FF0000FF00FFULL;
        board_t b2 = a & 0x00FF00FF00000000ULL;
        board_t b3 = a & 0x00000000FF00FF00ULL;
        return b1 | (b2 >> 24) | (b3 << 24);
    }

    static int get_rand(int bound) {
        static std::mt19937 rng{(uint32_t)std::chrono::steady_clock::now().time_since_epoch().count()};
        assert(bound > 0);
        return std::uniform_int_distribution<int>(0, bound - 1)(rng);
    }

    board_t gen_tile() const {
        board_t x = mask;
        int idx = x == 0 ? 16 : count_open(x);
        assert(idx > 0);
        idx = get_rand(idx);

        board_t tile = get_rand(10) < 9 ? 1 : 2; // use uniform_real_dist later? dunno if that improves perf
        while (true) {
            while ((x & 0xf) != 0) {
                x >>= 4;
                tile <<= 4;
            }

            if (idx == 0) break;
            idx--;

            x >>= 4;
            tile <<= 4;
        }

        return tile;
    }

    void play_tile(board_t tile) {
        mask ^= tile;
    }

    bool play_move(directions dir) {
        board_t x = mask;
        if (dir == LEFT) {
            for (int t = 0; t < 4; t++) {
                board_t row = (mask >> (t * 16)) & ROW_MASK;
                x ^= (board_t) left_table[row] << (t * 16);
                score += scores_left[row];
            }
        } else if (dir == RIGHT) {
            for (int t = 0; t < 4; t++) {
                board_t row = (mask >> (t * 16)) & ROW_MASK;
                x ^= (board_t) right_table[row] << (t * 16);
                score += scores_right[row];
            }
        } else if (dir == UP) {
            x = transpose(x);

            for (int t = 0; t < 4; t++) {
                board_t row = (x >> (t * 16)) & ROW_MASK;
                x ^= (board_t) left_table[row] << (t * 16);
                score += scores_left[row];
            }

            x = transpose(x);
        } else {
            x = transpose(x);

            for (int t = 0; t < 4; t++) {
                board_t row = (x >> (t * 16)) & ROW_MASK;
                x ^= (board_t) right_table[row] << (t * 16);
                score += scores_right[row];
            }

            x = transpose(x);
        }

        if (x == mask) return false;

        mask = x;
        return true; 
    }

    directions gen_move() const {
        int num_good = 0;
        for (int i = 0; i < 4; i++) {
            bitboard cpy = *this;
            if (cpy.play_move((directions) i)) {
                num_good++;
            }
        }

        if (num_good == 0) return NONE;
        num_good = get_rand(num_good);
        for (int i = 0; i < 4; i++) {
            bitboard cpy = *this;
            if (cpy.play_move((directions) i)) {
                if (num_good == 0) return (directions) i;
                num_good--;
            }
        }

        return NONE;
    }

    bool terminal() const {
        for (int i = 0; i < 4; i++) {
            bitboard cpy = *this;
            if (cpy.play_move((directions) i)) {
                return false;
            }
        }

        return true;
    }

    int at(int r, int c) const {
        return (mask >> (4 * (r * 4 + c))) & 0xF;
    }

    static board_t tile_repr(int r, int c, int v) {
        return (board_t) v << (4 * (r * 4 + c));
    }
};