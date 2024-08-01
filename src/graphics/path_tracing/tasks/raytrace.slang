#include "daxa/daxa.inl"
#include "raytrace.inl"

#include "../../../shader_library/path_tracing/sphere.slang"
#include "../../../shader_library/path_tracing/aabb.slang"

[[vk::push_constant]] RayTracePush push;

struct World {
    fn hit(Ray ray, Interval ray_t, inout HitRecord record) -> bool {
        HitRecord temp_record;
        bool hit_anything = false;
        f32 closest_so_far = ray_t.max;

        Sphere sphere = Sphere(f32vec3(0,0,-1), 0.5);

        Sphere spheres[2];
        spheres[0] = Sphere(f32vec3(0,0,0), 0.5);
        spheres[1] = Sphere(f32vec3(0,-102.5,-1), 100);

        for(u32 i = 0; i < 2; i++) {
            if(spheres[i].hit(ray, Interval(ray_t.min, closest_so_far), temp_record)) {
                hit_anything = true;
                closest_so_far = temp_record.t;
                record = temp_record;
            }
        }

        AABBRT aabb = AABBRT(f32vec3(0, 5, 0), f32vec3(0.5));
        if(aabb.hit(ray, Interval(ray_t.min, closest_so_far), temp_record)) {
            hit_anything = true;
            closest_so_far = temp_record.t;
            record = temp_record;
        }

        return hit_anything;
    }
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(u32vec3 pixel : SV_DispatchThreadID) {
    if(any(pixel.xy >= deref(push.uses.u_globals).render_target_size)) { return; }

    f32vec2 ray_uv = f32vec2(pixel.xy) / f32vec2(deref(push.uses.u_globals).render_target_size.xy - 1.0);
    f32vec3 origin = mul(deref(push.uses.u_globals).camera_inverse_view_matrix, f32vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    f32vec2 ray_ndc = ray_uv * 2.0 - 1.0;
    f32vec4 ray_view_space = deref(push.uses.u_globals).camera_inverse_projection_matrix * f32vec4(ray_ndc, -1.0, 1.0);
    f32vec4 ray_world_space = deref(push.uses.u_globals).camera_inverse_view_matrix * f32vec4(ray_view_space.xyz, 0.0);
    Ray ray = Ray(deref(push.uses.u_globals).camera_position, normalize(ray_world_space.xyz));

    f32vec3 color = f32vec3(1.0);

    World world;
    HitRecord record;
    if(world.hit(ray, Interval(0, 10000000), record)) {
        color = 0.5 * (record.normal + 1);
    } else {
        f32vec3 unit_direction = normalize(ray.direction);
        f32 a = 0.5*(unit_direction.y + 1.0);
        color = (1.0-a)* f32vec3(1.0, 1.0, 1.0) + a*f32vec3(0.5, 0.7, 1.0);
    }

    RWTexture2D<f32vec4> image = RWTexture2D<f32>::get(push.uses.u_image);
    image[pixel.xy] = f32vec4(pow(color, sqrt(2.2)), 1.0f);
}