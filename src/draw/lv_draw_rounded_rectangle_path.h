// Copyright wuyin 2026. All rights reserved.

/**
 * @file lv_draw_rounded_rectangle_path.h
 *
 */

#ifndef LV_DRAW_ROUNDED_RECTANGLE_PATH_H
#define LV_DRAW_ROUNDED_RECTANGLE_PATH_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lv_draw.h"
#include "../misc/lv_area.h"
#include "../misc/lv_color.h"

/*********************
 *      DEFINES
 *********************/
#define LV_DRAW_ROUNDED_RECTANGLE_PATH_MAX_POINTS 64U

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    lv_area_t coords;
    float inset;
    float radius;
    float start_ratio;
} lv_draw_rounded_rectangle_path_data_build_dsc_t;

typedef struct {
    lv_point_precise_t * points;
    uint16_t point_count;
    lv_point_precise_t * full_points;
    float * segment_lengths;
    uint8_t * render_segments;
    float * render_data;
    uint16_t full_point_count;
    float total_length;
    lv_area_t area;
} lv_draw_rounded_rectangle_path_data_t;

typedef struct {
    lv_draw_dsc_base_t base;

    const lv_draw_rounded_rectangle_path_data_t * path;
    int32_t width;
    lv_color32_t color;
    bool rounded;
} lv_draw_rounded_rectangle_path_dsc_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Initialize a rounded rectangle path draw descriptor.
 * @param dsc       pointer to a draw descriptor
 */
void lv_draw_rounded_rectangle_path_dsc_init(lv_draw_rounded_rectangle_path_dsc_t * dsc);

/**
 * Initialize a rounded rectangle path data build descriptor.
 * @param dsc       pointer to a build descriptor
 */
void lv_draw_rounded_rectangle_path_data_build_dsc_init(lv_draw_rounded_rectangle_path_data_build_dsc_t * dsc);

/**
 * Build rounded rectangle path data.
 * If path already owns points, they are released first. The visible progress defaults to 100%.
 * The path storage must be zero-initialized before its first build.
 * @param path      pointer to a path storage
 * @param dsc       pointer to an initialized build descriptor
 * @return          LV_RESULT_OK on success, LV_RESULT_INVALID on invalid parameters or allocation failure
 */
lv_result_t lv_draw_rounded_rectangle_path_data_build(lv_draw_rounded_rectangle_path_data_t * path,
                                                      const lv_draw_rounded_rectangle_path_data_build_dsc_t * dsc);

/**
 * Refresh visible progress using already built rounded rectangle path data.
 * @param path          pointer to a built path storage
 * @param progress      current progress
 * @param progress_max  maximum progress value
 * @return              LV_RESULT_OK on success, LV_RESULT_INVALID on invalid parameters
 */
lv_result_t lv_draw_rounded_rectangle_path_data_set_progress(lv_draw_rounded_rectangle_path_data_t * path,
                                                             int32_t progress,
                                                             int32_t progress_max);

/**
 * Release points owned by rounded rectangle path data.
 * @param path      pointer to a path storage
 */
void lv_draw_rounded_rectangle_path_data_release(lv_draw_rounded_rectangle_path_data_t * path);

/**
 * Try to get a rounded rectangle path draw descriptor from a draw task.
 * @param task      draw task
 * @return          the task's draw descriptor or NULL if the task is not of type LV_DRAW_TASK_TYPE_ROUNDED_RECTANGLE_PATH
 */
lv_draw_rounded_rectangle_path_dsc_t * lv_draw_task_get_rounded_rectangle_path_dsc(lv_draw_task_t * task);

/**
 * Create a rounded rectangle path draw task.
 * @param layer     pointer to a layer
 * @param dsc       pointer to an initialized draw descriptor variable
 */
void lv_draw_rounded_rectangle_path(lv_layer_t * layer, const lv_draw_rounded_rectangle_path_dsc_t * dsc);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_DRAW_ROUNDED_RECTANGLE_PATH_H*/
