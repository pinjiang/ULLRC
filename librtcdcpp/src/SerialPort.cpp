#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

int open_port(std::string serialPortName)
{
	int fd;
	//O_NONBLOCK设置为非阻塞模式，在read时不会阻塞住，在读的时候将read放在while循环中，下一节篇文档将详细讲解阻塞和非阻塞
	fd = open(serialPortName, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd == -1)
	{
		perror("Can't open serial port: " + serialPortName);
	}
	return fd;
}

int set_opt(int fd, int nSpeed, int nBits, char nEvent, int nStop)
{
	struct termios newtio, oldtio;
	/*保存测试现有串口参数设置，在这里如果串口号等出错，会有相关的出错信息*/
	if (tcgetattr(fd, &oldtio) != 0) {
		perror("SetupSerial 1");
		printf("tcgetattr(fd, &oldtio) -> %d\n", tcgetattr(fd, &oldtio));
		return -1;
	}
	bzero(&newtio, sizeof(newtio));
	/*步骤一，设置字符大小*/
	newtio.c_cflag |= CLOCAL | CREAD;
	newtio.c_cflag &= ~CSIZE;
	/*设置停止位*/
	switch (nBits)
	{
	case 7:
		newtio.c_cflag |= CS7;
		break;
	case 8:
		newtio.c_cflag |= CS8;
		break;
	}
	/*设置奇偶校验位*/
	switch (nEvent)
	{
	case 'o':
	case 'O': //奇数
		newtio.c_cflag |= PARENB;
		newtio.c_cflag |= PARODD;
		newtio.c_iflag |= (INPCK | ISTRIP);
		break;
	case 'e':
	case 'E': //偶数
		newtio.c_iflag |= (INPCK | ISTRIP);
		newtio.c_cflag |= PARENB;
		newtio.c_cflag &= ~PARODD;
		break;
	case 'n':
	case 'N': //无奇偶校验位
		newtio.c_cflag &= ~PARENB;
		break;
	default:
		break;
	}
	/*设置波特率*/
	switch (nSpeed)
	{
	case 2400:
		cfsetispeed(&newtio, B2400);
		cfsetospeed(&newtio, B2400);
		break;
	case 4800:
		cfsetispeed(&newtio, B4800);
		cfsetospeed(&newtio, B4800);
		break;
	case 9600:
		cfsetispeed(&newtio, B9600);
		cfsetospeed(&newtio, B9600);
		break;
	case 115200:
		cfsetispeed(&newtio, B115200);
		cfsetospeed(&newtio, B115200);
		break;
	case 460800:
		cfsetispeed(&newtio, B460800);
		cfsetospeed(&newtio, B460800);
		break;
	default:
		cfsetispeed(&newtio, B9600);
		cfsetospeed(&newtio, B9600);
		break;
	}
	/*设置停止位*/
	if (nStop == 1)
		newtio.c_cflag &= ~CSTOPB;
	else if (nStop == 2)
		newtio.c_cflag |= CSTOPB;
	/*设置等待时间和最小接收字符*/
	newtio.c_cc[VTIME] = 0;
	newtio.c_cc[VMIN] = 0;
	/*处理未接收字符*/
	tcflush(fd, TCIFLUSH);
	/*激活新配置*/
	if ((tcsetattr(fd, TCSANOW, &newtio)) != 0)
	{
		perror("error occurs when setting serial port");
		return -1;
	}
	printf("serial port set done!\n");
	return 0;
}

ssize_t read_port(int fd, void *buf, size_t count) {
	ssize_t n = read(fd, buf, count);
	return n;
}

ssize_t write_port(int fd, const void *buf, size_t count) {
	ssize_t n = write(fd, buf, count);
	return n;
}

int close_port(int fd) {
	return close(fd);
}

int main（void） {
	std::cout << "hello world" << std::endl;
	return 0;
}