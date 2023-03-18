#pragma once
#include "base/base.hh"

// Ring-buffer of performance metrics/datapoints, like frame or render pass times.
struct MetricBuffer {
	uint32_t frames;
	uint32_t next = 0;
	uint32_t used = 0;
	float* values = nullptr;

	MetricBuffer(uint32_t frames) : frames(frames) {}

	void push(float datapoint) {
		if (ExpectFalse(!values)) {
			// Leaked because we don't care. If you want to fix the leak, note that MetricBuffer is
			// copied as part of Engine in the ShaderDefine code. It would need a copy-constructor
			// that copies or discards this buffer.
			values = new float[frames];
		}
		values[next++] = datapoint;
		if (next >= frames) {
			next = 0;
		} else if (used < frames) {
			used++;
		}
	}

	float avg() {
		float accum = 0.0f;
		for (uint32_t i = 0; i < used; i++) {
			accum += values[i];
		}
		return accum / float(used);
	}

	float max() {
		float maxval = 0.0f; // NOTE: all datapoints should be positive
		for (uint32_t i = 0; i < used; i++) {
			if (values[i] > maxval) {
				maxval = values[i];
			}
		}
		return maxval;
	}
};
