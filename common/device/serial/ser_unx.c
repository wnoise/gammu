/* (c) 2002-2005 by Marcin Wiacek */
/* locking device and settings all speeds by Michal Cihar */
/* based on some work from Gnokii (www.gnokii.org)
 * (C) 1999-2000 Hugh Blemings & Pavel Janik ml. (C) 2001-2004 Pawel Kot
 * GNU GPL version 2 or later
 */
/* Due to a problem in the source code management, the names of some of
 * the authors have unfortunately been lost. We do not mean to belittle
 * their efforts and hope they will contact us to see their names
 * properly added to the Copyright notice above.
 * Having published their contributions under the terms of the GNU
 * General Public License (GPL) [version 2], the Copyright of these
 * authors will remain respected by adhering to the license they chose
 * to publish their code under.
 */

#include "../../gsmstate.h"

#ifdef GSM_ENABLE_SERIALDEVICE
#ifndef WIN32
#ifndef DJGPP

#include <sys/file.h>
#include <sys/time.h>
#include <string.h>
#include <termios.h>
#include <errno.h>

#include "../../gsmcomon.h"
#include "ser_unx.h"

#ifndef O_NONBLOCK
#  define O_NONBLOCK  0
#endif

#ifdef __NetBSD__
#  define FNONBLOCK O_NONBLOCK

#  define  B57600   0010001
#  define  B115200  0010002
#  define  B230400  0010003
#  define  B460800  0010004
#  define  B500000  0010005
#  define  B576000  0010006
#  define  B921600  0010007
#  define  B1000000 0010010
#  define  B1152000 0010011
#  define  B1500000 0010012
#  define  B2000000 0010013
#  define  B2500000 0010014
#  define  B3000000 0010015
#  define  B3500000 0010016
#  define  B4000000 0010017
#endif

typedef struct {
	speed_t code;
	int	value;
} baud_record;


static baud_record baud_table[] = {
	{ B50,		50 },
	{ B75,		75 },
	{ B110,		110 },
	{ B134,		134 },
	{ B150,		150 },
	{ B200,		200 },
	{ B300,		300 },
	{ B600,		600 },
	{ B1200,	1200 },
	{ B1800,	1800 },
	{ B2400,	2400 },
	{ B4800,	4800 },
	{ B9600,	9600 },
#ifdef B19200
	{ B19200,	19200 },
#else /* ! defined (B19200) */
#ifdef EXTA
	{ EXTA,		19200 },
#endif /* EXTA */
#endif /* ! defined (B19200) */
#ifdef B38400
	{ B38400,	38400 },
#else /* ! defined (B38400) */
#ifdef EXTB
	{ EXTB,		38400 },
#endif /* EXTB */
#endif /* ! defined (B38400) */
#ifdef B57600
	{ B57600,	57600 },
#endif
#ifdef B76800
	{ B76800,	76800 },
#endif
#ifdef B115200
	{ B115200,	115200 },
#endif
#ifdef B230400
	{ B230400,	230400 },
#else
#ifdef _B230400
	{ _B230400,	230400 },
#endif /* _B230400 */
#endif /* ! defined (B230400) */
#ifdef B460800
	{ B460800,	460800 },
#else
#ifdef _B460800
	{ _B460800,	460800 },
#endif /* _B460800 */
#endif /* ! defined (B460800) */
#ifdef B500000
	{ B500000,	500000 },
#endif
#ifdef B576000
	{ B576000,	576000 },
#endif
#ifdef B921600
	{ B921600,	921600 },
#endif
#ifdef B1000000
	{ B1000000,	1000000 },
#endif
#ifdef B1152000
	{ B1152000,	1152000 },
#endif
#ifdef B1500000
	{ B1500000,	1500000 },
#endif
#ifdef B2000000
	{ B2000000,	2000000 },
#endif
#ifdef B2500000
	{ B2500000,	2500000 },
#endif
#ifdef B3000000
	{ B3000000,	3000000 },
#endif
#ifdef B3500000
	{ B3500000,	3500000 },
#endif
#ifdef B4000000
	{ B4000000,	4000000 },
#endif
	{ B0,		0 },
};

static GSM_Error serial_close(GSM_StateMachine *s)
{
	GSM_Device_SerialData *d = &s->Device.Data.Serial;

	/* Restores old settings */
	tcsetattr(d->hPhone, TCSANOW, &d->old_settings);

	/* Closes device */
	close(d->hPhone);

	return ERR_NONE;
}

static GSM_Error serial_open (GSM_StateMachine *s)
{
	GSM_Device_SerialData   *d = &s->Device.Data.Serial;
	struct termios	  	t;
	int			i;

	/* O_NONBLOCK MUST is required to avoid waiting for DCD */
	d->hPhone = open(s->CurrentConfig->Device, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (d->hPhone < 0) {
		i = errno;
		GSM_OSErrorInfo(s,"open in serial_open");
		if (i ==  2) return ERR_DEVICENOTEXIST;		//no such file or directory
		if (i == 13) return ERR_DEVICENOPERMISSION;	//permission denied
		return ERR_DEVICEOPENERROR;
	}

#ifdef TIOCEXCL
	/* open() calls from other applications shall fail now */
	ioctl(d->hPhone, TIOCEXCL, (char *) 0);
#endif

	if (tcgetattr(d->hPhone, &d->old_settings) == -1) {
		close(d->hPhone);
		GSM_OSErrorInfo(s,"tcgetattr in serial_open");
		return ERR_DEVICEREADERROR;
    	}

    	if (tcflush(d->hPhone, TCIOFLUSH) == -1) {
		serial_close(s);
		GSM_OSErrorInfo(s,"tcflush in serial_open");
		return ERR_DEVICEOPENERROR;
    	}

    	memcpy(&t, &d->old_settings, sizeof(struct termios));

	/* Opening without parity */
    	t.c_iflag       = IGNPAR;
    	t.c_oflag       = 0;
    	/* disconnect line, 8 bits, enable receiver,
     	 * ignore modem lines,lower modem line after disconnect
     	 */
    	t.c_cflag       = B0 | CS8 | CREAD | CLOCAL | HUPCL;
    	/* enable hardware (RTS/CTS) flow control (NON POSIX) 	*/
    	/* t.c_cflag 	|= CRTSCTS;			  	*/
    	t.c_lflag       = 0;
    	t.c_cc[VMIN]    = 1;
    	t.c_cc[VTIME]   = 0;

    	if (tcsetattr(d->hPhone, TCSANOW, &t) == -1) {
		serial_close(s);
		GSM_OSErrorInfo(s,"tcsetattr in serial_open");
		return ERR_DEVICEOPENERROR;
    	}

    	/* Making file descriptor asynchronous. */
    	if (fcntl(d->hPhone, F_SETFL, FASYNC | FNONBLOCK) == -1) {
		serial_close(s);
		GSM_OSErrorInfo(s,"fcntl in serial_open");
		return ERR_DEVICEOPENERROR;
    	}

    	return ERR_NONE;
}

static GSM_Error serial_setparity(GSM_StateMachine *s, bool parity)
{
    	GSM_Device_SerialData   *d = &s->Device.Data.Serial;
    	struct termios	  	t;

    	if (tcgetattr(d->hPhone, &t)) {
		GSM_OSErrorInfo(s,"tcgetattr in serial_setparity");
		return ERR_DEVICEREADERROR;
    	}

    	if (parity) {
		t.c_cflag |= (PARENB | PARODD);
		t.c_iflag = 0;
    	} else {
		t.c_iflag = IGNPAR;
    	}

    	if (tcsetattr(d->hPhone, TCSANOW, &t) == -1){
		serial_close(s);
		GSM_OSErrorInfo(s,"tcsetattr in serial_setparity");
		return ERR_DEVICEPARITYERROR;
    	}

    	return ERR_NONE;
}

static GSM_Error serial_setdtrrts(GSM_StateMachine *s, bool dtr, bool rts)
{
    	GSM_Device_SerialData   *d = &s->Device.Data.Serial;
    	struct termios	  	t;
    	unsigned int	    	flags;

    	if (tcgetattr(d->hPhone, &t)) {
		GSM_OSErrorInfo(s,"tcgetattr in serial_setdtrrts");
		return ERR_DEVICEREADERROR;
    	}

#ifdef CRTSCTS
    	/* Disabling hardware flow control */
    	t.c_cflag &= ~CRTSCTS;
#endif

    	if (tcsetattr(d->hPhone, TCSANOW, &t) == -1) {
		serial_close(s);
		GSM_OSErrorInfo(s,"tcsetattr in serial_setdtrrts");
		return ERR_DEVICEDTRRTSERROR;
    	}

    	flags = TIOCM_DTR;
    	if (dtr) {
		ioctl(d->hPhone, TIOCMBIS, &flags);
	} else {
		ioctl(d->hPhone, TIOCMBIC, &flags);
	}

    	flags = TIOCM_RTS;
    	if (rts) {
		ioctl(d->hPhone, TIOCMBIS, &flags);
	} else {
		ioctl(d->hPhone, TIOCMBIC, &flags);
	}

    	flags = 0;
    	ioctl(d->hPhone, TIOCMGET, &flags);

    	dbgprintf("Serial device:");
    	dbgprintf(" DTR is %s",       flags&TIOCM_DTR?"up":"down");
    	dbgprintf(", RTS is %s",      flags&TIOCM_RTS?"up":"down");
    	dbgprintf(", CAR is %s",      flags&TIOCM_CAR?"up":"down");
    	dbgprintf(", CTS is %s\n",    flags&TIOCM_CTS?"up":"down");
    	if (((flags&TIOCM_DTR)==TIOCM_DTR) != dtr) return ERR_DEVICEDTRRTSERROR;
    	if (((flags&TIOCM_RTS)==TIOCM_RTS) != rts) return ERR_DEVICEDTRRTSERROR;

    	return ERR_NONE;
}

static GSM_Error serial_setspeed(GSM_StateMachine *s, int speed)
{
    	GSM_Device_SerialData 	*d = &s->Device.Data.Serial;
    	struct termios  	t;
	baud_record		*curr = baud_table;


    	if (tcgetattr(d->hPhone, &t)) {
		GSM_OSErrorInfo(s,"tcgetattr in serial_setspeed");
		return ERR_DEVICEREADERROR;
    	}

	while (curr->value != speed) {
		curr++;
		/* This is how we make default fallback */
		if (curr->value == 0) speed = 19200;
	}

    	smprintf(s, "Setting speed to %d\n", curr->value);

    	cfsetispeed(&t, curr->code);
    	cfsetospeed(&t, curr->code);

    	if (tcsetattr(d->hPhone, TCSADRAIN, &t) == -1) {
		serial_close(s);
		GSM_OSErrorInfo(s,"tcsetattr in serial_setspeed");
		return ERR_DEVICECHANGESPEEDERROR;
    	}

    	return ERR_NONE;
}

static int serial_read(GSM_StateMachine *s, void *buf, size_t nbytes)
{
    	GSM_Device_SerialData 		*d = &s->Device.Data.Serial;
    	struct timeval  		timeout2;
    	fd_set	  			readfds;
    	int	     			actual = 0;

    	FD_ZERO(&readfds);
    	FD_SET(d->hPhone, &readfds);

    	timeout2.tv_sec     = 0;
//    	timeout2.tv_usec    = 1;
    	timeout2.tv_usec    = 50000;

    	if (select(d->hPhone+1, &readfds, NULL, NULL, &timeout2)) {
		actual = read(d->hPhone, buf, nbytes);
		if (actual == -1) GSM_OSErrorInfo(s,"serial_read");
    	}
    	return actual;
}

static int serial_write(GSM_StateMachine *s, void *buf, size_t nbytes)
{
    	GSM_Device_SerialData   *d = &s->Device.Data.Serial;
    	int		     	ret;
    	size_t		  	actual = 0;

    	do {
		ret = write(d->hPhone, (unsigned char *)buf, nbytes - actual);
		if (ret < 0 && errno == EAGAIN) continue;
		if (ret < 0) {
	    		if (actual != nbytes) GSM_OSErrorInfo(s,"serial_write");
	    		return actual;
		}
		actual  += ret;
		buf     += ret;
    		if (s->ConnectionType == GCT_FBUS2PL2303) my_sleep(1);
    	} while (actual < nbytes);
    	return actual;
}

GSM_Device_Functions SerialDevice = {
    	serial_open,
    	serial_close,
    	serial_setparity,
    	serial_setdtrrts,
    	serial_setspeed,
    	serial_read,
    	serial_write
};

#endif
#endif
#endif

/* How should editor hadle tabs in this file? Add editor commands here.
 * vim: noexpandtab sw=8 ts=8 sts=8:
 */
