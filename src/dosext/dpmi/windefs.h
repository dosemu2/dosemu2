/* assorted defines, mostly from wine, to get vxd.c to compile */

#define VXD_BARF(name) \
    D_printf( "vxd %s: unknown/not implemented parameters:\n" \
                     "AX %04x, BX %04x, CX %04x, DX %04x, " \
                     "SI %04x, DI %04x, DS %04x, ES %04x\n", \
             (name), LO_WORD(_eax), LO_WORD(_ebx), \
             LO_WORD(_ecx), LO_WORD(_edx), LO_WORD(_esi), \
             LO_WORD(_edi), (WORD)_ds, (WORD)_es )

#define WARN D_printf
#define ERR D_printf

#undef BYTE
typedef unsigned char BYTE;
#undef WORD
typedef unsigned short WORD;
#undef DWORD
typedef unsigned int DWORD;
#undef LONG
typedef unsigned int LONG;
#define HANDLE16 u_short
#define MAKEWORD(low,high)     ((WORD)(((BYTE)(low)) | ((WORD)((BYTE)(high))) << 8))
#define MAKELONG(low,high)     ((LONG)(((WORD)(low)) | (((DWORD)((WORD)(high))) << 16)))
#define UINT16 u_short
#define UINT u_int
#define WINAPI
#define TRACE D_printf
#define FIXME D_printf
#define CALLBACK

#define SET_AX(dummy, val) (_LWORD(eax) = val)
#define SET_DX(dummy, val) (_LWORD(edx) = val)
#define SET_DI(dummy, val) (_LWORD(edi) = val)
#define SET_AL(dummy, val) (_LO(ax) = val)
#define RESET_CFLAG(dummy) (_eflags &= ~CF)
#define SET_CFLAG(dummy) (_eflags |= CF)
#define AX_reg(dummy) _LWORD(eax)
#define BX_reg(dummy) _LWORD(ebx)
#define CX_reg(dummy) _LWORD(ecx)
#define DX_reg(dummy) _LWORD(edx)
#define CHAR char

#define LOWORD(l)              ((WORD)(DWORD)(l))
#define HIWORD(l)              ((WORD)((DWORD)(l) >> 16))

#define MAKESEGPTR(seg,off) ((SEGPTR)MAKELONG(off,seg))
#define SELECTOROF(ptr)     (HIWORD(ptr))
#define OFFSETOF(ptr)       (LOWORD(ptr))

#define IMAGE_SCN_CNT_CODE			0x00000020
#define IMAGE_SCN_CNT_INITIALIZED_DATA		0x00000040
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA	0x00000080
#define HFILE_ERROR     ((HFILE)-1)

#define OF_READ               0x0000
#define OF_WRITE              0x0001
#define OF_READWRITE          0x0002
#define OF_SHARE_COMPAT       0x0000
#define OF_SHARE_EXCLUSIVE    0x0010
#define OF_SHARE_DENY_WRITE   0x0020
#define OF_SHARE_DENY_READ    0x0030
#define OF_SHARE_DENY_NONE    0x0040
#define OF_PARSE              0x0100
#define OF_DELETE             0x0200
#define OF_VERIFY             0x0400   /* Used with OF_REOPEN */
#define OF_SEARCH             0x0400   /* Used without OF_REOPEN */
#define OF_CANCEL             0x0800
#define OF_CREATE             0x1000
#define OF_PROMPT             0x2000
#define OF_EXIST              0x4000
#define OF_REOPEN             0x8000

#define	IMAGE_DIRECTORY_ENTRY_EXPORT		0
#define	IMAGE_DIRECTORY_ENTRY_IMPORT		1
#define	IMAGE_DIRECTORY_ENTRY_RESOURCE		2
#define	IMAGE_DIRECTORY_ENTRY_EXCEPTION		3
#define	IMAGE_DIRECTORY_ENTRY_SECURITY		4
#define	IMAGE_DIRECTORY_ENTRY_BASERELOC		5
#define	IMAGE_DIRECTORY_ENTRY_DEBUG		6
#define	IMAGE_DIRECTORY_ENTRY_COPYRIGHT		7
#define	IMAGE_DIRECTORY_ENTRY_GLOBALPTR		8   /* (MIPS GP) */
#define	IMAGE_DIRECTORY_ENTRY_TLS		9
#define	IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG	10
#define	IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT	11
#define	IMAGE_DIRECTORY_ENTRY_IAT		12  /* Import Address Table */
#define	IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT	13
#define	IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR	14

#define IMAGE_REL_BASED_ABSOLUTE 		0
#define IMAGE_REL_BASED_HIGH			1
#define IMAGE_REL_BASED_LOW			2
#define IMAGE_REL_BASED_HIGHLOW			3
#define IMAGE_REL_BASED_HIGHADJ			4
#define IMAGE_REL_BASED_MIPS_JMPADDR		5
#define IMAGE_REL_BASED_SECTION			6
#define	IMAGE_REL_BASED_REL			7
#define IMAGE_REL_BASED_MIPS_JMPADDR16		9
#define IMAGE_REL_BASED_IA64_IMM64		9 /* yes, 9 too */
#define IMAGE_REL_BASED_DIR64			10
#define IMAGE_REL_BASED_HIGH3ADJ		11

#define	PAGE_NOACCESS		0x01
#define	PAGE_READONLY		0x02
#define	PAGE_READWRITE		0x04
#define	PAGE_WRITECOPY		0x08
#define	PAGE_EXECUTE		0x10
#define	PAGE_EXECUTE_READ	0x20
#define	PAGE_EXECUTE_READWRITE	0x40
#define	PAGE_EXECUTE_WRITECOPY	0x80
#define	PAGE_GUARD		0x100
#define	PAGE_NOCACHE		0x200

#define MEM_COMMIT              0x00001000
#define MEM_RESERVE             0x00002000
#define MEM_DECOMMIT            0x00004000
#define MEM_RELEASE             0x00008000
#define MEM_FREE                0x00010000
#define MEM_PRIVATE             0x00020000
#define MEM_MAPPED              0x00040000
#define MEM_RESET               0x00080000
#define MEM_TOP_DOWN            0x00100000

#define	IMAGE_SIZEOF_FILE_HEADER		20
#define IMAGE_SIZEOF_ROM_OPTIONAL_HEADER	56
#define IMAGE_SIZEOF_STD_OPTIONAL_HEADER	28
#define IMAGE_SIZEOF_NT_OPTIONAL_HEADER 	224
#define IMAGE_SIZEOF_SHORT_NAME 		8
#define IMAGE_SIZEOF_SECTION_HEADER 		40
#define IMAGE_SIZEOF_SYMBOL 			18
#define IMAGE_SIZEOF_AUX_SYMBOL 		18
#define IMAGE_SIZEOF_RELOCATION 		10
#define IMAGE_SIZEOF_BASE_RELOCATION 		8
#define IMAGE_SIZEOF_LINENUMBER 		6
#define IMAGE_SIZEOF_ARCHIVE_MEMBER_HDR 	60

#define INVALID_HANDLE_VALUE     ((HANDLE)~0UL)
#define INVALID_FILE_SIZE        ((DWORD)~0UL)
#define INVALID_SET_FILE_POINTER ((DWORD)~0UL)
#define INVALID_FILE_ATTRIBUTES  ((DWORD)~0UL)

#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES 16

#define DUPLICATE_CLOSE_SOURCE		0x00000001
#define DUPLICATE_SAME_ACCESS		0x00000002

#define FILE_MAP_COPY                   0x00000001
#define FILE_MAP_WRITE                  0x00000002
#define FILE_MAP_READ                   0x00000004
#define FILE_MAP_ALL_ACCESS             0x000f001f

typedef long BOOL;
typedef int HFILE;
typedef const CHAR *LPCSTR;
typedef unsigned char   *PBYTE,    *LPBYTE;
typedef void *HMODULE;
typedef void                                   *LPVOID;
typedef void *HANDLE;
typedef long long LONGLONG;
typedef DWORD           SEGPTR;
typedef signed long long   LONG_PTR, *PLONG_PTR;
typedef LONG_PTR LRESULT;
typedef LRESULT (CALLBACK *FARPROC16)(void);
typedef unsigned long long ULONG_PTR, *PULONG_PTR;
typedef ULONG_PTR SIZE_T, *PSIZE_T;
typedef unsigned int   *PDWORD,   *LPDWORD;
typedef const void                             *LPCVOID;
typedef WORD            ATOM;
typedef CHAR *LPSTR;
typedef int INT;

typedef struct _SECURITY_ATTRIBUTES {
    DWORD nLength;
    LPVOID lpSecurityDescriptor;
    BOOL bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;
/* Process database (i.e. a normal DOS PSP) */
typedef struct
{
    WORD      int20;            /* 00 int 20h instruction */
    WORD      nextParagraph;    /* 02 Segment of next paragraph */
    BYTE      reserved1;
    BYTE      dispatcher[5];    /* 05 Long call to DOS */
    FARPROC16 savedint22;       /* 0a Saved int 22h handler */
    FARPROC16 savedint23;       /* 0e Saved int 23h handler */
    FARPROC16 savedint24;       /* 12 Saved int 24h handler */
    WORD      parentPSP;        /* 16 Selector of parent PSP */
    BYTE      fileHandles[20];  /* 18 Open file handles */
    HANDLE16  environment;      /* 2c Selector of environment */
    DWORD     saveStack;        /* 2e SS:SP on last int21 call */
    WORD      nbFiles;          /* 32 Number of file handles */
    SEGPTR    fileHandlesPtr;   /* 34 Pointer to file handle table */
    HANDLE16  hFileHandles;     /* 38 Handle to fileHandlesPtr */
    WORD      reserved3[17];
    BYTE      fcb1[16];         /* 5c First FCB */
    BYTE      fcb2[20];         /* 6c Second FCB */
    BYTE      cmdLine[128];     /* 80 Command-line (first byte is len)*/
    BYTE      padding[16];      /* Some apps access beyond the end of the cmd line */
} PDB16;
typedef struct _LARGE_INTEGER {
    LONGLONG QuadPart;
} LARGE_INTEGER;
typedef struct _MEMORY_BASIC_INFORMATION
{
    LPVOID   BaseAddress;
    LPVOID   AllocationBase;
    DWORD    AllocationProtect;
    DWORD    RegionSize;
    DWORD    State;
    DWORD    Protect;
    DWORD    Type;
} MEMORY_BASIC_INFORMATION, *PMEMORY_BASIC_INFORMATION;
typedef struct _IMAGE_BASE_RELOCATION
{
	DWORD	VirtualAddress;
	DWORD	SizeOfBlock;
	/* WORD	TypeOffset[1]; */
} IMAGE_BASE_RELOCATION,*PIMAGE_BASE_RELOCATION;
typedef struct _IMAGE_SECTION_HEADER {
  BYTE  Name[IMAGE_SIZEOF_SHORT_NAME];
  union {
    DWORD PhysicalAddress;
    DWORD VirtualSize;
  } Misc;
  DWORD VirtualAddress;
  DWORD SizeOfRawData;
  DWORD PointerToRawData;
  DWORD PointerToRelocations;
  DWORD PointerToLinenumbers;
  WORD  NumberOfRelocations;
  WORD  NumberOfLinenumbers;
  DWORD Characteristics;
} IMAGE_SECTION_HEADER, *PIMAGE_SECTION_HEADER;
typedef struct _IMAGE_DATA_DIRECTORY {
  DWORD VirtualAddress;
  DWORD Size;
} IMAGE_DATA_DIRECTORY, *PIMAGE_DATA_DIRECTORY;
typedef struct _IMAGE_OPTIONAL_HEADER {

  /* Standard fields */

  WORD  Magic; /* 0x10b or 0x107 */	/* 0x00 */
  BYTE  MajorLinkerVersion;
  BYTE  MinorLinkerVersion;
  DWORD SizeOfCode;
  DWORD SizeOfInitializedData;
  DWORD SizeOfUninitializedData;
  DWORD AddressOfEntryPoint;		/* 0x10 */
  DWORD BaseOfCode;
  DWORD BaseOfData;

  /* NT additional fields */

  DWORD ImageBase;
  DWORD SectionAlignment;		/* 0x20 */
  DWORD FileAlignment;
  WORD  MajorOperatingSystemVersion;
  WORD  MinorOperatingSystemVersion;
  WORD  MajorImageVersion;
  WORD  MinorImageVersion;
  WORD  MajorSubsystemVersion;		/* 0x30 */
  WORD  MinorSubsystemVersion;
  DWORD Win32VersionValue;
  DWORD SizeOfImage;
  DWORD SizeOfHeaders;
  DWORD CheckSum;			/* 0x40 */
  WORD  Subsystem;
  WORD  DllCharacteristics;
  DWORD SizeOfStackReserve;
  DWORD SizeOfStackCommit;
  DWORD SizeOfHeapReserve;		/* 0x50 */
  DWORD SizeOfHeapCommit;
  DWORD LoaderFlags;
  DWORD NumberOfRvaAndSizes;
  IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES]; /* 0x60 */
  /* 0xE0 */
} IMAGE_OPTIONAL_HEADER, *PIMAGE_OPTIONAL_HEADER;
typedef struct _IMAGE_FILE_HEADER {
  WORD  Machine;
  WORD  NumberOfSections;
  DWORD TimeDateStamp;
  DWORD PointerToSymbolTable;
  DWORD NumberOfSymbols;
  WORD  SizeOfOptionalHeader;
  WORD  Characteristics;
} IMAGE_FILE_HEADER, *PIMAGE_FILE_HEADER;
typedef struct _IMAGE_NT_HEADERS {
  DWORD Signature; /* "PE"\0\0 */	/* 0x00 */
  IMAGE_FILE_HEADER FileHeader;		/* 0x04 */
  IMAGE_OPTIONAL_HEADER OptionalHeader;	/* 0x18 */
} IMAGE_NT_HEADERS, *PIMAGE_NT_HEADERS;

#define W32S_APP2WINE(addr) ((addr)? (uintptr_t)(addr) + W32S_offset : 0)
#define W32S_WINE2APP(addr) ((addr)? (uintptr_t)(addr) - W32S_offset : 0)

#define CONTEXT86 struct sigcontext

#define GetTickCount() GETtickTIME(0)

#define _lopen open
#define _llseek lseek
#define _lread read
#define _lclose close

#define STATUS_SUCCESS                   0x00000000
#define STATUS_NO_MEMORY                 0xC0000017
