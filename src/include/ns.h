#ifndef NS_H
#define NS_H

#ifdef HAVE_UNSHARE

int ns_init(void);
int ns_priv_drop(void);

#else

static inline int ns_init(void) { return 0; }
static inline int ns_priv_drop(void) { return 0; }

#endif

#endif
