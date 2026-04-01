#include <vector>
#include <memory>
#include <cmath>

struct node {
    static constexpr long double C = 0.01;
    static constexpr long double INF = 1e9;

    long double q;
    int n;
    bool is_chance;
    int move;
    std::vector<std::unique_ptr<node>> ch;
    node* par;

    node() : q(0), n(0), is_chance(true), move(-1), ch(), par(nullptr) {}

    node(bool _is_chance, int _move, node* _par) 
        : q(0), n(0), is_chance(_is_chance), move(_move), ch(), par(_par) {
        if (is_chance) {
            ch.reserve(32);
        } else {
            ch.reserve(4);
        }
    }

    void add_ch(bool is_chance, int move) {
        ch.push_back(std::make_unique<node>(is_chance, move, this));
    }

    long double value() const {
        if (n == 0) return INF;
        return q / n + C * std::sqrt(std::log((long double) par->n) / n);
    }
};