/**
 * @file main.cpp
 * @author Piotr Zapart (www.hexefx.com)
 * @brief 	demo project for the CYD 2.8" board (ESP32 + ILI9341 display)
 * 
 * 	Libraries:
 * 		CYD28_LDR - onboard LDR for light intensite measurement
 * 		CYD28_RGBled - onboard RGB led, or just R if PSRAM mod is perfromed
 * 		CYD28_SD - sd card library and filesystem
 * 		CYD28_display - dual buffered display driver configured for LVGL v8
 * 		CYD28_Touchscreen - bit banged SPI driver for the onboard touch screen controller
 * 		CYD_Audio - audio library optimized for internal 8bit DAC, based on ESP32-audioI2S by Wolle (schreibfaul1)
 * 		lvgl - v8.3.9 
 * 		SimpleCLI - console command via UART
 * 		TFT_eSPI - used for low level dislpay access.
 * 		WifiManager - added as lib dependency in platformio.ini
 * 
 * 		--- Audio ---
 * 		Audio is runing as a separate task on core 0. Use queues to communicate with it. 
 * 		See CYD28_audio.h/cpp and gui.h/cpp files  
 * 
 * @version 1.
 * @date 2023-08-25
 */
#include <Arduino.h>
#include <FastLED.h>

#define RGB_LED_PIN 27
#define NUM_LEDS 1
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

#define LCD_BACKLIGHT_PIN 25


#define WAVE_PIN 8      // P3 / CN1 connector
#define FREQ_HZ 523

CRGB leds[NUM_LEDS];
const float FLASH_DELAY_MS = 500;

void setup()
{
	// Set serial baud rate
	Serial.begin(9600);
	Serial.write("Serial initialized\n");

	// Flash LCD backlight
	// pinMode(LCD_BACKLIGHT_PIN, OUTPUT);
	// digitalWrite(LCD_BACKLIGHT_PIN, HIGH);

	// Pin the RMT driver at compile time (never initializes or links PARLIO)
  	FastLED.addLeds<LED_TYPE, RGB_LED_PIN, COLOR_ORDER, fl::Bus::RMT>(leds, NUM_LEDS);
    FastLED.setBrightness(50);

	pinMode(WAVE_PIN, OUTPUT);
	tone(WAVE_PIN, FREQ_HZ);
}

void loop()
{
	leds[0] = CRGB::Red; FastLED.show();
	// rgbLedWrite(27, 255, 0, 0); // Red
	// digitalWrite(LCD_BACKLIGHT_PIN, LOW);
	delay(FLASH_DELAY_MS);

	leds[0] = CRGB::Blue; FastLED.show();
	// rgbLedWrite(27, 0, 0, 255); // Blue
	// digitalWrite(LCD_BACKLIGHT_PIN, HIGH);
	delay(FLASH_DELAY_MS);
}
