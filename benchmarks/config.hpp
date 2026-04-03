#pragma once

namespace config {
    long double C;
    int OPEN_WT;
    int MONO_WT;
    int rollouts;

    void init(long double _C, int _OPEN_WT, int _MONO_WT) {
        C = _C;
        OPEN_WT = _OPEN_WT;
        MONO_WT = _MONO_WT;
    }
}