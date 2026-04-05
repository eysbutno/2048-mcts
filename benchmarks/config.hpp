#pragma once

namespace config {
    long double C;
    double OPEN_WT;
    double MONO_WT;
    double SUM_WT;

    void init(long double _C, double _OPEN_WT, double _MONO_WT, double _SUM_WT) {
        C = _C;
        OPEN_WT = _OPEN_WT;
        MONO_WT = _MONO_WT;
        SUM_WT = _SUM_WT;
    }
}