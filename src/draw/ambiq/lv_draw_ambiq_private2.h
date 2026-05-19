#ifndef LV_DRAW_AMBIQ_PRIVATE_H
#define LV_DRAW_AMBIQ_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif
/* Includes -------------------------------------------------------------------*/
#include "../../misc/lv_area.h"

/* Define ---------------------------------------------------------------------*/


/* Typedef --------------------------------------------------------------------*/

/* Extern Variables------------------------------------------------------------*/

/* Function -------------------------------------------------------------------*/
void lv_draw_ambiq_display_buffer_sync(lv_draw_buf_t * target_buffer,
                                       const lv_area_t * area,
                                       void * src, lv_color_format_t cf);

#ifdef __cplusplus
}
#endif
#endif