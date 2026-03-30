#include <vector>
#include <memory>
#include <cmath>
#include <atomic>
#include <mutex>

struct threaded_node {
    static constexpr double C = 0.3;
    static constexpr double INF = 1e9;

    std::atomic<long double> q;
    std::atomic<int> n, n_p;
    bool is_chance;
    int move;
    std::vector<std::unique_ptr<threaded_node>> ch;
    threaded_node* par;
    std::mutex mtx;

    threaded_node() : q(0), n(0), n_p(0), is_chance(true), move(-1), ch(), par(nullptr), mtx() {}

    threaded_node(bool _is_chance, int _move, threaded_node* _par) 
        : q(0), n(0), is_chance(_is_chance), move(_move), ch(), par(_par), mtx() {
        if (is_chance) {
            ch.reserve(32);
        } else {
            ch.reserve(4);
        }
    }

    std::unique_lock<std::mutex> lock_node() {
        return std::unique_lock<std::mutex>(mtx);
    }

    bool has_ch() {
        std::lock_guard<std::mutex> lock(mtx);
        return !ch.empty();
    }

    void add_ch(bool is_chance, int move) {
        std::lock_guard<std::mutex> lock(mtx);
        ch.push_back(std::make_unique<threaded_node>(is_chance, move, this));
    }

    void upd(long double q_a, int n_a = 1) {
        q.fetch_add(q_a);
        n += n_a;
        n_p += n_a;
    }

    long double value() {
        int n_val = n.load();
        if (n_val == 0) return INF;
        return q.load() / n_val + C * std::sqrt(std::log(n_p.load()) / n_val);
    }
};