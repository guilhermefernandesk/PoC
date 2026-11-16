/*
 * mbtcp_map.c - mapa de pontos sobre o cliente Modbus TCP.
 *
 * Assume uso por UMA task (a task de processo). O transporte tem mutex,
 * mas os buffers de scratch abaixo sao unicos; se duas tasks chamarem o
 * mapa ao mesmo tempo, proteja com um mutex da aplicacao.
 */
#include "mbtcp_map.h"
#include "tcpprobe.h"

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#define MBMAP_MAX_REGS 125u
#define MBMAP_MAX_BITS 2000u

static uint16_t s_regs[MBMAP_MAX_REGS];
static uint8_t s_bits[(MBMAP_MAX_BITS + 7u) / 8u];

static float scale_or_one(float scale)
{
    return ((scale > 0.0f) || (scale < 0.0f)) ? scale : 1.0f;
}

/* IEEE-754 float32: exp=0xFF cobre NaN e infinito. */
static int is_nonfinite_f32(float f)
{
    uint32_t u;

    memcpy(&u, &f, sizeof(u));
    return (u & 0x7F800000u) == 0x7F800000u;
}

static int is_nan_f32(float f)
{
    uint32_t u;

    memcpy(&u, &f, sizeof(u));
    return ((u & 0x7F800000u) == 0x7F800000u) &&
           ((u & 0x007FFFFFu) != 0u);
}

static uint16_t point_regs(const mbpoint_t *p)
{
    switch (p->type) {
    case MBV_U32:
    case MBV_S32:
    case MBV_F32:
        return 2u;
    default:
        return 1u;
    }
}

static void decode_point(const mbpoint_t *p, const uint16_t *regs,
                         const uint8_t *bits, mbval_t *v)
{
    v->valid = 1u;
    v->type = p->type;
    v->timestamp = (uint32_t)xTaskGetTickCount();

    switch (p->type) {
    case MBV_BIT:
        if ((p->table == MBT_COIL) || (p->table == MBT_DISCRETE)) {
            v->raw_u = (bits != NULL) ? (uint32_t)(bits[0] & 1u) : 0u;
        } else {
            v->raw_u = (regs != NULL) ? (uint32_t)mbcodec_get_bit(regs[0], p->bit)
                                      : 0u;
        }
        v->raw_i = (int32_t)v->raw_u;
        v->eng = (float)v->raw_u;
        break;

    case MBV_U16:
        v->raw_u = (uint32_t)mbcodec_get_u16(regs);
        v->raw_i = (int32_t)(int16_t)v->raw_u;
        v->eng = mbcodec_scale((int32_t)v->raw_u, p->scale, p->offset);
        break;

    case MBV_S16:
        v->raw_i = (int32_t)mbcodec_get_s16(regs);
        v->raw_u = (uint32_t)v->raw_i;
        v->eng = mbcodec_scale(v->raw_i, p->scale, p->offset);
        break;

    case MBV_U32:
        v->raw_u = mbcodec_get_u32(regs, p->word_order);
        v->raw_i = (int32_t)v->raw_u;
        v->eng = ((float)v->raw_u * scale_or_one(p->scale)) + p->offset;
        break;

    case MBV_S32:
        v->raw_i = mbcodec_get_s32(regs, p->word_order);
        v->raw_u = (uint32_t)v->raw_i;
        v->eng = mbcodec_scale(v->raw_i, p->scale, p->offset);
        break;

    case MBV_F32:
        v->raw_u = mbcodec_get_u32(regs, p->word_order);
        v->raw_i = (int32_t)v->raw_u;
        v->eng = (mbcodec_get_f32(regs, p->word_order) *
                  scale_or_one(p->scale)) + p->offset;
        break;

    default:
        v->valid = 0u;
        v->raw_i = 0;
        v->raw_u = 0u;
        v->eng = 0.0f;
        break;
    }
}

static void encode_point(const mbpoint_t *p, float eng,
                         uint16_t *regs, uint8_t *bits)
{
    float sc = scale_or_one(p->scale);

    switch (p->type) {
    case MBV_BIT:
        if ((p->table == MBT_COIL) || (p->table == MBT_DISCRETE)) {
            bits[0] = ((eng > 0.0f) || (eng < 0.0f)) ? 1u : 0u;
        } else {
            regs[0] = mbcodec_set_bit(regs[0], p->bit,
                                      ((eng > 0.0f) || (eng < 0.0f)) ? 1u : 0u);
        }
        break;

    case MBV_U16: {
        int32_t raw = mbcodec_unscale(eng, p->scale, p->offset);
        if (raw < 0) {
            raw = 0;
        } else if (raw > 65535) {
            raw = 65535;
        }
        mbcodec_put_u16(regs, (uint16_t)raw);
        break;
    }

    case MBV_S16: {
        int32_t raw = mbcodec_unscale(eng, p->scale, p->offset);
        if (raw < -32768) {
            raw = -32768;
        } else if (raw > 32767) {
            raw = 32767;
        }
        mbcodec_put_s16(regs, (int16_t)raw);
        break;
    }

    case MBV_U32: {
        float f = (eng - p->offset) / sc;
        uint32_t raw;

        if (is_nan_f32(f)) {
            raw = 0u;
        } else if (f <= 0.0f) {
            raw = 0u;
        } else if (f >= 4294967295.0f) {
            raw = 0xFFFFFFFFu;
        } else {
            raw = (uint32_t)(f + 0.5f);
        }
        mbcodec_put_u32(regs, raw, p->word_order);
        break;
    }

    case MBV_S32:
        mbcodec_put_s32(regs, mbcodec_unscale(eng, p->scale, p->offset),
                        p->word_order);
        break;

    case MBV_F32:
        mbcodec_put_f32(regs, (eng - p->offset) / sc, p->word_order);
        break;

    default:
        break;
    }
}

static void invalidate_group(mbmap_t *m, uint8_t group_id)
{
    uint16_t i;

    for (i = 0u; i < m->point_count; i++) {
        if ((m->points[i].group == group_id) && (m->cache != NULL)) {
            m->cache[i].valid = 0u;
        }
    }
}

int mbmap_init(mbmap_t *m, const mbpoint_t *points, uint16_t point_count,
               mbgroup_t *groups, uint16_t group_count, mbval_t *cache)
{
    uint16_t i;
    uint16_t j;

    if ((m == NULL) || (points == NULL) || (point_count == 0u) ||
        (cache == NULL)) {
        return MBTCP_EINVAL;
    }
    if ((group_count != 0u) && (groups == NULL)) {
        return MBTCP_EINVAL;
    }

    for (i = 0u; i < point_count; i++) {
        if ((points[i].table > MBT_DISCRETE) ||
            (points[i].type > MBV_BIT) ||
            (points[i].access > MBA_WO) ||
            (points[i].word_order > MBW_LO_HI)) {
            return MBTCP_EINVAL;
        }
        if (is_nonfinite_f32(points[i].scale) ||
            is_nonfinite_f32(points[i].offset) ||
            is_nonfinite_f32(points[i].min) ||
            is_nonfinite_f32(points[i].max)) {
            return MBTCP_EINVAL;
        }
        if ((points[i].type == MBV_BIT) && (points[i].bit > 15u)) {
            return MBTCP_EINVAL;
        }
        if (((points[i].table == MBT_COIL) ||
             (points[i].table == MBT_DISCRETE)) &&
            (points[i].type != MBV_BIT)) {
            return MBTCP_EINVAL;
        }
        if (((points[i].table == MBT_INPUT) ||
             (points[i].table == MBT_DISCRETE)) &&
            (points[i].access != MBA_RO)) {
            return MBTCP_EINVAL;
        }
        if (points[i].group != 0u) {
            const mbgroup_t *g = NULL;
            uint16_t qty = point_regs(&points[i]);

            for (j = 0u; j < group_count; j++) {
                if (groups[j].id == points[i].group) {
                    g = &groups[j];
                    break;
                }
            }
            if ((g == NULL) || (g->table != points[i].table) ||
                (points[i].addr < g->rd_addr) ||
                (((uint32_t)points[i].addr + (uint32_t)qty) >
                 ((uint32_t)g->rd_addr + (uint32_t)g->rd_qty))) {
                return MBTCP_EINVAL;
            }
        }
    }
    for (i = 0u; i < group_count; i++) {
        uint16_t max_qty;

        if ((groups[i].id == 0u) ||
            (groups[i].table > MBT_DISCRETE) ||
            (groups[i].rd_qty == 0u)) {
            return MBTCP_EINVAL;
        }
        max_qty = ((groups[i].table == MBT_HOLDING) ||
                   (groups[i].table == MBT_INPUT))
                      ? MBMAP_MAX_REGS
                      : MBMAP_MAX_BITS;
        if (groups[i].rd_qty > max_qty) {
            return MBTCP_EINVAL;
        }
        for (j = (uint16_t)(i + 1u); j < group_count; j++) {
            if (groups[i].id == groups[j].id) {
                return MBTCP_EINVAL;
            }
        }
    }

    m->points = points;
    m->point_count = point_count;
    m->groups = groups;
    m->group_count = group_count;
    m->cache = cache;

    memset(cache, 0, (size_t)point_count * sizeof(mbval_t));

    for (i = 0u; i < group_count; i++) {
        groups[i].next_tick = 0u;       /* primeiro tick le todos */
    }

    return MBTCP_OK;
}

int mbmap_id(const mbmap_t *m, const char *name)
{
    uint16_t i;

    if ((m == NULL) || (name == NULL)) {
        return -1;
    }

    for (i = 0u; i < m->point_count; i++) {
        if ((m->points[i].name != NULL) &&
            (strcmp(m->points[i].name, name) == 0)) {
            return (int)i;
        }
    }
    return -1;
}

int mbmap_read(mbmap_t *m, uint16_t id, mbval_t *out)
{
    const mbpoint_t *p;
    mbval_t v;
    int rc;

    if ((m == NULL) || (m->points == NULL) || (m->cache == NULL) ||
        (id >= m->point_count)) {
        return MBTCP_EINVAL;
    }

    p = &m->points[id];
    if (p->access == MBA_WO) {
        return MBTCP_EINVAL;
    }

    if ((p->table == MBT_HOLDING) || (p->table == MBT_INPUT)) {
        uint16_t regs[2] = { 0u, 0u };
        uint16_t qty = point_regs(p);

        if (p->table == MBT_HOLDING) {
            rc = mbtcp_read_holding(p->addr, qty, regs);
        } else {
            rc = mbtcp_read_input(p->addr, qty, regs);
        }
        if (rc != MBTCP_OK) {
            return rc;
        }
        decode_point(p, regs, NULL, &v);
    } else if ((p->table == MBT_COIL) || (p->table == MBT_DISCRETE)) {
        uint8_t bits[1] = { 0u };

        if (p->table == MBT_COIL) {
            rc = mbtcp_read_coils(p->addr, 1u, bits);
        } else {
            rc = mbtcp_read_discrete_inputs(p->addr, 1u, bits);
        }
        if (rc != MBTCP_OK) {
            return rc;
        }
        decode_point(p, NULL, bits, &v);
    } else {
        return MBTCP_EINVAL;
    }

    m->cache[id] = v;
    if (out != NULL) {
        *out = v;
    }
    return MBTCP_OK;
}

int mbmap_write(mbmap_t *m, uint16_t id, float eng)
{
    const mbpoint_t *p;
    mbval_t v;
    int rc;

    if ((m == NULL) || (m->points == NULL) || (m->cache == NULL) ||
        (id >= m->point_count)) {
        return MBTCP_EINVAL;
    }

    p = &m->points[id];
    if (p->access == MBA_RO) {
        return MBTCP_EINVAL;
    }
    if (is_nonfinite_f32(eng)) {
        return MBTCP_EINVAL;
    }
    if ((p->min < p->max) && ((eng < p->min) || (eng > p->max))) {
        return MBTCP_EINVAL;
    }

    if (p->table == MBT_HOLDING) {
        uint16_t regs[2] = { 0u, 0u };
        uint8_t bits[1] = { 0u };

        if (p->type == MBV_BIT) {
            rc = mbtcp_read_holding(p->addr, 1u, regs);
            if (rc != MBTCP_OK) {
                return rc;
            }
            encode_point(p, eng, regs, bits);
            rc = mbtcp_write_register(p->addr, regs[0]);
        } else {
            uint16_t qty = point_regs(p);
            encode_point(p, eng, regs, bits);

            if (qty == 1u) {
                rc = mbtcp_write_register(p->addr, regs[0]);
            } else {
#if MBMAP_USE_FC16
                rc = mbtcp_write_registers(p->addr, qty, regs);
#else
                uint16_t dummy = 0u;
                rc = mbtcp_read_write_registers(p->addr, 1u, &dummy,
                                                p->addr, qty, regs);
#endif
            }
        }
        if (rc != MBTCP_OK) {
            return rc;
        }
        decode_point(p, regs, NULL, &v);
    } else if (p->table == MBT_COIL) {
        uint8_t bits[1] = { 0u };

        encode_point(p, eng, NULL, bits);
        rc = mbtcp_write_coil(p->addr, (int)bits[0]);
        if (rc != MBTCP_OK) {
            return rc;
        }
        decode_point(p, NULL, bits, &v);
    } else {
        /* input/discrete sao somente leitura */
        return MBTCP_EINVAL;
    }

    m->cache[id] = v;
    return MBTCP_OK;
}

int mbmap_read_cached(const mbmap_t *m, uint16_t id, mbval_t *out)
{
    if ((m == NULL) || (m->cache == NULL) || (out == NULL) ||
        (id >= m->point_count)) {
        return MBTCP_EINVAL;
    }
    *out = m->cache[id];
    return MBTCP_OK;
}

int mbmap_poll_group(mbmap_t *m, uint8_t group_id)
{
    mbgroup_t *g = NULL;
    uint16_t i;
    uint16_t gi;
    int rc;

    if ((m == NULL) || (m->groups == NULL) || (m->cache == NULL)) {
        return MBTCP_EINVAL;
    }

    for (gi = 0u; gi < m->group_count; gi++) {
        if (m->groups[gi].id == group_id) {
            g = &m->groups[gi];
            break;
        }
    }
    if (g == NULL) {
        return MBTCP_EINVAL;
    }

    if ((g->table == MBT_HOLDING) || (g->table == MBT_INPUT)) {
        if ((g->rd_qty == 0u) || (g->rd_qty > MBMAP_MAX_REGS)) {
            return MBTCP_EINVAL;
        }

        if (g->table == MBT_HOLDING) {
            rc = mbtcp_read_holding(g->rd_addr, g->rd_qty, s_regs);
        } else {
            rc = mbtcp_read_input(g->rd_addr, g->rd_qty, s_regs);
        }
        if (rc != MBTCP_OK) {
            invalidate_group(m, group_id);
            return rc;
        }

        for (i = 0u; i < m->point_count; i++) {
            const mbpoint_t *p = &m->points[i];
            uint16_t qty;
            uint16_t off;

            if ((p->group != group_id) || (p->table != g->table)) {
                continue;
            }
            qty = point_regs(p);
            if (p->addr < g->rd_addr) {
                continue;
            }
            off = (uint16_t)(p->addr - g->rd_addr);
            if (((uint32_t)off + (uint32_t)qty) > (uint32_t)g->rd_qty) {
                continue;
            }
            decode_point(p, &s_regs[off], NULL, &m->cache[i]);
        }
    } else if ((g->table == MBT_COIL) || (g->table == MBT_DISCRETE)) {
        if ((g->rd_qty == 0u) || (g->rd_qty > MBMAP_MAX_BITS)) {
            return MBTCP_EINVAL;
        }

        if (g->table == MBT_COIL) {
            rc = mbtcp_read_coils(g->rd_addr, g->rd_qty, s_bits);
        } else {
            rc = mbtcp_read_discrete_inputs(g->rd_addr, g->rd_qty, s_bits);
        }
        if (rc != MBTCP_OK) {
            invalidate_group(m, group_id);
            return rc;
        }

        for (i = 0u; i < m->point_count; i++) {
            const mbpoint_t *p = &m->points[i];
            uint16_t off;

            if ((p->group != group_id) || (p->table != g->table)) {
                continue;
            }
            if (p->addr < g->rd_addr) {
                continue;
            }
            off = (uint16_t)(p->addr - g->rd_addr);
            if (off >= g->rd_qty) {
                continue;
            }
            decode_point(p, NULL, &s_bits[off], &m->cache[i]);
        }
    } else {
        return MBTCP_EINVAL;
    }

    return MBTCP_OK;
}

int mbmap_tick(mbmap_t *m)
{
    TickType_t now;
    int first = MBTCP_OK;
    uint16_t i;

    if ((m == NULL) || (m->groups == NULL) || (m->cache == NULL)) {
        return MBTCP_EINVAL;
    }

    now = xTaskGetTickCount();

    for (i = 0u; i < m->group_count; i++) {
        mbgroup_t *g = &m->groups[i];

        if ((int32_t)((uint32_t)now - g->next_tick) >= 0) {
            int rc = mbmap_poll_group(m, g->id);
            uint32_t period = (g->period_ms != 0u) ? g->period_ms : 1u;

            if ((rc != MBTCP_OK) && (first == MBTCP_OK)) {
                first = rc;
            }

            g->next_tick = (uint32_t)now + (uint32_t)pdMS_TO_TICKS(period);
            if (g->next_tick == 0u) {
                g->next_tick = 1u;
            }
        }
    }

    return first;
}

int mbmap_wait(mbmap_t *m, uint16_t id, uint32_t mask,
               uint32_t value, uint32_t timeout_ms)
{
    TickType_t start;
    TickType_t limit;

    if ((m == NULL) || (id >= m->point_count)) {
        return MBTCP_EINVAL;
    }

    start = xTaskGetTickCount();
    limit = pdMS_TO_TICKS(timeout_ms);
    if ((timeout_ms != 0u) && (limit == 0u)) {
        limit = 1u;                 /* timeout menor que 1 tick */
    }

    for (;;) {
        mbval_t v;
        int rc = mbmap_read(m, id, &v);

        if ((rc == MBTCP_OK) && ((v.raw_u & mask) == value)) {
            return MBTCP_OK;
        }

        if ((uint32_t)(xTaskGetTickCount() - start) >= (uint32_t)limit) {
            return MBTCP_ETIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(10u));
    }
}

void mbmap_invalidate(mbmap_t *m, uint16_t id)
{
    if ((m != NULL) && (m->cache != NULL) && (id < m->point_count)) {
        m->cache[id].valid = 0u;
    }
}

void mbmap_dump(const mbmap_t *m)
{
    uint16_t i;

    if ((m == NULL) || (m->points == NULL) || (m->cache == NULL)) {
        return;
    }

    for (i = 0u; i < m->point_count; i++) {
        const mbval_t *v = &m->cache[i];

        if (v->valid == 0u) {
            continue;
        }
        tcpprobe_log("%-24s raw=%ld/%lu eng=%.3f",
                     (m->points[i].name != NULL) ? m->points[i].name : "?",
                     (long)v->raw_i, (unsigned long)v->raw_u,
                     (double)v->eng);
    }
}
