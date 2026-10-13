// C++ client: RAII wrapper over the C API, errors turned into exceptions on the C++ side only.
#include "acc.h"
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

class Accumulator {
    acc_t *h_;
public:
    Accumulator() : h_(acc_new()) { if (!h_) throw std::bad_alloc(); }
    ~Accumulator() { acc_free(h_); }                           // the handle is released on every path, including exceptions
    Accumulator(const Accumulator &) = delete;                 // a copy would double-free
    Accumulator &operator=(const Accumulator &) = delete;
    void add(double x) { int rc = acc_add(h_, x); if (rc != ACC_OK) throw std::runtime_error(acc_strerror(rc)); }
    acc_stats stats() const { acc_stats s; int rc = acc_stats_get(h_, &s); if (rc != ACC_OK) throw std::runtime_error(acc_strerror(rc)); return s; }
    std::vector<double> values() const {
        std::vector<double> out;
        // a captureless lambda converts to a C function pointer; the vector travels through the void* user pointer
        acc_foreach(h_, [](double v, void *u) { static_cast<std::vector<double> *>(u)->push_back(v); }, &out);
        return out;
    }
    std::string describe() const {
        char *s = acc_describe(h_);                            // library-allocated: must go back to acc_string_free
        std::string r = s ? s : "";
        acc_string_free(s);
        return r;
    }
};

int main() {
    Accumulator a;
    for (double x : {3.0, 1.5, 4.5, 9.0}) a.add(x);
    std::printf("C++ : %s\n", a.describe().c_str());
    std::printf("C++ : values copied through the callback:");
    for (double v : a.values()) std::printf(" %g", v);
    std::printf("\n");
    try { Accumulator empty; empty.stats(); } catch (const std::exception &e) { std::printf("C++ : error from the C library became an exception: %s\n", e.what()); }
    return 0;
}
