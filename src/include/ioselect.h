#ifndef IOSELECT_H
#define IOSELECT_H

#define IOFLG_IMMED 1
#define IOFLG_MASKED 2
extern void add_to_io_select_new(int, void(*)(int, void *), void *,
	unsigned flags, const char *name);
#define add_to_io_select(fd, func, arg) \
	add_to_io_select_new(fd, func, arg, 0, #func)
#define add_to_io_select_threaded(fd, func, arg) \
	add_to_io_select_new(fd, func, arg, IOFLG_IMMED, #func)
#define add_to_io_select_masked(fd, func, arg) \
	add_to_io_select_new(fd, func, arg, IOFLG_IMMED | IOFLG_MASKED, #func)
extern void remove_from_io_select(int);
extern void ioselect_complete(int fd);
extern void ioselect_block(int fd);
extern void ioselect_unblock(int fd);
extern void ioselect_init(void);
extern void ioselect_done(void);

#endif
