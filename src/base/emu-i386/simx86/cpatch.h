#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN extern
#endif
#ifdef __i386__
#define asmlinkage EXTERN __attribute__((cdecl))
#else
#define asmlinkage EXTERN
#endif

struct rep_stack;

asmlinkage void rep_movs_stos(struct rep_stack *stack);
asmlinkage void stk_16(dosaddr_t addr, Bit16u value);
asmlinkage void stk_32(dosaddr_t addr, Bit32u value);
asmlinkage void wri_8(dosaddr_t addr, Bit8u value, unsigned char *eip);
asmlinkage void wri_16(dosaddr_t addr, Bit16u value, unsigned char *eip);
asmlinkage void wri_32(dosaddr_t addr, Bit32u value, unsigned char *eip);
asmlinkage Bit8u read_8(dosaddr_t addr);
asmlinkage Bit16u read_16(dosaddr_t addr);
asmlinkage Bit32u read_32(dosaddr_t addr);
