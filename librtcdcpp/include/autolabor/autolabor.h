#ifndef _AUTOLABOR_H
#define _AUTOLABOR_H

#define PI 3.1415926

class Autolabor
{
public:
	const char* ODOM_FRAME = "odom";
	const char* BASE_FRAME = "base_link";
	const int CONTROL_RATE = 10;
	const int SENSOR_RATE = 5;
	const double REDUCTION_RATIO = 2.5;
	const double ENCODER_RESOLUTION = 1600.0;
	const double WHEEL_DIAMETER = 0.15;
	const double MODEL_PARAM = 0.78;
	const double PID_RATE = 50.0;
	const double MAXIMUM_ENCODING = 32.0;
	const size_t SPEED_CMD_LENGTH = 14;
	const size_t RESET_CMD_LENGTH = 7;
	double pulse_per_cycle;

	Autolabor();

	~Autolabor();

	uint8_t* CreateSpeedCmd(double linear_speed, double angular_speed);

	uint8_t* CreateResetCmd();

private:
	uint8_t msg_seq;

	void Check(uint8_t* data, size_t len, uint8_t& dest);
};

#endif
