#ifndef MB_MAP_H
#define MB_MAP_H

#include "mb_types.h"

void mb_map_init(mb_map_t *map, uint8_t unit_id, bool accept_any_unit);
bool mb_map_unit_matches(const mb_map_t *map, uint8_t unit_id);

bool mb_map_read_holding(const mb_map_t *map, uint16_t address, uint16_t *value);
bool mb_map_read_input(const mb_map_t *map, uint16_t address, uint16_t *value);
bool mb_map_write_holding(mb_map_t *map, uint16_t address, uint16_t value);

#endif
