#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN extern
#endif
#ifdef __i386__
#define ASMLINKAGE(x,y,z) EXTERN __attribute__((cdecl)) x y z asm(#y)
#else
#define ASMLINKAGE(x,y,z) EXTERN x y z asm(#y)
#endif

struct rep_stack;

ASMLINKAGE(void,rep_movs_stos,(struct rep_stack *stack));
ASMLINKAGE(void,stk_16,(dosaddr_t addr, Bit16u value));
ASMLINKAGE(void,stk_32,(dosaddr_t addr, Bit32u value));
ASMLINKAGE(void,wri_8,(dosaddr_t addr, Bit8u value, unsigned char *eip));
ASMLINKAGE(void,wri_16,(dosaddr_t addr, Bit16u value, unsigned char *eip));
ASMLINKAGE(void,wri_32,(dosaddr_t addr, Bit32u value, unsigned char *eip));
ASMLINKAGE(Bit8u,read_8,(dosaddr_t addr));
ASMLINKAGE(Bit16u,read_16,(dosaddr_t addr));
ASMLINKAGE(Bit32u,read_32,(dosaddr_t addr));
