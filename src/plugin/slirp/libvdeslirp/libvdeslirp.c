/*
 * VDE - libvdeslirp: libslirp made easy peasy for Linux
 * Copyright (C) 2019 Renzo Davoli VirtualSquare
 * thanks to Simone Fabbri for early prototype testing
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <libvdeslirp.h>
#include <_config.h>

#include <slirp/libslirp.h>

//#define FUTURE_SLIRP_fWD_FEATURES
#define JUMBOMTU 9014

#define LIBSLIRP_POLLFD_SIZE_INCREASE 16
#define APPSIDE 0
#define DAEMONSIDE 1

struct vdeslirp_timer {
  struct vdeslirp_timer *next;
  uint64_t expire_time;
	SlirpTimerCb handler;
  void *opaque;
};

struct vdeslirp {
	Slirp *slirp;
	pthread_t daemon;
	int channel[2];
	int pfd_len;
  int pfd_size;
  struct pollfd *pfd;
	struct vdeslirp_timer *timer_head;
};

static ssize_t vdeslirp_output(const void *buf, size_t len, void *opaque);
static void vdeslirp_guest_error(const char *msg, void *opaque);
static int64_t vdeslirp_clock_get_ns(void *opaque);
static void *vdeslirp_timer_new(SlirpTimerCb cb, void *cb_opaque, void *opaque);
static void vdeslirp_timer_free(void *timer, void *opaque);
static void vdeslirp_timer_mod(void *timer, int64_t expire_time, void *opaque);
static void vdeslirp_register_poll_fd(int fd, void *opaque);
static void vdeslirp_unregister_poll_fd(int fd, void *opaque);
static void vdeslirp_notify(void *opaque);

struct SlirpCb callbacks = {
	.send_packet = vdeslirp_output,
	.guest_error = vdeslirp_guest_error,
	.clock_get_ns = vdeslirp_clock_get_ns,
	.timer_new = vdeslirp_timer_new,
	.timer_free = vdeslirp_timer_free,
	.timer_mod = vdeslirp_timer_mod,
	.register_poll_fd = vdeslirp_register_poll_fd,
	.unregister_poll_fd = vdeslirp_unregister_poll_fd,
	.notify = vdeslirp_notify,
};

#define SLIRP_ADD_FWD 0x11
#define SLIRP_DEL_FWD 0x12
#define SLIRP_ADD_UNIXFWD 0x21
#define SLIRP_DEL_UNIXFWD 0x22
#define SLIRP_ADD_EXEC 0x31
#define SLIRP_DEL_EXEC 0x32

struct slirp_request {
  int tag;
  int pipefd[2];
  int intarg;
  const void *ptrarg;
  void *host_addr;
  int host_port;
  void *guest_addr;
  int guest_port;
};

static void vdeslirp_guest_error(const char *msg, void *opaque){
	(void) opaque;
	fprintf(stderr, "vdeslirp: %s\n", msg);
}

/* FWD MANAGEMENT DAEMON SIDE */

static void slirp_do_req(Slirp *slirp, struct slirp_request *preq) {
	int rval;
	struct in_addr *host_addr = preq->host_addr;
	struct in_addr *guest_addr = preq->guest_addr;
	switch (preq->tag) {
		case SLIRP_ADD_FWD:
			rval = slirp_add_hostfwd(slirp, preq->intarg,
					*host_addr, preq->host_port,
					*guest_addr, preq->guest_port);
			break;
		case SLIRP_DEL_FWD:
			rval = slirp_remove_hostfwd(slirp, preq->intarg,
					*host_addr, preq->host_port);
			break;
#ifdef HAS_ADD_UNIX
		/* currently unsupported by libslirp */
		case SLIRP_ADD_UNIXFWD:
			rval = slirp_add_unix(slirp, preq->ptrarg,
					guest_addr, preq->guest_port);
			break;
#else
#ifdef HAS_ADD_EXEC
		/* warkaround */
		case SLIRP_ADD_UNIXFWD:
			{
				size_t cmdlen = strlen(preq->ptrarg) + 8;
				char cmd[cmdlen];
				snprintf(cmd, cmdlen, "nc -UN %s", (const char *) preq->ptrarg);
				rval = slirp_add_exec(slirp, cmd,
						guest_addr, preq->guest_port);
			}
			break;
#endif
#endif
#ifdef HAS_ADD_EXEC
		case SLIRP_ADD_EXEC:
			rval = slirp_add_exec(slirp, preq->ptrarg,
					guest_addr, preq->guest_port);
			break;
#endif
#ifdef HAS_REMOVE_GUESTFWD
		case SLIRP_DEL_UNIXFWD:
		case SLIRP_DEL_EXEC:
			rval = slirp_remove_guestfwd(slirp,
					*guest_addr, preq->guest_port);
			break;
#endif
		default:
			rval = -ENOSYS;
	}
	rval = write(preq->pipefd[DAEMONSIDE],&rval,sizeof(rval));
}


/* FWD MANAGEMENT APP SIDE */
static int slirp_send_req(struct vdeslirp *slirp, struct slirp_request *preq) {
	int rval;
	if (pipe(preq->pipefd) < 0) {
		return -1;
	} else {
		rval = write(slirp->channel[APPSIDE], &preq, sizeof(struct slirp_request *));
		if (rval >= 0)
			rval =  read(preq->pipefd[APPSIDE],&rval,sizeof(rval));
		close(preq->pipefd[0]);
		close(preq->pipefd[1]);
		return rval < 0 ? (errno = -rval, -1) : 0;
	}
}

int vdeslirp_add_fwd(struct vdeslirp *slirp, int is_udp,
		struct in_addr host_addr, int host_port,
		struct in_addr guest_addr, int guest_port) {
	struct slirp_request req = {
		.tag = SLIRP_ADD_FWD,
		.intarg = is_udp,
		.host_addr = &host_addr,
		.host_port = host_port,
		.guest_addr = &guest_addr,
		.guest_port = guest_port };
	return slirp_send_req(slirp, &req);
}

int vdeslirp_remove_fwd(struct vdeslirp *slirp, int is_udp,
		struct in_addr host_addr, int host_port) {
	struct slirp_request req = {
		.tag = SLIRP_DEL_FWD,
		.intarg = is_udp,
		.host_addr = &host_addr,
		.host_port = host_port};
	return slirp_send_req(slirp, &req);
}

int vdeslirp_add_unixfwd(struct vdeslirp *slirp, char *path,
		struct in_addr *guest_addr, int guest_port) {
	struct slirp_request req = {
		.tag = SLIRP_ADD_UNIXFWD,
		.guest_addr = guest_addr,
		.guest_port = guest_port,
		.ptrarg = path };
	return slirp_send_req(slirp, &req);
}

int vdeslirp_remove_unixfwd(struct vdeslirp *slirp,
		struct in_addr guest_addr, int guest_port) {
	struct slirp_request req = {
		.tag = SLIRP_DEL_UNIXFWD,
		.guest_addr = &guest_addr,
		.guest_port = guest_port};
	return slirp_send_req(slirp, &req);
}

int vdeslirp_add_cmdexec(struct vdeslirp *slirp, const char *cmdline,
		struct in_addr *guest_addr, int guest_port) {
	 struct slirp_request req = {
    .tag = SLIRP_ADD_EXEC,
    .ptrarg = cmdline,
    .guest_addr = guest_addr,
    .guest_port = guest_port };
  return slirp_send_req(slirp, &req);
}

int vdeslirp_remove_cmdexec(struct vdeslirp *slirp,
		struct in_addr guest_addr, int guest_port) {
	struct slirp_request req = {
		.tag = SLIRP_DEL_EXEC,
		.guest_addr = &guest_addr,
		.guest_port = guest_port};
	return slirp_send_req(slirp, &req);
}

/* TIMER MANAGEMENT */
static int64_t vdeslirp_clock_get_ns(void *opaque){
	(void) opaque;
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static void *vdeslirp_timer_new(SlirpTimerCb cb, void *cb_opaque, void *opaque){
	struct vdeslirp *slirp = opaque;
	struct vdeslirp_timer *qt = malloc(sizeof(*qt));
	if (qt) {
		qt->next = slirp->timer_head;
		qt->expire_time = -1;
		qt->handler = cb;
		qt->opaque = cb_opaque;
		slirp->timer_head=qt;
	}
	return qt;
}

static void vdeslirp_timer_free(void *timer, void *opaque){
	struct vdeslirp *slirp = opaque;
	struct vdeslirp_timer *qt = timer;
	struct vdeslirp_timer **scan;
	for (scan =  &slirp->timer_head; *scan != NULL && *scan != qt; scan = &((*scan) ->next))
		;
	if (*scan) {
		*scan = qt->next;
		free(qt);
	}
}

static void vdeslirp_timer_mod(void *timer, int64_t expire_time, void *opaque){
	(void) opaque;
	struct vdeslirp_timer *qt = timer;
	qt->expire_time = expire_time;
}

static void update_ra_timeout(uint32_t *timeout, void *opaque) {
	struct vdeslirp *slirp = opaque;
	struct vdeslirp_timer *qt;
	int64_t now_ms = vdeslirp_clock_get_ns(opaque) / 1000000;
	for (qt = slirp->timer_head; qt != NULL; qt =  qt->next) {
		if (qt->expire_time != -1UL) {
			int64_t diff = qt->expire_time - now_ms;
			if (diff < 0) diff = 0;
			if (diff < *timeout) *timeout = diff;
		}
	}
}

static void check_ra_timeout(void *opaque) {
	struct vdeslirp *slirp = opaque;
	struct vdeslirp_timer *qt;
	int64_t now_ms = vdeslirp_clock_get_ns(opaque) / 1000000;
	for (qt = slirp->timer_head; qt != NULL; qt =  qt->next) {
		if (qt->expire_time != -1UL) {
			int64_t diff = qt->expire_time - now_ms;
			if (diff <= 0) {
				qt->expire_time = -1UL;
				qt->handler(qt->opaque);
			}
		}
	}
}


/* POLL MANAGEMENT */
static int vdeslirp_slirp_to_poll(int events) {
	int ret = 0;
	if (events & SLIRP_POLL_IN) ret |= POLLIN;
	if (events & SLIRP_POLL_OUT) ret |= POLLOUT;
	if (events & SLIRP_POLL_PRI) ret |= POLLPRI;
	if (events & SLIRP_POLL_ERR) ret |= POLLERR;
	if (events & SLIRP_POLL_HUP) ret |= POLLHUP;
	return ret;
}

static int vdeslirp_poll_to_slirp(int events) {
	int ret = 0;
	if (events & POLLIN) ret |= SLIRP_POLL_IN;
	if (events & POLLOUT) ret |= SLIRP_POLL_OUT;
	if (events & POLLPRI) ret |= SLIRP_POLL_PRI;
	if (events & POLLERR) ret |= SLIRP_POLL_ERR;
	if (events & POLLHUP) ret |= SLIRP_POLL_HUP;
	return ret;
}

static int vdeslirp_add_poll(int fd, int events, void *opaque) {
	struct vdeslirp *slirp = opaque;
	if (slirp->pfd_len >= slirp->pfd_size) {
		int newsize = slirp->pfd_size + LIBSLIRP_POLLFD_SIZE_INCREASE;
		struct pollfd *new = realloc(slirp->pfd,  newsize);
		if (new) {
			slirp->pfd = new;
			slirp->pfd_size = newsize;
		}
	}
	if (slirp->pfd_len < slirp->pfd_size) {
		int idx = slirp->pfd_len++;
		slirp->pfd[idx].fd = fd;
		slirp->pfd[idx].events = vdeslirp_slirp_to_poll(events);
		return idx;
	} else
		return -1;
}

static int vdeslirp_get_revents(int idx, void *opaque) {
	struct vdeslirp *slirp = opaque;
	return vdeslirp_poll_to_slirp(slirp->pfd[idx].revents);
}

static void vdeslirp_register_poll_fd(int fd, void *opaque){
	(void) fd;
	(void) opaque;
}

static void vdeslirp_unregister_poll_fd(int fd, void *opaque){
	(void) fd;
	(void) opaque;
}

static void vdeslirp_notify(void *opaque){
	(void) opaque;
}

static ssize_t vdeslirp_output(const void *buf, size_t len, void *opaque) {
	struct vdeslirp *slirp = opaque;
	return write(slirp->channel[DAEMONSIDE], buf, len);
}

static void vdeslirp_cleanup(struct vdeslirp *slirp) {
  slirp_cleanup(slirp->slirp);
  while(slirp->timer_head)
    vdeslirp_timer_free(slirp->timer_head, slirp);
  close(slirp->channel[DAEMONSIDE]);
  free(slirp->pfd);
  free(slirp);
}

static void *slirp_daemon(void *opaque) {
	struct vdeslirp *slirp = opaque;
	vdeslirp_add_poll(slirp->channel[DAEMONSIDE], SLIRP_POLL_IN | SLIRP_POLL_HUP, slirp);
	for(;;) {
		int pollout;
		uint32_t timeout = -1;
		slirp->pfd_len = 1;
		slirp_pollfds_fill(slirp->slirp, &timeout, vdeslirp_add_poll, slirp);
		update_ra_timeout(&timeout, slirp);
		pollout = poll(slirp->pfd, slirp->pfd_len, timeout);
		if (slirp->pfd[0].revents) {
			uint8_t buf[JUMBOMTU];
			size_t len = read(slirp->channel[DAEMONSIDE], buf, JUMBOMTU);
			if (len <= 0)
				break;
			if (len == sizeof(struct slirp_request *))
				slirp_do_req(slirp->slirp, *((struct slirp_request **) buf));
			else
				slirp_input(slirp->slirp, buf, len);
		}
		slirp_pollfds_poll(slirp->slirp, (pollout <= 0), vdeslirp_get_revents, slirp);
		check_ra_timeout(slirp);
	}
	return NULL;
}

static void *memmask(void *mask, size_t len, size_t prefix) {
	uint8_t *bufmask = mask;
	size_t i;
	for (i = 0; (i < len) & (prefix >= 8); i++, prefix -= 8)
		bufmask[i] = 0xff;
	for (; (i < len) & (prefix > 0); i++, prefix -= 8)
		bufmask[i] = ~((1 << (8 - prefix)) - 1);
	for (; i < len; i++, prefix -= 8)
		bufmask[i] = 0x0;
	return bufmask;
}

static void *memmaskcpy(void *dest, const void *src, const void *mask, size_t n) {
	uint8_t *bufdest = dest;
	const uint8_t *bufsrc = src;
	const uint8_t *bufmask = mask;
	size_t i;
	for (i = 0; i < n; i++)
		bufdest[i] = (bufsrc[i] & bufmask[i]) | (bufdest[i] & ~bufmask[i]);
	return bufdest;
}

void vdeslirp_init(SlirpConfig *cfg, int flags) {
	memset(cfg, 0, sizeof(*cfg));
	cfg->version = 1;

	if (flags & VDE_INIT_DEFAULT) {
		cfg->restricted = 0;
		cfg->in_enabled = 1;
		inet_pton(AF_INET,"10.0.2.0", &(cfg->vnetwork));
		inet_pton(AF_INET,"255.255.255.0", &(cfg->vnetmask));
		inet_pton(AF_INET,"10.0.2.2", &(cfg->vhost));
		cfg->in6_enabled = 1;
		inet_pton(AF_INET6, "fd00::", &cfg->vprefix_addr6);
		cfg->vprefix_len = 64;
		inet_pton(AF_INET6, "fd00::2", &cfg->vhost6);
		cfg->vhostname = "slirp";
		cfg->tftp_server_name = NULL;
		cfg->tftp_path = NULL;
		cfg->bootfile = NULL;
		inet_pton(AF_INET,"10.0.2.15", &(cfg->vdhcp_start));
		inet_pton(AF_INET,"10.0.2.3", &(cfg->vnameserver));
		inet_pton(AF_INET6, "fd00::3", &cfg->vnameserver6);
		cfg->vdnssearch = NULL;
		cfg->vdomainname = NULL;
		cfg->if_mtu = 0; // IF_MTU_DEFAULT
		cfg->if_mru = 0; // IF_MTU_DEFAULT

		cfg->disable_host_loopback = 0;
	}
}

void vdeslirp_setvprefix(SlirpConfig *cfg, int prefix) {
	static const struct in_addr inaddr_any = {.s_addr = INADDR_ANY};
	memmask(&cfg->vnetmask, sizeof(struct in_addr), prefix);
	cfg->vnetwork = inaddr_any;
	memmaskcpy(&cfg->vnetwork, &cfg->vhost, &cfg->vnetmask, sizeof(struct in_addr));
	if (cfg->vdhcp_start.s_addr != inaddr_any.s_addr)
		memmaskcpy(&cfg->vdhcp_start, &cfg->vhost, &cfg->vnetmask, sizeof(struct in_addr));
	if (cfg->vnameserver.s_addr != inaddr_any.s_addr)
		memmaskcpy(&cfg->vnameserver, &cfg->vhost, &cfg->vnetmask, sizeof(struct in_addr));
}

void vdeslirp_setvprefix6(SlirpConfig *cfg, int prefix6) {
	struct in6_addr vnetmask6;
	cfg->vprefix_len = prefix6;
	memmask(&vnetmask6, sizeof(struct in6_addr), prefix6);
	cfg->vprefix_addr6 = in6addr_any;
	memmaskcpy(&cfg->vprefix_addr6,  &cfg->vhost6, &vnetmask6, sizeof(struct in6_addr));
	if (memcmp(&cfg->vnameserver6, &in6addr_any, sizeof(struct in6_addr)) != 0)
		memmaskcpy(&cfg->vnameserver6,  &cfg->vhost6, &vnetmask6, sizeof(struct in6_addr));
}

struct vdeslirp *vdeslirp_open(SlirpConfig *cfg) {
	struct vdeslirp *slirp = calloc(1, sizeof(struct vdeslirp));
	if (slirp) {
		if (socketpair(AF_LOCAL, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, slirp->channel) < 0)
			goto nosocketpair;
		if ((slirp->slirp = slirp_new(cfg, &callbacks, slirp)) == NULL)
			goto noslirp;
		if (pthread_create(&slirp->daemon, NULL, slirp_daemon, slirp) != 0)
			goto nopthread;
	}
	return slirp;
nopthread:
	slirp_cleanup(slirp->slirp);
noslirp:
	close(slirp->channel[APPSIDE]);
	close(slirp->channel[DAEMONSIDE]);
nosocketpair:
	free(slirp);
	return NULL;
}

ssize_t vdeslirp_send(struct vdeslirp *slirp, const void *buf, size_t count) {
	if (count > sizeof(void *)) {
		return write(slirp->channel[APPSIDE], buf, count);
	} else
		return errno = EINVAL, -1;
}

ssize_t vdeslirp_recv(struct vdeslirp *slirp, void *buf, size_t count) {
	return read(slirp->channel[APPSIDE], buf, count);
}

int vdeslirp_fd(struct vdeslirp *slirp) {
	return slirp->channel[APPSIDE];
}

int vdeslirp_close(struct vdeslirp *slirp) {
	void *retval;
	int rv = close(slirp->channel[APPSIDE]);
	pthread_join(slirp->daemon, &retval);
	vdeslirp_cleanup(slirp);
	return rv;
}

