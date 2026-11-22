#include <Arduino.h>
#include <Adafruit_ST7789.h>
#include <FastLED.h>
#include <Preferences.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include "ToneInput.h"
#include "TouchCalibration.h"

#define RGB_LED_PIN 27
#define NUM_LEDS 1
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define LCD_BACKLIGHT_PIN 25
#define WAVE_PIN 8
#define FREQ_HZ 523

constexpr uint8_t LCD_CS_PIN = 23;
constexpr uint8_t LCD_DC_PIN = 24;
constexpr uint8_t TOUCH_CS_PIN = 1;
constexpr uint8_t SD_CS_PIN = 10;
constexpr uint8_t SPI_SCK_PIN = 6;
constexpr uint8_t SPI_MISO_PIN = 2;
constexpr uint8_t SPI_MOSI_PIN = 7;
constexpr uint8_t LCD_MADCTL_BGR = 0x08;
constexpr uint16_t BACKGROUND = 0x0841;
constexpr uint16_t KEY_COLOR = 0x2124;
constexpr uint16_t ACCENT = 0x07FF;
constexpr uint16_t ERROR_COLOR = 0xFBE0;
constexpr uint32_t FLASH_DELAY_MS = 500;

Adafruit_ST7789 lcd(&SPI, LCD_CS_PIN, LCD_DC_PIN, -1);
XPT2046_Touchscreen touch(TOUCH_CS_PIN);
CRGB leds[NUM_LEDS];
Preferences preferences;
ToneInput input;
TouchCalibration calibration;
bool preferencesReady = false;
bool playing = false;
uint16_t activeFrequency = FREQ_HZ;
uint32_t lastFlash = 0;
bool flashing = false;
bool calibrating = false;

struct Key
{
	int16_t x;
	int16_t y;
	int16_t width;
	int16_t height;
	const char *label;
	uint16_t color;
};

const Key keys[] = {
	{8, 88, 68, 32, "7", KEY_COLOR},
	{84, 88, 68, 32, "8", KEY_COLOR},
	{160, 88, 68, 32, "9", KEY_COLOR},
	{8, 126, 68, 32, "4", KEY_COLOR},
	{84, 126, 68, 32, "5", KEY_COLOR},
	{160, 126, 68, 32, "6", KEY_COLOR},
	{8, 164, 68, 32, "1", KEY_COLOR},
	{84, 164, 68, 32, "2", KEY_COLOR},
	{160, 164, 68, 32, "3", KEY_COLOR},
	{8, 202, 68, 32, "CLR", KEY_COLOR},
	{84, 202, 68, 32, "0", KEY_COLOR},
	{160, 202, 68, 32, "DEL", KEY_COLOR},
	{236, 88, 76, 70, "APPLY", 0x03E8},
	{236, 164, 76, 32, "STOP", 0xA800},
	{236, 202, 76, 32, "CAL", KEY_COLOR}
};
constexpr int KEY_COUNT = sizeof(keys) / sizeof(keys[0]);
constexpr int APPLY_KEY = 12;
constexpr int STOP_KEY = 13;
constexpr int CAL_KEY = 14;

void centeredText(const char *text, int16_t x, int16_t y,
				  int16_t width, int16_t height, uint8_t size, uint16_t color)
{
	int16_t textX, textY;
	uint16_t textWidth, textHeight;
	lcd.setTextSize(size);
	lcd.setTextColor(color);
	lcd.getTextBounds(text, 0, 0, &textX, &textY, &textWidth, &textHeight);
	lcd.setCursor(x + (width - textWidth) / 2 - textX,
				  y + (height - textHeight) / 2 - textY);
	lcd.print(text);
}

void drawKey(int index, bool pressed = false)
{
	const Key &key = keys[index];
	lcd.fillRoundRect(key.x, key.y, key.width, key.height, 5,
					  pressed ? ACCENT : key.color);
	centeredText(key.label, key.x, key.y, key.width, key.height,
				  index < 9 || index == 10 ? 3 : 2,
				  pressed ? ST77XX_BLACK : ST77XX_WHITE);
}

void drawHeader(const char *message = "Enter Hz, then APPLY", bool error = false)
{
	lcd.fillRect(0, 0, 320, 84, BACKGROUND);
	lcd.setTextSize(1);
	lcd.setTextColor(ACCENT);
	lcd.setCursor(8, 5);
	lcd.printf("SQUARE WAVE  |  GPIO8  |  %u-%u Hz", ToneInput::MIN_HZ, ToneInput::MAX_HZ);
	char status[40];
	snprintf(status, sizeof(status), "%s %u Hz",
			 playing ? "Playing" : "Stopped | Last:", activeFrequency);
	lcd.setTextColor(ST77XX_WHITE);
	lcd.setCursor(8, 19);
	lcd.print(status);
	const char *text = input.text()[0] ? input.text() : "---";
	centeredText(text, 8, 32, 220, 34, 4, ST77XX_WHITE);
	centeredText("Hz", 236, 32, 76, 34, 2, ACCENT);
	lcd.setTextSize(1);
	lcd.setTextColor(error ? ERROR_COLOR : ST77XX_WHITE);
	lcd.setCursor(8, 73);
	lcd.print(message);
}

void drawUI()
{
	lcd.fillScreen(BACKGROUND);
	drawHeader();
	for (int i = 0; i < KEY_COUNT; ++i)
		drawKey(i);
}

void updateLed()
{
	if (flashing && millis() - lastFlash >= FLASH_DELAY_MS)
		flashing = false;
	const CRGB color = calibrating ? CRGB::White :
					   flashing ? CRGB::Red :
					   playing ? CRGB::Green : CRGB::Blue;
	if (leds[0] != color)
	{
		leds[0] = color;
		FastLED.show();
	}
}

void flashLed()
{
	lastFlash = millis();
	flashing = true;
	updateLed();
}

void waitForRelease()
{
	uint32_t releasedAt = millis();
	while (millis() - releasedAt < 60)
	{
		if (touch.touched())
			releasedAt = millis();
		updateLed();
		delay(5);
	}
}

void calibrateTouch()
{
	calibrating = true;
	flashing = false;
	updateLed();
	const int16_t targetX[] = {24, 296, 24};
	const int16_t targetY[] = {24, 24, 216};
	do
	{
		for (uint8_t i = 0; i < 3; ++i)
		{
			waitForRelease();
			lcd.fillScreen(BACKGROUND);
			centeredText("TOUCH CALIBRATION", 0, 80, 320, 24, 2, ACCENT);
			centeredText("Hold each cross until it disappears", 0, 112, 320, 16, 1, ST77XX_WHITE);
			lcd.drawCircle(targetX[i], targetY[i], 10, ST77XX_WHITE);
			lcd.drawFastHLine(targetX[i] - 14, targetY[i], 29, ACCENT);
			lcd.drawFastVLine(targetX[i], targetY[i] - 14, 29, ACCENT);
			int32_t sumX = 0, sumY = 0;
			uint8_t samples = 0;
			while (samples < 12)
			{
				if (touch.touched())
				{
					TS_Point point = touch.getPoint();
					sumX += point.x;
					sumY += point.y;
					++samples;
				}
				else
				{
					samples = 0;
					sumX = sumY = 0;
				}
				updateLed();
				delay(10);
			}
			calibration.x[i] = sumX / samples;
			calibration.y[i] = sumY / samples;
			lcd.fillScreen(BACKGROUND);
		}
		if (!calibration.valid())
		{
			Serial.println("Invalid touch calibration; please repeat the three targets.");
			centeredText("Invalid calibration. Try again.", 0, 100, 320, 24, 1, ERROR_COLOR);
			delay(1500);
		}
	} while (!calibration.valid());
	waitForRelease();
	calibrating = false;
	updateLed();
	drawUI();
	if (!preferencesReady ||
		preferences.putBytes("calibration", &calibration, sizeof(calibration)) != sizeof(calibration))
	{
		Serial.println("Could not save touch calibration.");
		drawHeader("Calibration not saved", true);
	}
}

void handleKey(int index)
{
	if (index == CAL_KEY)
	{
		calibrateTouch();
		return;
	}
	if (index == STOP_KEY)
	{
		flashLed();
		if (playing)
			noTone(WAVE_PIN);
		playing = false;
		drawHeader("Stopped. APPLY to play");
		return;
	}
	if (index == APPLY_KEY)
	{
		flashLed();
		if (!input.valid())
		{
			Serial.println("Frequency must be between 400 and 800 Hz.");
			drawHeader("Enter 400-800 Hz", true);
			return;
		}
		activeFrequency = input.frequency();
		tone(WAVE_PIN, activeFrequency);
		playing = true;
		input.applied();
		Serial.printf("Square wave: %u Hz on GPIO%u\n", activeFrequency, WAVE_PIN);
		drawHeader("Applied. Enter next tone");
		return;
	}
	if (index == 9)
		input.clear();
	else if (index == 11)
		input.backspace();
	else if (!input.append(keys[index].label[0]))
	{
		Serial.println("Frequency input is limited to three digits.");
		drawHeader("Max 3 digits. CLR or DEL", true);
		return;
	}
	drawHeader();
}

int keyAt(int16_t x, int16_t y)
{
	for (int i = 0; i < KEY_COUNT; ++i)
	{
		const Key &key = keys[i];
		if (x >= key.x && x < key.x + key.width &&
			y >= key.y && y < key.y + key.height)
			return i;
	}
	return -1;
}

void setup()
{
	Serial.begin(9600);
	Serial.println("Serial initialized");
	FastLED.addLeds<LED_TYPE, RGB_LED_PIN, COLOR_ORDER, fl::Bus::RMT>(leds, NUM_LEDS);
	FastLED.setBrightness(50);
	leds[0] = CRGB::Blue;
	FastLED.show();
	pinMode(WAVE_PIN, OUTPUT);
	digitalWrite(WAVE_PIN, LOW);

	// All three devices share SPI; keep unused devices deselected.
	for (uint8_t pin : {LCD_CS_PIN, TOUCH_CS_PIN, SD_CS_PIN})
	{
		pinMode(pin, OUTPUT);
		digitalWrite(pin, HIGH);
	}
	SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);
	lcd.init(240, 320);
	lcd.setSPISpeed(40000000); 
	lcd.setRotation(1);
	// Manufacturer's setup specifies BGR with inversion disabled.
	uint8_t madctl = ST77XX_MADCTL_MY | ST77XX_MADCTL_MV | LCD_MADCTL_BGR;
	lcd.sendCommand(ST77XX_MADCTL, &madctl, 1);
	lcd.invertDisplay(false);
	lcd.setTextWrap(false);
	pinMode(LCD_BACKLIGHT_PIN, OUTPUT);
	digitalWrite(LCD_BACKLIGHT_PIN, HIGH);
	if (!touch.begin(SPI))
	{
		Serial.println("Touch controller initialization failed.");
		lcd.fillScreen(BACKGROUND);
		centeredText("Touch initialization failed", 0, 100, 320, 24, 1, ERROR_COLOR);
		while (true)
		{
			updateLed();
			delay(5);
		}
	}
	touch.setRotation(1);
	preferencesReady = preferences.begin("tone-keypad", false);
	if (!preferencesReady)
		Serial.println("Could not open touch calibration storage.");
	if (preferencesReady && preferences.getBytesLength("calibration") == sizeof(calibration))
	{
		if (preferences.getBytes("calibration", &calibration, sizeof(calibration)) != sizeof(calibration))
		{
			Serial.println("Could not read touch calibration.");
			calibration = TouchCalibration{};
		}
	}
	input.append('5');
	input.append('2');
	input.append('3');
	input.applied();
	if (!calibration.valid())
		calibrateTouch();
	else
		drawUI();
}

void loop()
{
	static int pressedKey = -1;
	static bool touchHeld = false;
	static uint32_t lastTouch = 0;
	updateLed();
	if (touch.touched())
	{
		lastTouch = millis();
		if (!touchHeld)
		{
			touchHeld = true;
			TS_Point point = touch.getPoint();
			int16_t x, y;
			calibration.map(point.x, point.y, x, y);
			pressedKey = keyAt(x, y);
			if (pressedKey >= 0)
				drawKey(pressedKey, true);
		}
	}
	else if (touchHeld && millis() - lastTouch >= 60)
	{
		touchHeld = false;
		if (pressedKey >= 0)
		{
			drawKey(pressedKey);
			handleKey(pressedKey);
			pressedKey = -1;
		}
	}
	delay(5);
}
