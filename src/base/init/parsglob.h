/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef PARSGLOB_H
#define PARSGLOB_H

enum argument_type {
	TYPE_NONE,
	TYPE_INTEGER,
	TYPE_BOOLEAN,
	TYPE_REAL,
	TYPE_STRING = 0x10,
	TYPE_STRING1,
	TYPE_STRQUOTELESS
};

typedef struct {
	char placeholder[4];
	short token;
	char placeholder_;
	char type;
} __attribute__((packed)) ExprType;

#define EXPRTYPE(x) ((*(ExprType *)&(x)).type)
#define EXPRTOKEN(x) ((*(ExprType *)&(x)).token)

#define TYPINT(x) EXPRTYPE(x) = TYPE_INTEGER
#define TYPBOOL(x) EXPRTYPE(x) = TYPE_BOOLEAN
#define TYPREAL(x) EXPRTYPE(x) = TYPE_REAL
#define TYPSTR(x) EXPRTYPE(x) = TYPE_STRING

#define TOF(x) ( (EXPRTYPE(x) == TYPE_REAL) ? *((float *)&(x)) : *((int *)&(x)) )

#define ALL(x) ((*(YYSTYPE *)&(x)).all_value)

#define VAL_I(x) ( (*(YYSTYPE *)&(x)).i_value )
#define VAL_B(x) ( (*(YYSTYPE *)&(x)).i_value )
#define VAL_R(x) ( (*(YYSTYPE *)&(x)).r_value )
#define VAL_S(x) ( (*(YYSTYPE *)&(x)).s_value )


#define I_VAL(x) ( *( TYPINT(x), &((*(YYSTYPE *)&(x)).i_value )))
#define B_VAL(x) ( *( TYPBOOL(x), &((*(YYSTYPE *)&(x)).i_value )))
#define R_VAL(x) ( *( TYPREAL(x), &((*(YYSTYPE *)&(x)).r_value )))
#define S_VAL(x) ( *( TYPSTR(x), &((*(YYSTYPE *)&(x)).s_value )))
#define V_VAL(x,y,z) \
do { if (EXPRTYPE(y) == TYPE_REAL) R_VAL(x) = (z); else \
      if (EXPRTYPE(y) == TYPE_BOOLEAN) B_VAL(x) = (z); else I_VAL(x) = (z); } \
while (0)


#endif /* PARSGLOB_H */
