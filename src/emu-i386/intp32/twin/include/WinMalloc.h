/*	Willows Software, Inc. - Aug 1996	*/
#ifndef WinMalloc__h
#define WinMalloc__h
/*	"@(#)WinMalloc.h	1.4 :/users/sccs/src/win/s.WinMalloc.h 9/27/96 14:28:25" */


/*
 *	WinMallocInfo 
 *	tool to examine the WinMalloc history.
 *
 *	WinMallocInfo(opcode , string, type , handle )
 * 
 *	opcodes
 *		one of the operations available
 *		0	dump winmalloc info
 *		1	how much has been winmalloced so far -
 *			how much has been freed
 *	type
 *		one of the types of memory allocation
 *		0	unknown allocation type
 *		1	persistent (will not be freed)
 *		2	transient, (allocated/freed short term)
 *		3	library object, with handle defined
 *		4	globalalloc object,with handle
 *		5	internal use by library, not persistent	
 *		6	dll allocation
 *		7	driver allocation
 */ 

/* opcodes */
#define	WMI_DUMP	0
#define WMI_STAT	1
#define WMI_TAG		2
#define WMI_CHECK	3

#ifdef WINMALLOC_CHECK

#define WINMALLOCINFO(a,b,c,d)	WinMallocInfo(a,b,c,d)

#else

#define WINMALLOCINFO(a,b,c,d)	

#endif

long WinMallocInfo(int , char * ,int , int );


#endif /* WinMalloc__h */
