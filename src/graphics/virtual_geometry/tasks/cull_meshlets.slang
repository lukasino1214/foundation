#include "daxa/daxa.inl"
#include "cull_meshlets.inl"

[[vk::push_constant]] CullMeshletsWriteCommandPush write_push;
[[vk::push_constant]] CullMeshletsPush push;

[shader("compute")]
[numthreads(1, 1, 1)]
void cull_meshlets_write_command() {
    const u32 needed_threads = write_push.uses.u_meshlets_data->count;
    const u32 needed_workgroups = round_up_div(needed_threads, 16);
    DispatchIndirectStruct command;
    command.x = needed_workgroups;
    command.y = 1;
    command.z = 1;
    *write_push.uses.u_command = command;
}

void add_aabb(ShaderDebugBufferHead* head, ShaderDebugAABBDraw draw) {
    u32 index = 0;
    InterlockedAdd(head->aabb_draw_indirect_info.instance_count, 1, index);
    head->aabb_draws[index] = draw;
}

struct NdcAABB {
    daxa_f32vec3 ndc_min;
    daxa_f32vec3 ndc_max;
};

NdcAABB calculate_ndc_aabb(daxa_f32mat4x4 model_matrix, AABB aabb) {
    bool initialized_min_max = false;
    NdcAABB ret;
    for (int z = -1; z <= 1; z += 2) {
        for (int y = -1; y <= 1; y += 2) {
            for (int x = -1; x <= 1; x += 2) {
                const daxa_f32vec3 model_corner_position = aabb.center + aabb.extent * daxa_f32vec3(x,y,z) * 0.5f;
                const daxa_f32vec4 worldspace_corner_position = mul(model_matrix, daxa_f32vec4(model_corner_position,1));
                const daxa_f32vec4 viewspace_corner_position = mul(push.uses.u_globals->camera_view_matrix, worldspace_corner_position);
                const daxa_f32vec4 clipspace_corner_position = mul(push.uses.u_globals->camera_projection_matrix, viewspace_corner_position);
                const daxa_f32vec3 ndc_corner_position = clipspace_corner_position.xyz / clipspace_corner_position.w;
                ret.ndc_min.x = !initialized_min_max ? ndc_corner_position.x : min(ndc_corner_position.x, ret.ndc_min.x);
                ret.ndc_min.y = !initialized_min_max ? ndc_corner_position.y : min(ndc_corner_position.y, ret.ndc_min.y);
                ret.ndc_min.z = !initialized_min_max ? ndc_corner_position.z : min(ndc_corner_position.z, ret.ndc_min.z);
                ret.ndc_max.x = !initialized_min_max ? ndc_corner_position.x : max(ndc_corner_position.x, ret.ndc_max.x);
                ret.ndc_max.y = !initialized_min_max ? ndc_corner_position.y : max(ndc_corner_position.y, ret.ndc_max.y);
                ret.ndc_max.z = !initialized_min_max ? ndc_corner_position.z : max(ndc_corner_position.z, ret.ndc_max.z);
                initialized_min_max = true;
            }
        }
    }

    ret.ndc_min.x = max(ret.ndc_min.x, -1.0f);
    ret.ndc_min.y = max(ret.ndc_min.y, -1.0f);
    ret.ndc_max.x = min(ret.ndc_max.x,  1.0f);
    ret.ndc_max.y = min(ret.ndc_max.y,  1.0f);
    return ret;
}

float4 texelFetch(Texture2D<float4> tex, int2 index, uint mip)
{
    return tex.Load(uint3(index, mip));
}

uint4 texelFetch(Texture2DArray<uint4> tex, int3 index, uint mip)
{
    return tex.Load(uint4(index, mip));
}

#define daxa_texture2D(TEX) Texture2D<float>::get(TEX)
#define greaterThan(X, Y) ((X) > (Y))
bool is_ndc_aabb_hiz_depth_visible(NdcAABB meshlet_ndc_aabb, daxa_u32vec2 hiz_res, daxa_ImageViewId hiz) {
    const daxa_f32vec2 f_hiz_resolution = hiz_res;
    const daxa_f32vec2 min_uv = (meshlet_ndc_aabb.ndc_min.xy + 1.0f) * 0.5f;
    const daxa_f32vec2 max_uv = (meshlet_ndc_aabb.ndc_max.xy + 1.0f) * 0.5f;
    const daxa_f32vec2 min_texel_i = floor(clamp(f_hiz_resolution * min_uv, daxa_f32vec2(0.0f, 0.0f), f_hiz_resolution - 1.0f));
    const daxa_f32vec2 max_texel_i = floor(clamp(f_hiz_resolution * max_uv, daxa_f32vec2(0.0f, 0.0f), f_hiz_resolution - 1.0f));
    const float pixel_range = max(max_texel_i.x - min_texel_i.x + 1.0f, max_texel_i.y - min_texel_i.y + 1.0f);
    const float mip = ceil(log2(max(2.0f, pixel_range))) - 1 /* we want one mip lower, as we sample a quad */;

    int imip = int(mip);
    const daxa_i32vec2 min_corner_texel = daxa_i32vec2(min_texel_i) >> imip;
    const daxa_i32vec2 max_corner_texel = daxa_i32vec2(max_texel_i) >> imip;
    if (any(greaterThan(max_corner_texel - min_corner_texel, daxa_i32vec2(1)))) {
        imip += 1;
    }
    const daxa_i32vec2 quad_corner_texel = daxa_i32vec2(min_texel_i) >> imip;
    const daxa_i32vec2 texel_bounds = max(daxa_i32vec2(0,0), (daxa_i32vec2(f_hiz_resolution) >> imip) - 1);

    const daxa_f32vec4 fetch = daxa_f32vec4(
        texelFetch(daxa_texture2D(hiz), clamp(quad_corner_texel + daxa_i32vec2(0,0), daxa_i32vec2(0,0), texel_bounds), imip).x,
        texelFetch(daxa_texture2D(hiz), clamp(quad_corner_texel + daxa_i32vec2(0,1), daxa_i32vec2(0,0), texel_bounds), imip).x,
        texelFetch(daxa_texture2D(hiz), clamp(quad_corner_texel + daxa_i32vec2(1,0), daxa_i32vec2(0,0), texel_bounds), imip).x,
        texelFetch(daxa_texture2D(hiz), clamp(quad_corner_texel + daxa_i32vec2(1,1), daxa_i32vec2(0,0), texel_bounds), imip).x
    );
    const float conservative_depth = min(min(fetch.x,fetch.y), min(fetch.z, fetch.w));
    const bool depth_cull = meshlet_ndc_aabb.ndc_max.z < conservative_depth;
    return depth_cull;
}

#define greaterThan(X, Y) ((X) > (Y))
#define lessThan(X, Y) ((X) < (Y))
#define lessThanEqual(X, Y) ((X) <= (Y))
#define greaterThanEqual(X, Y) ((X) >= (Y))
#define equal(X, Y) ((X) == (Y))

bool is_aabb_inside_plane(in f32vec3 center, in f32vec3 extent, in f32vec4 plane) {
    const f32vec3 normal = plane.xyz;
    const float radius = dot(extent, abs(normal));
    return (dot(normal, center) - plane.w) >= -radius;
}

bool is_meshlet_visible(AABB aabb, daxa_f32mat4x4 model) {
    const f32vec3 world_aabb_center = mul(model, f32vec4(aabb.center, 1.0)).xyz;
    const f32vec3 right = model[0].xyz * aabb.extent.x;
    const f32vec3 up = model[1].xyz * aabb.extent.y;
    const f32vec3 forward = (-model[2].xyz) * aabb.extent.z;

    const f32vec3 world_extent = f32vec3(
        abs(dot(f32vec3(1.0, 0.0, 0.0), right)) +
        abs(dot(f32vec3(1.0, 0.0, 0.0), up)) +
        abs(dot(f32vec3(1.0, 0.0, 0.0), forward)),

        abs(dot(f32vec3(0.0, 1.0, 0.0), right)) +
        abs(dot(f32vec3(0.0, 1.0, 0.0), up)) +
        abs(dot(f32vec3(0.0, 1.0, 0.0), forward)),

        abs(dot(f32vec3(0.0, 0.0, 1.0), right)) +
        abs(dot(f32vec3(0.0, 0.0, 1.0), up)) +
        abs(dot(f32vec3(0.0, 0.0, 1.0), forward))
    );

    const uint planes = 6;
    for (uint i = 0; i < planes; ++i) {
        if (!is_aabb_inside_plane(world_aabb_center, world_extent / 2, deref(push.uses.u_globals).frustum_planes[i])) {
            return false;
        }
    }

    return !is_ndc_aabb_hiz_depth_visible(calculate_ndc_aabb(model, aabb), push.uses.u_globals->next_lower_po2_render_target_size, push.uses.u_hiz);
}

void write_meshlet(MeshletIndices* meshlets_indices, in u32 index) {
    uint i = 0;
    InterlockedAdd(meshlets_indices->count, 1, i);
    meshlets_indices.indices[i] = index;
}

[shader("compute")]
[numthreads(16, 1, 1)]
void cull_meshlets(u32vec3 thread_index : SV_DispatchThreadID) {
    const u32 count = push.uses.u_meshlets_data->count;
    const u32 current_thread_index = thread_index.x;
    const bool thread_active = current_thread_index < count;

    if(current_thread_index == 0) {
        push.uses.u_hw_culled_meshlet_indices->count = 0;
        push.uses.u_sw_culled_meshlet_indices->count = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    if(thread_active) {
        const MeshletData meshlet_data = push.uses.u_meshlets_data->meshlets[current_thread_index];
        const Mesh mesh = push.uses.u_meshes[meshlet_data.mesh_index];
        const AABB aabb = mesh.meshlet_aabbs[meshlet_data.meshlet_index];
        const f32mat4x4 transform = push.uses.u_transforms[meshlet_data.transform_index].model_matrix;

        if(is_meshlet_visible(aabb, transform)) {
            const u32 viewport_width = push.uses.u_globals->render_target_size.x;
            const u32 viewport_height = push.uses.u_globals->render_target_size.y;

            const f32vec3 aabb_min = aabb.center - aabb.extent;
            const f32vec3 aabb_max = aabb.center + aabb.extent;
            const f32vec3 aabb_size = aabb_max - aabb_min + 1e-5;
            const f32vec3[8] corners = f32vec3[8](
                aabb_min,
                aabb_min + f32vec3(aabb_size.x, 0.0, 0.0),
                aabb_min + f32vec3(0.0, aabb_size.y, 0.0),
                aabb_min + f32vec3(0.0, 0.0, aabb_size.z),
                aabb_min + f32vec3(aabb_size.xy, 0.0),
                aabb_min + f32vec3(0.0, aabb_size.yz),
                aabb_min + f32vec3(aabb_size.x, 0.0, aabb_size.z),
                aabb_min + aabb_size);
            f32vec2 min_xy = f32vec2(3.402823466e+38);
            f32vec2 max_xy = f32vec2(-3.402823466e+38);
            float min_z = 3.402823466e+38;
            bool clip = false;
            for (uint i = 0; i < 8; ++i) {
                const daxa_f32vec4 worldspace_corner_position = mul(transform, daxa_f32vec4(corners[i], 1));
                const daxa_f32vec4 viewspace_corner_position = mul(push.uses.u_globals->camera_view_matrix, worldspace_corner_position);
                const daxa_f32vec4 clipspace_corner_position = mul(push.uses.u_globals->camera_projection_matrix, viewspace_corner_position);
                const f32vec3 ndc = clipspace_corner_position.xyz / clipspace_corner_position.w;
                min_z = min(min_z, ndc.z);
                if (ndc.x <= -1.0 || ndc.x >= 1.0 || ndc.y <= -1.0 || ndc.y >= 1.0 || min_z <= 0.0) {
                    clip = true;
                    break;
                }
                min_xy = min(min_xy, ndc.xy);
                max_xy = max(max_xy, ndc.xy);
            }
            i32vec2 min_viewport = i32vec2((min_xy * f32vec2(0.5, -0.5) + 0.5) * f32vec2(viewport_width, viewport_height));
            i32vec2 max_viewport = i32vec2((max_xy * f32vec2(0.5, -0.5) + 0.5) * f32vec2(viewport_width, viewport_height));
            min_viewport = max(min_viewport, i32vec2(0));
            max_viewport = min(max_viewport, i32vec2(viewport_width, viewport_height) - 1);

            const bool is_sw_candidate =
                !clip &&
                abs(max_viewport.x - min_viewport.x) < 32 &&
                abs(max_viewport.y - min_viewport.y) < 32;

            if(is_sw_candidate) {
                write_meshlet(push.uses.u_sw_culled_meshlet_indices, current_thread_index);
            } else {
                write_meshlet(push.uses.u_hw_culled_meshlet_indices, current_thread_index);
            }
        }
    }
}