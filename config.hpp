#pragma once

#include <cstdint>

namespace config {
    constexpr long double C = 0.1;
    constexpr float SCORE_LOST_PENALTY = 200000.0f;
    constexpr float SCORE_MONOTONICITY_POWER = 4.0f;
    constexpr float SCORE_MONOTONICITY_WEIGHT = 47.0f;
    constexpr float SCORE_SUM_POWER = 3.5f;
    constexpr float SCORE_SUM_WEIGHT = 11.0f;
    constexpr float SCORE_MERGES_WEIGHT = 700.0f;
    constexpr float SCORE_EMPTY_WEIGHT = 270.0f;
}