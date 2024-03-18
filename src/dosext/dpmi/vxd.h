/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

void get_VXD_entry(cpuctx_t *scp );
void vxd_call(cpuctx_t *scp);

struct vxd_client
{
  char               name[8];
  unsigned short     bx;
  void             (*entry)(cpuctx_t *scp);
  unsigned short     thunk;
  struct vxd_client *next;
};

void register_vxd_client (struct vxd_client *vxd);
