/**
 * @file lv_draw_sw_gradient_arc.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "../../misc/lv_area_private.h"
#include "../../stdlib/lv_mem.h"
#include "../lv_draw_private.h"
#include "blend/lv_draw_sw_blend_private.h"
#include "lv_draw_sw.h"
#if LV_USE_DRAW_SW

#include <math.h>

/*********************
 *      DEFINES
 *********************/
#define GRADIENT_ARC_PI 3.14159265358979323846f
#define GRADIENT_ARC_AA_WIDTH 1.0f

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static float normalize_angle(float angle);
static float angle_sweep(float start_angle, float end_angle);
static bool angle_in_sweep(float angle, float start_angle, float sweep, float * ratio);
static uint8_t ring_coverage(float radius, float inner_radius, float outer_radius);
static uint8_t circle_coverage(float radius, float circle_radius);
static uint8_t mix_channel(uint8_t from, uint8_t to, float ratio);
static lv_color32_t mix_color(lv_color32_t from, lv_color32_t to, float ratio, uint8_t alpha);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void lv_draw_sw_gradient_arc(lv_draw_task_t * t, const lv_draw_gradient_arc_dsc_t * dsc,
                             const lv_area_t * coords)
{
    if(dsc == NULL) return;
    if(dsc->width <= 0.0f) return;
    if(dsc->radius <= 0.0f) return;
    if(dsc->start_angle == dsc->end_angle) return;

    lv_area_t clipped_area;
    if(!lv_area_intersect(&clipped_area, coords, &t->clip_area)) return;

    const int32_t clipped_w = lv_area_get_width(&clipped_area);
    lv_color32_t * line_buf = lv_malloc(sizeof(lv_color32_t) * clipped_w);
    if(line_buf == NULL) {
        LV_LOG_WARN("No memory for SW gradient arc line buffer");
        return;
    }

    const float start_angle = normalize_angle(dsc->start_angle);
    const float sweep = angle_sweep(dsc->start_angle, dsc->end_angle);
    const float inner_radius = dsc->radius - (dsc->width * 0.5f);
    const float outer_radius = dsc->radius + (dsc->width * 0.5f);
    const float cap_radius = dsc->width * 0.5f;
    const bool draw_start_cap = dsc->round_start && sweep < 360.0f;
    const bool draw_end_cap = dsc->round_end && sweep < 360.0f;
    const float start_rad = start_angle * GRADIENT_ARC_PI / 180.0f;
    const float end_rad = normalize_angle(dsc->start_angle + sweep) * GRADIENT_ARC_PI / 180.0f;
    const float start_cap_x = dsc->center_x + (cosf(start_rad) * dsc->radius);
    const float start_cap_y = dsc->center_y + (sinf(start_rad) * dsc->radius);
    const float end_cap_x = dsc->center_x + (cosf(end_rad) * dsc->radius);
    const float end_cap_y = dsc->center_y + (sinf(end_rad) * dsc->radius);

    lv_area_t line_area = clipped_area;
    lv_draw_sw_blend_dsc_t blend_dsc;
    lv_memzero(&blend_dsc, sizeof(blend_dsc));
    blend_dsc.blend_area = &line_area;
    blend_dsc.src_area = &line_area;
    blend_dsc.src_buf = line_buf;
    blend_dsc.src_stride = sizeof(lv_color32_t) * clipped_w;
    blend_dsc.src_color_format = LV_COLOR_FORMAT_ARGB8888;
    blend_dsc.opa = LV_OPA_COVER;
    blend_dsc.blend_mode = LV_BLEND_MODE_NORMAL;

    for(int32_t y = clipped_area.y1; y <= clipped_area.y2; y++) {
        for(int32_t x = clipped_area.x1; x <= clipped_area.x2; x++) {
            const float dx = ((float)x + 0.5f) - dsc->center_x;
            const float dy = ((float)y + 0.5f) - dsc->center_y;
            const float radius = sqrtf((dx * dx) + (dy * dy));
            uint8_t coverage = 0U;
            float ratio = 0.0f;
            lv_color32_t * pixel = &line_buf[x - clipped_area.x1];

            float angle = atan2f(dy, dx) * 180.0f / GRADIENT_ARC_PI;
            angle = normalize_angle(angle);

            if(angle_in_sweep(angle, start_angle, sweep, &ratio)) {
                coverage = ring_coverage(radius, inner_radius, outer_radius);
            }

            if(draw_start_cap) {
                const float px = (float)x + 0.5f;
                const float py = (float)y + 0.5f;
                const float start_dx = px - start_cap_x;
                const float start_dy = py - start_cap_y;
                const uint8_t start_cap_coverage = circle_coverage(sqrtf((start_dx * start_dx) + (start_dy * start_dy)),
                                                                    cap_radius);
                if(start_cap_coverage > coverage) {
                    coverage = start_cap_coverage;
                    ratio = 0.0f;
                }
            }
            if(draw_end_cap) {
                const float px = (float)x + 0.5f;
                const float py = (float)y + 0.5f;
                const float end_dx = px - end_cap_x;
                const float end_dy = py - end_cap_y;
                const uint8_t end_cap_coverage = circle_coverage(sqrtf((end_dx * end_dx) + (end_dy * end_dy)),
                                                                  cap_radius);
                if(end_cap_coverage > coverage) {
                    coverage = end_cap_coverage;
                    ratio = 1.0f;
                }
            }

            if(coverage == 0U) {
                lv_memzero(pixel, sizeof(*pixel));
                continue;
            }

            if(dsc->use_gradient) {
                const uint8_t alpha = (uint8_t)(((uint16_t)coverage * dsc->start_color.alpha) / 255U);
                *pixel = mix_color(dsc->start_color, dsc->end_color, ratio, alpha);
            }
            else {
                *pixel = dsc->start_color;
                pixel->alpha = (uint8_t)(((uint16_t)coverage * dsc->start_color.alpha) / 255U);
            }
        }

        line_area.y1 = y;
        line_area.y2 = y;
        lv_draw_sw_blend(t, &blend_dsc);
    }

    lv_free(line_buf);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static float normalize_angle(float angle)
{
    while(angle < 0.0f) {
        angle += 360.0f;
    }
    while(angle >= 360.0f) {
        angle -= 360.0f;
    }
    return angle;
}

static float angle_sweep(float start_angle, float end_angle)
{
    float sweep = end_angle - start_angle;
    while(sweep <= 0.0f) {
        sweep += 360.0f;
    }
    return sweep;
}

static bool angle_in_sweep(float angle, float start_angle, float sweep, float * ratio)
{
    float normalized_angle = angle;
    if(normalized_angle < start_angle) {
        normalized_angle += 360.0f;
    }

    if(normalized_angle < start_angle || normalized_angle > start_angle + sweep) {
        return false;
    }

    *ratio = sweep > 0.0f ? (normalized_angle - start_angle) / sweep : 0.0f;
    if(*ratio > 1.0f) *ratio = 1.0f;
    return true;
}

static uint8_t ring_coverage(float radius, float inner_radius, float outer_radius)
{
    if(radius < inner_radius - GRADIENT_ARC_AA_WIDTH || radius > outer_radius + GRADIENT_ARC_AA_WIDTH) {
        return 0U;
    }
    if(radius >= inner_radius + GRADIENT_ARC_AA_WIDTH && radius <= outer_radius - GRADIENT_ARC_AA_WIDTH) {
        return 255U;
    }

    float coverage = 1.0f;
    if(radius < inner_radius + GRADIENT_ARC_AA_WIDTH) {
        coverage = (radius - (inner_radius - GRADIENT_ARC_AA_WIDTH)) / (2.0f * GRADIENT_ARC_AA_WIDTH);
    }
    else if(radius > outer_radius - GRADIENT_ARC_AA_WIDTH) {
        coverage = ((outer_radius + GRADIENT_ARC_AA_WIDTH) - radius) / (2.0f * GRADIENT_ARC_AA_WIDTH);
    }

    if(coverage <= 0.0f) return 0U;
    if(coverage >= 1.0f) return 255U;
    return (uint8_t)((coverage * 255.0f) + 0.5f);
}

static uint8_t circle_coverage(float radius, float circle_radius)
{
    if(radius < 0.0f) return 0U;
    if(radius > circle_radius + GRADIENT_ARC_AA_WIDTH) return 0U;
    if(radius <= circle_radius - GRADIENT_ARC_AA_WIDTH) return 255U;

    const float coverage = ((circle_radius + GRADIENT_ARC_AA_WIDTH) - radius) /
                           (2.0f * GRADIENT_ARC_AA_WIDTH);
    if(coverage <= 0.0f) return 0U;
    if(coverage >= 1.0f) return 255U;
    return (uint8_t)((coverage * 255.0f) + 0.5f);
}

static uint8_t mix_channel(uint8_t from, uint8_t to, float ratio)
{
    const float value = (float)from + (((float)to - (float)from) * ratio);
    if(value <= 0.0f) return 0U;
    if(value >= 255.0f) return 255U;
    return (uint8_t)(value + 0.5f);
}

static lv_color32_t mix_color(lv_color32_t from, lv_color32_t to, float ratio, uint8_t alpha)
{
    lv_color32_t color;
    color.red = mix_channel(from.red, to.red, ratio);
    color.green = mix_channel(from.green, to.green, ratio);
    color.blue = mix_channel(from.blue, to.blue, ratio);
    color.alpha = alpha;
    return color;
}

#endif /*LV_USE_DRAW_SW*/
