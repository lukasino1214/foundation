#pragma once
#include <daxa/daxa.inl>
#include "glue.inl"

void encode_visibility_index(inout u32 id, in u32 meshlet_index, in u32 triangle_index) {
    id = (meshlet_index << 6) | (triangle_index);
}

void decode_visibility_index(in u32 id, out u32 meshlet_index, out u32 triangle_index) {
    meshlet_index = id >> 6;
    triangle_index = id & 0x3F;
}

f32 interpolate_float(f32vec3 derivator, f32 v0, f32 v1, f32 v2) {
    f32vec3 mergedV = f32vec3(v0, v1, v2);
    f32 ret;
    ret = dot(mergedV, derivator);
    return ret;
}

f32vec2 interpolate_vec2(f32vec3 derivator, f32vec2 v0, f32vec2 v1, f32vec2 v2) {
    f32vec3 merged_x = f32vec3(v0.x, v1.x, v2.x);
    f32vec3 merged_y = f32vec3(v0.y, v1.y, v2.y);
    f32vec2 ret;
    ret.x = dot(merged_x, derivator);
    ret.y = dot(merged_y, derivator);
    return ret;
}

f32vec3 interpolate_vec3(f32vec3 derivator, f32vec3 v0, f32vec3 v1, f32vec3 v2) {
    f32vec3 merged_x = f32vec3(v0.x, v1.x, v2.x);
    f32vec3 merged_y = f32vec3(v0.y, v1.y, v2.y);
    f32vec3 merged_z = f32vec3(v0.z, v1.z, v2.z);
    f32vec3 ret;
    ret.x = dot(merged_x, derivator);
    ret.y = dot(merged_y, derivator);
    ret.z = dot(merged_z, derivator);
    return ret;
}

f32vec4 interpolate_vec4(f32vec3 derivator, f32vec4 v0, f32vec4 v1, f32vec4 v2) {
    f32vec3 merged_x = f32vec3(v0.x, v1.x, v2.x);
    f32vec3 merged_y = f32vec3(v0.y, v1.y, v2.y);
    f32vec3 merged_z = f32vec3(v0.z, v1.z, v2.z);
    f32vec3 merged_w = f32vec3(v0.w, v1.w, v2.w);
    f32vec4 ret;
    ret.x = dot(merged_x, derivator);
    ret.y = dot(merged_y, derivator);
    ret.z = dot(merged_z, derivator);
    ret.w = dot(merged_w, derivator);
    return ret;
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

UvGrad get_uv_grad(BarycentricDeriv barycentrics, f32vec2 v0, f32vec2 v1, f32vec2 v2) {
    UvGrad uv_grad;
    uv_grad.uv = interpolate_vec2(barycentrics.m_lambda, v0, v1, v2);
    uv_grad.ddx = interpolate_vec2(barycentrics.m_ddx, v0, v1, v2);
    uv_grad.ddy = interpolate_vec2(barycentrics.m_ddy, v0, v1, v2);
    return uv_grad;
}