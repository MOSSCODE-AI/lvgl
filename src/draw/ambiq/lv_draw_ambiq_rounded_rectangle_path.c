// Copyright wuyin 2026. All rights reserved.

/**
 * @file lv_draw_ambiq_rounded_rectangle_path.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_draw_ambiq.h"
#if LV_USE_DRAW_AMBIQ
#include "lv_draw_ambiq_private.h"

#if LV_USE_AMBIQ_VG

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void lv_draw_ambiq_rounded_rectangle_path(lv_draw_task_t * t, const lv_draw_rounded_rectangle_path_dsc_t * dsc)
{
    if(dsc == NULL) return;
    if(dsc->path == NULL) return;
    if(dsc->path->points == NULL || dsc->path->point_count < 2U) return;
    if(dsc->width <= 0) return;
    if(dsc->color.alpha <= LV_OPA_MIN) return;

    lv_draw_ambiq_unit_t * unit = (lv_draw_ambiq_unit_t *)t->draw_unit;
    lv_layer_t * layer = t->target_layer;
    if(unit == NULL || unit->vg_path == NULL || unit->vg_paint == NULL || layer == NULL) return;

    const uint16_t point_count = dsc->path->point_count;
    uint8_t * segments = dsc->path->render_segments;
    nema_vg_float_t * data = (nema_vg_float_t *)dsc->path->render_data;
    if(segments == NULL || data == NULL) return;

    for(uint16_t i = 0U; i < point_count; ++i) {
        segments[i] = (i == 0U) ? NEMA_VG_PRIM_MOVE : NEMA_VG_PRIM_LINE;
        data[i * 2U] = (nema_vg_float_t)(dsc->path->points[i].x - layer->buf_area.x1);
        data[i * 2U + 1U] = (nema_vg_float_t)(dsc->path->points[i].y - layer->buf_area.y1);
    }

    uint32_t blending_mode;
    if(layer->color_format == LV_COLOR_FORMAT_ARGB8888) {
        blending_mode = NEMA_BL_SRC_OVER | NEMA_BLOP_SRC_PREMULT;
    }
    else {
        blending_mode = NEMA_BL_SRC_OVER;
    }

    nema_vg_path_clear(unit->vg_path);
    nema_vg_paint_clear(unit->vg_paint);
    nema_vg_paint_set_opacity(unit->vg_paint, 1.0f);
    nema_vg_paint_set_type(unit->vg_paint, NEMA_VG_PAINT_COLOR);
    nema_vg_paint_set_paint_color(unit->vg_paint, nema_rgba(dsc->color.red,
                                                            dsc->color.green,
                                                            dsc->color.blue,
                                                            dsc->color.alpha));
    nema_vg_set_quality(NEMA_VG_QUALITY_BETTER);
    nema_vg_set_blend(blending_mode);
    nema_vg_set_fill_rule(NEMA_VG_STROKE);
    nema_vg_stroke_set_width((float)dsc->width);
    nema_vg_stroke_set_cap_style(dsc->rounded ? NEMA_VG_CAP_ROUND : NEMA_VG_CAP_BUTT,
                                 dsc->rounded ? NEMA_VG_CAP_ROUND : NEMA_VG_CAP_BUTT);
    nema_vg_path_set_shape(unit->vg_path, point_count, segments, point_count * 2U, data);
    nema_vg_draw_path(unit->vg_path, unit->vg_paint);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

#endif /*LV_USE_AMBIQ_VG*/
#endif /*LV_USE_DRAW_AMBIQ*/
