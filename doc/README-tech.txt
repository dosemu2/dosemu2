  DOSEmu Technical Guide
  The DOSEmu team, Edited by Alistair MacDonald  <alis-
  tair@slitesys.demon.co.uk>
  For DOSEMU v0.67 pl5.0

  This document is the amalgamation of a series of technical README
  files which were created to deal with the lack of DOSEmu documenta-
  tion.
  ______________________________________________________________________

  Table of Contents:

  1.      Introduction

  2.      Accessing ports with dosemu

  2.1.    General

  2.2.    Port I/O access

  2.2.1.  System Integrity

  2.2.2.  System Security

  3.      The Virtual Flags

  4.      VM86PLUS, new kernel's vm86 for a full feature dosemu

  4.1.    Restrictions

  4.2.    Parts in the kernel that get changed for vm86plus

  4.2.1.  Changes to arch/i386/kernel/vm86.c

  4.2.1.1.        New vm86() syscall interface

  4.2.1.2.        Additional Data passed to vm86()

  4.2.1.3.        IRQ passing

  4.2.1.4.        Debugger support

  4.2.2.  Changes to arch/i386/kernel/ldt.c

  4.2.2.1.        New functioncode for `write' in modify_ldt syscall

  4.2.2.2.        `useable' bit in LDT descriptor

  4.2.2.3.        `present' bit in LDT selector

  4.2.3.  Changes to arch/i386/kernel/signal.c

  4.2.4.  Changes to arch/i386/kernel/traps.c

  4.3.    Abandoned `bells and whistles' from older emumodule

  4.3.1.  Kernel space LDT.

  4.3.2.  LDT Selectors accessing the `whole space'

  4.3.3.  Fast syscalls

  4.3.4.  Separate syscall interface (syscall manager)

  5.      Video Code

  5.1.    C files

  5.2.    Header files

  5.3.    Notes

  5.4.    Todo

  6.      The old way of generating a bootable DOSEmu

  6.1.    Boot ( 'traditional' method )

  6.1.1.  If you are

  6.1.2.  If you already have a HDIMAGE file

  6.1.3.  If you don't know how to copy files from/to the hdimage

  7.      New Keyboard Code

  7.1.    Whats New

  7.2.    Status

  7.3.    Keyboard server interface

  7.4.    Keyboard server structure

  7.4.1.  queue handling functions

  7.4.2.  The Front End

  7.4.2.1.        Functions in serv_xlat.c

  7.4.2.1.1.      putrawkey

  7.4.2.1.2.      putkey & others

  7.4.3.  The Back End

  7.4.3.1.        Queue Back End in keybint=on mode

  7.4.3.2.        Queue Back End in keybint=off mode

  7.4.3.3.        Functions in newkbd-server.c

  7.5.    Known bugs & incompatibilites

  7.6.    Changes from 0.61.10

  7.7.    TODO

  8.      IBM Character Set

  8.1.    What's new in configuration

  8.1.1.  IBM character set in an xterm

  8.1.2.  IBM character set at the console

  8.1.3.  IBM character set over a serial line into an IBM ANSI terminal

  8.2.    THE FUTURE by Mark Rejhon

  9.      Setting HogThreshold

  10.     Priveleges and Running as User

  10.1.   What we were suffering from

  10.2.   The new 'priv stuff'

  11.     Timing issues in dosemu

  11.1.   The 64-bit timers

  11.2.   DOS 'view of time' and time stretching

  11.3.   Non-periodic timer modes in PIT

  11.4.   Fast timing

  11.5.   PIC/PIT synchronization and interrupt delay

  11.6.   The RTC emulation

  11.7.   General warnings

  12.     Pentium-specific issues in dosemu

  12.1.   The pentium cycle counter

  12.2.   How to compile for pentium

  12.3.   Runtime calibration

  12.4.   Timer precision

  12.5.   Additional points

  13.     The DANG system

  13.1.   Description

  13.2.   Changes from last compiler release

  13.3.   Using DANG in your code

  13.4.   DANG Markers

  13.5.   DANG_BEGIN_MODULE / DANG_END_MODULE

  13.5.1. DANG_BEGIN_FUNCTION / DANG_END_FUNCTION

  13.5.2. DANG_BEGIN_REMARK / DANG_END_REMARK

  13.5.3. DANG_BEGIN_NEWIDEA / DANG_END_NEWIDEA

  13.5.4. DANG_FIXTHIS

  13.5.5. DANG_BEGIN_CHANGELOG / DANG_END_CHANGELOG

  13.6.   Usage

  13.7.   Future

  14.     mkfatimage -- Make a FAT hdimage pre-loaded with files

  15.     mkfatimage16 -- Make a large FAT hdimage pre-loaded with files

  16.     Documenting DOSEmu

  16.1.   Sections

  16.2.   Emphasising text

  16.3.   Lists

  16.4.   Quoting stuff

  16.5.   Special Characters

  16.6.   Cross-References & URLs

  16.6.1. Cross-References

  16.6.2. URLs

  16.7.   Gotchas

  17.     Sound Code

  17.1.   Current DOSEmu sound code Unfortunately I haven't documented
  this yet. However, the current code has been completely rewritten and
  has been designed to support multiple operating systems and sound
  systems. For details of the internal interface and any available
  patches see my WWW page at

  17.2.   Original DOSEMU sound code

  18.     DMA Code

  18.1.   Current DOSEmu DMA code Unfortunately I haven't documented
  this yet. However, the current code has been completely rewritten from
  this.

  18.2.   Original DOSEMU DMA code

  18.2.1. Adding DMA devices to DOSEMU

  19.     DOSEmu Programmable Interrupt Controller

  19.1.   Other features

  19.2.   Caveats

  19.3.   Notes on theory of operation:

  19.3.1. Functions supported from DOSEmu side

  19.3.1.1.       Functions that Interface with DOS:

  19.3.2. Other Functions

  19.4.   A (very) little technical information for the curious

  20.     DOSEMU debugger v0.4

  20.1.   Introduction

  20.2.   Files

  20.2.1. modules

  20.2.2. executable

  20.3.   Installation

  20.4.   Usage

  20.5.   Commands

  20.6.   Performance

  20.7.   Wish List

  20.8.   BUGS

  20.8.1. Known bugs

  21.     MARK REJHON'S 16550 UART EMULATOR

  21.1.   PROGRAMMING INFORMATION

  21.2.   DEBUGGING HELP

  21.3.   FOSSIL EMULATION

  21.4.   COPYRIGHTS

  22.     Recovering the console after a crash

  22.1.   The mail message

  23.     Net code

  24.     Software X386 emulation

  24.1.   The CPU emulator
  ______________________________________________________________________

  11..  IInnttrroodduuccttiioonn

  This documentation is derived from a number of smaller documents. This
  makes it easier for individuals to maintain the documentation relevant
  to their area of expertise. Previous attempts at documenting DOSEmu
  failed because the documentation on a large project like this quickly
  becomes too much for one person to handle.

  These are the technical READMEs. Many of these have traditionally been
  scattered around the source directories.

  22..  AAcccceessssiinngg ppoorrttss wwiitthh ddoosseemmuu

  This section written by Alberto Vignani <vignani@mbox.vol.it> , Aug
  10, 1997

  22..11..  GGeenneerraall

  For port I/O access the type iiooppoorrtt__tt has been defined; it should be
  an unsigned short, but the previous unsigned int is retained for
  compatibility. Also for compatibility, the order of parameters has
  been kept as-is; new code should use the port_real_xxx(port,value)
  function.  The new port code is selected at configuration time by the
  parameter --enable-new-port which will define the macro NEW_PORT_CODE.
  Files: portss.c is now no more used and has been replaced by
  n_ports.c; all functions used by 'old' port code have been moved to
  ports.c. Note that the code in portss.c (retrieved from Scott
  Buchholz's pre-0.61) was previously disabled (not used), because it
  had problems with older versions of dosemu.

  The _r_e_p_;_i_n_,_o_u_t instructions will been optimized so to call iopl() only
  once.

  22..22..  PPoorrtt II//OO aacccceessss

  Every process under Linux has a map of ports it is allowed to access.
  Too bad this map covers only the first 1024 (0x400) ports. For all the
  ports whose access permission is off, and all ports over 0x400, an
  exception is generated and trapped by dosemu.

  When the I/O permission (ioperm) bit for a port is ON, the time it
  takes to access the port is much lower than a microsecond (30 cycles
  on a P5-150); when the port is accessed from dosemu through the
  exception mechanism, access times are in the range of tenths of us
  (3000 cycles on the P5-150) instead.  It is easy to show that 99% of
  this time is spent in the kernel trap and syscalls, and the influence
  of the port selection method (table or switch) is actually minimal.

  There is nothing we can do for ports over 0x400, only hope that these
  slow access times are not critical for the hardware (generally they
  are not) and use the largest possible word width (i.e. do not break
  16- and 32-bit accesses).

  The 'old' port code used a switch...case mechanism for accessing
  ports, while now the table access (previously in the unused file
  portss.c) has been chosen, as it is much more clear, easy to maintain
  and not slower than the "giant switch" method (at least on pentium and
  up).

  There are two main tables in ports.c:

  +o  the _p_o_r_t _t_a_b_l_e, a 64k char array indexed by port number and storing
     the number of the handle to be used for that port. 0 means no
     handle defined, valid range is 1-253, 0xfe and 0xff are reserved.

  +o  the _h_a_n_d_l_e _t_a_b_l_e, an array of structures describing the properties
     for a port group and its associated functions:

       static struct _port_handler {
             unsigned char(*read_portb)  (ioport_t port_addr);
             void (*write_portb) (ioport_t port_addr, unsigned char byte);
             unsigned short(*read_portw) (ioport_t port_addr);
             void (*write_portw) (ioport_t port_addr, unsigned short word);
             char *handler_name;
             int irq, fd;
       } port_handler[EMU_MAX_IO_DEVICES];

  It works this way: when an I/O instruction is trapped (in do_vm86.c
  and dpmi.c) the standard entry points _p_o_r_t___i_n_b_w_d_,_p_o_r_t___o_u_t_b_w_d are
  called. They log the port access if specified and then perform a
  double indexed jump into the port table to the function responsible
  for the actual port access/emulation.

  Ports must be _r_e_g_i_s_t_e_r_e_d before they can be used. This is the purpose
  of the ppoorrtt__rreeggiisstteerr__hhaannddlleerr function, which is called from inside the
  various device initializations in dosemu itself, and of ppoorrtt__aallllooww__iioo,
  which is used for user-specified ports instead.  Some ports, esp.
  those of the video adapter, are registered an trapped this way only
  under X, while in console mode they are permanently enabled (ioperm
  ON). The ioperm bit is also set to ON for the user-specified ports
  below 0x400 defined as _f_a_s_t.  Of course, when a port has the ioperm
  bit ON, it is not trapped, and thus cannot be traced from inside
  dosemu.

  There are other things to consider:

  +o  system integrity

  +o  system security

  22..22..11..  SSyysstteemm IInntteeggrriittyy

  To block two programs from accessing a port without knowing what the
  other program does, this is the strategy dosemu takes:

  +o  If the port is not listed in /proc/ioports, no other program should
     access the port. Dosemu will register these ports. This will also
     block a second dosemu-process from accessing these ports.
     Unfortunately there is _n_o _k_e_r_n_e_l _c_a_l_l _y_e_t for registering a port
     range system-wide; see later for current hacks.

  +o  If the port is listed, there's probably a device that could use
     these ports. So we require the system administrator to give the
     name of the corresponding device. Dosemu tries to open this device
     and hopes this will block other from accessing. The parallel ports
     (0x378-0x37f) and /dev/lp1 act in this way.  To allow access to a
     port registered in /proc/ioports, it is necessary that the open on
     the device given by the system administrator succeeds. An open on
     /dev/null will always succeed, but use it at your own risk.

  22..22..22..  SSyysstteemm SSeeccuurriittyy

  If the strategy administrator did list ports in /etc/dosemu.conf and
  allows a user listed in /etc/dosemu.users to use dosemu, (s)he must
  know what (s)he is doing. Port access is inherently dangerous, as the
  system can easily be screwed up if something goes wrong: just think of
  the blank screen you get when dosemu crashes without restoring screen
  registers...

  33..  TThhee VViirrttuuaall FFllaaggss

  Info written by Hans <lermen@fgan.de> to describe the virtual flags
  used by DOSEmu

  +o  DOS sees only IF, but IF will never _r_e_a_l_l_y set in the CPU
     flagregister, because this would block Linux. So Linus maintains a
     virtual IF (VIF), which he sets and resets accordingly to CLI, STI,
     POPFx, IRETx.  Because the DOS programm cannot look directly into
     the flagregister (exception the low 8 bits, but the IF is in bit
     9), it does not realize, that the IF isn't set. To see it, it has
     to perform PUSHF and look at the stack.

     Well, but PUSHF or INTxx is intercepted by vm86 and then Linus
     looks at his virtual IF to set the IF _o_n the stack.  Also, if IRETx
     or POPFx happen, Linus is getting the IF _f_r_o_m the stack, sets VIF
     accordingly, but leaves the _r_e_a_l IF untouched.

  +o  Now, how is this realized? This is a bit more complicated.  We have
     3 places were the eflag register is stored in memory:
     vvmm8866ss..rreeggss..eeffllaaggss
        in user space, seen by dosemu

     ccuurrrreenntt-->>ttssss..vv8866ffllaaggss
        virtual flags, macro VEFLAGS

     ccuurrrreenntt-->>ttssss..vvmm8866__iinnffoo-->>rreeggss..eeffllaaggss
        the _r_e_a_l flags, CPU reg. VM86

     The last one is a kernel space copy of vm86_struct.regs.eflags.

     Also there are some masks to define, which bits of the flag should
     be passed to DOS and which should be taken from DOS:

     ccuurrrreenntt-->>ttssss..vv8866mmaasskk
        CPU model dependent bits

     SSAAFFEE__MMAASSKK       ((00xxDDDD55))
        used the way _t_o DOS

     RREETTUURRNN__MMAASSKK     ((00xxDDFFFF))
        used the way _f_r_o_m DOS

     When sys_vm86 is entered, it first makes a copy of the whole
     vm86_struct vm86s (containing regs.eflags) and saves a pointer to
     it to current->tss.vm86_info. It merges the flags from 32-bit user
     space (NOTE: IF is always set) over SAFE_MASK and cur-
     rent->tss.v86mask into current->tss.v86mask. From this point on,
     all changes to VIF, VIP (virtual interrupt pending) are only done
     in VEFLAGS.  To handle the flag setting there are macros within
     vm86.c, which do the following:

     sseett__IIFF,, cclleeaarr__IIFF
        only modifies VEFLAGS;

     cclleeaarr__TTFF
        sets a bit in the _r_e_a_l flags;

     sseett__vvffllaaggss((xx))
        set flags x over SAFE_MASK  to _r_e_a_l flags (IF is not touched)

     xx==ggeett__vvffllaaggss
        returns _r_e_a_l flags over RETURN_MASK and translates VIF of
        VEFLAGS to IF in x;

     When it's time to return from vm86() to user space, the _r_e_a_l flags
     are merged together with VIF and VIP from VEFLAGS and put into the
     userspace vm86s.regs.eflags. This is done by save_v86_state() and
     this does _n_o_t translate the VIF to IF, it should be as it was on
     entry of sys_vm86: set to 1.

  +o  Now what are we doing with eflags in dosemu ?  Well, this I don't
     really know. I saw IF used (told it Larry), I saw VIF tested an
     set, I saw TF cleared, and NT flag e.t.c.

     But I know what Linus thinks that we should do: Always interpret
     and set VIF, and let IF untouched, it will nevertheless set to 1 at
     entry of sys_vm86.

     How I think we should proceed? Well this I did describe in my last
     mail.
     ,,,, and this from a follow-up mail:

     _N_O_T_E VIF and VIP in DOS-CPU-flagsregister are inherited from
     32-bit, so actually they are both ZERO.

     On return to 32-bit, _o_n_l_y VIF will appear in vm86s.regs.eflags !
     _V_I_P _w_i_l_l _b_e _Z_E_R_O, again: _V_I_P will be used _o_n_l_y _o_n_c_e !!!!

     ,,,

     I have to add, that VIP will be cleared, because it is not in any
     of the masks of vm86.

  44..  VVMM8866PPLLUUSS,, nneeww kkeerrnneell''ss vvmm8866 ffoorr aa ffuullll ffeeaattuurree ddoosseemmuu

  ( available now in all kernels >= 2.0.28, >= 2.1.15 )

  The below gives some details on the new kernel vm86 functionality that
  is used for a `full feature dosemu'. We had more of those kernel
  changes in the older emumodule, but reduced the kernel support to an
  absolute minimum. As a result of this we now have this support in the
  mainstream kernels >= 2.0.28 as well as >= 2.1.15 and do not need
  emumodule any more (removed since dosemu 0.64.3). To distinguish
  between the old vm86 functionality and the new one, we call the later
  VM86PLUS.

  Written on January 14, 1997 by Hans Lermen <lermen@fgan.de>.

  44..11..  RReessttrriiccttiioonnss

  +o  Starting with dosemu-0.64.3 we will no longer support older kernels
     ( < 2.0.28 ) for vm86plus. If you for any reasons can't upgrade the
     kernel, then either use an older dosemu or don't configure the
     vm86plus support.

  +o  Please don't use any 2.1.x kernels ^lt; 2.1.15 and don't use the
     patch that came with dosemu-0.64.1, we changed the syscall
     interface because Linus wanted to be absolutely shure that no older
     (non-dosemu) binary would break.

     Also, don't use any dosemu binaries that were compiled under 2.1.x
     but earlier then 2.1.15.

  44..22..  PPaarrttss iinn tthhee kkeerrnneell tthhaatt ggeett cchhaannggeedd ffoorr vvmm8866pplluuss

  44..22..11..  CChhaannggeess ttoo aarrcchh//ii338866//kkeerrnneell//vvmm8866..cc

  44..22..11..11..  NNeeww vvmm8866(()) ssyyssccaallll iinntteerrffaaccee

  The vm86() syscall of vm86plus contains a generic interface: old style
  vm86 syscall is 113, the new one is 166.  At entry of vm86() the
  vm86_struct gets completely copied into kernel space and now _r_e_m_a_i_n_s
  on the kernel stack until control return to user space. This has the
  advantage that performance is increased as long as emulation loops
  between VM86 and kernel space ( which happens quite often ). A second
  advantage is, that we now better can translate between old
  vm86_struct, vm86plus_struct and kernel 2.1.x changed internal
  pt_regs, hence old vm86 and new vm86plus user space binaries run on
  both 2.0.x and 2.1.x kernel.  The entry routine of the old style
  vm86() translates to the new expanded vm86plus_struct before calling
  the common new do_sys_vm86().

  It is possible to detect the existence of vm86plus support in the
  kernel by just calling vm86(0,(void *)0) on syscall 166 entry.  On
  success 0 is returned, an unpatched kernel will return with -1.

  44..22..11..22..  AAddddiittiioonnaall DDaattaa ppaasssseedd ttoo vvmm8866(())

  When in vm86plus mode vm86() uses the new `struct vm86plus_struct'
  instead of `struct vm86_struct'. This contains some additional flags
  that are used to control whether vm86() should return earlier than
  usual to give the timer emulation in dosemu a chance to be in sync.
  Without this, updating the emulated timer chip happens too seldom and
  may even result in `jumping back', because the granulation is too big
  and rounding happens. As we don't know what granulation the DOS
  application is relying on, we can't emulate the expected behave, hence
  the application locks or crashes.  This especially happens when the
  application is doing micro timing.

  As a downside of `returning more often', we get DOS-space stack
  overflows, when we suck too much CPU. This we compensate by detecting
  this possibility and decreasing the `return rate', hence giving more
  CPU back to DOS-space.

  So we can realize a self adapting control loop with this feature.

  44..22..11..33..  IIRRQQ ppaassssiinngg

  Vm86plus also hosts the IRQ passing stuff now, that was a separate
  syscall in the older emumodule (no syscallmgr any more).  As this IRQ
  passing is special to dosemu, we anyway  couldn't it use for other
  (unix) applications. So having it as part of vm86() should be the
  right place.

  44..22..11..44..  DDeebbuuggggeerr ssuuppppoorrtt

  GDB is a great tool, however, we can't debug DOS and/or DPMI code with
  it. Dosemu has its own builtin debugger (dosdebug) which allows
  especially the dosemu developers to track down problems with dosemu
  and DOS applications for which (as usual) we have no source.  ( ...
  and debugging DOS applications always has been the `heart' of dosemu
  development ).

  Dosdebug uses some special flags and data in `vm86plus_struct', which
  are passed to vm86(), and vm86() reacts on it and returns back to
  dosemu with the dosdebug special return codes.

  As with dosemu-0.64.1 you now can run both debuggers simultaneously,
  dosdebug as well as GDB. Dosdebug will be triggered only for VM86
  traps and with GDB you may debug dosemu itself. However, GDB can't be
  used when DPMI is in use, because it will break on each trap that is
  used to simulate DPMI, you won't like that.

  44..22..22..  CChhaannggeess ttoo aarrcchh//ii338866//kkeerrnneell//llddtt..cc

  44..22..22..11..  NNeeww ffuunnccttiioonnccooddee ffoorr ``wwrriittee'' iinn mmooddiiffyy__llddtt ssyyssccaallll

  In order to preserve backword compatibility with Wine and Wabi the
  changes in the LDT stuff are only available when using function code
  0x11 for `write' in the modify_ldt syscall.  Hence old binaries will
  be served with the old LDT behavior.

  44..22..22..22..  ``uusseeaabbllee'' bbiitt iinn LLDDTT ddeessccrriippttoorr

  The `struct modify_ldt_ldt_s' got an additional bit: `useable'.  This
  is needed for DPMI clients that make use of the `available' bits in
  the descriptor (bit 52).  `available' means, the hardware isn't using
  it, but software can put information into.

  Because the kernel does not use this bit, its save and harmless.
  Windows 3.1 is such a client, but also some 32-bit DPMI clients are
  reported to need it. This bit only is used for 32-bit clients.  DPMI-
  function SetDescriptorAccessRights (AX=0009) passes this in bit 4 of
  CH ((80386 extended access rights).

  44..22..22..33..  ``pprreesseenntt'' bbiitt iinn LLDDTT sseelleeccttoorr

  The function 1 (write_ldt) of syscall modify_ldt() allows
  creation/modification of selectors containing a `present' bit, that
  get updated correctly later on. These selectors are setup so, that
  they _e_i_t_h_e_r can't be used for access (null-selector) _o_r the `present'
  info goes into bit 47 (bit 7 of type byte) of a call gate descriptor
  (segment present). This call gate of course is checked to not give any
  kernel access rights.  Hence, security will not be hurt by this.

  44..22..33..  CChhaannggeess ttoo aarrcchh//ii338866//kkeerrnneell//ssiiggnnaall..cc

  Because DPMI code switches via signal return, some type of selectors
  that the kernel normally would not allow to be loaded into a segment
  registers have been made loadable. The involved register are DS, ES FS
  and GS. Loading of CS or SS is not changed.

  The original kernel code would forbid any non-null selector that
  hasn't privilege level 3, and this also could be one of the LDT
  selectors. However, sys_sigreturn doesn't check the descriptors that
  belong to the selector, hence would not see that they are save.  But
  as we assure proper setting of _a_l_l LDT selector via `write_ldt' of
  modify_ldt(), we safely may allow LDT selectors to be loaded.  If they
  are not proper, we then get an exception and have a chance to emulate
  access. And because old type binaries (Wabi) will not be able create
  newer type selector (see 2.2.1), gain this wont hurt.

  44..22..44..  CChhaannggeess ttoo aarrcchh//ii338866//kkeerrnneell//ttrraappss..cc

  The low-level exception entry points for INTx (x= 0, 1..5, 6) in the
  kernel normally send a signal to the process, that then may handle the
  exception. For INT1 (debug), the kernel does special treatment and
  checks whether it gets interrupted from VM86.

  Due to limitation in how we can handle signals in dosemu without
  becoming to far behind `real time' and because we need to handle those
  things on the current vm86() return stack, we need to handle the above
  INTx in a similar manor then INT1.

  When INTx happens out of VM86 (i.e. the CPU was in virtual 8086 mode
  when the exception occurred), we do not send a signal, but return from
  the vm86() syscall with an appropriate return code.

  If the above INTx happens from within old style vm86() call, the
  exceptions also are handled `the old way'. (backward comptibility)
  44..33..  AAbbaannddoonneedd ``bbeellllss aanndd wwhhiissttlleess'' ffrroomm oollddeerr eemmuummoodduullee

  ( If you have an application that needs it, well then it won't work,
  and please don't ask us to re-implement the old behaviour.  We have
  good reasons for our decision. )

  44..33..11..  KKeerrnneell ssppaaccee LLDDTT..

  Some DPMI clients have really odd programming techniques that don't
  use the LAR instruction to get info from a descriptor but access the
  LDT directly to get it. Well, this is not problem with our user space
  LDT copy (LDT_ALIAS) as long as the DPMI client doesn't need a
  reliable information about the `accessed bit'.

  In the older emumodule we had a so called KERNEL_LDT, which (readonly)
  accessed the LDT directly in kernel space. This now has been abandoned
  and we use some workarounds which may (or may not) work for the above
  mentioned DPMI clients.

  44..33..22..  LLDDTT SSeelleeccttoorrss aacccceessssiinngg tthhee ``wwhhoollee ssppaaccee''

  DPMI clients may very well try to create selectors with a type and
  size that would overlap with kernel space, though the client normally
  only would access user space with such selectors (e.g. expand down
  segments).

  This was a security hole in the older Linux kernel, that was fixed in
  the early 1.3.x kernel series. Due to complaints on linux-msdos
  emumodule did allow those selectors if dosemu was run as root.
  Because only very few DOS applications are needing this (e.g. some odd
  programmed games), we now favourite security and don't allow this any
  more.

  44..33..33..  FFaasstt ssyyssccaallllss

  In order to gain speed and to be more atomic on some operations we had
  so called fast syscalls, that uses INT 0xe6 to quickly enter kernel
  space get/set the dosemu used IRQ-flags and return without letting the
  kernel a chance to reschedule.

  Today the machines perform much better, so there is no need for for
  those ugly tricks any more. In dosemu-0.64.1 fast syscalls are no
  longer used.

  44..33..44..  SSeeppaarraattee ssyyssccaallll iinntteerrffaaccee ((ssyyssccaallll mmaannaaggeerr))

  The old emumodule uses the syscallmgr interface to establish a new
  (temporary) system call, that was used to interface with emumodule.
  We now have integrated all needed stuff into the vm86 system call,
  hence we do not need this technique any more.

  55..  VViiddeeoo CCooddee

  I have largely reorganized the video code. Be aware that a lot of code
  is now in a different place, and some files were moved or renamed.

  55..11..  CC ffiilleess

     vviiddeeoo//vviiddeeoo..cc
        global variables, init/cleanup code video_init(), video_close(),
        load_file(), video_config_init()

     vviiddeeoo//iinntt1100..cc
        int10 emulation (formerly int10.h), Scroll, clear_screen,
        char_out, set_video_mode

     vviiddeeoo//vvcc..cc
        console switching code (formerly video.c)

     vviiddeeoo//ccoonnssoollee..cc
        console mode routines

     vviiddeeoo//eett44000000..cc,,
        video/s3.c, video/trident.c" card-dependent console/graphics
        switching routines

     vviiddeeoo//vvggaa..cc
        console mode? graphics?

     vviiddeeoo//tteerrmmiinnaall..cc
        terminal mode video update routines (ansi/ncurses)

     vviiddeeoo//XX..cc
        Xwindow stuff

  55..22..  HHeeaaddeerr ffiilleess

     vviiddeeoo//vvcc..hh
        def's relating to console switching (formerly include/video.h)

     iinncclluuddee//vviiddeeoo..hh
        def's for general video status variables and functions

  55..33..  NNootteess

  +o  we now have 'virtual' video support. Every video front-end defines
     a static, initialized structure 'Video_xxx' (e.g. Video_console)
     which contains (mostly) pointers to the appropriate routines for
     certain tasks.

     Please use this when implementing new video front ends!  (See
     video/video.h for more info)

  +o  most of the video code & defs is removed from emu.c and emu.h (now
     mostly in video/video.c)

  +o  int10.h no longer exists (now in video/int10.c)

  +o  sigalrm now again checks the dirty bit(s) in vm86s.screen_bitmap
     before calling update_screen.
     At the moment, it doesn't seem to achieve much, but it might help
     when we use mark's scroll detector, and will be absolutely
     necessary for a good X graphics support.

     Note that for all video memory modifications done outside of vm86
     mode we must now use set_dirty()  (defined in int10.c).  _t_h_i_s _i_s
     _c_u_r_r_e_n_t_l_y _d_i_s_a_b_l_e_d_. _T_o _e_n_a_b_l_e _i_t_, _s_e_t _V_I_D_E_O___C_H_E_C_K___D_I_R_T_Y _t_o _1 _i_n
     _i_n_c_l_u_d_e_/_v_i_d_e_o_._c_.

  55..44..  TTooddoo

  +o  cleanup terminal/console mode initialization (currently called from
     video_init() in video.c) termioInit(), termioClose() should
     probably be split up somehow (i.e. general/terminal/console)

  +o  fix bug: screen is not restored properly when killing dosemu in
     graphics mode

  +o  properly implement set_video_mode()   (int10.c)

  +o  cursor in console mode ok?

  +o  put int10.c into video.c?

  +o  put vc.c into console.c?

  66..  TThhee oolldd wwaayy ooff ggeenneerraattiinngg aa bboooottaabbllee DDOOSSEEmmuu

  Since dosemu-0.66.4 you will not need the complicated method of
  generating a bootable dosemu suite (see Quickstart). However, who
  knows, maybe you need the old information too ...

  66..11..  BBoooott (( ''ttrraaddiittiioonnaall'' mmeetthhoodd ))

  66..11..11..  IIff yyoouu aarree _n_e_w _t_o _D_O_S_E_M_U

  +o  Make _C_E_R_T_A_I_N that your first disk statement in /etc/dosemu.conf _I_S
     pointing to your hdimage!

  +o  Reboot DOS (the real version, not DOSEMU.)  Put a newly formatted
     diskette in your a: drive.  Run "sys a:" to transfer the system
     files to the diskette.  Copy SYS.COM from your DOS directory
     (usually C:\DOS) to the diskette.

  +o  Reboot Linux.  Run 'dos -A'.  After a short pause, the system
     should come up.  Try "dir c:" which should list the files on the
     harddrive distribution image.  If that works, run: "sys c:".  Exit
     dos by running "c:\xitemu".  If you have problems, hold down the
     <ctrl> and <alt> buttons simultaneously while pressing <pgdn>.
     (<ctrl><alt><pgdn> will automatically exit dosemu.)

  66..11..22..  IIff yyoouu aallrreeaaddyy hhaavvee aa HHDDIIMMAAGGEE ffiillee

  +o  If you have a previous version of DOSEMU running, you should copy
     the *.SYS, *.COM, & *.EXE files in the ./commands directory over to
     the hdimage drive that you boot from.  Some of these files have
     changed.

  66..11..33..  IIff yyoouu ddoonn''tt kknnooww hhooww ttoo ccooppyy ffiilleess ffrroomm//ttoo tthhee hhddiimmaaggee

  +o  Have a look at the recent mtools package (version 3.6 at time of
     writeing).  If you have the following line in /etc/mtools.conf

            drive g: file="/var/lib/dosemu/hdimage" partition=1 offset=128

  then you can use _a_l_l _t_h_e _m_t_o_o_l _c_o_m_m_a_n_d_s to access it, such as

           # mcopy g:autoexec.bat -
           Copying autoexec.bat
           @echo off
           echo "Welcome to dosemu 0.66!"

  all clear ? ;-)

  77..  NNeeww KKeeyybbooaarrdd CCooddee

  This file describes the new keyboard code which was written in late
  '95 for scottb's dosemu-0.61, and adapted to the mainstream 0.63 in
  mid-'96.

  It was last updated by R.Zimmermann <zimmerm@mathematik.uni-
  marburg.de> on 18 Sep 96 and updated by Hans <lermen@fgan.de> on 17
  Jan 97. ( correction notes marked *HH  -- Hans )

  77..11..  WWhhaattss NNeeww

  What's new in the new keyboard code? A lot.

  To the user:

  +o  Most of the keyboard-related bugs should have gone away. Hope I
     didn't introduce too many new ones (-: Keyboard emulation should be
     more accurate now; some keys are supported that weren't before,
     e.g. Pause.

  +o  The X { keycode } option is now obsolete. This was basically a bad
     hack to make things work, and was incompatible to X servers other
     than XFree86.

  To the dosemu hacker:

  +o  While the old code already claimed to be "client-server" (and was,
     to some extent), the new code introduces a clean, well-defined
     interface between the `server', which is the interface to DOS
     (int9, bios etc.), and the `clients', which are the interfaces to
     the user frontends supported by dosemu. Currently, clients are
     `raw', `slang' (i.e. terminal), and `X'.

     Clients send keystrokes to the server through the interface
     mentioned above (which is defined in "keyboard.h"), the most
     important functions being `putkey()' and `putrawkey()'.

  +o  The keyboard server was rewritten from scratch, the clients were
     heavily modified.

  +o  There is now general and efficient support for pasting large text
     objects.  Simply call paste_text().

  +o  The keyboard-related code is now largely confined to base/keyboard,
     rather than scattered around in various files.

  There is a compile-time option NEW_KBD_CODE (on by default) to
  activate the new keyboard code. The old stuff is still in there, but I
  haven't recently checked whether it still works, or even compiles.
  Once the new code is reasonably well tested I'll remove it.  ( *HH:
  the old code _i_s made workeable and remains ON per default, it will
  stay maintained for a while, so we can easily check where the bugs
  come from )

  Just like the old keyboard code, we still have the rawkeyboard=on/off
  and keybint=on/off modes.

  77..22..  SSttaattuuss

  Almost everything seems to work well now.

  The keyboard server should now quite accurately emulate all key
  combinations described the `MAKE CODES' & `SCAN CODES' tables of
  HelpPC 2.1, which I used as a reference.

  See below for a list of known bugs.

  What I need now is YOUR beta-testing... please go ahead and try if all
  your application's wierd key combinations work, and let me know if
  they don't.

  77..33..  KKeeyybbooaarrdd sseerrvveerr iinntteerrffaaccee

  This is all you should need to know if you just want to send
  keystrokes to DOS.

  Use the functions

  +o  putrawkey(t_rawkeycode code);

  +o  putkey(Boolean make, t_keysym key)

  +o  putkey_shift(Boolean make, t_keysym key, t_shiftstate shiftstate)

  You may also read (but not write!) the variable 'shiftstate' if
  necessary.

  ehm... see the DANG comments in base/newkbd-server.c for more
  information...

  _N_O_T_E_: the server's queue is limited to a relatively small number of
  keyboard events (currently 15). IMO, it is not a good idea to let the
  queue be arbitrarily long, as this would render the behaviour more
  incontrollable if the user typed a lot of mad things while a dos
  program wasn't polling the keyboard.

  For pasting, there is special support in base/keyboard/keyb_clients.c
  which runs on top of the server.

  77..44..  KKeeyybbooaarrdd sseerrvveerr ssttrruuccttuurree

  [_N_O_T_E_: you won't need to read this unless you actually want to modify
  the keyboard server code. In that case, however, you MUST read it!]

  [_N_o_t_e_: I'll have to update this. The queue backend works somewhat
  different now.]

  The central data structure of the keyboard server is the dosemu
  keyboard queue (to be distinguished from the bios keyboard buffer,
  which is run by int09 and int16).

  The keyboard server code can be largely divided into the `queue
  frontend' (serv_xlat.c, serv_maps.c), which does keycode translation,
  and the `queue backend' (serv_backend.c, serv_8042.c), which does the
  interfacing to DOS. The two sides communicate only through the queue.

  Each queue entry holds a data structure corresponding to (mostly) one
  keypress or release event. [The exception are the braindead 0xe02a /
  0xe0aa shift key emulation codes the keyboard processor `decorates'
  some kinds of keyboard events with, which for convenience are treated
  as seperate events.]

  Each queue entry holds a up to 4 bytes of raw keycodes for the port
  60h emulation, along with a 2-byte translated int16h keycode and the
  shift state after this event was processed.  Note that the bios_key
  field can be empty (=0), e.g. for shift keys, while the raw field
  should always contain something.

  77..44..11..  qquueeuuee hhaannddlliinngg ffuunnccttiioonnss

  +o  static inline Boolean queue_empty(void);

  +o  static inline void clear_queue(void);

  +o  static inline void write_queue(Bit16u bios_key,t_shiftstate
     shift,Bit32u raw);

  +o  static void read_queue(Bit16u *bios_key, t_shiftstate *shift,
     t_rawkeycode *raw);

  Accordingly, the keyboard code is largely divided into two parts,

  +o  the 'front end' of the queue, responsible for translating keyboard
     events into the 'queue entry' format.

  +o  the 'back end' of the queue, which reads the queue and sends
     keycodes to DOS

  77..44..22..  TThhee FFrroonntt EEnndd

        putrawkey() -------->----+
             \   \               |
              \   v              |
               \  translate()    |
                \     |          |
                 \    v          \    (t_rawkeycode[4])      /---QUEUE----\
            /->---\---|-----------*------------------------> [ raw        ]
           /       \  \  (t_keysym+char)                     [            ]
        putkey() ->-\--*--------------> make_bios_code() --> [ bios_key   ]
           \         \                                       [            ]
            \         v                           /--------> [ shiftstate ]
             \---> do_shift_keys()               /           \------------/
                       |                        /
                       v        (t_shiftstate) /
                   [shiftstate]---------------/

       --------->  data flow (&calls, sometimes)
       ,,,,,,,,,>  calls

  77..44..22..11..  FFuunnccttiioonnss iinn sseerrvv__xxllaatt..cc

  +o  static Boolean do_shift_keys(Boolean make, t_keysym key);

  +o  static Bit16u make_bios_code(Boolean make, t_keysym key, uchar
     ascii);

  +o  static uchar translate(t_keysym key);

  +o  static Boolean handle_dosemu_keys(t_keysym key);

  +o  void putrawkey(t_rawkeycode code);

  +o  void putkey(Boolean make, t_keysym key, uchar ascii);

  +o  void putkey_shift(Boolean make, t_keysym key, uchar ascii,
     t_shiftstate s);

  Any keyboard client or other part of dosemu wishing to send keyboard
  events to DOS will do so by calling one of the functions putrawkey,
  putkey, and putkey_shift.

  77..44..22..11..11..  ppuuttrraawwkkeeyy

  is called with a single raw scancode byte. Scancodes from subsequent
  calls are assembled into complete keyboard events, translated and
  placed into the queue.

  77..44..22..11..22..  ppuuttkkeeyy && ootthheerrss

  ,,,to be documented.

  77..44..33..  TThhee BBaacckk EEnndd

  77..44..33..11..  QQuueeuuee BBaacckk EEnndd iinn kkeeyybbiinntt==oonn mmooddee

                          EMULATOR SIDE        |    x86 SIDE
                                               |
                             ....[through PIC].|....................
                             :                 |           :        v
       QUEUE      .....> out_b_8042() --> [ port 60h ] ----:---> other_int9_handler
       |         :                             |        \  `.......    (:) (|)
       |         :                             |         \         v   (v) (|)
       +->int_chk_q()-> bios_buffer----> [ get_bios_key ]-----> default_int9_handler
             ^  \                           :  |                   |       (|)
             :   \----> shiftstate_buffer   :  |                   v       (v)
             :               |         .....:  |               bios keyb buffer
             :               v        v        |
             :          copy_shift_state() ----+-------------> bios shiftstate
             :                                 |
             :                                 |
             :                                 |
           backend_run()                       |

       Abbreviations:
       int_chk_q() = int_check_queue()
       out_b_8042() = output_byte_8042()

  77..44..33..22..  QQuueeuuee BBaacckk EEnndd iinn kkeeyybbiinntt==ooffff mmooddee

                         EMULATOR SIDE         |    x86 SIDE
                                               |
                    kbd_process()              |
                         :     :               |
                         :     v               |
        QUEUE -----------:--> put_keybuf() ----+-------------> bios keyb buffer
            \            v                     |
             \--------> copy_shift_state() ----+-------------> bios shiftstate
                                               |
                                               |
                                               |

  77..44..33..33..  FFuunnccttiioonnss iinn nneewwkkbbdd--sseerrvveerr..cc

  +o  void do_irq1();

  +o  void clear_keybuf();

  +o  static inline Boolean keybuf_full(void);

  +o  static inline void put_keybuf(Bit16u scancode);

  +o  void copy_shift_state(t_shiftstate shift);

  +o  static void kbd_process(void);

  Transfer of the keyboard events from the dosemu queue to DOS is done
  in two completely different ways, depending on the keybint setting in
  dosemu.conf:

     kkeeyybbiinntt==ooffff
        kbd_process() simply reads the queue until it finds a bios
        keycode (as we're not interested in raw scancodes without int9
        emulation), which it stores in the bios keyboard buffer, while
        also copying the shift state to the appropriate bios variables.

     kkeeyybbiinntt==oonn
        As soon as a key is stored into the empty queue, kbd_process()
        triggers IRQ1 through the PIC emulation, which some time later
        will call do_irq1().

        do_irq1() will prepare for the interrupt execution by reading
        from the queue and storing the values in the variables
        raw_buffer, shiftstate_buffer, and bios_buffer, and then call
        run_irq() to run the actual DOS interrupt handler.

        again, there are two cases:

     +o  the default int09 handler in the dosemu bios (base/bios_emu.S)
        will call the helper function get_bios_key(), which returns the
        translated bios keycode from bios_buffer and copies the
        shiftstate from shiftstate_buffer. The raw keycodes are not
        used.  get_bios_key() may also return 0 if no translated keycode
        is ready.

        The int9 handler will also call the `keyboard hook' int15h,
        ax=????.

     +o  if a dos application or TSR has redirected the keyboard
        interrupt, its handler might read from port 60h to get raw
        scancodes.  Port 60h is of course virtualized, and the read
        returns the value from raw_buffer.

        Note that a mix between the two cases is also possible, e.g. a
        TSR's int9 handler first reads port 60h to check if a particular
        key was pressed, then gives over to the default int9 handler.
        Even these cases should be (and are, I think) handled properly.

        Note also that in any case, int9 is called once for each raw
        scancode byte. Eg.,suppose the user pressed the PgDn key, whose
        raw scancode is E0 51:

     +o  first call to int9:

                     read port 60h        = 0xe0
                     read port 60h        = 0xe0   (**)
                     call get_bios_key()  = 0
                     iret

     do_irq1() reschedules IRQ1 because further scancodes are in the
     queue

     +o  second call to int9

                     read port 60h        = 0x51
                     call get_bios_key()  = 0x5100    (bios scancode of PgDn)
                     iret

     (** multiple port 60h reads during the same interrupt yield the
     same result.)

  This is not a complete documentation. If you actually want to hack the
  keyboard server, you can't avoid reading the code, I'm afraid ;-)

  77..55..  KKnnoowwnn bbuuggss && iinnccoommppaattiibbiilliitteess

  +o  behaviour wrt. cli/sti is inaccurate, because the PIC code
     currently doesn't allow un-requesting if IRQ's.

  +o  emulation of special 8042 and keyboard commands is incomplete and
     probably still somewhat faulty.

  +o  the 'internal' keyboard flags in seg 0x40, like E0 prefix received
     etc.  are never set. This shouldn't hurt, for all but the most
     braindead TSRs.

  +o  CAPS LOCK uppercase translation may be incorrect for some (non-
     german) national characters.

  +o  typematic codes in X and non-raw modes are Make+Break, not just
     Make.  This shouldn't hurt, though.

  +o  in X mode, shift+Gray cursor keys deliver numbers if NumLock is
     off.  This is an X problem, and AFIK nothing can be done about it.

  +o  in X, something may be wrong with F11+F12 handling (and again,
     possibly necessarily wrong).

  +o  the Pause key works in terms of raw scancodes, however it's
     function is not implemented (i.e. it doesn't actually halt DOS
     execution.)

  +o  in terminal (i.e. slang) mode, several things might be wrong or at
     least improveable.

  +o  there is no difference between the int16h functions 0,1 and the
     extended functions 0x10,0x11 - i.e. 0,1 don't filter out extended
     keycodes.

  +o  keyb.exe still doesn't work (hangs) - most probably due to the
     above.

  77..66..  CChhaannggeess ffrroomm 00..6611..1100

  +o  adapted to 0.63.55

  +o  adapted to 0.63.33

  +o  renamed various files

  +o  various minor cleanups

  +o  removed putkey_shift, added set_shiftstate

  +o  in RAW mode, read current shiftstate at startup

  +o  created base/keyboard/keyb_client.c for general client
     initialisation and paste support.

  77..77..  TTOODDOO

  +o  find what's wrong with TC++ 1.0

  +o  implement pause key

  +o  adapt x2dos (implement interface to send keystrokes from outside
     dosemu)

  +o  once everything is proved to work, remove the old keyboard code

  88..  IIBBMM CChhaarraacctteerr SSeett

  This is Mark Rejhon's version 2 of terminal support for the IBM
  graphics character set with a remote DOSEMU or a xterm DOSEMU.   Don't
  take these patches as final or original, as there is some cleaning up
  to do, Look forward to version 3 of the terminal support...! :-)

  Please give feedback or suggestions to Mark Rejhon at the
  <marky@magmacom.com> address.  Thanks!

  This prereleases updates supports:

  +o  "xdosemu" script to run DOSEMU in a normal xterm/rxvt!

  +o  IBM characters in telnet or kermit session from a Linux console.

  +o  IBM characters in an IBM character terminal like minicom or a DOS-
     based ANSI terminal.

  88..11..  WWhhaatt''ss nneeww iinn ccoonnffiigguurraattiioonn

  There is a new keyword called "charset" which accept either of these
  values: "latin" "ibm" and "fullibm".   View ./video/terminal.h for
  more information on these character set.  Here are the instructions on
  how to activate the 8-bit graphical IBM character set for DOSEMU:
  88..11..11..  IIBBMM cchhaarraacctteerr sseett iinn aann xxtteerrmm

  GREAT NEWS: You just use your existing ordinary "rxvt" or "xterm".
  Just install the VGA font by going into the DOSEMU directory and
  running the "xinstallvgafont" script by typing "./xinstallvgafont".
  Then just run "xdosemu" to start a DOSEMU window!

  88..11..22..  IIBBMM cchhaarraacctteerr sseett aatt tthhee ccoonnssoollee

  This will work if you are using the Linux console for telnet or
  kermit, to run a remote DOSEMU session.  Just simply run the
  "ibmtermdos" command on the remote end.  It puts the Linux console to
  use the IBM font for VT100 display, so that high ASCII characters are
  directly printable.

  _N_O_T_E_: This will not work with "screen" or any other programs that
  affect character set mapping to the Linux console screen.  For this,
  you should specify "charset fullibm" inside the "video { }"
  configuration in /etc/dosemu.conf.

  88..11..33..  IIBBMM cchhaarraacctteerr sseett oovveerr aa sseerriiaall lliinnee iinnttoo aann IIBBMM AANNSSII tteerrmmiinnaall

  Simply specify "charset fullibm" inside the "video { }" configuration
  in /etc/dosemu.conf and then run "dos" in the normal manner.  You must
  be running in an ANSI emulation mode (or a vt102 mode that is
  compatible with ANSI and uses the IBM character set.)

  88..22..  TTHHEE FFUUTTUURREE bbyy MMaarrkk RReejjhhoonn

  +o  NCURSES color to be added.  The IBM character set problem is
     solved.

  +o  clean up terminal code.

  +o  Add command line interface for character set and for serial port
     enable/disabling.

  +o  Use a separate "terminal { }" line for all the ncurses/termcap
     related configuration, instead of putting it in the "video { }"
     line.

  99..  SSeettttiinngg HHooggTThhrreesshhoolldd

  Greetings DOSEMU fans,

  Hogthreshold is a value that you may modify in your DOSEMU.CONF file.
  It is a measure of the "niceness" of Dosemu.  That is to say, it
  attempts to return to Linux while DOS is 'idling' so that DOSEMU does
  not hog all the CPU cycles while waiting at the DOS prompt.

  Determining the optimal Hogthreshold value involves a little bit of
  magic (but not so much really.)  One way is to try different values
  and look at the 'top' reading in another console.  Setting the value
  too low may mildly slow Dosemu performance.  Setting the value too
  high will keep the idling code from working.

  That said, a good basic value to try is "half of your Bogo-Mips
  value".  (The Bogo-Mips value is displayed when the kernel is booting,
  it's an imaginary value somewhat related to CPU performance.)

  Setting the value to 0 will disable idling entirely.  The default
  value is 10.

  This files is some kind of FAQ on how to use the 'HogThreshold' value
  in the dosemu config file.

  In case you have more questions feel free to ask me (
  <andi@andiunx.m.isar.de>).

  Those of you who simply want to have DOSEMU running at highest
  possible speed simply leave the value to zero, but if you are
  concerned about DOSEMU eating too much CPU time it's worth playing
  with the HogThreshold value.

     WWhhyy ddoo II nneeeedd ttoo sseett tthhee HHooggTThhrreesshhoolldd vvaalluuee,, wwhhyy ccaann''tt DDOOSSEEMMUU
        just stop if it is waiting for a keystroke ?"  The reason is the
        way how DOS and a lot of applications have implemented `waiting
        for a keystroke'.

        It's most often done by something similar to the following code
        fragment :

          wait_for_key:
                  ; do something
                  mov ah,1
                  int 0x16 ; check key status
                  jz      wait_for_key ; jump if no key
                  ; found a key
                  mov ah,0
                  int 0x16 ; get key

     This means that the application is busy waiting for the keystroke.

     WWhhaatt iiss aa ggoooodd vvaalluuee ffoorr HHooggTThhrreesshhoolldd ttoo ssttaarrtt wwiitthh ??
        On a 40 MHZ 486 start with a value of 10.  Increase this value
        if you to have your DOS application run faster, decrease it if
        you think too much CPU time is used.

     IItt ddooeess nnoott wwoorrkk oonn mmyy mmaacchhiinnee..
        You need to have at least dosemu0.53pl40 in order to have the
        anti-hog code in effect.

     WWhhyy nnoott ssiimmppllyy uussee aa vveerryy llooww vvaalluuee ooff
        Do I really have to try an individual value of HogThreshold ?"
        This would slow down your DOS application. But why not, DOS is
        slow anyway :-).

     HHooww ddoo II ffoouunndd oouutt aabboouutt CCPPUU uussaaggee ooff DDOOSSEEMMUU ??
        Simply use `top'. It displays cpu and memory usage.

  P.S.  If you want to change the HogThreshold value during execution,
  simply call

        mov al,12h
        mov bx,the_new_value
        int e6h

  This is what speed.com does. If you are interested, please take a look
  at speed.c.

  Notes:  If your application is unkind enough to do waits using an
  int16h fcn 1h loop without calling the keyboard idle interrupt (int
  28h), this code is not going to help much.  If someone runs into a
  program like this, let me ( <scottb@eecs.nwu.edu> ) know and I'll
  rewrite something into the int16 bios.

  1100..  TThhiiss sseeccttiioonn wwrriitttteenn bbyy HHaannss LLeerrmmeenn <<lleerrmmeenn@@ffggaann..ddee>> ,, AApprr 66,,
  11999977..  AAnndd uuppddaatteedd bbyy EErriicc BBiieeddeerrmmaann <<eebbiieeddeerrmm++eerriicc@@nnppwwtt..nneett>> 3300 NNoovv
  11999977..  PPrriivveelleeggeess aanndd RRuunnnniinngg aass UUsseerr

  1100..11..  WWhhaatt wwee wweerree ssuuffffeerriinngg ffrroomm

  Well, I got sick with the complaints about 'have problems running as
  user' and did much effort to 'learn' about what we were doing with
  priv-settings.  To make it short: It was a real mess, there was the
  whole dosemu history of different strategies to reach the same aim.
  Well, this didn't make it clearer. I appreciate all the efforts that
  were done by a lot of people in the team to make that damn stuff
  working, and atleast finding all those places _w_h_e_r_e we need to handle
  privs was a great work and is yet worth.

  The main odds happened because sometimes functions, that were called
  within a priv_on/off()...priv_default() brackets were calling
  priv_on/off()...priv_default() themselves. And with introduction of
  this priv_default() macro, which handled the 'default' case (run as
  user or run as root) the confusion was complete. Conclusion: recursive
  settings were not handled, and you always had to manually keep track
  of wether privs were on or off. ... terrible and not maintainable.

  Therefore I decided to do it 'the right way (tm)' and overworked it
  completely. The only thing that now remains to do, is to check for
  more places where we have to temporary allow/disallow root
  priviledges. I also decided to be a good boy and make 'RUN_AS_ROOT' a
  compile time option and 'run as user' the _d_e_f_a_u_l_t one.

  (promise: I'll also will run dosemu as user before releasing, so that
  I can see, if something goes wrong with it ;-)

  Enter Eric:

  What we have been suffering from lately is that threads were added to
  dosemu, and the stacks Hans added when he did it 'the right way (tm)'
  were all of a sudden large static variables that could not be kept
  consistent.  That and hans added caching of the curent uids for
  efficiency in dosemu, again more static variables.

  When I went through the first time and added all of the strange
  unmaintainable things Hans complains of above, and found where
  priveleges where actually needed I hand't thought it was as bad as
  Hans perceived it, so I had taken the lazy way out.  That and my main
  concern was to make the privelege setting consistent enough to not
  give mysterous erros in dosemu.  But I had comtemplated doing it 'the
  right way (tm)' and my scheme for doing it was a bit different from
  the way Hans did it.

  With Hans the stack was explicit but hidden behind the scenes.  With
  my latest incarnation the stack is even more explicit.  The elements
  of the privelege stack are now local variables in subroutines.  And
  these local variables need to be declared explicitly in a given
  subroutine.  This method isn't quite a fool proof as Han's method, but
  a fool could mess up Hans's method up as well.  And any competent
  person should be able to handle a local variable, safely.

  For the case of the static cached uid I have simply placed them in the
  thread control block.  The real challenge in integrating the with the
  thread code is the thread code was using root priveleges and changing
  it's priveleges with the priv code during it's initialization.   For
  the time being I have disabled all of the thread code's mucking with
  root priveleges, and placed it's initialization before the privelege
  code's initialization.  We can see later if I can make Han's thread
  code work `the right way (tm)' as he did for my privelege code.

  ReEnter Hans:  ;-)

  In order to have more checking wether we (not an anonymous fool) are
  forgetting something on the road, I modified Erics method such that
  the local variable is declared by a macro and preset with a magic,
  that is checked in priv.c. The below explanations reflect this change.

  1100..22..  TThhee nneeww ''pprriivv ssttuuffff''

  This works as follows

  +o  All settings have to be done in 'pairs' such as

               enter_priv_on();   /* need to have root access for 'do_something' */
               do_something();
               leave_priv_setting();

  or

               enter_priv_off();  /* need pure user access for 'do_something' */
               do_something();
               leave_priv_setting();

  +o  On enter_priv_XXX() the current state will be saved (pushed) on a
     local variable on the stack and later restored from that on
     leave_priv_setting(). This variable is has to be defined at entry
     of each function (or block), that uses a enter/leave_priv bracket.
     To avoid errors it has to be defined via the macro PRIV_SAVE_AREA.
     The 'stack depth' is just _one_ and is checked to not overflow.
     The enter/leave_priv_* in fact are macros, that pass a pointer to
     the local privs save area to the appropriate 'real_' functions.  (
     this way, we can't forget to include priv.h and PRIV_SAVE_AREA )
     Hence, you never again have to worry about previous priv settings,
     and whenever you feel you need to switch off or on privs, you can
     do it without coming into trouble.

  +o  We now have the system calls (getuid, setreuid, etc.) _o_n_l_y in
     src/base/misc/priv.c. We cash the setting and don't do unnecessary
     systemcalls. Hence NEVER call 'getuid', 'setreuid' etc. yourself,
     instead use the above supplied functions. The only places where I
     broke this 'holy law' myself was when printing the log, showing
     both values (the _r_e_a_l and the cached one).

  +o  In case of dosemu was started out of a root login, we skip _a_l_l
     priv-settings. There is a new variable 'under_root_login' which is
     only set when dosemu is started from a root login.

  +o  On all places were iopl() is called outside a
     enter_priv...leave_priv bracket, the new priv_iopl() function is to
     be used in order to force privileges.

  This is much cleaner and 'automagically' also solves the problem with
  the old 'priv_off/default' sheme. Because, 'running as user' just
  needs to do _one_ priv_off at start of dosemu, if we don't do it we
  are

  A last remark: Though it may be desirable to have a non suid root
  dosemu, this is not possible. Even if we get GGI in the official
  kernel, we can't solve the problems with port access (which we need
  for a lot of DOS apps) This leads to the fact that 'i_am_root' will be
  always '1', makeing it a macro gives GCC a changes optimize away the
  checks for it.  We will leave 'i_am_root' in, maybe there is some day
  in the future that gives us a kernel allowing a ports stuff without
  root privilege,

  Enter Eric:

  The current goal is to have a non suid-root dosemu atleast in X,
  lacking some features. But the reason I disabled it when I first
  introduced the infamous in this file priv_on/priv_default mechanism is
  that it hasn't been tested yet, still stands.  What remains to do is
  an audit of what code _needs_ root permissions, what code could
  benefit with a sgid dosemu, and what code just uses root permissions
  because when we are suid root some operations can only been done as
  root.

  When the audit is done (which has started with my most recent patch (I
  now know what code to look at)).  It should be possible to disable
  options that are only possible when we are suid root, at dosemu
  configuration time.  Then will be the task of finding ways to do
  things without root permissions.

  1111..  TTiimmiinngg iissssuueess iinn ddoosseemmuu

  This section written by Alberto Vignani <vignani@mbox.vol.it> , Aug
  10, 1997

  1111..11..  TThhee 6644--bbiitt ttiimmeerrss

  The idea for the 64-bit timers came, of course, from using the pentium
  cycle counter, and has been extended in dosemu to the whole timing
  system.

  All timer variables are now defined of the type hhiittiimmeerr__tt (an unsigned
  long long, see include/types.h).

  Timers in dosemu can have one of the following resolutions:

           MICROSECOND, for general-purpose timing
           TICK (838ns), for PIT/PIC emulation

  You can get the current time with the two functions

           GETusTIME    for 1-usec resolution
           GETtickTIME  for 838ns resolution

  The actual timer used (kernel or pentium TSC) is configured at runtime
  (it defaults to on for 586 and higher CPUs, but can be overridden both
  by the -2345 command-line switch or by the "rdtsc on|off" statement in
  the config file).

  The DOS time is now kept in the global variable ppiicc__ssyyss__ttiimmee.  See the
  DANG notes about emu-i386/cputime.c for details.

  1111..22..  DDOOSS ''vviieeww ooff ttiimmee'' aanndd ttiimmee ssttrreettcchhiinngg

  The time stretcher implements DOS 'view of time', as opposed to the
  system time.  It would be very easy to just give DOS its time, by
  incrementing it only when the dosemu process is active. To do that, it
  is enough to get SIGPROF from the kernel and (with some adjustments
  for sleeps) calculate the CPU% used by dosemu.  The real tricky part
  comes when we try to get this stuff back in sync with real time. We
  must 'stretch' the time, slowing it down and speeding it up, so that
  the DOS time is always trying to catch the real one.

  To enable the time stretcher start dosemu with the command-line option
  _-_t. If your CPU is not a 586 or higher, dosemu will exit with an error
  message.  Not that this algorithm doesn't run on a 486 - it was tested
  and was quite successful - but the use of gettimeofday() makes it a
  bit too heavy for such machines. Hence, it has been restricted to the
  pentium-class CPUs only, which can use the CPU timer to reduce use of
  kernel resources.

  1111..33..  NNoonn--ppeerriiooddiicc ttiimmeerr mmooddeess iinn PPIITT

  Normally, PIT timer 0 runs in a periodic mode (mode 2 or 3), it counts
  down to 0 then it issues an int8 and reinitializes itself. But many
  demos and games use it in one of the non-periodic (NP) modes (0 or 1):
  the timer counts down to 0, issues the interrupt and then stops. In NP
  modes, software intervention is required to keep the timer running.
  The NP modes were not implemented by dosemu-0.66, and this is the
  reason why some programs failed to run or required strange hacks in
  the dosemu code.  To be fair, most of these games ran also in
  dosemu-0.66 without any specific hack, but looking at the debug log
  always made me call for some type of divine intervention :-)

  The most common situation is: the DOS program first calibrates itself
  by calculating the time between two vertical screen retraces, then
  programs the timer 0 in a NP mode with a time slightly less than this
  interval; the int8 routine looks at the screen retrace to sync itself
  and then restarts the timer. You can understand how this kind of setup
  is very tricky to emulate in a multitasking environment, and sometimes
  requires using the eemmuurreettrraaccee feature. But, at the end, many demos
  that do not run under Win95 actually run under dosemu.

  1111..44..  FFaasstt ttiimmiinngg

  By "fast timing", I define a timer 0 period less than half of the
  Linux "jiffie" time (10ms). This is empirically determined - programs
  that use a timer period greater than 5ms usually run well with the
  'standard' timing of dosemu-0.66.

  There will always be a speed limit (the PIT can be programmed up to
  500kHz), but this is surely higher now. As an example, if you run M$
  Flight Simulator 0.5 with PC speaker enabled, as soon as a sound
  starts the PIT is set for an interrupt rate of 15kHz (and the int8
  routine is really nasty, in that it _keeps_count_ of the events);
  dosemu-0.66 just crashes, while now the sound is not perfect but
  reasonabily near to the real thing (but do not try to activate the
  debug log, or it will take forever!)

  Fast timing was not easy to achieve, it needs many tricks esp. to
  avoid interrupt bursts in PIC; it is still NOT completely working on
  slow machines (386,486) - but it will maybe never work for all cases.

  1111..55..  PPIICC//PPIITT ssyynncchhrroonniizzaattiioonn aanndd iinntteerrrruupptt ddeellaayy

  Another tricky issue... There are actually two timing systems for
  int8, the one connected to the interrupt in PIC, the other to port
  0x43 access in PIT.  Things are not yet perfectly clean, but at least
  now the two timings are kept in sync and can help one another.

  One of the new features in PIC is the correct emulation of interrupt
  delay when for any reason the interrupt gets masked for some time; as
  the time for int8 is delayed, it loses its sync, so the PIT value will
  be useful for recalculating the correct int8 time. (This case was the
  source for many int bursts and stack overflows in dosemu-0.66).

  The contrary happens when the PIT is in a NP mode; any time the timer
  is restarted, the PIT must be reset too. And so on.

  1111..66..  TThhee RRTTCC eemmuullaattiioonn

  There is a totally new emulation of the CMOS Real Time Clock, complete
  with alarm interrupt. A process ticks at exactly 1sec, always in real
  (=system) time; it is normally implemented with a thread, but can be
  run from inside the SIGALRM call when threads are not used.

  Also, int0x1a has been mostly rewritten to keep in sync with the new
  CMOS and time-stretching features.

  1111..77..  GGeenneerraall wwaarrnniinnggss

  Do not try to use games or programs with hi-freq timers while running
  heavy tasks in the background. I tried to make dosemu quite robust in
  such respects, so it will probably NOT crash, but, apart being
  unplayable, the game could behave in unpredictable ways.

  1122..  PPeennttiiuumm--ssppeecciiffiicc iissssuueess iinn ddoosseemmuu

  This section written by Alberto Vignani <vignani@mbox.vol.it> , Aug
  10, 1997

  1122..11..  TThhee ppeennttiiuumm ccyyccllee ccoouunntteerr

  On 586 and higher CPUs the 'rdtsc' instruction allows access to an
  internal 64-bit TimeStamp Counter (TSC) which increments at the CPU
  clock rate.  Access to this register is controlled by bit 2 in the
  config register cr4; hopefully under Linux this bit is always off,
  thus allowing access to the counter also from user programs at CPL 3.

  The TSC is part of a more general group of performance evaluation
  registers, but no other feature of these registers is of any use for
  dosemu.  Too bad the TSC can't raise an interrupt!

  Advantages of the TSC: it is extremely cheap to access (11 clock
  cycles, no system call).

  Drawbacks of using the TSC: you must know your CPU speed to get the
  absolute time value, and the result is machine-class specific, i.e.
  you can't run a binary compiled for pentium on a 486 or lower CPU
  (this is not the case for dosemu, as it can dynamically switch back to
  486-style code).

  1122..22..  HHooww ttoo ccoommppiillee ffoorr ppeennttiiuumm

  There are no special options required to compile for pentium, the CPU
  selection is done at runtime by parsing /proc/cpuinfo. You can't
  override the CPU selection of the real CPU, only the emulated one.

  1122..33..  RRuunnttiimmee ccaalliibbrraattiioonn

  At the very start of dosemu the bogospeed() function in
  base/init/config.c is called. This function first looks for the CPUID
  instruction (missing on 386s and early 486s), then looks for the
  CPUSPEED environment variable, and at the end tries to determine the
  speed itself.

  The environment variable CPUSPEED takes an integer value, which is the
  speed of your processor, e.g.:

           export CPUSPEED=200

  The last method used is the autocalibration, which compares the values
  of gettimeofday() and TSC over an interval of several hundred
  milliseconds, and is quite accurate AFAIK.

  You can further override the speed determination by using the
  statement

           cpuspeed div mul

  in your configuration file. The integer ratio (mul/div) allows to
  specify almost any possible speed (e.g. 133.33... will become '400
  3'). You can even slow down dosemu for debugging purposes (only if
  using TSC, however).

  The speed value is internally converted into two pairs of integers of
  the form {multiplier,divider}, to avoid float calculations. The first
  pair is used for the 1-usec clock, the second one for the tick(838ns)
  clock.

  1122..44..  TTiimmeerr pprreecciissiioonn

  I found no info about this issue. It is reasonable to assume that if
  your CPU is specified to run at 100MHz, it should run at that exact
  speed (within the tolerances of quartz crystals, which are normally
  around 10ppm). But I suspect that the exact speed depends on your
  motherboard.  If you are worried, there are many small test programs
  you can run under plain DOS to get the exact speed.  Anyway, this
  should be not a very important point, since all the file timings are
  done by calling the library/kernel time routines, and do not depend on
  the TSC.

  1122..55..  AAddddiittiioonnaall ppooiinnttss

  The experimental 'time stretching' algorithm is only enabled when
  using the pentium (with or without TSC). I found that it is a bit
  'heavy' for 486 machines and disallowed it.

  If your dosemu was compiled with pentium features, you can switch back
  to the 'standard' (gettimeofday()) timing at runtime by adding the
  statement

           rdtsc off

  in your configuration file.

  1133..  TThhee DDAANNGG ssyysstteemm

  1133..11..  DDeessccrriippttiioonn

  The DANG compiler is a perl script which produces a linuxdoc-sgml
  document from special comments embedded in the DOSEmu source code.
  This document is intended to give an overview of the DOSEmu code for
  the benefit of hackers.

  1133..22..  CChhaannggeess ffrroomm llaasstt ccoommppiilleerr rreelleeaassee

  +o  Recognizes 'maintainer:' information.

  +o

  +o  Corrections to sgml special character protection.

  1133..33..  UUssiinngg DDAANNGG iinn yyoouurr ccooddee

  TTHHEE FFOOLLLLOOWWIINNGG MMAAYY NNOOTT SSOOUUNNDD VVEERRYY NNIICCEE,, BBUUTT IISS IINNTTEENNDDEEDD TTOO KKEEEEPP DDAANNGG AASS
  IITT WWAASS DDEESSIIGGNNEEDD TTOO BBEE -- AA _G_U_I_D_E FFOORR _N_O_V_I_C_E_S..

  DANG is not intended to be a vehicle for copyrights, gratuitous plugs,
  or anything similar. It is intended to guide hackers through the maze
  of DOSEmu source code. The comments should be short and informative. I
  will mercilessly hack out comments which do not conform to this. If
  this damages anything, or annoys anyone, so be it.

  I spent hours correcting the DOSEmu 0.63.1 source code because some
  developers didn't follow the rules. They are very simple, and I'll
  remind you of the below. (I am here to co-ordinate this, not do the
  work. The comments are the responsibility of all of us.)

  1133..44..  DDAANNGG MMaarrkkeerrss

  Some initial comments:

  +o  All the text is passed through a text formatting system. This means
     that text may not be formatted how you want it to be. If you insist
     on having something formatted in a particular way, then it probably
     shouldn't be in DANG. Try a README instead. Embedding sgml tags in
     the comment will probably not work.

  +o  Copyrights and long detailed discussions should not be in DANG. If
     there is a good reason for including a copyright notice, then it
     should be included _O_N_C_E, prefereably in the group summary file (see

  +o  If I say something _m_u_s_t be done in a particular way, then it _M_U_S_T.
     There is no point in doing it differently because the script will
     not work correctly (and changing the script won't help anyone
     else.) In most cases the reason for a particular style is to help
     users who are actually reading the SOURCE file.

  1133..55..  DDAANNGG__BBEEGGIINN__MMOODDUULLEE // DDAANNGG__EENNDD__MMOODDUULLEE

  These should enclose a piece of summary text at the start of a file.
  It should explain the purpose of the file. Anything on the same line
  as DANG_BEGIN_MODULE is ignored. A future version may use the rest of
  this line to provide a user selected name for the module.  There may
  be any number of lines of text.  To include information on the
  maintainer(s) of a module, put 'maintainer:' on a blank line. The
  following lines should contain the maintainers details in form:

       name ..... <user@address>

  There should only be one maintainer on a line. There can be no other
  lines of description before DANG_END_MODULE.

  Example:

       /*
        * DANG_BEGIN_MODULE
        *
        * This is the goobledygook module. It provides BAR services. Currently there
        * are no facilities for mixing a 'FOO' cocktail. The stubs are in 'mix_foo()'.
        *
        * Maintainer:
        *      Alistair MacDonald      <alistair@slitesys.demon.co.uk>
        *      Foo Bar                 <foobar@inter.net.junk>
        *
        * DANG_END_MODULE
        */

  1133..55..11..  DDAANNGG__BBEEGGIINN__FFUUNNCCTTIIOONN // DDAANNGG__EENNDD__FFUUNNCCTTIIOONN

  This is used to describe functions. Ideally this should only be
  complicated or important functions as this avoids cluttering DANG with
  lots of descriptions for trivial functions. Any text on the same line
  as

  There are two optional sub-markers: 'description:' & 'arguments:'
  These can be included at any point, and the default marker is a
  description. The difference is that arguments are converted into a
  bulleted list. For this reason, there should be only one argument (&
  it's description) per line.  I suggest that arguments should be in the
  form 'name - description of name'.

  Example:

       /*
        * DANG_BEGIN_FUNCTION mix_foo
        *
        * This function provides the foo mixing for the module. The 'foo' cocktail
        * is used to wet the throat of busy hackers.
        *
        * arguments:
        *  soft_drink - the type of soft drink to use.
        *  ice_cream - the flavour of ice-cream required
        *
        * description:
        * A number of hackers would prefer something a little stronger.
        *
        * DANG_END_FUNCTION
        */

  1133..55..22..  DDAANNGG__BBEEGGIINN__RREEMMAARRKK // DDAANNGG__EENNDD__RREEMMAARRKK

  This is used to provide in-context comments within the code. They can
  be used to describe some particularly interesting or complex code. It
  should be borne in mind that this will be out of context in DANG, and
  that DANG is intended for Novice DOSEmu hackers too ...

  Example:

       /*
        * DANG_BEGIN_REMARK
        *
        * We select the method of preparation of the cocktail, according to the type
        * of cocktail being prepared. To do this we divide the cocktails up into :
        * VERY_ALCHOHOLIC, MILDLY_ALCHOHOLIC & ALCOHOL_FREE
        *
        * DANG_END_REMARK
        */

  1133..55..33..  DDAANNGG__BBEEGGIINN__NNEEWWIIDDEEAA // DDAANNGG__EENNDD__NNEEWWIIDDEEAA

  This is used to make a note of a new idea which we would like to have
  implemented, or an alternative way of coding something. This can be
  used as a scratch pad until the idea is in a state where someone can
  actually begin coding.

  Example:

       /*
        * DANG_BEGIN_NEWIDEA
        *
        * Rather than hard coding the names of the mixer functions we could try
        * using an array constructed at compile time - Alistair
        *
        * How to we get the list of functions ? - Foo
        *
        * DANG_END_NEWIDEA
        */

  1133..55..44..  DDAANNGG__FFIIXXTTHHIISS

  This is for single line comments on an area which needs fixing. This
  should say where the fix is required. This may become a multi-line
  comment in the future.

  Example:

       /* DANG_FIXTHIS The mix_foo() function should be written to replace the stub */

  1133..55..55..  DDAANNGG__BBEEGGIINN__CCHHAANNGGEELLOOGG // DDAANNGG__EENNDD__CCHHAANNGGEELLOOGG

  This is not currently used. It should be placed around the CHANGES
  section in the code. It was intended to be used along with the DPR.

  * No Example *

  1133..66..  UUssaaggee

  The current version of the configuration file resides in
  './src/doc/DANG'. The program needs to be told where to find this
  using '-c <file>'. On my system I run it in the './src/doc/DANG'
  directory using './DANG_c.pl -c DANG_CONFIG', but its really up to
  you.

  The other options are '-v' for verbose mode and '-i'.  -v isn't really
  useful, but it does tell you what it is doing at all times.  -i is
  'irritating mode' and makes comments in the DANG file about missing
  items, such as no MODULE information, no FUNCTION information, etc.

  If the program is executed with a list of files as parameters then the
  files will be processed, but no sgml output produced. This can be used
  for basic debugging, as it warns you when there are missing BEGIN/END
  markers, etc.

  1133..77..  FFuuttuurree

  I have vaguelly started writing the successor of this program. This
  will be a more general program with a more flexible configuration
  system. Don't hold your breath for an imminent release though. The new
  version will make it easier to change the markers & document
  structure, as well as providing feedback on errors.

  1144..  mmkkffaattiimmaaggee ---- MMaakkee aa FFAATT hhddiimmaaggee pprree--llooaaddeedd wwiitthh ffiilleess

  This section from Pasi Eronen <pe@iki.fi> (1995-08-28)

  To put it brief, mkfatimage creates a hdimage file for dosemu that's
  pre-loaded with the files specified on the command line. It's purpose
  is to remove the need to include a hdimage.dist file in the dosemu
  distribution (and it saves about 100K in the .tar.gz size on the way).

  I haven't done any modifications to the makefiles since they're
  changing all the time anyway, but the idea is to run mkfatimage after
  compiling dosemu to create a hdimage of the programs in the commands
  directory. An additional benefit is that we always get the most recent
  versions (since they were just compiled).

  As always, comments, suggestions, etc. are welcome.

  1155..  mmkkffaattiimmaaggee1166 ---- MMaakkee aa llaarrggee FFAATT hhddiimmaaggee pprree--llooaaddeedd wwiitthh ffiilleess

  I have also attached (gzipped in MIME format) mkfatimage16.c, a
  modified version of mkfatimage which takes an additional switch -t
  number of tracks and automatically switches to the 16-bit FAT format
  if the specified size is > 16MB.  I have deleted the "real" DOS
  partition from my disk, and so I am now running an entirely "virtual"
  DOS system within a big hdimage!

  1166..  DDooccuummeennttiinngg DDOOSSEEmmuu

  This is all about the new style DOSEmu documentation. When I was
  discussing this with Hans he was concerned (as I am) about the fact
  that we are all programmers - and generally programmers are poor at
  documentation. Of course, I'd love you all to prove me (us ?) wrong!

  However, _e_v_e_r_y programmer can handle the few basic linuxdoc-sgml
  commands that are need to make some really good documents! Much of
  linuxdoc-sgml is like HTML, which is hardly surprising as they are
  both SGMLs. The source to this document may make useful reading (This
  is the file './src/doc/README/doc')

  1166..11..  SSeeccttiioonnss

  There are 5 section levels you can use. They are all automatically
  numbered. Your choices are:

     <<sseecctt>>
        A top level section. This is indexed.

     <<sseecctt11>>
        A 1st level section. This is indexed.

     <<sseecctt22>>
        A 2nd level section. This is _n_o_t indexed.

     <<sseecctt33>>
        A 3rd level section. This is _n_o_t indexed.

     <<sseecctt44>>
        A 4th level section. This is _n_o_t indexed.

  +o  You _c_a_n_n_o_t skip section levels on the way down (ie you must go
     <sect>,<sect1>,<sect2> and not <sect>,<sect2>).  On the way back it
     doesn't matter!

  +o  Any text on the same line as the tag will be used to identify the
     section

  +o  You must have a <p> before you start the section text

  1166..22..  EEmmpphhaassiissiinngg tteexxtt

  I think there are lots of ways of doing this, but I've only been using
  2:

     <<eemm>>......<<//eemm>>
        Emphasises text like _t_h_i_s.

     <<bbff>>......<<//bbff>>
        Emboldens text like tthhiiss.

  There is a useful shorthand here. These (and many other tags) can be
  written either as:

  +o  <em>...</em>

  +o  <em/.../

  This second form can be very useful.

  1166..33..  LLiissttss

  Here we have 3 useful types:

     iitteemmiizzee
        A standard bulletted list

     eennuumm
        A numbered list

     ddeessccrriipp
        A description list (like this)

  For the ``itemize'' and ``enum'' lists the items are marked with
  <item>. eg:

       <itemize>
       <item> item 1
       <item> item 2
       </itemize>

  For the ``descrip'' lists the items are marked with either
  <tag>...</tag> or it's short form (<tag/.../). eg:

       <descrip>
       <tag>item 1</tag> is here
       <tag/item 2/ is here!
       </descrip>

  1166..44..  QQuuoottiinngg ssttuuffff

  If you want to quote a small amount use <tt>. eg:

  This is ./src/doc/README/doc

  To quote a large section, such as part of a file or an example use
  <tscreen> and <verb>. eg:

       <tscreen><verb>
       <descrip>
       ,,,
       </descrip>
       </verb></tscreen>

  Note that the order of closing the tags is the reverse of opening! You
  also still need to ``quote'' any < or > characters although most other
  characters should be OK.

  1166..55..  SSppeecciiaall CChhaarraacctteerrss

  Obviously some characters are going to need to be quoted. In general
  these are the same ones as HTML (eg < is written as &lt;) There are 2
  additional exceptions (which had me in trouble until I realised) which
  are [ and ]. These _m_u_s_t be written as &lsqb; and &rsqb;.

  1166..66..  CCrroossss--RReeffeerreenncceess && UURRLLss

  One of the extra feature that this lets us do is include URLs and
  cross-references.

  1166..66..11..  CCrroossss--RReeffeerreenncceess

  These have 2 parts: a label, and a reference.

  The label is <label id="...">

  The reference is <ref id="..." name="...">. The id should refer to a
  lable somewhere else in the document (not necessarily the same file as
  the REAMDE's consist of multiple files) The content of name will be
  used as the link text.

  1166..66..22..  UURRLLss

  This looks slightly horrible, but is very flexible. It looks quite
  similar to the reference above. It is <htmlurl url="..."  name="...">.
  The url will be active _o_n_l_y for HTML. The text in _n_a_m_e will be used at
  all times. This means that it is possible to make things like email
  addresses look good! eg:

       <htmlurl url="mailto:someone@no.where.com" name="&lt;someone@no.where.com&gt;">

  Which will appear as <someone@no.where.com>

  1166..77..  GGoottcchhaass

  +o  You _m_u_s_t do the sect*'s in sequence on the way up

  +o  You _m_u_s_t have a <p> after each sect*

  +o  You _m_u_s_t close your lists

  +o  You _m_u_s_t reverse the order of the tags when closing tscreen/verb
     structures

  +o  You _m_u_s_t replace <, >, [ and ]

  1177..  SSoouunndd CCooddee

  1177..11..  UUnnffoorrttuunnaatteellyy II hhaavveenn''tt ddooccuummeenntteedd tthhiiss yyeett.. HHoowweevveerr,, tthhee ccuurr--
  rreenntt ccooddee hhaass bbeeeenn ccoommpplleetteellyy rreewwrriitttteenn aanndd hhaass bbeeeenn ddeessiiggnneedd ttoo ssuupp--
  ppoorrtt mmuullttiippllee ooppeerraattiinngg ssyysstteemmss aanndd ssoouunndd ssyysstteemmss..  FFoorr ddeettaaiillss ooff tthhee
  iinntteerrnnaall iinntteerrffaaccee aanndd aannyy aavvaaiillaabbllee ppaattcchheess sseeee mmyy WWWWWW ppaaggee aatt
  hhttttpp::////wwwwww..sslliitteessyyss..ddeemmoonn..ccoo..uukk//aa..mmaaccddoonnaalldd//ddoosseemmuu//ssoouunndd// CCuurrrreenntt
  DDOOSSEEmmuu ssoouunndd ccooddee
  1177..22..  OOrriiggiinnaall DDOOSSEEMMUU ssoouunndd ccooddee

           Copyright (C) 1995  Joel N. Weber II

           This sound code is free software; you can redistribute it and/or modify
           it under the terms of the GNU General Public License as published by
           the Free Software Foundation; either version 2 of the License, or
           (at your option) any later version.

           This program is distributed in the hope that it will be useful,
           but WITHOUT ANY WARRANTY; without even the implied warranty of
           MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
           GNU General Public License for more details.

  This is a very incomplete sound blaster pro emulator.  I recomend you
  get the PC Game Programmer's Encycolpedia; I give the URL at the end
  of this document.

  The mixer emulator probably doesn't do the math precisely enough (why
  did Hannu use a round base 10 number when all the sound boards I know
  use base 2?)  It ignores obscure registers.  So repeatedly reading and
  writing data will zero the volume.  If you want to fix this, send
  Hannu a message indicating that you want the volume to be out of 255.
  It will probably fix the problem that he advertises with his driver:
  if you read the volume and write it back as is, you'll get zero volume
  in the end.  And on the obscure registers issue, it only pays
  attention to volumes and the dsp stereo bit.  The filters probably
  don't matter much, but the record source will need to be added some
  day.

  The dsp only supports reset and your basic dma playpack.  Recording
  hasn't been implemented, directly reading and writing samples won't
  work too well because it's too timing sensitive, and compression isn't
  supported.

  FM currently has been ignored.  Maybe there's a PCGPE newer than 1.0
  which describes the OPL3.  But I have an OPL3, and it would be nice if
  it emulated that.

  MIDI and joystick functions are ignored.  And I think that DOSEMU is
  supposed to already have good CDROM support, but I don't know how well
  audio CD players will work.

  If you're having performance problems with wavesample playback, read
  /usr/src/linux/drivers/sound/Readme.v30, which describes the fragment
  parameter which you currently can adjust in ../include/sound.h

  I haven't tested this code extensively.  All the software that came
  with my system is for Windows.  (My father claimed that one of
  Compaq's advantages is user friendlyness.  Well, user friendlyness and
  hackerfriendlyness don't exactly go hand in hand.  I also haven't
  found a way to disable bios shadowing or even know if it's
  happening...)  I can't get either of my DOS games to work, either
  (Descent and Sim City 2000).  Can't you guys support the Cirrus?  (Oh,
  and while I'm complaining, those mystery ports that SimEarth needs are
  for the FM synthesiser.  Watch it guys, you might generate interrupts
  with that....)

  Reference:

  PC Game Programers Encyclopedia
  ftp://teeri.oulu.fi/pub/msdos/programming/gpe/

  Joel Weber (age 15)

  July 17, 1995

  1188..  DDMMAA CCooddee

  1188..11..  UUnnffoorrttuunnaatteellyy II hhaavveenn''tt ddooccuummeenntteedd tthhiiss yyeett.. HHoowweevveerr,, tthhee ccuurr--
  rreenntt ccooddee hhaass bbeeeenn ccoommpplleetteellyy rreewwrriitttteenn ffrroomm tthhiiss..  CCuurrrreenntt DDOOSSEEmmuu DDMMAA
  ccooddee

  1188..22..  OOrriiggiinnaall DDOOSSEEMMUU DDMMAA ccooddee

           DOSEMU DMA code
           Copyright (C) 1995  Joel N. Weber II

           This dma code is free software; you can redistribute it and/or modify
           it under the terms of the GNU General Public License as published by
           the Free Software Foundation; either version 2 of the License, or
           (at your option) any later version.

           This program is distributed in the hope that it will be useful,
           but WITHOUT ANY WARRANTY; without even the implied warranty of
           MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
           GNU General Public License for more details.

  This is intended to be a reasonably complete implementation of dma.
  However, the following has been omitted:

  +o  There is no command register.  Half the bits control hardware
     details, two bits are related to memory to memory transfer, and two
     bits have misc. functions that might be useful execpt that they are
     global, so probably only the bios messes with them.  And dosemu's
     psuedo-bios certainly doesn't need them.

  +o  16 bit DMA isn't supported, because I'm not quite sure how it's
     supposed to work.  DMA channels 4-7 are treated the same way as 0-3
     execpt that they have different address.  Also, my currnet goal is
     a sb pro emulator, and the sb pro doesn't use 16 bit DMA.  So it's
     really a combination of lack of information and lack of need.

  +o  Verify is not supported.  It doesn't actually move any data, so how
     can it possibly be simulated?

  +o  The mode selection that determines whether to transfer one byte at
     a time or several is ignored because we want to feed the data to
     the kernel efficiently and let it control the speed.

  +o  Cascade mode really isn't supported; however all the channels are
     available so I don't consider it nessisary.

  +o  Autoinitialization may be broken.  From the docs I have, the
     current and reload registers might be written at the same time???
     I can't tell.

  +o  The docs I have refer to a "temporary register".  I can't figure
     out what it is, so guess what: It ain't implemented.  (It's only a
     read register).

  The following is known to be broken.  It should be fixed.  Any
  volunteers? :-)

  +o  dma_ff1 is not used yet

  +o  Address decrement is not supported.  As far as I know, there's no
     simple, effecient flag to pass to the kernel.

  +o  I should have used much more bitwise logic and conditional
     expressions.

  This is my second real C program, and I had a lot of experience in
  Pascal before that.

  1188..22..11..  AAddddiinngg DDMMAA ddeevviicceess ttoo DDOOSSEEMMUU

  Read include/dma.h.  In the dma_ch struct, you'll find some fields
  that don't exist on the real DMA controller itself.  Those are for you
  to fill in.  I trust that they are self-explainatory.

  One trick that you should know: if you know you're writing to a device
  which will fill up and you want the transfer to occur in the
  background, open the file with O_NONBLOCK.

  References:

  PC Game Programers Encyclopedia
  ftp://teeri.oulu.fi/pub/msdos/programming/gpe/

  The Intel Microprocessors: 8086/8088, 80186, 80286, 80386, 80486,
  Barry B. Brey,ISBN 0-02-314250-2,1994,Macmillan

  (The only reason I use this book so extensively is it's the only one
  like it that I can find in the Hawaii State Library System :-)

  1199..  DDOOSSEEmmuu PPrrooggrraammmmaabbllee IInntteerrrruupptt CCoonnttrroolllleerr

  This emulation, in files picu.c and picu.h, emulates all of the useful
  features of the two 8259 programmable interrupt controllers.  Support
  includes these i/o commands:

     IICCWW11    bbiittss 00 aanndd 11
        number of ICWs to expect

     IICCWW22    bbiittss 33 -- 77
        base address of IRQs

     IICCWW33    nnoo bbiittss
        accepted but ignored

     IICCWW44    nnoo bbiittss
        accepted but ignored

     OOCCWW11    aallll bbiittss
        sets interrupt mask

     OOCCWW22    bbiittss 77,,55--00
        EOI commands only

     OOCCWW33    bbiittss 00,,11,,55,,66
        select read register, select special mask mode
  Reads of both PICs ports are supported completely.

  1199..11..  OOtthheerr ffeeaattuurreess

  +o  Support for 16 additional lower priority interrupts.  Interrupts
     are run in a fully nested fashion.  All interrupts call dosemu
     functions, which may then determine if the vm mode interrupt should
     be executed.  The vm mode interrupt is executed via a function
     call.

  +o  Limited support for the non-maskable interrupt.  This is handled
     the same way as the extra low priority interrupts, but has the
     highest priority of all.

  +o  Masking is handled correctly: masking bit 2 of PIC0 also masks all
     IRQs on PIC1.

  +o  An additional mask register is added for use by dosemu itself.
     This register initially has all interrupts masked, and checks that
     a dosemu function has been registered for the interrupt before
     unmasking it.

  +o  Dos IRQ handlers are deemed complete when they notify the PIC via
     OCW2 writes.  The additional lower priority interrupts are deemed
     complete as soon as they are successfully started.

  +o  Interrupts are triggered when the PIC emulator, called in the vm86
     loop, detects a bit set in the interrupt request register.  This
     register is a global variable, so that any dosemu code can easily
     trigger an interrupt.

  1199..22..

  CCaavveeaattss

  OCW2 support is not exactly correct for IRQs 8 - 15.  The correct
  sequence is that an OCW2 to PIC0 enables IRQs 0, 1, 3, 4, 5, 6, and 7;
  and an OCW2 to PIC1 enables IRQs 8-15 (IRQ2 is really IRQ9).  This
  emulation simply enables everything after two OCW2s, regardless of
  which PIC they are sent to.

  OCW2s reset the currently executing interrupt, not the highest
  priority one, although the two should always be the same, unless some
  dos programmer tries special mask mode.  This emulation works
  correctly in special mask mode:  all types of EOIs (specific and non-
  specific) are treated as specific EOIs to the currently executing
  interrupt.  The interrupt specification in a specific EOI is ignored.

  Modes not supported:

  +o  Auto-EOI (see below)

  +o  8080-8085

  +o  Polling

  +o  Rotating Priority

  None of these modes is useable in a PC, anyway.  The 16 additional
  interrupts are run in Auto-EOI mode, since any dos code for them
  shouldn't include OCW2s.  The level 0 (NMI) interrupt is also handled
  this way.

  1199..33..

  NNootteess oonn tthheeoorryy ooff ooppeerraattiioonn::

  The documentation refers to levels.  These are priority levels, the
  lowest (0) having highest priority.  Priority zero is reserved for use
  by NMI.  The levels correspond with IRQ numbers as follows:

         IRQ0=1  IRQ1=2   IRQ8=3  IRQ9=4  IRQ10=5 IRQ11=6 IRQ12=7 IRQ13=8
         IRQ14=9 IRQ15=10 IRQ3=11 IRQ4=12 IRQ5=13 IRQ6=14 IRQ7=15

  There is no IRQ2; it's really IRQ9.

  There are 16 more levels (16 - 31) available for use by dosemu
  functions.

  If all levels were activated, from 31 down to 1 at the right rate, the
  two functions run_irqs() and do_irq() could recurse up to 15 times.
  No other functions would recurse; however a top level interrupt
  handler that served more than one level could get re-entered, but only
  while the first level started was in a call to do_irq().  If all
  levels were activated at once, there would be no recursion.  There
  would also be no recursion if the dos irq code didn't enable
  interrupts early, which is quite common.

  1199..33..11..  FFuunnccttiioonnss ssuuppppoorrtteedd ffrroomm DDOOSSEEmmuu ssiiddee

  1199..33..11..11..  FFuunnccttiioonnss tthhaatt IInntteerrffaaccee wwiitthh DDOOSS::

     uunnssiiggnneedd cchhaarr rreeaadd__ppiiccuu00((ppoorrtt)),,
        unsigned char read_picu1(port)" should be called by the i/o
        handler whenever a read of the PIC i/o ports is requested.  The
        "port" parameter is either a 0 or a 1; for PIC0 these correspond
        to i/o addresses 0x20 and 0x21.  For PIC1 they correspond with
        i/o addresses 0xa0 and 0xa1.  The returned value is the byte
        that was read.

     vvooiidd wwrriittee__ppiiccuu00((ppoorrtt,,uunnssiiggnneedd cchhaarr vvaalluuee)),,
        void write_picu1(port,unsigned char value)" should be called by
        the i/o handler whenever a write to a PIC port is requested.
        Port mapping is the same as for read_picu, above.  The value to
        be written is passed in parameter "value".

     iinntt ddoo__iirrqq(())
        is the function that actually executes a dos interrupt.
        do_irq() looks up the correct interrupt, and if it points to the
        dosemu "bios", does nothing and returns a 1.  It is assumed that
        the calling function will test this value and run the dosemu
        emulation code directly if needed.  If the interrupt is
        revectored to dos or application code, run_int() is called,
        followed by a while loop executing the standard run_vm86() and
        other calls.  The in-service register is checked by the while
        statement, and when the necessary OCW2s have been received, the
        loop exits and a 0 is returned to the caller.  Note that the 16
        interrupts that are not part of the standard AT PIC system will
        not be writing OCW2s; for these interrupts the vm86 loop is
        executed once, and control is returned before the corresponding
        dos code has completed.  If run_int() is called, the flags, cs,
        and ip are pushed onto the stack, and cs:ip is pointed to
        PIC_SEG:PIC_OFF in the bios, where there is a hlt instruction.
        This is handled by pic_iret.

        Since the while loop above checks for interrupts, this function
        can be re-entered.  In the worst case, a total of about 1k of
        process stack space could be needed.

     ppiicc__ssttii(()),,
        pic_cli()" These are really macros that should be called when an
        sti or cli instruction is encountered.  Alternately, these could
        be called as part of run_irqs() before anything else is done,
        based on the condition of the vm86 v_iflag.  Note that pic_cli()
        sets the pic_iflag to a non-zero value, and pic_sti() sets
        pic_iflag to zero.

     ppiicc__iirreett(())
        should be called whenever it is reasonably certain that an iret
        has occurred.  This call need not be done at exactly the right
        time, its only function is to activate any pending interrupts.
        A pending interrupt is one which was self-scheduled.  For
        example, if the IRQ4 code calls pic_request(PIC_IRQ4), the
        request won't have any effect until after pic_iret() is called.
        It is held off until this call in an effort to avoid filling up
        the stack.  If pic_iret() is called too soon, it may aggravate
        stack overflow problems.  If it is called too late, a self-
        scheduled interrupt won't occur quite as soon.  To guarantee
        that pic_iret will be called, do_irq saves the real return
        address on the stack, then pushes the address of a hlt, which
        generates a SIGSEGV.  Because the SIGSEGV comes before the hlt,
        pic_iret can be called by the signal handler, as well as from
        the vm86 loop.  In either case, pic_iret looks for this
        situation, and pops the real flags, cs, and ip.

  1199..33..22..  OOtthheerr FFuunnccttiioonnss

     vvooiidd rruunn__iirrqqss(())
        causes the PIC code to run all requested interrupts, in priority
        order.  The execution of this function is described below.

        First we save the old interrupt level.  Then *or* the interrupt
        mask and the in-service register, and use the complement of that
        to mask off any interrupt requests that are inhibited.  Then we
        enter a loop, looking for any bits still set.  When one is
        found, it's interrupt level is compared with the old level, to
        make sure it is a higher priority.  If special mask mode is set,
        the old level is biased so that this test can't fail.  Once we
        know we want to run this interrupt, the request bit is atomicly
        cleared and double checked (in case something else came along
        and cleared it somehow).  Finally, if the bit was actually
        cleared by this code, the appropriate bit is set in the in-
        service register.  The second pic register (for PIC1) is also
        set if needed.  Then the user code registered for this interrupt
        is called.  Finally, when the user code returns, the in-service
        register(s) are reset, the old interrupt level is restored, and
        control is returned to the caller.  The assembly language
        version of this funcction takes up to about 16 486 clocks if no
        request bits are set, plus 100-150 clocks for each bit that is
        set in the request register.

     vvooiidd ppiiccuu__sseettii((uunnssiiggnneedd iinntt lleevveell,,vvooiidd ((**ffuunncc)),, uunnssiiggnneedd iinntt iivveecc))
        sets the interrupt vector and emulator function to be called
        then the bit of the interrupt request register corresponding to
        level is set.  The interrupt vector is ignored if level<16,
        since those vectors are under the control of dos.  If the
        function is specified as NULL, the mask bit for that level is
        set.

     vvooiidd ppiiccuu__mmaasskkii((iinntt lleevveell))
        sets the emulator mask bit for that level, inhibiting its
        execution.

     vvooiidd ppiiccuu__uunnmmaasskk((iinntt iilleevveell))
        checks if an emulator function is assigned to the given level.
        If so, the mask bit for the level is reset.

     vvooiidd ppiicc__wwaattcchh(())
        runs periodically and checks for 'stuck' interrupt requests.
        These can happen if an irq causes a stack switch and enables
        interrupts before returning.  If an interrupt is stuck this way
        for 2 successive calls to pic_watch, that interrupt is
        activated.

  1199..44..  AA ((vveerryy)) lliittttllee tteecchhnniiccaall iinnffoorrmmaattiioonn ffoorr tthhee ccuurriioouuss

  There are two big differences when using pic.  First, interrupts are
  not queued beyond a depth of 1 for each interrupt.  It is up to the
  interrupt code to detect that further interrupts are required and
  reschedule itself.  Second, interrupt handlers are designated at
  dosemu initialization time.  Triggering them involves merely
  specifying the appropriate irq.

  Since timer interrupts are always spaced apart, the lack of queueing
  has no effect on them.  The keyboard interrupts are re-scheduled if
  there is anything in the scan_queue.

  2200..  DDOOSSEEMMUU ddeebbuuggggeerr vv00..44

  This section written on 7/10/96.  Send comments to Max Parke
  <mhp@light.lightlink.com> and to Hans Lermen <lermen@fgan.de>

  2200..11..  IInnttrroodduuccttiioonn

  This is release v0.4 of the DOSEMU debugger, with the following
  features:

  +o  interactive

  +o  DPMI-support

  +o  display/disassembly/modify of registers and memory (DOS and DPMI)

  +o  display/disassembly memory (dosemu code and data)

  +o  read-only access to DOSEMU kernel via memory dump and disassembly

  +o  uses /usr/src/dosemu/dosemu.map for above

  +o  breakpoints (int3-style, breakpoint on INT xx and DPMI-INT xx)

  +o  DPMI-INT breakpoints can have an AX value for matching.  (e.g.
     'bpintd 31 0203' will stop _before_ DPMI function 0x203)

  +o  symbolic debugging via microsoft linker .MAP file support

  +o  access is via the 'dosdebug' client from another virtual console.
     So, you have a "debug window" and the DOS window/keyboard, etc. are
     undisturbed.    VM86 execution can be started, stopped, etc.

  +o  If dosemu 'hangs' you can use the 'kill' command from dosbugger to
     recover.

  +o  code base is on dosemu-0.63.1.50

  2200..22..  FFiilleess

  All changes are #ifdef'ed with USE_MHPDBG

  2200..22..11..  mmoodduulleess

  +o

  +o

  +o

  +o

  +o

  2200..22..22..  eexxeeccuuttaabbllee

  +o

  2200..33..  IInnssttaallllaattiioonn

  In order to use DOSEMU debugger you must also use EMUMODULE.

  During ./configure be sure you have _N_O_T set --enable-noemumod

  2200..44..  UUssaaggee

  To run, start up DOSEMU.  Then switch to another virtual console (or
  remote login) and do:

    dosdebug

  If there are more then one dosemu process running, you will need to
  pass the pid to dosdebug, e.g:

         dosdebug 2134

  _N_O_T_E_: You must be the owner of the running dosemu to 'debug-login'.

  You should get connected and a banner message.  If you type 'q', only
  the terminal client will terminate, if you type 'kill', both dosemu
  and the terminal client will be terminated.

  2200..55..  CCoommmmaannddss

  See mhpdbgc.c for code and cmd table.

  (all numeric args in hex)

     ??  Print a help page

     qq  Quit the debug session

     kkiillll
        Kill the dosemu process (this may take a while, so be patient)
        See also ``Recovering the display''

     ccoonnssoollee nn
        Switch to console n

     rr  list regs

     rr rreegg vvaall
        change contents of 'reg' to 'val' (e.g: r AX 1234)

     ee AADDDDRR HHEEXXSSTTRR
        modify memory (0-1Mb)

     dd AADDDDRR SSIIZZEE
        dump memory (no limit)

     uu AADDDDRR SSIIZZEE
        unassemble memory (no limit)

     gg  go (if stopped)

     ssttoopp
        stop (if running)

     mmooddee 00||11||++dd||--dd
        set mode (0=SEG16, 1=LIN32) for u and d commands +d enables DPMI
        mode (default on startup), -d disables DPMI mode.

     tt  single step (not fully debugged!!!)

     ttff single step, force over IRET and POPF _N_O_T_E_: the scope of 't'
        'tf' or a 'come back for break' is either 'in DPMI' or realmode,
        depending on wether a DPMI-client is active (in_dpmi).

     rr3322
        dump regs in 32 bit format

     bbpp aaddddrr
        set int3 style breakpoint _N_O_T_E_: the scope is defined wether a
        DPMI-client is active (in_dpmi). The resulting 'come back' will
        force the mode that was when you defined the breakpoint.

     bbcc bbrreeaakkpp..NNoo..
        Clear a breakpoint.

     bbppiinntt xxxx
        set breakpoint on INT xx

     bbcciinntt xxxx
        clr breakpoint on INT xx

     bbppiinnttdd xxxx aaxx
        set breakpoint on DPMI INT xx optionaly matching ax.

     bbcciinnttdd xxxx aaxx
        clear  breakpoint on DPMI INT xx.

     bbppllooaadd
        set one shot breakpoint at entry point of the next loaded DOS-
        program.

     bbll list active breakpoints

     llddtt sseell lliinneess
        dump ldt starting at selector 'sel' for 'lines'

     ((rrmmaappffiillee))
        (internal command to read /usr/src/dosemu/dosemu.map at startup
        time)

     rruusseerrmmaapp oorrgg ffnn
        read microsoft linker format .MAP file "fn" code origin = "org".
        for example if your code is at 1234:0, org would be 12340.

  Addresses may be specified as:

  1. a linear address.  Allows 'd' and 'u' commands to look at both
     DOSEMU kernel and DOS box memory (0-1Mb).

  2. a seg:off address (0-1Mb) seg as well as off can be a symbolic
     registers name (e.g cs:eip) is prefixed by # (e.g. #00af:0000.  You
     may force a seg to treaten as LDT selector by prefixing the '#'.
     Accordingly to the default address mode 'off' under DPMI is 16 or
     32 bit.  When in DPMI mode, and you want to address/display
     realmode stuff, then you must switch off DPMI mode ('mode -d')

  3. a symbolic address.    usermap is searched first, then dosemu map.
     ( not for DPMI programms )

  4. an asterisk(*): CS:IP    (cs:eip)

  5. a dollar sign($): SS:SP  (ss:esp)

  2200..66..  PPeerrffoorrmmaannccee

  If you have dosemu compiled with the debugger support, but the
  debugger is not active and/or the process is not stopped, you will not
  see any great performance penalty.

  2200..77..  WWiisshh LLiisstt

  Main wish is to add support for hardware debug registers (if someone
  would point me in the direction, what syscalls to use, etc.)  Then you
  could breakpoint on memory reads/writes, etc!

  2200..88..  BBUUGGSS

  There must be some.

  2200..88..11..  KKnnoowwnn bbuuggss

  +o  Though you may set breakpoints and do singlestep in Windows31, this
     is a 'one shot': It will bomb _a_f_t_e_r you type 'g' again.  ( I
     suspect this is a timer problem, we _r_e_a_l_l_y should freeze the timer
     and all hardware/mouse IRQs while the program is in 'stop').
     Debugging and singlestepping through DJGPP code doesn't have any
     problems.

  +o  INT3 type breakpoints in DPMI code are _v_e_r_y tricky, because you
     never know when the client has remapped/freed the piece of code
     that is patched with 0xCC ( the one byte INT3 instruction ).  Use
     that with caution !!

  +o  Single stepping doesn't work correctly on call's. May be the trap-
     flag is lost.  However, when in DPMI the problems are minor.

  +o  popf sometime clears the trap-flag, so single stepping results in a
     'go' command.

  +o  When stopped for a long period, the BIOS-timer will be updated to
     fast and may result in stack overflow. We need to also stop the
     timer for dosemu.

  +o  When not stopped, setting break points doesn't work properly.  So,
     as a work around: Only set breakpoints while in stop.

  2211..  MMAARRKK RREEJJHHOONN''SS 1166555500 UUAARRTT EEMMUULLAATTOORR

  The ./src/base/serial directory contains Mark Rejhon's 16550A UART
  emulation code.

  Please send bug reports related to DOSEMU serial to either
  <marky@magmacom.com> or <ag115@freenet.carleton.ca>

  The files in this subdirectory are:

     MMaakkeeffiillee
        The Makefile for this subdir.

     sseerr__iinniitt..cc
        Serial initialization code.

     sseerr__ppoorrttss..cc
        Serial Port I/O emulation code.

     sseerr__iirrqq..cc
        Serial Interrupts emulation code.

     iinntt1144..cc
        BIOS int14 emulation code.

     ffoossssiill..cc
        FOSSIL driver emulation code.

     ....//ccoommmmaannddss//ffoossssiill..SS
        DOS part of FOSSIL emulation.

     sseerr__ddeeffss..hh
        Include file for above files only.

     ....//iinncclluuddee//sseerriiaall..hh
        General include file for all DOSEMU code.

  Redistribution of these files is permitted under the terms of the GNU
  Public License (GPL).  See end of this file for more information.

  2211..11..  PPRROOGGRRAAMMMMIINNGG IINNFFOORRMMAATTIIOONN

  Information on a 16550 is based on information from HELPPC.EXE 2.1 and
  results from National Semiconductor's COMTEST.EXE diagnostics program.
  This code aims to emulate a 16550A as accurately as possible, using
  just reasonably POSIX.2 compliant code.  In some cases, this code does
  a better job than OS/2 serial emulation (in non-direct mode) done by
  its COM.SYS driver!  There may be about 100 kilobytes of source code,
  but nearly 50% of this size are comments!

  This 16550A emulator never even touches the real serial ports, it
  merely traps port writes, and does the I/O via file functions on a
  device in /dev.  Interrupts are also simulated as necessary.

  One of the most important things to know before programming this
  serial code, is to understand how the com[] array works.

  Most of the serial variables are stored in the com[] array.  The com[]
  array is a structure in itself.   Take a look at the for more info
  about this.  Only the most commonly referenced global variables are
  listed here:

     ccoonnffiigg..nnuumm__sseerr
        Number of serial ports active.

     ccoomm[[xx]]..bbaassee__ppoorrtt
        The base port address of emulated serial port.

     ccoomm[[xx]]..rreeaall__ccoommppoorrtt
        The COM port number.

     ccoomm[[xx]]..iinntteerrrruupptt
        The PIC interrupt level (based on IRQ number)

     ccoomm[[xx]]..mmoouussee
        Flag  mouse (to enable extended features)

     ccoomm[[xx]]..ffdd
        File descriptor for port device

     ccoomm[[xx]]..ddeevv[[]]
        Filename of port port device

     ccoomm[[xx]]..ddeevv__lloocckkeedd
        Flag whether device has been locked

  The arbritary example variable 'x' in com[x] can have a minimum value
  of 0 and a maximum value of (config.numser - 1).  There can be no gaps
  for the value 'x', even though gaps between actual COM ports are
  permitted.  It is strongly noted that the 'x' does not equal the COM
  port number.  This example code illustrates the fact, and how the
  com[x] array works:

                 for (i = 0; i < config.numser; i++)
                   s_printf("COM port number %d has a base address of %x",
                            com[i].real_comport, com[i].base_port);

  2211..22..  DDEEBBUUGGGGIINNGG HHEELLPP

  If you require heavy debugging output for serial operations, please
  take a look in ./ser_defs.h for the following defines:

     SSEERR__DDEEBBUUGG__MMAAIINN   ((00 oorr 11))
        extra debug output on the most critical information.

     SSEERR__DDEEBBUUGG__HHEEAAVVYY   ((00 oorr 11))
        super-heavy extra debug output, including all ports reads and
        writes, and every character received and transmitted!

     SSEERR__DDEEBBUUGG__IINNTTEERRRRUUPPTT   ((00 oorr 11))
        additional debug output related to serial interrupt code,
        including flagging serial interrupts, or PIC-driven code.

     SSEERR__DDEEBBUUGG__FFOOSSSSIILL__RRWW   ((00 oorr 11))
        heavy FOSSIL debug output, including all reads and writes.

     SSEERR__DDEEBBUUGG__FFOOSSSSIILL__SSTTAATTUUSS   ((00 oorr 11))
        super-heavy FOSSIL debug output, including all status checks.

  2211..33..  FFOOSSSSIILL EEMMUULLAATTIIOONN

  The FOSSIL emulation requires a memory-resident DOS module,
  FOSSIL.COM. If you don't load it, the code in fossil.c does nothing,
  permitting you to use another FOSSIL driver such as X00 or BNU.

  The emulation isn't as complete as it could be. Some calls aren't
  implemented at all. However, the programs I've tried don't seem to use
  these calls. Check fossil.c if you're interested in details.

  I've done only minimal testing on this code, but at least the
  performance seems to be quite good. Depending on system load, I got
  Zmodem speeds ranging from 1500 to nearly 3800 cps over a 38400 bps
  null-modem cable when sending data to another PC. Zmodem receive,
  however, was only about 2200 cps, since Linux tried to slow down the
  sender with flow control (I'm not sure why).

  2211..44..  CCOOPPYYRRIIGGHHTTSS

  Most of the code in the DOSEMU 'serial' subdirectory is: Copyright (C)
  1995 by Mark D. Rejhon <marky@magmacom.com> with the following
  exceptions:

  FOSSIL driver emulation is Copyright (C) 1995 by Pasi Eronen
  <pasi@forum.nullnet.fi>.

  Lock-file functions were derived from Taylor UUCP: Copyright (C) 1991,
  1992 Ian Lance Taylor Uri Blumenthal <uri@watson.ibm.com> (C) 1994
  Paul Cadach, <paul@paul.east.alma-ata.su> (C) 1994

  UART definitions were derived from /linux/include/linux/serial.h
  Copyright (C) 1992 by Theodore Ts'o.

  All of this serial code is free software; you can redistribute it
  and/or modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.

  2222..  RReeccoovveerriinngg tthhee ccoonnssoollee aafftteerr aa ccrraasshh

  The below is a mail from Kevin Buhr <buhr@stat.wisc.edu> , that was
  posted on linux-msdos some time ago. Because it describes a way to
  recover from a totally locked console, the technique described below
  was partially intergrated in dosdebug. So, the below 'switchcon.c' is
  now part of dosdebug and you may use it via:

         dosdebug
         console n

  where n is the console you want to switch to.

  But keep in mind, that dosdebug tries to kill your dosemu process the
  safest way it can, so first use dosdebug's kill command:

         dosdebug
         kill

  In the worst case you will get the following output on your remote
  terminal:

     ...oh dear, have to do kill SIGKILL
     dosemu process (pid 1234) is killed
     If you want to switch to an other console,
     then enter a number between 1..8, else just type enter:
     2      <========= this is what you enter
     dosdebug terminated
     NOTE: If you had a totally locked console,
           you may have to blindly type in 'kbd -a; texmode
           on the console you switched to.

  2222..11..  TThhee mmaaiill mmeessssaaggee

  Date: Fri, 21 Apr 95 14:16 CDT
  To: tegla@katalin.csoma.elte.hu
  Cc: linux-msdos@vger.rutgers.edu
  In-Reply-To: <Pine.LNX.3.91.950421163705.1348B-100000@katalin.csoma.elte.hu> (message from Nagy Peter on Fri, 21 Apr 1995 16:51:27 +0200 (MET DST))
  Subject: Restoring text mode (was Re: talk)
  From: buhr@stat.wisc.edu (Kevin Buhr)
  Sender: owner-linux-msdos@vger.rutgers.edu
  Precedence: bulk
  Status: RO
  X-Status:

  | But when dosemu dies in graphics mode ( this happens every 30 minutes
  | or so), it leaves the screen in graphics mode. You can do anything
  | blindly (even start dosemu again) but the console screen is always left
  | in graphics mode.

  I know what you mean... this is a real pain in the ass.

  Here's my solution.  A few useful scripts and programs are supplied
  with the SVGA binaries.  "savetextmode" is a script that will write
  register and font information to "/tmp/textregs" and "/tmp/fontdata".
  Run this from the console as root while you're in text mode.  If
  you've got a cron job that clears out your "/tmp" directory, you'll
  probably want to copy these someplace safe.

  The next time "dosemu" or something similar takes out your video, use
  the "textmode" script (which reads the register and font from those
  temporary files and also restores the palette), and everything should
  be back to normal.  Of course, this assumes you're able to get enough
  control of your computer to enter the "textmode" command as root at
  the console ("restoretextmode" complains if executed from a terminal
  other than the console).  One solution is to modify the source for
  "restoretextmode" to operate correctly from off-console.

  I'm lazy, so I use a little program called "switchcon" (source
  attached) that takes a single integer argument and switches to that
  virtual console.

  So, if "dosemu" dies hard (so that "ctrl-alt-pagedown" doesn't work)
  or exits without restoring text mode, I do this:

          (1)  Log in from another terminal
          (2)  Kill "dosemu", if necessary.  Killing with SIGTERM will
               usually restore text mode automatically, but if I have
               to SIGKILL it, I continue...
          (3)  Run "switchcon" as root to switch to another VC
          (4)  Sometimes I have to run "kbd_mode -a", too, if I'm stuck
               in raw mode

  If Linux fails to automatically restore text mode, I log in (blindly)
  as root on the console and run "textmode".  With the canned register
  and font files in place, this inevitably brings me back to text mode
  bliss.

  Kevin <buhr@stat.wisc.edu>

                          *       *       *

  switchcon.c:

  #include <stdio.h>
  #include <stdlib.h>
  #include <fcntl.h>
  #include <linux/vt.h>
  #include <sys/ioctl.h>

  main( int argc, char **argv ) {
     int newvt;
     int vt;

     if(argc != 2 || !(newvt = atoi(argv[1]))) {
        fprintf(stderr, "syntax: switchcon number\n");
        exit(2);
     }

     vt = open( "/dev/tty1", O_RDONLY );
     if( vt == -1 ) {
        perror("open(/dev/tty1)");
        exit(1);
     }
     if( ioctl( vt, VT_ACTIVATE, newvt ) ) {
        perror("ioctl(VT_ACTIVATE)");
        exit(1);
     }
     if( ioctl( vt, VT_WAITACTIVE, newvt ) ) {
        perror("ioctl(VT_WAITACTIVE)");
        exit(1);
     }

     close(vt);

     return(0);
  }

  2233..  NNeett ccooddee

  +o  Added support for multiple type handling.  So it does type
     demultiplexing within dosemu.

     So we are able to run telbin and pdipx/netx  simultaneously ;-)

  +o  Changed the code for F_DRIVER_INFO. If the handle is '0', it need
     not be valid handle (when input to this function).  At least CUTCP
     simply set this value to zero when it requests for info for the
     first time. The current code wrongly treated it as valid handle,
     and returned the class of first registered type.

  +o  Some things still to be done ...

  +o  Novell Hack is currently disabled.

  +o  Compare class along with type when a new packet arrives.

  +o  Lots of clean ups required. (I have put in lots of pd_printf's.)
     Need to be removed even if debug is not used because function call
     within packet handler is very costly ...

  +o  change all 'dsn0' / 'dosnet' to vnet (i.e. virtual net.) or so.

     and

  +o  Use of SIGIO ...

  +o  Special protocol stack to do demultiplexing of types??

  +o  Use of devices instead of the current mechanism of implementing v-
     net?

  _N_O_T_E_: THERE ARE STILL BUGS.  The dosemu session just hangs. -- Usually
  when in CUTCP. (And specially when I open two sessions.)  What is
  strange is, it seems to work fine for some time even when two sessions
  are opened.  I strongly feel  it is due to new PIC code because it is
  used heavily by the network code ... (especially in interactive telnet
  sessions.)

  Vinod <vinod@cse.iitb.ernet.in>

  2244..  SSooffttwwaarree XX338866 eemmuullaattiioonn

  This section written in a hurry by Alberto Vignani
  <vignani@mbox.vol.it> , Oct 20, 1997

  2244..11..  TThhee CCPPUU eemmuullaattoorr

  The CPU emulator has been derived from <the Twin Willows libraries>.
  Only the relevant parts of the library, namely the /intp32
  subdirectory and the needed include files, have been extracted from
  the Twin sources into the src/twin directory. The Twin reference
  version is 3.1.1.  In the Twin code, changes needed for the dosemu
  interface have been marked with

           #ifdef DOSEMU

  Here is a summary of the changes I made in the Twin libraries:

           - I added vm86 mode, and related exception handling.
           - I made a first attempt to entry-point symmetry; the final goal is
             to have an 'invoke_code32' in interp_32_32.c, which can reach the
             16-bit code using 0x66,0x67 prefixes, the same way the 16-bit code
             is currently doing the other way. The variables 'code32' and 'data32'
             are used for prefix control.
           - some optimizations to memory access and multiplication code for
             little-endian machines and GNU compiler.
           - dosemu-style debug output; this is the biggest part of the patch
           - bugfixes. These are NOT marked with #ifdef DOSEMU!

  The second part of the cpuemu patch is the interface to dosemu, which
  is controlled by the X86_EMULATOR macro. This macro was probably part
  of a very old attempt to interface dosemu with Bochs, I deleted the
  old code and replaced it with the Twin interface.

  The X86_EMULATOR macro enables the compilation of the two files (cpu-
  emu.c and emu-utils.c) in the src/emu-i386/intp32 directory, which
  contain the vm86 emulator call (taken from the kernel sources) and
  some utility/debug functions. These files are kept separate from the
  Twin directory but need it to compile.

  For controlling the emulator behaviour, the file include/cpu-emu.h
  provides three macros:

      DONT_START_EMU: if undefined, the emulator starts immediately;
         otherwise, a call to int 0xe6 al=0x90 is required to switch from
         the standard vm86 to it. To switch in and out from the emulator,
         the small utilities 'ecpuon.com' and 'ecpuoff.com' are provided.
      TRACE_HIGH: controls the memory areas you want to include into the
         debug trace. The default value excludes the video BIOS and the HMA,
         but feel free to change it following your needs.
      VT_EMU_ONLY: if defined, use of the emulator forces VT console mode, by
         ignoring the 'console' and 'graphics' statements in the video
         config line.

  To enable the CPU emulator add

           cpuemu on

  to compiletime-settings, or pass

           --enable-cpuemu

  to configure.

  To use the emulator, put

           cpu emulated

  into /etc/dosemu.conf. Or start dosemu with -I 'cpu emulated'.

  The 'e' flag was added to the debug control string, it has currently a
  value range from 1 to 4 and controls the level of detail the emulator
  writes into the dosemu debug log. WARNING - logs greater than
  100Mbytes are the rule with cpu-emu!!!. As a safety measure, 'e' is
  not automatically added to the debug flags when you use 'a'; the 'e'
  parameter must be explicitly added. In addition, there is a new
  configuration parameter for /etc/dosemu.conf:

           logfilesize value

  This will limit the file size of the logfile. Once the limit is
  reached, it truncates the file to zero and continues writing to it.

