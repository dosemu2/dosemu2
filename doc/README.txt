  DOSEmu
  The DOSEmu team Edited by Alistair MacDonald  <alis-
  tair@slitesys.demon.co.uk>
  For DOSEMU v0.67 pl5.0

  This document is the amalgamation of a series of README files which
  were created to deal with the lack of DOSEmu documentation.
  ______________________________________________________________________

  Table of Contents:

  1.      Introduction

  2.      Runtime Configuration Options

  2.1.    Format of /etc/dosemu.users

  2.2.    Format of /etc/dosemu.conf (.dosrc, -I option)

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

  3.      Security

  4.      Directly executable DOS applications using dosemu (DEXE)

  4.1.    Making and Using DEXEs

  4.2.    Using binfmt_misc to spawn DEXEs directly

  4.3.    Making a bootable hdimage for general purpose

  4.4.    Accessing hdimage files using mtools

  5.      Sound

  5.1.    Using the MPU-401 "Emulation".

  5.2.    The MIDI daemon

  5.3.    Disabling the Emulation at Runtime

  6.      Using Lredir

  6.1.    how do you use it?

  6.2.    Other alternatives using Lredir

  7.      Running dosemu as a normal user

  8.      Using CDROMS

  8.1.    The built in driver

  9.      Logging from DOSEmu

  10.     Using X

  10.1.   Latest Info

  10.2.   Slightly older information

  10.3.   Status of X support (Sept 5, 1994)

  10.3.1. Done

  10.3.2. ToDo (in no special order)

  10.4.   The appearance of Graphics modes (November 13, 1995)

  10.4.1. vgaemu

  10.4.2. vesa

  10.4.3. X

  10.5.   The new VGAEmu/X code (July 11, 1997)

  11.     Running Windows under DOSEmu

  11.1.   Windows 3.0 Real Mode

  11.2.   Windows 3.1 Protected Mode

  11.3.   Windows 3.x in xdos

  12.     Mouse Garrot

  13.     Running a DOS-application directly from Unix shell

  13.1.   Using the keystroke and commandline options.

  13.2.   Using an input file

  14.     Setting HogThreshold

  15.     Commands & Utilities

  15.1.   Programs

  15.2.   Drivers

  16.     Keymaps

  17.     Networking using DOSEmu

  17.1.   The DOSNET virtual device.

  17.2.   Setup for virtual TCP/IP

  17.3.   Full Details

  17.3.1. Introduction

  17.3.2. Design

  17.3.3. Implementation

  17.3.4. Virtual device 'dsn0'

  17.3.5. Packet driver code

  17.3.6. Conclusion ..

  17.3.6.1.       Telnetting to other Systems

  17.3.6.2.       Accessing Novell netware

  18.     Using Windows and Winsock

  18.1.   LIST OF REQUIRED SOFTWARE

  18.2.   STEP BY STEP OPERATION (LINUX SIDE)

  18.3.   STEP BY STEP OPERATION (DOS SIDE)
  ______________________________________________________________________

  11..  IInnttrroodduuccttiioonn

  This documentation is derived from a number of smaller documents. This
  makes it easier for individuals to maintain the documentation relevant
  to their area of expertise. Previous attempts at documenting DOSEmu
  failed because the documentation on a large project like this quickly
  becomes too much for one person to handle.

  22..  RRuunnttiimmee CCoonnffiigguurraattiioonn OOppttiioonnss

  This section of the document by Hans, <lermen@fgan.de>. Last updated
  on January 15, 1998.
  Most of DOSEMU configuration is done during runtime and per default it
  expects the system wide configuration file /etc/dosemu.conf,
  optionally folowed by the users  /.dosrc and additional configurations
  statements on the commandline (-I option). The builtin configuration
  file of a DEXE file is passed using the -I technique, hence the rules
  of -I apply.

  However, the first file expected (and interpreted before) is
  /etc/dosemu.users.  Within /etc/dosemu.users the general permissions
  are set:

  +o  which users are allowed to use DOSEMU.

  +o  what kind of access class the user belongs to.

  +o  wether the user is allowed to define a private dosemu.conf that
     replaces /etc/dosemu.conf (option -F).

  +o  what special configuration stuff the users needs

  This is done via setting configuration variables.

  After /etc/dosemu.users /etc/dosemu.conf is interpreted, and only if
  its _r_e_a_l_l_y this file (not a -F one) access to all configuration
  options is allowed.

  The /etc/dosemu.conf may check for the configuration variables, that
  are set in /etc/dosemu.users and optionaly include further
  configuration files. But once /etc/dosemu.conf has finished
  interpretation, the access to secure relevant configurations is
  (class-wise) restricted while the following interpretation of .dosrc
  and -I statements.

  For an example of a 'sophisticated' configuration look at the end of
  this readme. For an example of a general configuration  look at
  ./etc/dosemu.conf. The later behaves insecure, when /etc/dosemu.users
  is a copy of ./etc/dosemu.users.easy and behave 'secure', when
  /etc/dosemu.users is a copy of ./etc/dosemu.users.secure.

  22..11..  FFoorrmmaatt ooff //eettcc//ddoosseemmuu..uusseerrss

  Each line corresponds to exactly _one_ valid user count:

         loginname [ c_strict ] [ classes ...] [ c_dexeonly ] [ other ]

  where the elements are:

     llooggiinnnnaammee
        valid login name (root also is one) or 'all'. The later means
        any user not mentioned in previous lines.

     cc__ssttrriicctt
        Do not allow -F option (/etc/dosemu.conf can't be replaced)
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
        test in /etc/dosemu.conf (or .dosrc, -I), see 'ifdef', 'ifndef'
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

  22..22..  FFoorrmmaatt ooff //eettcc//ddoosseemmuu..ccoonnff ((..ddoossrrcc,, --II ooppttiioonn))

  The configuration files are not line oriented, instead are consisting
  of `statements' (optionally separated by `;'), whitespaces are removed
  and all behind a '#' up to the end of the line is treated as comment.
  ( Note that older DOSEMUs also allowed `!' and `;' as comment
  character, but those are no longer supported ).

  22..22..11..  EEnnvviirroommeenntt vvaarriiaabblleess aanndd ccoonnffiigguurraattiioonn vvaarriiaabblleess

  They existed already in very early versions of DOSEMU (until now), but
  now evironment variables are much more useful in /etc/dosemu.conf as
  before, because you now can set them, test them in the new 'if'
  statement and compute them in expressions.  The problem with the
  enviroment variables is, however, that the user may set and fake them
  _b_e_f_o_r_e calling DOSEMU, hence this is a security problem. To avoid
  this, we have the above mentioned _c_o_n_f_i_g_u_r_a_t_i_o_n _v_a_r_i_a_b_l_e_s, which are
  of complete different name space and are not visible outside of
  DOSEMU's configuration parser. On the other hand it may be useful to
  export settings from /etc/dosemu.conf to the running DOS environment,
  which can be achieved by the 'unix.exe -e' DOS programm.

  To summarize:

     ccoonnffiigguurraattiioonn vvaarriiaabblleess
        have their own namespace only within the configuration parser.
        They usual are prefixed by _c__, _u__ and _h__ and cannot be made
        visible outside. They do not contain any value and are only
        tested for existence.

     eennvviirroonnmmeenntt vvaarriiaabblleess
        are inherited from the calling process, can be set within
        /etc/dosemu.conf and passed to DOSEMU running DOS-applications.
        Within /etc/dosemu.conf they always are prefixed by '$' (Hence
        TERM becomes $TERM, even on the left side of an assigment).
        However, _s_e_t_t_i_n_g them is controled by the configuration variable
        'c_var' (see above) and may be disallowed within the user
        supplied .dosrc and alike.

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

  However, use of define/undef is restricted to scope of
  /etc/dosemu.conf, as long as you don't 'define c_var' _within_
  /etc/dosemu.conf.  If you are under scope of a 'user config file'
  (e.g. outside /etc/dosemu.conf) you have to prefix the _c_o_n_f_i_g_u_r_a_t_i_o_n
  _v_a_r_i_a_b_l_e name with 'u_', else it will not be allowed to be set/unset
  (hence 'c_' type variables can't be unset out of scope of
  /etc/dosemu.conf).

  There are some _c_o_n_f_i_g_u_r_a_t_i_o_n _v_a_r_i_a_b_l_e_s (besides the ones described
  above for dosemu.users) implicitely predefined by DOSEMU itself:

     cc__ssyysstteemm
        set while being in /etc/dosemu.conf

     cc__uusseerr
        set while parsing a user configuration file

     cc__ddoossrrcc
        set while parsing .dosrc

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
  parsing any configuration file. You then may check this in your
  ./dosrc or /etc/dosemu.conf to do the needed special configuration.

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
  which is included, hence all what is included by /etc/dosemu.conf has
  its privilege.

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
        be executed from within /etc/dosemu.conf.

  With these tools you should be able to make your /etc/dosemu.conf
  adapt to any special case, such as different terminal types, different
  hdimages for different users and/or different host, adapt to different
  (future) dosemu and/or kernel versions. Here some small examples:

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

  It may be usefull to verify, that your /etc/dosemu.conf does what you
  want before starting a real running DOSEMU. For this purpose there is
  a new commandline option (-h), which just runs the parser, print some
  useful output, dumps the main configuration structure and then exits.
  The option has an argument (0,1,2), which sets the amount of parser
  debug output: 0 = no parser debug, 1 = print loop debugs, 2 = same as
  1 plus if_else_endif-stack debug. This feature can be used such as

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
  except for help debugging your /etc/dosemu.conf such as

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
  inclusion into dosemu.conf (hence it follows the syntax):

         keytable dump "kbdtables"

  However, don't put this not into your dosemu.conf, because dosemu will
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
  in /etc/dosemu.conf), they new one will be take anyway and a warning
  will be printed on stderr.

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
  /etc/dosemu.conf.  To generate this first working and bootable
  hdimage, you should use "setup-hdimage" in the dosemu root directory.
  This script extracts your DOS from any native bootable DOS-partition
  and builts a bootable hdimage.  (for experience dosemu users: you need
  not to fiddle with floppies any more) Keep using the hdimage while you
  are setting this hard disk configuration up for DOSEMU, and testing by
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

  33..  SSeeccuurriittyy

  This part of the document by Hans Lermen, <lermen@fgan.de> on Apr 6,
  1997.

  These are the hints we give you, when running dosemu on a machine that
  is (even temporary) connected to the internet or other machines, or
  that otherwise allows 'foreign' people login to your machine.

  +o  _N_E_V_E_R let foreign users execute dosemu under root login !!!
     (Starting with dosemu-0.66.1.4 this isn't necessary any more, all
     functionality should also be available when running as user)

  +o  Do _n_o_t configure dosemu with the --enable-runasroot option.
     Normally dosemu will switch privileges off at startup and only set
     them on, when it needs them. With '--enable-runasroot' it would
     permanently run under root privileges and only disable them when
     accessing secure relevant resources, ... not so good.

  +o  Never allow DPMI programms to run, when dosemu is suid root,

     (in /etc/dosemu.conf set 'dpmi off' to disable)

     It is possible to overwrite sensitive parts of the emulator code,
     and this makes it possible for a intruder program under DOS, who
     knows about dosemu interna (what is easy as you have the source) to
     get root access also on non dosemu processes.  Because a lot of
     games won't work without, we allow creation of LDT-descriptor that
     span the whole user space.

     There is a 'secure' option in /etc/dosemu.conf, that allows to turn
     off creation of above mentioned descritors, but those currently
     protect only the dosemu code and the stack, may be some diabolic
     person finds a way to use the (unprotected) heap in his sense of
     humor.

     Anyway, better 'secure on' then nothing.

  +o  Never allow the 'system.com' command (part of dosemu) to be
     executed.  It makes dosemu execute the libc 'system() function'.
     Though privileges are turned off, the process inherits the switched
     uid-setting (uid=root, euid=user), hence the unix process can use
     setreuid to gain root access back. ... the rest you can imagine
     your self. Use of 'system' can be disabled by the 'secure on'
     option in /etc/dosemu.conf

     The 'unix.com' command (also part of dosemu) does _not_ have this
     security hole: before execution a separate process is forked that
     completely drops prililege,

  44..  DDiirreeccttllyy eexxeeccuuttaabbllee DDOOSS aapppplliiccaattiioonnss uussiinngg ddoosseemmuu ((DDEEXXEE))

  This section of the document by Hans, <lermen@fgan.de>. Last updated
  on June 16, 1997.

  44..11..  MMaakkiinngg aanndd UUssiinngg DDEEXXEEss

  Well, you may have wondered what these *.dexe file extension stands
  for.  Looking at the title above you now know it ;-)

  In fact its a tiny hdimage which is bootable and just contains _o_n_e DOS
  application. Because this has isolated access to the hdimage only, it
  may have less security issues then a complete DOS environement under
  Linux and, more worth, you need not to fiddle with installation,
  because the implementor already has done this for you.

  On the other hand, you may create your own *.dexe files using the
  script `mkdexe'. For this to run however you need at least mtools-3.6
  because this versions has options that older versions don't have.

  In detail you need the following to make a *.dexe:

  +o  have _m_t_o_o_l_s_-_3_._6 available.

  +o  have dosemu already compiled _a_n_d did _n_o_t 'make pristine'

  +o  have a *.zip file containing the all files which belong to the DOS
     application (may be whole directories)

  +o  have the following info handy before starting 'mkdexe':

  +o  The partition size needed for the hdimage

  +o  The DOS version that you want to put onto the hdimage.  If your DOS
     app can run under the FreeDos kernel, you don't need that, else
     'mkdexe' needs to know from what bootable existing partition to get
     the system from.

  +o  The contents of the config.sys and autoexec.bat.

  +o  then (as root) do

              # cd ./dexe
              # mkdexe myapp.zip -x myapp.exe -o confirm

  +o  If all went ok, you then have a /var/lib/dosemu/myapp.dexe and this
     one can be executed via

              # dos -L myapp.dex [ dosemu-options ]

  or

              # dosexec myapp.dex [ dosemu-options ]

  Here is what ./mkdexe will print as help, when called without
  argument:

     USAGE:
       mkdexe [{ application | hdimage}]
                          [-b dospart] [{-s|-S} size] [-x appname]
                          [-c confsys] [-a autoexe] [-C comcom ] [-d dosemuconf]
                          [-i IOname] [-m MSname]
                          [-o <option> [-o ...]]

       application  the whole DOS application packet into a *.zip file
       hdimage      the name of the target hdimage, ih -o noapp is give
                    (see below)
       dospart      If not given, FreeDos will be used as system
                    If given it must be either a bootable DOS partion (/dev/...)
                    or a already made bootable dosemu hdimage
       -s size      The _additional_ free space (in Kbytes) on the hdimage
       -S size      The total size (in Kbytes) of the hdimage -s,-S are mutual
                    exclusive.
       appname      The DOS filename of the application, that should be executed
       confsys      Template for config.sys
       autoexe      Template for autoexec.bat
       comcom       file name of the shell, usually command.com
       dosemuconf   Template for the dosemu.conf to use
       IOname       The name of DOS file, that usually is called IO.SYS,
                    (default for FreeDos: IPL.SYS) this one is always put as
                    first file onto the hdimage
       MSname       The name of DOS file, that usually is called MSDOS.SYS,
                    (default for FreeDos: MSDOS.SYS) this one is always put as
                    second file onto the hdimage
       -o <option>  Following option flags are recognized:
                      confirm   offer config.sys, autoexec.bat and dconfig
                                to edit via $EDITOR
                      nocomcom  Omit command.com, because its not used anyway
                                when using  shell=c:\appname.exe
                      noapp     Make a simple bootable hdimage for standard
                                DOSEMU usage (replacement for hdimage.dist)

  If you want to change the builtin configuration file, you may use
  ./src/tools/periph/dexeconfig to extract/re-insert the configuration.

         # dexeconfig -x configfile dexefile

  extracts the configuration out of 'dexefile' and puts it into
  'configfile'

         # dexeconfig -i configfile dexefile

  does the reverse.

  There is a problem, however, when you want allow a user to execute a
  DEXE and the DOS application needs to write to a file within the dexe
  (which is in fact a whole DOS FS). For this would have to give write
  permissions to the user (what is not what you want).  DEXEs have a
  workaround for this: You may set read-only permissions for the DEXE
  file itself in the Linux-FS, but set a flag in the DEXE itself, so
  that DOSEMU will open this file read/write anyway.  This means: The
  user cannot delete or replace the DEXE, but the imbedded DOS-
  application can (totally isolated) write to DOS files within the DEXE
  (complicated, isn't it?). To set such a permission, you (again) need
  `dexeconfig':

         # dexeconfig -p W dexefile

  Here is what 'dexeconfig' will print as help, when called without
  argument:

          USAGE:
             dexeconfig [-M] [-p {w|W}] -i configfile dexefile
             dexeconfig -x configfile dexefile
             dexeconfig -v  dexefile
             dexeconfig -p {w|W}  dexefile

          where is
             -i      insert a config file
             -x      extract a config file
             -p w    clear write permission
             -p W    set write permission
             -v      view status information

  It would be great, if we could collect an archive of working, free
  distributatble *.dexe files, and I'm herein asking for contribution.
  However, _B_I_G _N_O_T_E, if you want to contribute a *.dexe file to the
  public, please do _N_O_T use any other DOS than FreeDos else you would
  violate existing copyrights. This also (unfortunately) is true for
  OpenDos which can only be distributed after Caldera did allow you to
  do so :-(

  If you _h_a_v_e assembled a *.dexe and you wnat to contribute it, please
  send me a mail and upload the stuff to

          tsx-11.mit.edu:/pub/linux/ALPHA/dosemu/Development/Incoming

  I'll then put it into

          tsx-11.mit.edu:/pub/linux/ALPHA/dosemu/dexe/*
          ftp.suse.com:/pub/dosemu/dexe/*

  There currently only is one in that archive: fallout.dexe Its a nice
  Tetris like games that I found on an old CDrom and which runs in
  300x200 on console and X (not Slang-terminal). When you put it into
  you /var/lib/dosemu/* directory, you may start it via:
          dosexec fallout.dexe -X

  Hope we get more of *.dexe in the future ....

  44..22..  UUssiinngg bbiinnffmmtt__mmiisscc ttoo ssppaawwnn DDEEXXEEss ddiirreeccttllyy

  There is a nifty kernel patch flying around from Richard Guenther
  <zxmpm11@student.uni-tuebingen.de>. This can be obtained via

  <http://www.anatom.uni-tuebingen.de/~richi/linux/binfmt_misc.html>

  We hope this patch makes it into the kernel, because then we could
  execute a DEXE just by typing its name at the Bash prompt. To register
  DEXE format using binfmt_misc you do (in your /etc/rc.-whatever)

         cd /proc/sys/fs/binfmt_misc
         echo :DEXE:M::\\x0eDEXE::/usr/bin/dosexec: >register

  thats all.

  44..33..  MMaakkiinngg aa bboooottaabbllee hhddiimmaaggee ffoorr ggeenneerraall ppuurrppoossee

  You may also use './mkdexe' do generate a _normal_ bootable hdimage,
  it then has the advantage, that you no longer need to fiddle with a
  DOS boot disk. I succeded to make a bootable hdimage with FreeDos,
  MSDOS-6.2 and also WINDOWS'95 (yes, that can be booted with the
  DOSEMU-own MBR ;-) I did not test other DOSes, but I guess, they also
  will work, as long as you pass the correct system file names to
  'mkdexe' (-i, -m options)

  Example: Given you have a bootable DOS-partition in /dev/hda1, then
  this ...

         # cd ./dexe
         # ./mkdexe myhdimage -b /dev/hda1 -o noapp

  will generate a direct bootable 'myhdimage' from your existing DOS
  installation. You need not to make a boot floppy, nor need you to
  fiddle with fdisk /MBR and sys.com any more. Using -o confirm you may
  also edit the configuration files before they are put onto the
  hdimage.

  Further more, there is a script on top of mkdexe: setup-hdimage, which
  helps more to firsttime install DOSEMU's hdimage. It prompts for
  needed things and should work on most machines.

    # cd /where/I/have/dosemu
    # ./setup-hdimage

  44..44..  AAcccceessssiinngg hhddiimmaaggee ffiilleess uussiinngg mmttoooollss

  In the ./dexe directory there is also a script, that allows you to
  directly access the hdimage's files, even without changing your
  /etc/mtools.conf. The usage of this script is:

          USAGE:
            do_mtools device mcommand [ arg1 [...] ]

          where is:
            device    = DOS-partition such as '/dev/hda1'
                        or a DOSEMU hdimage
            mcommand  = any valid mtools comand
            argX      = any valid mtools argument.
                        NOTE: for the DOS drive use 'W:'

          example:
            do_mtools /var/lib/dosemu/hdimage mcopy W:/autoexec.bat -

  55..  SSoouunndd

  The SB code is currently in a state of flux. Some changes to the code
  have been made which mean that I can separate the DSP handling from
  the rest of the SB code, making the main case statements simpler. In
  the meantime, Rutger Nijlunsing has provided a method for redirecting
  access to the MPU-401 chip into the main OS.

  55..11..  UUssiinngg tthhee MMPPUU--440011 ""EEmmuullaattiioonn""..

  The Sound driver opens "/var/run/dosemu-midi" and writes the Raw MIDI
  data to this. A daemon is provided which can be can be used to seletc
  the instruments required for use on some soundcards. It is also
  possible to get various instruments by redirecting '/var/run/dosemu-
  midi' to the relevant part of the sound driver eg:

       % ln -s /dev/midi /var/run/dosemu-midi

  This will send all output straight to the default midi device and use
  whatever instruments happen to be loaded.

  55..22..  TThhee MMIIDDII ddaaeemmoonn

         make midid

  This compiles and installs the midi daemon. The daemon currently has
  support for the 'ultra' driver and partial support for the 'OSS'
  driver (as supplied with the kernel) and for no midi system. Support
  for the 'ultra' driver will be compiled in automatically if available
  on your system.

  Copy the executable './bin/midid' so that it is on your path, or
  somewhere you can run it easily.

  Before you run DOSEmu for the first time, do the following:

         rm -f /var/run/dosemu-midi
         mknod /var/run/dosemu-midi p

  Then you can use the midi daemon like this:

         ./midid < /var/run/dosemu-midi &; dos

  (Assuming that you put the midid executeable in the directory you run
  DOSEmu from.)

  55..33..  DDiissaabblliinngg tthhee EEmmuullaattiioonn aatt RRuunnttiimmee

  You can now disable the code after it is compiled in by setting the
  IRQ or DMA channels to invalid values (ie IRQ = 0 or > 15, or DMA = 4
  or > 7) The simplest way, however, is to say 'sound_emu off' in
  /etc/dosemu.conf

  66..  UUssiinngg LLrreeddiirr

  This section of the document by Hans, <lermen@fgan.de>. Last updated
  on June 16, 1997.

  What is it? Well, its simply a small DOS program that tells the MFS
  (Mach File System) code what 'network' drives to redirect.  With this
  you can 'mount' any Linux directory as a virtual drive into DOS. In
  addition to this, Linux as well as multiple dosemu sessions may
  simultaneously access the same drives, what you can't when using
  partition access.

  66..11..  hhooww ddoo yyoouu uussee iitt??

  First, mount your dos hard disk partition as a Linux subdirectory.
  For example, you could create a directory in Linux such as /dos (mkdir
  -m 755 /dos) and add a line like

          /dev/hda1       /dos     msdos   umask=022

  to your /etc/fstab.  (In this example, the hard disk is mounted read-
  only.  You may want to mount it read/write by replacing "022" with
  "000" and using the -m 777 option with mkdir).  Now mount /dos.  Now
  you can add a line like

         lredir d: linux\fs/dos

  to the AUTOEXEC.BAT file in your hdimage (but see the comments below).
  On a multi-user system you may want to use

         lredir d: linux\fs\${home}

  where "home" is the name of an environmental variable that contains
  the location of the dos directory (/dos in this example)

  However, you may want to have your native DOS partion as C: under
  dosemu.  To reach this aim you also can use Lredir to turn off the
  'virtual' hdimage and switch on the _r_e_a_l drive C: such as this:

  Assuming you have a c:osemu directory on both drives (the virtual and
  the real one) _a_n_d have mounted your DOS partition as /dosc, you then
  should have the following files on the virtual drive:

  autoexec.bat:

         lredir z: linux\fs\dosc
         copy c:\dosemu\auto2.bat z:\dosemu\auto2.bat
         lredir del z:
         c:\dosemu\auto2.bat

  dosemuuto2.bat:

         lredir c: linux\fs\dosc
         rem further autoexec stuff

  To make the reason clear _w_h_y the batch file (not necessaryly
  autoexec.bat) _m_u_s_t be identical:

  Command.com, which interpretes the batchfile keeps a position pointer
  (byte offset) to find the next line within this file. It opens/closes
  the batchfile for _e_v_e_r_y new batchline it reads from it.  If the
  batchfile in which the 'lredir c: ...' happens is on c:, then
  command.com suddenly reads the next line from the batchfile of that
  newly 'redired' drive.  ... you see what is meant?

  66..22..  OOtthheerr aalltteerrnnaattiivveess uussiinngg LLrreeddiirr

  To have a redirected drive available at time of config.sys you may
  either use emufs.sys such as

          device=c:\emufs.sys /dosc

  or make use of the install instruction of config.sys (but not both)
  such as

          install=c:\lredir.exe c: linux\fs\dosc

  The later has the advantage, that you are on your native C: from the
  beginning, _b_u_t, as with autoexec.bat, both config.sys must be
  identical.

  For information on using 'lredired' drives as a 'user' (ie having the
  right permissions), please look at ./doc/README.runasuser.

  77..  RRuunnnniinngg ddoosseemmuu aass aa nnoorrmmaall uusseerr

  This section of the document by Hans, <lermen@fgan.de>. Last updated
  on June 16, 1997.

  1. You have to make 'dos' suid root, this normally is done when you
     run 'make install'.

  2. Have the users that are allow to execute dosemu in
     /etc/dosemu.user.  The format is:

            loginname [ c_strict ] [ classes ...] [ c_dexeonly ] [ other ]

  For details see ``Configuring DOSEmu''. For a first time _e_a_s_y instal-
  lation you may set it as follows (but don't forget to enhance it later
  !!!):

            root c_all
            all c_all

  3. The msdos partitions, that you want to be accessable through
     ``lredir'' should be mounted with proper permissions. I recommend
     doing this via 'group's, not via user ownership. Given you have a
     group 'dosemu' for this and want to give the user 'lermen' access,
     then the following should be

  +o  in /etc/passwd:

            lermen:x:500:100:Hans Lermen:/home/lermen:/bin/bash
                         ^^^-- note: this is NOT the group id of 'dosemu'

  +o  in /etc/group:

            users:x:100:
            dosemu:x:200:dosemu,lermen
                     ^^^

  +o  in /etc/fstab:

            /dev/hda1 /dosc msdos defaults,gid=200,umask=002 0 0
                                               ^^^

  _N_o_t_e_: the changes to /etc/passwd and /etc/group only take place the
  _n_e_x_t time you login, so don't forget to re-login.

  The fstab entry will mount /dosc such that is has the proper permis-
  sions

          ( drwxrwxr-x  22 root     dosemu      16384 Jan  1  1970 /dosc )

  You can do the same with an explicit mount command:

             mount -t msdos -o gid=200,umask=002 /dev/hda1 /dosc

  Of course normal lredir'ed unix directories should have the same per-
  missions.

  4. Make sure you have read/write permissions of the devices you
     configured (in /etc/dosemu.conf) for serial and mouse.

  Starting with dosemu-0.66.1.4 there should be no reason against
  running dosemu as a normal user. The privilege stuff has been
  extensively reworked, and there was no program that I tested under
  root, that didn't also run as user. However, if you suspect problems
  resulting from the fact that you run as user, you should first try
  configuring dosemu with the as (suid) root and only use user
  privileges when accessing secure relevant resources. Normally dosemu
  will permanently run as user and only temporarily use root privilege
  when needed.

  88..  UUssiinngg CCDDRROOMMSS

  88..11..  TThhee bbuuiilltt iinn ddrriivveerr

  This documents the cdrom extension rucker@astro.uni-bonn.de has
  written for Dosemu.

  The driver consists of a server on the Linux side
  (dosemu/drivers/cdrom.c, accessed via int 0xe6 handle 0x40) and a
  device driver (dosemu/commands/cdrom.S) on the DOS side.

  Please send any suggestions and bug reports to <rucker@astro.uni-
  bonn.de>

  To install it:

  +o  Create a (symbolic) link /dev/cdrom to the device file of your
     drive or use the cdrom statement in /etc/dosemu.conf to define it.

  +o  Make sure that you have read/write access to the device file of
     your drive, otherwise you won't be able to use the cdrom under
     Dosemu directly because of security reasons.

  +o  Load cdrom.sys within your config.sys file with e.g.:

           devicehigh=c:\emu\cdrom.sys

  +o  Start Microsoft cdrom extension as follows:

           mscdex /d:mscd0001 /l:driveletter

  To change the cd while Dosemu is running, use the DOS program
  'eject.com'.  It is not possible to change the disk, when the drive
  has been opened by another process (e.g. mounted)!

  Remarks by zimmerma@rz.fht-esslingen.de: This driver has been
  successfully tested with Linux' SCSI CDROMS by the author, with the
  Mitsumi driver mcd.c and with the Aztech/Orchid/Okano/Wearnes- CDROM
  driver aztcd.c by me. With the latter CDROM-drives changing the CD-ROM
  is not recognized correctly by the drive under all circumstances and
  is therefore disabled. So eject.com will not work.  For other CD-ROM
  drives you may enable this feature by setting the variable top of the
  file). With the mcd.c and aztcd.c Linux drivers this may cause your
  system to hang for some 30 seconds (or even indefinitely), so don't
  change the default value 'eject_allowed = 0'.

  History: Release with dosemu.0.60.0 Karsten Rucker (rucker@astro.uni-
  bonn.de) April 1995

  Additional remarks for mcd.c and aztcd.c Werner Zimmermann
  (zimmerma@rz.fht-esslingen.de) May 30, 1995

  99..  LLooggggiinngg ffrroomm DDOOSSEEmmuu

  This section was written by Erik Mouw <J.A.K.Mouw@et.tudelft.nl> on
  February 17, 1996 and last edited on 21 June 1997.

  This section documents the log facilities of DOSEMU.

  DOSEMU is able to log all use with email or syslogd(8).  If you want
  to enable logging, create a file /etc/dosemu.loglevel.  In this file
  you can set the loglevel for both mail and syslog with a very simple
  format. Only four keywords are recognized:

     mmaaiill__eerrrroorr
        mail errors about mis-usage to root

     mmaaiill__aallwwaayyss
        mail all DOSEMU usage to root

     ssyysslloogg__eerrrroorr
        log all errors about mis-usage with syslogd(8)

     ssyysslloogg__aallwwaayyss
        log all DOSEMU usage with syslogd(8)

  A line starting with a '#' is a comment.

  Some samples:

       #
       # mail everything to root, but don't log
       #
       mail_always

       #
       # mail errors to root, log everything
       # (this is the recommended usage)
       #
       mail_error
       syslog_always

       #
       # log errors only
       #
       syslog_errors

  1100..  UUssiinngg XX

  Please read all of this for a more complete X information ;-)

  1100..11..  LLaatteesstt IInnffoo

  From Uwe Bonnes <bon@elektron.ikp.physik.th-darmstadt.de>:

  xdos from dosemu version pre-0.60.4.2 with the patch from my last
  messages should now be more capable.  In particular it should now
  understand the keys from the keypad-area (the keys the most right on a
  MF-Keyboard) and numlock and keyevents in the range of the latin
  characters, even when you run xdos from a remote X-terminal.

  If it dosen't work for you as expected, please check out the
  following:

  +o  Don't specify "keycode" for X-support.  The keycode-option is
     something specific for a special X-server, here XFree86, and so
     isn't portable and I disencourage its use.

  +o  Check out whether your key sends a sensible ksym, with e.g."xev".
     If it sends something sensible which you think should be mapped by
     xdos, please let me know details.

  +o  If you are running xdos on XFree86, version 3.1.1 and newer and the
     keys of the keypad-area don't work, you have two options:

  +o  Comment out the line "    ServerNumLock " in
     /usr/X11R6/lib/X11/XFConfig

  +o  Feed the following file to xmodmap. From what I understand from the
     docs, XFree-3.1.1 should do that intrinsicly, but for me it didn't.
     This is a part of the file /usr/X11R6/lib/X11/etc/xmodmap.std

       ! When using ServerNumLock in your XF86Config, the following codes/symbols
       ! are available in place of 79-81, 83-85, 87-91
       keycode  136 = KP_7
       keycode  137 = KP_8
       keycode  138 = KP_9
       keycode  139 = KP_4
       keycode  140 = KP_5
       keycode  141 = KP_6
       keycode  142 = KP_1
       keycode  143 = KP_2
       keycode  144 = KP_3
       keycode  145 = KP_0
       keycode  146 = KP_Decimal
       keycode  147 = Home
       keycode  148 = Up
       keycode  149 = Prior
       keycode  150 = Left
       keycode  151 = Begin
       keycode  152 = Right
       keycode  153 = End
       keycode  154 = Down
       keycode  155 = Next
       keycode  156 = Insert
       keycode  157 = Delete

  1100..22..  SSlliigghhttllyy oollddeerr iinnffoorrmmaattiioonn

  From Rainer Zimmermann <zimmerm@mathematik.uni-marburg.de>

  Some basic information about dosemu's X support.  Sometimes it's even
  sort of useable now.

  What you should take care of:

  +o  Do a 'xmodmap -e "keycode 22 = 0xff08"' to get use of your
     backspace key.

  +o  Do a 'xmodmap -e "keycode 107 = 0xffff"' to get use of your delete
     key.

  +o  Make sure dosemu has X support compiled in.  (X_SUPPORT = 1 in the
     Makefile)

  +o  you should have the vga font installed. See README.ncurses.

  +o  start dosemu with 'dos -X' from an X terminal window.  Or make a
     link to 'dos' named 'xdos' - when called by that name, dosemu will
     automatically assume -X.  There is also a new debug flag 'X' for X-
     related messages.  To exit xdos, use 'exitemu' or select 'Close'
     aka 'Delete' (better not 'Destroy') from the 'Window' menu.

  +o  there are some X-related configuration options for dosemu.conf.
     See examples/config.dist for details.

  +o  starting xdos in the background (like from a window manager menu)
     appears not to work for some reason.

  +o  Keyboard support in the dosemu window isn't perfect yet. It
     probably could be faster, some key combos still don't work (e.g.
     Ctrl-Fn), etc.  However, input through the terminal window (i.e.
     the window you started dosemu from) is still supported. If you have
     problems, you *might* be better off putting your focus in there.

  +o  Keyboard support of course depends on your X keyboard mappings
     (xmodmap).  If certain keys don't work (like Pause, Backspace,...),
     it *might* be because you haven't defined them in your xmodmap, or
     defined to something other than dosemu expects.

  +o  using the provided icon (dosemu.xpm):

  +o  you need the xpm (pixmaps) package. If you're not sure, look for a
     file like /lib/libXpm.so.*

  +o  you also need a window manager which supports pixmaps. Fvwm is
     fine, but I can't tell you about others. Twm probaby won't do.

  +o  copy dosemu.xpm to where you usually keep your pixmap (not bitmap!)
     files (perhaps /usr/include/X11/pixmaps)

  +o  tell your window manager to use it. For fvwm, add the following
     line to your fvwmrc file:

       Icon "xdos"   dosemu.xpm

  This assumes you have defined a PixmapPath. Otherwise, specify the
  entire pathname.

  +o  note that if you set a different icon name than "xdos" in your
     dosemu.conf, you will also have to change the fvwmrc entry.

  +o  restart the window manager. There's usually an option in the root
     menu to do so.

     Now you should see the icon whenever you iconify xdos.

     Note that the xdos program itself does not include the icon -
     that's why you have to tell the window manager. I chose to do it
     this way to avoid xdos _r_e_q_u_i_r_i_n_g the Xpm library.

  +o  If anything else does not work as expected, don't panic :-)
     Remember the thing is still under construction.  However, if you
     think it's a real bug, please tell me.

  important changes to previous version (pre53_17):

  +o  fixed focus handling at startup

  +o  support for 21/28/43/50 line modes!  (43+50 look a bit funny,
     though... I use the 8x16 font for all modes)

  +o

  +o  fixed startup error handling (won't hang now if display not found)

  +o  limited window size

  1100..33..  SSttaattuuss ooff XX ssuuppppoorrtt ((SSeepptt 55,, 11999944))

  1100..33..11..  DDoonnee

  +o  X_update_screen    (video output)

  +o  implement cursor

  +o  fix cursor/scrolling bugs

  +o  fix Scroll (video/terminal.c) (?)

  +o  fix banner message (initialization) (works after video cleanup,
     dunno why :)

  +o  check video memory dirty bit

  +o  X event handling   (close, expose, focus etc.)

  +o  fixed cursor initialization

  +o  cleaned up cursor handling

  +o  added 'xdos' calling method

  +o  disable 'mouse' serial ports in X mode

  +o  write direct scroll routine (not used yet, though)

  +o  care about int10 calls  -ok?

  +o  Handle close ("delete") window event - (copied from xloadimage)

  +o  X keyboard support (pcemu code, heavily modified)

  +o  Mouse support

  +o  X configuration (display, updatefreq, updatelines,... what else?)

  +o  int10 video mode switches (resize window)

  +o  Window SizeHints  (fixed size or max size?)

  +o  create icon :-)

  1100..33..22..  TTooDDoo ((iinn nnoo ssppeecciiaall oorrddeerr))

  +o  xor cursor? blinking cursor?

  +o  use mark's scroll detector

  +o  jump scroll?

  +o  fine-tune X_update_screen

  +o  graphics support?

  +o  allow non-standard font heights via bios

  +o  cut & paste

  1100..44..  TThhee aappppeeaarraannccee ooff GGrraapphhiiccss mmooddeess ((NNoovveemmbbeerr 1133,, 11999955))

  Erik Mouw <J.A.K.Mouw@et.tudelft.nl> & Arjan Filius
  <I.A.Filius@et.tudelft.nl>

  We've made some major changes in X.c that enables X to run graphics
  modes.  Unfortunately, this disables the cut-and-paste support, but we
  think the graphics stuff is much more fun (after things have
  established, we'll put the cut-and-paste stuff back). The graphics is
  done through vgaemu, the VGA emulator. Status of the work:

  1100..44..11..  vvggaaeemmuu

  +o  Video memory. 1 Mb is allocated. It is mapped with mmap() in the
     VGA memory region of dosemu (0xa00000-0xbfffff) to support bank
     switching.  This is very i386-Linux specific, don't be surprised if
     it doesn't work under NetBSD or another Linux flavour
     (Alpha/Sparc/MIPS/etc).
  +o  The DAC (Digital to Analog Converter). The DAC is completely
     emulated, except for the pelmask. This is not difficult to
     implement, but it is terribly slow because a change in the pelmask
     requires a complete redraw of the screen. Fortunately, the pelmask
     changes aren't used often so nobody will notice ;-)

  +o  The attribute controller is partially emulated. (Actually, only
     reads and writes to the ports are emulated)

  +o  The working modes are 0x13 (320x200x256) and some other 256 color
     modes.

  +o  To do (in no particular order): font support in graphics modes
     (8x8, 8x16, 9x16, etc), text mode support, 16, 4 and 2 color
     support, better bank switching, write the X code out of vgaemu to
     get it more generic.

  1100..44..22..  vveessaa

  +o  VESA set/get mode, get information and bankswitch functions work.

  +o  All VESA 256 color (640x480, 800x600, 1024x768) modes work, but due
     to bad bank switch code in vgaemu they won't display right.

  +o  A VESA compatible video BIOS is mapped at 0xc00000. It's very
     small, but in future it's a good place to store the BIOS fonts
     (8x8, 8x16) in.

  +o  To do: implement the other VESA functions.

  1100..44..33..  XX

  +o  Added own colormap support for the 256 color modes.

  +o  Support for vgaemu.

  +o  Some cleanups.

  +o  To do: remove text support and let vgaemu do the text modes, put
     back the cut-and-paste stuff, more cleanups.

  +o  _N_O_T_E_: we've developed on X servers with 8 bit pixel depths
     (XF86_SVGA) so we don't know how our code behaves on other pixel
     depths. We don't even know if it works.

  As stated before, this code was written for Linux (tested with 1.2.13
  and 1.3.39) and we don't know if it works under NetBSD. The mmap() of
  /proc/self/mem and mprotect() magic in vgaemu are very (i386) Linux
  specific.

  Erik

  1100..55..  TThhee nneeww VVGGAAEEmmuu//XX ccooddee ((JJuullyy 1111,, 11999977))

  Steffen Winterfeldt <Steffen.Winterfeldt@itp.uni-leipzig.de>

  I've been working on the X code and the VGA emulation over the last
  few months. This is the outcome so far:

  +o  graphics support in X now works on all X servers with color depth
     >= 8

  +o  the graphics window is resizeable

  +o  support for hi- and true-color modes (using Trident SVGA mode
     numbers and bank switching)

  +o  some basic support for mode-X type graphics modes (non-chain4 modes
     as used by e.g. DOOM)

  +o  some even more basic support for 16 color modes

  +o  nearly full VESA 2.0 support

  +o  gamma correction for graphics modes

  +o  video memory size is configurable via dosemu.conf

  +o  initial graphics window size is configurable

  The current implementation supports 4 and 8 bit SVGA modes on all
  types of X display. Hi-color modes are supported only on displays
  matching the exact color depth (15 or 16); true color modes are
  supported only on true color X displays, but always both 24 bit and 32
  bit SVGA modes.

  In addition, the current hi- and true color support does not allow
  resizing of the graphics window and gamma correction is ignored.

  As the typical graphics mode with 320x200x8 will be used often with
  large scalings and modern graphics boards are pretty fast, I added
  something to eat up your CPU time: you can turn on the bilinear
  interpolation. It greatly improves the display quality (but is rather
  slow as I haven't had time yet to implement an optimized version -
  it's plain C for now).  If the bilinear filter is too slow, you might
  instead try the linear filter which interpolates only horizontally.

  Note that (bi)linear filtering is not available on all VGA/X display
  combinations. The standard drawing routines are used instead in such
  cases.

  If a VGA mode is not supported on your current X display, the graphics
  screen will just remain black. Note that this ddooeess nnoott mean that xdos
  has crashed.

  The VESA support is (or should be) nearly VBE 2.0 compatible. As a
  reference I used several documents including the unofficial VBE 2.0
  specs made available by SciTech Software. I checked this against some
  actual implementations of the VBE 2.0 standard, including SciTech's
  Display Doctor (formerly known as UniVBE).  Unfortunately
  implementations and specs disagree at some points.  In such cases I
  assumed the actual implementation to be correct.

  The only unsupported VBE function is VGA state save/restore. But this
  functionality is rarely used and its lack should not cause too much
  problems.

  VBE allows you to use the horizontal and vertical scrolling function
  even in text modes. This feature is not implemented.

  If you think it causes problems, the linear frame buffer (LFB) can be
  turned of via dosemu.conf as well as the protected mode interface.
  Note, however, that LFB modes are faster than banked modes, even in
  DOSEmu.

  The default VBE mode list defines a lot of medium resolution modes
  suitable for games (like Duke3D). You can still create your own modes
  via dosemu.conf. Note that you cannot define the same mode twice; the
  second (and all subsequent) definitions will be ignored.

  Modes that are defined but cannot be supported due to lack of video
  memory or because they cannot be displayed on your X display, are
  marked as unsupported in the VBE mode list (but are still in it).
  Note that there is currently no support of 4 bit VESA modes.

  The current interface between VGAEmu and X will try to update all
  invalid video pages at a time. This may, particularly in hi-res
  VBE/SVGA modes, considerably disturb DOSEmu's signal handling. That
  cannot be helped for the moment, but will be addressed soon (by
  running an extra update thread).

  If you really think that this is the cause of your problem, you might
  try to play with veut.max_max_len in env/video/n_X.c, near line 2005.
  This variable limits the amount of video memory that is updated during
  one timer interrupt. This way you can dramatically reduce the load of
  screen updates, but at the same rate reduce your display quality.

  Gamma correction works in both 4 and 8 bit modes. As the dosemu.conf
  parser doesn't support float values, it must be specified as a
  percentage value: gamma 100 = gamma 1.0. Higher values give brighter
  graphics, lower make them darker. Reasonable values are within a range
  of 50 ... 200.

  You can specify the video memory size that the VGA emulator should use
  in dosemu.conf. The value will be rounded up to the nearest 256 kbyte
  boundary. You should stick to typical values like 1024, 2048 or 4096
  as not to confuse DOS applications.  Note that whatever value you
  give, 4 bit modes are only supported up to a size of 800x600.

  You can influence the initial size of the graphics window in various
  ways. Normally it will have the same size (in pixel) as the VGA
  graphics mode, except for mode 0x13 (320x200, 256 colors), which will
  be scaled by the value of _m_o_d_e_1_3_f_a_c_t (defaults to 2).  Alternatively,
  you can directly specify a window size in dosemu.conf via _w_i_n_s_i_z_e. You
  can still resize the window later.

  The config option _f_i_x_e_d___a_s_p_e_c_t allows you to fix the aspect ratio of
  the graphics window while resizing it. Alternatively, _a_s_p_e_c_t___4_3 ties
  the aspect ratio to a value of 4:3. The idea behind this is that,
  whatever the actual resolution of a graphics mode is in DOS, it is
  displayed on a 4:3 monitor. This way you won't run into problems with
  modes such as 640x200 (or even 320x200) which would look somewhat
  distorted otherwise.

  1111..  RRuunnnniinngg WWiinnddoowwss uunnddeerr DDOOSSEEmmuu

  Okay, perhaps you've heard the hooplah.  DOSEMU can run Windows (sort
  of.)

  1111..11..  WWiinnddoowwss 33..00 RReeaall MMooddee

  DOSEMU has been able to run Windows 3.0 in Real Mode for some time
  now.  If you really, really, really want to run Windows under DOSEMU,
  this is the route to take for the moment.

  1111..22..  WWiinnddoowwss 33..11 PPrrootteecctteedd MMooddee

       ***************************************************************
       *    WARNING!!! WARNING!!! WARNING!!! WARNING!!! WARNING!!!   *
       *                                                             *
       *  Danger Will Robinson!!!  This is not yet fully supported   *
       *  and there are many known bugs!  Large programs will almost *
       *  certainly NOT WORK!!!  BE PREPARED FOR SYSTEM CRASHES IF   *
       *  YOU TRY THIS!!!                                            *
       *                                                             *
       *    WARNING!!! WARNING!!! WARNING!!! WARNING!!! WARNING!!!   *
       ***************************************************************

  What, you're still reading?

  Okay, it is possible to boot WINOS2 (the modified version of Windows
  3.1 that OS/2 uses) under DOSEMU.  Many kudos to Lutz & Dong!

  However, YOU NEED BBOOTTHH LICENSES, for WINDOWS-3.1 as well OS/2 !!!

  There are many known problems.  Windows is prone to crash, could take
  data with it, large programs will not load, etc. etc. etc.  In other
  words, it is NOT ready for daily use.  Many video cards are known to
  have problems (you may see a nice white screen, however, look below
  for win31-in-xdos).  Your program groups are all likely to disappear.
  Basically, it's a pain.

  On the other hand, if you're dying to see the little Windows screen
  running under Linux and you have read this CAREFULLY and PROMISE NOT
  TO BOMBARD THE DOSEMU DEVELOPERS WITH "MS Word 6.0 doesn't run!!!"
  MESSAGES

  +o  Get DOSEMU & the Linux source distributions.

  +o  Unpack DOSEMU.

  +o  Configure DOSEMU typing './configure' and do _not_ disable
     vm86plus.

  +o  Compile DOSEMU typing 'make'.

  +o  Get the OS2WIN31.ZIP distribution from  ????  oh well, and now you
     have the first problem.  It _was_ on ibm.com sometime ago, but has
     vanished from that site, and as long as it was there, we could
     mirror it. ,,,, you see the problem?  However, use 'archie' to find
     it, it will be around somewhere on the net ,,, for some time ;-)

  +o  Unpack the OS2WIN31 files into your WINDOWSTEM directory.  (Infact
     you only need WINDOWS/SYSTEM/os2k386.exe and the mouse driver)

  +o  Startup dosemu (make certain that DPMI is set to a value such as
     4096)

  +o  Copy the file winemu.bat to your c: drive.

  +o  Cross your fingers.

  Good luck!

  RREEMMEEMMBBEERR::  TTHHIISS IISS NNOOTT AATT AALLLL RREECCOOMMMMEENNDDEEDD!!!!!!  TTHHIISS IISS NNOOTT
  RREECCOOMMMMEENNDDEEDD!!!!!!  WWEE DDOO NNOOTT RREECCOOMMMMEENNDD YYOOUU TTRRYY TTHHIISS!!!!!!

  1111..33..  WWiinnddoowwss 33..xx iinn xxddooss

  As of version 0.64.3 DOSEMU is able to run Windows in xdos. Of course,
  this is not recommended at all, but if you really want to try, it is
  safer then starting windows-31 on the console, because _w_h_e_n it
  crashes, it doesn't block your keyboard or freeze your screen.

  Hints:

  +o  Get Dosemu & Linux source.

  +o  Unpack dosemu.

  +o  Run "./configure" to configure Dosemu (it will enable vm86plus as a
     default).

  +o  Type "make" to compile.

  +o  Get a Trident SVGA drivers for Windows. The files are tvgaw31a.zip
     and/or tvgaw31b.zip. They are available at garbo.uwasa.fi in
     /windows/drivers (any mirrors?).

  +o  Unpack the Trident drivers.

  +o  In Windows setup, install the Trident "800x600 256 color for 512K
     boards" driver.

  +o  Do the things described above to get and install OS2WIN31.

  +o  Start xdos.

  +o  In Dosemu, go to windows directory and start winemu.

  +o  Cross your fingers.

  Notes for the mouse under win31-in-xdos:

  +o  In order to let the mouse properly work you need the following in
     your win.ini file:
            [windows]
            MouseThreshold1=0
            MouseThreshold2=0
            MouseSpeed=0

  +o  The mouse cursor gets not painted by X, but by windows itself, so
     it depends on the refresh rate how often it gets updated, though
     the mouse coordinates movement itself will not get delayed.  ( In
     fact you have 2 cursors, but the X-cursor is given an 'invisible'
     cursor shape while within the DOS-Box. )

  +o  Because the coordinates passed to windows are interpreted
     relatively, we need to calibrate the cursor. This is done
     automatically whenever you enter the DOS-Box window: The cursor
     gets forced to 0,0 and then back to its right coordinates. Hence,
     if you want to re-calibrate the cursor, just move the cursor
     outside and then inside the DOS-Box again.

  1122..  MMoouussee GGaarrrroott

  This section, and Mouse Garrot were written by Ed Sirett
  <ed@cityscape.co.uk> on 30 Jan 1995.

  Mouse Garrot is an extension to the Linux Dos Emulator that tries to
  help the emulator give up CPU time to the rest of the Linux system.

  It tries to detect the fact the the mouse is not doing anything and
  thus to release up CPU time, as is often the case when a mouse-aware
  program is standing idle, such programs are often executing a loop
  which is continually checking the mouse buttons & position.

  Mouse Garrot is built directly into dosemu _i_f _a_n_d _o_n_l_y _i_f you are
  using the internal mouse driver of dosemu. All you have to do is make
  sure that the HOGTHRESHOLD value in the config file is non-zero.

  If you are loading a (DOS based) mouse driver when you boot then you
  will also need to load MGARROT.COM It is essential that you load
  MGARROT.COM _a_f_t_e_r you load the mouse driver. MGARROT will tell you if
  you are (a) running real DOS not dosemu, or (b) have requested a zero
  value for the HOGTHRESHOLD in the config file. In either case MGARROT
  fails to install and no memory will be lost. When MGARROT is installed
  it costs about 0.4k.  MGARROT may loaded `high' if UMBs are available.

  Mouse Garrot will work with many (maybe most) mouse-aware programs
  including Works, Dosshell and the Norton Commander.  Unfortunately,
  there are some programs which get the mouse driver to call the program
  whenever the mouse moves or a button is pressed. Programs that work in
  this way are immune to the efforts of Mouse Garrot.

  1133..  RRuunnnniinngg aa DDOOSS--aapppplliiccaattiioonn ddiirreeccttllyy ffrroomm UUnniixx sshheellll

  This part of the document was written by Hans <lermen@fgan.de>.

  1133..11..  UUssiinngg tthhee kkeeyyssttrrookkee aanndd ccoommmmaannddlliinnee ooppttiioonnss..

  Make use of the keystroke configure option and the -I commandline
  option of DOSEMU (>=dosemu-0.66.2) such as

       dos -D-a -I 'keystroke "dir > C:\\garbage\rexitemu\r"'

  The "..." will be 'typed in' by dosemu exactly as if you had them
  typed at the keyboard. The advantage of this technique is, that all
  DOS applications will accept them, even interactive ones. A '' is
  interpreted as in C and leads in ESC-codes. Here a list of of the
  current implemented ones:

       \r     Carriage return == <ENTER>
       \n     LF
       \t     tab
       \b     backspace
       \f     formfeed
       \a     bell
       \v     vertical tab

       \^x    <Ctrl>x, where X is one of the usual C,M,L,[ ...
              (e.g.: \^[ == <Ctrl>[ == ESC )

       \Ax    <Alt>x, hence  \Ad means <Alt>d

       \Fn;   Function key Fn. Note that the trailing ';' is needed.
              (e.g.:  \F10;  == F10 )

       \Pn;   Set the virtual typematic rate, thats the speed for
              autotyping in. It is given in unix timer ticks to wait
              between two strokes. A value of 7 for example leads to
              a rate of 100/7=14 cps.

       \pn;   Before typing the next stroke wait n unix ticks.
              This is usefull, when the DOS-application fushes the
              keybord buffer on startup. Your strokes would be discared,
              if you don't wait.

  When using X, the keystroke feature can be used to directly fire up a
  DOS application with one click, if you have the right entry in your
  .fvwmrc

  1133..22..  UUssiinngg aann iinnppuutt ffiillee

  +o  Make a file "FILE" containing all keystrokes you need to boot
     dosemu and to start your dos-application, ... and don't forget to
     have CRLF for 'ENTER'. FILE may look like this (as on my machine):

       2^M                    <== this chooses point 2 of the boot menu
       dir > C:\garbage^M     <== this executes 'dir', result to 'garbage'
       exitemu^M              <== this terminates dosemu

  (the ^M stands for CR)

  +o  execute dosemu on a spare (not used) console, maybe /dev/tty20 such
     like this:

          # dos -D-a 2>/dev/null <FILE >/dev/tty20

  This will _not_ switch to /dev/tty20, but silently execute dosemu and
  you will get the '#' prompt back, when dosemu returns.

  I tested this with dosemu-0.64.4/Linux-2.0.28 and it works fine.

  When your dos-app does only normal printout (text), then you may even
  do this

          # dos -D-a 2>/dev/null <FILE >FILE.out

  FILE.out then contains the output from the dos-app, but merged with
  ESC-sequences from Slang.

  You may elaborate this technique by writing a script, which gets the
  dos-command to execute from the commandline and generate 'FILE' for
  you.

  When you try to use one of the above to start dosemu out of a crontab,
  then you have to asure, that the process has a proper environement set
  up ( especially the TERM and/or TERMCAP variable ).

  1144..  SSeettttiinngg HHooggTThhrreesshhoolldd

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

  1155..  CCoommmmaannddss && UUttiilliittiieess

  These are some utitlies to assist you in using Dosemu.

  1155..11..  PPrrooggrraammss

     bboooottooffff..ccoomm
        switch off the bootfile to access disk see examples/config.dist
        at bootdisk option

     bboooottoonn..ccoomm
        switch on the bootfile to access bootfile see
        examples/config.dist at bootdisk option

     cchhddiirr..ccoomm
        change the Unix directory for Dosemu (use chdir(2))

     ddoossddbbgg..ccoomm
        change the debug setting from within Dosemu

     +o  dosdbg -- show current state

     +o  dosdbg <string>

     +o  dosdbg help -- show usage information

     dduummppccoonnff..eexxee
        ???

     eejjeecctt..ccoomm
        eject CD-ROM from drive

     eemmuummoouussee..ccoomm
        fine tune internal mousedriver of Dosemu

     +o  emumouse h -- display help screen

     eexxiitteemmuu..ccoomm
        terminate Dosemu

     ggeettccwwdd..ccoomm
        get the Unix directory for Dosemu (use getcwd(2))

     iisseemmuu..ccoomm
        detects Dosemu version and returns greater 0 if running under
        Dosemu

     llaanncchheecckk..eexxee
        ???

     llrreeddiirr..ccoomm
        redirect Linux directory to Dosemu

     +o  lredir -- show current redirections

     +o  lredir D: LINUXFSmp -- redirects /tmp to drive D:

     +o  lredir help -- display help screen

     mmggaarrrroott..ccoomm
        give up cpu time when Dosemu is idle

     ssppeeeedd..ccoomm
        set cpu usage (HogThreshold) from inside Dosemu

     ssyysstteemm..ccoomm
        interface to system(2)...

     uunniixx..ccoomm
        execute Unix commands from Dosemu

     +o  unix -- display help screen

     +o  unix ls -al -- list current Linux directory

     +o  caution! try "unix" and read the help screen

     ccmmddlliinnee..eexxee
        Read /proc/self/cmdline and put strings such as "var=xxx" found
        on the commandline into the DOS enviroment.  Example having this
        as the dos commandine:

                           dos "autoexec=echo This is a test..."

     then doing

                           C:\cmdline < D:\proc\self\cmdline
                           %autoexec%

     would display "This is a test..." on the DOS-Terminal

     vvggaaooffff..ccoomm
        disable vga option

     vvggaaoonn..ccoomm
        enable vga option

  1155..22..  DDrriivveerrss

  These are useful drivers for Dosemu

     ccddrroomm..ssyyss
        allow direct access to CD-ROM drives from Dosemu

     eemmss..ssyyss
        enable EMM in Dosemu

     eemmuuffss..ssyyss
        redirect Unix directory to Dosemu

  1166..  KKeeyymmaappss

  This keymap is for using dosemu over telnet, and having *all* your
  keys work.  This keymap is not complete.  But hopefully with everyones
  help it will be someday :)

  There are a couple of things that are intentionally broken with this
  keymap, most noteably F11 and F12.  This is because they are not
  working though slang correctly at the current instant.  I have them
  mapped to "greyplus" and "greyminus".  Also the scroll lock is mapped
  to shift-f3.  This is because the scroll lock dosn't work right at
  all.  Please feel free to send keymap patches in that fix anything but
  these.

  If you want to patch _d_o_s_e_m_u to fix either of those problems, i'd be
  _g_l_a_d to accept those :)

  to figure out how to edit this, read the keystroke-howto.

  as of 3/30/95, control/shift/alternate
  home/insert/delete/end/pageup/pagedown should work.

  Major issues will be:

  Do we move "alt-<fkey>" to "control-<fkey>" to switch virtual
  consoles?

  who is going to fix the linux keyboard device to be able to handle
  multiple keymaps at the same time?

  --------------------------------------------------------

  to use it:

  as root type

          loadkeys dosemu.new.keymap

  (then run dosemu via telnet, or something in slang mode)

  when your done, find your old keymap, and load it back, cause control-
  home won't work in emacs anymore (or any other special key in any
  applicaion that uses xlate)

  if you find a key missing, please add it and send me the patch.  (test
  it first! :)

  if you find a key missing, and don't feel like coding it, _d_o_n_'_t _t_e_l_l
  _m_e!  I already know that there are a lot of keys missing.

  corey sweeney <corey@interaccess.com >

  Sytron Computers

  1177..  NNeettwwoorrkkiinngg uussiinngg DDOOSSEEmmuu

  A mini-HOWTO from Bart Hartgers <barth@stack.nl> ( for the detailed
  original description see below )

  1177..11..  TThhee DDOOSSNNEETT vviirrttuuaall ddeevviiccee..

  Dosnet.o is a kernel module that implements a special virtual network
  device. In combination with pktdrv.c.multi and libpacket.c.multi, this
  will enable multiple dosemu sessions and the linux kernel to be on a
  virtual network. Each has it's own network device and ethernet
  address.

  This means that you can telnet or ftp from the dos-session to your
  telnetd/ftpd running in linux and, with IP forwarding enabled in the
  kernel, connect to any host on your network.

  1177..22..  SSeettuupp ffoorr vviirrttuuaall TTCCPP//IIPP

  First replace ./src/dosext/net/net/libpacket.c with libpacket.c.multi
  and are also in ./src/dosext/net/net). Now (re)build dosemu.

  Go to ./src/dosext/net/v-net and make dosnet.o. As root, insmod
  dosnet.o. Now as root, configure the dsn0 interface (for example:
  ifconfig dsn0 192.168.74.1 netmask 255.255.255.0), and add a route for
  it (for example: route add -net 192.168.74.0 netmask 255.255.255.0
  dsn0).

  Finally, start dosemu, and give your TCP/IP client and ip-address in
  the subnet you just configured. This address should be unique, i.e. no
  other dosemu, or the kernel, should have this address. For the example
  addresses given above, 192.168.74.2-192.168.74.254 would be good. Now
  you can use a dos telnet client to telnet to your own machine!

  1177..33..  FFuullll DDeettaaiillss

  Detailed original description of Vinod G Kulkarni
  <vinod@cse.iitb.ernet.in>

  Allowing a program to have its own network protocol stacks.

  Resulting in multiple dosemu's to use netware, ncsa telnet etc.

  1177..33..11..  IInnttrroodduuccttiioonn

  Allowing network access from dosemu is an important functionality.
  For pc based network product developers, it will offer an easy
  development environment will full control over all the traffic without
  having to run around and use  several machines.  It will allow already
  available client-server based "front-ends" to run on dosemulator.
  (Assuming that they are all packet driver based -- as of now ;-) )

  To accomplish that, we require independent protocol stacks to coexist
  along with linux' IP stack. One way is to add independent network
  card.  However, it is cumbersome and allows at most only 2-3 stacks.
  Other option is to use the a virtual network device that will route
  the packets to the actual stacks which run as user programs.

  1177..33..22..

  DDeessiiggnn

  Have a virtual device which provides  routing interface at one end (so
  it is a network device from linux side) and at other end, it
  sends/receives packets from/to user stacks.

  All the user stacks AND virtual device are virtually connected by a
  network (equavalent to a physical cable). Any broadcast packet (sent
  by either user stack or router interface of the virtual device) should
  be sent to all the user stacks and router.  All non-broadcast packets
  can be sent by communicating with each other.

  Each user stack (here dosemu process) will have an base interface
  which allows sending and receiving of packets. On the top of this, a
  proper interface (such as  packet driver interface) can be built.  In
  dosemu, a packet driver interface is emulated.

  Every user stack will have a unique  virtual ethernet address.

  1177..33..33..  IImmpplleemmeennttaattiioonn

  This package includes:

  1. dosnet module. Acts as virtual network device introducing It
     provides usual network interface AND also facility to communicate
     with dosemu's.

  2. Modified  packet driver code (pktnew.c and libdosemu.c) to enable
     the above. Modifications include these:

     a. Generate an unique ethernet address for each dosemu .  I have
        used minor no. of the tty in use as part of ethernet address.
        This works unless you start two dosemu's in same tty.

     b. Communication with dosnet device is done by opening a
        SOCK_PACKET socket of special type.
  3. IPX bridge code. Between eth0 and dsn0 so that multiple lan
     accesses can be made. 0.1 is non-intelligent.  (both versions are
     alpha codes.)  Actually IPX routing code is there in kernel.  Has
     anyone been successful in using this?  Yet another alternative is
     to use IPTunnelling of IPX packets (rfc 1234). Novell has NLMs for
     this on the netware side. On linux, we should run a daemon
     implementing this rfc.

  1177..33..44..  VViirrttuuaall ddeevviiccee ''ddssnn00''

  Compile the module dosnet and insmod it, and give it an IP address,
  with a new IP network number. And You have to set up proper routing
  tables on all machines you want to connect to.  So linux side
  interface is easy to set up.

  _N_o_t_e_: Some 3 kernel symbols are used by the module, which are not
  exported by kernel/ksyms.c in the kernel code. You could either add
  these symbols there, or use Hans' improved 'insmod'  (part of syscall
  Manager package). (In that case, it will resolve the symbols from the
  zSystem.map file.

  This device is assigned a virtual ethernet address, defined in
  dosnet.h.

  This device is usual loadable module. (Someone please check if it can
  be made more efficient.) However, what is interesting is the way it
  allows access to user stacks (i.e. dosemu's.) i.e.  its media
  interface.

  A packet arrives to dosnet from linux for our virtual internal network
  (after routing process). If it is broadcast packet, dosnet should send
  it to all dosemu's/user stacks.  If it is normal packet, it should
  send it only particular user stack which has same destination ethernet
  address .

  It performs this process by the following method, using SOCK_PACKET
  interface , (and not introducing new devices).:

  The dosemu opens a SOCK_PACKET interface for type 'x' with the dosnet
  device. The result of this will be an addition of entry into type
  handler table for type 'x'.  This table stores the type and
  corresponding handler function  (called when a packet of this type
  arrives.)

  Each dosemu will open the interface with unique 'x'  .

     sseennddiinngg ppaacckkeettss ffrroomm ddoosseemmuu ttoo ddoossnneett
        SOCK_PACKET  allows you to send the packet "as is".  So not a
        problem at all.

     ddoossnneett -->> ddoosseemmuu
        this is tricky. The packet is simply given by dosnet device to
        upper layers. However, the upper layer calls function to find
        out the type of the packet which is device specific (default is
        eth_type_trans().).  This routine, which returns type of given
        packet, is to be implemented in each device. So in dosnet, this
        plays a special role. If the packet is identified as type 'x',
        the upper layers (net/inet/dev.c) call the type handler for 'x'.

        Looking at destination ethernet address of a packet, we can say
        deduct that it is packet for dosemu, and its type is address.)
        Type handler function for 'x' is essentially SOCK_PACKET
        receiver function which sends packet back to dosemu.

        _N_O_T_E_: the "type" field is in destination ethernet address and
        not its usual place (which depends on frame type.) So the packet
        is left intact -- there is no wrapper function etc.  We should
        use type "x" which is unused by others; so the packet can carry
        _ANY_ protocol since the data field is left untouched.

     BBrrooaaddccaasstt ppaacckkeettss
        We use a common type "y" for dosnet broadcasts. Each dosemu
        "registers" for "y" along with usual "x" type packet using
        SOCK_PACKET. This "y" is same for all dosemu's.  (The packet is
        duplicated  if  more than one SOCK_PACKET asks for same type. )

  1177..33..55..  PPaacckkeett ddrriivveerr ccooddee

  I have add the code for handling multiple protocols.

  When a packet arrives,  it arrives on  one of the two SOCK_PACKET
  handle we need to find out which of the registered protocols  should
  be handled this code.  (Earlier code opened multiple sockets, one for
  each IPX type.  However it is not useful now because we use *any*
  type.) When a new type is registered, it is added to a Type list. When
  a new packet arrives, first we find out the frame type(and hence the
  position of type field in the packet, and then try matching it with
  registered types.  [ ----  I missed comparing class; I will add it
  later.]  Then call the helper corresponding to the handle of that
  type.

  Rob, you should help in the following:

  1. Packet driver code ...

     We should now open only two sockets: one specific to dosemu and
     other broadcast. So we have to add code to demultiplex into packet
     types... I couldn't succeed. Even broadcast packets are not getting
     to dosemu.

  2. Which virtual ethernet  addresses to use (officially)?

  3. Which  special packet type can be used?

  4. Kernel overhead .. lots of packet types getting introduced in type
     handler table... how to reduce?

  1177..33..66..  CCoonncclluussiioonn ....

  So at last one can open multiple DOSEMU's and access network from each
  of them ...  However, you HAVE TO set up ROUTING TABLES etc.

  Vinod G Kulkarni <vinod@cse.iitb.ernet.in>

               From macleajb@ednet.ns.ca Mon Oct 10 06:16:35 1994
               Return-Path: <macleajb@ednet.ns.ca>

               At any rate, I compiled dosnet.c with :
               cc -o dosnet.o  -D__KERNEL__ -DLINUX -O6 dosnet.c -c
               but insmod declares:
               _eth_type_trans undefined
               _eth_header undefined
               _eth_rebuild_header undefined

  [ Note from JES : If you wish, you may use Hans ../syscallmgr/insmod
  with the -m flag instead of patching the kernel as elluded below ]

  I forgot to mention this: The kernel sources need to be patched
  slightly. (This is happening with any new loadable module these days
  ;-) Here is what you need to do:  The only file that gets affected is
  kernel/ksyms.c in the linux sources:

       *** ksyms.c.old Mon Oct 10 11:12:01 1994
       --- ksyms.c     Mon Oct 10 11:13:31 1994
       ***************
       *** 28,33 ****
       --- 28,39 ----
         #include <linux/serial.h>
         #ifdef CONFIG_INET
         #include <linux/netdevice.h>
       + extern unsigned short eth_type_trans(struct sk_buff *skb, struct device *dev);
       + extern int eth_header(unsigned char *buff, struct device *dev, unsigned
       +               short type, void *daddr, void *saddr, unsigned len, struct
       +               sk_buff *skb);
       + extern int eth_rebuild_header(void *buff, struct device *dev, unsigned long
       +               dst, struct sk_buff *skb);
         #endif

         #include <asm/irq.h>
       ***************
       *** 183,188 ****
       --- 189,197 ----
               X(dev_rint),
               X(dev_tint),
               X(irq2dev_map),
       +         X(eth_type_trans),
       +         X(eth_header),
       +         X(eth_rebuild_header),
         #endif

               /********************************************************

  After this, recompile the kernel. You should compile with "IP FORWARD"
  config option enabled (for allowing routing).  With this kernel,
  'insmod dosnet' will work. After this step,

  After this it is admin stuff:

  I run these commands: 144.16.112.1 is "special" address. 'dsn0' is the
  new interface name.

       ifconfig dsn0 144.16.112.1 broadcast 144.16.112.255 netmask 255.255.255.0
       route add -net 144.16.112.0 dsn0

  Compile dosemu with  new pktnew.c and libpacket.c sources (to be put
  in net directory.) Start dosemu. You could start 'telbin 144.16.112.1'
  after assigning IP address (say 144.16.112.10) for the dosemu in
  CONFIG.TEL file. Each dosemu should get a new IP address.

  1177..33..66..11..  TTeellnneettttiinngg ttoo ootthheerr SSyysstteemmss

  Other systems need to have route to this "new" network. The easiest
  way to do this is to have  static route for dosnet IP network included
  in remote machine you want to connect to.  After all tests are carried
  out, one could include them permanently (i.e. in gated configurations
  etc.). However, the "new" IP address should only be internal to your
  organisation, and not allowed to route outside.  There is some rfc in
  this regard, I will let you know later.  For e.g., I am working on
  144.16.98.20. Internal network I created was 144.16.112.0. (See the
  above route command.)  To connect to another linux system 144.16.98.26
  from dosemu, I include static route by running 'route add -net
  144.16.112.0 gw 144.16.98.20' on that system. It becomes more complex
  if you need to connect to outside of 144.16.98.0.

  1177..33..66..22..  AAcccceessssiinngg NNoovveellll nneettwwaarree

  Since dosemu is now on "different device", IPX needs to be either
  bridged or routed. If it is bridged, then there is no requirement for
  any extra administration ; simply run 'ipxbridge' program supplied
  with the dosnet sources. (There are two versions of it; 0.1 copies all
  packets to from/to both interface. 0.2 is "intelligent bridge", it
  copies packet to other interface only if the destination lies on other
  interface. )

  If you instead want to use "routing" for IPX, then you need to enable
  IPX config option in the kernel.  Next, you should select a network
  number that won't clash with others. Set up static direct ipx routes
  in linux, and then in one Novell netware server which is directly
  connected (i.e. without any router inbetween.). (That is where you
  should contact Novell sysadm's ;-) The idea is, the server acts as
  route broadcaster. (I haven't actually tested this; and we are working
  on getting proper daemons etc. which will make linux act as IPX router
  proper.)

  (You could include this info along with other documentation...)

  Hope this helps,

  Vinod.

  I just realised one more thing: The ipxbridge-0.2 code assumes that
  you have 'eth0' and 'eth1' as the two interfaces. And it uses this
  fact while choosing the interface to put the packet. So it won't
  recognise when 'dsn0' is used.

  ipxbridge-0.1 will work though.

  Also, note that both these programs put the card in promiscuous mode.

  So my suggestion is to "somehow" get IPX routing done by linux!

  Vinod.

  1188..  UUssiinngg WWiinnddoowwss aanndd WWiinnssoocckk

  This is the Windows Net Howto by Frisoni Gloriano <gfrisoni@hi-net.it>
  on 15 may 1997

  This document tries to describe how to run the Windows trumpet winsock
  over the dosemu built-in packet driver, and then run all TCP/IP
  winsock-based application (netscape, eudora, mirc, free agent .....)
  in windows environment.

  This is a very long step-by-step list of operation, but you can make
  little scripts to do all very quickly ;-)

  In this example, I use the dosnet based packet driver. It is very
  powerful because you can run a "Virtual net"  between your dos-windows
  session and the linux, and run tcp/application application without a
  real (hardware) net.

  1188..11..  LLIISSTT OOFF RREEQQUUIIRREEDD SSOOFFTTWWAARREE

  +o  The WINPKT.COM virtual packet driver, version 11.2 I have found
     this little tsr in the Crynwr packet driver distribution file
     PKTD11.ZIP

  +o  The Trumpet Winsock 2.0 revision B winsock driver for windows.

  1188..22..  SSTTEEPP BBYY SSTTEEPP OOPPEERRAATTIIOONN ((LLIINNUUXX SSIIDDEE))

  +o  Enable "dosnet" based dosemu packet driver:

             cd ./src/dosext/net/net
             select_packet      (Ask  single or multi ->  m)

  +o  Make the dosnet linux module:

             cd ./src/dosext/net/v-net
             make

  +o  Make the new dosemu, with right packet driver support built-in:

             make
             make install

  +o  Now you must load the dosnet module:

             insmod ./src/dosext/net/v-net/dosnet.o

  +o  Some linux-side network setup (activate device, routing). This
     stuff depends from your environment, so I write an example setup.

     Here you configure a network interface dsn0 (the dosnet interface)
     with the ip address 144.16.112.1 and add a route to this interface.

     This is a good example to make a "virtul network" from your
     dos/windows environment and the linux environment.

             ifconfig dsn0 144.16.112.1 broadcast 144.16.112.255 netmask 255.255.255.0
             route add -net 144.16.112.0 dsn0

  1188..33..  SSTTEEPP BBYY SSTTEEPP OOPPEERRAATTIIOONN ((DDOOSS SSIIDDEE))

  I suppose you know how to run windows in dosemu. You can read the
  ``Running Windows'' document if you need more information. Windows is
  not very stable, but works.

  +o  start dosemu.

  +o  copy the winpkt.com driver and the trumpet winsock driver in some
     dos directory.

  +o  start the winpkt TSR. (dosemu assign the 0x60 interrupt vector to
     the built-in packet driver)

               winpkt 0x60

  +o  edit the trumpet winsock setup file trumpwsk.ini. Here is an
     example of how to setup this file: (I think you can use less
     parameters, if you have the time to play with this file. You can
     also setup this stuff from the winsock setup dialog-box).

          [Trumpet Winsock]
          netmask=255.255.255.0  <-- class C netmask.
          gateway=144.16.112.1   <-- address in the default gateway.
          dns=www.xxx.yyy.zzz    <-- You must use right value for the dns.
          domain=hi-net.it
          ip=144.16.112.10       <-- Windows address in the dosnet.
          vector=60              <-- packet driver interrupt vector.
          mtu=1500
          rwin=4096
          mss=1460
          rtomax=60
          ip-buffers=32
          slip-enabled=0         <--- disable slip
          slip-port=2
          slip-baudrate=57600
          slip-handshake=1
          slip-compressed=0
          dial-option=1
          online-check=0
          inactivity-timeout=5
          slip-timeout=0
          slip-redial=0
          dial-parity=0
          font=Courier,9
          registration-name=""
          registration-password=""
          use-socks=0
          socks-host=0.0.0.0
          socks-port=1080
          socks-id=
          socks-local1=0.0.0.0 0.0.0.0
          socks-local2=0.0.0.0 0.0.0.0
          socks-local3=0.0.0.0 0.0.0.0
          socks-local4=0.0.0.0 0.0.0.0
          ppp-enabled=0            <-------- disable ppp
          ppp-usepap=0
          ppp-username=""
          ppp-password=""
          win-posn=42 220 867 686 -1 -1 -4 -4 1
          trace-options=16392

          [default vars]

  +o  Now you can run windows, startup trumpet winsock and .....  enjoy
     with your windoze tcp/ip :-)

  Gloriano Frisoni.  <gfrisoni@hi-net.it>

