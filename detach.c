#include <stdio.h>
#include <fcntl.h>
#include <sys/vt.h>
#include <sys/kd.h>

unsigned short
detach()
{
	struct vt_stat vts;
	int pid;
	int vt;
	int fd;
	char dev[16];

	if ((fd = open("/dev/tty0", O_RDONLY)) < 0) {
		fprintf(stderr, "Could not open current VT.\n");
		return(0);
	}

	if (ioctl(fd, VT_GETSTATE, &vts) < 0) {
		perror("VT_GETSTATE");
		return(0);
	}

	if (ioctl(fd, VT_OPENQRY, &vt) < 0) {
		perror("VT_OPENWRY");
		return(0);
	}

	if (vt < 1) {
		fprintf(stderr, "No free vts to open\n");
		return(0);
	}

	sprintf(dev, "/dev/tty%d", vt);

	if (ioctl(fd, VT_ACTIVATE, vt) < 0) {
		perror("VT_ACTIVATE");
		return(0);
	}

	if (ioctl(fd, VT_WAITACTIVE, vt) < 0) {
		perror("VT_WAITACTIVE");
		return(0);
	}

	if ((pid = fork()) < 0) {
		perror("fork");
		return(0);
	}

	if (pid) {
		exit(0);
	}

	close(fd);
	close(2);
	close(1);
	close(0);

	(void)open(dev, O_RDWR);
	(void)open(dev, O_RDWR);
	(void)open(dev, O_RDWR);

	(void)setsid();
	return(vts.v_active); /* return old VT. */
}

restore_vt(vt)
unsigned short vt;
{

	if (ioctl(0, VT_ACTIVATE, vt) < 0) {
		perror("VT_ACTIVATE");
		return;
	}

	if (ioctl(0, VT_WAITACTIVE, vt) < 0) {
		perror("VT_WAITACTIVE");
		return;
	}

}
