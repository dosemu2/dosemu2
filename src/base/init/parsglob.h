/* 
 * (C) Copyright 1992, ..., 2005 the "DOSEMU-Development-Team".
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
	union {
		int i_value;
		char *s_value;
		float r_value;
	} u;
	short token;
	char placeholder_;
	char type;
} ExprType;

#define ALL(x) (*(YYSTYPE *)&(x))

#define EXPRTYPE(x) (ALL(x).t_value.type)
#define EXPRTOKEN(x) (ALL(x).t_value.token)

#define TYPINT(x) EXPRTYPE(x) = TYPE_INTEGER
#define TYPBOOL(x) EXPRTYPE(x) = TYPE_BOOLEAN
#define TYPREAL(x) EXPRTYPE(x) = TYPE_REAL
#define TYPSTR(x) EXPRTYPE(x) = TYPE_STRING

#define VAL_I(x) ( ALL(x).i_value )
#define VAL_B(x) ( ALL(x).i_value )
#define VAL_R(x) ( ALL(x).r_value )
#define VAL_S(x) ( ALL(x).s_value )

#define TOF(x) ( (EXPRTYPE(x) == TYPE_REAL) ? VAL_R(x) : VAL_I(x) )

#define I_VAL(x) ( *( TYPINT(x), &VAL_I(x) ) )
#define B_VAL(x) ( *( TYPBOOL(x), &VAL_B(x) ) )
#define R_VAL(x) ( *( TYPREAL(x), &VAL_R(x) ) )
#define S_VAL(x) ( *( TYPSTR(x), &VAL_S(x) ) )
#define V_VAL(x,y,z) \
do { if (EXPRTYPE(y) == TYPE_REAL) R_VAL(x) = (z); else \
      if (EXPRTYPE(y) == TYPE_BOOLEAN) B_VAL(x) = (z); else I_VAL(x) = (z); } \
while (0)


#endif /* PARSGLOB_H */
