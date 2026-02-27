#include <daxa/daxa.inl>
#include "gen_hiz.inl"

#define TILE_WIDTH 64
#define TILE_WIDTH_LOG2 6
#define TILE_SIZE i32vec2(TILE_WIDTH,TILE_WIDTH)
#define TILE_THREAD_WIDTH 16
#define TILE_THREAD_COUNT (TILE_THREAD_WIDTH * TILE_THREAD_WIDTH)

[[vk::push_constant]] GenHizPush push;

groupshared f32 s_mins[2 * TILE_THREAD_WIDTH * TILE_THREAD_WIDTH];

f32vec4 downsample_tile_mip_n0<bool BASE_CASE>(i32 start_mip, u32vec2 tile, u32vec2 tile_thread) {
    f32vec4 quad_values = { 1.0f,1.0f,1.0f,1.0f };

    [[unroll]]
    for (u32 quad_i = 0; quad_i < 4; quad_i++) {
        const i32vec2 sub_i = i32vec2(quad_i >> 1, quad_i & 1);

        const i32vec2 dst_index = i32vec2((tile * TILE_THREAD_WIDTH + tile_thread) * 2 + sub_i);
        const i32 dst_mip = start_mip + 1;

        const i32vec2 src_index = dst_index * 2;
        const i32 src_mip = start_mip;
        const u32vec2 dst_mip_size = (push.dst_mip0_size >> dst_mip);

        if(all(lessThan(dst_index, dst_mip_size))) {
            f32vec4 fetch;
            if(BASE_CASE) {
                const f32vec2 src_inv_size = rcp(f32vec2(push.src_size));
                fetch = push.attach.src.get().GatherRed(push.attach.globals.samplers.linear_clamp.get(), make_gather_uv(src_inv_size, src_index), 0);
            } else {
                const u32vec2 src_max_index = (push.dst_mip0_size >> start_mip) - 1;
                fetch.x = push.attach.hiz[src_mip].get_coherent().Load(min(src_index + i32vec2(0,0), src_max_index));
                fetch.y = push.attach.hiz[src_mip].get_coherent().Load(min(src_index + i32vec2(0,1), src_max_index));
                fetch.z = push.attach.hiz[src_mip].get_coherent().Load(min(src_index + i32vec2(1,0), src_max_index));
                fetch.w = push.attach.hiz[src_mip].get_coherent().Load(min(src_index + i32vec2(1,1), src_max_index));
            }
            const f32 min_v = min(min(fetch.x, fetch.y), min(fetch.z, fetch.w));

            push.attach.hiz[dst_mip].get()[dst_index] = min_v;
            quad_values[quad_i] = min_v;
        }
    }
    return quad_values;
}

void downsample_tile_mip_n1(i32 start_mip, u32vec2 tile, u32vec2 tile_thread, f32vec4 quad_values) {
    const i32 dst_mip = start_mip + 2;
    if(dst_mip >= push.mip_count) return;

    const f32 min_v = min(min(quad_values.x, quad_values.y), min(quad_values.z, quad_values.w));
    const i32vec2 dst_index = i32vec2(tile * TILE_THREAD_WIDTH + tile_thread);
    
    const u32vec2 dst_mip_size = (push.dst_mip0_size >> dst_mip);
    if(all(lessThan(dst_index, dst_mip_size))) {
        push.attach.hiz[dst_mip].get()[dst_index] = min_v;
    }

    s_mins[tile_thread.y * TILE_THREAD_WIDTH + tile_thread.x] = min_v;
}

void downsample_tile_mip_n2_n5(i32 start_mip, u32vec2 tile, u32vec2 tile_thread, i32 mip_index) {
    const i32vec2 read_mip_tile_corner_index   = tile * TILE_SIZE;
    const u32 dst_mip = start_mip + 3 + mip_index;
    const u32 ping_pong_src_index = (mip_index & 1u);
    const u32 ping_pong_dst_index = ((mip_index+1) & 1u);

    GroupMemoryBarrierWithGroupSync();

    const u32vec2 remaining_tile_size = u32vec2(1,1) * (TILE_THREAD_WIDTH >> (mip_index + 1));
    const i32vec2 dst_index = tile_thread + tile * remaining_tile_size;
    const i32vec2 in_tile_src_index = tile_thread * 2;
    const u32vec2 dst_mip_size = (push.dst_mip0_size >> dst_mip);
    const bool thread_active = all(lessThan(tile_thread, remaining_tile_size)) && all(lessThan(dst_index, dst_mip_size));

    if(thread_active) {
        const f32vec4 fetch = f32vec4(
            s_mins[ping_pong_src_index * TILE_THREAD_COUNT + (in_tile_src_index.y + 0) * TILE_THREAD_WIDTH + (in_tile_src_index.x + 1)],
            s_mins[ping_pong_src_index * TILE_THREAD_COUNT + (in_tile_src_index.y + 1) * TILE_THREAD_WIDTH + (in_tile_src_index.x + 1)],
            s_mins[ping_pong_src_index * TILE_THREAD_COUNT + (in_tile_src_index.y + 0) * TILE_THREAD_WIDTH + (in_tile_src_index.x + 0)],
            s_mins[ping_pong_src_index * TILE_THREAD_COUNT + (in_tile_src_index.y + 1) * TILE_THREAD_WIDTH + (in_tile_src_index.x + 0)]
        );
        const f32 min_v = min(min(fetch.x,fetch.y), min(fetch.z,fetch.w));
        s_mins[((ping_pong_src_index + 1) & 1) * TILE_THREAD_COUNT + tile_thread.y * TILE_THREAD_WIDTH + tile_thread.x] = min_v;

        if(dst_mip == 5 || dst_mip == 11) {
            push.attach.hiz[dst_mip].get_coherent()[dst_index] = min_v;
        } else {
            push.attach.hiz[dst_mip].get()[dst_index] = min_v;
        }
    }
}

void downsample_tile<bool BASE_CASE>(i32 start_mip, i32vec2 tile, i32vec2 in_tile_thread_index) {
    i32 dst_mip;
    
    dst_mip = start_mip + 1;
    if(dst_mip >= push.mip_count) return;
    f32vec4 mip0_values = 
        downsample_tile_mip_n0<BASE_CASE>(start_mip, tile, in_tile_thread_index);

    dst_mip = start_mip + 2;
    if (dst_mip >= push.mip_count) return;
    downsample_tile_mip_n1(start_mip, tile, in_tile_thread_index, mip0_values);

    [unroll]
    for(i32 mip_index = 0; mip_index < 4; mip_index++) {
        dst_mip = start_mip + 3 + mip_index;
        if(dst_mip >= push.mip_count) return;

        downsample_tile_mip_n2_n5(start_mip, tile, in_tile_thread_index, mip_index);
    }
}

groupshared bool s_is_last_workgroup;

[shader("compute")]
[numthreads(GEN_HIZ_X, GEN_HIZ_Y, 1)]
void entry_gen_hiz(
    u32vec2 dtid : SV_DispatchThreadID,
    u32vec2 gid : SV_GroupID,
    u32vec2 gtid : SV_GroupThreadID
)
{
    downsample_tile<true>(-1, gid, gtid);

    if(all(equal(gtid, u32vec2(0,0)))) {
        u32 previously_finished_workgroups = 0;
        InterlockedAdd(*push.workgroup_finish_counter, 1, previously_finished_workgroups);
        const u32 total_finished_workgroups = previously_finished_workgroups + 1;
        s_is_last_workgroup = total_finished_workgroups == push.total_workgroup_count;
    }

    GroupMemoryBarrierWithGroupSync();

    if(s_is_last_workgroup) {
        u32vec2 mip5_size = push.dst_mip0_size / 64;
        u32vec2 mip5_tile_count = max((mip5_size >> 5), u32vec2(1,1));
        for(u32 tile_y = 0; tile_y < mip5_tile_count.y; tile_y++) {
            for(u32 tile_x = 0; tile_x < mip5_tile_count.x; tile_x++) {
                u32vec2 tile = u32vec2(tile_x, tile_y);
                downsample_tile<false>(5, tile, gtid);
            }
        }
        
        GroupMemoryBarrierWithGroupSync();

        if(any(greaterThan(mip5_tile_count, u32vec2(1,1)))) {
            downsample_tile<false>(11, u32vec2(0,0), gtid);
        }
    }
}