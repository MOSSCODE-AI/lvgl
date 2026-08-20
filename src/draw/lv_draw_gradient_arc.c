// Copyright wuyin Oy 2026. All rights reserved.

/**
 * @file lv_draw_gradient_arc.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_draw_private.h"
#include "lv_draw_gradient_arc.h"
#include "../stdlib/lv_string.h"

/*********************
 *      DEFINES
 *********************/
#define GRADIENT_ARC_AA_EXTEND 1.0f

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static int32_t floor_to_i32(float value);
static int32_t ceil_to_i32(float value);
static void get_gradient_arc_area(const lv_draw_gradient_arc_dsc_t * dsc, lv_area_t * area);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void lv_draw_gradient_arc_dsc_init(lv_draw_gradient_arc_dsc_t * dsc)
{
    lv_memzero(dsc, sizeof(lv_draw_gradient_arc_dsc_t));
    dsc->radius = 1.0f;
    dsc->width = 1.0f;
    dsc->start_color = lv_color_to_32(lv_color_black(), LV_OPA_COVER);
    dsc->end_color = lv_color_to_32(lv_color_black(), LV_OPA_COVER);
    dsc->round_start = true;
    dsc->round_end = true;
    dsc->use_gradient = true;
    dsc->base.dsc_size = sizeof(lv_draw_gradient_arc_dsc_t);
}

lv_draw_gradient_arc_dsc_t * lv_draw_task_get_gradient_arc_dsc(lv_draw_task_t * task)
{
    return task->type == LV_DRAW_TASK_TYPE_GRADIENT_ARC ? (lv_draw_gradient_arc_dsc_t *)task->draw_dsc : NULL;
}

void lv_draw_gradient_arc(lv_layer_t * layer, const lv_draw_gradient_arc_dsc_t * dsc)
{
    if(dsc == NULL) return;
    if(dsc->width <= 0.0f) return;
    if(dsc->radius <= 0.0f) return;
    if(dsc->start_angle == dsc->end_angle) return;

    LV_PROFILER_DRAW_BEGIN;

    lv_area_t area;
    get_gradient_arc_area(dsc, &area);

    if(dsc->base.drop_shadow_opa) {
        lv_layer_t * ds_layer = lv_draw_layer_create_drop_shadow(layer, &dsc->base, &area);
        LV_ASSERT_NULL(ds_layer);
        lv_draw_gradient_arc_dsc_t ds_dsc = *dsc;
        ds_dsc.base.drop_shadow_opa = 0;
        lv_draw_gradient_arc(ds_layer, &ds_dsc);
        lv_draw_layer_finish_drop_shadow(ds_layer, &dsc->base);
    }

    lv_draw_task_t * t = lv_draw_add_task(layer, &area, LV_DRAW_TASK_TYPE_GRADIENT_ARC);
    lv_memcpy(t->draw_dsc, dsc, sizeof(*dsc));
    lv_draw_finalize_task_creation(layer, t);

    LV_PROFILER_DRAW_END;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static int32_t floor_to_i32(float value)
{
    int32_t result = (int32_t)value;
    if((float)result > value) result--;
    return result;
}

static int32_t ceil_to_i32(float value)
{
    int32_t result = (int32_t)value;
    if((float)result < value) result++;
    return result;
}

static void get_gradient_arc_area(const lv_draw_gradient_arc_dsc_t * dsc, lv_area_t * area)
{
    const float half_width = dsc->width * 0.5f;
    const float extent = dsc->radius + half_width + GRADIENT_ARC_AA_EXTEND;

    area->x1 = floor_to_i32(dsc->center_x - extent);
    area->y1 = floor_to_i32(dsc->center_y - extent);
    area->x2 = ceil_to_i32(dsc->center_x + extent);
    area->y2 = ceil_to_i32(dsc->center_y + extent);
}
