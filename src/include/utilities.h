#ifndef UTILITIES_H
#define UTILITIES_H

void open_proc_scan(char *name);
void close_proc_scan(void);
char *get_proc_string_by_key(char *key);
void advance_proc_bufferptr(void);
int get_proc_intvalue_by_key(char *key);


#endif /* UTILITIES_H */
