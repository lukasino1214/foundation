#pragma once
#include "glue.inl"
#include "shared.inl"

struct DstItemInfo {
    uint src_item_index;
    uint in_expansion_index;
};

func prefix_sum_expansion_get_workitem(PrefixSumWorkExpansion* self, u32 thread_index, out DstItemInfo ret) -> bool {
    u64 merged_counters = self->merged_expansion_count_thread_count;
    u32 total_work_item_count = u32(merged_counters);

    u32 expansion_count = min(u32(merged_counters >> 32), self->expansions_max);
    u32 window_end = expansion_count;
    u32 window_begin = 0;

    while((window_end - window_begin) > 1) {
        const u32 window_size = window_end - window_begin;
        const u32 cursor = window_begin + ((window_size - 1) / 2);
        const u32 prefix_sum_val = self->expansions_inclusive_prefix_sum[cursor];

        const bool greater = prefix_sum_val > thread_index;
        window_end = select(greater, cursor + 1, window_end);

        const bool less_equal = !greater;
        window_begin = select(less_equal, cursor + 1, window_begin);
    }

    if(thread_index >= total_work_item_count) {
        ret = (DstItemInfo)0;
        return false;
    }

    if(window_begin >= expansion_count || window_begin >= window_end) {
        ret = (DstItemInfo)0;
        return false;
    }

    const u32 expansion_index = window_begin;
    const u32 expansion_inc_prefix_value = self->expansions_inclusive_prefix_sum[expansion_index];

    const u32 src_item_index = self->expansions_src_work_item[expansion_index];
    const u32 expansion_factor = self->expansions_expansion_factor[expansion_index];
    const u32 in_expansion_index = thread_index - (expansion_inc_prefix_value - expansion_factor);

    ret.src_item_index = src_item_index;
    ret.in_expansion_index = in_expansion_index;
    return true;
}

func prefix_sum_expansion_add_workitems(PrefixSumWorkExpansion* self, u32 src_item_index, u32 expansion_factor, u32 workgroup_size) {
    u64 previous_merged_count = AtomicAddU64(self->merged_expansion_count_thread_count, u64(1) << 32 | u64(expansion_factor));
    u32 expansion_index = u32(previous_merged_count >> 32);
    u32 inclusive_dst_item_count_prefix_sum = u32(previous_merged_count) + expansion_factor;

    if(expansion_index < self->expansions_max) {
        self->expansions_inclusive_prefix_sum[expansion_index] = inclusive_dst_item_count_prefix_sum;
        self->expansions_src_work_item[expansion_index] = src_item_index;
        self->expansions_expansion_factor[expansion_index] = expansion_factor;

        u32 needed_workgroups = round_up_div(inclusive_dst_item_count_prefix_sum, workgroup_size);
        InterlockedMax(self->workgroup_count, needed_workgroups);
    }
}