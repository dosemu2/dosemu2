  DOSEmu Technical Guide
  The DOSEmu team, Edited by Alistair MacDonald  <alis-
  tair@slitesys.demon.co.uk>
  For DOSEMU v0.97 pl3.0

  This document is the amalgamation of a series of technical README
  files which were created to deal with the lack of DOSEmu documenta-
  tion.
  ______________________________________________________________________

  Table of Contents:

  1.      Introduction

  2.      Runtime Configuration Options

  2.1.    Format of /etc/dosemu.users

  2.2.    Format of /var/lib/dosemu/global.conf ( (old) .dosrc, -I
  option)

  2.2.1.  Enviroment variables and configuration variables

  2.2.2.  Conditional statements

  2.2.3.  Include files

  2.2.4.  Macro substitution

  2.2.5.  Expressions

  2.2.6.  String expressions

  2.2.7.  `Dry' testing your configuration

  2.2.8.  Debug statement

  2.2.9.  Miscellaneous

  2.2.10. Keyboard settings

  2.2.11. Serial stuff

  2.2.12. Networking Support

  2.2.13. Terminals

  2.2.14. X Support settings

  2.2.15. Video settings ( console only )

  2.2.16. Memory settings

  2.2.17. IRQ passing

  2.2.18. Port Access

  2.2.19. Speaker

  2.2.20. Hard disks

  2.2.21. DOSEMU boot

  2.2.22. Floppy disks

  2.2.23. Printers

  2.2.24. Sound

  2.2.25. DEXE support

  3.      Accessing ports with dosemu

  3.1.    General

  3.2.    Port I/O access

  3.2.1.  System Integrity

  3.2.2.  System Security

  4.      The Virtual Flags

  5.      VM86PLUS, new kernel's vm86 for a full feature dosemu

  5.1.    Restrictions

  5.2.    Parts in the kernel that get changed for vm86plus

  5.2.1.  Changes to arch/i386/kernel/vm86.c

  5.2.1.1.        New vm86() syscall interface

  5.2.1.2.        Additional Data passed to vm86()

  5.2.1.3.        IRQ passing

  5.2.1.4.        Debugger support

  5.2.2.  Changes to arch/i386/kernel/ldt.c

  5.2.2.1.        New functioncode for `write' in modify_ldt syscall

  5.2.2.2.        `useable' bit in LDT descriptor

  5.2.2.3.        `present' bit in LDT selector

  5.2.3.  Changes to arch/i386/kernel/signal.c

  5.2.4.  Changes to arch/i386/kernel/traps.c

  5.3.    Abandoned `bells and whistles' from older emumodule

  5.3.1.  Kernel space LDT.

  5.3.2.  LDT Selectors accessing the `whole space'

  5.3.3.  Fast syscalls

  5.3.4.  Separate syscall interface (syscall manager)

  6.      Video Code

  6.1.    C files

  6.2.    Header files

  6.3.    Notes

  6.4.    Todo

  7.      The old way of generating a bootable DOSEmu

  7.1.    Boot ( 'traditional' method )

  7.1.1.  If you are

  7.1.2.  If you already have a HDIMAGE file

  7.1.3.  If you don't know how to copy files from/to the hdimage

  8.      New Keyboard Code

  8.1.    Whats New

  8.2.    Status

  8.3.    Keyboard server interface

  8.4.    Keyboard server structure

  8.4.1.  queue handling functions

  8.4.2.  The Front End

  8.4.2.1.        Functions in serv_xlat.c

  8.4.2.1.1.      putrawkey

  8.4.2.1.2.      putkey & others

  8.4.3.  The Back End

  8.4.3.1.        Queue Back End in keybint=on mode

  8.4.3.2.        Queue Back End in keybint=off mode

  8.4.3.3.        Functions in newkbd-server.c

  8.5.    Known bugs & incompatibilites

  8.6.    Changes from 0.61.10

  8.7.    TODO

  9.      IBM Character Set

  9.1.    What's new in configuration

  9.1.1.  IBM character set in an xterm

  9.1.2.  IBM character set at the console

  9.1.3.  IBM character set over a serial line into an IBM ANSI terminal

  9.2.    THE FUTURE by Mark Rejhon

  10.     Setting HogThreshold

  11.     Priveleges and Running as User

  11.1.   What we were suffering from

  11.2.   The new 'priv stuff'

  12.     Timing issues in dosemu

  12.1.   The 64-bit timers

  12.2.   DOS 'view of time' and time stretching

  12.3.   Non-periodic timer modes in PIT

  12.4.   Fast timing

  12.5.   PIC/PIT synchronization and interrupt delay

  12.6.   The RTC emulation

  12.7.   General warnings

  13.     Pentium-specific issues in dosemu

  13.1.   The pentium cycle counter

  13.2.   How to compile for pentium

  13.3.   Runtime calibration

  13.4.   Timer precision

  13.5.   Additional points

  14.     The DANG system

  14.1.   Description

  14.2.   Changes from last compiler release

  14.3.   Using DANG in your code

  14.4.   DANG Markers

  14.5.   DANG_BEGIN_MODULE / DANG_END_MODULE

  14.5.1. DANG_BEGIN_FUNCTION / DANG_END_FUNCTION

  14.5.2. DANG_BEGIN_REMARK / DANG_END_REMARK

  14.5.3. DANG_BEGIN_NEWIDEA / DANG_END_NEWIDEA

  14.5.4. DANG_FIXTHIS

  14.5.5. DANG_BEGIN_CHANGELOG / DANG_END_CHANGELOG

  14.6.   Usage

  14.7.   Future

  15.     mkfatimage -- Make a FAT hdimage pre-loaded with files

  16.     mkfatimage16 -- Make a large FAT hdimage pre-loaded with files

  17.     Documenting DOSEmu

  17.1.   Sections

  17.2.   Emphasising text

  17.3.   Lists

  17.4.   Quoting stuff

  17.5.   Special Characters

  17.6.   Cross-References & URLs

  17.6.1. Cross-References

  17.6.2. URLs

  17.7.   Gotchas

  18.     Sound Code

  18.1.   Current DOSEmu sound code

  18.2.   Original DOSEMU sound code

  19.     DMA Code

  19.1.   Current DOSEmu DMA code

  19.2.   Original DOSEMU DMA code

  19.2.1. Adding DMA devices to DOSEMU

  20.     DOSEmu Programmable Interrupt Controller

  20.1.   Other features

  20.2.   Caveats

  20.3.   Notes on theory of operation:

  20.3.1. Functions supported from DOSEmu side

  20.3.1.1.       Functions that Interface with DOS:

  20.3.2. Other Functions

  20.4.   A (very) little technical information for the curious

  21.     DOSEMU debugger v0.6

  21.1.   Introduction

  21.2.   Usage

  21.3.   Commands

  21.4.   Performance

  21.5.   Wish List

  21.6.   BUGS

  21.6.1. Known bugs

  22.     MARK REJHON'S 16550 UART EMULATOR

  22.1.   PROGRAMMING INFORMATION

  22.2.   DEBUGGING HELP

  22.3.   FOSSIL EMULATION

  22.4.   COPYRIGHTS

  23.     Recovering the console after a crash

  23.1.   The mail message

  24.     Net code

  25.     Software X386 emulation

  25.1.   The CPU emulator
  ______________________________________________________________________

  11..  IInnttrroodduuccttiioonn

  This documentation is derived from a number of smaller documents. This
  makes it easier for individuals to maintain the documentation relevant
  to their area of expertise. Previous attempts at documenting DOSEmu
  failed because the documentation on a large project like this quickly
  becomes too much for one person to handle.

  These are the technical READMEs. Many of these have traditionally been
  scattered around the source directories.

  22..  RRuunnttiimmee CCoonnffiigguurraattiioonn OOppttiioonnss

  This section of the document by Hans, <lermen@fgan.de>. Last updated
  on Mar 20, 1998.

  Most of DOSEMU configuration is done during runtime and per default it
  expects the system wide configuration file /etc/dosemu.conf optionally
  followed by the users  /.dosemurc and additional configurations
  statements on the commandline (-I option). The builtin configuration
  file of a DEXE file is passed using the -I technique, hence the rules
  of -I apply.

  In fact /etc/dosemu.conf and  /.dosemurc (which have identical syntax)
  are included by the systemwide configuration script
  /var/lib/dosemu/global.conf, but as a normal user you won't ever think
  on editing this, only dosemu.conf and your personal  /.dosemurc.

  In DOSEMU prior to 0.97.5 the private configuration file was called
   /.dosrc (not to be confused with the new  /.dosemurc). This will work
  as expected formerly, but is subject to be nolonger supported in the
  near future.  This (old)  /.dosrc is processed _a_f_t_e_r global.conf and
  follows (same as -I) the syntax of global.conf.

  The first file expected (and interpreted) before any other
  configuration (such as global.conf, dosemu.conf and  /.dosemurc) is
  /etc/dosemu.users.  Within /etc/dosemu.users the general permissions
  are set:

  +o  which users are allowed to use DOSEMU.

  +o  what kind of access class the user belongs to.

  +o  wether the user is allowed to define a private global.conf that
     replaces /var/lib/dosemu/global.conf (option -F).

  +o  what special configuration stuff the users needs

  This is done via setting configuration variables.

  After /etc/dosemu.users /etc/dosemu.conf (via global.conf) is
  interpreted, and only during global.conf parsing access to all
  configuration options is allowed. Your personal  /.dosemurc is
  included directly after dosemu.conf, but has less access rights (in
  fact the lowest level), all variables you define within  /.dosemurc
  transparently are prefixed with `dosemu_' such that the normal
  namespace cannot be polluted (and a hacker cannot overwrite security
  relevant enviroment variables). Within global.conf only those
   /.dosemurc created variables, that are needed are taken over and may
  overwrite those defined in /etc/dosemu.conf.

  The dosemu.conf (global.conf) may check for the configuration
  variables, that are set in /etc/dosemu.users and optionaly include
  further configuration files. But once /etc/dosemu.conf (global.conf)
  has finished interpretation, the access to secure relevant
  configurations is (class-wise) restricted while the following
  interpretation of (old) .dosrc and -I statements.

  For an example of a general configuration  look at ./etc/global.conf.
  The later behaves insecure, when /etc/dosemu.users is a copy of
  ./etc/dosemu.users.easy and behave 'secure', when /etc/dosemu.users is
  a copy of ./etc/dosemu.users.secure.

  22..11..  FFoorrmmaatt ooff //eettcc//ddoosseemmuu..uusseerrss

  Each line corresponds to exactly _one_ valid user count:

         loginname [ c_strict ] [ classes ...] [ c_dexeonly ] [ other ]

  where the elements are:

     llooggiinnnnaammee
        valid login name (root also is one) or 'all'. The later means
        any user not mentioned in previous lines.

     cc__ssttrriicctt
        Do not allow -F option (global.conf can't be replaced)

     cc__ddeexxeeoonnllyy
        Only allow execution of DEXE files, forbid any other use.

     ccllaasssseess
        One or more of the following:

        cc__aallll
           no restriction

        cc__nnoorrmmaall
           normal restrictions, all but the following classes: c_var,
           c_boot, c_vport, c_secure, c_irq, c_hardram.
        cc__vvaarr
           allow (un)setting of configuration- and environment variables

        cc__nniiccee
           allow `HogThreshold' setting

        cc__ffllooppppyy
           allow floppy access

        cc__bboooott
           allow definition of boot file/device

        cc__sseeccuurree
           allow setting of 'secure off'

        cc__vvppoorrtt
           allow setting of 'allowvideoportaccess'

        cc__ddppmmii
           allow DPMI setting

        cc__vviiddeeoo
           allow `video' setting

        cc__ppoorrtt
           allow `port' setting

        cc__ddiisskk
           allow `disk'  settings

        cc__xx
           allow X support settings

        cc__ssoouunndd
           allow sound settings

        cc__iirrqq
           allow `irqpassing' statement

        cc__ddeexxee
           allow `dexe' settings

        cc__pprriinntteerr
           allow printer settings

        cc__hhaarrddrraamm
           allow 'hardware_ram' settings

        cc__sshheellll
           allow the parser's `shell()' function

     ootthheerr
        Here you may define any configuration variable, that you want to
        test in global.conf (or (old) .dosrc, -I), see 'ifdef', 'ifndef'
        When this variable is intended to be unset in lower privilege
        configuration files (.dosrc, -I), then the variable name has to
        be prefixed with 'u_'.

  A line with '#' at colum 1 is treated as comment line. When only the
  login name is given (no further parameters, old format) the following
  setting is assumed:

    if 'root'  c_all
    else       c_normal

  Other than with previous DOSEMU versions, the /etc/dosemu.users now is
  mandatory. Also note, that you may restrict 'root' doing something
  silly ;-)

  22..22..  FFoorrmmaatt ooff //vvaarr//lliibb//ddoosseemmuu//gglloobbaall..ccoonnff (( ((oolldd)) ..ddoossrrcc,, --II ooppttiioonn))

  The configuration files are not line oriented, instead are consisting
  of `statements' (optionally separated by `;'), whitespaces are removed
  and all behind a '#' up to the end of the line is treated as comment.
  ( Note that older DOSEMUs also allowed `!' and `;' as comment
  character, but those are no longer supported ).

  22..22..11..  EEnnvviirroommeenntt vvaarriiaabblleess aanndd ccoonnffiigguurraattiioonn vvaarriiaabblleess

  They existed already in very early versions of DOSEMU (until now), but
  now evironment variables are much more useful in dosemu.conf /
  global.conf as before, because you now can set them, test them in the
  new 'if' statement and compute them in expressions.  The problem with
  the enviroment variables is, however, that the user may set and fake
  them _b_e_f_o_r_e calling DOSEMU, hence this is a security problem. To avoid
  this, we have the above mentioned _c_o_n_f_i_g_u_r_a_t_i_o_n _v_a_r_i_a_b_l_e_s, which are
  of complete different name space and are not visible outside of
  DOSEMU's configuration parser. On the other hand it may be useful to
  export settings from global.conf to the running DOS environment, which
  can be achieved by the 'unix.exe -e' DOS programm.

  To summarize:

     ccoonnffiigguurraattiioonn vvaarriiaabblleess
        have their own namespace only within the configuration parser.
        They usual are prefixed by _c__, _u__ and _h__ and cannot be made
        visible outside. They do not contain any value and are only
        tested for existence.

     eennvviirroonnmmeenntt vvaarriiaabblleess
        are inherited from the calling process, can be set within
        global.conf (dosemu.conf) and passed to DOSEMU running DOS-
        applications. Within *.conf they always are prefixed by '$'
        (Hence TERM becomes $TERM, even on the left side of an
        assigment). However, _s_e_t_t_i_n_g them is controled by the
        configuration variable 'c_var' (see above) and may be disallowed
        within the user supplied (old) .dosrc and alike.

  At startup DOSEMU generates the following environment variables, which
  may be used to let the configuration adapt better:

     KKEERRNNEELL__VVEERRSSIIOONN__CCOODDEE
        holds the numerical coded version of the running linux kernel
        (same format as within linux/version.h)

     DDOOSSEEMMUU__VVEERRSSIIOONN__CCOODDEE
        holds the numerical coded version of the running DOSEMU version
        (format: MSB ... LSB == major, minor, patchlevel, sublevel)

     DDOOSSEEMMUU__EEUUIIDD
        effective uid

     DDOOSSEEMMUU__UUIIDD
        uid. You may protect security relevant parts of the
        configuration such as:

             if ( ! $DOSEMU_EUID && ($DOSEMU_EUID != $DOSEMU_UID) )
               warn "running suid root"
             endif

     DDOOSSEEMMUU__HHOOSSTT
        Name of the host DOSEMU is running on. This is same as what
        follows the `h_' configuration variable, see below.  ( if not
        available, then DOSEMU_HOST contains `unknown' )

     DDOOSSEEMMUU__UUSSEERR
        The user name, that got matched in /etc/dosemu.users.  This
        needs not to be the _real_ user name, it may be `all' or
        `unknown'.

     DDOOSSEEMMUU__RREEAALL__UUSSEERR
        The user name out of getpwuid(uid).

     DDOOSSEEMMUU__SSHHEELLLL__RREETTUURRNN
        The exitcode (0-255) from the recently executed _s_h_e_l_l_(_) command.

     DDOOSSEEMMUU__OOPPTTIIOONNSS
        A string of all commandline options used (one character per
        option). You may remove a character from this string, such that
        the normal override of dosemu.conf settings will not take place
        for that option. However, parsing the command line options
        happens in two stages, one _before_ parsing dosemu.conf and one
        _after_. The options 'FfhIdLoO23456' have already gotten
        processed before dosemu.conf, so they can be disabled.

  22..22..22..  CCoonnddiittiioonnaall ssttaatteemmeennttss

  You may control execution of configuration statements via the
  following conditional statement:

         ifdef <configuration variable>

  or

    ifndef <configuration variable>
      ...
    else
      ...
    endif

  where _v_a_r_i_a_b_l_e is a _c_o_n_f_i_g_u_r_a_t_i_o_n _v_a_r_i_a_b_l_e (not an environment
  variable). Additionally there is a `normal' _i_f _s_t_a_t_e_m_e_n_t, a _w_h_i_l_e
  _s_t_a_t_e_m_e_n_t and a _f_o_r_e_a_c_h _s_t_a_t_e_m_e_n_t such as

         if ( expression )
         endif
         while ( expression )
         done
         foreach loop_variable (delim, list)
         done

  but these behaves a bit different and are described later.

  The 'else' clause may be ommitted and 'ifndef' is the opposite to
  'ifdef'.  The <variable> can't be tested for its contents, only if it
  is set or not.  Clauses also may contain further if*def..endif clause
  up to a depth of 15.  All stuff in /etc/dosemu.users behind the
  'loginname' in fact are _c_o_n_f_i_g_u_r_a_t_i_o_n _v_a_r_i_a_b_l_e_s that are set. Hence,
  what you set there, can be tested here in the config file. Further
  more you may set/unset  _c_o_n_f_i_g_u_r_a_t_i_o_n _v_a_r_i_a_b_l_e_s in the config files
  itself:

         define <configuration variable>
         undef  <configuration variable>

  However, use of define/undef is restricted to scope of global.conf, as
  long as you don't 'define c_var' _within_ global.conf.  If you are
  under scope of a 'user config file' (e.g. outside global.conf) you
  have to prefix the _c_o_n_f_i_g_u_r_a_t_i_o_n _v_a_r_i_a_b_l_e name with 'u_', else it will
  not be allowed to be set/unset (hence 'c_' type variables can't be
  unset out of scope of global.conf).

  There are some _c_o_n_f_i_g_u_r_a_t_i_o_n _v_a_r_i_a_b_l_e_s (besides the ones described
  above for dosemu.users) implicitely predefined by DOSEMU itself:

     cc__ssyysstteemm
        set while being in /var/lib/dosemu/global.conf

     cc__uusseerr
        set while parsing a user configuration file

     cc__ddoossrrcc
        set while parsing (old) .dosrc
     cc__ccoommlliinnee
        set while parsing -I option statements

     cc__ddeexxeerruunn
        set if a DEXE will be executed

     hh__<<oowwnnhhoosstt>>
        defined on startup as 'h_<host>.<domain>' of the host DOSEMU is
        running on. If <domain> can't be resolved, the pure hostname is
        taken. This makes sense only if a file system containing DOSEMU
        is mounted on diskless machines and you want restrict access.
        Note however, h_<ownhost> is set using
        gethostname/getdomainname. Hence, if the user on the diskless
        machine has root access, this is _not_ secure, because he could
        fake a valid address.

  Also, you may define any 'u_' type variable at start of DOSEMU via the
  new option -u such as

       # dos -u myspecialfun

  this will then define the config variable 'u_myspecialfun' _before_
  parsing any configuration file. You then may check this in your (old)
  ./dosrc or global.conf to do the needed special configuration.

  Now, what's this with the _i_f _s_t_a_t_e_m_e_n_t and _w_h_i_l_e _s_t_a_t_e_m_e_n_t?  All those
  conditionals via _i_f_d_e_f and _i_n_d_e_f are completly handled _b_e_f_o_r_e the
  remaining input is passed to the parser. Hence you even may use them
  _w_i_t_h_i_n a configuration statement such as

       terminal { charset
         ifdef u_likeibm
           ibm
         else
           latin
         endif
         updatefreq 4  color on }

  This is not the case with the (above mentioned) _i_f _s_t_a_t_e_m_e_n_t, this one
  is of course processed within the parser itself and can only take
  place within the proper syntax context such as

         if ( defined( u_likeibm ) )
           $mycharset = "ibm"
         else
           $mycharset = "latin"
         endif
         terminal { charset $mycharset updatefreq 4  color on }

  but it has the advantage, that you can put any (even complex)
  _e_x_p_r_e_s_s_i_o_n (see chapter `expressions') between the brackets.  If the
  expression's numerical value is 0, then false is assumed, else true.

  Same rules apply to the _w_h_i_l_e _s_t_a_t_e_m_e_n_t, the loop will be executed as
  long as `expression' is not 0. The loop end is given by the keyword
  _d_o_n_e such as in

         $counter = (3)
         while ($counter > 0)
           # what ever should loop
           $counter = ($counter -1)
         done

         # or some kind of list processing
         # ... but look below, `foreach' does it better
         $list = "aa bbb ccc"
         while (strlen($list))
           $item = strdel($list, strchr($list," "), 999)
           $list = strsplit($list, (strlen($item)+1),9999);
           warn "doing something with ", $item
         done

  The _f_o_r_e_a_c_h _s_t_a_t_e_m_e_n_t behaves a bit like the /bin/sh `for i in', but
  you can specify a list of delimiters.

         $list = "anton, berta; caesar dora"
         $delim = " ,;"
         foreach $xx ($delim, $list)
           warn "My name is ", $xx
         done

         $list = "a b c : 1 2 3"
         $delim = ":"
         foreach $xx ($delim, $list)
           if ($delim eq ":")
             $delim = " ";
           else
             warn "processing number ", $xx
           endif
         done

  The later example jumps to the colon (`:') in one step and after that
  process the numbers step by step.

  For all loops and `if' statement the allowed depth is 32 (totally).

  22..22..33..  IInncclluuddee ffiilleess

  If you for some reason want to bundle some major settings in a
  separate file you can include it via

    include "somefile"

  If 'somefile' doesn't have a leading '/', it is assumed to be relative
  to /etc.  Also includeing may be nested up to a max depth of 10 files.
  Note however, that the privilege is inherited from the main file from
  which is included, hence all what is included by
  /var/lib/dosemu/global.conf has its privilege.

  However, there are restrictions for `while' loops: You can't have
  _i_n_c_l_u_d_e _s_t_a_t_e_m_e_n_t_s within loops without beeing prepared for unexpected
  behave. In fact you may try, but due to the technique used, include
  files within loops are loaded completely _p_r_i_o_r loop execution.  Hence,
  if you do conditional including this won't work.

  A further restriction is, that the name of the include file must not
  be a variable. However, you can work around this with a macro (see
  next chapter) as shows the following example:

         $file = $HOME, "/.my_dosrc_include"
         shell("test -f ", $file)
         if ( ! $DOSEMU_SHELL_RETURN)
           # we can include
           $INC = ' include "', $file, '"';
           $$INC
         endif

  22..22..44..  MMaaccrroo ssuubbssttiittuuttiioonn

  There is a _v_e_r_y rudimentary macro substitution available, which does
  allow recursion but no arguments: Whenever you write

         $MYMACRO = "warn 'this is executed as macro'" ;
         $$MYMACRO

  it will expand to

         warn 'this is executed as macro'

  Note, that the `;' is required for parser to recognize the end of the
  statement and to set the variable _b_e_f_o_r_e it tries to read the next
  token (which will let the lexer process `$$MYMACRO'). Macro
  substitution is completely done on the input stream before the parser
  gets the data.

  For what is it worth then? Now, this enables you to insert text into
  the input stream, that otherwise would be expected to be constant. Or
  simple, it allows you to be lazy to write the same things more then
  once.

         $loop = '
           while ($xxx)
             warn "loop in macro ",$xxx
             $xxx = ($xxx -1)
           done
         ';
         $xxx = (2); $$loop; $xxx = (3); $$loop;

         $_X_keycode = (off)
         $_X_lin_filt = (on)
         ...
         if ($_X_keycode) $_X_keycode = "keycode" else $_X_keycode = "" endif
         if ($_X_lin_filt) $_X_lin_filt = "lin_filt" else $_X_lin_filt = "" endif
         X { icon_name "xdos" $$_X_keycode $$_X_lin_filt }

  You see, that in cases the variables are `false', the (parameterless)
  `keycode' and/or `lin_filt' keywords would not appear in the `X{}'
  statement.

  22..22..55..  EExxpprreessssiioonnss

  Expression within the configuration files follow the usual numerical
  rules and may be as complex as you wish. At some places, the parser
  only can `understand' expressions, when you enclose them in brackets,
  but mostly you just can type

         123 + 456 + 2 * 1.2

  Though, if you want be sure, you better type them as

         ( 123 + 456 + 2 * 1.2 )

  You may place expressions whenever a numerical value is expected and
  there is no ambiguity in the syntax. Such an ambiguity is given, when
  a statement needs more then one successive number such as

         ... winsize x y ...
         ... vesamode width heigh ...
         ... range from to ...

  If you want to place expression herein, you need the new syntax for
  those statements / terms which have a coma (instead of a blank) as
  delimiter:

         ... winsize x , y ...
         ... vesamode width , heigh ...
         ... range from , to ...

  The old syntax is left for compatibility and is only parsed correcty,
  if pure numbers (integers) are used.

  Valid constant numbers (not only in expressions) are

         123     decimal, integer
         0x1a    hexadecimal, integer
         0b10101 bitstream, integer
         1.2     float number, real
         0.5e3   exponential form, real
         off     boolean false, integer
         on      boolean true, integer
         no      boolean false, integer
         yes     boolean true, integer

  The following operator are recognized:

         + - *   as usual
         / div   division, the '/' _must_ be surrounded by whitespaces, else
                 it conflicts with pathnames on quoteless strings
         |       bitwise or
         ^       bitwise exclusive or
         ~       bitwise not
         &       bitwise and
         >>      shift right
         <<      shift left
         <       less then
         <=      less or equal
         >       greater then
         >=      greater or equal
         ||      boolean or
         &&      boolean and
         !       boolean not
         ==      numerical equivalence
         eq      string equivalence
         !=      numerical, not equal
         ne      string not equal

  The type of the expression result may be real (float) or integer,
  depending on which type is on the `left side'. Conversion is mostly
  done automaticaly, but you may force it using the (below mentioned)
  int() and real() functions such as:

     $is_real =    ( 3.1415 * 100 )
     $is_integer = ( int( 3.1415 * 100) )
     $is_integer = ( 100 * 3.1415 )
     $is_real =  ( real($is_integer) )

  The above also shows, how environment variables can be set: if you
  want to place `expressions' (which are always numbers) onto a
  variable, you have to surround them with brackets, else the parser
  won't be able to detect them. In principal, all $xxx settings are
  string-settings and numbers will be converted correctly before. In
  fact the `$xxx =' statement accepts a complete coma separated list,
  which it will concatenate, examples:

         $termnum = (1)
         $MYTERM = "/dev/ttyp", $termnum       # results in "/dev/ttyp1"
         $VER = (($DOSEMU_VERSION_CODE >> 24) & 255)
         $MINOR = (($DOSEMU_VERSION_CODE >> 16) & 255)
         $running_dosemu = "dosemu-", $VER, ".", $MINOR

  Several builtin functions, which can be used in expressions, are
  available:

     iinntt((rreeaall))
        converts a float expression to integer

     rreeaall((iinntteeggeerr))
        converts a integer expression to float

     ssttrrlleenn((ssttrriinngg))
        returns the length of `string'

     ssttrrttooll((ssttrriinngg))
        returns the integer value of `string'

     ssttrrnnccmmpp((ssttrr11,,ssttrr22,,eexxpprreessssiioonn))
        compares strings, see `man strncmp'

     ssttrrppbbrrkk((ssttrr11,,ssttrr22))
        like `man strpbrk', but returns an index or -1 instead of a char
        pointer.

     ssttrrcchhrr((ssttrr11,,ssttrr22))
        like `man strchr', but returns an index or -1 instead of a char
        pointer.

     ssttrrrrcchhrr((ssttrr11,,ssttrr22))
        like `man strrchr', but returns an index or -1 instead of a char
        pointer.

     ssttrrssttrr((ssttrr11,,ssttrr22))
        like `man strstr', but returns an index or -1 instead of a char
        pointer.

     ssttrrssppnn((ssttrr11,,ssttrr22))
        as `man strspn'

     ssttrrccssppnn((ssttrr11,,ssttrr22))
        as `man strcspn'

     ddeeffiinneedd((vvaarrnnaammee))
        returns true, if the configuration variable `varname' exists.

  22..22..66..  SSttrriinngg eexxpprreessssiioonnss

  For manipulation of strings there are the following builtin functions,
  which all return a new string. These very well may be placed as
  argument to a numerical function's string argument, but within an
  `expression' they may be only used together with the `eq' or `ne'
  operator.

     ssttrrccaatt((ssttrr11,, ,,ssttrrnn))
        concatenates any number of strings, the result is a string
        again.

     ssttrrsspplliitt((ssttrriinngg,, eexxpprr11,, eexxpprr22))
        cuts off parts of `string', starting at index `expr1' with
        length of `expr2'.  If either `expr1' is < 0 or `expr2' is < 1,
        an empty string is returned.

     ssttrrddeell((ssttrriinngg,, eexxpprr11,, eexxpprr22))
        deletes parts of `string', starting at index `expr1' with length
        of `expr2'.  If either `expr1' or `expr2' is < 0, nothing is
        deleted (the original strings is returned).

     sshheellll((ssttrriinngg))
        executes the command in `string' and returns its stdout result
        in a string. The exit code of executed command is put into
        $DOSEMU_SHELL_RETURN (value between 0 and 255). You may also
        call shell() as a standalone statement (hence discarding the
        returned string), if you only need $DOSEMU_SHELL_RETURN (or not
        even that).  However, to avoid security implications all
        privilegdes are dropped and executions is under control of
        _c___s_h_e_l_l configuration variable. The default is, that it can only
        be executed from within global.conf.

  With these tools you should be able to make your global.conf adapt to
  any special case, such as different terminal types, different hdimages
  for different users and/or different host, adapt to different (future)
  dosemu and/or kernel versions. Here some small examples:

    $whoami = shell("who am i")
    if ( strchr($whoami, "(" ) < 0 )
      # beeing on console
    else
      if (strstr($whoami, "(:") < 0)
        # beeing remote logged in
      endif
      if ($TERM eq "xterm")
        # beeing on xterm
      else
        if (strstr("linux console", $TERM) < 0)
          # remote side must be some type of real terminal
        else
          # remote side is a Linux console
        endif
      endif
    endif

    if ($DISPLAY ne "")
      # we can rely on reaching an Xserver
      if (strsplit($DISPLAY, 0, 1) ne ":")
        # the X server is remote
      endif
    endif

    if ($DOSEMU_REAL_USER eq "alistair")
      # ha, this one is allowed to do odds sound tricks :-)
    endif

    if (strsplit($DOSEMU_HOST, 0, strchr($DOSEMU_HOST,"."))
                                                     eq $DOSEMU_HOST)
      # we have no domain suffix for this host, should be a local one
    endif

    # disable setting graphics mode per commandline option -g
    $DOSEMU_OPTIONS = strdel($DOSEMU_OPTIONS, strchr($DOSEMU_OPTIONS,"g"),1);

  22..22..77..  ``DDrryy'' tteessttiinngg yyoouurr ccoonnffiigguurraattiioonn

  It may be usefull to verify, that your *.conf does what you want
  before starting a real running DOSEMU. For this purpose there is a new
  commandline option (-h), which just runs the parser, print some useful
  output, dumps the main configuration structure and then exits.  The
  option has an argument (0,1,2), which sets the amount of parser debug
  output: 0 = no parser debug, 1 = print loop debugs, 2 = same as 1 plus
  if_else_endif-stack debug. This feature can be used such as

         $ dos -h0 -O 2>&1 | less

  The output of `-h2' looks like

    PUSH 1->2 1 >foreach__yy__<
    PUSH 2->3 1 >if<
    POP 2<-3 0 >endif< -1
    POP 1<-2 1 >done< -1
    PUSH 1->2 1 >foreach__yy__<
    PUSH 2->3 1 >if<
    POP 2<-3 1 >endif< -1
    POP 1<-2 1 >done< -1
        |  | |         +-------`cached' read status (0 = not cached)
        |  | +-----------------`if' or `loop' test result (0 = false = skipped)
        |  +-------------------inner level
        +----------------------outer level (1 = no depth)

  There are also some configuration statements, which aren't of any use
  except for help debugging your *.conf such as

         exprtest ($DOSEMU_VERSION_CODE)   # will just print the result
         warn "content of DOSEMU_VERSION_CODE: ", $DOSEMU_VERSION_CODE

  22..22..88..  DDeebbuugg ssttaatteemmeenntt

  This section is of interest mainly to programmers.  This is useful if
  you are having problems with DOSEMU and you want to enclose debug info
  when you make bug reports to a member of the DOSEMU development team.
  Simply set desired flags to "on" or "off", then redirect stderr of
  DOSEMU to a file using "dos -o debug" to record the debug information
  if desired.  Skip this section if you're only starting to set up.

         debug { config  off   disk    off     warning off     hardware off
               port    off     read    off     general off     IPC      off
               video   off     write   off     xms     off     ems      off
               serial  off     keyb    off     dpmi    off
               printer off     mouse   off     sound   off
         }

  Alternatively you may use the same format as -D commandline option
  (but without the -D in front), look at the dos.1 man page.

         debug "+a-v"     # all messages but video
         debug "+4r"      # default + maximum of PIC debug

  or simply (to turn off all debugging)

    debug { off }

  22..22..99..  MMiisscceellllaanneeoouuss

  The HogThreshold value determines how nice Dosemu will be about giving
  other Linux processes a chance to run.  Setting the HogThreshold value
  to approximately half of you BogoMips value will slightly degrade
  Dosemu performance, but significantly increase overall system idle
  time.  A zero value runs Dosemu at full tilt.

         HogThreshold 0

  Want startup DOSEMU banner messages?  Of course :-)

         dosbanner on

  Timint is necessary for many programs to work.

         timint on

  For "mathco", set this to "on" to enable the coprocessor during
  DOSEMU.  This really only has an effect on kernels prior to 1.0.3.

         mathco on

  For "cpu", set this to the CPU you want recognized during DOSEMU.

         cpu 80386

  If you have DOSEMU configured to use the 386-emulator, you can enable
  the emulator via

         cpu emulated

  You may ask why we need to emulate a 386 on an 386 ;-) Well, for
  normal purpose its not needed (and will be slower anyway), but the
  emulator offers a better way to debug DOS applications, especially
  DPMI code.  Also, we hope that some day we may be able to run DOSEMU
  on other machines than 386. At the time of writing this, the cpu
  emulators is very alpha and you should not use it except you are
  willing to help us. So please, don't even try to report 'bugs' except
  you have a patch for the bug too.

  If you have a pentium, DOSEMU can make use of the pentium cycle
  counter to do better timing. DOSEMU detects the pentium and will use
  the RDTSC instruction for get time per default. To disable this
  feature use

         rdtsc off

  Also, to use the pentium cycle counter correctly DOSEMU needs to know
  the CPU-clock which your chip is running. This per default is
  calibrated automatically, however, that may be not exact enough. In
  this case you have to set it yourself via

         cpuspeed 180

  or, for values like 166.6666 you may give two numbers such as

         cpuspeed 500 3

  which will be calculated as 'cpuspeed=500/3'

  If you have a PCI board you may allow DOSEMU to access the PCI
  configuration space by defining the below

         pci on | off

  PCI is assumed to be present on CPUs better then a pentium, otherwise
  the default is 'pci off'

  For "bootA"/"bootC", set this to the bootup drive you want to use.  It
  is strongly recommended you start with "bootA" to get DOSEMU going,
  and during configuration of DOSEMU to recognize hard disks.

    bootA

  During compile there will be a symbol map generated, this usually then
  is ./bin/dosemu.map. You may wnt to save it to an other places and let
  'dosdebug' know where to find it:

         dosemumap /var/lib/dosemu/dosemu.map

  Normally all debug logging is done _imediately_ (unbuffered). However,
  when dumping big amounts of logdata, the dynamic behave of DOSEMU may
  change, hence hiding the real problem (or causing a new one) Using the
  below switches buffering on and sets the buffer size.

         logbufsize 0x20000

  In addition, you may want to limit the file size of the log file,
  especially when you expect huge amounts of data (such as with -D+e).
  This can be done via

         logfilesize 0x200000

  which (in this case) will limit the size to 2Mbytes. The default
  setting is a file size of 10Mbytes.

  When you want to abort DOSEMU from within a configuration file
  (because you detected something weird) then do

         abort

  or

         abort "message text"

  When you want just to warn the user use the following (the message
  will get printed to the log file via the debug-flag '+c')

    warn "message text"

  When you want to use the CDrom driver and the Linux device is other
  then /dev/cdrom you may define

         cdrom /dev/xxx

  22..22..1100..  KKeeyybbooaarrdd sseettttiinnggss

  For defining the keyboard layouts you are using there is the
  "keyboard" statement such as

         keyboard {  layout us  .... }

       or

         keyboard {  layout us {alt 66=230} ... }

  The later modifies the US keyboard layout such that it will allow you
  to type a character 230 (micro) with right ALT-M.

  The same can be done via a "keytable" statement such as

         keytable keyb-user {alt 66=230}

  The "keyb-user" is preset with an US layout and is intended to be used
  as "buffer" for user defined keyboard tables.

  The format of the "{}" modification list (either in the "keyboard" or
  the "keytable" statement) is as follows:

         [submap] key_number = value [,value [,...]]

  Given we call the above  a "definition" then it may be repeated (blank
  separated) such as

         definition [definition [...]]

  "key_number" is the physical number, that the keybord send to the
  machine when you hit the key, its the index into the keytable. "value"
  may be any integer between 0...255, a string, or one of the following
  keywords for "dead keys" (NOTE: deadkeys have a value below 32, so be
  carefull).

         dgrave        (dead grave)
         dacute        (dead acute)
         dcircum       (dead circumflex)
         dtilde        (dead tilde)
         dbreve        (dead breve)
         daboved       (dead above dot)
         ddiares       (dead diaresis)
         dabover       (dead above ring)
         ddacute       (dead double acute)
         dcedilla      (dead cedilla)

  After a "key_number =" there may be any number of comma separated
  values, which will go into the table starting with "key_number", hence
  all below examples are equivalent

         { 2="1" 3="2" }
         { 2="1","2" }
         { 2="12" }
         { 2=0x31,50 }

  "submap" tells in about something about the shift state for which the
  definition is to use or wether we mean the numpad:

         shift  16="Q"      (means key 16 + SHIFT pressed)
         alt    16="@"      (means key 16 + right ALT pressed (so called AltGr))
         numpad 12=","      (means numpad index 12)

  You may even replace the whole table with a comma/blank separated list
  of values. In order to make it easy for you, you may use dosemu to
  create such a list. The following statement will dump all current key
  tables out into a file "kbdtables", which is directly suitable for
  inclusion into global.conf (hence it follows the syntax):

         keytable dump "kbdtables"

  However, don't put this not into your global.conf, because dosemu will
  exit after generating the tablefile. Instead use the -I commandline
  option such as

         $ dos -I 'keytable dump "kbdtables"'

  After installation of dosemu ("make install") you'll find the current
  dosemu keytables in /var/lib/dosemu/keymap (and in ./etc/keymap, where
  they are copied from). These tables are generated via "keytable dump"
  and split into discrete files, such that you may modify them to fit
  your needs.  You may load them such as

         $ dos -I 'include "keymap/de-latin1"'

  (when an include file is starting with "keymap/" it is taken out of
  /var/lib/dosemu). When there was a keytable previously defined (e.g.
  in global.conf), they new one will be take anyway and a warning will
  be printed on stderr.

  Anyway, you are not forced to modify or load a keytable, and with the
  "layout" keyword from the "keyboard" statement, you can specify your
  country's keyboard layout.  The following builtin layouts are
  implemented:

           finnish           us           dvorak       sf
           finnish-latin1    uk           sg           sf-latin1
           de                dk           sg-latin1    es
           de-latin1         dk-latin1    fr           es-latin1
           be                keyb-no      fr-latin1    po
           it                no-latin1    sw
           hu                hu-cwi       hu-latin2    keyb-user

  The keyb-user is selected by default if the "layout" keyword is
  omitted, and this in fact is identical to us-layout, as long as it got
  not overloaded by the "keytable" statement (see above).

  The keyword "keybint" allows more accurate of keyboard interrupts, It
  is a bit unstable, but makes keyboard work better when set to "on".

  The keyword "rawkeyboard" allows for accurate keyboard emulation for
  DOS programs, and is only activated when DOSEMU starts up at the
  console.  It only becomes a problem when DOSEMU prematurely exits with
  a "Segmentation Fault" fatal error, because the keyboard would have
  not been reset properly.  In that case, you would have to run kbd_mode
  -a remotely, or use the RESET button.  In reality, this should never
  happen.  But if it does, please do report to the dosemu development
  team, of the problem and detailed circumstances, we're trying our
  best!  If you don't need near complete keyboard emulation (needed by
  major software package), set it to "off"

  recommended:

    keyboard {  layout us  keybint on  rawkeyboard off  }

  or

         keyboard {  layout de-latin1  keybint on  rawkeyboard on  }

  If you want DOSEMU feed with keystrokes, that are typed in
  automagically, then you may define them such as

         keystroke "cd c:\\mysource\r"

  You may have any number of 'keystroke' statements, they all will be
  concatenated.

  This feature however doesn't make much sense _here_ in the
  configuration file, instead together with the commandine option -I you
  can start dosemu and execute any arbitrary dos command such as

         # dos -D-a -I 'keystroke "c:\rcd \\windows\rwinemu\r"'

  For more details please look at ./doc/README.batch

  Ah, but _one_ sensible useage _here_ is to 'pre-strike' that damned F8
  that is needed for DOS-7.0, when you don't want to edit the msdos.sys:

        keystroke "\F8;"

  22..22..1111..  SSeerriiaall ssttuuffff

  You can specify up to 4 simultaneous serial ports here.  If more than
  one ports have the same IRQ, only one of those ports can be used at
  the same time.  Also, you can specify the com port, base address, irq,
  and device path!  The defaults are:

  +o  COM1 default is base 0x03F8, irq 4, and device /dev/cua0

  +o  COM2 default is base 0x02F8, irq 3, and device /dev/cua1

  +o  COM3 default is base 0x03E8, irq 4, and device /dev/cua2

  +o  COM4 default is base 0x02E8, irq 3, and device /dev/cua3

  If the "com" keyword is omitted, the next unused COM port is assigned.
  Also, remember, these are only how you want the ports to be emulated
  in DOSEMU.  That means what is COM3 on IRQ 5 in real DOS, can become
  COM1 on IRQ 4 in DOSEMU!

  _N_O_T_E_: You must have /usr/spool/uucp for LCK-file generation !  You may
  change this path and the lockfile name via the below 'ttylocks'
  statement.

  Also, as an example of defaults, these two lines are functionally
  equal:

         serial { com 1  mouse }
         serial { com 1  mouse  base 0x03F8  irq 4  device /dev/cua0 }

  If you want to use a serial mouse with DOSEMU, the "mouse" keyword
  should be specified in only one of the serial lines.  (For PS/2 mice,
  it is not necessary, and device path is in mouse line instead)

  Use/modify any of the following if you want to support a modem: (or
  any other serial device.)

         serial { com 1  device /dev/modem }
         serial { com 2  device /dev/modem }
         serial { com 3  device /dev/modem }
         serial { com 4  device /dev/modem }
         serial { com 3  base 0x03E8  irq 5  device /dev/cua2 }

  If you are going to load a msdos mouse driver for mouse support
  use/modify one of the following

         serial { mouse  com 1  device /dev/mouse }
         serial { mouse  com 2  device /dev/mouse }

  What type is your mouse?  Use one of the following.  Use the
  'internaldriver' option to try Dosemu internaldriver.  Use the
  'emulate3buttons' for 3button mice.

    mouse { microsoft }
    mouse { logitech }
    mouse { mmseries }
    mouse { mouseman }
    mouse { hitachi }
    mouse { mousesystems }
    mouse { busmouse }
    mouse { ps2  device /dev/mouse internaldriver emulate3buttons }
    mouse { mousesystems device /dev/mouse internaldriver cleardtr }

  For tty locking capabilities:

  The serial lines are locked by dosemu via usual lock file technique,
  which is compatible with most other unix apps (such as mgetty, dip,
  e.t.c). However, you carefully need to check _where_ those other apps
  expect the lock files. The most common used (old) place is
  /usr/spool/uucp, but newer distributions following the FSSTND will
  have it in /var/lock. The dosemu default one is /usr/spool/uucp.  The
  below defines /var/lock

         ttylocks { directory /var/lock }

  _N_o_t_e_: you are responsible for ensuring that the directory exists !  If
  you want to define the lock prefix stub also, use this one

         ttylocks { directory /var/lock namestub LCK.. }

  If the lockfile should contain the PID in binary form (instead of
  ASCII}, you may use the following

         ttylocks { directory /var/lock namestub LCK.. binary }

  22..22..1122..  NNeettwwoorrkkiinngg SSuuppppoorrtt

  Turn the following option 'on' if you require IPX/SPX emulation.
  Therefore, there is no need to load IPX.COM within the DOS session.
  The following option does not emulate LSL.COM, IPXODI.COM, etc.  _N_O_T_E_:
  _Y_O_U _M_U_S_T _H_A_V_E _I_P_X _P_R_O_T_O_C_O_L _E_N_A_B_L_E_D _I_N _K_E_R_N_E_L _!_!

         ipxsupport off

  Enable Novell 8137->raw 802.3 translation hack in new packet driver.

         pktdriver novell_hack

  22..22..1133..  TTeerrmmiinnaallss

  This section applies whenever you run DOSEMU remotely or in an xterm.
  Color terminal support is now built into DOSEMU.  Skip this section
  for now to use terminal defaults, until you get DOSEMU to work.

  There are a number of keywords for the terminal { } configuration
  line.

     cchhaarrsseett
        Select the character set to use with DOSEMU. One of ``latin''
        (default) or ``ibm''.

     ccoolloorr
        Enable or disable color terminal support. One of ``on''
        (default) or ``off''.

     uuppddaatteeffrreeqq
        A number indicating the frequency of terminal updates of the
        screen.  The smaller the number, the more frequent.  A value of
        20 gives a frequency of about one per second, which is very
        slow.  However, more CPU time is given to DOS applications when
        updates are less frequent.  A value of 4 (default) is
        recommended in most cases, but if you have a fast system or
        link, you can decrease this to 0.

     eesscccchhaarr
        A number (ascii code below 32) that specifies the control
        character used as a prefix character for sending alt, shift,
        ctrl, and function keycodes.  The default value is 30 which is
        Ctrl-^. So, for example,

            F1 is 'Ctrl-^1', Alt-F7 is 'Ctrl-^s Ctrl-^7'.
            For online help, press 'Ctrl-^h' or 'Ctrl-^?'.

  Use the following to enable the IBM character set.

         terminal { charset ibm  color on }

  Use this for color xterms or rxvt's with no IBM font, with only 8
  colors.

    terminal { charset latin  color on }

  Use this for color xterms or rxvt's with IBM font, with only 8 colors.

         terminal { charset ibm  color on }

  More detailed line for user configuration:

         terminal { charset latin  updatefreq 4  color on }

  22..22..1144..  XX SSuuppppoorrtt sseettttiinnggss

  If DOSEMU is running in its own X-window (not xterm), you may need to
  tailor it to your needs. Valid keywords for the X { } config line:

     uuppddaatteeffrreeqq
        A number indicating the frequency of X updates of the screen.
        The smaller the number, the more frequent.  A value of 20 gives
        a frequency of about one per second, which is very slow.
        However, more CPU time is given to DOS applications when updates
        are less frequent.  The default is 8.

     ddiissppllaayy
        The X server to use. If this is not specified, dosemu will use
        the DISPLAY environment variable. (This is the normal case) The
        default is ":0".

     ttiittllee
        What you want dosemu to display in the title bar of its window.
        The default is "dosemu".

     iiccoonn__nnaammee
        Used when the dosemu window is iconified. The default is
        "dosemu".

     kkeeyyccooddee
        Used to give Xdos access to keycode part of XFree86. The default
        is off.

        _N_O_T_E_:

     +o  You should _not_ use this when using X remotely (the remote site
        may have other raw keyboard settings).

     +o  _I_f you use "keycode", you also _m_u_s_t define an appropriate
        keyboard layout (see above).

     +o  If you do _n_o_t use "keycode" then under X a neutral keyboard
        layout is forced ( keyboard {layout us} ) regardless of what you
        have set above.

        Anyway, a cleaner way than using "keycode" is to let the X-
        server fiddle with keyboard translation and customize it via
        .xmodmaps.

     bblliinnkkrraattee
        A number which sets the blink rate for the cursor. The default
        is 8.

     ffoonntt
        Used to pick a font other than vga (default). Must be
        monospaced.

     mmiittsshhmm
        Use shared memory extensions. The default is ``on''.

     sshhaarreeccmmaapp
        Used to share the colormap with other applications in graphics
        mode.  If not set, a private colormap is used. The default is
        off.

     ffiixxeedd__aassppeecctt
        Set fixed aspect for resize the graphics window. One of ``on''
        (default) or ``off''.

     aassppeecctt__4433
        Always use an aspect ratio of 4:3 for graphics. The default is
        ``floating''.

     lliinn__ffiilltt
        Use linear filtering for >15 bpp interpolation. The default is
        off.

     bbiilliinn__ffiilltt
        Use bi-linear filtering for >15 bpp interpolation. The default
        is off.

     mmooddee1133ffaacctt
        A number which sets the initial size factor for video mode 0x13
        (320x200).  The default is 2.

     wwiinnssiizzee
        A pair of numbers which set the initial width and height of the
        window to a fixed value. The default is to float.

     ggaammmmaa
        Set value for gamma correction, a value of 100 means gamma 1.0.
        The default is 100.

     vvggaaeemmuu__mmeemmssiizzee
        Set the size (in Kbytes) of the frame buffer for emulated vga
        under X. The default is 1024.

     llffbb
        Enable or disable the linear frame buffer in VESA modes. The
        default is ``on''.

     ppmm__iinntteerrffaaccee
        Enable or disable the protected mode interface for VESA modes.
        The default is ``on''.

     mmggrraabb__kkeeyy
        String, name of KeySym name to activate mouse grab. Default is
        `empty' (no mouse grab). A possible value could be "Home".

     vveessaammooddee
        Define a VESA mode. Two variants are supported: vesamode width
        height and vesamode width height color_bits. The first adds the
        specified resolution in all supported color depths (currently 8,
        15, 16, 24 and 32 bit).

  Recommended X statement:

         X { updatefreq 8 title "DOS in a BOX" icon_name "xdos" }

  22..22..1155..  VViiddeeoo sseettttiinnggss (( ccoonnssoollee oonnllyy ))

  _!_!_W_A_R_N_I_N_G_!_!_: _A _L_O_T _O_F _T_H_I_S _V_I_D_E_O _C_O_D_E _I_S _A_L_P_H_A_!  _I_F _Y_O_U _E_N_A_B_L_E
  _G_R_A_P_H_I_C_S _O_N _A_N _I_N_C_O_M_P_A_T_I_B_L_E _A_D_A_P_T_O_R_, _Y_O_U _C_O_U_L_D _G_E_T _A _B_L_A_N_K _S_C_R_E_E_N _O_R
  _M_E_S_S_Y _S_C_R_E_E_N _E_V_E_N _A_F_T_E_R _E_X_I_T_I_N_G _D_O_S_E_M_U_.  _J_U_S_T _R_E_B_O_O_T _(_B_L_I_N_D_L_Y_) _A_N_D
  _T_H_E_N _M_O_D_I_F_Y _C_O_N_F_I_G_.

  Start with only text video using the following line, to get started.
  then when DOSEMU is running, you can set up a better video
  configuration.

         video { vga }                    Use this line, if you are using VGA
         video { cga  console }           Use this line, if you are using CGA
         video { ega  console }           Use this line, if you are using EGA
         video { mda  console }           Use this line, if you are using MDA

  Notes for Graphics:

  +o  If your VGA-Bios resides at E000-EFFF, turn off video BIOS shadow
     for this address range and add the statement vbios_seg 0xe000 to
     the correct vios-statement, see the example below

  +o  If your VBios size is only 32K you set it with  vbios_size 0x8000,
     you then gain some space for UMB or hardware ram locations.
  +o  Set "allowvideoportaccess on" earlier in this configuration file if
     DOSEMU won't boot properly, such as hanging with a blank screen,
     beeping, leaving Linux video in a bad state, or the video card
     bootup message seems to stick.

  +o  Video BIOS shadowing (in your CMOS setup) at C000-CFFF must be
     disabled.

     _C_A_U_T_I_O_N_: TURN OFF VIDEO BIOS SHADOWING BEFORE ENABLING GRAPHICS!
     This is not always necessary, but a word to the wise shall be
     sufficient.

  +o  If you have a dual-monitor configuration (e.g. MDA as second
     display), you then may run CAD programs on 2 displays or let play
     your debugger on the MDA while debugging a graphics program on the
     VGA (e.g TD -do ).  You also may switch to the MDA display by using
     the DOS command mode mono (mode co80 returns to your normal
     display).  This feature can be enabled by the switch "dualmon" like
     this:

             video { vga  console  graphics dualmon }

  and can be used on a xterm and the console, but of course not, if you
  have the MDA as your primary display.  You also must set USE_DUALMON 1
  in include/video.h.  _N_O_T_E_: Make sure no more then one process is using
  this feature !  ( you will get funny garbage on your MDA display. )
  Also, you must NOT have the dualmon-patches for kernel applied ( hav-
  ing the MDA as Linux console )

  +o  If you want to run dosemu in situations when human doesn't sit at
     console (for instance to run it by cron) and want console option be
     enabled you should use option forcevtswitch.

                  { vga console forcevtswitch }

  Without the option dosemu waits for becoming virtual terminal on which
  dosemu is run active (i.e. user must press Alt-F?).  With this option
  dosemu perform the switch itself.  Be careful with this option because
  with it user sat at console may face with unexpected switch.

  It may be necessary to set this to "on" if DOSEMU can't boot up
  properly on your system when it's set "off" and when graphics are
  enabled.  _N_o_t_e_: May interfere with serial ports when using certain
  video boards.

         allowvideoportaccess on

  Any 100% compatible standard VGA card _M_A_Y work with this:

         video { vga  console  graphics }

  If your VGA-BIOS is at segment E000, this may work for you:

         video { vga  console  graphics  vbios_seg 0xe000 }

  Trident SVGA with 1 megabyte on board

         video { vga  console  graphics  chipset trident  memsize 1024 }

  Diamond SVGA (not S3 chip)

         video { vga  console  graphics  chipset diamond }

  Cirrus SVGA

         video { vga  console  graphics  chipset cirrus }

  ET4000 SVGA card with 1 megabyte on board:

         video { vga  console  graphics  chipset et4000  memsize 1024 }

  or

         video { vga  console  graphics  chipset et4000  memsize 1024 vbios_size 0x8000 }

  S3-based SVGA video card with 1 megabyte on board:

         video { vga  console  graphics  chipset s3  memsize 1024 }

  Avance Logic (ALI) 230x SVGA

    video { vga  console  graphics  chipset avance }

  For ATI graphic mode

         ports { 0x1ce 0x1cf 0x238 0x23b 0x23c 0x23f 0x9ae8 0x9ae9 0x9aee 0x9aef }

  Matrox millenium:

         video { vga  console  graphics  chipset matrox }

  VGA-cards with a WD(Paradise) chip:

         video { vga  console  graphics  chipset wdvga }

  22..22..1166..  MMeemmoorryy sseettttiinnggss

  These are memory parameters, stated in number of kilobytes.  If you
  get lots of disk swapping while DOSEMU runs, you should reduce these
  values.

  umb_max is a new parameter which tells DOSEMU to be more aggressive
  about finding upper memory blocks.  The default is 'off'.

  To be more aggressive about finding XMS UMB blocks use this:

         umb_max on

  To be more secure use 'secure on'. If "on", then it disables DPMI
  access to dosemu code and also disables execution of dosemu supplied
  'system' commands, which may execute arbitrary Linux-commands
  otherwise.  The background is, that DPMI clients are allowed to create
  selectors that span the whole user space, hence may hack into the
  dosemu code, and (when dosemu runs root or is suid root) can be a
  security hole.  "secure on" closes this hole, though this would very
  likely also disable some dos4gw games :(.  Therfore _N_O_T_E_: You may not
  be able to run some DPMI programs, hence, before reporting such a
  program as 'not running', first try to set 'secure off'.

         secure on                # "on" or "off"

  The below enables/disables DPMI and sets the size of DPMI memory.

         dpmi 4086                # DPMI size in K, or "off"

  XMS is enabled by the following statement

         xms 1024                 # XMS size in K,  or "off"

  For ems, you now can set the frame to any 16K between 0xc800..0xe000

         ems 1024                 # EMS size in K,  or "off"

  or

         ems { ems_size 1024 ems_frame 0xe000 }

  or

         ems { ems_size 2048 ems_frame 0xd000 }

  If you have adapters, which have memory mapped IO, you may map those
  regions with hardware_ram { .. }. You can only map in entities of 4k,
  you give the address, not the segment.  The below maps
  0xc8000..0xc8fff and 0xcc000..0xcffff:

         hardware_ram { 0xc8000 range 0xcc000 0xcffff }

  With the below you define the maximum conventional RAM to show apps:

         dosmem 640

  22..22..1177..  IIRRQQ ppaassssiinngg

  The irqpassing statement accepts IRQ values between 3..15, if using
  the { .. } syntax each value or range can be prefixed by the keyword
  use_sigio to monitor the IRQ via SIGIO.  If this is missing the IRQ is
  monitored by SIGALRM.

  Use/modify one of the below statements

         irqpassing off    # this disables IRQ monitoring
         irqpassing 15
         irqpassing { 15 }
         irqpassing { use_sigio 15 }
         irqpassing { 10  use_sigio range 3 5 }

  22..22..1188..  PPoorrtt AAcccceessss

  WWAARRNNIINNGG:: GGIIVVIINNGG AACCCCEESSSS TTOO PPOORRTTSS IISS BBOOTTHH AA SSEECCUURRIITTYY CCOONNCCEERRNN AANNDD SSOOMMEE
  PPOORRTTSS AARREE DDAANNGGEERROOUUSS TTOO UUSSEE..  PPLLEEAASSEE SSKKIIPP TTHHIISS SSEECCTTIIOONN,, AANNDD DDOONN''TT
  FFIIDDDDLLEE WWIITTHH TTHHIISS SSEECCTTIIOONN UUNNLLEESSSS YYOOUU KKNNOOWW WWHHAATT YYOOUU''RREE DDOOIINNGG..

  These keywords are allowable on a "ports" line.

     rraannggee aaddddrr11 aaddddrr22
        This allow access to this range of ports

     oorrmmaasskk vvaalluuee
        The default is 0

     aannddmmaasskk vvaalluuee
        The default is 0xffff

     rrddoonnllyy||wwrroonnllyy||rrddwwrr
        This specifies what kind of access to allow to the ports. The
        default is "rdwr"

     ffaasstt
        Put port(s) in the ioperm bitmap (only valid for ports below
        0x400) An access doesn't trap and isn't logged, but as vm86()
        isn't interrupted, it's much faster. The default is not fast.

     ddeevviiccee nnaammee
        If the ports are registered, open this device to block access.
        The open() must be successfull or access to the ports will be
        denied. If you know what you are doing, use /dev/null to fake a
        device to block

         ports { 0x388 0x389 }   # for SimEarth
         ports { 0x21e 0x22e 0x23e 0x24e 0x25e 0x26e 0x27e 0x28e 0x29e } # for jill

  22..22..1199..  SSppeeaakkeerr

  These keywords are allowable on the "speaker" line:

     nnaattiivvee
        Enable DOSEMU direct access to the speaker ports.

     eemmuullaatteedd
        Enable simple beeps at the terminal.

     ooffff
        Disable speaker emulation.

  Recommended:

         speaker off

  22..22..2200..  HHaarrdd ddiisskkss

  WWAARRNNIINNGG:: DDAAMMAAGGEE MMIIGGHHTT RREESSUULLTT TTOO YYOOUURR HHAARRDD DDIISSKK ((LLIINNUUXX AANNDD//OORR DDOOSS)) IIFF
  YYOOUU FFIIDDDDLLEE WWIITTHH TTHHIISS SSEECCTTIIOONN WWIITTHHOOUUTT KKNNOOWWIINNGG WWHHAATT YYOOUU''RREE DDOOIINNGG!!

  The best way to get started is to start with a hdimage, and set
  "bootC" and "disk {image "var/lib/dosemu/hdimage.first" }/" in
  global.conf.  To generate this first working and bootable hdimage, you
  should use "setup-hdimage" in the dosemu root directory. This script
  extracts your DOS from any native bootable DOS-partition and builts a
  bootable hdimage.  (for experience dosemu users: you need not to
  fiddle with floppies any more) Keep using the hdimage while you are
  setting this hard disk configuration up for DOSEMU, and testing by
  using DIR C: or something like that.  Whenever possible, use hdimage,
  mount your DOS partition under Linux and "lredir" it into dosemu. Look
  at ``Using Lredir'', ``Running as a user'', QuickStart etc. on how to
  use "lredir".

  As a last resort, if you want DOSEMU to be able to access a DOS
  partition, the safer type of access is "partition" access, because
  "wholedisk" access gives DOSEMU write access to a whole physical disk,
  including any vulnerable Linux partitions on that drive!

  _I_M_P_O_R_T_A_N_T

  You must not have LILO installed on the partition for dosemu to boot
  off.  As of 04/26/94, doublespace and stacker 3.1 will work with
  wholedisk or partition only access.  Stacker 4.0 has been reported to
  work with wholedisk access.

  Please read the documentation in the "doc" subdirectory for info on
  how to set up access to real hard disk.

  These are meanings of the keywords:

     iimmaaggee
        specifies a hard disk image file.

     ppaarrttiittiioonn
        specifies partition access, with device and partition number.

     wwhhoolleeddiisskk
        specifies full access to entire hard drive.

     rreeaaddoonnllyy
        for read only access.  A good idea to set up with.

     ddiisskkccyyll44009966
        INT13 support for more then 1024 cylinders, bits 6/7 of DH
        (head) used to build a 12 bit cylinder number.

     bboooottffiillee
        to specify an image of a boot sector to boot from.

  Use/modify one (or more) of the folling statements:

         disk { image "/var/lib/dosemu/hdimage" }      # use diskimage file.
         disk { partition "/dev/hda1" readonly }       # 1st partition on 1st IDE.
         disk { partition "/dev/hda1" bootfile "/var/lib/bootsect.dos" }
                                                       # 1st partition on 1st IDE
                                                       # booting from the specified
                                                       # file.
         disk { partition "/dev/hda6" readonly }       # 6th logical partition.
         disk { partition "/dev/sdb1" readonly }       # 1st partition on 2nd SCSI.
         disk { wholedisk "/dev/hda" }                 # Entire disk drive unit

  Recommended:

         disk { image "/var/lib/dosemu/hdimage" }

  22..22..2211..  DDOOSSEEMMUU bboooott

  Use the following option to boot from the specified file, and then
  once booted, have bootoff execute in autoexec.bat. Thanks Ted :-).
  Notice it follows a typical floppy spec. To create this file use:

       dd if=/dev/fd0 of=/var/lib/dosemu/bdisk bs=16k

         bootdisk { heads 2 sectors 18 tracks 80 threeinch file /var/lib/dosemu/bdisk }

  Specify extensions for the CONFIG and AUTOEXEC files.  If the below
  are uncommented, the extensions become CONFIG.EMU and AUTOEXEC.EMU.
  NOTE: this feature may affect file naming even after boot time.  If
  you use MSDOS 6+, you may want to use a CONFIG.SYS menu instead.

         EmuSys EMU
         EmuBat EMU

  22..22..2222..  FFllooppppyy ddiisskkss

  This part is fairly easy.  Make sure that the first (/dev/fd0) and
  second (/dev/fd1) floppy drives are of the correct size, "threeinch"
  and/or "fiveinch".  A floppy disk image can be used instead, however.

  FFOORR SSAAFFEETTYY,, UUNNMMOOUUNNTT AALLLL FFLLOOPPPPYY DDRRIIVVEESS FFRROOMM YYOOUURR FFIILLEESSYYSSTTEEMM BBEEFFOORREE
  SSTTAARRTTIINNGG UUPP DDOOSSEEMMUU!!  DDAAMMAAGGEE TTOO TTHHEE FFLLOOPPPPYY MMAAYY RREESSUULLTT OOTTHHEERRWWIISSEE!!

  Use/modify one of the below:

         floppy { device /dev/fd0 threeinch }
         floppy { device /dev/fd1 fiveinch }
         floppy { heads 2  sectors 18  tracks 80
                  threeinch  file /var/lib/dosemu/diskimage }

  If floppy disk speed is very important, uncomment the following line.
  However, this makes the floppy drive a bit unstable.  This is best
  used if the floppies are write-protected.  Use an integer value to set
  the time between floppy updates.

         FastFloppy 8

  22..22..2233..  PPrriinntteerrss

  Printer is emulated by piping printer data to a file or via a unix
  command such as "lpr".  Don't bother fiddling with this configuration
  until you've got DOSEMU up and running already.

  NOTE: Printers are assigned to LPT1:, LPT2:, and LPT3: on a one for
  one basis with each line below.  The first printer line is assigned to
  LPT1:, second to LPT2:, and third to LPT3:.  If you do not specify a
  base port, the emulator will setup the bios to report 0x378, 0x278,
  and 0x3bc for LPT1:, LPT2:, and LPT3: respectively.

  To use standard unix lpr command for printing use this line:

         printer { options "%s"  command "lpr"  timeout 20 }

  And for any special options like using pr to format files, add it to
  the options parameter:

         printer { options "-p %s"  command "lpr"  timeout 10 }     pr format it

  To just have your printer output end up in a file, use the following
  line:

         printer { file "lpt3" }

  If you have a DOS application that is looking to access the printer
  port directly, and uses the bios LPT: setting to find out the port to
  use, you can modify the base port the bios will report with the
  following:

         printer { options "%s"  command "lpr"  base 0x3bc }

  Be sure to also add a port line to allow the application access to the
  port:

         ports { device /dev/lp0 0x3bc 0x3bd 0x3be }

  NOTE: applications that require this will not interfere with
  applications that continue to use the standard bios calls.  These
  applications will continue to send the output piped to the file or
  unix command.

  22..22..2244..  SSoouunndd

  The sound driver is more or less likely to be broken at the moment.

     ssbb__bbaassee
        base address of the SB (HEX)

     ssbb__iirrqq
        IRQ for the SB

     ssbb__ddmmaa
        DMA channel for the SB

     ssbb__ddsspp
        Path the sound device

     ssbb__mmiixxeerr
        path to the mixer control

     mmppuu__bbaassee
        base address for the MPU-401 chip (HEX) (Not Implemented)

  Use this to disable sound support even if it is configured in

         sound_emu off

  Linux defaults

         sound_emu { sb_base 0x220 sb_irq 5 sb_dma 1 sb_dsp /dev/dsp
                      sb_mixer /dev/mixer mpu_base 0x330 }

  NetBSD defaults

         sound_emu { sb_base 0x220 sb_irq 5 sb_dma 1 sb_dsp /dev/sound
                     sb_mixer /dev/mixer mpu_base 0x330 }

  22..22..2255..  DDEEXXEE ssuuppppoorrtt

  These are the setting for DEXE type DOS application, which are
  executed by DOSEMU via the -L option.  ( for what DEXE is look at
  ./doc/README.dexe

  set the below to force 'secure on', when -L option is used _and_ the
  user isn't root.

         dexe { secure }

  set the below, if you want that a dexe may be allowed to have
  additional disks. Normally the hdimage containing the DOS app (the
  .dexe itself) is the only available disk, all other 'disk {}'
  statement are ignored.

         dexe { allowdisk }

  set the below, if you want a DEXE to be forced to 'xdos', when X
  available. This mainly is intended for beeing included into the
  configuration of a DEXE file (see mkdexe). e.g. when the application
  needs graphic it should not run on slang-terminal.

         dexe { forcexdos }

  set the below, if you want a DEXE _only_ running on X (because it
  otherwise would not run)

         dexe { xdosonly }

  33..  AAcccceessssiinngg ppoorrttss wwiitthh ddoosseemmuu

  This section written by Alberto Vignani <vignani@mbox.vol.it> , Aug
  10, 1997

  33..11..  GGeenneerraall

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

  33..22..  PPoorrtt II//OO aacccceessss

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

  33..22..11..  SSyysstteemm IInntteeggrriittyy

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

  33..22..22..  SSyysstteemm SSeeccuurriittyy

  If the strategy administrator did list ports in /etc/dosemu.conf and
  allows a user listed in /etc/dosemu.users to use dosemu, (s)he must
  know what (s)he is doing. Port access is inherently dangerous, as the
  system can easily be screwed up if something goes wrong: just think of
  the blank screen you get when dosemu crashes without restoring screen
  registers...

  44..  TThhee VViirrttuuaall FFllaaggss

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

  55..  VVMM8866PPLLUUSS,, nneeww kkeerrnneell''ss vvmm8866 ffoorr aa ffuullll ffeeaattuurree ddoosseemmuu

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

  55..11..  RReessttrriiccttiioonnss

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

  55..22..  PPaarrttss iinn tthhee kkeerrnneell tthhaatt ggeett cchhaannggeedd ffoorr vvmm8866pplluuss

  55..22..11..  CChhaannggeess ttoo aarrcchh//ii338866//kkeerrnneell//vvmm8866..cc

  55..22..11..11..  NNeeww vvmm8866(()) ssyyssccaallll iinntteerrffaaccee

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

  55..22..11..22..  AAddddiittiioonnaall DDaattaa ppaasssseedd ttoo vvmm8866(())

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

  55..22..11..33..  IIRRQQ ppaassssiinngg

  Vm86plus also hosts the IRQ passing stuff now, that was a separate
  syscall in the older emumodule (no syscallmgr any more).  As this IRQ
  passing is special to dosemu, we anyway  couldn't it use for other
  (unix) applications. So having it as part of vm86() should be the
  right place.

  55..22..11..44..  DDeebbuuggggeerr ssuuppppoorrtt

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

  55..22..22..  CChhaannggeess ttoo aarrcchh//ii338866//kkeerrnneell//llddtt..cc

  55..22..22..11..  NNeeww ffuunnccttiioonnccooddee ffoorr ``wwrriittee'' iinn mmooddiiffyy__llddtt ssyyssccaallll

  In order to preserve backword compatibility with Wine and Wabi the
  changes in the LDT stuff are only available when using function code
  0x11 for `write' in the modify_ldt syscall.  Hence old binaries will
  be served with the old LDT behavior.

  55..22..22..22..  ``uusseeaabbllee'' bbiitt iinn LLDDTT ddeessccrriippttoorr

  The `struct modify_ldt_ldt_s' got an additional bit: `useable'.  This
  is needed for DPMI clients that make use of the `available' bits in
  the descriptor (bit 52).  `available' means, the hardware isn't using
  it, but software can put information into.

  Because the kernel does not use this bit, its save and harmless.
  Windows 3.1 is such a client, but also some 32-bit DPMI clients are
  reported to need it. This bit only is used for 32-bit clients.  DPMI-
  function SetDescriptorAccessRights (AX=0009) passes this in bit 4 of
  CH ((80386 extended access rights).

  55..22..22..33..  ``pprreesseenntt'' bbiitt iinn LLDDTT sseelleeccttoorr

  The function 1 (write_ldt) of syscall modify_ldt() allows
  creation/modification of selectors containing a `present' bit, that
  get updated correctly later on. These selectors are setup so, that
  they _e_i_t_h_e_r can't be used for access (null-selector) _o_r the `present'
  info goes into bit 47 (bit 7 of type byte) of a call gate descriptor
  (segment present). This call gate of course is checked to not give any
  kernel access rights.  Hence, security will not be hurt by this.

  55..22..33..  CChhaannggeess ttoo aarrcchh//ii338866//kkeerrnneell//ssiiggnnaall..cc

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

  55..22..44..  CChhaannggeess ttoo aarrcchh//ii338866//kkeerrnneell//ttrraappss..cc

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
  55..33..  AAbbaannddoonneedd ``bbeellllss aanndd wwhhiissttlleess'' ffrroomm oollddeerr eemmuummoodduullee

  ( If you have an application that needs it, well then it won't work,
  and please don't ask us to re-implement the old behaviour.  We have
  good reasons for our decision. )

  55..33..11..  KKeerrnneell ssppaaccee LLDDTT..

  Some DPMI clients have really odd programming techniques that don't
  use the LAR instruction to get info from a descriptor but access the
  LDT directly to get it. Well, this is not problem with our user space
  LDT copy (LDT_ALIAS) as long as the DPMI client doesn't need a
  reliable information about the `accessed bit'.

  In the older emumodule we had a so called KERNEL_LDT, which (readonly)
  accessed the LDT directly in kernel space. This now has been abandoned
  and we use some workarounds which may (or may not) work for the above
  mentioned DPMI clients.

  55..33..22..  LLDDTT SSeelleeccttoorrss aacccceessssiinngg tthhee ``wwhhoollee ssppaaccee''

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

  55..33..33..  FFaasstt ssyyssccaallllss

  In order to gain speed and to be more atomic on some operations we had
  so called fast syscalls, that uses INT 0xe6 to quickly enter kernel
  space get/set the dosemu used IRQ-flags and return without letting the
  kernel a chance to reschedule.

  Today the machines perform much better, so there is no need for for
  those ugly tricks any more. In dosemu-0.64.1 fast syscalls are no
  longer used.

  55..33..44..  SSeeppaarraattee ssyyssccaallll iinntteerrffaaccee ((ssyyssccaallll mmaannaaggeerr))

  The old emumodule uses the syscallmgr interface to establish a new
  (temporary) system call, that was used to interface with emumodule.
  We now have integrated all needed stuff into the vm86 system call,
  hence we do not need this technique any more.

  66..  VViiddeeoo CCooddee

  I have largely reorganized the video code. Be aware that a lot of code
  is now in a different place, and some files were moved or renamed.

  66..11..  CC ffiilleess

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

  66..22..  HHeeaaddeerr ffiilleess

     vviiddeeoo//vvcc..hh
        def's relating to console switching (formerly include/video.h)

     iinncclluuddee//vviiddeeoo..hh
        def's for general video status variables and functions

  66..33..  NNootteess

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

  66..44..  TTooddoo

  +o  cleanup terminal/console mode initialization (currently called from
     video_init() in video.c) termioInit(), termioClose() should
     probably be split up somehow (i.e. general/terminal/console)

  +o  fix bug: screen is not restored properly when killing dosemu in
     graphics mode

  +o  properly implement set_video_mode()   (int10.c)

  +o  cursor in console mode ok?

  +o  put int10.c into video.c?

  +o  put vc.c into console.c?

  77..  TThhee oolldd wwaayy ooff ggeenneerraattiinngg aa bboooottaabbllee DDOOSSEEmmuu

  Since dosemu-0.66.4 you will not need the complicated method of
  generating a bootable dosemu suite (see Quickstart). However, who
  knows, maybe you need the old information too ...

  77..11..  BBoooott (( ''ttrraaddiittiioonnaall'' mmeetthhoodd ))

  77..11..11..  IIff yyoouu aarree _n_e_w _t_o _D_O_S_E_M_U

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

  77..11..22..  IIff yyoouu aallrreeaaddyy hhaavvee aa HHDDIIMMAAGGEE ffiillee

  +o  If you have a previous version of DOSEMU running, you should copy
     the *.SYS, *.COM, & *.EXE files in the ./commands directory over to
     the hdimage drive that you boot from.  Some of these files have
     changed.

  77..11..33..  IIff yyoouu ddoonn''tt kknnooww hhooww ttoo ccooppyy ffiilleess ffrroomm//ttoo tthhee hhddiimmaaggee

  +o  Have a look at the recent mtools package (version 3.6 at time of
     writeing).  If you have the following line in /etc/mtools.conf

            drive g: file="/var/lib/dosemu/hdimage" partition=1 offset=128

  then you can use _a_l_l _t_h_e _m_t_o_o_l _c_o_m_m_a_n_d_s to access it, such as

           # mcopy g:autoexec.bat -
           Copying autoexec.bat
           @echo off
           echo "Welcome to dosemu 0.66!"

  all clear ? ;-)

  88..  NNeeww KKeeyybbooaarrdd CCooddee

  This file describes the new keyboard code which was written in late
  '95 for scottb's dosemu-0.61, and adapted to the mainstream 0.63 in
  mid-'96.

  It was last updated by R.Zimmermann <zimmerm@mathematik.uni-
  marburg.de> on 18 Sep 96 and updated by Hans <lermen@fgan.de> on 17
  Jan 97. ( correction notes marked *HH  -- Hans )

  88..11..  WWhhaattss NNeeww

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

  88..22..  SSttaattuuss

  Almost everything seems to work well now.

  The keyboard server should now quite accurately emulate all key
  combinations described the `MAKE CODES' & `SCAN CODES' tables of
  HelpPC 2.1, which I used as a reference.

  See below for a list of known bugs.

  What I need now is YOUR beta-testing... please go ahead and try if all
  your application's wierd key combinations work, and let me know if
  they don't.

  88..33..  KKeeyybbooaarrdd sseerrvveerr iinntteerrffaaccee

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

  88..44..  KKeeyybbooaarrdd sseerrvveerr ssttrruuccttuurree

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

  88..44..11..  qquueeuuee hhaannddlliinngg ffuunnccttiioonnss

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

  88..44..22..  TThhee FFrroonntt EEnndd

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

  88..44..22..11..  FFuunnccttiioonnss iinn sseerrvv__xxllaatt..cc

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

  88..44..22..11..11..  ppuuttrraawwkkeeyy

  is called with a single raw scancode byte. Scancodes from subsequent
  calls are assembled into complete keyboard events, translated and
  placed into the queue.

  88..44..22..11..22..  ppuuttkkeeyy && ootthheerrss

  ,,,to be documented.

  88..44..33..  TThhee BBaacckk EEnndd

  88..44..33..11..  QQuueeuuee BBaacckk EEnndd iinn kkeeyybbiinntt==oonn mmooddee

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

  88..44..33..22..  QQuueeuuee BBaacckk EEnndd iinn kkeeyybbiinntt==ooffff mmooddee

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

  88..44..33..33..  FFuunnccttiioonnss iinn nneewwkkbbdd--sseerrvveerr..cc

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

  88..55..  KKnnoowwnn bbuuggss && iinnccoommppaattiibbiilliitteess

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

  88..66..  CChhaannggeess ffrroomm 00..6611..1100

  +o  adapted to 0.63.55

  +o  adapted to 0.63.33

  +o  renamed various files

  +o  various minor cleanups

  +o  removed putkey_shift, added set_shiftstate

  +o  in RAW mode, read current shiftstate at startup

  +o  created base/keyboard/keyb_client.c for general client
     initialisation and paste support.

  88..77..  TTOODDOO

  +o  find what's wrong with TC++ 1.0

  +o  implement pause key

  +o  adapt x2dos (implement interface to send keystrokes from outside
     dosemu)

  +o  once everything is proved to work, remove the old keyboard code

  99..  IIBBMM CChhaarraacctteerr SSeett

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

  99..11..  WWhhaatt''ss nneeww iinn ccoonnffiigguurraattiioonn

  There is a new keyword called "charset" which accept either of these
  values: "latin" "ibm" and "fullibm".   View ./video/terminal.h for
  more information on these character set.  Here are the instructions on
  how to activate the 8-bit graphical IBM character set for DOSEMU:
  99..11..11..  IIBBMM cchhaarraacctteerr sseett iinn aann xxtteerrmm

  GREAT NEWS: You just use your existing ordinary "rxvt" or "xterm".
  Just install the VGA font by going into the DOSEMU directory and
  running the "xinstallvgafont" script by typing "./xinstallvgafont".
  Then just run "xdosemu" to start a DOSEMU window!

  99..11..22..  IIBBMM cchhaarraacctteerr sseett aatt tthhee ccoonnssoollee

  This will work if you are using the Linux console for telnet or
  kermit, to run a remote DOSEMU session.  Just simply run the
  "ibmtermdos" command on the remote end.  It puts the Linux console to
  use the IBM font for VT100 display, so that high ASCII characters are
  directly printable.

  _N_O_T_E_: This will not work with "screen" or any other programs that
  affect character set mapping to the Linux console screen.  For this,
  you should specify "charset fullibm" inside the "video { }"
  configuration in /etc/dosemu.conf.

  99..11..33..  IIBBMM cchhaarraacctteerr sseett oovveerr aa sseerriiaall lliinnee iinnttoo aann IIBBMM AANNSSII tteerrmmiinnaall

  Simply specify "charset fullibm" inside the "video { }" configuration
  in /etc/dosemu.conf and then run "dos" in the normal manner.  You must
  be running in an ANSI emulation mode (or a vt102 mode that is
  compatible with ANSI and uses the IBM character set.)

  99..22..  TTHHEE FFUUTTUURREE bbyy MMaarrkk RReejjhhoonn

  +o  NCURSES color to be added.  The IBM character set problem is
     solved.

  +o  clean up terminal code.

  +o  Add command line interface for character set and for serial port
     enable/disabling.

  +o  Use a separate "terminal { }" line for all the ncurses/termcap
     related configuration, instead of putting it in the "video { }"
     line.

  1100..  SSeettttiinngg HHooggTThhrreesshhoolldd

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

  1111..  TThhiiss sseeccttiioonn wwrriitttteenn bbyy HHaannss LLeerrmmeenn <<lleerrmmeenn@@ffggaann..ddee>> ,, AApprr 66,,
  11999977..  AAnndd uuppddaatteedd bbyy EErriicc BBiieeddeerrmmaann <<eebbiieeddeerrmm++eerriicc@@nnppwwtt..nneett>> 3300 NNoovv
  11999977..  PPrriivveelleeggeess aanndd RRuunnnniinngg aass UUsseerr

  1111..11..  WWhhaatt wwee wweerree ssuuffffeerriinngg ffrroomm

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

  1111..22..  TThhee nneeww ''pprriivv ssttuuffff''

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

  1122..  TTiimmiinngg iissssuueess iinn ddoosseemmuu

  This section written by Alberto Vignani <vignani@mbox.vol.it> , Aug
  10, 1997

  1122..11..  TThhee 6644--bbiitt ttiimmeerrss

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

  1122..22..  DDOOSS ''vviieeww ooff ttiimmee'' aanndd ttiimmee ssttrreettcchhiinngg

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

  1122..33..  NNoonn--ppeerriiooddiicc ttiimmeerr mmooddeess iinn PPIITT

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

  1122..44..  FFaasstt ttiimmiinngg

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

  1122..55..  PPIICC//PPIITT ssyynncchhrroonniizzaattiioonn aanndd iinntteerrrruupptt ddeellaayy

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

  1122..66..  TThhee RRTTCC eemmuullaattiioonn

  There is a totally new emulation of the CMOS Real Time Clock, complete
  with alarm interrupt. A process ticks at exactly 1sec, always in real
  (=system) time; it is normally implemented with a thread, but can be
  run from inside the SIGALRM call when threads are not used.

  Also, int0x1a has been mostly rewritten to keep in sync with the new
  CMOS and time-stretching features.

  1122..77..  GGeenneerraall wwaarrnniinnggss

  Do not try to use games or programs with hi-freq timers while running
  heavy tasks in the background. I tried to make dosemu quite robust in
  such respects, so it will probably NOT crash, but, apart being
  unplayable, the game could behave in unpredictable ways.

  1133..  PPeennttiiuumm--ssppeecciiffiicc iissssuueess iinn ddoosseemmuu

  This section written by Alberto Vignani <vignani@mbox.vol.it> , Aug
  10, 1997

  1133..11..  TThhee ppeennttiiuumm ccyyccllee ccoouunntteerr

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

  1133..22..  HHooww ttoo ccoommppiillee ffoorr ppeennttiiuumm

  There are no special options required to compile for pentium, the CPU
  selection is done at runtime by parsing /proc/cpuinfo. You can't
  override the CPU selection of the real CPU, only the emulated one.

  1133..33..  RRuunnttiimmee ccaalliibbrraattiioonn

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

  1133..44..  TTiimmeerr pprreecciissiioonn

  I found no info about this issue. It is reasonable to assume that if
  your CPU is specified to run at 100MHz, it should run at that exact
  speed (within the tolerances of quartz crystals, which are normally
  around 10ppm). But I suspect that the exact speed depends on your
  motherboard.  If you are worried, there are many small test programs
  you can run under plain DOS to get the exact speed.  Anyway, this
  should be not a very important point, since all the file timings are
  done by calling the library/kernel time routines, and do not depend on
  the TSC.

  1133..55..  AAddddiittiioonnaall ppooiinnttss

  The experimental 'time stretching' algorithm is only enabled when
  using the pentium (with or without TSC). I found that it is a bit
  'heavy' for 486 machines and disallowed it.

  If your dosemu was compiled with pentium features, you can switch back
  to the 'standard' (gettimeofday()) timing at runtime by adding the
  statement

           rdtsc off

  in your configuration file.

  1144..  TThhee DDAANNGG ssyysstteemm

  1144..11..  DDeessccrriippttiioonn

  The DANG compiler is a perl script which produces a linuxdoc-sgml
  document from special comments embedded in the DOSEmu source code.
  This document is intended to give an overview of the DOSEmu code for
  the benefit of hackers.

  1144..22..  CChhaannggeess ffrroomm llaasstt ccoommppiilleerr rreelleeaassee

  +o  Recognizes 'maintainer:' information.

  +o

  +o  Corrections to sgml special character protection.

  1144..33..  UUssiinngg DDAANNGG iinn yyoouurr ccooddee

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

  1144..44..  DDAANNGG MMaarrkkeerrss

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

  1144..55..  DDAANNGG__BBEEGGIINN__MMOODDUULLEE // DDAANNGG__EENNDD__MMOODDUULLEE

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

  1144..55..11..  DDAANNGG__BBEEGGIINN__FFUUNNCCTTIIOONN // DDAANNGG__EENNDD__FFUUNNCCTTIIOONN

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

  1144..55..22..  DDAANNGG__BBEEGGIINN__RREEMMAARRKK // DDAANNGG__EENNDD__RREEMMAARRKK

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

  1144..55..33..  DDAANNGG__BBEEGGIINN__NNEEWWIIDDEEAA // DDAANNGG__EENNDD__NNEEWWIIDDEEAA

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

  1144..55..44..  DDAANNGG__FFIIXXTTHHIISS

  This is for single line comments on an area which needs fixing. This
  should say where the fix is required. This may become a multi-line
  comment in the future.

  Example:

       /* DANG_FIXTHIS The mix_foo() function should be written to replace the stub */

  1144..55..55..  DDAANNGG__BBEEGGIINN__CCHHAANNGGEELLOOGG // DDAANNGG__EENNDD__CCHHAANNGGEELLOOGG

  This is not currently used. It should be placed around the CHANGES
  section in the code. It was intended to be used along with the DPR.

  * No Example *

  1144..66..  UUssaaggee

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

  1144..77..  FFuuttuurree

  I have vaguelly started writing the successor of this program. This
  will be a more general program with a more flexible configuration
  system. Don't hold your breath for an imminent release though. The new
  version will make it easier to change the markers & document
  structure, as well as providing feedback on errors.

  1155..  mmkkffaattiimmaaggee ---- MMaakkee aa FFAATT hhddiimmaaggee pprree--llooaaddeedd wwiitthh ffiilleess

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

  1166..  mmkkffaattiimmaaggee1166 ---- MMaakkee aa llaarrggee FFAATT hhddiimmaaggee pprree--llooaaddeedd wwiitthh ffiilleess

  I have also attached (gzipped in MIME format) mkfatimage16.c, a
  modified version of mkfatimage which takes an additional switch -t
  number of tracks and automatically switches to the 16-bit FAT format
  if the specified size is > 16MB.  I have deleted the "real" DOS
  partition from my disk, and so I am now running an entirely "virtual"
  DOS system within a big hdimage!

  1177..  DDooccuummeennttiinngg DDOOSSEEmmuu

  This is all about the new style DOSEmu documentation. When I was
  discussing this with Hans he was concerned (as I am) about the fact
  that we are all programmers - and generally programmers are poor at
  documentation. Of course, I'd love you all to prove me (us ?) wrong!

  However, _e_v_e_r_y programmer can handle the few basic linuxdoc-sgml
  commands that are need to make some really good documents! Much of
  linuxdoc-sgml is like HTML, which is hardly surprising as they are
  both SGMLs. The source to this document may make useful reading (This
  is the file './src/doc/README/doc')

  1177..11..  SSeeccttiioonnss

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

  1177..22..  EEmmpphhaassiissiinngg tteexxtt

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

  1177..33..  LLiissttss

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

  1177..44..  QQuuoottiinngg ssttuuffff

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

  1177..55..  SSppeecciiaall CChhaarraacctteerrss

  Obviously some characters are going to need to be quoted. In general
  these are the same ones as HTML (eg < is written as &lt;) There are 2
  additional exceptions (which had me in trouble until I realised) which
  are [ and ]. These _m_u_s_t be written as &lsqb; and &rsqb;.

  1177..66..  CCrroossss--RReeffeerreenncceess && UURRLLss

  One of the extra feature that this lets us do is include URLs and
  cross-references.

  1177..66..11..  CCrroossss--RReeffeerreenncceess

  These have 2 parts: a label, and a reference.

  The label is <label id="...">

  The reference is <ref id="..." name="...">. The id should refer to a
  lable somewhere else in the document (not necessarily the same file as
  the REAMDE's consist of multiple files) The content of name will be
  used as the link text.

  1177..66..22..  UURRLLss

  This looks slightly horrible, but is very flexible. It looks quite
  similar to the reference above. It is <htmlurl url="..."  name="...">.
  The url will be active _o_n_l_y for HTML. The text in _n_a_m_e will be used at
  all times. This means that it is possible to make things like email
  addresses look good! eg:

       <htmlurl url="mailto:someone@no.where.com" name="&lt;someone@no.where.com&gt;">

  Which will appear as <someone@no.where.com>

  1177..77..  GGoottcchhaass

  +o  You _m_u_s_t do the sect*'s in sequence on the way up

  +o  You _m_u_s_t have a <p> after each sect*

  +o  You _m_u_s_t close your lists

  +o  You _m_u_s_t reverse the order of the tags when closing tscreen/verb
     structures

  +o  You _m_u_s_t replace <, >, [ and ]

  1188..  SSoouunndd CCooddee

  1188..11..  CCuurrrreenntt DDOOSSEEmmuu ssoouunndd ccooddee

  Unfortunately I haven't documented this yet. However, the current code
  has been completely rewritten and has been designed to support
  multiple operating systems and sound systems.

  For details of the internal interface and any available patches see my
  WWW page at http://www.slitesys.demon.co.uk/a.macdonald/dosemu/sound/

  1188..22..  OOrriiggiinnaall DDOOSSEEMMUU ssoouunndd ccooddee

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

  1199..  DDMMAA CCooddee

  1199..11..  CCuurrrreenntt DDOOSSEEmmuu DDMMAA ccooddee

  Unfortunately I haven't documented this yet. However, the current code
  has been completely rewritten from this.

  1199..22..  OOrriiggiinnaall DDOOSSEEMMUU DDMMAA ccooddee

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

  1199..22..11..  AAddddiinngg DDMMAA ddeevviicceess ttoo DDOOSSEEMMUU

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

  2200..  DDOOSSEEmmuu PPrrooggrraammmmaabbllee IInntteerrrruupptt CCoonnttrroolllleerr

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

  2200..11..  OOtthheerr ffeeaattuurreess

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

  2200..22..

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

  2200..33..

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

  2200..33..11..  FFuunnccttiioonnss ssuuppppoorrtteedd ffrroomm DDOOSSEEmmuu ssiiddee

  2200..33..11..11..  FFuunnccttiioonnss tthhaatt IInntteerrffaaccee wwiitthh DDOOSS::

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

  2200..33..22..  OOtthheerr FFuunnccttiioonnss

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

  2200..44..  AA ((vveerryy)) lliittttllee tteecchhnniiccaall iinnffoorrmmaattiioonn ffoorr tthhee ccuurriioouuss

  There are two big differences when using pic.  First, interrupts are
  not queued beyond a depth of 1 for each interrupt.  It is up to the
  interrupt code to detect that further interrupts are required and
  reschedule itself.  Second, interrupt handlers are designated at
  dosemu initialization time.  Triggering them involves merely
  specifying the appropriate irq.

  Since timer interrupts are always spaced apart, the lack of queueing
  has no effect on them.  The keyboard interrupts are re-scheduled if
  there is anything in the scan_queue.

  2211..  DDOOSSEEMMUU ddeebbuuggggeerr vv00..66

  This section written on 98/02/08.  Send comments to Max Parke
  <mhp@light.lightlink.com> and to Hans Lermen <lermen@fgan.de>

  2211..11..  IInnttrroodduuccttiioonn

  This is release v0.6 of the DOSEMU debugger, with the following
  features:

  +o  interactive

  +o  DPMI-support

  +o  display/disassembly/modify of registers and memory (DOS and DPMI)

  +o  display/disassembly memory (dosemu code and data)

  +o  read-only access to DOSEMU kernel via memory dump and disassembly

  +o  uses /usr/src/dosemu/dosemu.map for above (can be changed via
     runtime configuration)

  +o  breakpoints (int3-style, breakpoint on INT xx and DPMI-INT xx)

  +o  DPMI-INT breakpoints can have an AX value for matching.  (e.g.
     'bpintd 31 0203' will stop _before_ DPMI function 0x203)

  +o  breakpoints via monitoring DOSEMU's logoutput using regular
     expressions

  +o  on-the-fly changing amount of logoutput (-D debugflags)

  +o  (temporary) redirect logoutput to debugger terminal.

  +o  single stepping (traceing).

  +o  dump parts of DOS mem to file.

  +o  symbolic debugging via microsoft linker .MAP file support

  +o  access is via the 'dosdebug' client from another virtual console.
     So, you have a "debug window" and the DOS window/keyboard, etc. are
     undisturbed.    VM86 execution can be started, stopped, etc.

  +o  If dosemu 'hangs' you can use the 'kill' command from dosbugger to
     recover.

  +o  code base is on dosemu-0.97.2.1

  2211..22..  UUssaaggee

  To run, start up DOSEMU.  Then switch to another virtual console (or
  remote login or use another xterm) and do:

         dosdebug

  If there are more then one dosemu process running, you will need to
  pass the pid to dosdebug, e.g:

         dosdebug 2134

  _N_O_T_E_: You must be the owner of the running dosemu to 'debug-login'.

  You should get connected and a banner message.  If you type 'q', only
  the terminal client will terminate, if you type 'kill', both dosemu
  and the terminal client will be terminated.

  It may be desirable to debug the DOS or its drivers itself during
  startup, to realize that you need to synchronize DOSEMU and your
  debugger terminal.  This can be done using the -H1 command line option
  of DOSEMU:

         $ dos -H1

  DOSEMU will then lock before jumping into the loaded bootsector wait-
  ing for dosdebug to connect. Once connected you are in `stopped' state
  and can set breakpoints or singlestep through the bootstrap code.

  2211..33..  CCoommmmaannddss

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

     ee AADDDDRR vvaalluueelliisstt
        modify memory (0-1Mb) `ADDR' maybe just a `-' (minus), then last
        (incremented) address from a previous `e' or `ed' command is
        used (this allowes consecutive writes).

        `valuelist' is a blank separated list of

        hheexxnnuummbbeerr
           such as 0F or C800

        cchhaarr
           enclosed in single quotes such as 'A' or 'b'

        rreeggiisstteerr
           any valid register symbol, in this case the current value
           (and size) of that registe is take (e.g AX is 2 bytes, EAX is
           4 bytes)

        ssttrriinngg
           enclosed in double quotes such "this is a string"

        The default size of each value is one byte (except registers),
        this size can be overridden by suffixing a `W' (word, 2 bytes)
        or `L' (long, 4 bytes) such as C800w or F0008123L

     eedd AADDDDRR vvaalluueelliisstt
        same as above `e' command, except that the numbers are expected
        as _d_e_c_i_m_a_l_s per default. To write a hexvalue with `ed' you may
        prefix it with `0x' as in C or write an octal value prefixing  a
        `0'.

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

     tt  single step (may jump over IRET or POPF)

     ttcc single step, loop forever until key pressed

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

     bbpplloogg rreeggeexx
        set a breakpoint on logoutput using regex. With this the normal
        DOSEMU log output (enabled via the -D commandline option or the
        dosdebug `log' command) is monitored via the regular expression
        `regex' (look at GNU regex manual) and when a match is found
        emulation is set into `stopped' mode. There may be 8 log
        breakpoint active simultaneously. Without the `regex' given
        `bplog' such prints the current active breakpoints.

     bbppcclloogg nnuummbbeerr
        clears a log break point.

     lloogg ffllaaggss
        get/set debug-log flags (e.g 'log +M-k')

     lloogg oonn||ooffff
        redirect dbug-log output to the dosdebug terminal

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

  2211..44..  PPeerrffoorrmmaannccee

  If you have dosemu compiled with the debugger support, but the
  debugger is not active and/or the process is not stopped, you will not
  see any great performance penalty.

  2211..55..  WWiisshh LLiisstt

  Main wish is to add support for hardware debug registers (if someone
  would point me in the direction, what syscalls to use, etc.)  Then you
  could breakpoint on memory reads/writes, etc!

  2211..66..  BBUUGGSS

  There must be some.

  2211..66..11..  KKnnoowwnn bbuuggss

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
     as a work around: Setting breakpoints while not in stop is
     disabled.

  2222..  MMAARRKK RREEJJHHOONN''SS 1166555500 UUAARRTT EEMMUULLAATTOORR

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

  2222..11..  PPRROOGGRRAAMMMMIINNGG IINNFFOORRMMAATTIIOONN

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

  2222..22..  DDEEBBUUGGGGIINNGG HHEELLPP

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

  2222..33..  FFOOSSSSIILL EEMMUULLAATTIIOONN

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

  2222..44..  CCOOPPYYRRIIGGHHTTSS

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

  2233..  RReeccoovveerriinngg tthhee ccoonnssoollee aafftteerr aa ccrraasshh

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

  2233..11..  TThhee mmaaiill mmeessssaaggee

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

  2244..  NNeett ccooddee

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

  2255..  SSooffttwwaarree XX338866 eemmuullaattiioonn

  This section written in a hurry by Alberto Vignani
  <vignani@mbox.vol.it> , Oct 20, 1997

  2255..11..  TThhee CCPPUU eemmuullaattoorr

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

