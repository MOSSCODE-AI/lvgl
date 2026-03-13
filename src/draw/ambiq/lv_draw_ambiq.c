//*****************************************************************************
//
// Copyright (c) 2025, Ambiq Micro, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
//
// Third party software included in this distribution is subject to the
// additional license terms as defined in the /docs/licenses directory.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//*****************************************************************************

/**
 * @file lv_draw_ambiq.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "../lv_draw_private.h"
#if LV_USE_DRAW_AMBIQ

#include "../../core/lv_refr.h"
#include "lv_draw_ambiq.h"
#include "lv_draw_ambiq_private.h"
#include "../lv_draw_image_private.h"
#include "../../display/lv_display_private.h"
#include "../../stdlib/lv_string.h"
#include "../../core/lv_global.h"
#include "platform/bsp/debug.h"
#include "oswrapper/kernel.hpp"

/*********************
 *      DEFINES
 *********************/
#define DRAW_UNIT_ID_AMBIQ     9
#ifndef LV_AMBIQ_DRAW_DIAG_ENABLE
    #define LV_AMBIQ_DRAW_DIAG_ENABLE            0
#endif
#ifndef LV_AMBIQ_DRAW_DIAG_DETAIL_FRAMES
    #define LV_AMBIQ_DRAW_DIAG_DETAIL_FRAMES     4U
#endif
#ifndef LV_AMBIQ_DRAW_DIAG_DETAIL_LIMIT
    #define LV_AMBIQ_DRAW_DIAG_DETAIL_LIMIT      8U
#endif
#ifndef LV_AMBIQ_DRAW_DIAG_SLOW_TASK_MS
    #define LV_AMBIQ_DRAW_DIAG_SLOW_TASK_MS      20U
#endif
#ifndef LV_AMBIQ_DRAW_DIAG_SUMMARY_MIN_MS
    #define LV_AMBIQ_DRAW_DIAG_SUMMARY_MIN_MS    100U
#endif
#ifndef LV_AMBIQ_DRAW_DIAG_TYPE_BUCKETS
    #define LV_AMBIQ_DRAW_DIAG_TYPE_BUCKETS      16U
#endif


#if  LV_AMBIQ_CPU_GPU_ASYNC && LV_USE_DRAW_SW
    #error "We cannot use CPU_GPU_ASYNC mode alone with software render engine!"
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
#if LV_USE_OS
    static void render_thread_cb(void * ptr);
#endif

static void execute_drawing(lv_draw_task_t * t);

static int32_t dispatch(lv_draw_unit_t * draw_unit, lv_layer_t * layer);
static int32_t evaluate(lv_draw_unit_t * draw_unit, lv_draw_task_t * task);
static int32_t lv_draw_ambiq_delete(lv_draw_unit_t * draw_unit);

/**********************
 *  STATIC VARIABLES
 **********************/
#define _draw_info LV_GLOBAL_DEFAULT()->draw_info
static lv_draw_ambiq_unit_t * draw_ambiq_unit = NULL;
static bool nema_backend_initialized = false;

#if LV_AMBIQ_DRAW_DIAG_ENABLE
typedef struct {
    bool active;
    uint32_t frame_seq;
    uint32_t task_count;
    uint32_t total_task_ms;
    uint32_t max_task_ms;
    uint32_t detail_logs_emitted;
    lv_draw_task_type_t max_task_type;
    uint32_t type_count[LV_AMBIQ_DRAW_DIAG_TYPE_BUCKETS];
    uint32_t type_time_ms[LV_AMBIQ_DRAW_DIAG_TYPE_BUCKETS];
} lv_ambiq_draw_diag_state_t;

static lv_ambiq_draw_diag_state_t s_draw_diag;

#define LV_AMBIQ_DRAW_LOG(fmt, ...) \
    PLATFORM_LOG_CHANNEL("LV_DISPLAY", "[LV_AMBIQ_DRAW] " fmt, ##__VA_ARGS__)

static uint32_t lv_ambiq_draw_diag_type_index(lv_draw_task_type_t type)
{
    return ((uint32_t)type < LV_AMBIQ_DRAW_DIAG_TYPE_BUCKETS) ? (uint32_t)type : 0U;
}

static const char * lv_ambiq_draw_diag_type_name(lv_draw_task_type_t type)
{
    switch(type) {
        case LV_DRAW_TASK_TYPE_FILL:
            return "FILL";
        case LV_DRAW_TASK_TYPE_BORDER:
            return "BORDER";
        case LV_DRAW_TASK_TYPE_BOX_SHADOW:
            return "BOX_SHADOW";
        case LV_DRAW_TASK_TYPE_LETTER:
            return "LETTER";
        case LV_DRAW_TASK_TYPE_LABEL:
            return "LABEL";
        case LV_DRAW_TASK_TYPE_IMAGE:
            return "IMAGE";
        case LV_DRAW_TASK_TYPE_LAYER:
            return "LAYER";
        case LV_DRAW_TASK_TYPE_LINE:
            return "LINE";
        case LV_DRAW_TASK_TYPE_ARC:
            return "ARC";
        case LV_DRAW_TASK_TYPE_TRIANGLE:
            return "TRIANGLE";
        case LV_DRAW_TASK_TYPE_MASK_RECTANGLE:
            return "MASK_RECT";
        case LV_DRAW_TASK_TYPE_MASK_BITMAP:
            return "MASK_BMP";
#if LV_USE_VECTOR_GRAPHIC
        case LV_DRAW_TASK_TYPE_VECTOR:
            return "VECTOR";
#endif
#if LV_USE_3DTEXTURE
        case LV_DRAW_TASK_TYPE_3D:
            return "3D";
#endif
        case LV_DRAW_TASK_TYPE_NONE:
        default:
            return "UNKNOWN";
    }
}

static void lv_ambiq_draw_diag_log_task(const lv_draw_task_t * t, uint32_t task_ms, bool transformed)
{
    int32_t w = lv_area_get_width(&t->area);
    int32_t h = lv_area_get_height(&t->area);

    if(t->type == LV_DRAW_TASK_TYPE_IMAGE || t->type == LV_DRAW_TASK_TYPE_LAYER) {
        const lv_draw_image_dsc_t * draw_dsc = t->draw_dsc;
        LV_AMBIQ_DRAW_LOG(
            "frame#%lu task=%s dur=%lums size=%ldx%ld area=(%d,%d)-(%d,%d) cf=%u rot=%ld scale=%ld/%ld transformed=%u\r\n",
            (unsigned long)s_draw_diag.frame_seq,
            lv_ambiq_draw_diag_type_name(t->type),
            (unsigned long)task_ms,
            (long)w,
            (long)h,
            (int)t->area.x1,
            (int)t->area.y1,
            (int)t->area.x2,
            (int)t->area.y2,
            (unsigned int)draw_dsc->header.cf,
            (long)draw_dsc->rotation,
            (long)draw_dsc->scale_x,
            (long)draw_dsc->scale_y,
            transformed ? 1U : 0U);
        return;
    }

    if(t->type == LV_DRAW_TASK_TYPE_LABEL) {
        const lv_draw_label_dsc_t * draw_dsc = t->draw_dsc;
        uint32_t text_length = draw_dsc->text_length;
        if(text_length == 0U && draw_dsc->text != NULL) {
            text_length = lv_strlen(draw_dsc->text);
        }

        LV_AMBIQ_DRAW_LOG(
            "frame#%lu task=%s dur=%lums size=%ldx%ld area=(%d,%d)-(%d,%d) textLen=%lu rot=%ld\r\n",
            (unsigned long)s_draw_diag.frame_seq,
            lv_ambiq_draw_diag_type_name(t->type),
            (unsigned long)task_ms,
            (long)w,
            (long)h,
            (int)t->area.x1,
            (int)t->area.y1,
            (int)t->area.x2,
            (int)t->area.y2,
            (unsigned long)text_length,
            (long)draw_dsc->rotation);
        return;
    }

    if(t->type == LV_DRAW_TASK_TYPE_LETTER) {
        const lv_draw_letter_dsc_t * draw_dsc = t->draw_dsc;
        LV_AMBIQ_DRAW_LOG(
            "frame#%lu task=%s dur=%lums size=%ldx%ld area=(%d,%d)-(%d,%d) unicode=%lu rot=%ld scale=%ld/%ld\r\n",
            (unsigned long)s_draw_diag.frame_seq,
            lv_ambiq_draw_diag_type_name(t->type),
            (unsigned long)task_ms,
            (long)w,
            (long)h,
            (int)t->area.x1,
            (int)t->area.y1,
            (int)t->area.x2,
            (int)t->area.y2,
            (unsigned long)draw_dsc->unicode,
            (long)draw_dsc->rotation,
            (long)draw_dsc->scale_x,
            (long)draw_dsc->scale_y);
        return;
    }

    LV_AMBIQ_DRAW_LOG(
        "frame#%lu task=%s dur=%lums size=%ldx%ld area=(%d,%d)-(%d,%d)\r\n",
        (unsigned long)s_draw_diag.frame_seq,
        lv_ambiq_draw_diag_type_name(t->type),
        (unsigned long)task_ms,
        (long)w,
        (long)h,
        (int)t->area.x1,
        (int)t->area.y1,
        (int)t->area.x2,
        (int)t->area.y2);
}

void lv_draw_ambiq_diag_begin_frame(uint32_t frame_seq)
{
    lv_memset(&s_draw_diag, 0, sizeof(s_draw_diag));
    s_draw_diag.active = true;
    s_draw_diag.frame_seq = frame_seq;
}

void lv_draw_ambiq_diag_end_frame(uint32_t handler_ms)
{
    uint32_t i;
    bool should_log;

    if(!s_draw_diag.active) {
        return;
    }

    s_draw_diag.active = false;

    should_log = (handler_ms >= LV_AMBIQ_DRAW_DIAG_SUMMARY_MIN_MS) ||
                 (s_draw_diag.frame_seq <= LV_AMBIQ_DRAW_DIAG_DETAIL_FRAMES);
    if(!should_log) {
        return;
    }

    if(s_draw_diag.task_count == 0U) {
        LV_AMBIQ_DRAW_LOG(
            "frame#%lu handler=%lums ambiqTasks=0 unaccounted=%lums\r\n",
            (unsigned long)s_draw_diag.frame_seq,
            (unsigned long)handler_ms,
            (unsigned long)handler_ms);
        return;
    }

    LV_AMBIQ_DRAW_LOG(
        "frame#%lu handler=%lums ambiqTasks=%lu ambiqTotal=%lums maxTask=%lums(%s) unaccounted=%lums\r\n",
        (unsigned long)s_draw_diag.frame_seq,
        (unsigned long)handler_ms,
        (unsigned long)s_draw_diag.task_count,
        (unsigned long)s_draw_diag.total_task_ms,
        (unsigned long)s_draw_diag.max_task_ms,
        lv_ambiq_draw_diag_type_name(s_draw_diag.max_task_type),
        (unsigned long)((handler_ms > s_draw_diag.total_task_ms) ?
                        (handler_ms - s_draw_diag.total_task_ms) : 0U));

    for(i = 0U; i < LV_AMBIQ_DRAW_DIAG_TYPE_BUCKETS; i++) {
        if(s_draw_diag.type_count[i] == 0U) {
            continue;
        }

        LV_AMBIQ_DRAW_LOG(
            "frame#%lu type=%s count=%lu total=%lums avg=%lums\r\n",
            (unsigned long)s_draw_diag.frame_seq,
            lv_ambiq_draw_diag_type_name((lv_draw_task_type_t)i),
            (unsigned long)s_draw_diag.type_count[i],
            (unsigned long)s_draw_diag.type_time_ms[i],
            (unsigned long)(s_draw_diag.type_time_ms[i] / s_draw_diag.type_count[i]));
    }
}
#else
#define LV_AMBIQ_DRAW_LOG(fmt, ...) ((void)0)
void lv_draw_ambiq_diag_begin_frame(uint32_t frame_seq)
{
    LV_UNUSED(frame_seq);
}

void lv_draw_ambiq_diag_end_frame(uint32_t handler_ms)
{
    LV_UNUSED(handler_ms);
}
#endif


/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_draw_ambiq_init(void)
{
    uint32_t hal_ret = AM_HAL_STATUS_SUCCESS;
    bool first_nema_init = !nema_backend_initialized;

    /* The upstream port assumes a prior SDK init path has already set its
     * internal ring-buffer sentinel. In this project LVGL is the first Nema
     * bring-up site, so keep an explicit local init flag instead. */
    if(first_nema_init) {
#if !defined(AM_PART_APOLLO510L)
        am_hal_pwrctrl_gpu_mode_e current_mode = AM_HAL_PWRCTRL_GPU_MODE_LOW_POWER;

        /* Match Ambiq's reference bring-up more closely: select HP GPU mode
         * before the first nema_init() so we can rule out an underclocked GPU
         * path before swapping larger chunks of the porting layer. */
        (void)am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_GFX);
        hal_ret = am_hal_pwrctrl_gpu_mode_select(AM_HAL_PWRCTRL_GPU_MODE_HIGH_PERFORMANCE);
        (void)am_hal_pwrctrl_gpu_mode_status(&current_mode);
        if((hal_ret != AM_HAL_STATUS_SUCCESS) ||
           (current_mode != AM_HAL_PWRCTRL_GPU_MODE_HIGH_PERFORMANCE)) {
            LV_AMBIQ_DRAW_LOG("gpu hp mode select status=%lu mode=%u\r\n",
                              (unsigned long)hal_ret,
                              (unsigned int)current_mode);
        }
#endif

        hal_ret = nemagfx_power_control(AM_HAL_SYSCTRL_WAKE, false);
        if(hal_ret != AM_HAL_STATUS_SUCCESS) {
            LV_LOG_ERROR("Power control failed: %" LV_PRIu32 "\r\n", hal_ret);
        }

        /* Initialize the NemaGFX (raster graphics) SDK. */
        nema_init();
        if(NEMA_ERR_NO_ERROR != nema_get_error()) {
            LV_LOG_ERROR("NemaGFX initialization failed!");
        }

#if LV_USE_AMBIQ_VG
        /* Initialize the NemaVG (vector graphics) SDK. */
        nema_buffer_t stencil_buffer = {.base_phys = 0, .base_virt = 0, .size = 0, .fd = 0};
        nema_vg_init_stencil_prealloc(0, 0, stencil_buffer);
        if(NEMA_VG_ERR_NO_ERROR != nema_vg_get_error()) {
            LV_LOG_ERROR("NemaVG initialization failed!");
        }
#endif

        nema_backend_initialized = true;
    }

#if LV_AMBIQ_GPU_POWER_SAVE
    hal_ret = nemagfx_power_control(AM_HAL_SYSCTRL_DEEPSLEEP, true);
    if(hal_ret != AM_HAL_STATUS_SUCCESS) {
        LV_LOG_ERROR("Power control failed: %" LV_PRIu32 "\r\n", hal_ret);
    }
#else
    /* The first WAKE+init path has already brought Nema up. Calling WAKE with
     * retained-state recovery here can force an early nema_reinit() on this
     * project's bring-up path and fault before the first frame. */
    hal_ret = nemagfx_power_control(AM_HAL_SYSCTRL_WAKE, first_nema_init ? false : true);
    if(hal_ret != AM_HAL_STATUS_SUCCESS) {
        LV_LOG_ERROR("Power control failed: %" LV_PRIu32 "\r\n", hal_ret);
    }
#endif

    lv_draw_ambiq_init_buf_handlers();

    draw_ambiq_unit = lv_draw_create_unit(sizeof(lv_draw_ambiq_unit_t));
    draw_ambiq_unit->base_unit.dispatch_cb = dispatch;
    draw_ambiq_unit->base_unit.evaluate_cb = evaluate;
    draw_ambiq_unit->base_unit.delete_cb = LV_USE_OS ? lv_draw_ambiq_delete : NULL;
    draw_ambiq_unit->base_unit.name = "AMBIQ";
    draw_ambiq_unit->small_texture_buffer  = lv_draw_buf_create(64, 1, LV_COLOR_FORMAT_ARGB8888, 0);
    draw_ambiq_unit->stencil_buffer = NULL;
#if LV_USE_AMBIQ_VG
    draw_ambiq_unit->vg_path = nema_vg_path_create();
    draw_ambiq_unit->vg_paint = nema_vg_paint_create();
    draw_ambiq_unit->vg_grad = nema_vg_grad_create();
#endif

    draw_ambiq_unit->blend_mode = 0;
    draw_ambiq_unit->dst_tex = NEMA_NOTEX;
    draw_ambiq_unit->fg_tex = NEMA_NOTEX;
    draw_ambiq_unit->bg_tex = NEMA_NOTEX;

    lv_memset(&draw_ambiq_unit->des_buffer, 0, sizeof(lv_draw_buf_t));
    lv_memset(&draw_ambiq_unit->clip_area, 0, sizeof(lv_area_t));

    draw_ambiq_unit->cl = nema_cl_create_sized(LV_AMBIQ_COMMAND_LIST_SECTOR * LV_AMBIQ_COMMAND_LIST_SECTOR_SIZE);
    LV_ASSERT_NULL(draw_ambiq_unit->cl.bo.base_virt);

    //    lv_ll_init(&draw_ambiq_unit->inserted_cl_ll, sizeof(nema_cmdlist_t));

    draw_ambiq_unit->nema_context_lock_count = 0;

#ifdef LV_USE_AMBIQ_VG
    lv_draw_ambiq_vector_font_init((lv_draw_unit_t *)draw_ambiq_unit);
#endif

#if LV_USE_OS
    lv_mutex_init(&draw_ambiq_unit->mutex_nema_context);
    lv_thread_init(&draw_ambiq_unit->thread, "ambiqdraw", LV_DRAW_THREAD_PRIO, render_thread_cb, LV_DRAW_THREAD_STACK_SIZE,
                   draw_ambiq_unit);
#endif
}

void lv_draw_ambiq_deinit(void)
{
    // lv_result_t ret;
    // ret = lv_ambiq_nema_gpu_check_busy_and_suspend();
    // if(ret == LV_RESULT_OK)
    // {
    //     LV_LOG_ERROR("GPU is still busy, cannot poweroff now!\n");
    //     return ;
    // }

#if LV_USE_AMBIQ_VG
    //This will release the internal buffer in NemaVG.
    nema_vg_deinit();
#endif

    // Call low level API to release global ring buffer
    nema_backend_initialized = false;
}

static int32_t lv_draw_ambiq_delete(lv_draw_unit_t * draw_unit)
{
    lv_draw_ambiq_unit_t * unit = (lv_draw_ambiq_unit_t *) draw_unit;

    lv_draw_buf_destroy(unit->small_texture_buffer);

    if(unit->stencil_buffer) {
        lv_draw_buf_destroy(unit->stencil_buffer);
    }

#if LV_USE_VECTOR_GRAPHIC
    //Release VG path
    nema_vg_path_destroy(unit->vg_path);

    //Release VG paint
    nema_vg_paint_destroy(unit->vg_paint);

    //Release VG gradient
    nema_vg_grad_destroy(unit->vg_grad);
#endif

    nema_cl_destroy(&unit->cl);

#if LV_USE_OS
    LV_LOG_INFO("cancel Ambiq GPU rendering thread");
    unit->exit_status = true;

    if(unit->inited) {
        lv_thread_sync_signal(&unit->sync);
    }

    return lv_thread_delete(&unit->thread);
#else
    LV_UNUSED(draw_unit);
    return 0;
#endif
}



/**********************
 *   STATIC FUNCTIONS
 **********************/
static inline void execute_drawing_unit(lv_draw_task_t * t)
{
    execute_drawing(t);

    lv_draw_ambiq_unit_t * unit = (lv_draw_ambiq_unit_t *)t->draw_unit;

    t->state = LV_DRAW_TASK_STATE_FINISHED;
    unit->task_act = NULL;

    /*The draw unit is free now. Request a new dispatching as it can get a new task*/
    lv_draw_dispatch_request();
}

static int32_t evaluate(lv_draw_unit_t * draw_unit, lv_draw_task_t * task)
{
    LV_UNUSED(draw_unit);

    switch(task->type) {
        case LV_DRAW_TASK_TYPE_FILL: {
            lv_draw_fill_dsc_t * draw_dsc_fill = task->draw_dsc;

            if(draw_dsc_fill->grad.dir == LV_GRAD_DIR_RADIAL ||
               draw_dsc_fill->grad.dir == LV_GRAD_DIR_CONICAL ||
               draw_dsc_fill->grad.dir == LV_GRAD_DIR_LINEAR) {
                /**
                 * Note: The underlying system supports these features; however,
                 * their full implementation and support are currently under development.
                 */
                return 0;
            }

            task->preference_score = 10;
            task->preferred_draw_unit_id = DRAW_UNIT_ID_AMBIQ;
            break;
        }
        case LV_DRAW_TASK_TYPE_BORDER:
            task->preference_score = 10;
            task->preferred_draw_unit_id = DRAW_UNIT_ID_AMBIQ;
            break;
        case LV_DRAW_TASK_TYPE_BOX_SHADOW:
            task->preference_score = 10;
            task->preferred_draw_unit_id = DRAW_UNIT_ID_AMBIQ;
            break;
        case LV_DRAW_TASK_TYPE_TRIANGLE: {
            //return 0;
            lv_draw_triangle_dsc_t * draw_dsc_tri = task->draw_dsc;

            if(draw_dsc_tri->grad.dir == LV_GRAD_DIR_RADIAL ||
               draw_dsc_tri->grad.dir == LV_GRAD_DIR_CONICAL ||
               draw_dsc_tri->grad.dir == LV_GRAD_DIR_LINEAR) {
                /**
                 * Note: The underlying system supports these features; however,
                 * their full implementation and support are currently under development.
                 */
                return 0;
            }
            task->preference_score = 10;
            task->preferred_draw_unit_id = DRAW_UNIT_ID_AMBIQ;
            break;
        }
        case LV_DRAW_TASK_TYPE_LINE:
            task->preference_score = 10;
            task->preferred_draw_unit_id = DRAW_UNIT_ID_AMBIQ;
            break;

        case LV_DRAW_TASK_TYPE_ARC:
            task->preference_score = 10;
            task->preferred_draw_unit_id = DRAW_UNIT_ID_AMBIQ;
            break;

        case LV_DRAW_TASK_TYPE_LAYER:
            task->preference_score = 10;
            task->preferred_draw_unit_id = DRAW_UNIT_ID_AMBIQ;
            break;

        case LV_DRAW_TASK_TYPE_IMAGE: {
            lv_draw_image_dsc_t * draw_dsc_image = task->draw_dsc;

            /* Indexed images, especially I1 canvas content used by QR codes,
             * are more reliable through the software draw unit on Apollo510.
             * Let SW handle them instead of forcing the Ambiq image path. */
            if(LV_COLOR_FORMAT_IS_INDEXED(draw_dsc_image->header.cf)) {
                platform_log_println("[GPU_DIAG] ambiq skip indexed image cf=%u -> SW",
                                     (unsigned int)draw_dsc_image->header.cf);
                return 0;
            }

            nema_tex_format_t nema_cf = lv_ambiq_color_format_map_src(draw_dsc_image->header.cf);
            if(nema_cf == COLOR_FORMAT_INVALID) {
                return 0;
            }

            //Set blend mode
            if((draw_dsc_image->blend_mode == LV_BLEND_MODE_SUBTRACTIVE) ||
               (draw_dsc_image->blend_mode == LV_BLEND_MODE_MULTIPLY)) {
                return 0;
            }

            task->preference_score = 10;
            task->preferred_draw_unit_id = DRAW_UNIT_ID_AMBIQ;
            break;
        }

        case LV_DRAW_TASK_TYPE_LABEL:
            task->preference_score = 10;
            task->preferred_draw_unit_id = DRAW_UNIT_ID_AMBIQ;
            break;

        case LV_DRAW_TASK_TYPE_LETTER:
            task->preference_score = 10;
            task->preferred_draw_unit_id = DRAW_UNIT_ID_AMBIQ;
            break;

        case LV_DRAW_TASK_TYPE_MASK_RECTANGLE:
            task->preference_score = 10;
            task->preferred_draw_unit_id = DRAW_UNIT_ID_AMBIQ;
            break;

#if LV_USE_VECTOR_GRAPHIC
        case LV_DRAW_TASK_TYPE_VECTOR:
#if LV_USE_AMBIQ_VG
            task->preference_score = 10;
            task->preferred_draw_unit_id = DRAW_UNIT_ID_AMBIQ;
#else
            return 0;
#endif
            break;
#endif

        default:
            break;
    }

    return 0;
}

static int32_t dispatch(lv_draw_unit_t * draw_unit, lv_layer_t * layer)
{
    LV_PROFILER_DRAW_BEGIN;
    lv_draw_ambiq_unit_t * unit = (lv_draw_ambiq_unit_t *) draw_unit;

    /*Return immediately if it's busy with draw task*/
    if(unit->task_act) {
        LV_PROFILER_DRAW_END;
        return 0;
    }

    lv_draw_task_t * t = NULL;
    t = lv_draw_get_available_task(layer, NULL, DRAW_UNIT_ID_AMBIQ);
    if(t == NULL) {
        LV_PROFILER_DRAW_END;
        return LV_DRAW_UNIT_IDLE;
    }

    void * buf = lv_draw_layer_alloc_buf(layer);
    if(buf == NULL) {
        LV_PROFILER_DRAW_END;
        return LV_DRAW_UNIT_IDLE;
    }

    t->state = LV_DRAW_TASK_STATE_IN_PROGRESS;
    t->draw_unit = (lv_draw_unit_t *)draw_unit;
    unit->task_act = t;

#if LV_USE_OS
    /*Let the render thread work*/
    if(unit->inited) lv_thread_sync_signal(&unit->sync);
#else
    execute_drawing_unit(t);
#endif
    LV_PROFILER_DRAW_END;
    return 1;
}

#if LV_USE_OS
static void render_thread_cb(void * ptr)
{
    lv_draw_ambiq_unit_t * u = ptr;

    lv_thread_sync_init(&u->sync);
    u->inited = true;

    while(1) {
        while(u->task_act == NULL && !u->exit_status) {
            lv_thread_sync_wait(&u->sync);
        }

        if(u->exit_status) {
            LV_LOG_INFO("ready to exit software rendering thread");
            break;
        }

        execute_drawing_unit(u->task_act);
    }

    u->inited = false;
    lv_thread_sync_delete(&u->sync);
    lv_mutex_delete(&u->mutex_nema_context);
    LV_LOG_INFO("exit software rendering thread");
}
#endif

static void execute_drawing(lv_draw_task_t * t)
{
    LV_PROFILER_DRAW_BEGIN;
    bool transformed = false;
#if LV_AMBIQ_DRAW_DIAG_ENABLE
    uint32_t task_start_ms = s_draw_diag.active ? kernelGetUptimeMs() : 0U;
    uint32_t phase_flush_ms = 0U;
    uint32_t phase_start_ms = 0U;
    uint32_t phase_draw_ms = 0U;
    uint32_t phase_end_ms = 0U;
    uint32_t phase_invalidate_ms = 0U;
    uint32_t phase_ts = task_start_ms;
#endif

    /*Render the draw task*/
    lv_layer_t * layer = t->target_layer;
    lv_draw_buf_t * draw_buf = layer->draw_buf;

    lv_area_t clip_area;
    lv_area_copy(&clip_area, &t->clip_area);
    lv_area_move(&clip_area, -layer->buf_area.x1, -layer->buf_area.y1);

    lv_area_t draw_area;
    lv_area_copy(&draw_area, &t->area);

    if(t->type == LV_DRAW_TASK_TYPE_IMAGE) {
        lv_draw_image_dsc_t * draw_dsc = t->draw_dsc;

        transformed = draw_dsc->rotation != 0 || draw_dsc->scale_x != LV_SCALE_NONE ||
                      draw_dsc->scale_y != LV_SCALE_NONE || draw_dsc->skew_y != 0 || draw_dsc->skew_x != 0 ? true : false;


        if(transformed) {
            int32_t w = lv_area_get_width(&draw_area);
            int32_t h = lv_area_get_height(&draw_area);

            lv_image_buf_get_transformed_area(&draw_area, w, h,
                                              draw_dsc->rotation,
                                              draw_dsc->scale_x, draw_dsc->scale_y,
                                              &draw_dsc->pivot);

            draw_area.x1 += t->area.x1;
            draw_area.y1 += t->area.y1;
            draw_area.x2 += t->area.x1;
            draw_area.y2 += t->area.y1;
        }
    }

    lv_area_move(&draw_area, -layer->buf_area.x1, -layer->buf_area.y1);

    if(!lv_area_intersect(&draw_area, &draw_area, &clip_area))
        return; /*Fully clipped, nothing to do*/

    /* If GPU and CPU work in async mode, software rendering pipeline will not be used,
    draw buffer will only be accessed by GPU, no cache flush is needed. */
#if LV_AMBIQ_CPU_GPU_ASYNC==0
    /* Flush the drawing area */
    lv_draw_buf_flush_cache(draw_buf, &draw_area);
#if LV_AMBIQ_DRAW_DIAG_ENABLE
    if(s_draw_diag.active) {
        uint32_t now_ms = kernelGetUptimeMs();
        phase_flush_ms = now_ms - phase_ts;
        phase_ts = now_ms;
    }
#endif
#endif

    lv_result_t ret = lv_draw_ambiq_common_start(draw_buf, &clip_area, false);
#if LV_AMBIQ_DRAW_DIAG_ENABLE
    if(s_draw_diag.active) {
        uint32_t now_ms = kernelGetUptimeMs();
        phase_start_ms = now_ms - phase_ts;
        phase_ts = now_ms;
    }
#endif
    if(ret != LV_RESULT_OK) {
        return;
    }

    switch(t->type) {
        case LV_DRAW_TASK_TYPE_FILL:
            lv_draw_ambiq_fill(t, t->draw_dsc, &t->area);
            break;
        case LV_DRAW_TASK_TYPE_BORDER:
            lv_draw_ambiq_border(t, t->draw_dsc, &t->area);
            break;
        case LV_DRAW_TASK_TYPE_TRIANGLE:
            lv_draw_ambiq_triangle(t, t->draw_dsc);
            break;
        case LV_DRAW_TASK_TYPE_LINE:
            lv_draw_ambiq_line(t, t->draw_dsc);
            break;
        case LV_DRAW_TASK_TYPE_ARC:
            lv_draw_ambiq_arc(t, t->draw_dsc, &t->area);
            break;
        case LV_DRAW_TASK_TYPE_IMAGE:
            lv_draw_ambiq_image(t, t->draw_dsc, &t->area);
            break;
        case LV_DRAW_TASK_TYPE_LABEL:
            lv_draw_ambiq_vg_start(draw_buf->header.w, draw_buf->header.h);
            lv_draw_ambiq_label(t, t->draw_dsc, &t->area);
            break;
        case LV_DRAW_TASK_TYPE_LETTER:
            lv_draw_ambiq_vg_start(draw_buf->header.w, draw_buf->header.h);
            lv_draw_ambiq_letter(t, t->draw_dsc, &t->area);
            break;
        case LV_DRAW_TASK_TYPE_BOX_SHADOW:
            lv_draw_ambiq_box_shadow(t, t->draw_dsc, &t->area);
            break;
        case LV_DRAW_TASK_TYPE_MASK_RECTANGLE:
            lv_draw_ambiq_mask_rect(t, t->draw_dsc, &t->area);
            break;
        case LV_DRAW_TASK_TYPE_LAYER:
            lv_draw_ambiq_layer(t, t->draw_dsc, &t->area);
            break;

#if LV_USE_VECTOR_GRAPHIC
        case LV_DRAW_TASK_TYPE_VECTOR:
#if LV_USE_AMBIQ_VG
            lv_draw_ambiq_vg_start(draw_buf->header.w, draw_buf->header.h);
            lv_draw_ambiq_vector(t, t->draw_dsc);
#endif
            break;
#endif

        default:
            break;
    }

#if LV_AMBIQ_DRAW_DIAG_ENABLE
    if(s_draw_diag.active) {
        uint32_t now_ms = kernelGetUptimeMs();
        phase_draw_ms = now_ms - phase_ts;
        phase_ts = now_ms;
    }
#endif

#if LV_AMBIQ_CPU_GPU_ASYNC
    lv_draw_ambiq_common_end(false);
#else
    lv_draw_ambiq_common_end(true);
#if LV_AMBIQ_DRAW_DIAG_ENABLE
    if(s_draw_diag.active) {
        uint32_t now_ms = kernelGetUptimeMs();
        phase_end_ms = now_ms - phase_ts;
        phase_ts = now_ms;
    }
#endif
    lv_draw_buf_invalidate_cache(draw_buf, &draw_area);
#if LV_AMBIQ_DRAW_DIAG_ENABLE
    if(s_draw_diag.active) {
        uint32_t now_ms = kernelGetUptimeMs();
        phase_invalidate_ms = now_ms - phase_ts;
        phase_ts = now_ms;
    }
#endif
#endif

#if LV_AMBIQ_DRAW_DIAG_ENABLE
    if(s_draw_diag.active) {
        uint32_t task_ms = kernelGetUptimeMs() - task_start_ms;
        uint32_t type_index = lv_ambiq_draw_diag_type_index(t->type);
        bool should_log_task = false;

        s_draw_diag.task_count++;
        s_draw_diag.total_task_ms += task_ms;
        s_draw_diag.type_count[type_index]++;
        s_draw_diag.type_time_ms[type_index] += task_ms;

        if(task_ms >= s_draw_diag.max_task_ms) {
            s_draw_diag.max_task_ms = task_ms;
            s_draw_diag.max_task_type = t->type;
        }

        should_log_task = (task_ms >= LV_AMBIQ_DRAW_DIAG_SLOW_TASK_MS);
        if(!should_log_task &&
           s_draw_diag.frame_seq <= 2U &&
           s_draw_diag.detail_logs_emitted < 3U &&
           t->type == LV_DRAW_TASK_TYPE_IMAGE) {
            should_log_task = true;
        }

        if(should_log_task &&
           s_draw_diag.detail_logs_emitted < LV_AMBIQ_DRAW_DIAG_DETAIL_LIMIT) {
            s_draw_diag.detail_logs_emitted++;
            lv_ambiq_draw_diag_log_task(t, task_ms, transformed);
            LV_AMBIQ_DRAW_LOG(
                "frame#%lu task=%s phases flush=%lums start=%lums draw=%lums end=%lums invalidate=%lums\r\n",
                (unsigned long)s_draw_diag.frame_seq,
                lv_ambiq_draw_diag_type_name(t->type),
                (unsigned long)phase_flush_ms,
                (unsigned long)phase_start_ms,
                (unsigned long)phase_draw_ms,
                (unsigned long)phase_end_ms,
                (unsigned long)phase_invalidate_ms);
        }
    }
#endif

    LV_PROFILER_DRAW_END;
}

lv_draw_ambiq_unit_t * lv_draw_ambiq_get_default_unit(void)
{
    return draw_ambiq_unit;
}

#else
void lv_draw_ambiq_diag_begin_frame(uint32_t frame_seq)
{
    LV_UNUSED(frame_seq);
}

void lv_draw_ambiq_diag_end_frame(uint32_t handler_ms)
{
    LV_UNUSED(handler_ms);
}
#endif /*LV_USE_DRAW_AMBIQ*/
