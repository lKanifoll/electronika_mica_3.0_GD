/*
 * Pixels. Graphics library for TFT displays.
 *
 * Copyright (C) 2012-2013  Igor Repinetski
 *
 * The code is written in C/C++ for Arduino and can be easily ported to any microcontroller by rewritting the low level pin access functions.
 *
 * Text output methods of the library rely on Pixelmeister's font data format. See: http://pd4ml.com/pixelmeister
 *
 * This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License. To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/3.0/
 *
 * This library includes some code portions and algoritmic ideas derived from works of
 * - Andreas Schiffler -- aschiffler at ferzkopp dot net (SDL_gfx Project)
 * - K. Townsend http://microBuilder.eu (lpc1343codebase Project)
 */

/*
 * Parallel interface 16bit layer
 */

#include "Pixels.h"
#include "main.h"

#ifdef PIXELS_MAIN
#error Pixels_PPI8.h must be included before Pixels_<CONTROLLER>.h
#endif

#ifndef PIXELS_PPI8_H
#define PIXELS_PPI8_H

#define pulseLowWR PIN_RESET(LCD_WR_GPIO_Port, LCD_WR_Pin);PIN_SET(LCD_WR_GPIO_Port, LCD_WR_Pin);

class PPI8 {
private:
protected:
    void reset() 
		{
			PIN_SET(LCD_RST_GPIO_Port, LCD_RST_Pin);
			delay_1ms(5);
			PIN_RESET(LCD_RST_GPIO_Port, LCD_RST_Pin);
			delay_1ms(15);
			PIN_SET(LCD_RST_GPIO_Port, LCD_RST_Pin);
			delay_1ms(150);
    }

    void initInterface();

    void writeCmd(uint8_t b) 
		{
			PIN_RESET(LCD_RS_GPIO_Port, LCD_RS_Pin);
			LL_GPIO_WriteOutputPort(GPIOA, (LL_GPIO_ReadOutputPort(GPIOA) & 0xFF00) | b);
			pulseLowWR;
    }

    void writeData(uint8_t data) 
		{
			PIN_SET(LCD_RS_GPIO_Port, LCD_RS_Pin);
			LL_GPIO_WriteOutputPort(GPIOA, (LL_GPIO_ReadOutputPort(GPIOA) & 0xFF00) | data);
			pulseLowWR;
    }
		
		uint8_t readData()
		{
			/*
			LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
			GPIO_InitStruct.Pin = LL_GPIO_PIN_0 | LL_GPIO_PIN_1 | LL_GPIO_PIN_2 | LL_GPIO_PIN_3 | LL_GPIO_PIN_4 | LL_GPIO_PIN_5 | LL_GPIO_PIN_6 | LL_GPIO_PIN_7;
			GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
			GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
			GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
			LL_GPIO_Init(GPIOA, &GPIO_InitStruct);
			
			PIN_SET(LCD_RS_GPIO_Port, LCD_RS_Pin);
			PIN_RESET(LCD_RD_GPIO_Port, LCD_RD_Pin);
			uint8_t result = LL_GPIO_ReadInputPort(GPIOA) & 0xFF;
			PIN_SET(LCD_RD_GPIO_Port, LCD_RD_Pin);

			GPIO_InitStruct.Pin = LL_GPIO_PIN_0 | LL_GPIO_PIN_1 | LL_GPIO_PIN_2 | LL_GPIO_PIN_3 | LL_GPIO_PIN_4 | LL_GPIO_PIN_5 | LL_GPIO_PIN_6 | LL_GPIO_PIN_7;
			GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
			GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
			GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
			GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
			LL_GPIO_Init(GPIOA, &GPIO_InitStruct);
			
			return result;*/
		}

    void writeData(uint8_t hi, uint8_t lo) 
		{
			PIN_SET(LCD_RS_GPIO_Port, LCD_RS_Pin);
			LL_GPIO_WriteOutputPort(GPIOA, (LL_GPIO_ReadOutputPort(GPIOA) & 0xFF00) | hi);
			LL_GPIO_WriteOutputPort(GPIOA, (LL_GPIO_ReadOutputPort(GPIOA) & 0xFF00) | lo);
			pulseLowWR;
    }

    void writeDataTwice(uint8_t b) 
		{
			PIN_SET(LCD_RS_GPIO_Port, LCD_RS_Pin);
			LL_GPIO_WriteOutputPort(GPIOA, b);
			pulseLowWR;
			LL_GPIO_WriteOutputPort(GPIOA, b);
			pulseLowWR;
    }

    void writeCmdData(uint8_t cmd, uint16_t data) {
        writeCmd(cmd);
        writeData(highByte(data), lowByte(data));
    }

public:
    /**
     * Overrides SPI pins
     * @param scl
     * @param sda
     * @param cs chip select
     * @param rst reset
     * @param wr write pin
     */

    inline void setSpiPins(uint8_t scl, uint8_t sda, uint8_t cs, uint8_t rst, uint8_t wr) {
        // nop
    }

    /**
     * Overrides PPI pins
     * @param cs chip select
     */
    inline void setPpiPins(uint8_t rs, uint8_t wr, uint8_t cs, uint8_t rst, uint8_t rd) {
    }

    inline void registerSelect() {
      PIN_SET(LCD_RS_GPIO_Port, LCD_RS_Pin);  
    }
};

void PPI8::initInterface() {
/*
	LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = LL_GPIO_PIN_0 | LL_GPIO_PIN_1 | LL_GPIO_PIN_2 | LL_GPIO_PIN_3 | LL_GPIO_PIN_4 | LL_GPIO_PIN_5 | LL_GPIO_PIN_6 | LL_GPIO_PIN_7;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);
*/
	PIN_RESET(LCD_CS_GPIO_Port, LCD_CS_Pin);
	PIN_SET(LCD_RD_GPIO_Port, LCD_RD_Pin);
	PIN_SET(LCD_WR_GPIO_Port, LCD_WR_Pin);
	//PIN_SET(LCD_BL_GPIO_Port, LCD_BL_Pin);

  reset();
}
#endif
