#pragma once
#include "glue.inl"
#include <daxa/daxa.inl>

[[vk::binding(DAXA_STORAGE_IMAGE_BINDING, 0)]] RWTexture2D<daxa::u64> tex_rw_u64_table[];

func encode_visibility_buffer_payload(u32 meshlet_data_index, f32 depth) -> u64 {
    const u64 u64_depth = u64(asuint(depth));
    return (u64_depth << 32) | meshlet_data_index;
}

func decode_visibility_buffer_payload(u64 payload, __ref u32 meshlet_data_index, __ref u32 depth) {
    meshlet_data_index = u32(payload & (~(u32(0))));
    depth = u32(payload >> 32);
}

func decode_visibility_buffer_payload(u64 payload, __ref u32 meshlet_data_index, __ref f32 depth) {
    meshlet_data_index = u32(payload & (~(u32(0))));
    depth = asfloat(u32(payload >> 32));
}

struct BarycentricDeriv {
    f32vec3 m_lambda;
    f32vec3 m_ddx;
    f32vec3 m_ddy;
};

BarycentricDeriv calc_bary_and_deriv(f32vec4 pt0, f32vec4 pt1, f32vec4 pt2, f32vec2 pixelNdc, f32vec2 winSize) {
    BarycentricDeriv ret;   
    f32vec3 invW = 1.0f / (f32vec3(pt0.w, pt1.w, pt2.w)); 
    f32vec2 ndc0 = pt0.xy * invW.x;
    f32vec2 ndc1 = pt1.xy * invW.y;
    f32vec2 ndc2 = pt2.xy * invW.z;  
    f32 invDet = 1.0f / (determinant(daxa_f32mat2x2(ndc2 - ndc1, ndc0 - ndc1)));
    ret.m_ddx = f32vec3(ndc1.y - ndc2.y, ndc2.y - ndc0.y, ndc0.y - ndc1.y) * invDet * invW;
    ret.m_ddy = f32vec3(ndc2.x - ndc1.x, ndc0.x - ndc2.x, ndc1.x - ndc0.x) * invDet * invW;
    f32 ddxSum = dot(ret.m_ddx, f32vec3(1,1,1));
    f32 ddySum = dot(ret.m_ddy, f32vec3(1,1,1));   
    f32vec2 deltaVec = pixelNdc - ndc0;
    f32 interpInvW = invW.x + deltaVec.x*ddxSum + deltaVec.y*ddySum;
    f32 interpW = 1.0f / (interpInvW);    
    ret.m_lambda.x = interpW * (invW[0] + deltaVec.x*ret.m_ddx.x + deltaVec.y*ret.m_ddy.x);
    ret.m_lambda.y = interpW * (0.0f    + deltaVec.x*ret.m_ddx.y + deltaVec.y*ret.m_ddy.y);
    ret.m_lambda.z = interpW * (0.0f    + deltaVec.x*ret.m_ddx.z + deltaVec.y*ret.m_ddy.z); 
    ret.m_ddx *= (2.0f / winSize.x);
    ret.m_ddy *= (2.0f / winSize.y);
    ddxSum    *= (2.0f / winSize.x);
    ddySum    *= (2.0f / winSize.y);  
    ret.m_ddy *= -1.0f;
    ddySum    *= -1.0f; 
    f32 interpW_ddx = 1.0f / (interpInvW + ddxSum);
    f32 interpW_ddy = 1.0f / (interpInvW + ddySum);   
    ret.m_ddx = interpW_ddx * (ret.m_lambda*interpInvW + ret.m_ddx) - ret.m_lambda;
    ret.m_ddy = interpW_ddy * (ret.m_lambda*interpInvW + ret.m_ddy) - ret.m_lambda;   
    return ret;
}

struct UvGrad {
    f32vec2 uv;
    f32vec2 ddx;
    f32vec2 ddy;
};

f32vec3 interpolate_attributes(in BarycentricDeriv derivatives, in f32[3] attributes) {
    const f32vec3 v = f32vec3(attributes[0], attributes[1], attributes[2]);
    return f32vec3(
        dot(v, derivatives.m_lambda),
        dot(v, derivatives.m_ddx),
        dot(v, derivatives.m_ddy));
}

f32vec3 visgeo_interpolate_vec3(f32vec3 derivator, f32vec3 v0, f32vec3 v1, f32vec3 v2)
{
    f32vec3 merged_x = f32vec3(v0.x, v1.x, v2.x);
    f32vec3 merged_y = f32vec3(v0.y, v1.y, v2.y);
    f32vec3 merged_z = f32vec3(v0.z, v1.z, v2.z);
    f32vec3 ret;
    ret.x = dot(merged_x, derivator);
    ret.y = dot(merged_y, derivator);
    ret.z = dot(merged_z, derivator);
    return ret;
}

f32vec2 visgeo_interpolate_vec2(f32vec3 derivator, f32vec2 v0, f32vec2 v1, f32vec2 v2)
{
    f32vec3 merged_x = f32vec3(v0.x, v1.x, v2.x);
    f32vec3 merged_y = f32vec3(v0.y, v1.y, v2.y);
    f32vec2 ret;
    ret.x = dot(merged_x, derivator);
    ret.y = dot(merged_y, derivator);
    return ret;
}

f32vec3 interpolate_attributes(in BarycentricDeriv derivatives, in f32vec3[3] attributes) {
    return
        attributes[0] * derivatives.m_lambda.x +
        attributes[1] * derivatives.m_lambda.y +
        attributes[2] * derivatives.m_lambda.z;
}