#pragma once
#include <chrono>
namespace pfd {
struct Timer {
    using clk = std::chrono::steady_clock;
    clk::time_point t0 = clk::now(), last = t0;
    double lap() { auto n = clk::now(); double ms = std::chrono::duration<double, std::milli>(n - last).count(); last = n; return ms; }
    double total() const { return std::chrono::duration<double, std::milli>(clk::now() - t0).count(); }
};
}  // namespace pfd
