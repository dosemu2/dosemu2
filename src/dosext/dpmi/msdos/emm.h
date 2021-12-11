#ifndef EMM_H
#define EMM_H

int emm_allocate_handle(sigcontext_t *scp, int is_32, int pages_needed);
int emm_deallocate_handle(sigcontext_t *scp, int is_32, int handle);
int emm_save_handle_state(sigcontext_t *scp, int is_32, int handle);
int emm_restore_handle_state(sigcontext_t *scp, int is_32, int handle);
int emm_map_unmap_multi(sigcontext_t *scp, int is_32, const u_short *array,
    int handle, int map_len);

#endif
