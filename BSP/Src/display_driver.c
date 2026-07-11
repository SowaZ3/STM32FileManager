#if defined(__GNUC__)
#pragma GCC optimize("O3")
#endif

#include "main.h"
#include "display_driver.h"

#define LCD_WIDTH   320U
#define LCD_HEIGHT  240U

#define COLOR_WHITE       0xFFFFU

/* Raw calibration is scaled from the LCDWiki Arduino touch example to 12-bit ADC. */
#define TOUCH_X_MIN  500U
#define TOUCH_X_MAX  3625U
#define TOUCH_Y_MIN  330U
#define TOUCH_Y_MAX  3575U
#define TOUCH_SCREEN_X_OFFSET 352

typedef struct
{
  GPIO_TypeDef *port;
  uint16_t pin;
} GpioPin;

typedef struct
{
  uint16_t x;
  uint16_t y;
  uint8_t pressed;
} TouchPoint;

/* LCDWiki / MCUFRIEND 2.4" Arduino 8-bit shield on NUCLEO-F303RE Arduino headers. */
static const GpioPin LCD_RD       = {GPIOA, GPIO_PIN_0};  /* A0 */
static const GpioPin LCD_WR       = {GPIOA, GPIO_PIN_1};  /* A1 */
static const GpioPin LCD_RS       = {GPIOA, GPIO_PIN_4};  /* A2 / LCD CD / touch XM */
static const GpioPin LCD_CS       = {GPIOB, GPIO_PIN_0};  /* A3 / touch YP */
static const GpioPin LCD_RST_PC1  = {GPIOC, GPIO_PIN_1};  /* A4 on default Nucleo solder bridge */
static const GpioPin LCD_RST_PB9  = {GPIOB, GPIO_PIN_9};  /* A4 on alternate Nucleo solder bridge */
static const GpioPin TOUCH_XP     = {GPIOA, GPIO_PIN_9};  /* D8 / LCD D0 */
static const GpioPin TOUCH_YM     = {GPIOC, GPIO_PIN_7};  /* D9 / LCD D1 */

/* 8-bit LCD data bus: D0..D7 of the TFT shield, not Arduino D0..D7. */
#define LCD_DATA_A_MASK  (GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10)
#define LCD_DATA_B_MASK  (GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_10)
#define LCD_DATA_C_MASK  (GPIO_PIN_7)

static ADC_HandleTypeDef hadc2;
static ADC_HandleTypeDef hadc3;

static const uint8_t font5x7[26][5] = {
  {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
  {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
  {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00},
  {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
  {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
  {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
  {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F},
  {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63},
  {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}
};

static const uint8_t digit5x7[10][5] = {
  {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
  {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
  {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
  {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
  {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}
};

static const uint8_t glyph_space[5] = {0,0,0,0,0};
static const uint8_t glyph_dot[5] = {0,0x60,0x60,0,0};
static const uint8_t glyph_dash[5] = {0x08,0x08,0x08,0x08,0x08};
static const uint8_t glyph_underscore[5] = {0x40,0x40,0x40,0x40,0x40};
static const uint8_t glyph_slash[5] = {0x20,0x10,0x08,0x04,0x02};
static const uint8_t glyph_colon[5] = {0,0x36,0x36,0,0};
static const uint8_t glyph_left[5] = {0x08,0x14,0x22,0x41,0};
static const uint8_t glyph_right[5] = {0,0x41,0x22,0x14,0x08};
static const uint8_t glyph_question[5] = {0x02,0x01,0x51,0x09,0x06};

static const uint8_t *LCD_Glyph(char ch)
{
  if ((ch >= 'a') && (ch <= 'z')) ch = (char)(ch - 'a' + 'A');
  if ((ch >= 'A') && (ch <= 'Z')) return font5x7[ch - 'A'];
  if ((ch >= '0') && (ch <= '9')) return digit5x7[ch - '0'];

  switch (ch)
  {
    case ' ': return glyph_space;
    case '.': return glyph_dot;
    case '-': return glyph_dash;
    case '_': return glyph_underscore;
    case '/': return glyph_slash;
    case ':': return glyph_colon;
    case '<': return glyph_left;
    case '>': return glyph_right;
    default: return glyph_question;
  }
}

static inline void PinHigh(GpioPin p)
{
  p.port->BSRR = p.pin;
}

static inline void PinLow(GpioPin p)
{
  p.port->BSRR = ((uint32_t)p.pin << 16);
}

static void PinOutput(GpioPin p)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = p.pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(p.port, &GPIO_InitStruct);
}

static void PinInput(GpioPin p)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = p.pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(p.port, &GPIO_InitStruct);
}

static void PinInputPullup(GpioPin p)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = p.pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(p.port, &GPIO_InitStruct);
}

static void PinAnalog(GpioPin p)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = p.pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(p.port, &GPIO_InitStruct);
}

static void LCD_DataPinsOutput(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

  GPIO_InitStruct.Pin = LCD_DATA_A_MASK;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LCD_DATA_B_MASK;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LCD_DATA_C_MASK;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

static void LCD_ControlPinsOutput(void)
{
  PinOutput(LCD_RD);
  PinOutput(LCD_WR);
  PinOutput(LCD_RS);
  PinOutput(LCD_CS);
  PinOutput(LCD_RST_PC1);
  PinOutput(LCD_RST_PB9);

  PinHigh(LCD_RD);
  PinHigh(LCD_WR);
  PinHigh(LCD_CS);
  PinHigh(LCD_RS);
  PinHigh(LCD_RST_PC1);
  PinHigh(LCD_RST_PB9);
}

static void LCD_RestoreBus(void)
{
  LCD_DataPinsOutput();
  LCD_ControlPinsOutput();
}

static inline void LCD_Write8(uint8_t value)
{
  uint32_t set_a = 0;
  uint32_t set_b = 0;
  uint32_t set_c = 0;

  if ((value & 0x01U) != 0U) set_a |= GPIO_PIN_9;
  if ((value & 0x02U) != 0U) set_c |= GPIO_PIN_7;
  if ((value & 0x04U) != 0U) set_a |= GPIO_PIN_10;
  if ((value & 0x08U) != 0U) set_b |= GPIO_PIN_3;
  if ((value & 0x10U) != 0U) set_b |= GPIO_PIN_5;
  if ((value & 0x20U) != 0U) set_b |= GPIO_PIN_4;
  if ((value & 0x40U) != 0U) set_b |= GPIO_PIN_10;
  if ((value & 0x80U) != 0U) set_a |= GPIO_PIN_8;

  GPIOA->BSRR = ((uint32_t)LCD_DATA_A_MASK << 16) | set_a;
  GPIOB->BSRR = ((uint32_t)LCD_DATA_B_MASK << 16) | set_b;
  GPIOC->BSRR = ((uint32_t)LCD_DATA_C_MASK << 16) | set_c;

  PinLow(LCD_WR);
  PinHigh(LCD_WR);
}

static void LCD_Command(uint8_t cmd)
{
  PinLow(LCD_CS);
  PinLow(LCD_RS);
  LCD_Write8(cmd);
  PinHigh(LCD_CS);
}

static void LCD_Data(uint8_t data)
{
  PinLow(LCD_CS);
  PinHigh(LCD_RS);
  LCD_Write8(data);
  PinHigh(LCD_CS);
}

static inline void LCD_Data16(uint16_t data)
{
  LCD_Write8((uint8_t)(data >> 8));
  LCD_Write8((uint8_t)data);
}

static void LCD_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
  LCD_Command(0x2A);
  LCD_Data((uint8_t)(x0 >> 8));
  LCD_Data((uint8_t)x0);
  LCD_Data((uint8_t)(x1 >> 8));
  LCD_Data((uint8_t)x1);

  LCD_Command(0x2B);
  LCD_Data((uint8_t)(y0 >> 8));
  LCD_Data((uint8_t)y0);
  LCD_Data((uint8_t)(y1 >> 8));
  LCD_Data((uint8_t)y1);

  LCD_Command(0x2C);
}

static void LCD_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  uint32_t pixels;

  if ((x >= LCD_WIDTH) || (y >= LCD_HEIGHT) || (w == 0U) || (h == 0U))
  {
    return;
  }

  if ((x + w) > LCD_WIDTH) w = LCD_WIDTH - x;
  if ((y + h) > LCD_HEIGHT) h = LCD_HEIGHT - y;

  LCD_SetAddressWindow(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
  PinLow(LCD_CS);
  PinHigh(LCD_RS);
  pixels = (uint32_t)w * h;
  while (pixels-- > 0U)
  {
    LCD_Data16(color);
  }
  PinHigh(LCD_CS);
}

static void LCD_DrawHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color)
{
  LCD_FillRect(x, y, w, 1U, color);
}

static void LCD_DrawVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color)
{
  LCD_FillRect(x, y, 1U, h, color);
}

static void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  if ((w < 2U) || (h < 2U)) return;
  LCD_DrawHLine(x, y, w, color);
  LCD_DrawHLine(x, (uint16_t)(y + h - 1U), w, color);
  LCD_DrawVLine(x, y, h, color);
  LCD_DrawVLine((uint16_t)(x + w - 1U), y, h, color);
}

static void LCD_DrawChar(char ch, uint16_t x, uint16_t y, uint16_t color, uint16_t bg, uint8_t scale)
{
  const uint8_t *glyph;
  uint8_t col;
  uint8_t row;
  uint8_t repeat_x;
  uint8_t repeat_y;
  uint16_t width;
  uint16_t height;

  glyph = LCD_Glyph(ch);
  width = (uint16_t)(6U * scale);
  height = (uint16_t)(7U * scale);
  if ((scale == 0U) || ((x + width) > LCD_WIDTH) || ((y + height) > LCD_HEIGHT))
  {
    return;
  }

  LCD_SetAddressWindow(x, y, (uint16_t)(x + width - 1U),
                       (uint16_t)(y + height - 1U));
  PinLow(LCD_CS);
  PinHigh(LCD_RS);
  for (row = 0U; row < 7U; row++)
  {
    for (repeat_y = 0U; repeat_y < scale; repeat_y++)
    {
      for (col = 0U; col < 6U; col++)
      {
        const uint16_t fill = ((col < 5U) &&
            ((glyph[col] & (1U << row)) != 0U)) ? color : bg;
        for (repeat_x = 0U; repeat_x < scale; repeat_x++)
        {
          LCD_Data16(fill);
        }
      }
    }
  }
  PinHigh(LCD_CS);
}

static void LCD_Init(void)
{
  LCD_RestoreBus();

  PinLow(LCD_RST_PC1);
  PinLow(LCD_RST_PB9);
  HAL_Delay(20);
  PinHigh(LCD_RST_PC1);
  PinHigh(LCD_RST_PB9);
  HAL_Delay(120);

  LCD_Command(0x01);
  HAL_Delay(120);

  LCD_Command(0xEF); LCD_Data(0x03); LCD_Data(0x80); LCD_Data(0x02);
  LCD_Command(0xCF); LCD_Data(0x00); LCD_Data(0xC1); LCD_Data(0x30);
  LCD_Command(0xED); LCD_Data(0x64); LCD_Data(0x03); LCD_Data(0x12); LCD_Data(0x81);
  LCD_Command(0xE8); LCD_Data(0x85); LCD_Data(0x00); LCD_Data(0x78);
  LCD_Command(0xCB); LCD_Data(0x39); LCD_Data(0x2C); LCD_Data(0x00); LCD_Data(0x34); LCD_Data(0x02);
  LCD_Command(0xF7); LCD_Data(0x20);
  LCD_Command(0xEA); LCD_Data(0x00); LCD_Data(0x00);
  LCD_Command(0xC0); LCD_Data(0x23);
  LCD_Command(0xC1); LCD_Data(0x10);
  LCD_Command(0xC5); LCD_Data(0x3E); LCD_Data(0x28);
  LCD_Command(0xC7); LCD_Data(0x86);
  LCD_Command(0x36); LCD_Data(0x28);
  LCD_Command(0x3A); LCD_Data(0x55);
  LCD_Command(0xB1); LCD_Data(0x00); LCD_Data(0x18);
  LCD_Command(0xB6); LCD_Data(0x08); LCD_Data(0x82); LCD_Data(0x27);
  LCD_Command(0xF2); LCD_Data(0x00);
  LCD_Command(0x26); LCD_Data(0x01);
  LCD_Command(0x11);
  HAL_Delay(120);
  LCD_Command(0x29);
  HAL_Delay(20);
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
  if ((hadc->Instance == ADC1) || (hadc->Instance == ADC2))
  {
    __HAL_RCC_ADC12_CLK_ENABLE();
  }
  else if ((hadc->Instance == ADC3) || (hadc->Instance == ADC4))
  {
    __HAL_RCC_ADC34_CLK_ENABLE();
  }
}

static void TouchAdc_ConfigOne(ADC_HandleTypeDef *hadc, ADC_TypeDef *instance)
{
  hadc->Instance = instance;
  hadc->Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc->Init.Resolution = ADC_RESOLUTION_12B;
  hadc->Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc->Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc->Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc->Init.LowPowerAutoWait = DISABLE;
  hadc->Init.ContinuousConvMode = DISABLE;
  hadc->Init.NbrOfConversion = 1;
  hadc->Init.DiscontinuousConvMode = DISABLE;
  hadc->Init.NbrOfDiscConversion = 1;
  hadc->Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc->Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc->Init.DMAContinuousRequests = DISABLE;
  hadc->Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;

  if (HAL_ADC_Init(hadc) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_ADCEx_Calibration_Start(hadc, ADC_SINGLE_ENDED) != HAL_OK)
  {
    Error_Handler();
  }
}

static void TouchAdc_Init(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_ADC12_CLK_ENABLE();
  __HAL_RCC_ADC34_CLK_ENABLE();

  TouchAdc_ConfigOne(&hadc2, ADC2);
  TouchAdc_ConfigOne(&hadc3, ADC3);
}

static uint16_t TouchAdc_Read(ADC_HandleTypeDef *hadc, uint32_t channel)
{
  ADC_ChannelConfTypeDef config = {0};
  uint16_t value = 0U;

  config.Channel = channel;
  config.Rank = ADC_REGULAR_RANK_1;
  config.SamplingTime = ADC_SAMPLETIME_181CYCLES_5;
  config.SingleDiff = ADC_SINGLE_ENDED;
  config.OffsetNumber = ADC_OFFSET_NONE;
  config.Offset = 0;

  if (HAL_ADC_ConfigChannel(hadc, &config) == HAL_OK)
  {
    if (HAL_ADC_Start(hadc) == HAL_OK)
    {
      if (HAL_ADC_PollForConversion(hadc, 10U) == HAL_OK)
      {
        value = (uint16_t)HAL_ADC_GetValue(hadc);
      }
      HAL_ADC_Stop(hadc);
    }
  }

  return value;
}

static uint8_t TouchPressedRaw(void)
{
  uint8_t pressed;

  PinHigh(LCD_WR);
  PinHigh(LCD_RD);

  PinOutput(LCD_CS);
  PinOutput(TOUCH_YM);
  PinLow(LCD_CS);
  PinLow(TOUCH_YM);

  PinInputPullup(TOUCH_XP);
  PinInputPullup(LCD_RS);
  HAL_Delay(1);

  pressed = (HAL_GPIO_ReadPin(TOUCH_XP.port, TOUCH_XP.pin) == GPIO_PIN_RESET) ||
            (HAL_GPIO_ReadPin(LCD_RS.port, LCD_RS.pin) == GPIO_PIN_RESET);

  LCD_RestoreBus();
  return pressed;
}

static uint16_t MapClamp(uint16_t value, uint16_t in_min, uint16_t in_max, uint16_t out_max)
{
  uint32_t result;

  if (value <= in_min) return 0U;
  if (value >= in_max) return (uint16_t)(out_max - 1U);

  result = ((uint32_t)(value - in_min) * (out_max - 1U)) / (uint32_t)(in_max - in_min);
  return (uint16_t)result;
}

static uint16_t MapTouchX(uint16_t value)
{
  int32_t scaled;
  int32_t x;

  scaled = ((int32_t)value - (int32_t)TOUCH_Y_MIN) * (int32_t)(LCD_WIDTH - 1U);
  scaled /= (int32_t)(TOUCH_Y_MAX - TOUCH_Y_MIN);
  x = TOUCH_SCREEN_X_OFFSET - scaled;

  if (x < 0) return 0U;
  if (x >= (int32_t)LCD_WIDTH) return LCD_WIDTH - 1U;
  return (uint16_t)x;
}

static uint8_t TouchRead(TouchPoint *point)
{
  uint32_t raw_x = 0U;
  uint32_t raw_y = 0U;
  uint8_t i;

  point->pressed = 0U;

  if (TouchPressedRaw() == 0U)
  {
    return 0U;
  }

  for (i = 0U; i < 4U; i++)
  {
    PinOutput(TOUCH_XP);
    PinOutput(LCD_RS);
    PinHigh(TOUCH_XP);
    PinLow(LCD_RS);
    PinAnalog(LCD_CS);
    PinInput(TOUCH_YM);
    HAL_Delay(1);
    raw_x += TouchAdc_Read(&hadc3, ADC_CHANNEL_12); /* YP = PB0 = ADC3_IN12 */

    PinOutput(LCD_CS);
    PinOutput(TOUCH_YM);
    PinHigh(LCD_CS);
    PinLow(TOUCH_YM);
    PinAnalog(LCD_RS);
    PinInput(TOUCH_XP);
    HAL_Delay(1);
    raw_y += TouchAdc_Read(&hadc2, ADC_CHANNEL_1);  /* XM = PA4 = ADC2_IN1 */
  }

  raw_x /= 4U;
  raw_y /= 4U;
  LCD_RestoreBus();

  if (TouchPressedRaw() == 0U)
  {
    return 0U;
  }

  /*
   * The LCD is in landscape mode. With this shield the resistive axes are
   * swapped relative to the display axes.
   */
  point->x = MapTouchX((uint16_t)raw_y);
  point->y = MapClamp((uint16_t)raw_x, TOUCH_X_MIN, TOUCH_X_MAX, LCD_HEIGHT);
  point->pressed = 1U;
  return 1U;
}

void DisplayDriver_Init(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  TouchAdc_Init();
  LCD_Init();
  LCD_FillRect(0U, 0U, LCD_WIDTH, LCD_HEIGHT, COLOR_WHITE);
}

void DisplayDriver_FillScreen(uint16_t color)
{
  LCD_FillRect(0U, 0U, LCD_WIDTH, LCD_HEIGHT, color);
}

void DisplayDriver_FillRect(uint16_t x, uint16_t y, uint16_t width,
                            uint16_t height, uint16_t color)
{
  LCD_FillRect(x, y, width, height, color);
}

void DisplayDriver_DrawRect(uint16_t x, uint16_t y, uint16_t width,
                            uint16_t height, uint16_t color)
{
  LCD_DrawRect(x, y, width, height, color);
}

void DisplayDriver_DrawText(uint16_t x, uint16_t y, const char *text,
                            uint16_t color, uint16_t background,
                            uint8_t scale)
{
  uint16_t cursor = x;
  if ((text == NULL) || (scale == 0U)) return;

  while ((*text != '\0') &&
         ((uint32_t)cursor + (uint32_t)(6U * scale) <= LCD_WIDTH))
  {
    LCD_DrawChar(*text++, cursor, y, color, background, scale);
    cursor = (uint16_t)(cursor + 6U * scale);
  }
}

uint8_t DisplayDriver_ReadTouch(DisplayTouchPoint *point)
{
  TouchPoint raw;
  if (point == NULL) return 0U;
  if (TouchRead(&raw) == 0U)
  {
    point->pressed = 0U;
    return 0U;
  }
  point->x = raw.x;
  point->y = raw.y;
  point->pressed = raw.pressed;
  return 1U;
}
