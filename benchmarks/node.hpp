#include <vector>
#include <memory>
#include <cmath>
#include "config.hpp"

struct node {
    static constexpr long double INF = 1e9;

    long double q;
    int n;
    bool is_chance;
    std::vector<std::unique_ptr<node>> ch;
    node* par;

    node() : q(0), n(0), is_chance(true), ch(), par(nullptr) {}

    node(bool _is_chance, node* _par) 
        : q(0), n(0), is_chance(_is_chance), ch(), par(_par) {}

    void alloc() {
        is_chance ? ch.resize(4) : ch.resize(64);
    }

    void add_ch(int idx, bool is_chance) {
        ch[idx] = std::make_unique<node>(is_chance, this);
    }

    long double value() const {
        if (n == 0) return INF;
        return q / n + config::C * std::sqrt(std::log((long double) par->n) / n);
    }
};