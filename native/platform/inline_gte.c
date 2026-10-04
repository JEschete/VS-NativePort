#include <psx_gte.h>
#include <psx_inline_gte.h>

void psx_inline_rotation(const void* matrix)
{
    const uint32_t* words = (const uint32_t*)matrix;
    int index;
    for (index = 0; index < 5; index++) gte_set_ctrl(index, words[index]);
}

void psx_inline_translation(const void* vector)
{
    const uint32_t* words = (const uint32_t*)vector;
    gte_set_ctrl(GTE_C_TRX, words[0]);
    gte_set_ctrl(GTE_C_TRY, words[1]);
    gte_set_ctrl(GTE_C_TRZ, words[2]);
}

void psx_inline_load_vector(const void* vector)
{
    const uint32_t* words = (const uint32_t*)vector;
    gte_set_data(GTE_D_VXY0, words[0]);
    gte_set_data(GTE_D_VZ0, words[1]);
}

void psx_inline_transform(void) { gte_op_mvmva(1, 0, 0, 0, 0); }
void psx_inline_project(void) { gte_op_rtps(); }

void psx_inline_store_vector(void* vector)
{
    uint32_t* words = (uint32_t*)vector;
    words[0] = gte_get_data(GTE_D_MAC1);
    words[1] = gte_get_data(GTE_D_MAC2);
    words[2] = gte_get_data(GTE_D_MAC3);
}

void psx_inline_store_screen(void* screen)
{
    *(uint32_t*)screen = gte_get_data(GTE_D_SXY2);
}

void psx_inline_store_depth(void* depth)
{
    *(int32_t*)depth = (int32_t)gte_get_data(GTE_D_SZ3) >> 2;
}

void psx_inline_clear_translation(void)
{
    gte_set_ctrl(GTE_C_TRX, 0);
    gte_set_ctrl(GTE_C_TRY, 0);
    gte_set_ctrl(GTE_C_TRZ, 0);
}
