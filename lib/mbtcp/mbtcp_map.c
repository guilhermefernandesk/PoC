/*
 * mbtcp_map.c - mapa de pontos sobre o cliente Modbus TCP.
 *
 * Assume uso por UMA task (a task de processo). O transporte tem mutex,
 * mas o buffer de leitura abaixo e' unico; se duas tasks chamarem o mapa
 * ao mesmo tempo, proteja com um mutex da aplicacao.
 */
#include "mbtcp_map.h"
#include "tcpprobe.h"

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#define MBMAP_WAIT_INTERVAL_MS 10u
#define MBMAP_MAX_POINT_REGISTERS 2u
#define MBMAP_ADDRESS_SPACE 65536u

static uint16_t s_registers[MBMAP_MAX_REGISTERS];

static uint16_t register_count(uint8_t type)
{
    switch (type) {
    case MBV_U32:
    case MBV_S32:
    case MBV_F32:
        return MBMAP_MAX_POINT_REGISTERS;
    default:
        return 1u;
    }
}

static void decode_point(const mbpoint_t *point, const uint16_t *registers,
                         mbval_t *value)
{
    value->valid = 1u;
    value->type = point->type;
    value->timestamp = (uint32_t)xTaskGetTickCount();

    switch (point->type) {
    case MBV_U16:
        value->raw_unsigned = mbcodec_get_u16(registers);
        value->raw_signed = (int32_t)value->raw_unsigned;
        value->value = mbcodec_scale_unsigned(value->raw_unsigned,
                                              point->scale, point->offset);
        break;

    case MBV_S16:
        value->raw_unsigned = mbcodec_get_u16(registers);
        value->raw_signed = mbcodec_get_s16(registers);
        value->value = mbcodec_scale(value->raw_signed,
                                     point->scale, point->offset);
        break;

    case MBV_U32:
        value->raw_unsigned = mbcodec_get_u32(registers, point->word_order);
        value->raw_signed = (int32_t)value->raw_unsigned;
        value->value = mbcodec_scale_unsigned(value->raw_unsigned,
                                              point->scale, point->offset);
        break;

    case MBV_S32:
        value->raw_unsigned = mbcodec_get_u32(registers, point->word_order);
        value->raw_signed = mbcodec_get_s32(registers, point->word_order);
        value->value = mbcodec_scale(value->raw_signed,
                                     point->scale, point->offset);
        break;

    case MBV_F32:
        value->raw_unsigned = mbcodec_get_u32(registers, point->word_order);
        value->raw_signed = (int32_t)value->raw_unsigned;
        value->value = (mbcodec_get_f32(registers, point->word_order) *
                        point->scale) + point->offset;
        break;

    default:
        value->valid = 0u;
        value->type = point->type;
        value->raw_signed = 0;
        value->raw_unsigned = 0u;
        value->value = 0.0f;
        break;
    }

    if (!mbcodec_is_finite(value->value)) {
        value->valid = 0u;
    }
}

static int encode_point(const mbpoint_t *point, float value,
                        uint16_t *registers)
{
    switch (point->type) {
    case MBV_U16: {
        int32_t raw = mbcodec_unscale(value, point->scale, point->offset);

        if (raw < 0) {
            raw = 0;
        } else if (raw > (int32_t)UINT16_MAX) {
            raw = (int32_t)UINT16_MAX;
        }
        mbcodec_put_u16(registers, (uint16_t)raw);
        return MBTCP_OK;
    }

    case MBV_S16: {
        int32_t raw = mbcodec_unscale(value, point->scale, point->offset);

        if (raw < INT16_MIN) {
            raw = INT16_MIN;
        } else if (raw > INT16_MAX) {
            raw = INT16_MAX;
        }
        mbcodec_put_s16(registers, (int16_t)raw);
        return MBTCP_OK;
    }

    case MBV_U32:
        mbcodec_put_u32(registers,
                        mbcodec_unscale_unsigned(value, point->scale,
                                                 point->offset),
                        point->word_order);
        return MBTCP_OK;

    case MBV_S32:
        mbcodec_put_s32(registers,
                        mbcodec_unscale(value, point->scale, point->offset),
                        point->word_order);
        return MBTCP_OK;

    case MBV_F32: {
        float raw = (value - point->offset) / point->scale;

        if (!mbcodec_is_finite(raw)) {
            return MBTCP_EINVAL;
        }
        mbcodec_put_f32(registers, raw, point->word_order);
        return MBTCP_OK;
    }

    default:
        return MBTCP_EINVAL;
    }
}

static int group_index(const mbmap_t *map, uint8_t group_id)
{
    uint16_t index;

    for (index = 0u; index < map->group_count; index++) {
        if (map->groups[index].id == group_id) {
            return (int)index;
        }
    }
    return -1;
}

static int point_fits_group(const mbpoint_t *point, const mbgroup_t *group)
{
    uint32_t point_end = (uint32_t)point->address +
                         register_count(point->type);
    uint32_t group_end = (uint32_t)group->address + group->quantity;

    return (point->address >= group->address) && (point_end <= group_end);
}

static void invalidate_group(mbmap_t *map, uint8_t group_id)
{
    uint16_t index;

    for (index = 0u; index < map->point_count; index++) {
        if (map->points[index].group == group_id) {
            map->cache[index].valid = 0u;
        }
    }
}

int mbmap_init(mbmap_t *map, const mbpoint_t *points, uint16_t point_count,
               mbgroup_t *groups, uint16_t group_count, mbval_t *cache)
{
    uint16_t index;
    uint16_t other;

    if ((map == NULL) || (points == NULL) || (point_count == 0u) ||
        (cache == NULL)) {
        return MBTCP_EINVAL;
    }
    if ((group_count != 0u) && (groups == NULL)) {
        return MBTCP_EINVAL;
    }

    for (index = 0u; index < group_count; index++) {
        if ((groups[index].id == 0u) ||
            (groups[index].quantity == 0u) ||
            (groups[index].quantity > MBMAP_MAX_REGISTERS) ||
            (groups[index].period_ms == 0u) ||
            (((uint32_t)groups[index].address +
              (uint32_t)groups[index].quantity) > MBMAP_ADDRESS_SPACE)) {
            return MBTCP_EINVAL;
        }

        for (other = (uint16_t)(index + 1u); other < group_count; other++) {
            if (groups[index].id == groups[other].id) {
                return MBTCP_EINVAL;
            }
        }
    }

    for (index = 0u; index < point_count; index++) {
        const mbpoint_t *point = &points[index];
        uint16_t count;

        if ((point->name == NULL) || (point->name[0] == '\0') ||
            (point->type > MBV_F32) ||
            (point->access > MBA_WO) ||
            (point->word_order > MBW_LO_HI) ||
            (point->scale == 0.0f) ||
            !mbcodec_is_finite(point->scale) ||
            !mbcodec_is_finite(point->offset) ||
            !mbcodec_is_finite(point->minimum) ||
            !mbcodec_is_finite(point->maximum)) {
            return MBTCP_EINVAL;
        }

        if ((point->access != MBA_RO) &&
            (point->minimum >= point->maximum)) {
            return MBTCP_EINVAL;
        }

        count = register_count(point->type);
        if (((uint32_t)point->address + (uint32_t)count) > MBMAP_ADDRESS_SPACE) {
            return MBTCP_EINVAL;
        }

        if (point->group != MBMAP_NO_GROUP) {
            int group = -1;

            for (other = 0u; other < group_count; other++) {
                if (groups[other].id == point->group) {
                    group = (int)other;
                    break;
                }
            }
            if ((group < 0) || !point_fits_group(point, &groups[group])) {
                return MBTCP_EINVAL;
            }
        }

        for (other = 0u; other < index; other++) {
            if ((points[other].name != NULL) &&
                (strcmp(points[other].name, point->name) == 0)) {
                return MBTCP_EINVAL;
            }
        }
    }

    map->points = points;
    map->point_count = point_count;
    map->groups = groups;
    map->group_count = group_count;
    map->cache = cache;

    memset(cache, 0, (size_t)point_count * sizeof(mbval_t));

    for (index = 0u; index < group_count; index++) {
        groups[index].next_tick = 0u;   /* o primeiro tick le todos */
    }

    return MBTCP_OK;
}

int mbmap_id(const mbmap_t *map, const char *name)
{
    uint16_t index;

    if ((map == NULL) || (map->points == NULL) || (name == NULL)) {
        return -1;
    }

    for (index = 0u; index < map->point_count; index++) {
        if ((map->points[index].name != NULL) &&
            (strcmp(map->points[index].name, name) == 0)) {
            return (int)index;
        }
    }
    return -1;
}

int mbmap_read(mbmap_t *map, uint16_t point_id, mbval_t *out)
{
    const mbpoint_t *point;
    uint16_t registers[MBMAP_MAX_POINT_REGISTERS] = { 0u, 0u };
    mbval_t value;
    int rc;

    if ((map == NULL) || (map->points == NULL) || (map->cache == NULL) ||
        (point_id >= map->point_count)) {
        return MBTCP_EINVAL;
    }

    point = &map->points[point_id];
    if (point->access == MBA_WO) {
        return MBTCP_EINVAL;
    }

    rc = mbtcp_read_holding(point->address, register_count(point->type),
                            registers);
    if (rc != MBTCP_OK) {
        map->cache[point_id].valid = 0u;
        return rc;
    }

    decode_point(point, registers, &value);
    map->cache[point_id] = value;
    if (out != NULL) {
        *out = value;
    }
    return MBTCP_OK;
}

int mbmap_write(mbmap_t *map, uint16_t point_id, float value)
{
    const mbpoint_t *point;
    uint16_t registers[MBMAP_MAX_POINT_REGISTERS] = { 0u, 0u };
    uint16_t register_total;
    int rc;

    if ((map == NULL) || (map->points == NULL) || (map->cache == NULL) ||
        (point_id >= map->point_count)) {
        return MBTCP_EINVAL;
    }

    point = &map->points[point_id];
    if (point->access == MBA_RO) {
        return MBTCP_EINVAL;
    }
    if (!mbcodec_is_finite(value)) {
        return MBTCP_EINVAL;
    }
    if ((value < point->minimum) || (value > point->maximum)) {
        return MBTCP_EINVAL;
    }

    rc = encode_point(point, value, registers);
    if (rc != MBTCP_OK) {
        return rc;
    }

    register_total = register_count(point->type);
    if (register_total == 1u) {
        rc = mbtcp_write_register(point->address, registers[0]);
    } else {
        uint16_t ignored_read = 0u;

        /*
         * O UHX65A nao garante FC16 para os 32 bits de process data.
         * FC23 le 1 register e escreve os 2 no mesmo endereco.
         */
        rc = mbtcp_read_write_registers(point->address, 1u, &ignored_read,
                                        point->address, register_total,
                                        registers);
    }
    if (rc != MBTCP_OK) {
        return rc;
    }

    decode_point(point, registers, &map->cache[point_id]);
    return MBTCP_OK;
}

int mbmap_read_cached(const mbmap_t *map, uint16_t point_id, mbval_t *out)
{
    if ((map == NULL) || (map->cache == NULL) || (out == NULL) ||
        (point_id >= map->point_count)) {
        return MBTCP_EINVAL;
    }
    *out = map->cache[point_id];
    return MBTCP_OK;
}

int mbmap_poll_group(mbmap_t *map, uint8_t group_id)
{
    mbgroup_t *group;
    uint16_t index;
    int group_position;
    int rc;

    if ((map == NULL) || (map->points == NULL) || (map->groups == NULL) ||
        (map->cache == NULL)) {
        return MBTCP_EINVAL;
    }

    group_position = group_index(map, group_id);
    if (group_position < 0) {
        return MBTCP_EINVAL;
    }
    group = &map->groups[group_position];

    rc = mbtcp_read_holding(group->address, group->quantity, s_registers);
    if (rc != MBTCP_OK) {
        invalidate_group(map, group_id);
        return rc;
    }

    for (index = 0u; index < map->point_count; index++) {
        const mbpoint_t *point = &map->points[index];
        uint16_t offset;

        if (point->group != group_id) {
            continue;
        }
        if (!point_fits_group(point, group)) {
            continue;
        }

        offset = (uint16_t)(point->address - group->address);
        decode_point(point, &s_registers[offset], &map->cache[index]);
    }

    return MBTCP_OK;
}

int mbmap_tick(mbmap_t *map)
{
    TickType_t now;
    int first_error = MBTCP_OK;
    uint16_t index;

    if ((map == NULL) || (map->points == NULL) || (map->cache == NULL)) {
        return MBTCP_EINVAL;
    }
    if (map->group_count == 0u) {
        return MBTCP_OK;
    }
    if (map->groups == NULL) {
        return MBTCP_EINVAL;
    }

    now = xTaskGetTickCount();

    for (index = 0u; index < map->group_count; index++) {
        mbgroup_t *group = &map->groups[index];

        if ((int32_t)((uint32_t)now - group->next_tick) >= 0) {
            int rc = mbmap_poll_group(map, group->id);
            uint32_t period = (uint32_t)pdMS_TO_TICKS(group->period_ms);

            if ((rc != MBTCP_OK) && (first_error == MBTCP_OK)) {
                first_error = rc;
            }

            if (period == 0u) {
                period = 1u;
            }
            group->next_tick = (uint32_t)now + period;
            if (group->next_tick == 0u) {
                group->next_tick = 1u;
            }
        }
    }

    return first_error;
}

int mbmap_wait(mbmap_t *map, uint16_t point_id, uint32_t mask,
               uint32_t expected, uint32_t timeout_ms)
{
    TickType_t started;
    TickType_t limit;

    if ((map == NULL) || (map->points == NULL) || (map->cache == NULL) ||
        (point_id >= map->point_count)) {
        return MBTCP_EINVAL;
    }

    if (timeout_ms > MBTCP_MAX_TIMEOUT_MS) {
        timeout_ms = MBTCP_MAX_TIMEOUT_MS;
    }

    started = xTaskGetTickCount();
    limit = pdMS_TO_TICKS(timeout_ms);
    if ((timeout_ms != 0u) && (limit == 0u)) {
        limit = 1u;
    }

    for (;;) {
        mbval_t value;
        int rc = mbmap_read(map, point_id, &value);

        if ((rc == MBTCP_OK) && (value.valid != 0u) &&
            ((value.raw_unsigned & mask) == expected)) {
            return MBTCP_OK;
        }

        if ((uint32_t)(xTaskGetTickCount() - started) >=
            (uint32_t)limit) {
            return MBTCP_ETIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(MBMAP_WAIT_INTERVAL_MS));
    }
}

void mbmap_invalidate(mbmap_t *map, uint16_t point_id)
{
    if ((map != NULL) && (map->cache != NULL) &&
        (point_id < map->point_count)) {
        map->cache[point_id].valid = 0u;
    }
}

void mbmap_dump(const mbmap_t *map)
{
    uint16_t index;

    if ((map == NULL) || (map->points == NULL) || (map->cache == NULL)) {
        return;
    }

    for (index = 0u; index < map->point_count; index++) {
        const mbval_t *value = &map->cache[index];

        if (value->valid == 0u) {
            continue;
        }
        tcpprobe_log("%-24s raw=%ld/%lu value=%.3f",
                     (map->points[index].name != NULL)
                         ? map->points[index].name
                         : "?",
                     (long)value->raw_signed,
                     (unsigned long)value->raw_unsigned,
                     (double)value->value);
    }
}
