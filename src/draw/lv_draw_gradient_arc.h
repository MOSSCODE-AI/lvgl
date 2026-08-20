/**
 * @file lv_draw_gradient_arc.h
 *
 */

#ifndef LV_DRAW_GRADIENT_ARC_H
#define LV_DRAW_GRADIENT_ARC_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lv_draw.h"
#include "../misc/lv_color.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    lv_draw_dsc_base_t base;

    float center_x;
    float center_y;
    float radius;
    float width;
    float start_angle;
    float end_angle;
    lv_color32_t start_color;
    lv_color32_t end_color;
    bool round_start;
    bool round_end;
    bool use_gradient;
} lv_draw_gradient_arc_dsc_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Initialize a gradient arc draw descriptor.
 * @param dsc       pointer to a draw descriptor
 */
void lv_draw_gradient_arc_dsc_init(lv_draw_gradient_arc_dsc_t * dsc);

/**
 * Try to get a gradient arc draw descriptor from a draw task.
 * @param task      draw task
 * @return          the task's draw descriptor or NULL if the task is not of type LV_DRAW_TASK_TYPE_GRADIENT_ARC
 */
lv_draw_gradient_arc_dsc_t * lv_draw_task_get_gradient_arc_dsc(lv_draw_task_t * task);

/**
 * Create a gradient arc draw task.
 * @param layer     pointer to a layer
 * @param dsc       pointer to an initialized draw descriptor variable
 */
void lv_draw_gradient_arc(lv_layer_t * layer, const lv_draw_gradient_arc_dsc_t * dsc);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_DRAW_GRADIENT_ARC_H*/
