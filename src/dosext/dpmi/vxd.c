/*
 * VxD emulation
 *
 * Copyright 1995 Anand Kumria
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "emu.h"
#include "dpmi.h"
#include "dpmisel.h"
#include "bios.h"
#include "timers.h"
#include "vxd.h"
#include "windefs.h"
#include <Asm/ldt.h>

#include <fcntl.h>
#include <memory.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static UINT W32S_offset = 0;

void get_VXD_entry( struct sigcontext *scp )
{
    switch (_LWORD(ebx)) {
	case 0x01:
	    D_printf("DPMI: VMM VxD entry point requested\n");
	    _es = dpmi_sel();
	    _edi = DPMI_SEL_OFF(DPMI_VXD_VMM);
	    break;
	case 0x05:
	    D_printf("DPMI: VTD VxD entry point requested\n");
	    _es = dpmi_sel();
	    _edi = DPMI_SEL_OFF(DPMI_VXD_VTD);
	    break;
	case 0x09:
	    D_printf("DPMI: Reboot VxD entry point requested\n");
	    _es = dpmi_sel();
	    _edi = DPMI_SEL_OFF(DPMI_VXD_Reboot);
	    break;
	case 0x0a:
	    D_printf("DPMI: VDD VxD entry point requested\n");
	    _es = dpmi_sel();
	    _edi = DPMI_SEL_OFF(DPMI_VXD_VDD);
	    break;
	case 0x0c:
	    D_printf("DPMI: VMD VxD entry point requested\n");
	    _es = dpmi_sel();
	    _edi = DPMI_SEL_OFF(DPMI_VXD_VMD);
	    break;
	case 0x0e:
	    D_printf("DPMI: VCD VxD entry point requested\n");
	    _es = dpmi_sel();
	    _edi = DPMI_SEL_OFF(DPMI_VXD_VCD);
	    break;
	case 0x17:
	    D_printf("DPMI: SHELL VxD entry point requested\n");
	    _es = dpmi_sel();
	    _edi = DPMI_SEL_OFF(DPMI_VXD_SHELL);
	    break;
	case 0x21:
	    D_printf("DPMI: PageFile VxD entry point requested\n");
	    _es = dpmi_sel();
	    _edi = DPMI_SEL_OFF(DPMI_VXD_PageFile);
	    break;
	case 0x26:
	    D_printf("DPMI: APM VxD entry point requested\n");
	    _es = dpmi_sel();
	    _edi = DPMI_SEL_OFF(DPMI_VXD_APM);
	    break;
#if 0
	case 0x27:
	    D_printf("DPMI: VXDLDR VxD entry point requested\n");
	    _es = dpmi_sel();
	    _edi = DPMI_SEL_OFF(DPMI_VXD_VXDLDR);
	    break;
#endif
	case 0x33:
	    D_printf("DPMI: CONFIGMG VxD entry point requested\n");
	    _es = dpmi_sel();
	    _edi = DPMI_SEL_OFF(DPMI_VXD_CONFIGMG);
	    break;
	case 0x37:
	    D_printf("DPMI: ENABLE VxD entry point requested\n");
	    _es = dpmi_sel();
	    _edi = DPMI_SEL_OFF(DPMI_VXD_ENABLE);
	    break;
	case 0x442:
	    D_printf("DPMI: VTDAPI VxD entry point requested\n");
	    _es = dpmi_sel();
	    _edi = DPMI_SEL_OFF(DPMI_VXD_VTDAPI);
	    break;
	default:
	    D_printf("DPMI: ERROR: Unsupported VxD\n");
	    /* no entry point */
	    _es = _edi = 0;
    }
}

/***********************************************************************
 *         GetVersion   (KERNEL.3)
 */
static DWORD WINAPI GetVersion16(void)
{
    WORD dosver, winver;

    winver = MAKEWORD( 3, 1 );
    dosver = 0x0616;  /* DOS 6.22 for Windows 3.1 and later */
    return MAKELONG( winver, dosver );
}

static WORD VXD_WinVersion(void)
{
    WORD version = GetVersion16();
    return (version >> 8) | (version << 8);
}

/***********************************************************************
 *           VXD_VMM (WPROCS.401)
 */
static void WINAPI VXD_VMM ( CONTEXT86 *scp )
{
    unsigned service = AX_reg(scp);

    TRACE("[%04x] VMM\n", (UINT16)service);

    switch(service)
    {
    case 0x0000: /* version */
        SET_AX( scp, VXD_WinVersion() );
        RESET_CFLAG(scp);
        break;

    case 0x026d: /* Get_Debug_Flag '/m' */
    case 0x026e: /* Get_Debug_Flag '/n' */
        SET_AL( scp, 0 );
        RESET_CFLAG(scp);
        break;

    default:
        VXD_BARF( "VMM" );
    }
}

/***********************************************************************
 *           VXD_PageFile (WPROCS.433)
 */
static void WINAPI VXD_PageFile( CONTEXT86 *scp )
{
    unsigned	service = AX_reg(scp);

    /* taken from Ralf Brown's Interrupt List */

    TRACE("[%04x] PageFile\n", (UINT16)service );

    switch(service)
    {
    case 0x00: /* get version, is this windows version? */
	TRACE("returning version\n");
        SET_AX( scp, VXD_WinVersion() );
	RESET_CFLAG(scp);
	break;

    case 0x01: /* get swap file info */
	TRACE("VxD PageFile: returning swap file info\n");
	SET_AX( scp, 0x00 ); /* paging disabled */
	_ecx = 0;   /* maximum size of paging file */
	/* FIXME: do I touch DS:SI or DS:DI? */
	RESET_CFLAG(scp);
	break;

    case 0x02: /* delete permanent swap on exit */
	TRACE("VxD PageFile: supposed to delete swap\n");
	RESET_CFLAG(scp);
	break;

    case 0x03: /* current temporary swap file size */
	TRACE("VxD PageFile: what is current temp. swap size\n");
	RESET_CFLAG(scp);
	break;

    case 0x04: /* read or write?? INTERRUP.D */
    case 0x05: /* cancel?? INTERRUP.D */
    case 0x06: /* test I/O valid INTERRUP.D */
    default:
	VXD_BARF( "pagefile" );
	break;
    }
}

/***********************************************************************
 *           VXD_Reboot (WPROCS.409)
 */
static void WINAPI VXD_Reboot ( CONTEXT86 *scp )
{
    unsigned service = AX_reg(scp);

    TRACE("[%04x] Reboot\n", (UINT16)service);

    switch(service)
    {
    case 0x0000: /* version */
        SET_AX( scp, VXD_WinVersion() );
        RESET_CFLAG(scp);
        break;

    default:
        VXD_BARF( "REBOOT" );
    }
}

/***********************************************************************
 *           VXD_VDD (WPROCS.410)
 */
static void WINAPI VXD_VDD ( CONTEXT86 *scp )
{
    unsigned service = AX_reg(scp);

    TRACE("[%04x] VDD\n", (UINT16)service);

    switch(service)
    {
    case 0x0000: /* version */
        SET_AX( scp, VXD_WinVersion() );
        RESET_CFLAG(scp);
        break;

    default:
        VXD_BARF( "VDD" );
    }
}

/***********************************************************************
 *           VXD_VMD (WPROCS.412)
 */
static void WINAPI VXD_VMD ( CONTEXT86 *scp )
{
    unsigned service = AX_reg(scp);

    TRACE("[%04x] VMD\n", (UINT16)service);

    switch(service)
    {
    case 0x0000: /* version */
        SET_AX( scp, VXD_WinVersion() );
        RESET_CFLAG(scp);
        break;

    default:
        VXD_BARF( "VMD" );
    }
}

/***********************************************************************
 *           VXD_VXDLoader (WPROCS.439)
 */
static void WINAPI VXD_VXDLoader( CONTEXT86 *scp )
{
    unsigned service = AX_reg(scp);

    TRACE("[%04x] VXDLoader\n", (UINT16)service);

    switch (service)
    {
    case 0x0000: /* get version */
	TRACE("returning version\n");
	SET_AX( scp, 0x0000 );
	SET_DX( scp, VXD_WinVersion() );
	RESET_CFLAG(scp);
	break;

    case 0x0001: /* load device */
	SET_AX( scp, 0x0000 );
	_es = 0x0000;
	SET_DI( scp, 0x0000 );
	RESET_CFLAG(scp);
	break;

    case 0x0002: /* unload device */
	FIXME("unload device (%08x)\n", _ebx);
	SET_AX( scp, 0x0000 );
	RESET_CFLAG(scp);
	break;

    default:
	VXD_BARF( "VXDLDR" );
	SET_AX( scp, 0x000B ); /* invalid function number */
	SET_CFLAG(scp);
	break;
    }
}

/***********************************************************************
 *           VXD_Shell (WPROCS.423)
 */
static void WINAPI VXD_Shell( CONTEXT86 *scp )
{
    unsigned	service = DX_reg(scp);

    TRACE("[%04x] Shell\n", (UINT16)service);

    switch (service) /* Ralf Brown says EDX, but I use DX instead */
    {
    case 0x0000:
	TRACE("returning version\n");
        SET_AX( scp, VXD_WinVersion() );
	_ebx = 1; /* system VM Handle */
	break;

    case 0x0001:
    case 0x0002:
    case 0x0003:
        /* SHELL_SYSMODAL_Message
	ebx virtual maschine handle
	eax message box flags
	ecx address of message
	edi address of caption
	return response in eax
	*/
    case 0x0004:
	/* SHELL_Message
	ebx virtual maschine handle
	eax message box flags
	ecx address of message
	edi address of caption
	esi address callback
	edx reference data for callback
	return response in eax
	*/
    case 0x0005:
	VXD_BARF( "shell" );
	break;

    case 0x0006: /* SHELL_Get_VM_State */
	TRACE("VxD Shell: returning VM state\n");
	/* Actually we don't, not yet. We have to return a structure
         * and I am not to sure how to set it up and return it yet,
         * so for now let's do nothing. I can (hopefully) get this
         * by the next release
         */
	/* RESET_CFLAG(scp); */
	break;

    case 0x0007:
    case 0x0008:
    case 0x0009:
    case 0x000A:
    case 0x000B:
    case 0x000C:
    case 0x000D:
    case 0x000E:
    case 0x000F:
    case 0x0010:
    case 0x0011:
    case 0x0012:
    case 0x0013:
    case 0x0014:
    case 0x0015:
    case 0x0016:
	VXD_BARF( "SHELL" );
	break;

    /* the new Win95 shell API */
    case 0x0100:     /* get version */
        SET_AX( scp, VXD_WinVersion() );
	break;

    case 0x0104:   /* retrieve Hook_Properties list */
    case 0x0105:   /* call Hook_Properties callbacks */
	VXD_BARF( "SHELL" );
	break;

    case 0x0106:   /* install timeout callback */
	TRACE("VxD Shell: ignoring shell callback (%d sec.)\n", _ebx);
	SET_CFLAG(scp);
	break;

    case 0x0107:   /* get version of any VxD */
    default:
	VXD_BARF( "SHELL" );
	break;
    }
}


/***********************************************************************
 *           VXD_Comm (WPROCS.414)
 */
static void WINAPI VXD_Comm( CONTEXT86 *scp )
{
    unsigned	service = AX_reg(scp);

    TRACE("[%04x] Comm\n", (UINT16)service);

    switch (service)
    {
    case 0x0000: /* get version */
	TRACE("returning version\n");
        SET_AX( scp, VXD_WinVersion() );
	RESET_CFLAG(scp);
	break;

    case 0x0001: /* set port global */
    case 0x0002: /* get focus */
    case 0x0003: /* virtualise port */
    default:
        VXD_BARF( "comm" );
    }
}

/***********************************************************************
 *           VXD_Timer (WPROCS.405)
 */
static void WINAPI VXD_Timer( CONTEXT86 *scp )
{
    unsigned service = AX_reg(scp);

    TRACE("[%04x] Virtual Timer\n", (UINT16)service);

    switch(service)
    {
    case 0x0000: /* version */
	SET_AX( scp, VXD_WinVersion() );
	RESET_CFLAG(scp);
	break;

    case 0x0100: /* clock tick time, in 840nsecs */
	_eax = GetTickCount();

	_edx = _eax >> 22;
	_eax <<= 10; /* not very precise */
	break;

    case 0x0101: /* current Windows time, msecs */
    case 0x0102: /* current VM time, msecs */
	_eax = GETusTIME(0) / 1000;
	break;

    default:
	VXD_BARF( "VTD" );
    }
}

/***********************************************************************
 *           VXD_TimerAPI (WPROCS.1490)
 */
static void WINAPI VXD_TimerAPI ( CONTEXT86 *scp )
{
    static WORD System_Time_Selector;
    DWORD cur_time;

    unsigned service = AX_reg(scp);

    TRACE("[%04x] TimerAPI\n", (UINT16)service);

    switch(service)
    {
    case 0x0000: /* version */
        SET_AX( scp, VXD_WinVersion() );
        RESET_CFLAG(scp);
        break;

    case 0x0004: /* current VM time, msecs */
	cur_time = GETusTIME(0) / 1000;
	_edx = HI_WORD(cur_time);
	_eax = LO_WORD(cur_time);
        RESET_CFLAG(scp);
	break;

    case 0x0009: /* get system time selector */
        if ( !System_Time_Selector )
        {
            HANDLE16 handle = AllocateDescriptors(1);
	    SetSelector(handle, (unsigned long) &pic_sys_time,
		    sizeof(DWORD)-1, 0, MODIFY_LDT_CONTENTS_DATA, 1, 0, 0, 0);
            System_Time_Selector = handle;
        }
        SET_AX( scp, System_Time_Selector );
        RESET_CFLAG(scp);
        break;

    default:
        VXD_BARF( "VTDAPI" );
    }
}

/***********************************************************************
 *           VXD_ConfigMG (WPROCS.451)
 */
static void WINAPI VXD_ConfigMG ( CONTEXT86 *scp )
{
    unsigned service = AX_reg(scp);

    TRACE("[%04x] ConfigMG\n", (UINT16)service);

    switch(service)
    {
    case 0x0000: /* version */
        SET_AX( scp, VXD_WinVersion() );
        RESET_CFLAG(scp);
        break;

    default:
        VXD_BARF( "CONFIGMG" );
    }
}

/***********************************************************************
 *           VXD_Enable (WPROCS.455)
 */
static void WINAPI VXD_Enable ( CONTEXT86 *scp )
{
    unsigned service = AX_reg(scp);

    TRACE("[%04x] Enable\n", (UINT16)service);

    switch(service)
    {
    case 0x0000: /* version */
        SET_AX( scp, VXD_WinVersion() );
        RESET_CFLAG(scp);
        break;

    default:
        VXD_BARF( "ENABLE" );
    }
}

/***********************************************************************
 *           VXD_APM (WPROCS.438)
 */
static void WINAPI VXD_APM ( CONTEXT86 *scp )
{
    unsigned service = AX_reg(scp);

    TRACE("[%04x] APM\n", (UINT16)service);

    switch(service)
    {
    case 0x0000: /* version */
        SET_AX( scp, VXD_WinVersion() );
        RESET_CFLAG(scp);
        break;

    default:
        VXD_BARF( "APM" );
    }
}


static LPVOID WINAPI MapSL( SEGPTR segptr )
{
    return (LPVOID)SEL_ADR(SELECTOROF(segptr), OFFSETOF(segptr));
}

static PIMAGE_NT_HEADERS WINAPI RtlImageNtHeader(HMODULE mod)
{
    return NULL;
}
static LPVOID WINAPI VirtualAlloc(LPVOID a,SIZE_T b,DWORD c,DWORD d)
{
    return NULL;
}
static BOOL WINAPI VirtualFree(LPVOID a,SIZE_T b,DWORD c)
{
    return 0;
}
static BOOL WINAPI VirtualProtect(LPVOID a,SIZE_T b,DWORD c,LPDWORD d)
{
    return 0;
}
static SIZE_T WINAPI VirtualQuery(LPCVOID a,PMEMORY_BASIC_INFORMATION b,SIZE_T c)
{
    return 0;
}
static HANDLE WINAPI DosFileHandleToWin32Handle(HFILE f)
{
    return 0;
}
static UINT WINAPI GlobalGetAtomNameA(ATOM a,LPSTR b,INT c)
{
    return 0;
}
static HANDLE WINAPI CreateFileMappingA(HANDLE a,LPSECURITY_ATTRIBUTES b,
	DWORD c,DWORD d,DWORD e,LPCSTR f)
{
    return 0;
}
static HANDLE WINAPI OpenFileMappingA(DWORD a,BOOL b,LPCSTR c)
{
    return 0;
}
static BOOL WINAPI CloseHandle(HANDLE h)
{
    return 0;
}
static BOOL WINAPI DuplicateHandle(HANDLE a,HANDLE b,HANDLE c,HANDLE* d,
	DWORD e,BOOL f,DWORD g)
{
    return 0;
}
static HANDLE WINAPI GetCurrentProcess(void)
{
    return 0;
}
static LPVOID WINAPI MapViewOfFileEx(HANDLE a,DWORD b,DWORD c,DWORD d,
	SIZE_T e,LPVOID f)
{
    return NULL;
}
static BOOL WINAPI UnmapViewOfFile(LPVOID a)
{
    return 0;
}
static BOOL WINAPI FlushViewOfFile(LPCVOID a,SIZE_T b)
{
    return 0;
}
static BOOL WINAPI VirtualLock(LPVOID a,SIZE_T b)
{
    return 0;
}
static BOOL WINAPI VirtualUnlock(LPVOID a,SIZE_T b)
{
    return 0;
}

/***********************************************************************
 *           VXD_Win32s (WPROCS.445)
 *
 * This is an implementation of the services of the Win32s VxD.
 * Since official documentation of these does not seem to be available,
 * certain arguments of some of the services remain unclear.
 *
 * FIXME: The following services are currently unimplemented:
 *        Exception handling      (0x01, 0x1C)
 *        Debugger support        (0x0C, 0x14, 0x17)
 *        Low-level memory access (0x02, 0x03, 0x0A, 0x0B)
 *        Memory Statistics       (0x1B)
 *
 *
 * We have a specific problem running Win32s on Linux (and probably also
 * the other x86 unixes), since Win32s tries to allocate its main 'flat
 * code/data segment' selectors with a base of 0xffff0000 (and limit 4GB).
 * The rationale for this seems to be that they want one the one hand to
 * be able to leave the Win 3.1 memory (starting with the main DOS memory)
 * at linear address 0, but want at other hand to have offset 0 of the
 * flat data/code segment point to an unmapped page (to catch NULL pointer
 * accesses). Hence they allocate the flat segments with a base of 0xffff0000
 * so that the Win 3.1 memory area at linear address zero shows up in the
 * flat segments at offset 0x10000 (since linear addresses wrap around at
 * 4GB). To compensate for that discrepancy between flat segment offsets
 * and plain linear addresses, all flat pointers passed between the 32-bit
 * and the 16-bit parts of Win32s are shifted by 0x10000 in the appropriate
 * direction by the glue code (mainly) in W32SKRNL and WIN32S16.
 *
 * The problem for us is now that Linux does not allow a LDT selector with
 * base 0xffff0000 to be created, since it would 'see' a part of the kernel
 * address space. To address this problem we introduce *another* offset:
 * We add 0x10000 to every linear address we get as an argument from Win32s.
 * This means especially that the flat code/data selectors get actually
 * allocated with base 0x0, so that flat offsets and (real) linear addresses
 * do again agree!  In fact, every call e.g. of a Win32s VxD service now
 * has all pointer arguments (which are offsets in the flat data segement)
 * first reduced by 0x10000 by the W32SKRNL glue code, and then again
 * increased by 0x10000 by *our* code.
 *
 * Note that to keep everything consistent, this offset has to be applied by
 * every Wine function that operates on 'linear addresses' passed to it by
 * Win32s. Fortunately, since Win32s does not directly call any Wine 32-bit
 * API routines, this affects only two locations: this VxD and the DPMI
 * handler. (NOTE: Should any Win32s application pass a linear address to
 * any routine apart from those, e.g. some other VxD handler, that code
 * would have to take the offset into account as well!)
 *
 * The offset is set the first time any application calls the GetVersion()
 * service of the Win32s VxD. (Note that the offset is never reset.)
 *
 */
#if 1
#if 1
__attribute__((unused))
#endif
static void WINAPI VXD_Win32s( CONTEXT86 *scp )
{
    switch (AX_reg(scp))
    {
    case 0x0000: /* Get Version */
        /*
         * Input:   None
         *
         * Output:  EAX: LoWord: Win32s Version (1.30)
         *               HiWord: VxD Version (200)
         *
         *          EBX: Build (172)
         *
         *          ECX: ???   (1)
         *
         *          EDX: Debugging Flags
         *
         *          EDI: Error Flag
         *               0 if OK,
         *               1 if VMCPD VxD not found
         */

        TRACE("GetVersion()\n");

	_eax = VXD_WinVersion() | (200 << 16);
        _ebx = 0;
        _ecx = 0;
        _edx = 0;
        _edi = 0;

#if 0
        /*
         * If this is the first time we are called for this process,
         * hack the memory image of WIN32S16 so that it doesn't try
         * to access the GDT directly ...
         *
         * The first code segment of WIN32S16 (version 1.30) contains
         * an unexported function somewhere between the exported functions
         * SetFS and StackLinearToSegmented that tries to find a selector
         * in the LDT that maps to the memory image of the LDT itself.
         * If it succeeds, it stores this selector into a global variable
         * which will be used to speed up execution by using this selector
         * to modify the LDT directly instead of using the DPMI calls.
         *
         * To perform this search of the LDT, this function uses the
         * sgdt and sldt instructions to find the linear address of
         * the (GDT and then) LDT. While those instructions themselves
         * execute without problem, the linear address that sgdt returns
         * points (at least under Linux) to the kernel address space, so
         * that any subsequent access leads to a segfault.
         *
         * Fortunately, WIN32S16 still contains as a fallback option the
         * mechanism of using DPMI calls to modify LDT selectors instead
         * of direct writes to the LDT. Thus we can circumvent the problem
         * by simply replacing the first byte of the offending function
         * with an 'retf' instruction. This means that the global variable
         * supposed to contain the LDT alias selector will remain zero,
         * and hence WIN32S16 will fall back to using DPMI calls.
         *
         * The heuristic we employ to _find_ that function is as follows:
         * We search between the addresses of the exported symbols SetFS
         * and StackLinearToSegmented for the byte sequence '0F 01 04'
         * (this is the opcode of 'sgdt [si]'). We then search backwards
         * from this address for the last occurrence of 'CB' (retf) that marks
         * the end of the preceeding function. The following byte (which
         * should now be the first byte of the function we are looking for)
         * will be replaced by 'CB' (retf).
         *
         * This heuristic works for the retail as well as the debug version
         * of Win32s version 1.30. For versions earlier than that this
         * hack should not be necessary at all, since the whole mechanism
         * ('PERF130') was introduced only in 1.30 to improve the overall
         * performance of Win32s.
         */

        if (!W32S_offset)
        {
            HMODULE16 hModule = GetModuleHandle16("win32s16");
            SEGPTR func1 = (SEGPTR)GetProcAddress16(hModule, "SetFS");
            SEGPTR func2 = (SEGPTR)GetProcAddress16(hModule, "StackLinearToSegmented");

            if (   hModule && func1 && func2
                && SELECTOROF(func1) == SELECTOROF(func2))
            {
                BYTE *start = MapSL(func1);
                BYTE *end   = MapSL(func2);
                BYTE *p, *retv = NULL;
                int found = 0;

                for (p = start; p < end; p++)
                    if (*p == 0xCB) found = 0, retv = p;
                    else if (*p == 0x0F) found = 1;
                    else if (*p == 0x01 && found == 1) found = 2;
                    else if (*p == 0x04 && found == 2) { found = 3; break; }
                    else found = 0;

                if (found == 3 && retv)
                {
                    TRACE("PERF130 hack: "
                               "Replacing byte %02X at offset %04X:%04X\n",
                               *(retv+1), SELECTOROF(func1),
                                          OFFSETOF(func1) + retv+1-start);

                    *(retv+1) = (BYTE)0xCB;
                }
            }
        }
#endif

        /*
         * Mark process as Win32s, so that subsequent DPMI calls
         * will perform the W32S_APP2WINE/W32S_WINE2APP address shift.
         */
        W32S_offset = 0x10000;
        break;


    case 0x0001: /* Install Exception Handling */
        /*
         * Input:   EBX: Flat address of W32SKRNL Exception Data
         *
         *          ECX: LoWord: Flat Code Selector
         *               HiWord: Flat Data Selector
         *
         *          EDX: Flat address of W32SKRNL Exception Handler
         *               (this is equal to W32S_BackTo32 + 0x40)
         *
         *          ESI: SEGPTR KERNEL.HASGPHANDLER
         *
         *          EDI: SEGPTR phCurrentTask (KERNEL.THHOOK + 0x10)
         *
         * Output:  EAX: 0 if OK
         */

        TRACE("[0001] EBX=%x ECX=%x EDX=%x ESI=%x EDI=%x\n",
                   _ebx, _ecx, _edx,
                   _esi, _edi);

        /* FIXME */

        _eax = 0;
        break;


    case 0x0002: /* Set Page Access Flags */
        /*
         * Input:   EBX: New access flags
         *               Bit 2: User Page if set, Supervisor Page if clear
         *               Bit 1: Read-Write if set, Read-Only if clear
         *
         *          ECX: Size of memory area to change
         *
         *          EDX: Flat start address of memory area
         *
         * Output:  EAX: Size of area changed
         */

        TRACE("[0002] EBX=%x ECX=%x EDX=%x\n",
                   _ebx, _ecx, _edx);

        /* FIXME */

        _eax = _ecx;
        break;


    case 0x0003: /* Get Page Access Flags */
        /*
         * Input:   EDX: Flat address of page to query
         *
         * Output:  EAX: Page access flags
         *               Bit 2: User Page if set, Supervisor Page if clear
         *               Bit 1: Read-Write if set, Read-Only if clear
         */

        TRACE("[0003] EDX=%x\n", _edx);

        /* FIXME */

        _eax = 6;
        break;


    case 0x0004: /* Map Module */
        /*
         * Input:   ECX: IMTE (offset in Module Table) of new module
         *
         *          EDX: Flat address of Win32s Module Table
         *
         * Output:  EAX: 0 if OK
         */

    if (!_edx || CX_reg(scp) == 0xFFFF)
    {
        TRACE("MapModule: Initialization call\n");
        _eax = 0;
    }
    else
    {
        /*
         * Structure of a Win32s Module Table Entry:
         */
        struct Win32sModule
        {
            DWORD  flags;
            DWORD  flatBaseAddr;
            LPCSTR moduleName;
            LPCSTR pathName;
            LPCSTR unknown;
            LPBYTE baseAddr;
            DWORD  hModule;
            DWORD  relocDelta;
        };

        /*
         * Note: This function should set up a demand-paged memory image
         *       of the given module. Since mmap does not allow file offsets
         *       not aligned at 1024 bytes, we simply load the image fully
         *       into memory.
         */

        struct Win32sModule *moduleTable =
                            (struct Win32sModule *)W32S_APP2WINE(_edx);
        struct Win32sModule *module = moduleTable + _ecx;

        IMAGE_NT_HEADERS *nt_header = RtlImageNtHeader( (HMODULE)module->baseAddr );
        IMAGE_SECTION_HEADER *pe_seg = (IMAGE_SECTION_HEADER*)((char *)&nt_header->OptionalHeader +
                                                         nt_header->FileHeader.SizeOfOptionalHeader);


        HFILE image = _lopen(module->pathName, OF_READ);
        BOOL error = (image == HFILE_ERROR);
        UINT i;

        TRACE("MapModule: Loading %s\n", module->pathName);

        for (i = 0;
             !error && i < nt_header->FileHeader.NumberOfSections;
             i++, pe_seg++)
            if(!(pe_seg->Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA))
            {
                DWORD  off  = pe_seg->PointerToRawData;
                DWORD  len  = pe_seg->SizeOfRawData;
                LPBYTE addr = module->baseAddr + pe_seg->VirtualAddress;

                TRACE("MapModule: "
                           "Section %d at %08lx from %08x len %08x\n",
                           i, (long)addr, off, len);

                if (   _llseek(image, off, SEEK_SET) != off
                    || _lread(image, addr, len) != len)
                    error = TRUE;
            }

        _lclose(image);

        if (error)
            ERR("MapModule: Unable to load %s\n", module->pathName);

        else if (module->relocDelta != 0)
        {
            IMAGE_DATA_DIRECTORY *dir = nt_header->OptionalHeader.DataDirectory
                                      + IMAGE_DIRECTORY_ENTRY_BASERELOC;
            IMAGE_BASE_RELOCATION *r = (IMAGE_BASE_RELOCATION *)
                (dir->Size? module->baseAddr + dir->VirtualAddress : 0);

            TRACE("MapModule: Reloc delta %08x\n", module->relocDelta);

            while (r && r->VirtualAddress)
            {
                LPBYTE page  = module->baseAddr + r->VirtualAddress;
                WORD *TypeOffset = (WORD *)(r + 1);
                int count = (r->SizeOfBlock - sizeof(*r)) / sizeof(*TypeOffset);

                TRACE("MapModule: %d relocations for page %08lx\n",
                           count, (unsigned long)page);

                for(i = 0; i < count; i++)
                {
                    int offset = TypeOffset[i] & 0xFFF;
                    int type   = TypeOffset[i] >> 12;
                    switch(type)
                    {
                    case IMAGE_REL_BASED_ABSOLUTE:
                        break;
                    case IMAGE_REL_BASED_HIGH:
                        *(WORD *)(page+offset) += HIWORD(module->relocDelta);
                        break;
                    case IMAGE_REL_BASED_LOW:
                        *(WORD *)(page+offset) += LOWORD(module->relocDelta);
                        break;
                    case IMAGE_REL_BASED_HIGHLOW:
                        *(DWORD*)(page+offset) += module->relocDelta;
                        break;
                    default:
                        WARN("MapModule: Unsupported fixup type\n");
                        break;
                    }
                }

                r = (IMAGE_BASE_RELOCATION *)((LPBYTE)r + r->SizeOfBlock);
            }
        }

        _eax = 0;
        RESET_CFLAG(scp);
    }
    break;


    case 0x0005: /* UnMap Module */
        /*
         * Input:   EDX: Flat address of module image
         *
         * Output:  EAX: 1 if OK
         */

        TRACE("UnMapModule: %x\n", (DWORD)W32S_APP2WINE(_edx));

        /* As we didn't map anything, there's nothing to unmap ... */

        _eax = 1;
        break;


    case 0x0006: /* VirtualAlloc */
        /*
         * Input:   ECX: Current Process
         *
         *          EDX: Flat address of arguments on stack
         *
         *   DWORD *retv     [out] Flat base address of allocated region
         *   LPVOID base     [in]  Flat address of region to reserve/commit
         *   DWORD  size     [in]  Size of region
         *   DWORD  type     [in]  Type of allocation
         *   DWORD  prot     [in]  Type of access protection
         *
         * Output:  EAX: NtStatus
         */
    {
        DWORD *stack  = (DWORD *)W32S_APP2WINE(_edx);
        DWORD *retv   = (DWORD *)W32S_APP2WINE(stack[0]);
        LPVOID base   = (LPVOID) W32S_APP2WINE(stack[1]);
        DWORD  size   = stack[2];
        DWORD  type   = stack[3];
        DWORD  prot   = stack[4];
        DWORD  result;

        TRACE("VirtualAlloc(%lx, %lx, %x, %x, %x)\n",
                   (long)retv, (long)base, size, type, prot);

        if (type & 0x80000000)
        {
            WARN("VirtualAlloc: strange type %x\n", type);
            type &= 0x7fffffff;
        }

        if (!base && (type & MEM_COMMIT) && prot == PAGE_READONLY)
        {
            WARN("VirtualAlloc: NLS hack, allowing write access!\n");
            prot = PAGE_READWRITE;
        }

        result = (DWORD)(uintptr_t)VirtualAlloc(base, size, type, prot);

        if (W32S_WINE2APP(result))
            *retv            = W32S_WINE2APP(result),
            _eax = STATUS_SUCCESS;
        else
            *retv            = 0,
            _eax = STATUS_NO_MEMORY;  /* FIXME */
    }
    break;


    case 0x0007: /* VirtualFree */
        /*
         * Input:   ECX: Current Process
         *
         *          EDX: Flat address of arguments on stack
         *
         *   DWORD *retv     [out] TRUE if success, FALSE if failure
         *   LPVOID base     [in]  Flat address of region
         *   DWORD  size     [in]  Size of region
         *   DWORD  type     [in]  Type of operation
         *
         * Output:  EAX: NtStatus
         */
    {
        DWORD *stack  = (DWORD *)W32S_APP2WINE(_edx);
        DWORD *retv   = (DWORD *)W32S_APP2WINE(stack[0]);
        LPVOID base   = (LPVOID) W32S_APP2WINE(stack[1]);
        DWORD  size   = stack[2];
        DWORD  type   = stack[3];
        DWORD  result;

        TRACE("VirtualFree(%lx, %lx, %x, %x)\n",
                   (long)retv, (long)base, size, type);

        result = VirtualFree(base, size, type);

        if (result)
            *retv            = TRUE,
            _eax = STATUS_SUCCESS;
        else
            *retv            = FALSE,
            _eax = STATUS_NO_MEMORY;  /* FIXME */
    }
    break;


    case 0x0008: /* VirtualProtect */
        /*
         * Input:   ECX: Current Process
         *
         *          EDX: Flat address of arguments on stack
         *
         *   DWORD *retv     [out] TRUE if success, FALSE if failure
         *   LPVOID base     [in]  Flat address of region
         *   DWORD  size     [in]  Size of region
         *   DWORD  new_prot [in]  Desired access protection
         *   DWORD *old_prot [out] Previous access protection
         *
         * Output:  EAX: NtStatus
         */
    {
        DWORD *stack    = (DWORD *)W32S_APP2WINE(_edx);
        DWORD *retv     = (DWORD *)W32S_APP2WINE(stack[0]);
        LPVOID base     = (LPVOID) W32S_APP2WINE(stack[1]);
        DWORD  size     = stack[2];
        DWORD  new_prot = stack[3];
        DWORD *old_prot = (DWORD *)W32S_APP2WINE(stack[4]);
        DWORD  result;

        TRACE("VirtualProtect(%lx, %lx, %x, %x, %lx)\n",
                   (long)retv, (long)base, size, new_prot, (long)old_prot);

        result = VirtualProtect(base, size, new_prot, old_prot);

        if (result)
            *retv            = TRUE,
            _eax = STATUS_SUCCESS;
        else
            *retv            = FALSE,
            _eax = STATUS_NO_MEMORY;  /* FIXME */
    }
    break;


    case 0x0009: /* VirtualQuery */
        /*
         * Input:   ECX: Current Process
         *
         *          EDX: Flat address of arguments on stack
         *
         *   DWORD *retv                     [out] Nr. bytes returned
         *   LPVOID base                     [in]  Flat address of region
         *   LPMEMORY_BASIC_INFORMATION info [out] Info buffer
         *   DWORD  len                      [in]  Size of buffer
         *
         * Output:  EAX: NtStatus
         */
    {
        DWORD *stack  = (DWORD *)W32S_APP2WINE(_edx);
        DWORD *retv   = (DWORD *)W32S_APP2WINE(stack[0]);
        LPVOID base   = (LPVOID) W32S_APP2WINE(stack[1]);
        PMEMORY_BASIC_INFORMATION info =
                        (PMEMORY_BASIC_INFORMATION)W32S_APP2WINE(stack[2]);
        DWORD  len    = stack[3];
        DWORD  result;

        TRACE("VirtualQuery(%lx, %lx, %lx, %x)\n",
                   (long)retv, (long)base, (long)info, len);

        result = VirtualQuery(base, info, len);

        *retv            = result;
        _eax = STATUS_SUCCESS;
    }
    break;


    case 0x000A: /* SetVirtMemProcess */
        /*
         * Input:   ECX: Process Handle
         *
         *          EDX: Flat address of region
         *
         * Output:  EAX: NtStatus
         */

        TRACE("[000a] ECX=%x EDX=%x\n",
                   _ecx, _edx);

        /* FIXME */

        _eax = STATUS_SUCCESS;
        break;


    case 0x000B: /* ??? some kind of cleanup */
        /*
         * Input:   ECX: Process Handle
         *
         * Output:  EAX: NtStatus
         */

        TRACE("[000b] ECX=%x\n", _ecx);

        /* FIXME */

        _eax = STATUS_SUCCESS;
        break;


    case 0x000C: /* Set Debug Flags */
        /*
         * Input:   EDX: Debug Flags
         *
         * Output:  EDX: Previous Debug Flags
         */

        FIXME("[000c] EDX=%x\n", _edx);

        /* FIXME */

        _edx = 0;
        break;


    case 0x000D: /* NtCreateSection */
        /*
         * Input:   EDX: Flat address of arguments on stack
         *
         *   HANDLE32 *retv      [out] Handle of Section created
         *   DWORD  flags1       [in]  (?? unknown ??)
         *   DWORD  atom         [in]  Name of Section to create
         *   LARGE_INTEGER *size [in]  Size of Section
         *   DWORD  protect      [in]  Access protection
         *   DWORD  flags2       [in]  (?? unknown ??)
         *   HFILE32 hFile       [in]  Handle of file to map
         *   DWORD  psp          [in]  (Win32s: PSP that hFile belongs to)
         *
         * Output:  EAX: NtStatus
         */
    {
        DWORD *stack    = (DWORD *)   W32S_APP2WINE(_edx);
        HANDLE *retv  = (HANDLE *)W32S_APP2WINE(stack[0]);
        DWORD  flags1   = stack[1];
        DWORD  atom     = stack[2];
        LONGLONG *size = (LONGLONG *)W32S_APP2WINE(stack[3]);
        DWORD  protect  = stack[4];
        DWORD  flags2   = stack[5];
        HANDLE hFile    = DosFileHandleToWin32Handle(stack[6]);
        DWORD  psp      = stack[7];

        HANDLE result = INVALID_HANDLE_VALUE;
        char name[128];

        TRACE("NtCreateSection(%lx, %x, %x, %lx, %x, %x, %lx, %x)\n",
                   (long)retv, flags1, atom, (long)size, protect, flags2,
                   (long)hFile, psp);

        if (!atom || GlobalGetAtomNameA(atom, name, sizeof(name)))
        {
            TRACE("NtCreateSection: name=%s\n", atom? name : NULL);

            result = CreateFileMappingA(hFile, NULL, protect,
                                          size? (*size >> 32) : 0,
                                          size? (uint32_t)*size  : 0,
                                          atom? name : NULL);
        }

        if (result == INVALID_HANDLE_VALUE)
            WARN("NtCreateSection: failed!\n");
        else
            TRACE("NtCreateSection: returned %lx\n", (long)result);

        if (result != INVALID_HANDLE_VALUE)
            *retv            = result,
            _eax = STATUS_SUCCESS;
        else
            *retv            = result,
            _eax = STATUS_NO_MEMORY;   /* FIXME */
    }
    break;


    case 0x000E: /* NtOpenSection */
        /*
         * Input:   EDX: Flat address of arguments on stack
         *
         *   HANDLE32 *retv  [out] Handle of Section opened
         *   DWORD  protect  [in]  Access protection
         *   DWORD  atom     [in]  Name of Section to create
         *
         * Output:  EAX: NtStatus
         */
    {
        DWORD *stack    = (DWORD *)W32S_APP2WINE(_edx);
        HANDLE *retv  = (HANDLE *)W32S_APP2WINE(stack[0]);
        DWORD  protect  = stack[1];
        DWORD  atom     = stack[2];

        HANDLE result = INVALID_HANDLE_VALUE;
        char name[128];

        TRACE("NtOpenSection(%lx, %x, %x)\n",
                   (long)retv, protect, atom);

        if (atom && GlobalGetAtomNameA(atom, name, sizeof(name)))
        {
            TRACE("NtOpenSection: name=%s\n", name);

            result = OpenFileMappingA(protect, FALSE, name);
        }

        if (result == INVALID_HANDLE_VALUE)
            WARN("NtOpenSection: failed!\n");
        else
            TRACE("NtOpenSection: returned %lx\n", (long)result);

        if (result != INVALID_HANDLE_VALUE)
            *retv            = result,
            _eax = STATUS_SUCCESS;
        else
            *retv            = result,
            _eax = STATUS_NO_MEMORY;   /* FIXME */
    }
    break;


    case 0x000F: /* NtCloseSection */
        /*
         * Input:   EDX: Flat address of arguments on stack
         *
         *   HANDLE32 handle  [in]  Handle of Section to close
         *   DWORD *id        [out] Unique ID  (?? unclear ??)
         *
         * Output:  EAX: NtStatus
         */
    {
        DWORD *stack    = (DWORD *)W32S_APP2WINE(_edx);
        HANDLE handle   = (HANDLE)(uintptr_t)stack[0];
        DWORD *id       = (DWORD *)W32S_APP2WINE(stack[1]);

        TRACE("NtCloseSection(%lx, %lx)\n", (long)handle, (long)id);

        CloseHandle(handle);
        if (id) *id = 0; /* FIXME */

        _eax = STATUS_SUCCESS;
    }
    break;


    case 0x0010: /* NtDupSection */
        /*
         * Input:   EDX: Flat address of arguments on stack
         *
         *   HANDLE32 handle  [in]  Handle of Section to duplicate
         *
         * Output:  EAX: NtStatus
         */
    {
        DWORD *stack    = (DWORD *)W32S_APP2WINE(_edx);
        HANDLE handle   = (HANDLE)(uintptr_t)stack[0];
        HANDLE new_handle;

        TRACE("NtDupSection(%lx)\n", (long)handle);

        DuplicateHandle( GetCurrentProcess(), handle,
                         GetCurrentProcess(), &new_handle,
                         0, FALSE, DUPLICATE_SAME_ACCESS );
        _eax = STATUS_SUCCESS;
    }
    break;


    case 0x0011: /* NtMapViewOfSection */
        /*
         * Input:   EDX: Flat address of arguments on stack
         *
         *   HANDLE32 SectionHandle       [in]     Section to be mapped
         *   DWORD    ProcessHandle       [in]     Process to be mapped into
         *   DWORD *  BaseAddress         [in/out] Address to be mapped at
         *   DWORD    ZeroBits            [in]     (?? unclear ??)
         *   DWORD    CommitSize          [in]     (?? unclear ??)
         *   LARGE_INTEGER *SectionOffset [in]     Offset within section
         *   DWORD *  ViewSize            [in]     Size of view
         *   DWORD    InheritDisposition  [in]     (?? unclear ??)
         *   DWORD    AllocationType      [in]     (?? unclear ??)
         *   DWORD    Protect             [in]     Access protection
         *
         * Output:  EAX: NtStatus
         */
    {
        DWORD *  stack          = (DWORD *)W32S_APP2WINE(_edx);
        HANDLE   SectionHandle  = (HANDLE)(uintptr_t)stack[0];
        DWORD    ProcessHandle  = stack[1]; /* ignored */
        DWORD *  BaseAddress    = (DWORD *)W32S_APP2WINE(stack[2]);
        DWORD    ZeroBits       = stack[3];
        DWORD    CommitSize     = stack[4];
        LONGLONG *SectionOffset = (LONGLONG *)W32S_APP2WINE(stack[5]);
        DWORD *  ViewSize       = (DWORD *)W32S_APP2WINE(stack[6]);
        DWORD    InheritDisposition = stack[7];
        DWORD    AllocationType = stack[8];
        DWORD    Protect        = stack[9];

        LPBYTE address = (LPBYTE)(BaseAddress?
			W32S_APP2WINE(*BaseAddress) : 0);
        DWORD  access = 0, result;

        switch (Protect & ~(PAGE_GUARD|PAGE_NOCACHE))
        {
            case PAGE_READONLY:           access = FILE_MAP_READ;  break;
            case PAGE_READWRITE:          access = FILE_MAP_WRITE; break;
            case PAGE_WRITECOPY:          access = FILE_MAP_COPY;  break;

            case PAGE_EXECUTE_READ:       access = FILE_MAP_READ;  break;
            case PAGE_EXECUTE_READWRITE:  access = FILE_MAP_WRITE; break;
            case PAGE_EXECUTE_WRITECOPY:  access = FILE_MAP_COPY;  break;
        }

        TRACE("NtMapViewOfSection"
                   "(%lx, %x, %lx, %x, %x, %lx, %lx, %x, %x, %x)\n",
                   (long)SectionHandle, ProcessHandle, (long)BaseAddress,
                   ZeroBits, CommitSize, (long)SectionOffset, (long)ViewSize,
                   InheritDisposition, AllocationType, Protect);
        TRACE("NtMapViewOfSection: "
                   "base=%lx, offset=%lx, size=%x, access=%x\n",
                   (unsigned long)address, SectionOffset? (long)*SectionOffset : 0,
                   ViewSize? *ViewSize : 0, access);

        result = (DWORD)(uintptr_t)MapViewOfFileEx(SectionHandle, access,
                            SectionOffset? (*SectionOffset >> 32) : 0,
                            SectionOffset? (uint32_t)*SectionOffset : 0,
                            ViewSize? *ViewSize : 0, address);

        TRACE("NtMapViewOfSection: result=%x\n", result);

        if (W32S_WINE2APP(result))
        {
            if (BaseAddress) *BaseAddress = W32S_WINE2APP(result);
            _eax = STATUS_SUCCESS;
        }
        else
            _eax = STATUS_NO_MEMORY; /* FIXME */
    }
    break;


    case 0x0012: /* NtUnmapViewOfSection */
        /*
         * Input:   EDX: Flat address of arguments on stack
         *
         *   DWORD  ProcessHandle  [in]  Process (defining address space)
         *   LPBYTE BaseAddress    [in]  Base address of view to be unmapped
         *
         * Output:  EAX: NtStatus
         */
    {
        DWORD *stack          = (DWORD *)W32S_APP2WINE(_edx);
        DWORD  ProcessHandle  = stack[0]; /* ignored */
        LPBYTE BaseAddress    = (LPBYTE)W32S_APP2WINE(stack[1]);

        TRACE("NtUnmapViewOfSection(%x, %lx)\n",
                   ProcessHandle, (long)BaseAddress);

        UnmapViewOfFile(BaseAddress);

        _eax = STATUS_SUCCESS;
    }
    break;


    case 0x0013: /* NtFlushVirtualMemory */
        /*
         * Input:   EDX: Flat address of arguments on stack
         *
         *   DWORD   ProcessHandle  [in]  Process (defining address space)
         *   LPBYTE *BaseAddress    [in?] Base address of range to be flushed
         *   DWORD  *ViewSize       [in?] Number of bytes to be flushed
         *   DWORD  *unknown        [???] (?? unknown ??)
         *
         * Output:  EAX: NtStatus
         */
    {
        DWORD *stack          = (DWORD *)W32S_APP2WINE(_edx);
        DWORD  ProcessHandle  = stack[0]; /* ignored */
        DWORD *BaseAddress    = (DWORD *)W32S_APP2WINE(stack[1]);
        DWORD *ViewSize       = (DWORD *)W32S_APP2WINE(stack[2]);
        DWORD *unknown        = (DWORD *)W32S_APP2WINE(stack[3]);

        LPBYTE address = (LPBYTE)(BaseAddress? W32S_APP2WINE(*BaseAddress) : 0);
        DWORD  size    = ViewSize? *ViewSize : 0;

        TRACE("NtFlushVirtualMemory(%x, %lx, %lx, %lx)\n",
                   ProcessHandle, (long)BaseAddress, (long)ViewSize,
                   (long)unknown);
        TRACE("NtFlushVirtualMemory: base=%lx, size=%x\n",
                   (unsigned long)address, size);

        FlushViewOfFile(address, size);

        _eax = STATUS_SUCCESS;
    }
    break;


    case 0x0014: /* Get/Set Debug Registers */
        /*
         * Input:   ECX: 0 if Get, 1 if Set
         *
         *          EDX: Get: Flat address of buffer to receive values of
         *                    debug registers DR0 .. DR7
         *               Set: Flat address of buffer containing values of
         *                    debug registers DR0 .. DR7 to be set
         * Output:  None
         */

        FIXME("[0014] ECX=%x EDX=%x\n",
                   _ecx, _edx);

        /* FIXME */
        break;


    case 0x0015: /* Set Coprocessor Emulation Flag */
        /*
         * Input:   EDX: 0 to deactivate, 1 to activate coprocessor emulation
         *
         * Output:  None
         */

        TRACE("[0015] EDX=%x\n", _edx);

        /* We don't care, as we always have a coprocessor anyway */
        break;


    case 0x0016: /* Init Win32S VxD PSP */
        /*
         * If called to query required PSP size:
         *
         *     Input:  EBX: 0
         *     Output: EDX: Required size of Win32s VxD PSP
         *
         * If called to initialize allocated PSP:
         *
         *     Input:  EBX: LoWord: Selector of Win32s VxD PSP
         *                  HiWord: Paragraph of Win32s VxD PSP (DOSMEM)
         *     Output: None
         */

        if (_ebx == 0)
            _edx = 0x80;
        else
        {
            PDB16 *psp = MapSL( MAKESEGPTR( BX_reg(scp), 0 ));
            psp->nbFiles = 32;
            psp->fileHandlesPtr = MAKELONG(HIWORD(_ebx), 0x5c);
            memset((LPBYTE)psp + 0x5c, '\xFF', 32);
        }
        break;


    case 0x0017: /* Set Break Point */
        /*
         * Input:   EBX: Offset of Break Point
         *          CX:  Selector of Break Point
         *
         * Output:  None
         */

        FIXME("[0017] EBX=%x CX=%x\n",
                   _ebx, CX_reg(scp));

        /* FIXME */
        break;


    case 0x0018: /* VirtualLock */
        /*
         * Input:   ECX: Current Process
         *
         *          EDX: Flat address of arguments on stack
         *
         *   DWORD *retv     [out] TRUE if success, FALSE if failure
         *   LPVOID base     [in]  Flat address of range to lock
         *   DWORD  size     [in]  Size of range
         *
         * Output:  EAX: NtStatus
         */
    {
        DWORD *stack  = (DWORD *)W32S_APP2WINE(_edx);
        DWORD *retv   = (DWORD *)W32S_APP2WINE(stack[0]);
        LPVOID base   = (LPVOID) W32S_APP2WINE(stack[1]);
        DWORD  size   = stack[2];
        DWORD  result;

        TRACE("VirtualLock(%lx, %lx, %x)\n",
                   (long)retv, (long)base, size);

        result = VirtualLock(base, size);

        if (result)
            *retv            = TRUE,
            _eax = STATUS_SUCCESS;
        else
            *retv            = FALSE,
            _eax = STATUS_NO_MEMORY;  /* FIXME */
    }
    break;


    case 0x0019: /* VirtualUnlock */
        /*
         * Input:   ECX: Current Process
         *
         *          EDX: Flat address of arguments on stack
         *
         *   DWORD *retv     [out] TRUE if success, FALSE if failure
         *   LPVOID base     [in]  Flat address of range to unlock
         *   DWORD  size     [in]  Size of range
         *
         * Output:  EAX: NtStatus
         */
    {
        DWORD *stack  = (DWORD *)W32S_APP2WINE(_edx);
        DWORD *retv   = (DWORD *)W32S_APP2WINE(stack[0]);
        LPVOID base   = (LPVOID) W32S_APP2WINE(stack[1]);
        DWORD  size   = stack[2];
        DWORD  result;

        TRACE("VirtualUnlock(%lx, %lx, %x)\n",
                   (long)retv, (long)base, size);

        result = VirtualUnlock(base, size);

        if (result)
            *retv            = TRUE,
            _eax = STATUS_SUCCESS;
        else
            *retv            = FALSE,
            _eax = STATUS_NO_MEMORY;  /* FIXME */
    }
    break;


    case 0x001A: /* KGetSystemInfo */
        /*
         * Input:   None
         *
         * Output:  ECX:  Start of sparse memory arena
         *          EDX:  End of sparse memory arena
         */

        TRACE("KGetSystemInfo()\n");

        /*
         * Note: Win32s reserves 0GB - 2GB for Win 3.1 and uses 2GB - 4GB as
         *       sparse memory arena. We do it the other way around, since
         *       we have to reserve 3GB - 4GB for Linux, and thus use
         *       0GB - 3GB as sparse memory arena.
         *
         *       FIXME: What about other OSes ?
         */

        _ecx = W32S_WINE2APP(0x00000000);
        _edx = W32S_WINE2APP(0xbfffffff);
        break;


    case 0x001B: /* KGlobalMemStat */
        /*
         * Input:   ESI: Flat address of buffer to receive memory info
         *
         * Output:  None
         */
    {
        struct Win32sMemoryInfo
        {
            DWORD DIPhys_Count;       /* Total physical pages */
            DWORD DIFree_Count;       /* Free physical pages */
            DWORD DILin_Total_Count;  /* Total virtual pages (private arena) */
            DWORD DILin_Total_Free;   /* Free virtual pages (private arena) */

            DWORD SparseTotal;        /* Total size of sparse arena (bytes ?) */
            DWORD SparseFree;         /* Free size of sparse arena (bytes ?) */
        };

        struct Win32sMemoryInfo *info =
                       (struct Win32sMemoryInfo *)W32S_APP2WINE(_esi);

        FIXME("KGlobalMemStat(%lx)\n", (long)info);

        /* FIXME */
    }
    break;


    case 0x001C: /* Enable/Disable Exceptions */
        /*
         * Input:   ECX: 0 to disable, 1 to enable exception handling
         *
         * Output:  None
         */

        TRACE("[001c] ECX=%x\n", _ecx);

        /* FIXME */
        break;


    case 0x001D: /* VirtualAlloc called from 16-bit code */
        /*
         * Input:   EDX: Segmented address of arguments on stack
         *
         *   LPVOID base     [in]  Flat address of region to reserve/commit
         *   DWORD  size     [in]  Size of region
         *   DWORD  type     [in]  Type of allocation
         *   DWORD  prot     [in]  Type of access protection
         *
         * Output:  EAX: NtStatus
         *          EDX: Flat base address of allocated region
         */
    {
        DWORD *stack  = MapSL( MAKESEGPTR( LOWORD(_edx), HIWORD(_edx) ));
        LPVOID base   = (LPVOID)W32S_APP2WINE(stack[0]);
        DWORD  size   = stack[1];
        DWORD  type   = stack[2];
        DWORD  prot   = stack[3];
        DWORD  result;

        TRACE("VirtualAlloc16(%lx, %x, %x, %x)\n",
                   (long)base, size, type, prot);

        if (type & 0x80000000)
        {
            WARN("VirtualAlloc16: strange type %x\n", type);
            type &= 0x7fffffff;
        }

        result = (DWORD)(uintptr_t)VirtualAlloc(base, size, type, prot);

        if (W32S_WINE2APP(result))
            _edx = W32S_WINE2APP(result),
            _eax = STATUS_SUCCESS;
        else
            _edx = 0,
            _eax = STATUS_NO_MEMORY;  /* FIXME */
	TRACE("VirtualAlloc16: returning base %x\n", _edx);
    }
    break;


    case 0x001E: /* VirtualFree called from 16-bit code */
        /*
         * Input:   EDX: Segmented address of arguments on stack
         *
         *   LPVOID base     [in]  Flat address of region
         *   DWORD  size     [in]  Size of region
         *   DWORD  type     [in]  Type of operation
         *
         * Output:  EAX: NtStatus
         *          EDX: TRUE if success, FALSE if failure
         */
    {
        DWORD *stack  = MapSL( MAKESEGPTR( LOWORD(_edx), HIWORD(_edx) ));
        LPVOID base   = (LPVOID)W32S_APP2WINE(stack[0]);
        DWORD  size   = stack[1];
        DWORD  type   = stack[2];
        DWORD  result;

        TRACE("VirtualFree16(%lx, %x, %x)\n",
                   (long)base, size, type);

        result = VirtualFree(base, size, type);

        if (result)
            _edx = TRUE,
            _eax = STATUS_SUCCESS;
        else
            _edx = FALSE,
            _eax = STATUS_NO_MEMORY;  /* FIXME */
    }
    break;


    case 0x001F: /* FWorkingSetSize */
        /*
         * Input:   EDX: 0 if Get, 1 if Set
         *
         *          ECX: Get: Buffer to receive Working Set Size
         *               Set: Buffer containing Working Set Size
         *
         * Output:  NtStatus
         */
    {
        DWORD *ptr = (DWORD *)W32S_APP2WINE(_ecx);
        BOOL set = _edx;

        TRACE("FWorkingSetSize(%lx, %lx)\n", (long)ptr, (long)set);

        if (set)
            /* We do it differently ... */;
        else
            *ptr = 0x100;

        _eax = STATUS_SUCCESS;
    }
    break;


    default:
	VXD_BARF( "W32S" );
    }

}
#endif

void vxd_call(struct sigcontext *scp)
{
    if (_eip==1+DPMI_SEL_OFF(DPMI_VXD_VMM)) {
      D_printf("DPMI: VMM VxD called, ax=%#x\n", _LWORD(eax));
      VXD_VMM(scp);

    } else if (_eip==1+DPMI_SEL_OFF(DPMI_VXD_PageFile)) {
      D_printf("DPMI: PageFile VxD called, ax=%#x\n", _LWORD(eax));
      VXD_PageFile(scp);

    } else if (_eip==1+DPMI_SEL_OFF(DPMI_VXD_Reboot)) {
      D_printf("DPMI: Reboot VxD called, ax=%#x\n", _LWORD(eax));
      VXD_Reboot(scp);

    } else if (_eip==1+DPMI_SEL_OFF(DPMI_VXD_VDD)) {
      D_printf("DPMI: VDD VxD called, ax=%#x\n", _LWORD(eax));
      VXD_VDD(scp);

    } else if (_eip==1+DPMI_SEL_OFF(DPMI_VXD_VMD)) {
      D_printf("DPMI: VMD VxD called, ax=%#x\n", _LWORD(eax));
      VXD_VMD(scp);

    } else if (_eip==1+DPMI_SEL_OFF(DPMI_VXD_VXDLDR)) {
      D_printf("DPMI: VXDLDR VxD called, ax=%#x\n", _LWORD(eax));
      VXD_VXDLoader(scp);

    } else if (_eip==1+DPMI_SEL_OFF(DPMI_VXD_SHELL)) {
      D_printf("DPMI: SHELL VxD called, ax=%#x\n", _LWORD(eax));
      VXD_Shell(scp);

    } else if (_eip==1+DPMI_SEL_OFF(DPMI_VXD_VCD)) {
      D_printf("DPMI: VCD VxD called, ax=%#x\n", _LWORD(eax));
      VXD_Comm(scp);

    } else if (_eip==1+DPMI_SEL_OFF(DPMI_VXD_VTD)) {
      D_printf("DPMI: VTD VxD called, ax=%#x\n", _LWORD(eax));
      VXD_Timer(scp);

    } else if (_eip==1+DPMI_SEL_OFF(DPMI_VXD_CONFIGMG)) {
      D_printf("DPMI: CONFIGMG VxD called, ax=%#x\n", _LWORD(eax));
      VXD_ConfigMG(scp);

    } else if (_eip==1+DPMI_SEL_OFF(DPMI_VXD_ENABLE)) {
      D_printf("DPMI: ENABLE VxD called, ax=%#x\n", _LWORD(eax));
      VXD_Enable(scp);

    } else if (_eip==1+DPMI_SEL_OFF(DPMI_VXD_APM)) {
      D_printf("DPMI: APM VxD called, ax=%#x\n", _LWORD(eax));
      VXD_APM(scp);

    } else if (_eip==1+DPMI_SEL_OFF(DPMI_VXD_VTDAPI)) {
      D_printf("DPMI: VTDAPI VxD called, ax=%#x\n", _LWORD(eax));
      VXD_TimerAPI(scp);
    }
}
