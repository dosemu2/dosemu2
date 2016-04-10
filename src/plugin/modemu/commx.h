#ifdef HAVE_GRANTPT
void
commxForkExec(const char *cmd, char * ptyslave);
#else
void
commxForkExec(const char *cmd, char c10, char c01);
#endif
