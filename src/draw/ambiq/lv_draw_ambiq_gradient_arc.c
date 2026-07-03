// Copyright wuyin Oy 2026. All rights reserved.

/**
 * @file lv_draw_ambiq_gradient_arc.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_draw_ambiq.h"
#if LV_USE_DRAW_AMBIQ
#include "lv_draw_ambiq_private.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
#if LV_USE_AMBIQ_VG
static color_var_t lv_draw_ambiq_gradient_color(lv_color32_t color);
static color_var_t lv_draw_ambiq_gradient_mix_color(lv_color32_t start_color, lv_color32_t end_color, float ratio);
static float lv_draw_ambiq_normalize_angle(float angle);
static float lv_draw_ambiq_gradient_sweep(float start_angle, float end_angle);
static void lv_draw_ambiq_set_conical_gradient(NEMA_VG_GRAD_HANDLE grad,
                                               NEMA_VG_PAINT_HANDLE paint,
                                               const lv_draw_gradient_arc_dsc_t * dsc,
                                               float cx,
                                               float cy);
#endif

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
#if LV_USE_AMBIQ_VG
void lv_draw_ambiq_gradient_arc(lv_draw_task_t * t, const lv_draw_gradient_arc_dsc_t * dsc)
{
    if(dsc == NULL) return;
    if(dsc->width <= 0.0f) return;
    if(dsc->radius <= 0.0f) return;
    if(dsc->start_angle == dsc->end_angle) return;

    lv_draw_ambiq_unit_t * unit = (lv_draw_ambiq_unit_t *)t->draw_unit;
    lv_layer_t * layer = t->target_layer;

    const float cx = dsc->center_x - (float)layer->buf_area.x1;
    const float cy = dsc->center_y - (float)layer->buf_area.y1;

    uint32_t blending_mode;
    if(layer->color_format == LV_COLOR_FORMAT_ARGB8888) {
        blending_mode = NEMA_BL_SRC_OVER | NEMA_BLOP_SRC_PREMULT;
    }
    else {
        blending_mode = NEMA_BL_SRC_OVER;
    }

    nema_vg_paint_set_opacity(unit->vg_paint, 1.0f);
    if(dsc->use_gradient) {
        lv_draw_ambiq_set_conical_gradient(unit->vg_grad, unit->vg_paint, dsc, cx, cy);
    }
    else {
        nema_vg_paint_set_type(unit->vg_paint, NEMA_VG_PAINT_COLOR);
        nema_vg_paint_set_paint_color(unit->vg_paint, nema_rgba(dsc->start_color.red,
                                                                dsc->start_color.green,
                                                                dsc->start_color.blue,
                                                                dsc->start_color.alpha));
    }

    nema_vg_stroke_set_width(dsc->width);
    if(dsc->rounded) {
        nema_vg_stroke_set_cap_style(NEMA_VG_CAP_ROUND, NEMA_VG_CAP_ROUND);
    }
    else {
        nema_vg_stroke_set_cap_style(NEMA_VG_CAP_BUTT, NEMA_VG_CAP_BUTT);
    }
    nema_vg_set_quality(NEMA_VG_QUALITY_BETTER);
    nema_vg_set_blend(blending_mode);
    nema_vg_draw_ring(cx, cy, dsc->radius, dsc->start_angle, dsc->end_angle, unit->vg_paint);
}
#endif

/**********************
 *   STATIC FUNCTIONS
 **********************/
#if LV_USE_AMBIQ_VG
static color_var_t lv_draw_ambiq_gradient_color(lv_color32_t color)
{
    color_var_t result = {
        (float)color.red,
        (float)color.green,
        (float)color.blue,
        (float)color.alpha,
    };
    return result;
}

static color_var_t lv_draw_ambiq_gradient_mix_color(lv_color32_t start_color, lv_color32_t end_color, float ratio)
{
    if(ratio <= 0.0f) return lv_draw_ambiq_gradient_color(start_color);
    if(ratio >= 1.0f) return lv_draw_ambiq_gradient_color(end_color);

    color_var_t result = {
        (float)start_color.red + ((float)end_color.red - (float)start_color.red) * ratio,
        (float)start_color.green + ((float)end_color.green - (float)start_color.green) * ratio,
        (float)start_color.blue + ((float)end_color.blue - (float)start_color.blue) * ratio,
        (float)start_color.alpha + ((float)end_color.alpha - (float)start_color.alpha) * ratio,
    };
    return result;
}

static float lv_draw_ambiq_normalize_angle(float angle)
{
    while(angle < 0.0f) {
        angle += 360.0f;
    }
    while(angle >= 360.0f) {
        angle -= 360.0f;
    }
    return angle;
}

static float lv_draw_ambiq_gradient_sweep(float start_angle, float end_angle)
{
    float sweep = end_angle - start_angle;
    while(sweep <= 0.0f) {
        sweep += 360.0f;
    }
    return sweep;
}

static void lv_draw_ambiq_set_conical_gradient(NEMA_VG_GRAD_HANDLE grad,
                                               NEMA_VG_PAINT_HANDLE paint,
                                               const lv_draw_gradient_arc_dsc_t * dsc,
                                               float cx,
                                               float cy)
{
    float start_pos = lv_draw_ambiq_normalize_angle(dsc->start_angle) / 360.0f;
    float end_pos = lv_draw_ambiq_normalize_angle(dsc->end_angle) / 360.0f;
    float sweep_pos = lv_draw_ambiq_gradient_sweep(dsc->start_angle, dsc->end_angle) / 360.0f;
    float stops[4];
    color_var_t colors[4];
    int count = 0;

    if(sweep_pos >= 1.0f) {
        stops[count] = 0.0f;
        colors[count++] = lv_draw_ambiq_gradient_color(dsc->start_color);
        stops[count] = 1.0f;
        colors[count++] = lv_draw_ambiq_gradient_color(dsc->end_color);
    }
    else if(start_pos < end_pos) {
        if(start_pos > 0.0f) {
            stops[count] = 0.0f;
            colors[count++] = lv_draw_ambiq_gradient_color(dsc->start_color);
        }
        stops[count] = start_pos;
        colors[count++] = lv_draw_ambiq_gradient_color(dsc->start_color);
        stops[count] = end_pos;
        colors[count++] = lv_draw_ambiq_gradient_color(dsc->end_color);
        if(end_pos < 1.0f) {
            stops[count] = 1.0f;
            colors[count++] = lv_draw_ambiq_gradient_color(dsc->end_color);
        }
    }
    else {
        float zero_ratio = (1.0f - start_pos) / sweep_pos;
        color_var_t zero_color = lv_draw_ambiq_gradient_mix_color(dsc->start_color, dsc->end_color, zero_ratio);

        stops[count] = 0.0f;
        colors[count++] = zero_color;
        if(end_pos > 0.0f) {
            stops[count] = end_pos;
            colors[count++] = lv_draw_ambiq_gradient_color(dsc->end_color);
        }
        stops[count] = start_pos;
        colors[count++] = lv_draw_ambiq_gradient_color(dsc->start_color);
        stops[count] = 1.0f;
        colors[count++] = zero_color;
    }

    nema_vg_grad_set(grad, count, stops, colors);
    nema_vg_paint_set_type(paint, NEMA_VG_PAINT_GRAD_CONICAL);
    nema_vg_paint_set_grad_conical(paint, grad, cx, cy, NEMA_TEX_CLAMP);
}
#endif

#endif /*LV_USE_DRAW_AMBIQ*/
