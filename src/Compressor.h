#pragma once

#include <algorithm>
#include <cmath>

namespace nx
{
inline double gain(double db) noexcept { return std::pow(10.0, db / 20.0); }
inline double decibels(double linear) noexcept { return 20.0 * std::log10(std::max(linear, 0.00001)); }

inline double reduction(double input, double threshold, double ratio, double knee, double range) noexcept
{
    const auto over = input - threshold;
    const auto slope = 1.0 - 1.0 / ratio;
    const auto half = knee * 0.5;
    if (over <= -half)
        return 0.0;
    if (over >= half)
        return std::min(range, slope * over);
    return std::min(range, slope * (over + half) * (over + half) / (2.0 * knee));
}

inline double smoothing(double milliseconds, double rate) noexcept
{
    return std::exp(-1.0 / (0.001 * milliseconds * rate));
}

inline double automaticMakeup(double threshold, double ratio, double knee, double range) noexcept
{
    return std::min(6.0, 0.5 * reduction(-18.0, threshold, ratio, knee, range));
}
}
