#include <ios>
#include <iostream>
#include <stdlib.h>
#include "autolabor/autolabor.h"

Autolabor::Autolabor() {
	pulse_per_cycle = REDUCTION_RATIO * ENCODER_RESOLUTION / (PI * WHEEL_DIAMETER * PID_RATE);
	msg_seq = 0;
}

Autolabor::~Autolabor() {}

void Autolabor::Check(uint8_t* data, size_t len, uint8_t& dest) {
	dest = 0x00;
	for (int i = 0; i < len; i++) {
		dest = dest ^ *(data + i);
	}
}

uint8_t* Autolabor::CreateSpeedCmd(double linear_speed, double angular_speed) {
	double left_d, right_d, radio;
	short left, right;

	left_d = (linear_speed - MODEL_PARAM / 2 * angular_speed) * pulse_per_cycle;
	right_d = (linear_speed + MODEL_PARAM / 2 * angular_speed) * pulse_per_cycle;

	radio = std::max(std::max(abs(left_d), abs(right_d)) / MAXIMUM_ENCODING, 1.0);

	left = static_cast<short>(left_d / radio);
	right = static_cast<short>(right_d / radio);

	uint8_t* data = new uint8_t[SPEED_CMD_LENGTH];
	data[0] = 0x55;
	data[1] = 0xAA;
	data[2] = 0x09;
	data[3] = msg_seq++;
	data[4] = 0x01;
	data[5] = (left >> 8) & 0xff;
	data[6] = left & 0xff;
	data[7] = (right >> 8) & 0xff;
	data[8] = right & 0xff;
	data[9] = 0x00;
	data[10] = 0x00;
	data[11] = 0x00;
	data[12] = 0x00;
	data[13] = 0x00;
	Check(data, SPEED_CMD_LENGTH - 1, data[SPEED_CMD_LENGTH - 1]);

	return data;
}

uint8_t* Autolabor::CreateResetCmd() {
	uint8_t* data = new uint8_t[RESET_CMD_LENGTH];
	data[0] = 0x55;
	data[1] = 0xAA;
	data[2] = 0x02;
	data[3] = msg_seq++;
	data[4] = 0x05;
	data[5] = 0x00;
	data[6] = 0x00;
	Check(data, RESET_CMD_LENGTH - 1, data[RESET_CMD_LENGTH - 1]);

	return data;
}