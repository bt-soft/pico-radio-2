#include <Arduino.h>

#include "utils.h"

//------------------- Rotary Encoder
#include "RotaryEncoder.h"
RotaryEncoder rotaryEncoder = RotaryEncoder(PIN_ENCODER_CLK, PIN_ENCODER_DT, PIN_ENCODER_SW, ROTARY_ENCODER_STEPS_PER_NOTCH);
#define ROTARY_ENCODER_SERVICE_INTERVAL_IN_MSEC 1  // 1msec

#define __USE_ROTARY_ENCODER_IN_HW_TIMER

#ifdef __USE_ROTARY_ENCODER_IN_HW_TIMER
// Pico Hardware timer a Rotary encoder olvasására
#include <RPi_Pico_TimerInterrupt.h>
RPI_PICO_Timer ITimer1(1);

/**
 * Hardware timer interrupt service routine a rotaryhoz
 */
bool hardwareTimerHandler1(struct repeating_timer *t) {
    rotaryEncoder.service();
    return true;
}
#endif

//------------------- EEPROM Config
#include "Config.h"
Config config;

/**
 * setup() - a program kezdőpontja
 */
void setup() {
#ifdef __DEBUG
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
#endif

    // Rotary Encoder beállítása
    rotaryEncoder.setDoubleClickEnabled(true);
    rotaryEncoder.setAccelerationEnabled(true);
#ifdef __USE_ROTARY_ENCODER_IN_HW_TIMER
    // Pico HW Timer1 beállítása a rotaryhoz
    ITimer1.attachInterruptInterval(ROTARY_ENCODER_SERVICE_INTERVAL_IN_MSEC * 1000, hardwareTimerHandler1);
#endif

    // Beeper beállítása
    Utils::beepInit();

    // TFT LED háttérvilágítás kimenet
    pinMode(PIN_TFT_BACKGROUND_LED, OUTPUT);
    analogWrite(PIN_TFT_BACKGROUND_LED, TFT_BACKGROUND_LED_MAX_BRIGHTNESS);
}

/**
 * loop() - a program fő ciklusa
 */
void loop() {
    // put your main code here, to run repeatedly:
}
