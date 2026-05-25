/**
 * @file lv_bin_decoder.h
 *
 */

#ifndef LV_BIN_DECODER_H
#define LV_BIN_DECODER_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <stdint.h>
#include "../../draw/lv_image_decoder.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    uint8_t * (*get_static_data)(void);
    uint8_t * (*find_dynamic_data)(uint32_t hash);
    void (*move_dynamic_to_head)(uint32_t hash);
} lv_bin_decoder_psram_resource_provider_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Initialize the binary image decoder module
 */
void lv_bin_decoder_init(void);

/**
 * Register an optional PSRAM resource provider for platform-specific image sources.
 * Passing NULL disables PSRAM resource decoding while keeping other bin image sources available.
 */
void lv_bin_decoder_set_psram_resource_provider(const lv_bin_decoder_psram_resource_provider_t * provider);

/**
 * Get PSRAM data pointer for binary image decoder (weak, can be overridden)
 * @return pointer to PSRAM data, or NULL if not available
 */
// const uint8_t * lv_bin_decoder_psram_data(void) __attribute__((weak));

/**
 * Get info about a lvgl binary image
 * @param decoder the decoder where this function belongs
 * @param dsc image descriptor containing the source and type of the image and other info.
 * @param header store the image data here
 * @return LV_RESULT_OK: the info is successfully stored in `header`; LV_RESULT_INVALID: unknown format or other error.
 */
lv_result_t lv_bin_decoder_info(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc, lv_image_header_t * header);

lv_result_t lv_bin_decoder_get_area(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc,
                                    const lv_area_t * full_area, lv_area_t * decoded_area);

/**
 * Open a lvgl binary image
 * @param decoder the decoder where this function belongs
 * @param dsc pointer to decoder descriptor. `src`, `style` are already initialized in it.
 * @return LV_RESULT_OK: the info is successfully stored in `header`; LV_RESULT_INVALID: unknown format or other error.
 */
lv_result_t lv_bin_decoder_open(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc);

/**
 * Close the pending decoding. Free resources etc.
 * @param decoder pointer to the decoder the function associated with
 * @param dsc pointer to decoder descriptor
 */
void lv_bin_decoder_close(lv_image_decoder_t * decoder, lv_image_decoder_dsc_t * dsc);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_BIN_DECODER_H*/
