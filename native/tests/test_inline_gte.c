#include <psx_gte.h>
#include <psx_inline_gte.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    short matrix[12] = {4096, 0, 0, 0, 4096, 0, 0, 0, 4096, 0, 0, 0};
    short vector[4] = {4, 5, 6, 0};
    int32_t translation[3] = {10, 20, 30};
    int32_t result[3];

    gte_reset();
    psx_inline_rotation(matrix);
    psx_inline_translation(translation);
    psx_inline_load_vector(vector);
    psx_inline_transform();
    psx_inline_store_vector(result);
    if (result[0] != 14 || result[1] != 25 || result[2] != 36) return 1;
    psx_inline_clear_translation();
    if (gte_get_ctrl(GTE_C_TRX) || gte_get_ctrl(GTE_C_TRY) || gte_get_ctrl(GTE_C_TRZ)) return 2;
    printf("Inline GTE adapter: rotation, translation, MAC storage and clearing passed\n");
    return 0;
}
