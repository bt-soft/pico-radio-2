#ifndef __DEFINES_H
#define __DEFINES_H

#include <Arduino.h>

//---- Pinouts ------------------------------------------
// TFT (A TFT_eSPI_User_Setup.h-ban a pinout)

// Feszültségmérés
#define PIN_VBUS_INPUT A0  // A0/GPIO26 a VBUS bemenethez

// Audio FFT bemenet
#define AUDIO_INPUT_PIN A1  // A1/GPIO27 az audio bemenethez

// I2C si4735
#define PIN_SI4735_I2C_SDA 8
#define PIN_SI4735_I2C_SCL 9
#define PIN_SI4735_RESET 10

// Rotary Encoder
#define __USE_ROTARY_ENCODER_IN_HW_TIMER
#define PIN_ENCODER_CLK 17
#define PIN_ENCODER_DT 16
#define PIN_ENCODER_SW 18

// Others
#define PIN_TFT_BACKGROUND_LED 21
#define PIN_AUDIO_MUTE 20
#define PIN_BEEPER 22
//---- Pinouts ------------------------------------------

// TFT háttérvilágítás max érték
#define TFT_BACKGROUND_LED_MAX_BRIGHTNESS 255
#define TFT_BACKGROUND_LED_MIN_BRIGHTNESS 5

//--- Batterry ---
#define MIN_BATTERRY_VOLTAGE 270  // Minimum akkumulátor feszültség (V*100)
#define MAX_BATTERRY_VOLTAGE 405  // Maximum akkumulátor feszültség (V*100)

#define TFT_COLOR_DRAINED_BATTERRY TFT_COLOR(248, 252, 0)
#define TFT_COLOR_SUMBERSIBLE_BATTERRY TFT_ORANGE

//--- ScreenSaver
#define SCREEN_SAVER_TIMEOUT_MIN 1
#define SCREEN_SAVER_TIMEOUT_MAX 60
#define SCREEN_SAVER_TIMEOUT 10  // 10 perc a képernyővédő időzítése

//--- CW mód adatai
#define CW_DEMOD_MODE LSB          // CW demodulációs mód = LSB + 700Hz offset
#define CW_SHIFT_FREQUENCY 800.0f  // CW alap offset

//--- Debug ---
#define __DEBUG  // Debug mód bekapcsolása

#ifdef __DEBUG
// #define SHOW_MEMORY_INFO
// #define MEMORY_INFO_INTERVAL 20 * 1000  // 20mp

// Soros portra várakozás a debug üzenetek előtt
#define DEBUG_WAIT_FOR_SERIAL

#endif

//--- TFT colors ---
#define TFT_COLOR(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
// #define COMPLEMENT_COLOR(color) \
//     (TFT_COLOR((255 - ((color >> 16) & 0xFF)), (255 - ((color >> 8) & 0xFF)), (255 - (color & 0xFF))))
// #define PUSHED_COLOR(color) ((((color & 0xF800) >> 1) & 0xF800) | (((color & 0x07E0) >> 1) & 0x07E0) | (((color & 0x001F) >> 1) & 0x001F))
#define TFT_COLOR_BACKGROUND TFT_BLACK

//--- Array Utils ---
#define ARRAY_ITEM_COUNT(array) (sizeof(array) / sizeof(array[0]))

//--- C String compare -----
#define STREQ(a, b) (strcmp((a), (b)) == 0)

//--- Debug ---
#ifdef __DEBUG
#define DEBUG(fmt, ...) Serial.printf_P(PSTR(fmt) __VA_OPT__(, ) __VA_ARGS__)
#else
#define DEBUG(fmt, ...)  // Üres makró, ha __DEBUG nincs definiálva
#endif

#endif  // __DEFINES_H