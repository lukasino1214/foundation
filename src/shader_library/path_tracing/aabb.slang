#pragma once

#include "hittable.slang"

struct AABBRT : Hittable {
    f32vec3 minimum;
    f32vec3 maximum;

    __init(f32vec3 center, f32vec3 extent) {
        minimum = center - extent;
        maximum = center + extent;
    }

    fn hit(Ray ray, Interval ray_t, inout HitRecord record) -> bool {
        const f32vec3 inverse_direction = 1.0f / ray.direction;
        const f32vec3 t_min = (minimum - ray.origin) * inverse_direction;
        const f32vec3 t_max = (maximum - ray.origin) * inverse_direction;
        const f32vec3 t1 = min(t_min, t_max);
        const f32vec3 t2 = max(t_min, t_max);
        const f32 t_near = max(max(t1.x, t1.y), t1.z);
        const f32 t_far = min(min(t2.x, t2.y), t2.z);

        if(t_near > t_far || !ray_t.surrounds(t_near)) {
            return false;
        }

        record.t = t_near;
        record.position = ray.at(record.t);
        const f32vec3 center = (minimum + maximum) / 2.0f;
        f32vec3 outward_normal = f32vec3(0.0, 0.0, 0.0);
        if(t_near == t1.x) { outward_normal = f32vec3(-1.0, 0.0, 0.0); }
        else if(t_near == t1.y) { outward_normal = f32vec3(0.0, -1.0, 0.0); }
        else if(t_near == t1.z) { outward_normal = f32vec3(0.0, 0.0, -1.0); }
        record.set_face_normal(ray, outward_normal);

        return true;
    }
}