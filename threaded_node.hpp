#include <vector>
#include <memory>
#include <cmath>
#include <atomic>
#include <mutex>

struct threaded_node {
    static constexpr long double C = 0.03;
    static constexpr long double INF = 1e9;

    long double q;
    std::atomic<int> n;
    bool is_chance;
    int move;
    std::vector<std::unique_ptr<threaded_node>> ch;
    threaded_node* par;
    std::mutex mtx, mtx2;

    threaded_node() : q(0), n(0), is_chance(true), move(-1), ch(), par(nullptr), mtx(), mtx2() {}

    threaded_node(bool _is_chance, int _move, threaded_node* _par) 
        : q(0), n(0), is_chance(_is_chance), move(_move), ch(), par(_par), mtx(), mtx2() {
        if (is_chance) {
            ch.reserve(32);
        } else {
            ch.reserve(4);
        }
    }

    bool has_ch() {
        std::lock_guard<std::mutex> lock(mtx);
        return !ch.empty();
    }

    bool has_ch_ul() {
        return !ch.empty();
    }

    void add_ch(bool is_chance, int move) {
        ch.push_back(std::make_unique<threaded_node>(is_chance, move, this));
    }

    void upd(long double q_a, int n_a = 1) {
        std::lock_guard<std::mutex> lock(mtx2);
        q += q_a;
        n += n_a;
    }

    long double value() {
        std::lock_guard<std::mutex> lock(mtx2);
        if (n == 0) return INF;
        return q / n + C * std::sqrt(std::log(par->n.load()) / n);
    }
};