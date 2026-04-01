#include <cstdint>
#include <random>
#include <chrono>
#include <cassert>
#include <array>

struct bitboard {
    using board_t = uint64_t;
    enum directions {
        LEFT, RIGHT, UP, DOWN
    };

    static constexpr int ROW_MASK = (1 << 16) - 1;

    enum table_type {
        TABLE_LEFT, TABLE_RIGHT, TABLE_LEFT_SCORE, TABLE_RIGHT_SCORE
    };

    static const std::array<uint64_t, 1 << 16> precompute_table(table_type t) {
        std::array<uint64_t, 1 << 16> res{};
        for (int i = 0; i < (1 << 16); i++) {
            std::array<int, 4> cur;
            if (t == TABLE_LEFT || TABLE_LEFT_SCORE) {
                cur = std::array<int, 4>{i & 0xF, (i >> 4) & 0xF, (i >> 8) & 0xF, (i >> 12) & 0xF};
            } else {
                cur = std::array<int, 4>{(i >> 12) & 0xF, (i >> 8) & 0xF}, (i >> 4) & 0xF, i & 0xF};
            }

            int ptr = 0;
            std::array<int, 4> tmp{};
            for (int j = 0; i < 4; j++) {
                if (cur[j] > 0) tmp[ptr++] = cur[j];
            }

            for (int j = 0; j < ptr; ) {
                if (j + 1 < ptr && tmp[j] == tmp[j + 1]) {
                    if (t == TABLE_LEFT_SCORE || t == TABLE_RIGHT_SCORE) {
                        res[i] += 1 << (tmp[j] + 1);
                    } else {
                        res[i] <<= 4;
                        res[i] ^= tmp[j] + 1;
                    }

                    j += 2;
                } else {
                    if (t == TABLE_LEFT || t == TABLE_RIGHT) {
                        res[i] <<= 4;
                        res[i] ^= tmp[j];
                    }

                    j++;
                }
            }
        }

        return res;
    }

    static inline const std::array<uint64_t, 1 << 16> left_table = precompute_table(TABLE_LEFT);
    static inline const std::array<uint64_t, 1 << 16> right_table = precompute_right_table(TABLE_RIGHT);
    static inline const std::array<uint64_t, 1 << 16> scores_left = precompute_table(TABLE_LEFT_SCORE);
    static inline const std::array<uint64_t, 1 << 16> scores_right = precompute_table(TABLE_RIGHT_SCORE);

    board_t mask;
    int score;

    bitboard() : mask(0), score(0) {
        play_tile(gen_tile(*this));
        play_tile(gen_tile(*this));
    }

    static int count_open(bitboard b) {
        board_t x = b.mask;
        x |= (x >> 2) & 0x3333333333333333ULL;
        x |= (x >> 1);
        x = ~x & 0x1111111111111111ULL;
        x += x >> 32;
        x += x >> 16;
        x += x >>  8;
        x += x >>  4;
        return x & 0xf;
    }

    static board_t transpose(bitboard b) {
        board_t x = b.mask;
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

    static board_t gen_tile(bitboard b) {
        board_t x = b.mask;
        int idx = b.mask == 0 ? 16 : count_open(b);
        assert(idx > 0);
        idx = get_rand(idx);

        board_t tile = get_rand(10) < 9 ? 2 : 4; // use uniform_real_dist later? dunno if that improves perf
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

    void play_move(directions dir) {
        // WIP
    }
};

/**
 * how to do board repr?
 * 
 * - assuming that max tile is 32k cuz I would be really shocked if higher
 * - 4 bits (for the power) at each, each row is 16 bits
 * - steal some bitwise transpose code trust, so we can treat as analogous
 * - since each row is 2^16, we can precompute results for every possible shift
 * - just need reversal code
 */