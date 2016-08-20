#define VERSION_MAJOR	0
#define VERSION_MINOR	0.1

/* socket read buffer size */
#define SOCKBUFR_SIZE	1024

/* tty read buffer size */
#define TTYBUFR_SIZE	1024

/* line (!char) mode line-length */
#define LINEBUF_SIZE	256

/* command mode line-length (w/o null) */
#define CMDBUF_MAX	255

typedef unsigned char uchar;

#if defined(__GLIBC__) || defined(SVR4)
#define HAVE_GRANTPT
#endif

