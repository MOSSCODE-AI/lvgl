// Copyright wuyin 2026. All rights reserved.

/**
 * @file lv_draw_sw_rounded_rectangle_path.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_draw_sw.h"

#if LV_USE_DRAW_SW

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
void lv_draw_sw_rounded_rectangle_path(lv_draw_task_t * t, const lv_draw_rounded_rectangle_path_dsc_t * dsc,
                                       const lv_area_t * coords)
{
    LV_UNUSED(coords);

    if(dsc == NULL) return;
    if(dsc->path == NULL) return;
    if(dsc->path->points == NULL || dsc->path->point_count < 2U) return;
    if(dsc->width <= 0) return;
    if(dsc->color.alpha <= LV_OPA_MIN) return;

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.base = dsc->base;
    line_dsc.color = lv_color_make(dsc->color.red, dsc->color.green, dsc->color.blue);
    line_dsc.opa = dsc->color.alpha;
    line_dsc.width = dsc->width;
    line_dsc.round_start = dsc->rounded ? 1U : 0U;
    line_dsc.round_end = dsc->rounded ? 1U : 0U;

    for(uint16_t i = 1U; i < dsc->path->point_count; ++i) {
        line_dsc.p1 = dsc->path->points[i - 1U];
        line_dsc.p2 = dsc->path->points[i];
        lv_draw_sw_line(t, &line_dsc);
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

#endif /*LV_USE_DRAW_SW*/
