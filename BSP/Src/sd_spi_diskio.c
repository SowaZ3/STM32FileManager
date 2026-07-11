#include "diskio.h"
#include "main.h"

#define CMD0    0U
#define CMD1    1U
#define CMD8    8U
#define CMD9    9U
#define CMD12   12U
#define CMD16   16U
#define CMD17   17U
#define CMD18   18U
#define CMD23   23U
#define CMD24   24U
#define CMD25   25U
#define CMD55   55U
#define CMD58   58U
#define ACMD23  (0x80U + 23U)
#define ACMD41  (0x80U + 41U)

#define CT_MMC    0x01U
#define CT_SD1    0x02U
#define CT_SD2    0x04U
#define CT_BLOCK  0x08U

#define SD_CS_PORT GPIOB
#define SD_CS_PIN  GPIO_PIN_6

static SPI_HandleTypeDef hspi1;
static volatile DSTATUS diskStatus = STA_NOINIT;
static uint8_t cardType;

static void SD_Select(uint8_t selected)
{
  HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN,
                    selected ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static uint8_t SPI_Exchange(uint8_t value)
{
  uint8_t received = 0xFFU;
  if (HAL_SPI_TransmitReceive(&hspi1, &value, &received, 1U, 100U) != HAL_OK)
  {
    return 0xFFU;
  }
  return received;
}

static void SPI_SetPrescaler(uint32_t prescaler)
{
  __HAL_SPI_DISABLE(&hspi1);
  MODIFY_REG(hspi1.Instance->CR1, SPI_CR1_BR, prescaler);
  __HAL_SPI_ENABLE(&hspi1);
}

static uint8_t SD_HardwareInit(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_SPI1_CLK_ENABLE();

  gpio.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF5_SPI1;
  HAL_GPIO_Init(GPIOA, &gpio);

  gpio.Pin = SD_CS_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(SD_CS_PORT, &gpio);
  SD_Select(0U);

  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7U;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  return (HAL_SPI_Init(&hspi1) == HAL_OK) ? 1U : 0U;
}

static uint8_t SD_WaitReady(uint32_t timeoutMs)
{
  const uint32_t start = HAL_GetTick();
  do
  {
    if (SPI_Exchange(0xFFU) == 0xFFU)
    {
      return 1U;
    }
  } while ((HAL_GetTick() - start) < timeoutMs);
  return 0U;
}

static void SD_Deselect(void)
{
  SD_Select(0U);
  SPI_Exchange(0xFFU);
}

static uint8_t SD_SelectCard(void)
{
  SD_Select(1U);
  SPI_Exchange(0xFFU);
  if (SD_WaitReady(500U) != 0U)
  {
    return 1U;
  }
  SD_Deselect();
  return 0U;
}

static uint8_t SD_SendCommand(uint8_t command, uint32_t argument)
{
  uint8_t response;
  uint8_t crc = 0x01U;

  if ((command & 0x80U) != 0U)
  {
    command &= 0x7FU;
    response = SD_SendCommand(CMD55, 0U);
    if (response > 1U)
    {
      return response;
    }
  }

  if (command != CMD12)
  {
    SD_Deselect();
    if (SD_SelectCard() == 0U)
    {
      return 0xFFU;
    }
  }

  SPI_Exchange((uint8_t)(0x40U | command));
  SPI_Exchange((uint8_t)(argument >> 24));
  SPI_Exchange((uint8_t)(argument >> 16));
  SPI_Exchange((uint8_t)(argument >> 8));
  SPI_Exchange((uint8_t)argument);
  if (command == CMD0) crc = 0x95U;
  if (command == CMD8) crc = 0x87U;
  SPI_Exchange(crc);

  if (command == CMD12)
  {
    SPI_Exchange(0xFFU);
  }
  for (uint8_t attempts = 10U; attempts > 0U; --attempts)
  {
    response = SPI_Exchange(0xFFU);
    if ((response & 0x80U) == 0U)
    {
      return response;
    }
  }
  return 0xFFU;
}

static uint8_t SD_ReceiveBlock(uint8_t *buffer, uint16_t length)
{
  const uint32_t start = HAL_GetTick();
  uint8_t token;
  do
  {
    token = SPI_Exchange(0xFFU);
  } while ((token == 0xFFU) && ((HAL_GetTick() - start) < 200U));
  if (token != 0xFEU)
  {
    return 0U;
  }

  while (length-- > 0U)
  {
    *buffer++ = SPI_Exchange(0xFFU);
  }
  SPI_Exchange(0xFFU);
  SPI_Exchange(0xFFU);
  return 1U;
}

static uint8_t SD_TransmitBlock(const uint8_t *buffer, uint8_t token)
{
  if (SD_WaitReady(500U) == 0U)
  {
    return 0U;
  }
  SPI_Exchange(token);
  if (token == 0xFDU)
  {
    return 1U;
  }

  for (uint16_t i = 0U; i < 512U; ++i)
  {
    SPI_Exchange(buffer[i]);
  }
  SPI_Exchange(0xFFU);
  SPI_Exchange(0xFFU);
  return ((SPI_Exchange(0xFFU) & 0x1FU) == 0x05U) ? 1U : 0U;
}

DSTATUS disk_initialize(BYTE physicalDrive)
{
  uint8_t type = 0U;
  uint8_t response;
  uint8_t ocr[4] = {0};

  if (physicalDrive != 0U)
  {
    return STA_NOINIT;
  }
  if (SD_HardwareInit() == 0U)
  {
    return STA_NOINIT;
  }
  for (uint8_t i = 0U; i < 10U; ++i)
  {
    SPI_Exchange(0xFFU);
  }

  if (SD_SendCommand(CMD0, 0U) == 1U)
  {
    const uint32_t start = HAL_GetTick();
    if (SD_SendCommand(CMD8, 0x1AAU) == 1U)
    {
      for (uint8_t i = 0U; i < 4U; ++i) ocr[i] = SPI_Exchange(0xFFU);
      if ((ocr[2] == 0x01U) && (ocr[3] == 0xAAU))
      {
        do
        {
          response = SD_SendCommand(ACMD41, 1UL << 30);
        } while ((response != 0U) && ((HAL_GetTick() - start) < 1000U));
        if ((response == 0U) && (SD_SendCommand(CMD58, 0U) == 0U))
        {
          for (uint8_t i = 0U; i < 4U; ++i) ocr[i] = SPI_Exchange(0xFFU);
          type = CT_SD2 | ((ocr[0] & 0x40U) ? CT_BLOCK : 0U);
        }
      }
    }
    else
    {
      uint8_t initCommand;
      if (SD_SendCommand(ACMD41, 0U) <= 1U)
      {
        type = CT_SD1;
        initCommand = ACMD41;
      }
      else
      {
        type = CT_MMC;
        initCommand = CMD1;
      }
      do
      {
        response = SD_SendCommand(initCommand, 0U);
      } while ((response != 0U) && ((HAL_GetTick() - start) < 1000U));
      if ((response != 0U) || (SD_SendCommand(CMD16, 512U) != 0U))
      {
        type = 0U;
      }
    }
  }

  cardType = type;
  SD_Deselect();
  if (type != 0U)
  {
    diskStatus &= (DSTATUS)~STA_NOINIT;
    SPI_SetPrescaler(SPI_BAUDRATEPRESCALER_8);
  }
  else
  {
    diskStatus = STA_NOINIT;
  }
  return diskStatus;
}

DSTATUS disk_status(BYTE physicalDrive)
{
  return (physicalDrive == 0U) ? diskStatus : STA_NOINIT;
}

DRESULT disk_read(BYTE physicalDrive, BYTE *buffer, DWORD sector, UINT count)
{
  if ((physicalDrive != 0U) || (buffer == NULL) || (count == 0U))
  {
    return RES_PARERR;
  }
  if ((diskStatus & STA_NOINIT) != 0U) return RES_NOTRDY;
  if ((cardType & CT_BLOCK) == 0U)
  {
    sector *= 512U;
  }

  if (count == 1U)
  {
    if ((SD_SendCommand(CMD17, sector) == 0U) &&
        (SD_ReceiveBlock(buffer, 512U) != 0U))
    {
      count = 0U;
    }
  }
  else if (SD_SendCommand(CMD18, sector) == 0U)
  {
    do
    {
      if (SD_ReceiveBlock(buffer, 512U) == 0U) break;
      buffer += 512U;
    } while (--count > 0U);
    SD_SendCommand(CMD12, 0U);
  }
  SD_Deselect();
  return (count == 0U) ? RES_OK : RES_ERROR;
}

DRESULT disk_write(BYTE physicalDrive, const BYTE *buffer, DWORD sector, UINT count)
{
  if ((physicalDrive != 0U) || (buffer == NULL) || (count == 0U))
  {
    return RES_PARERR;
  }
  if ((diskStatus & STA_NOINIT) != 0U) return RES_NOTRDY;
  if ((cardType & CT_BLOCK) == 0U)
  {
    sector *= 512U;
  }

  if (count == 1U)
  {
    if ((SD_SendCommand(CMD24, sector) == 0U) &&
        (SD_TransmitBlock(buffer, 0xFEU) != 0U))
    {
      count = 0U;
    }
  }
  else
  {
    if ((cardType & (CT_SD1 | CT_SD2)) != 0U)
    {
      SD_SendCommand(ACMD23, count);
    }
    if (SD_SendCommand(CMD25, sector) == 0U)
    {
      do
      {
        if (SD_TransmitBlock(buffer, 0xFCU) == 0U) break;
        buffer += 512U;
      } while (--count > 0U);
      if (SD_TransmitBlock(NULL, 0xFDU) == 0U)
      {
        count = 1U;
      }
    }
  }
  SD_Deselect();
  return (count == 0U) ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl(BYTE physicalDrive, BYTE command, void *buffer)
{
  DRESULT result = RES_ERROR;
  uint8_t csd[16];

  if (physicalDrive != 0U) return RES_PARERR;
  if ((diskStatus & STA_NOINIT) != 0U) return RES_NOTRDY;

  if (command == CTRL_SYNC)
  {
    if (SD_SelectCard() != 0U) result = RES_OK;
  }
  else if (command == GET_SECTOR_SIZE)
  {
    *(WORD *)buffer = 512U;
    result = RES_OK;
  }
  else if (command == GET_SECTOR_COUNT)
  {
    if ((SD_SendCommand(CMD9, 0U) == 0U) &&
        (SD_ReceiveBlock(csd, 16U) != 0U))
    {
      if ((csd[0] >> 6) == 1U)
      {
        const DWORD size = (DWORD)csd[9] + ((DWORD)csd[8] << 8) + 1U;
        *(DWORD *)buffer = size << 10;
      }
      else
      {
        const uint8_t n = (uint8_t)((csd[5] & 15U) +
                            ((csd[10] & 128U) >> 7) +
                            ((csd[9] & 3U) << 1) + 2U);
        const WORD size = (WORD)((csd[8] >> 6) +
                          ((WORD)csd[7] << 2) +
                          ((WORD)(csd[6] & 3U) << 10) + 1U);
        *(DWORD *)buffer = (DWORD)size << (n - 9U);
      }
      result = RES_OK;
    }
  }
  else if (command == GET_BLOCK_SIZE)
  {
    *(DWORD *)buffer = 1U;
    result = RES_OK;
  }
  else if (command == MMC_GET_TYPE)
  {
    *(BYTE *)buffer = cardType;
    result = RES_OK;
  }
  else
  {
    result = RES_PARERR;
  }
  SD_Deselect();
  return result;
}

DWORD get_fattime(void)
{
  return ((DWORD)(2026U - 1980U) << 25) | ((DWORD)1U << 21) | ((DWORD)1U << 16);
}
