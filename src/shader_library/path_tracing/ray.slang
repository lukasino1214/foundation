#pragma once
#include "../../shared/shared.inl"

struct Ray {
    f32vec3 origin;
    f32vec3 direction;

    __init(f32vec3 _origin, f32vec3 _direction) {
        origin = _origin;
        direction = _direction;
    }

    fn at(f32 t) -> f32vec3 {
        return origin + direction * t;
    }
};