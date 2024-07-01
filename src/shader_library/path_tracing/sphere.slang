#pragma once

#include "hittable.slang"

struct Sphere : Hittable {
    f32vec3 position;
    f32 radius;

    __init(f32vec3 _position, f32 _radius) {
        position = _position;
        radius = _radius;
    }

    fn hit(Ray ray, Interval ray_t, inout HitRecord record) -> bool {
        const f32vec3 oc = position - ray.origin;
        const f32 a = dot(ray.direction, ray.direction);
        const f32 h = dot(ray.direction, oc);
        const f32 c = dot(oc, oc) - radius * radius;
        const f32 discriminant = h*h - a*c;

        if(discriminant < 0) {
            return false;
        }

        const f32 sqrtd = sqrt(discriminant);

        f32 root = (h - sqrtd) / a;
        if(!ray_t.surrounds(root)) {
            root = (h + sqrtd) / a;
            if(!ray_t.surrounds(root)) {
                return false;
            }
        }

        record.t = root;
        record.position = ray.at(record.t);
        f32vec3 outward_normal = (record.position - position) / radius;
        record.set_face_normal(ray, outward_normal);

        return true;
    }
}