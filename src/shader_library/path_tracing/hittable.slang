#pragma once

#include "ray.slang"
#include "interval.slang"

struct HitRecord {
    f32vec3 position;
    f32vec3 normal;
    f32 t;
    bool front_face;

    [mutating]
    void set_face_normal(Ray ray, f32vec3 outward_normal) {
        front_face = dot(ray.direction, outward_normal) < 0;
        normal = front_face ? outward_normal : -outward_normal;
    }
}

interface Hittable {
    fn hit(Ray ray, Interval ray_t, inout HitRecord record) -> bool;
};