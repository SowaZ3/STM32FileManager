#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define DISPLAY_WIDTH   320U
#define DISPLAY_HEIGHT  240U

#define DISPLAY_COLOR_BLACK       0x0000U
#define DISPLAY_COLOR_WHITE       0xFFFFU
#define DISPLAY_COLOR_LIGHT_GREY  0xD69AU
#define DISPLAY_COLOR_LINE_GREY   0xC618U
#define DISPLAY_COLOR_DARK_GREY   0x8410U
#define DISPLAY_COLOR_BLUE        0x2D7FU
#define DISPLAY_COLOR_RED         0xE8E4U
#define DISPLAY_COLOR_GREEN       0x4E69U

typedef struct
{
  uint16_t x;
  uint16_t y;
  uint8_t pressed;
} DisplayTouchPoint;

void DisplayDriver_Init(void);
void DisplayDriver_FillScreen(uint16_t color);
void DisplayDriver_FillRect(uint16_t x, uint16_t y, uint16_t width,
                            uint16_t height, uint16_t color);
void DisplayDriver_DrawRect(uint16_t x, uint16_t y, uint16_t width,
                            uint16_t height, uint16_t color);
void DisplayDriver_DrawText(uint16_t x, uint16_t y, const char *text,
                            uint16_t color, uint16_t background,
                            uint8_t scale);
uint8_t DisplayDriver_ReadTouch(DisplayTouchPoint *point);

#ifdef __cplusplus
}
#endif

#endif
