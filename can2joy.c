/* This file is part of can2joy.
 *
 * can2joy is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License 2 as
 * published by the Free Software Foundation.

 * can2joy is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with can2joy. If not, see <http://www.gnu.org/licenses/>.
 */
#include <fcntl.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))


static int init_can(char *ifname)
{
	struct sockaddr_can addr;
	struct ifreq ifr;
	int fd_can;

	/* Open CAN socket */
	fd_can = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	if (fd_can < 0) {
		perror("socket");
		return fd_can;
	}

	/* Get CAN interfaces's index by name */
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
	if (ioctl(fd_can, SIOCGIFINDEX, &ifr) < 0) {
		perror("SIOCGIFINDEX");
		return -1;
	}

	/* Bind CAN interface by index */
	memset(&addr, 0, sizeof(addr));
	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	if (bind(fd_can, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		return -1;
	}

	return fd_can;
}


static int init_uinput()
{
	struct uinput_user_dev uidev;
	int fd_uinput;

	/* Open uinput */
	fd_uinput = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
	if(fd_uinput < 0) {
		perror("open (/dev/uinput)");
		return fd_uinput;
	}

	/* Register push buttons A, B */
	ioctl(fd_uinput, UI_SET_EVBIT, EV_KEY);
	ioctl(fd_uinput, UI_SET_KEYBIT, BTN_TRIGGER);
	ioctl(fd_uinput, UI_SET_KEYBIT, BTN_THUMB);

	/* Register absolute axes X, Y */
	ioctl(fd_uinput, UI_SET_EVBIT, EV_ABS);
	ioctl(fd_uinput, UI_SET_ABSBIT, ABS_X);
	ioctl(fd_uinput, UI_SET_ABSBIT, ABS_Y);

	/* Create a virtual input device ID */
	memset(&uidev, 0, sizeof(uidev));
	strncpy(uidev.name, "can2joy", UINPUT_MAX_NAME_SIZE);
	uidev.id.bustype = BUS_USB;
	uidev.id.vendor  = 0x1234;
	uidev.id.product = 0xfedc;
	uidev.id.version = 1;
	write(fd_uinput, &uidev, sizeof(uidev));

	/* Set min/max values for axes */
	uidev.absmin[ABS_X] = 255;
	uidev.absmax[ABS_X] = -256;
	uidev.absmin[ABS_Y] = -1;
	uidev.absmax[ABS_Y] = 1;
	write(fd_uinput, &uidev, sizeof(uidev));

	/* Let uinput rip */
	ioctl(fd_uinput, UI_DEV_CREATE);

	return fd_uinput;
}



static void receive_one(int fd_can, int fd_uinput)
{
	struct can_frame frm;
	struct sockaddr_can addr;
	int ret;
	socklen_t len;
	struct input_event ev;
	short wheel_pos;

	memset(&ev, 0, sizeof(ev));

	ret = recvfrom(fd_can, &frm, sizeof(struct can_frame), 0,
	(struct sockaddr *)&addr, &len);
	if (ret < 0) {
	perror("recvfrom");
	exit(1);
	}

	switch(frm.can_id) {
	case 0x35b:
		/* Check clutch and brake pedal.
		 * We only read one bit for each, so don't expect fine
		 * control of your simulated Lamborghini!
		 */
		ev.type = EV_KEY;

		/* Brake sets bits 0 and 1 (value 3) in byte 4 */
		ev.code = BTN_TRIGGER;
		ev.value = !(!(frm.data[4] & 3));
		write(fd_uinput, &ev, sizeof(ev));

		/* Clutch sets bit 2 (value 4) in byte 6 */
		ev.code = BTN_THUMB;
		ev.value = !(!(frm.data[6] & 4));
		write(fd_uinput, &ev, sizeof(ev));
		break;

	case 0x3c3:
		/* Translate absolute wheel position.
		 * It's a signed 16-bit integer in the first two bytes
		 * of a 0x3c3 frame, indicating how far it is turned.
		 * Zero means we're driving straight.
		 * The next two bytes are the angular speed of the wheel
		 * being turned, but we don't care about that here.
		 */
		wheel_pos = *((short*)(&frm.data[0]));
		printf("Wheel position: %i\n", wheel_pos);

		/* Clamp the wheel position around the zero point.
		 * This is to avoid excessive wear of the wheels of the
		 * parked car as well as of the whole steering mechanism.
		 */
		if (wheel_pos >= 0) {
			wheel_pos = MIN(255, wheel_pos);
		} else {
			/* The first negative value next to zero is
			 * -32768. So we have to invert this to -1
			 * instead of -32768, -2 instead of -32767, etc.
			 */
			wheel_pos = 0 - (wheel_pos + 32768);
			wheel_pos = MAX(-256, wheel_pos);
		}
		printf("Wheel position (clamped): %i\n", wheel_pos);

		/* Report the change in wheel position as the X axis */
		ev.type = EV_ABS;
		ev.code = ABS_X;
		ev.value = wheel_pos;
		ret = write(fd_uinput, &ev, sizeof(ev));

		/* Report a dummy Y axis for programs that expect it */
		ev.code = ABS_Y;
		ev.value = 0;
		ret = write(fd_uinput, &ev, sizeof(ev));
		break;
	}

	/* Flush the uinput events we just generated */
	ev.type = EV_SYN;
	ev.code = SYN_REPORT;
	ev.value = 0;
	write(fd_uinput, &ev, sizeof(ev));
}




int main(int argc, char **argv)
{
	int fd_can;
	int fd_uinput;

	if (argc < 2) {
		printf("Usage: %s <can-interface>\n", argv[0]);
		printf("\n");
		printf("Example: %s can0\n", argv[0]);
		return 1;
	}


	fd_can = init_can(argv[1]);
	if (fd_can < 0)
		return 1;

	fd_uinput = init_uinput();
	if (fd_uinput < 0)
		return 1;


	for (;;)
		receive_one(fd_can, fd_uinput);


	ioctl(fd_uinput, UI_DEV_DESTROY);

	return 0;
}
