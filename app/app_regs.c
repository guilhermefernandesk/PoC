#include "app_regs.h"

#include "mb_map.h"

static mb_map_t g_map;

void app_regs_init(void)
{
    mb_map_init(&g_map, MB_UNIT_ID_DEFAULT, false);

    for (uint16_t i = 0u; i < MB_HOLDING_COUNT; ++i) {
        g_map.holding[i] = i;
    }

    for (uint16_t i = 0u; i < MB_INPUT_COUNT; ++i) {
        g_map.input[i] = (uint16_t)(1000u + i);
    }
}

mb_map_t *app_regs_map(void)
{
    return &g_map;
}
