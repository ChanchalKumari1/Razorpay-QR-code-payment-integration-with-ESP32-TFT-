#ifndef TFT_DISPLAY_H
#define TFT_DISPLAY_H

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_ili9341.h"  
#include "pngle.h" 

#define TFT_WIDTH    320
#define TFT_HEIGHT   240
#define TFT_HOR_RES  TFT_WIDTH
#define TFT_VER_RES  TFT_HEIGHT

#define TFT_PIN_MOSI    11  // MOSI
#define TFT_PIN_SCLK    12  // SCK  
#define TFT_PIN_CS      10  // CS
#define TFT_PIN_DC      9   // DC
#define TFT_PIN_RST     4   // RST
#define TFT_PIN_BL      21  // LED (backlight)

// Color definitions
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_YELLOW    0xFFE0
#define COLOR_ORANGE    0xFC00

extern esp_lcd_panel_handle_t panel_handle;

esp_err_t tft_init(void);
void tft_fill_screen(uint16_t color);
void tft_draw_image(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *data);
void tft_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void tft_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void tft_draw_text(const char* text, uint16_t x, uint16_t y, uint16_t color, uint16_t bg_color, uint8_t size);

#endif