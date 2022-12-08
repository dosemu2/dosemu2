#ifndef COOPTH_PM
#define COOPTH_PM

int coopth_create_pm(const char *name, coopth_func_t func,
	void (*post)(cpuctx_t *), void *hlt_state, unsigned offs,
	unsigned int *hlt_off);

int coopth_create_pm_multi(const char *name, coopth_func_t func,
	void (*post)(cpuctx_t *), void *hlt_state, unsigned offs,
	int len, unsigned int *hlt_off, int r_offs[]);

#endif
