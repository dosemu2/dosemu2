#include "dpmi_api.h"
#include "hlpmisc.h"

void do_call_to(cpuctx_t *scp, int is_32, far_t dst, __dpmi_regs *rmreg)
{
    RMREG(ss) = 0;
    RMREG(sp) = 0;
    RMREG(cs) = dst.segment;
    RMREG(ip) = dst.offset;
    _dpmi_simulate_real_mode_procedure_retf(scp, is_32, rmreg);
}
