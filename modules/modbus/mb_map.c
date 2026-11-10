#include "mb_map.h"

#include <string.h>

void mb_map_init(mb_map_t *map, uint8_t unit_id, bool accept_any_unit)
{
    if (map == NULL) {
        return;
    }

    memset(map, 0, sizeof(*map));
    map->unit_id = unit_id;
    map->accept_any_unit = accept_any_unit;
}

bool mb_map_unit_matches(const mb_map_t *map, uint8_t unit_id)
{
    if (map == NULL) {
        return false;
    }

    if (map->accept_any_unit) {
        return true;
    }

    return map->unit_id == unit_id;
}

bool mb_map_read_holding(const mb_map_t *map, uint16_t address, uint16_t *value)
{
    if ((map == NULL) || (value == NULL) || (address >= MB_HOLDING_COUNT)) {
        return false;
    }

    *value = map->holding[address];
    return true;
}

bool mb_map_read_input(const mb_map_t *map, uint16_t address, uint16_t *value)
{
    if ((map == NULL) || (value == NULL) || (address >= MB_INPUT_COUNT)) {
        return false;
    }

    *value = map->input[address];
    return true;
}

bool mb_map_write_holding(mb_map_t *map, uint16_t address, uint16_t value)
{
    if ((map == NULL) || (address >= MB_HOLDING_COUNT)) {
        return false;
    }

    map->holding[address] = value;
    return true;
}
