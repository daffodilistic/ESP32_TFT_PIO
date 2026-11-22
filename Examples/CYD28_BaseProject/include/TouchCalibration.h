#ifndef TOUCH_CALIBRATION_H
#define TOUCH_CALIBRATION_H

#include <stdint.h>

struct TouchCalibration
{
	int16_t x[3] = {};
	int16_t y[3] = {};

	int32_t determinant() const
	{
		return (x[1] - x[0]) * (y[2] - y[0]) -
			   (x[2] - x[0]) * (y[1] - y[0]);
	}

	bool valid() const
	{
		for (uint8_t i = 0; i < 3; ++i)
			if (x[i] < 0 || x[i] > 4095 || y[i] < 0 || y[i] > 4095)
				return false;
		const int32_t det = determinant();
		return det > 1000000 || det < -1000000;
	}

	void map(int16_t rawX, int16_t rawY, int16_t &screenX, int16_t &screenY) const
	{
		// Three targets handle reversed/swapped axes as well as panel skew.
		const float det = static_cast<float>(determinant());
		const float u = ((rawX - x[0]) * (y[2] - y[0]) -
						 (rawY - y[0]) * (x[2] - x[0])) / det;
		const float v = ((x[1] - x[0]) * (rawY - y[0]) -
						 (y[1] - y[0]) * (rawX - x[0])) / det;
		screenX = static_cast<int16_t>(24 + 272 * u);
		screenY = static_cast<int16_t>(24 + 192 * v);
	}
};

#endif
