// Copyright wuyin Oy 2026. All rights reserved.

/**
 * @file lv_draw_rounded_rectangle_path.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_draw_private.h"
#include "lv_draw_rounded_rectangle_path.h"
#include "../stdlib/lv_string.h"

#include <float.h>
#include <math.h>

/*********************
 *      DEFINES
 *********************/
#define ROUNDED_RECTANGLE_PATH_AA_EXTEND 1
#define ROUNDED_RECTANGLE_PATH_MIN_ARC_STEPS 2U
#define ROUNDED_RECTANGLE_PATH_MAX_ARC_STEPS ((LV_DRAW_ROUNDED_RECTANGLE_PATH_MAX_POINTS - 6U) / 4U)
#define ROUNDED_RECTANGLE_PATH_ARC_SAMPLE_LENGTH 7.0f
#define ROUNDED_RECTANGLE_PATH_START_EPSILON 0.0001f
#define ROUNDED_RECTANGLE_PATH_PI 3.1415926f

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static int32_t floor_to_i32(float value);
static int32_t ceil_to_i32(float value);
static float clamp_float(float value, float min, float max);
static float normalize_start_ratio(float start_ratio);
static uint16_t calc_arc_steps(float radius);
static uint16_t calc_full_point_count(uint16_t arc_steps);
static void add_point(lv_point_precise_t * points, uint16_t * point_count, uint16_t point_capacity, float x, float y);
static void add_arc_points(lv_point_precise_t * points, uint16_t * point_count,
                           uint16_t point_capacity, uint16_t arc_steps,
                           float center_x, float center_y, float radius,
                           float start_angle, float end_angle);
static float point_distance(const lv_point_precise_t * p1, const lv_point_precise_t * p2);
static lv_result_t rotate_path_start(lv_point_precise_t * points,
                                     lv_point_precise_t * temp_points,
                                     uint16_t * point_count,
                                     uint16_t point_capacity,
                                     float start_ratio);
static void rotate_path_from_existing_point(const lv_point_precise_t * points,
                                            lv_point_precise_t * temp_points,
                                            uint16_t * point_count,
                                            uint16_t start_index);
static void copy_temp_points(lv_point_precise_t * points,
                             const lv_point_precise_t * temp_points,
                             uint16_t point_count);
static bool is_built_path(const lv_draw_rounded_rectangle_path_data_t * path);
static bool is_drawable_path(const lv_draw_rounded_rectangle_path_data_t * path);
static bool is_drawable_dsc(const lv_draw_rounded_rectangle_path_dsc_t * dsc);
static void get_rounded_rectangle_path_area(const lv_draw_rounded_rectangle_path_dsc_t * dsc, lv_area_t * area);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void lv_draw_rounded_rectangle_path_dsc_init(lv_draw_rounded_rectangle_path_dsc_t * dsc)
{
    lv_memzero(dsc, sizeof(lv_draw_rounded_rectangle_path_dsc_t));
    dsc->width = 1;
    dsc->color = lv_color_to_32(lv_color_black(), LV_OPA_COVER);
    dsc->rounded = true;
    dsc->base.dsc_size = sizeof(lv_draw_rounded_rectangle_path_dsc_t);
}

void lv_draw_rounded_rectangle_path_data_build_dsc_init(lv_draw_rounded_rectangle_path_data_build_dsc_t * dsc)
{
    lv_memzero(dsc, sizeof(lv_draw_rounded_rectangle_path_data_build_dsc_t));
}

lv_result_t lv_draw_rounded_rectangle_path_data_build(lv_draw_rounded_rectangle_path_data_t * path,
                                                      const lv_draw_rounded_rectangle_path_data_build_dsc_t * dsc)
{
    if(path == NULL || dsc == NULL) return LV_RESULT_INVALID;

    const float left = (float)dsc->coords.x1 + dsc->inset;
    const float top = (float)dsc->coords.y1 + dsc->inset;
    const float right = (float)dsc->coords.x2 - dsc->inset;
    const float bottom = (float)dsc->coords.y2 - dsc->inset;
    const float width = right - left;
    const float height = bottom - top;
    if(width <= 0.0f || height <= 0.0f) return LV_RESULT_INVALID;

    const float radius = clamp_float(dsc->radius, 0.0f, LV_MIN(width, height) * 0.5f);
    const uint16_t arc_steps = calc_arc_steps(radius);
    const uint16_t base_point_count = calc_full_point_count(arc_steps);
    const uint16_t full_point_capacity = base_point_count + 1U;

    lv_point_precise_t * full_points = lv_malloc(sizeof(lv_point_precise_t) * full_point_capacity);
    float * segment_lengths = lv_malloc(sizeof(float) * (full_point_capacity - 1U));
    lv_point_precise_t * points = lv_malloc(sizeof(lv_point_precise_t) * full_point_capacity);
    if(full_points == NULL || segment_lengths == NULL || points == NULL) {
        lv_free(full_points);
        lv_free(segment_lengths);
        lv_free(points);
        return LV_RESULT_INVALID;
    }

    uint16_t full_point_count = 0U;

    if(arc_steps == 0U) {
        add_point(full_points, &full_point_count, full_point_capacity, left, top);
        add_point(full_points, &full_point_count, full_point_capacity, right, top);
        add_point(full_points, &full_point_count, full_point_capacity, right, bottom);
        add_point(full_points, &full_point_count, full_point_capacity, left, bottom);
        add_point(full_points, &full_point_count, full_point_capacity, left, top);
    }
    else {
        add_point(full_points, &full_point_count, full_point_capacity, left + radius, top);
        add_point(full_points, &full_point_count, full_point_capacity, right - radius, top);
        add_arc_points(full_points, &full_point_count, full_point_capacity, arc_steps,
                       right - radius, top + radius, radius, -90.0f, 0.0f);
        add_point(full_points, &full_point_count, full_point_capacity, right, bottom - radius);
        add_arc_points(full_points, &full_point_count, full_point_capacity, arc_steps,
                       right - radius, bottom - radius, radius, 0.0f, 90.0f);
        add_point(full_points, &full_point_count, full_point_capacity, left + radius, bottom);
        add_arc_points(full_points, &full_point_count, full_point_capacity, arc_steps,
                       left + radius, bottom - radius, radius, 90.0f, 180.0f);
        add_point(full_points, &full_point_count, full_point_capacity, left, top + radius);
        add_arc_points(full_points, &full_point_count, full_point_capacity, arc_steps,
                       left + radius, top + radius, radius, 180.0f, 270.0f);
    }

    if(full_point_count < 2U || full_point_count != base_point_count) {
        lv_free(full_points);
        lv_free(segment_lengths);
        lv_free(points);
        return LV_RESULT_INVALID;
    }

    if(rotate_path_start(full_points, points, &full_point_count,
                         full_point_capacity, dsc->start_ratio) != LV_RESULT_OK) {
        lv_free(full_points);
        lv_free(segment_lengths);
        lv_free(points);
        return LV_RESULT_INVALID;
    }

    float total_length = 0.0f;
    for(uint16_t i = 1U; i < full_point_count; ++i) {
        const float length = point_distance(&full_points[i - 1U], &full_points[i]);
        segment_lengths[i - 1U] = length;
        total_length += length;
    }

    lv_draw_rounded_rectangle_path_data_release(path);
    path->full_points = full_points;
    path->segment_lengths = segment_lengths;
    path->points = points;
    path->full_point_count = full_point_count;
    path->total_length = total_length;
    path->area = dsc->coords;

    return lv_draw_rounded_rectangle_path_data_set_progress(path, 1, 1);
}

lv_result_t lv_draw_rounded_rectangle_path_data_set_progress(lv_draw_rounded_rectangle_path_data_t * path,
                                                             int32_t progress,
                                                             int32_t progress_max)
{
    if(!is_built_path(path)) return LV_RESULT_INVALID;
    if(progress_max <= 0) return LV_RESULT_INVALID;

    if(progress <= 0 || path->total_length <= 0.0f) {
        path->point_count = 0U;
        return LV_RESULT_OK;
    }

    if(progress >= progress_max) {
        const size_t points_size = sizeof(lv_point_precise_t) * path->full_point_count;
        lv_memcpy(path->points, path->full_points, points_size);
        path->point_count = path->full_point_count;
        return LV_RESULT_OK;
    }

    const float target_length = path->total_length * ((float)progress / (float)progress_max);
    float traversed_length = 0.0f;
    uint16_t point_count = 0U;
    path->points[point_count++] = path->full_points[0];

    for(uint16_t i = 1U; i < path->full_point_count; ++i) {
        const float segment_length = path->segment_lengths[i - 1U];
        if(segment_length <= 0.0f) continue;

        if(traversed_length + segment_length <= target_length) {
            path->points[point_count++] = path->full_points[i];
            traversed_length += segment_length;
            continue;
        }

        const float ratio = (target_length - traversed_length) / segment_length;
        const lv_point_precise_t * start = &path->full_points[i - 1U];
        const lv_point_precise_t * end = &path->full_points[i];
        path->points[point_count].x = start->x + ((end->x - start->x) * ratio);
        path->points[point_count].y = start->y + ((end->y - start->y) * ratio);
        point_count++;
        break;
    }

    path->point_count = point_count;
    return LV_RESULT_OK;
}

void lv_draw_rounded_rectangle_path_data_release(lv_draw_rounded_rectangle_path_data_t * path)
{
    if(path == NULL) return;

    lv_free(path->points);
    lv_free(path->full_points);
    lv_free(path->segment_lengths);
    lv_free(path->render_segments);
    lv_free(path->render_data);
    lv_memzero(path, sizeof(lv_draw_rounded_rectangle_path_data_t));
}

lv_draw_rounded_rectangle_path_dsc_t * lv_draw_task_get_rounded_rectangle_path_dsc(lv_draw_task_t * task)
{
    return task->type == LV_DRAW_TASK_TYPE_ROUNDED_RECTANGLE_PATH ?
           (lv_draw_rounded_rectangle_path_dsc_t *)task->draw_dsc : NULL;
}

void lv_draw_rounded_rectangle_path(lv_layer_t * layer, const lv_draw_rounded_rectangle_path_dsc_t * dsc)
{
    if(layer == NULL) return;
    if(!is_drawable_dsc(dsc)) return;

    LV_PROFILER_DRAW_BEGIN;

    lv_area_t area;
    get_rounded_rectangle_path_area(dsc, &area);

    lv_draw_rounded_rectangle_path_data_t * task_path = lv_malloc_zeroed(sizeof(lv_draw_rounded_rectangle_path_data_t));
    lv_point_precise_t * points = lv_malloc(sizeof(lv_point_precise_t) * dsc->path->point_count);
    uint8_t * render_segments = lv_malloc(sizeof(uint8_t) * dsc->path->point_count);
    float * render_data = lv_malloc(sizeof(float) * dsc->path->point_count * 2U);
    if(task_path == NULL || points == NULL || render_segments == NULL || render_data == NULL) {
        lv_free(task_path);
        lv_free(points);
        lv_free(render_segments);
        lv_free(render_data);
        LV_PROFILER_DRAW_END;
        return;
    }

    lv_memcpy(points, dsc->path->points, sizeof(lv_point_precise_t) * dsc->path->point_count);
    task_path->points = points;
    task_path->point_count = dsc->path->point_count;
    task_path->render_segments = render_segments;
    task_path->render_data = render_data;

    lv_draw_task_t * t = lv_draw_add_task(layer, &area, LV_DRAW_TASK_TYPE_ROUNDED_RECTANGLE_PATH);
    if(t == NULL) {
        lv_draw_rounded_rectangle_path_data_release(task_path);
        lv_free(task_path);
        LV_PROFILER_DRAW_END;
        return;
    }
    lv_memcpy(t->draw_dsc, dsc, sizeof(*dsc));

    lv_draw_rounded_rectangle_path_dsc_t * task_dsc = t->draw_dsc;
    task_dsc->path = task_path;

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

static float clamp_float(float value, float min, float max)
{
    if(value < min) return min;
    if(value > max) return max;
    return value;
}

static float normalize_start_ratio(float start_ratio)
{
    if(!(start_ratio > -FLT_MAX && start_ratio < FLT_MAX)) return 0.0f;

    if(start_ratio < 0.0f || start_ratio >= 1.0f) {
        start_ratio -= floorf(start_ratio);
    }
    if(start_ratio < 0.0f) {
        start_ratio += 1.0f;
    }
    if(start_ratio >= 1.0f) {
        start_ratio = 0.0f;
    }

    return start_ratio;
}

static uint16_t calc_arc_steps(float radius)
{
    if(radius <= 0.0f) return 0U;

    const float quarter_arc_length = radius * ROUNDED_RECTANGLE_PATH_PI * 0.5f;
    int32_t arc_steps = ceil_to_i32(quarter_arc_length / ROUNDED_RECTANGLE_PATH_ARC_SAMPLE_LENGTH);
    if(arc_steps < (int32_t)ROUNDED_RECTANGLE_PATH_MIN_ARC_STEPS) {
        arc_steps = (int32_t)ROUNDED_RECTANGLE_PATH_MIN_ARC_STEPS;
    }
    if(arc_steps > (int32_t)ROUNDED_RECTANGLE_PATH_MAX_ARC_STEPS) {
        arc_steps = (int32_t)ROUNDED_RECTANGLE_PATH_MAX_ARC_STEPS;
    }

    return (uint16_t)arc_steps;
}

static uint16_t calc_full_point_count(uint16_t arc_steps)
{
    if(arc_steps == 0U) return 5U;
    return (uint16_t)(5U + (arc_steps * 4U));
}

static void add_point(lv_point_precise_t * points, uint16_t * point_count, uint16_t point_capacity, float x, float y)
{
    if(*point_count >= point_capacity) return;
    points[*point_count].x = x;
    points[*point_count].y = y;
    (*point_count)++;
}

static void add_arc_points(lv_point_precise_t * points, uint16_t * point_count,
                           uint16_t point_capacity, uint16_t arc_steps,
                           float center_x, float center_y, float radius,
                           float start_angle, float end_angle)
{
    const float step_angle = (end_angle - start_angle) / (float)arc_steps;
    for(uint16_t i = 1U; i <= arc_steps; ++i) {
        const float angle = (start_angle + (step_angle * (float)i)) * ROUNDED_RECTANGLE_PATH_PI / 180.0f;
        add_point(points, point_count, point_capacity,
                  center_x + (cosf(angle) * radius), center_y + (sinf(angle) * radius));
    }
}

static float point_distance(const lv_point_precise_t * p1, const lv_point_precise_t * p2)
{
    const float dx = p2->x - p1->x;
    const float dy = p2->y - p1->y;
    return sqrtf((dx * dx) + (dy * dy));
}

static lv_result_t rotate_path_start(lv_point_precise_t * points,
                                     lv_point_precise_t * temp_points,
                                     uint16_t * point_count,
                                     uint16_t point_capacity,
                                     float start_ratio)
{
    if(points == NULL || temp_points == NULL || point_count == NULL) return LV_RESULT_INVALID;
    if(*point_count < 2U || *point_count > point_capacity) return LV_RESULT_INVALID;

    const uint16_t old_point_count = *point_count;
    const float normalized_start_ratio = normalize_start_ratio(start_ratio);
    if(normalized_start_ratio <= ROUNDED_RECTANGLE_PATH_START_EPSILON) return LV_RESULT_OK;

    float total_length = 0.0f;
    for(uint16_t i = 1U; i < old_point_count; ++i) {
        total_length += point_distance(&points[i - 1U], &points[i]);
    }
    if(total_length <= 0.0f) return LV_RESULT_INVALID;

    const float target_length = total_length * normalized_start_ratio;
    if(target_length <= ROUNDED_RECTANGLE_PATH_START_EPSILON ||
       total_length - target_length <= ROUNDED_RECTANGLE_PATH_START_EPSILON) {
        return LV_RESULT_OK;
    }

    float traversed_length = 0.0f;
    for(uint16_t i = 1U; i < old_point_count; ++i) {
        const float segment_length = point_distance(&points[i - 1U], &points[i]);
        if(segment_length <= 0.0f) continue;

        if(traversed_length + segment_length < target_length) {
            traversed_length += segment_length;
            continue;
        }

        const float segment_offset = target_length - traversed_length;
        if(segment_offset <= ROUNDED_RECTANGLE_PATH_START_EPSILON) {
            rotate_path_from_existing_point(points, temp_points, point_count, i - 1U);
            copy_temp_points(points, temp_points, *point_count);
            return LV_RESULT_OK;
        }

        if(segment_length - segment_offset <= ROUNDED_RECTANGLE_PATH_START_EPSILON) {
            uint16_t start_index = i;
            if(start_index >= old_point_count - 1U) {
                start_index = 0U;
            }
            rotate_path_from_existing_point(points, temp_points, point_count, start_index);
            copy_temp_points(points, temp_points, *point_count);
            return LV_RESULT_OK;
        }

        if(old_point_count + 1U > point_capacity) return LV_RESULT_INVALID;

        const float ratio = segment_offset / segment_length;
        lv_point_precise_t start_point;
        start_point.x = points[i - 1U].x + ((points[i].x - points[i - 1U].x) * ratio);
        start_point.y = points[i - 1U].y + ((points[i].y - points[i - 1U].y) * ratio);

        uint16_t new_point_count = 0U;
        temp_points[new_point_count++] = start_point;
        for(uint16_t j = i; j < old_point_count; ++j) {
            temp_points[new_point_count++] = points[j];
        }
        for(uint16_t j = 1U; j <= i - 1U; ++j) {
            temp_points[new_point_count++] = points[j];
        }
        temp_points[new_point_count++] = start_point;

        *point_count = new_point_count;
        copy_temp_points(points, temp_points, *point_count);
        return LV_RESULT_OK;
    }

    return LV_RESULT_OK;
}

static void rotate_path_from_existing_point(const lv_point_precise_t * points,
                                            lv_point_precise_t * temp_points,
                                            uint16_t * point_count,
                                            uint16_t start_index)
{
    const uint16_t old_point_count = *point_count;
    const uint16_t unique_point_count = old_point_count - 1U;
    if(start_index >= unique_point_count) {
        start_index = 0U;
    }

    uint16_t new_point_count = 0U;
    for(uint16_t i = start_index; i < unique_point_count; ++i) {
        temp_points[new_point_count++] = points[i];
    }
    for(uint16_t i = 0U; i <= start_index; ++i) {
        temp_points[new_point_count++] = points[i];
    }

    *point_count = new_point_count;
}

static void copy_temp_points(lv_point_precise_t * points,
                             const lv_point_precise_t * temp_points,
                             uint16_t point_count)
{
    for(uint16_t i = 0U; i < point_count; ++i) {
        points[i] = temp_points[i];
    }
}

static bool is_built_path(const lv_draw_rounded_rectangle_path_data_t * path)
{
    if(path == NULL) return false;
    if(path->points == NULL) return false;
    if(path->full_points == NULL) return false;
    if(path->segment_lengths == NULL) return false;
    if(path->full_point_count < 2U) return false;
    return true;
}

static bool is_drawable_path(const lv_draw_rounded_rectangle_path_data_t * path)
{
    if(path == NULL) return false;
    if(path->points == NULL) return false;
    if(path->point_count < 2U) return false;
    return true;
}

static bool is_drawable_dsc(const lv_draw_rounded_rectangle_path_dsc_t * dsc)
{
    if(dsc == NULL) return false;
    if(!is_drawable_path(dsc->path)) return false;
    if(dsc->width <= 0) return false;
    if(dsc->color.alpha <= LV_OPA_MIN) return false;
    return true;
}

static void get_rounded_rectangle_path_area(const lv_draw_rounded_rectangle_path_dsc_t * dsc, lv_area_t * area)
{
    area->x1 = floor_to_i32(dsc->path->points[0].x);
    area->y1 = floor_to_i32(dsc->path->points[0].y);
    area->x2 = ceil_to_i32(dsc->path->points[0].x);
    area->y2 = ceil_to_i32(dsc->path->points[0].y);

    for(uint16_t i = 1U; i < dsc->path->point_count; ++i) {
        const int32_t x1 = floor_to_i32(dsc->path->points[i].x);
        const int32_t y1 = floor_to_i32(dsc->path->points[i].y);
        const int32_t x2 = ceil_to_i32(dsc->path->points[i].x);
        const int32_t y2 = ceil_to_i32(dsc->path->points[i].y);

        if(x1 < area->x1) area->x1 = x1;
        if(y1 < area->y1) area->y1 = y1;
        if(x2 > area->x2) area->x2 = x2;
        if(y2 > area->y2) area->y2 = y2;
    }

    lv_area_increase(area, (dsc->width + 1) / 2 + ROUNDED_RECTANGLE_PATH_AA_EXTEND,
                     (dsc->width + 1) / 2 + ROUNDED_RECTANGLE_PATH_AA_EXTEND);
}
