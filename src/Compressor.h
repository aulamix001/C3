#pragma once

#include <algorithm>
#include <cmath>

namespace nx
{
inline float gain(float db) noexcept { return std::pow(10.f, db / 20.f); }
inline float decibels(float linear) noexcept { return 20.f * std::log10(std::max(linear, 0.00001f)); }

inline float reduction(float input, float threshold, float ratio, float knee, float range) noexcept
{
    const auto over = input - threshold;
    const auto slope = 1.f - 1.f / ratio;
    const auto half = knee * 0.5f;
    if (over <= -half)
        return 0.f;
    if (over >= half)
        return std::min(range, slope * over);
    return std::min(range, slope * (over + half) * (over + half) / (2.f * knee));
}

inline float smoothing(float milliseconds, double rate) noexcept
{
    return std::exp(-1.f / static_cast<float>(0.001 * milliseconds * rate));
}
}
