// Copyright (c) wuyin 2026. All rights reserved.

/**
 * Destination clipping for scaled image quads, including rotation. Texture coordinates
 * still come from the original inverse matrix, so clipping does not stretch.
 */
#ifndef LV_DRAW_AMBIQ_SCALE_CLIP_H
#define LV_DRAW_AMBIQ_SCALE_CLIP_H

#include <stdbool.h>

typedef struct {
    float x1;
    float y1;
    float x2;
    float y2;
} lv_ambiq_scale_rect_t;

typedef struct {
    float x;
    float y;
} lv_ambiq_scale_point_t;

/* Convex quad/rectangle intersection has at most eight vertices. Extra slots
 * allow temporary boundary duplicates without any dynamic allocation. */
#define LV_AMBIQ_SCALE_CLIP_CAPACITY 12

static inline bool lv_ambiq_clip_scale_rect(lv_ambiq_scale_rect_t * rect,
                                           const lv_ambiq_scale_rect_t * clip)
{
    if(rect->x1 < clip->x1) rect->x1 = clip->x1;
    if(rect->y1 < clip->y1) rect->y1 = clip->y1;
    if(rect->x2 > clip->x2) rect->x2 = clip->x2;
    if(rect->y2 > clip->y2) rect->y2 = clip->y2;
    return rect->x1 <= rect->x2 && rect->y1 <= rect->y2;
}

static inline bool lv_ambiq_scale_points_equal(lv_ambiq_scale_point_t a, lv_ambiq_scale_point_t b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx >= -0.0001f && dx <= 0.0001f && dy >= -0.0001f && dy <= 0.0001f;
}

static inline bool lv_ambiq_scale_append(lv_ambiq_scale_point_t * points, unsigned * count,
                                       lv_ambiq_scale_point_t point)
{
    if(*count > 0 && lv_ambiq_scale_points_equal(points[*count - 1], point)) return true;
    if(*count == LV_AMBIQ_SCALE_CLIP_CAPACITY) return false;
    points[(*count)++] = point;
    return true;
}

/* Sutherland-Hodgman clipping in destination coordinates. The caller keeps the
 * original inverse texture matrix; no texture coordinates are re-interpolated. */
static inline unsigned lv_ambiq_clip_scale_quad(const lv_ambiq_scale_point_t quad[4],
                                               const lv_ambiq_scale_rect_t * clip,
                                               lv_ambiq_scale_point_t * output)
{
    if(clip->x1 > clip->x2 || clip->y1 > clip->y2) return 0;
    lv_ambiq_scale_point_t a[LV_AMBIQ_SCALE_CLIP_CAPACITY];
    lv_ambiq_scale_point_t b[LV_AMBIQ_SCALE_CLIP_CAPACITY];
    unsigned count = 4;
    for(unsigned i = 0; i < 4; ++i) a[i] = quad[i];

    for(unsigned edge = 0; edge < 4 && count > 0; ++edge) {
        const bool vertical = edge < 2;
        const bool minimum = edge == 0 || edge == 2;
        const float boundary = edge == 0 ? clip->x1 : edge == 1 ? clip->x2 :
                               edge == 2 ? clip->y1 : clip->y2;
        unsigned next_count = 0;
        lv_ambiq_scale_point_t previous = a[count - 1];
        float previous_value = vertical ? previous.x : previous.y;
        bool previous_inside = minimum ? previous_value >= boundary : previous_value <= boundary;
        for(unsigned i = 0; i < count; ++i) {
            const lv_ambiq_scale_point_t current = a[i];
            const float value = vertical ? current.x : current.y;
            const bool inside = minimum ? value >= boundary : value <= boundary;
            if(inside != previous_inside) {
                const float t = (boundary - previous_value) / (value - previous_value);
                lv_ambiq_scale_point_t intersection = {
                    previous.x + t * (current.x - previous.x),
                    previous.y + t * (current.y - previous.y)
                };
                if(vertical) intersection.x = boundary;
                else intersection.y = boundary;
                if(!lv_ambiq_scale_append(b, &next_count, intersection)) return 0;
            }
            if(inside && !lv_ambiq_scale_append(b, &next_count, current)) return 0;
            previous = current;
            previous_value = value;
            previous_inside = inside;
        }
        if(next_count > 1 && lv_ambiq_scale_points_equal(b[0], b[next_count - 1])) --next_count;
        count = next_count;
        for(unsigned i = 0; i < count; ++i) a[i] = b[i];
    }
    if(count < 3) return 0;
    float twice_area = 0.0f;
    for(unsigned i = 0; i < count; ++i) {
        const unsigned next = (i + 1) % count;
        twice_area += a[i].x * a[next].y - a[next].x * a[i].y;
    }
    if(twice_area > -0.0001f && twice_area < 0.0001f) return 0;
    for(unsigned i = 0; i < count; ++i) output[i] = a[i];
    return count;
}

#endif
