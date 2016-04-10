#define VERB_MISC	1
#define VERB_TELOPT	2

void
verboseOut(int mask, const char *format, ...);
void
verbosePerror(int mask, const char *s);
