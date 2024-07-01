#pragma once
#include "../../shared/shared.inl"

struct Interval {
    f32 min;
    f32 max;

    __init() {
        min = -1000000;
        max = 1000000;
    }

    __init(f32 _min, f32 _max) {
        min = _min;
        max = _max;
    }

    fn size() -> f32 {
        return max - min;
    }

    fn contains(f32 x) -> bool {
        return min <= x && x <= max;
    }

    fn surrounds(f32 x) -> bool {
        return min < x && x < max;
    }

    fn clamp(f32 x) -> f32 {
        if(x < min) return min;
        if(x > max) return max;
        return x;
    }
}