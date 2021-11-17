#ifndef COOPTH_PM
#define COOPTH_PM

int coopth_create_pm(const char *name, coopth_func_t func,
	void (*post)(sigcontext_t *), void *hlt_state, unsigned offs,
	unsigned int *hlt_off);

#endif
