#pragma once
#include "glue.inl"
#include "shared.inl"

extension ShaderGlobals {
    void debug_draw(ShaderDebugEntityOOBDraw draw) {
        u32 index;
        InterlockedAdd(debug->entity_oob_draw_indirect_info.instance_count, 1, index);
        debug->entity_oob_draws[index] = draw;
    }

    void debug_draw(ShaderDebugAABBDraw draw) {
        u32 index;
        InterlockedAdd(debug->aabb_draw_indirect_info.instance_count, 1, index);
        debug->aabb_draws[index] = draw;
    }

    void debug_draw(ShaderDebugCircleDraw draw) {
        u32 index;
        InterlockedAdd(debug->circle_draw_indirect_info.instance_count, 1, index);
        debug->circle_draws[index] = draw;
    }

    void debug_draw(ShaderDebugLineDraw draw) {
        u32 index;
        InterlockedAdd(debug->line_draw_indirect_info.instance_count, 1, index);
        debug->line_draws[index] = draw;
    }
};