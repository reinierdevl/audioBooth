#pragma once

#include <Arduino.h>

// Handset switch
constexpr gpio_num_t PIN_HEADSET_SWITCH = GPIO_NUM_1;

// Keypad rows
constexpr gpio_num_t PIN_ROW0 = GPIO_NUM_4;
constexpr gpio_num_t PIN_ROW1 = GPIO_NUM_5;
constexpr gpio_num_t PIN_ROW2 = GPIO_NUM_6;
constexpr gpio_num_t PIN_ROW3 = GPIO_NUM_7;

// Keypad columns
constexpr gpio_num_t PIN_COL0 = GPIO_NUM_15;
constexpr gpio_num_t PIN_COL1 = GPIO_NUM_16;
constexpr gpio_num_t PIN_COL2 = GPIO_NUM_17;
constexpr gpio_num_t PIN_COL3 = GPIO_NUM_18;

// ES8311 control interface
constexpr gpio_num_t PIN_I2C_SDA = GPIO_NUM_8;
constexpr gpio_num_t PIN_I2C_SCL = GPIO_NUM_9;

// ES8311 audio interface
constexpr gpio_num_t PIN_I2S_DOUT = GPIO_NUM_10;
constexpr gpio_num_t PIN_I2S_BCLK = GPIO_NUM_12;
constexpr gpio_num_t PIN_I2S_WS = GPIO_NUM_13;

// Physically connected to the ES8311, but unused by this player.
constexpr gpio_num_t PIN_I2S_MCLK = GPIO_NUM_11;
constexpr gpio_num_t PIN_I2S_DIN = GPIO_NUM_14;

// Optional microSD card in SPI mode
constexpr gpio_num_t PIN_SD_CS = GPIO_NUM_38;
constexpr gpio_num_t PIN_SD_SCK = GPIO_NUM_2;
constexpr gpio_num_t PIN_SD_MOSI = GPIO_NUM_47;
constexpr gpio_num_t PIN_SD_MISO = GPIO_NUM_21;

// Onboard addressable WS2812 RGB status LED.
constexpr gpio_num_t PIN_STATUS_LED = GPIO_NUM_48;

// Booth ID jumpers: open = logical 0, connected to ground = logical 1.
// ID0 is the least-significant bit; ID3 is the most-significant bit.
constexpr gpio_num_t PIN_BOOTH_ID0 = GPIO_NUM_39;
constexpr gpio_num_t PIN_BOOTH_ID1 = GPIO_NUM_40;
constexpr gpio_num_t PIN_BOOTH_ID2 = GPIO_NUM_41;
constexpr gpio_num_t PIN_BOOTH_ID3 = GPIO_NUM_42;
