#pragma once

#include <array>
#include <chrono>
#include <random>
#include <utility>
#include <cassert>

struct board {
    enum directions {
        LEFT, RIGHT, UP, DOWN
    };

    std::array<std::array<int, 4>, 4> grid;
    int score;
    int open;
    bool has_adj;

    board() : grid(), score(0), open(16), has_adj(false) {}

    /**
     * @return a random value in the range [0, bound], provided bound >= 0
     */
    inline int get_rand(int bound) const {
        thread_local static std::mt19937 rng{(uint32_t)std::chrono::steady_clock::now().time_since_epoch().count()};
        return std::uniform_int_distribution<int>(0, bound)(rng);
    }

    inline bool terminal() const {
        return open == 0 && !has_adj;
    }

    std::pair<int, int> gen_tile() const {
        assert(open > 0);
        int pos = get_rand(open - 1);
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (grid[i][j] > 0) continue;

                if (pos == 0) {
                    int v = get_rand(9) <= 8 ? 2 : 4;
                    return std::make_pair(i * 4 + j, v);
                } else {
                    pos--;
                }
            }
        }

        assert(false);
        return std::make_pair(-1, -1);
    }

    /**
     * Checks if we can play this move and have it do anything.
     * @param direction in question
     */
    bool can_play(directions dir) const {
        if (dir == LEFT) {
            for (int i = 0; i < 4; i++) {
                bool has_0 = false;
                for (int j = 0; j < 4; j++) {
                    has_0 |= grid[i][j] == 0;
                    if (grid[i][j] > 0 && (has_0 || (j && grid[i][j] == grid[i][j - 1])))
                        return true;
                }
            }
        } else if (dir == RIGHT) {
            for (int i = 0; i < 4; i++) {
                bool has_0 = false;
                for (int j = 3; j >= 0; j--) {
                    has_0 |= grid[i][j] == 0;
                    if (grid[i][j] > 0 && (has_0 || (j + 1 < 4 && grid[i][j] == grid[i][j + 1])))
                        return true;
                }
            }
        } else if (dir == UP) {
            for (int j = 0; j < 4; j++) {
                bool has_0 = false;
                for (int i = 0; i < 4; i++) {
                    has_0 |= grid[i][j] == 0;
                    if (grid[i][j] > 0 && (has_0 || (i && grid[i][j] == grid[i - 1][j])))
                        return true;
                }
            }
        } else if (dir == DOWN) {
            for (int j = 0; j < 4; j++) {
                bool has_0 = false;
                for (int i = 3; i >= 0; i--) {
                    has_0 |= grid[i][j] == 0;
                    if (grid[i][j] > 0 && (has_0 || (i + 1 < 4 && grid[i][j] == grid[i + 1][j])))
                        return true;
                }
            }
        }

        return false;
    }

    /**
     * Performs a merge to the left, in the same way as traditional 2048.
     * @param line the line of values in question
     * @param moved reference to store if any movement has been made
     * @return number of merges that happened
     */
    int merge_line(std::array<int, 4> &line, bool &moved) {
        std::array<int, 4> tmp{};
        int count = 0;
        for (int i = 0; i < 4; i++) {
            if (line[i]) tmp[count++] = line[i];
        }

        moved |= tmp != line;
 
        std::array<int, 4> result{};
        int merges = 0;
        int ptr = 0;
        for (int i = 0; i < count; ) {
            if (i + 1 < count && tmp[i] == tmp[i + 1]) {
                result[ptr++] = tmp[i] * 2;
                score += tmp[i] * 2;
                merges++;
                i += 2;
                moved = true;
            } else {
                result[ptr++] = tmp[i++];
            }
        }
 
        line = std::move(result);
        return merges;
    }

    /**
     * Plays the move's direction in question
     * @param dir the enum used for each direction
     * @return if you are allowed to play said move
     */
    bool play_move(directions dir) {
        bool moved = false;
        int prev_open = open;
 
        if (dir == LEFT) {
            for (int r = 0; r < 4; r++) {
                open += merge_line(grid[r], moved);
            }
 
        } else if (dir == RIGHT) {
            for (int r = 0; r < 4; r++) {
                std::array<int, 4> line = {grid[r][3], grid[r][2], grid[r][1], grid[r][0]};
                open += merge_line(line, moved);
                grid[r] = {line[3], line[2], line[1], line[0]};
            }
 
        } else if (dir == UP) {
            for (int c = 0; c < 4; c++) {
                std::array<int, 4> line = {grid[0][c], grid[1][c], grid[2][c], grid[3][c]};
                open += merge_line(line, moved);
                for (int r = 0; r < 4; r++) { grid[r][c] = line[r]; }
            }
 
        } else if (dir == DOWN) {
            for (int c = 0; c < 4; c++) {
                std::array<int, 4> line = {grid[3][c], grid[2][c], grid[1][c], grid[0][c]};
                open += merge_line(line, moved);
                for (int r = 0; r < 4; r++) grid[r][c] = line[3 - r];
            }
        }

        has_adj = false;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (i + 1 < 4) has_adj |= grid[i][j] != 0 && grid[i][j] == grid[i + 1][j];
                if (j + 1 < 4) has_adj |= grid[i][j] != 0 && grid[i][j] == grid[i][j + 1];
            }
        }
 
        return moved;
    }

    void add_tile(const std::pair<int, int> &tile) {
        const auto &[loc, val] = tile;
        int i = loc >> 2, j = loc & 3;
        assert(grid[i][j] == 0);
        grid[i][j] = val;
        has_adj |= i && val == grid[i - 1][j];
        has_adj |= i + 1 < 4 && val == grid[i + 1][j];
        has_adj |= j && val == grid[i][j - 1];
        has_adj |= j + 1 < 4 && val == grid[i][j + 1];
        open--;
    }
};