#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN extern
#endif
#ifdef __i386__
#define asmlinkage EXTERN __attribute__((cdecl)) \
	__attribute__((force_align_arg_pointer))
#else
#define asmlinkage EXTERN \
	__attribute__((force_align_arg_pointer))
#endif

struct rep_stack;

asmlinkage void rep_movs_stos(struct rep_stack *stack);
asmlinkage void stk_16(unsigned char *paddr, Bit16u value);
asmlinkage void stk_32(unsigned char *paddr, Bit32u value);
asmlinkage void wri_8(unsigned char *paddr, Bit8u value, unsigned char *eip);
asmlinkage void wri_16(unsigned char *paddr, Bit16u value, unsigned char *eip);
asmlinkage void wri_32(unsigned char *paddr, Bit32u value, unsigned char *eip);
