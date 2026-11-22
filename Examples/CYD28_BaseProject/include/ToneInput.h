#ifndef TONE_INPUT_H
#define TONE_INPUT_H

#include <stdint.h>

class ToneInput
{
public:
	static constexpr uint16_t MIN_HZ = 200;
	static constexpr uint16_t MAX_HZ = 20000;

	bool append(char digit)
	{
		if (digit < '0' || digit > '9' || (!replaceOnDigit && length == 3))
			return false;
		if (replaceOnDigit)
		{
			clear();
			replaceOnDigit = false;
		}
		digits[length++] = digit;
		digits[length] = '\0';
		return true;
	}

	void clear()
	{
		length = 0;
		digits[0] = '\0';
		replaceOnDigit = false;
	}

	void backspace()
	{
		replaceOnDigit = false;
		if (length > 0)
			digits[--length] = '\0';
	}

	uint16_t frequency() const
	{
		uint16_t value = 0;
		for (uint8_t i = 0; i < length; ++i)
			value = value * 10 + digits[i] - '0';
		return value;
	}

	bool valid() const
	{
		return frequency() >= MIN_HZ && frequency() <= MAX_HZ;
	}

	const char *text() const { return digits; }
	void applied() { replaceOnDigit = true; }

private:
	char digits[4] = "";
	uint8_t length = 0;
	bool replaceOnDigit = false;
};

#endif
