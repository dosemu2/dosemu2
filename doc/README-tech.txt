
DOSEMU Technical Guide

The DOSEMU team

Edited by

Alistair MacDonald

   For DOSEMU v1.0 pl2.0

   This document is the amalgamation of a series of technical README
   files which were created to deal with the lack of DOSEMU
   documentation.
     _________________________________________________________________

   Table of Contents
   1. Introduction
   2. Runtime Configuration Options

        2.1. Format of /etc/dosemu.users
        2.2. Format of global.conf ( (old) .dosrc, -I option)

              2.2.1. Enviroment variables and configuration variables
              2.2.2. Conditional statements
              2.2.3. Include files
              2.2.4. Macro substitution
              2.2.5. Expressions
              2.2.6. String expressions
              2.2.7. `Dry' testing your configuration
              2.2.8. Debug statement
              2.2.9. Miscellaneous
              2.2.10. Code page and character set
              2.2.11. Keyboard settings
              2.2.12. Serial stuff
              2.2.13. Networking Support
              2.2.14. Terminals
              2.2.15. X Support settings
              2.2.16. Video settings ( console only )
              2.2.17. Memory settings
              2.2.18. IRQ passing
              2.2.19. Port Access
              2.2.20. Speaker
              2.2.21. Hard disks
              2.2.22. DOSEMU boot
              2.2.23. Floppy disks
              2.2.24. Printers
              2.2.25. Sound

   3. Accessing ports with dosemu

        3.1. General
        3.2. Port I/O access

              3.2.1. System Integrity
              3.2.2. System Security
              3.2.3. The port server

   4. The Virtual Flags
   5. New Keyboard Code

        5.1. Whats New
        5.2. Status
        5.3. Known bugs & incompatibilites
        5.4. TODO

   6. Old Keyboard Code

        6.1. Whats New
        6.2. Status
        6.3. Keyboard server interface
        6.4. Keyboard server structure

              6.4.1. queue handling functions
              6.4.2. The Front End
              6.4.3. The Back End

        6.5. Known bugs & incompatibilites
        6.6. Changes from 0.61.10
        6.7. TODO

   7. Setting HogThreshold
   8. Privileges and Running as User

        8.1. What we were suffering from
        8.2. The new 'priv stuff'

   9. Timing issues in dosemu

        9.1. The 64-bit timers
        9.2. DOS 'view of time' and time stretching
        9.3. Non-periodic timer modes in PIT
        9.4. Fast timing
        9.5. PIC/PIT synchronization and interrupt delay
        9.6. The RTC emulation
        9.7. General warnings

   10. Pentium-specific issues in dosemu

        10.1. The pentium cycle counter
        10.2. How to compile for pentium
        10.3. Runtime calibration
        10.4. Timer precision
        10.5. Additional points

   11. The DANG system

        11.1. Description
        11.2. Changes from last compiler release
        11.3. Using DANG in your code
        11.4. DANG Markers

              11.4.1. DANG_BEGIN_MODULE / DANG_END_MODULE
              11.4.2. DANG_BEGIN_FUNCTION / DANG_END_FUNCTION
              11.4.3. DANG_BEGIN_REMARK / DANG_END_REMARK
              11.4.4. DANG_BEGIN_NEWIDEA / DANG_END_NEWIDEA
              11.4.5. DANG_FIXTHIS
              11.4.6. DANG_BEGIN_CHANGELOG / DANG_END_CHANGELOG

        11.5. Usage
        11.6. Future

   12. mkfatimage -- Make a FAT hdimage pre-loaded with files
   13. mkfatimage16 -- Make a large FAT hdimage pre-loaded with files
   14. Documenting DOSEMU

        14.1. Sections
        14.2. Emphasising text
        14.3. Lists
        14.4. Quoting stuff
        14.5. Special Characters
        14.6. Cross-References & URLs

              14.6.1. Cross-References
              14.6.2. URLs
              14.6.3. Email addresses

        14.7. References

   15. Sound Code

        15.1. Current DOSEMU sound code
        15.2. Original DOSEMU sound code

              15.2.1. Reference

   16. DMA Code

        16.1. Current DOSEMU DMA code
        16.2. Original DOSEMU DMA code

              16.2.1. Adding DMA devices to DOSEMU
              16.2.2. References

   17. DOSEMU Programmable Interrupt Controller

        17.1. Other features
        17.2. Caveats
        17.3. Notes on theory of operation:

              17.3.1. Functions supported from DOSEMU side
              17.3.2. Other Functions

        17.4. A (very) little technical information for the curious

   18. DOSEMU debugger v0.6

        18.1. Introduction
        18.2. Usage
        18.3. Commands
        18.4. Performance
        18.5. Wish List
        18.6. BUGS

              18.6.1. Known bugs

   19. MARK REJHON'S 16550 UART EMULATOR

        19.1. PROGRAMMING INFORMATION
        19.2. DEBUGGING HELP
        19.3. FOSSIL EMULATION
        19.4. COPYRIGHTS

   20. Recovering the console after a crash

        20.1. The mail message

   21. Net code
   22. Software X386 emulation

        22.1. The CPU emulator

   23. MFS and National Language Support

        23.1. MFS and National Language Support
        23.2. Patching of MFS
        23.3. TODO:

1. Introduction

   This documentation is derived from a number of smaller documents. This
   makes it easier for individuals to maintain the documentation relevant
   to their area of expertise. Previous attempts at documenting DOSEMU
   failed because the documentation on a large project like this quickly
   becomes too much for one person to handle.

   These are the technical READMEs. Many of these have traditionally been
   scattered around the source directories.
     _________________________________________________________________

2. Runtime Configuration Options

   This section of the document by Hans, <lermen@fgan.de>. Last updated
   on Mar 20, 1998.

   Most of DOSEMU configuration is done during runtime and per default it
   expects the system wide configuration file /etc/dosemu.conf optionally
   followed by the users ~/.dosemurc and additional configurations
   statements on the commandline (-I option). The builtin configuration
   file of a DEXE file is passed using the -I technique, hence the rules
   of -I apply.

   In fact /etc/dosemu.conf and ~/.dosemurc (which have identical syntax)
   are included by the systemwide configuration script
   /var/lib/dosemu/global.conf, but as a normal user you won't ever think
   on editing this, only dosemu.conf and your personal ~/.dosemurc.

   In DOSEMU prior to 0.97.5 the private configuration file was called
   ~/.dosrc (not to be confused with the new ~/.dosemurc). This will work
   as expected formerly, but is subject to be nolonger supported in the
   near future. This (old) ~/.dosrc is processed after global.conf and
   follows (same as -I) the syntax of global.conf.

   The first file expected (and interpreted) before any other
   configuration (such as global.conf, dosemu.conf and ~/.dosemurc) is
   /etc/dosemu.users. If /etc/dosemu.users doesn't exist, DOSEMU check
   for /etc/dosemu/dosemu.users, this makes people happy, which prefer to
   have to configuration stuff in a separate directory under /etc. Within
   dosemu.users the general permissions are set:

     * which users are allowed to have a private lib dir.
     * what kind of access class the user belongs to.
     * what special configuration stuff the users needs

   and further more:

     * whether the lib dir (/var/lib/dosemu) resides elsewhere.
     * setting the loglevel.

   This is done via setting configuration variables.

   After /etc/dosemu.users /etc/dosemu.conf (via global.conf) is
   interpreted, and only during global.conf parsing access to all
   configuration options is allowed. Your personal ~/.dosemurc is
   included directly after dosemu.conf, but has less access rights (in
   fact the lowest level), all variables you define within ~/.dosemurc
   transparently are prefixed with `dosemu_' such that the normal
   namespace cannot be polluted (and a hacker cannot overwrite security
   relevant enviroment variables). Within global.conf only those
   ~/.dosemurc created variables, that are needed are taken over and may
   overwrite those defined in /etc/dosemu.conf.

   The dosemu.conf (global.conf) may check for the configuration
   variables, that are set in /etc/dosemu.users and optionaly include
   further configuration files. But once /etc/dosemu.conf (global.conf)
   has finished interpretation, the access to secure relevant
   configurations is (class-wise) restricted while the following
   interpretation of (old) .dosrc and -I statements.

   For an example of a general configuration look at ./etc/global.conf.
   The later behaves insecure, when /etc/dosemu.users is a copy of
   ./etc/dosemu.users.easy and behave 'secure', when /etc/dosemu.users is
   a copy of ./etc/dosemu.users.secure.
     _________________________________________________________________

2.1. Format of /etc/dosemu.users

   Except for lines starting with x=' (explanation below), each line
   corresponds to exactly _one_ valid user count:

         loginname [ c_strict ] [ classes ...] [ c_dexeonly ] [ other ]

   where the elements are:

   loginname
          valid login name (root also is one) or 'all'. The later means
          any user not mentioned in previous lines.

   classes
          One or more of the following:

        c_all
                no restriction

        c_normal
                normal restrictions, all but the following classes:
                c_var, c_vport, c_irq, c_hardram, c_pci, c_net.

        c_var
                allow (un)setting of configuration- and environment
                variables

        c_vport
                allow setting of 'emuretrace'

        c_port
                allow `port' setting

        c_irq
                allow `irqpassing' statement

        c_hardram
                allow 'hardware_ram' settings

        c_pci
                allow 'PCI' settings

        c_net
                allow direct net and dosnet settings (TUN/TAP is always
                allowed).

   other
          Here you may define any configuration variable, that you want
          to test in global.conf (or (old) .dosrc, -I), see `ifdef',
          `ifndef' When this variable is intended to be unset in lower
          privilege configuration files (.dosrc, -I), then the variable
          name has to be prefixed with `u_'.

   private_setup
          Keyword, this makes dosemu to accept a private DOSEMU lib under
          $HOME/.dosemu/lib. If this directory is existing, DOSEMU will
          expect all normally under /var/lib/dosemu within that
          directory,including `global.conf'. As this would be a security
          risc, it only will be allowed, if the used DOSEMU binary is
          non-suid-root. If you realy trust a user you may additionally
          give the keyword `unrestricted', which will allow this user to
          execute a suid-root binary even on a private lib directory
          (though, be aware).

   unrestricted
          Keyword, used to allow `private_setup' on suid-root binaries
          too. USE WITH CARE !

   A line with '#' at colum 1 is treated as comment line. When only the
   login name is given (no further parameters, old format) the following
   setting is assumed:

      if 'root'  c_all
      else       c_normal

   In addition, dosemu.users can be used to define some global settings,
   which must be known before any other file is accessed, such as:
      default_lib_dir= /opt/dosemu  # replaces /var/lib/dosemu
      config_script= /etc/dosemu/global.conf    # uses this file instead of the
                                # built-in global.conf
      log_level= 2                  # highest log level

   With `default_lib_dir=' you may move /var/lib/dosemu elsewere, this
   mostly is interesting for distributors, who want it elsewhere but
   won't patch the DOSEMU source just for this purpose. But note, the
   dosemu supplied scripts and helpers may need some adaption too in
   order to fit your new directory.

   By default, a global.conf is used that is linked into the main DOSEMU
   executable. Using `config_script=` you can specify a standalone
   filename for use as 'global.conf'. The default setting is 'builtin'.

   The `log_level=' can be 0 (never log) or 1 (log only errors) or 2 (log
   all) and controls the ammount written to the systems log facility
   (notice). This keyword replaces the former /etc/dosemu.loglevel file,
   which now is obsolete.
     _________________________________________________________________

2.2. Format of global.conf ( (old) .dosrc, -I option)

   The configuration files are not line oriented, instead are consisting
   of `statements' (optionally separated by `;'), whitespaces are removed
   and all behind a '#' up to the end of the line is treated as comment.
   ( Note that older DOSEMUs also allowed `!' and `;' as comment
   character, but those are no longer supported ).
     _________________________________________________________________

2.2.1. Enviroment variables and configuration variables

   They existed already in very early versions of DOSEMU (until now), but
   now evironment variables are much more useful in dosemu.conf /
   global.conf as before, because you now can set them, test them in the
   new 'if' statement and compute them in expressions. The problem with
   the enviroment variables is, however, that the user may set and fake
   them before calling DOSEMU, hence this is a security problem. To avoid
   this, we have the above mentioned configuration variables, which are
   of complete different name space and are not visible outside of
   DOSEMU's configuration parser. On the other hand it may be useful to
   export settings from global.conf to the running DOS environment, which
   can be achieved by the 'unix.exe -e' DOS programm.

   To summarize:

   configuration variables
          have their own namespace only within the configuration parser.
          They usual are prefixed by c_, u_ and h_ and cannot be made
          visible outside. They do not contain any value and are only
          tested for existence.

   environment variables
          are inherited from the calling process, can be set within
          global.conf (dosemu.conf) and passed to DOSEMU running
          DOS-applications. Within *.conf they always are prefixed by '$'
          (Hence TERM becomes $TERM, even on the left side of an
          assigment). However, setting them is controled by the
          configuration variable 'c_var' (see above) and may be
          disallowed within the user supplied (old) .dosrc and alike.

   At startup DOSEMU generates the following environment variables, which
   may be used to let the configuration adapt better:

   KERNEL_VERSION_CODE
          holds the numerical coded version of the running linux kernel
          (same format as within linux/version.h)

   DOSEMU_VERSION_CODE
          holds the numerical coded version of the running DOSEMU version
          (format: MSB ... LSB == major, minor, patchlevel, sublevel)

   DOSEMU_EUID
          effective uid

   DOSEMU_UID
          uid. You may protect security relevant parts of the
          configuration such as:

    if ( ! $DOSEMU_EUID && ($DOSEMU_EUID != $DOSEMU_UID) )
      warn "running suid root"
    endif


   DOSEMU_USER
          The user name, that got matched in /etc/dosemu.users. This
          needs not to be the _real_ user name, it may be `all' or
          `unknown'.

   DOSEMU_REAL_USER
          The user name out of getpwuid(uid).

   DOSEMU_SHELL_RETURN
          The exitcode (0-255) from the recently executed shell()
          command.

   DOSEMU_OPTIONS
          A string of all commandline options used (one character per
          option). You may remove a character from this string, such that
          the normal override of dosemu.conf settings will not take place
          for that option. However, parsing the command line options
          happens in two stages, one _before_ parsing dosemu.conf and one
          _after_. The options 'FfhIdLoO23456' have already gotten
          processed before dosemu.conf, so they can be disabled.
     _________________________________________________________________

2.2.2. Conditional statements

   You may control execution of configuration statements via the
   following conditional statement:

         ifdef <configuration variable>

   or
      ifndef <configuration variable>
        ...
      else
        ...
      endif

   where variable is a configuration variable (not an environment
   variable). Additionally there is a `normal' if statement, a while
   statement and a foreach statement such as
      if ( expression )
      endif
      while ( expression )
      done
      foreach loop_variable (delim, list)
      done

   but these behaves a bit different and are described later.

   The 'else' clause may be ommitted and 'ifndef' is the opposite to
   'ifdef'. The <variable> can't be tested for its contents, only if it
   is set or not. Clauses also may contain further if*def..endif clause
   up to a depth of 15. All stuff in /etc/dosemu.users behind the
   'loginname' in fact are configuration variables that are set. Hence,
   what you set there, can be tested here in the config file. Further
   more you may set/unset configuration variables in the config files
   itself:

      define <configuration variable>
      undef  <configuration variable>

   However, use of define/undef is restricted to scope of global.conf, as
   long as you don't 'define c_var' _within_ global.conf. If you are
   under scope of a 'user config file' (e.g. outside global.conf) you
   have to prefix the configuration variable name with 'u_', else it will
   not be allowed to be set/unset (hence 'c_' type variables can't be
   unset out of scope of global.conf).

   There are some configuration variables (besides the ones described
   above for dosemu.users) implicitely predefined by DOSEMU itself:

   c_system
          set while being in global.conf

   c_user
          set while parsing a user configuration file

   c_dosrc
          set while parsing (old) .dosrc

   c_comline
          set while parsing -I option statements

   c_dexerun
          set if a DEXE will be executed

   h_<ownhost>
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

       # dosemu -u myspecialfun

   this will then define the config variable 'u_myspecialfun' _before_
   parsing any configuration file. You then may check this in your (old)
   ./dosrc or global.conf to do the needed special configuration.

   Now, what's this with the if statement and while statement? All those
   conditionals via ifdef and indef are completly handled before the
   remaining input is passed to the parser. Hence you even may use them
   within a configuration statement such as

    terminal { charset
      ifdef u_likeibm
        ibm
      else
        latin
      endif
      updatefreq 4  color on }

   This is not the case with the (above mentioned) if statement, this one
   is of course processed within the parser itself and can only take
   place within the proper syntax context such as


      if ( defined( u_likeibm ) )
        $mycharset = "ibm"
      else
        $mycharset = "latin"
      endif
      terminal { charset $mycharset updatefreq 4  color on }

   but it has the advantage, that you can put any (even complex)
   expression (see chapter `expressions') between the brackets. If the
   expression's numerical value is 0, then false is assumed, else true.

   Same rules apply to the while statement, the loop will be executed as
   long as `expression' is not 0. The loop end is given by the keyword
   done such as in

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

   The foreach statement behaves a bit like the /bin/sh `for i in', but
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
     _________________________________________________________________

2.2.3. Include files

   If you for some reason want to bundle some major settings in a
   separate file you can include it via

         include "somefile"

   If 'somefile' doesn't have a leading '/', it is assumed to be relative
   to /etc. Also includeing may be nested up to a max depth of 10 files.
   Note however, that the privilege is inherited from the main file from
   which is included, hence all what is included by global.conf has its
   privilege.

   However, there are restrictions for `while' loops: You can't have
   include statements within loops without beeing prepared for unexpected
   behave. In fact you may try, but due to the technique used, include
   files within loops are loaded completely prior loop execution. Hence,
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
     _________________________________________________________________

2.2.4. Macro substitution

   There is a very rudimentary macro substitution available, which does
   allow recursion but no arguments: Whenever you write

      $MYMACRO = "warn 'this is executed as macro'" ;
      $$MYMACRO

   it will expand to
         warn 'this is executed as macro'

   Note, that the `;' is required for parser to recognize the end of the
   statement and to set the variable before it tries to read the next
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
     _________________________________________________________________

2.2.5. Expressions

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

   int(real)
          converts a float expression to integer

   real(integer)
          converts a integer expression to float

   strlen(string)
          returns the length of `string'

   strtol(string)
          returns the integer value of `string'

   strncmp(str1,str2,expression)
          compares strings, see `man strncmp'

   strpbrk(str1,str2)
          like `man strpbrk', but returns an index or -1 instead of a
          char pointer.

   strchr(str1,str2)
          like `man strchr', but returns an index or -1 instead of a char
          pointer.

   strrchr(str1,str2)
          like `man strrchr', but returns an index or -1 instead of a
          char pointer.

   strstr(str1,str2)
          like `man strstr', but returns an index or -1 instead of a char
          pointer.

   strspn(str1,str2)
          as `man strspn'

   strcspn(str1,str2)
          as `man strcspn'

   defined(varname)
          returns true, if the configuration variable `varname' exists.
     _________________________________________________________________

2.2.6. String expressions

   For manipulation of strings there are the following builtin functions,
   which all return a new string. These very well may be placed as
   argument to a numerical function's string argument, but within an
   `expression' they may be only used together with the `eq' or `ne'
   operator.

   strcat(str1, ,strn)
          concatenates any number of strings, the result is a string
          again.

   strsplit(string, expr1, expr2)
          cuts off parts of `string', starting at index `expr1' with
          length of `expr2'. If either `expr1' is < 0 or `expr2' is < 1,
          an empty string is returned.

   strdel(string, expr1, expr2)
          deletes parts of `string', starting at index `expr1' with
          length of `expr2'. If either `expr1' or `expr2' is < 0, nothing
          is deleted (the original strings is returned.)

   shell(string)
          executes the command in `string' and returns its stdout result
          in a string. The exit code of executed command is put into
          $DOSEMU_SHELL_RETURN (value between 0 and 255). You may also
          call shell() as a standalone statement (hence discarding the
          returned string), if you only need $DOSEMU_SHELL_RETURN (or not
          even that). However, to avoid security implications all
          privilegdes are dropped and executions is under control of
          c_shell configuration variable. The default is, that it can
          only be executed from within global.conf.

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

      # disable setting direct VGA console graphics mode per commandline option
 -V
      $DOSEMU_OPTIONS = strdel($DOSEMU_OPTIONS, strchr($DOSEMU_OPTIONS,"V"),1);
     _________________________________________________________________

2.2.7. `Dry' testing your configuration

   It may be useful to verify, that your *.conf does what you want before
   starting a real running DOSEMU. For this purpose there is a new
   commandline option (-h), which just runs the parser, print some useful
   output, dumps the main configuration structure and then exits. The
   option has an argument (0,1,2), which sets the amount of parser debug
   output: 0 = no parser debug, 1 = print loop debugs, 2 = same as 1 plus
   if_else_endif-stack debug. This feature can be used such as

         $ dosemu.bin -h0 -O 2>&1 | less

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
          |  | +-----------------`if' or `loop' test result (0 = false = skippe
d)
          |  +-------------------inner level
          +----------------------outer level (1 = no depth)

   There are also some configuration statements, which aren't of any use
   except for help debugging your *.conf such as

      exprtest ($DOSEMU_VERSION_CODE)   # will just print the result
      warn "content of DOSEMU_VERSION_CODE: ", $DOSEMU_VERSION_CODE
     _________________________________________________________________

2.2.8. Debug statement

   This section is of interest mainly to programmers. This is useful if
   you are having problems with DOSEMU and you want to enclose debug info
   when you make bug reports to a member of the DOSEMU development team.
   Simply set desired flags to "on" or "off", then redirect stderr of
   DOSEMU to a file using "dosemu.bin -o debug" to record the debug
   information if desired. Skip this section if you're only starting to
   set up.

      debug { config  off       disk    off     warning off     hardware off
            port    off read    off     general off     IPC      off
            video   off write   off     xms     off     ems      off
            serial  off keyb    off     dpmi    off
                printer off     mouse   off     sound   off
      }

   Alternatively you may use the same format as -D commandline option
   (but without the -D in front), look at the dosemu.bin.1 man page.

      debug "+a-v"     # all messages but video
      debug "+4r"      # default + maximum of PIC debug

   or simply (to turn off all debugging)

         debug { off }
     _________________________________________________________________

2.2.9. Miscellaneous

   The HogThreshold value determines how nice Dosemu will be about giving
   other Linux processes a chance to run. Setting the HogThreshold value
   to approximately half of you BogoMips value will slightly degrade
   Dosemu performance, but significantly increase overall system idle
   time. A zero value runs Dosemu at full tilt.

         HogThreshold 0

   Want startup DOSEMU banner messages? Of course :-)

         dosbanner on

   For "mathco", set this to "on" to enable the coprocessor during
   DOSEMU. This really only has an effect on kernels prior to 1.0.3.

         mathco on

   For "cpu", set this to the CPU you want recognized during DOSEMU.

         cpu 80386

   If you have DOSEMU configured to use the 386-emulator, you can enable
   the emulator via

         cpu emulated

   You may ask why we need to emulate a 386 on an 386 ;-) Well, for
   normal purpose its not needed (and will be slower anyway), but the
   emulator offers a better way to debug DOS applications, especially
   DPMI code. Also, we hope that some day we may be able to run DOSEMU on
   other machines than 386. At the time of writing this, the cpu
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

   For "bootA"/"bootC", set this to the bootup drive you want to use. It
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

         cdrom { /dev/xxx }

   However, you need to include the DOSEMU supplied cdrom.sys into your
   config.sys such as
         device=cdrom.sys

   If you have more then one cdrom, you have to use the cdrom statement
   multiple times such as
      cdrom { /dev/cdrom }
      cdrom { /dev/cdrom2 }

   and have multiple instancies of the DOS driver too:
      device=cdrom.sys
      device=cdrom.sys 2

   In any case you will need MSCDEX.EXE in your autoexe.bat refer to the
   DOS devices MSCD0001, MSCD0002 respectively.
     _________________________________________________________________

2.2.10. Code page and character set

   To select the character set and code page for use with DOSEMU we now
   (against earlier versions of DOSEMU) have a separate statement:
         charset XXX

   where XXX is one of

   ibm
          With the new (default) unicode keyboard plugin, the text is
          processed using cp437->cp437 for the display, so the font used
          must be cp437 (eq cp437.f16 on the console). This is no longer
          a passthrough to the local character set, it never really was,
          but it acted like it. When reading characters they are assumed
          to be in iso-8859-1 from the terminal.

          With the old keyboard code, the text is taken whithout
          translation, it is to the user to load a proper DOS font
          (cp437.f16, cp850.f16 or cp852.f16 on the console).

   latin
          the text is processed using cp437->iso-8859-1 translation, so
          the font used must be iso-8859-1 (eg iso01.f16 on console);
          which is the default for unix in western languages countries.

   latin1
          like latin, but using cp850->iso-8859-1 translation (the
          difference between cp437 and cp850 is that cp437 uses some
          chars for drawing boxes while cp850 uses them for accentuated
          letters)

   latin2
          like latin1 but uses cp852->iso-8859-2 translation, so
          translates the default DOS charset of eastern european
          countries to the default unix charset for those countries.

   The default one is ``latin'' and if the string is empty, then an
   automatic attempt is made: ``ibm'' for remote console and ``latin''
   for anything else. Depending on the charset setting the (below
   described) keyboard layouts and/or the terminal behave may vary. You
   need to know the correct code page your DOS is configured for in order
   to get the correct results. For most western european countries
   'latin' should be the correct setting.

   With the advent of handling all characters internally to dosemu things
   have gotten more flexible, and more interesting. The following form of
   the charset option has been added.
         charset { external "iso8859_1" internal "cp437" }

   For external character set the allowable character sets are:
    "cp437", "cp737", "cp773", "cp775", "cp850", "cp852", "cp857", "cp860",
    "cp861", "cp862", "cp863", "cp864", "cp865", "cp866", "cp869", "cp874",
    "iso8859-1", "iso8859-2", "iso8859-3", "iso8859-4", "iso8859-5",
    "iso8859-6", "iso8859-7", "iso8859-8", "iso8859-9", "iso8859-14",
    "iso8859-15"

   For the internal character set the allowable character sets are:
    "cp437", "cp737", "cp773", "cp775", "cp850", "cp852", "cp857", "cp860",
    "cp861", "cp862", "cp863", "cp864", "cp865", "cp866", "cp869", "cp874"

   The external character set is used to:

     * compute the unicode values of characters coming in from the
       terminal
     * compute the character set index of unicode characters output to a
       terminal display screen.
     * compute the unicode values of characters pasted into dosemu.

   The internal character set is used to:

     * compute the unicode value of characters of video memory
     * compute the character set index of unicode characters returned by
       bios keyboard translation services.
     _________________________________________________________________

2.2.11. Keyboard settings

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

   Given we call the above a "definition" then it may be repeated (blank
   separated) such as

         definition [definition [...]]

   "key_number" is the physical number, that the keybord send to the
   machine when you hit the key, its the index into the keytable. "value"
   may be any integer between 0...255, a string, or one of the following
   keywords for "dead keys" (NOTE: deadkeys have a value below 32, so be
   careful).

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
      dogonek       (dead ogonek)
      dcaron        (dead caron)

   With the addition of internal unicode support "value" may be any
   integer between 0..65535 (specifying a unicode value), a hexadecimal
   integer in the form \uFFFF all 4 hexadecimal digits are mandatory, a
   string interpreted in the internal character set.

   The implementation of dead keys has changed, and values of the unicode
   combining characters are used for dead key character caps. In
   particular:
    0x0300          U_COMBINING_GRAVE_ACCENT        dgrave
    0x0301          U_COMBINING_ACUTE_ACCENT        dacute
    0x0302          U_COMBINING_CIRCUMFLEX_ACCENT   dcircum
    0x0303          U_COMBINING_TILDE               dtilde
    0x0306          U_COMBINING_BREVE               dbreve
    0x0307          U_COMBINING_DOT_ABOVE           daboved
    0x0308          U_COMBINING_DIAERESIS           ddiares
    0x030A          U_COMBINING_RING_ABOVE          dabover
    0x030B          U_COMBINING_DOUBLE_ACUTE_ACCENT ddacute
    0x0327          U_COMBINING_CEDILLA             dcedilla
    0x0328          U_COMBINING_OGONEK              dogonek
    0x030C          U_COMBINING_CARON               dcaron

   In unicode there is a private range of characters 0xE000 - 0xEFFF that
   an application can use as it wishes. In dosemu the exact usage of this
   range is defined in src/include/keyboard.h. Current this range is
   broke up as follows:
    0xEF00 - 0xEFFF is defined as a pass through to whatever
                    character set is being displayed.
    0xE100 - 0xE1FF are used for special keycaps
    0xE200 - 0xE2FF are used for any extra dead keys we may need to make up
    0xE300 - 0xE3FF are used for special dosemu function keys

   After a "key_number =" there may be any number of comma separated
   values, which will go into the table starting with "key_number", hence
   all below examples are equivalent

      { 2="1" 3="2" }
      { 2="1","2" }
      { 2="12" }
      { 2=0x31,50 }

   "submap" tells in about something about the shift state for which the
   definition is to use or wether we mean the numpad:

      shift     16="Q"      (means key 16 + SHIFT pressed)
      alt       16="@"      (means key 16 + right ALT pressed (so called AltGr)
)
      numpad    12=","      (means numpad index 12 (depricated))
      ctrl      16=\u0011   (means key 16 +  ctrl pressed)
      shift-alt 16=\u02A0   (means key 16 + AltGR pressed and shift pressed)
      ctrl-alt  209=\uE30A  (means key 209 (PGDN) + ctrl pressed and alt presse
d)

   You may even replace the whole table with a comma/blank separated list
   of values. In order to make it easy for you, you may use dosemu to
   create such a list. The following statement will dump all current key
   tables out into a file "kbdtables", which is directly suitable for
   inclusion into global.conf (hence it follows the syntax):

         keytable dump "kbdtables"

   However, don't put this not into your global.conf, because dosemu will
   exit after generating the tablefile. Instead use the -I commandline
   option such as

         $ dosemu.bin -I 'keytable dump "kbdtables"'

   After installation of dosemu ("make install") you'll find the current
   dosemu keytables in /var/lib/dosemu/keymap (and in ./etc/keymap, where
   they are copied from). These tables are generated via "keytable dump"
   and split into discrete files, such that you may modify them to fit
   your needs. You may load them such as

         $ dosemu.bin -I 'include "keymap/de-latin1"'

   (when an include file is starting with "keymap/" it is taken out of
   /var/lib/dosemu). When there was a keytable previously defined (e.g.
   in global.conf), they new one will be take anyway and a warning will
   be printed on stderr.

   Anyway, you are not forced to modify or load a keytable, and with the
   "layout" keyword from the "keyboard" statement, you can specify your
   country's keyboard layout. The following builtin layouts are
   implemented:

        finnish           us           dvorak       sf
        finnish-latin1    uk           sg           sf-latin1
        de                dk           sg-latin1    es
        de-latin1         dk-latin1    fr           es-latin1
        be                keyb-no      fr-latin1    po
        it                no-latin1    sw           jp106
        hu                hu-cwi       hu-latin2    keyb-user
        po                hr-cp852     hr-latin2

   The keyb-user is selected by default if the "layout" keyword is
   omitted, and this in fact is identical to us-layout, as long as it got
   not overloaded by the "keytable" statement (see above).

   The keyword "keybint" allows more accurate of keyboard interrupts, It
   is a bit unstable, but makes keyboard work better when set to "on".

   The keyword "rawkeyboard" allows for accurate keyboard emulation for
   DOS programs, and is only activated when DOSEMU starts up at the
   console. It only becomes a problem when DOSEMU prematurely exits with
   a "Segmentation Fault" fatal error, because the keyboard would have
   not been reset properly. In that case, you would have to run kbd_mode
   -a remotely, or use the RESET button. In reality, this should never
   happen. But if it does, please do report to the dosemu development
   team, of the problem and detailed circumstances, we're trying our
   best! If you don't need near complete keyboard emulation (needed by
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

         # dosemu.bin -D-a -I 'keystroke "c:\rcd \\windows\rwinemu\r"'

   For more details please look at ./doc/README.batch

   Ah, but _one_ sensible useage _here_ is to 'pre-strike' that damned F8
   that is needed for DOS-7.0, when you don't want to edit the msdos.sys:

        keystroke "\F8;"
     _________________________________________________________________

2.2.12. Serial stuff

   You can specify up to 4 simultaneous serial ports here. If more than
   one ports have the same IRQ, only one of those ports can be used at
   the same time. Also, you can specify the com port, base address, irq,
   and device path! The defaults are:

     * COM1 default is base 0x03F8, irq 4, and device /dev/cua0
     * COM2 default is base 0x02F8, irq 3, and device /dev/cua1
     * COM3 default is base 0x03E8, irq 4, and device /dev/cua2
     * COM4 default is base 0x02E8, irq 3, and device /dev/cua3

   If the "com" keyword is omitted, the next unused COM port is assigned.
   Also, remember, these are only how you want the ports to be emulated
   in DOSEMU. That means what is COM3 on IRQ 5 in real DOS, can become
   COM1 on IRQ 4 in DOSEMU!

   NOTE: You must have /var/lock for LCK-file generation ! You may change
   this path and the lockfile name via the below 'ttylocks' statement.

   Also, as an example of defaults, these two lines are functionally
   equal:

      serial { com 1  mouse }
      serial { com 1  mouse  base 0x03F8  irq 4  device /dev/cua0 }

   If you want to use a serial mouse with DOSEMU, the "mouse" keyword
   should be specified in only one of the serial lines. (For PS/2 mice,
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

   What type is your mouse? Use one of the following. Use the
   'internaldriver' option to try Dosemu internaldriver. Use the
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
   expect the lock files. The most common used place is /var/lock. The
   dosemu default one is /var/lock. The below defines /var/lock

         ttylocks { directory /var/lock }

   Note: you are responsible for ensuring that the directory exists ! If
   you want to define the lock prefix stub also, use this one

         ttylocks { directory /var/lock namestub LCK.. }

   If the lockfile should contain the PID in binary form (instead of
   ASCII}, you may use the following

         ttylocks { directory /var/lock namestub LCK.. binary }
     _________________________________________________________________

2.2.13. Networking Support

   Turn the following option 'on' if you require IPX/SPX emulation.
   Therefore, there is no need to load IPX.COM within the DOS session.
   The following option does not emulate LSL.COM, IPXODI.COM, etc. NOTE:
   YOU MUST HAVE IPX PROTOCOL ENABLED IN KERNEL !!

         ipxsupport off

   Enable Novell 8137->raw 802.3 translation hack in new packet driver.

         pktdriver novell_hack
     _________________________________________________________________

2.2.14. Terminals

   This section applies whenever you run DOSEMU remotely or in an xterm.
   Color terminal support is now built into DOSEMU. Skip this section for
   now to use terminal defaults, until you get DOSEMU to work.

   There are a number of keywords for the terminal { } configuration
   line.

   color
          Enable or disable color terminal support. One of ``on''
          (default) or ``off''.

   updatefreq
          A number indicating the frequency of terminal updates of the
          screen. The smaller the number, the more frequent. A value of
          20 gives a frequency of about one per second, which is very
          slow. However, more CPU time is given to DOS applications when
          updates are less frequent. A value of 4 (default) is
          recommended in most cases, but if you have a fast system or
          link, you can decrease this to 0.

   escchar
          A number (ascii code below 32) that specifies the control
          character used as a prefix character for sending alt, shift,
          ctrl, and function keycodes. The default value is 30 which is
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
     _________________________________________________________________

2.2.15. X Support settings

   If DOSEMU is running in its own X-window (not xterm), you may need to
   tailor it to your needs. Valid keywords for the X { } config line:

   updatefreq
          A number indicating the frequency of X updates of the screen.
          The smaller the number, the more frequent. A value of 20 gives
          a frequency of about one per second, which is very slow.
          However, more CPU time is given to DOS applications when
          updates are less frequent. The default is 8.

   display
          The X server to use. If this is not specified, dosemu will use
          the DISPLAY environment variable. (This is the normal case) The
          default is ":0".

   title
          What you want dosemu to display in the title bar of its window.
          The default is "dosemu".

   icon_name
          Used when the dosemu window is iconified. The default is
          "dosemu".

   keycode
          Used to give Xdos access to keycode part of XFree86. The
          default is off.

          NOTE:

          + You should _not_ use this when using X remotely (the remote
            site may have other raw keyboard settings).
          + If you use "keycode", you also must define an appropriate
            keyboard layout (see above).
          + If you do not use "keycode" then under X a neutral keyboard
            layout is forced ( keyboard {layout us} ) regardless of what
            you have set above.

          Anyway, a cleaner way than using "keycode" is to let the
          X-server fiddle with keyboard translation and customize it via
          .xmodmaps.

   blinkrate
          A number which sets the blink rate for the cursor. The default
          is 8.

   font
          Used to pick a font other than vga (default). Must be
          monospaced.

   mitshm
          Use shared memory extensions. The default is ``on''.

   sharecmap
          Used to share the colormap with other applications in graphics
          mode. If not set, a private colormap is used. The default is
          off.

   fixed_aspect
          Set fixed aspect for resize the graphics window. One of ``on''
          (default) or ``off''.

   aspect_43
          Always use an aspect ratio of 4:3 for graphics. The default is
          ``floating''.

   lin_filt
          Use linear filtering for >15 bpp interpolation. The default is
          off.

   bilin_filt
          Use bi-linear filtering for >15 bpp interpolation. The default
          is off.

   mode13fact
          A number which sets the initial size factor for video mode 0x13
          (320x200). The default is 2.

   winsize
          A pair of numbers which set the initial width and height of the
          window to a fixed value. The default is to float.

   gamma
          Set value for gamma correction, a value of 100 means gamma 1.0.
          The default is 100.

   vgaemu_memsize
          Set the size (in Kbytes) of the frame buffer for emulated vga
          under X. The default is 1024.

   lfb
          Enable or disable the linear frame buffer in VESA modes. The
          default is ``on''.

   pm_interface
          Enable or disable the protected mode interface for VESA modes.
          The default is ``on''.

   mgrab_key
          String, name of KeySym name to activate mouse grab. Default is
          `empty' (no mouse grab). A possible value could be "Home".

   vesamode
          Define a VESA mode. Two variants are supported: vesamode width
          height and vesamode width height color_bits. The first adds the
          specified resolution in all supported color depths (currently
          8, 15, 16, 24 and 32 bit).

   Recommended X statement:

         X { updatefreq 8 title "DOS in a BOX" icon_name "xdos" }
     _________________________________________________________________

2.2.16. Video settings ( console only )

   !!WARNING!!: A LOT OF THIS VIDEO CODE IS ALPHA! IF YOU ENABLE GRAPHICS
   ON AN INCOMPATIBLE ADAPTOR, YOU COULD GET A BLANK SCREEN OR MESSY
   SCREEN EVEN AFTER EXITING DOSEMU. JUST REBOOT (BLINDLY) AND THEN
   MODIFY CONFIG.

   Start with only text video using the following line, to get started.
   then when DOSEMU is running, you can set up a better video
   configuration.

      video { vga }                    Use this line, if you are using VGA
      video { cga  console }           Use this line, if you are using CGA
      video { ega  console }           Use this line, if you are using EGA
      video { mda  console }           Use this line, if you are using MDA

   Notes for Graphics:

     * If your VGA-Bios resides at E000-EFFF, turn off video BIOS shadow
       for this address range and add the statement vbios_seg 0xe000 to
       the correct vios-statement, see the example below
     * If your VBios size is only 32K you set it with vbios_size 0x8000,
       you then gain some space for UMB or hardware ram locations.
     * Set "allowvideoportaccess on" earlier in this configuration file
       if DOSEMU won't boot properly, such as hanging with a blank
       screen, beeping, leaving Linux video in a bad state, or the video
       card bootup message seems to stick.
     * Video BIOS shadowing (in your CMOS setup) at C000-CFFF must be
       disabled.
       CAUTION: TURN OFF VIDEO BIOS SHADOWING BEFORE ENABLING GRAPHICS!
       This is not always necessary, but a word to the wise shall be
       sufficient.
     * If you have a dual-monitor configuration (e.g. MDA as second
       display), you then may run CAD programs on 2 displays or let play
       your debugger on the MDA while debugging a graphics program on the
       VGA (e.g TD -do ). You also may switch to the MDA display by using
       the DOS command mode mono (mode co80 returns to your normal
       display). This feature can be enabled by the switch "dualmon" like
       this:

          video { vga  console  graphics dualmon }

       and can be used on a xterm and the console, but of course not, if
       you have the MDA as your primary display. You also must set
       USE_DUALMON 1 in include/video.h. NOTE: Make sure no more then one
       process is using this feature ! ( you will get funny garbage on
       your MDA display. ) Also, you must NOT have the dualmon-patches
       for kernel applied ( having the MDA as Linux console )
     * If you want to run dosemu in situations when human doesn't sit at
       console (for instance to run it by cron) and want console option
       be enabled you should use option forcevtswitch.

               { vga console forcevtswitch }

       Without the option dosemu waits for becoming virtual terminal on
       which dosemu is run active (i.e. user must press Alt-F?). With
       this option dosemu perform the switch itself. Be careful with this
       option because with it user sat at console may face with
       unexpected switch.

   It may be necessary to set this to "on" if DOSEMU can't boot up
   properly on your system when it's set "off" and when graphics are
   enabled. Note: May interfere with serial ports when using certain
   video boards.

         allowvideoportaccess on

   Any 100% compatible standard VGA card MAY work with this:

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
      video { vga  console  graphics  chipset et4000  memsize 1024 vbios_size 0
x8000 }

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
     _________________________________________________________________

2.2.17. Memory settings

   These are memory parameters, stated in number of kilobytes. If you get
   lots of disk swapping while DOSEMU runs, you should reduce these
   values.

   umb_max is a new parameter which tells DOSEMU to be more aggressive
   about finding upper memory blocks. The default is 'off'.

   To be more aggressive about finding XMS UMB blocks use this:

         umb_max on

   To be more secure use 'secure on'. If "on", then it disables DPMI
   access to dosemu code and also disables execution of dosemu supplied
   'system' commands, which may execute arbitrary Linux-commands
   otherwise. The background is, that DPMI clients are allowed to create
   selectors that span the whole user space, hence may hack into the
   dosemu code, and (when dosemu runs root or is suid root) can be a
   security hole. "secure on" closes this hole, though this would very
   likely also disable some dos4gw games :(. Therfore NOTE: You may not
   be able to run some DPMI programs, hence, before reporting such a
   program as 'not running', first try to set 'secure off'.

         secure on                # "on" or "off"

   The below enables/disables DPMI and sets the size of DPMI memory.

         dpmi 4086                # DPMI size in K, or "off"

   The best solution (security wise), however, is to run dosemu non-suid
   root under X (which is possible since dosemu-0.97.10). Under X most of
   the things we would need to run under root privilidges aren't needed,
   as the X server supports them. And giving DPMI access to non-suid root
   dosemu is not at all critical. You may forbid some users to use an
   eventually available suid root binary by setting `nosuidroot' in
   /etc/dosemu.users.

   XMS is enabled by the following statement

         xms 1024             # XMS size in K,  or "off"

   For ems, you now can set the frame to any 16K between 0xc800..0xe000

         ems 1024             # EMS size in K,  or "off"

   or
         ems { ems_size 1024 ems_frame 0xe000 }

   or
         ems { ems_size 2048 ems_frame 0xd000 }

   If you have adapters, which have memory mapped IO, you may map those
   regions with hardware_ram { .. }. You can only map in entities of 4k,
   you give the address, not the segment. The entry below maps
   0xc8000..0xc8fff and 0xcc000..0xcffff:

         hardware_ram { 0xc8000 range 0xcc000 0xcffff }

   With the entry below you define the maximum conventional RAM to show
   apps:

         dosmem 640
     _________________________________________________________________

2.2.18. IRQ passing

   The irqpassing statement accepts IRQ values between 3..15, if using
   the { .. } syntax each value or range can be prefixed by the keyword
   use_sigio to monitor the IRQ via SIGIO. If this is missing the IRQ is
   monitored by SIGALRM.

   Use/modify one of the below statements

      irqpassing off    # this disables IRQ monitoring
      irqpassing 15
      irqpassing { 15 }
      irqpassing { use_sigio 15 }
      irqpassing { 10  use_sigio range 3 5 }
     _________________________________________________________________

2.2.19. Port Access

   WARNING: GIVING ACCESS TO PORTS IS BOTH A SECURITY CONCERN AND SOME
   PORTS ARE DANGEROUS TO USE. PLEASE SKIP THIS SECTION, AND DON'T FIDDLE
   WITH THIS SECTION UNLESS YOU KNOW WHAT YOU'RE DOING.

   These keywords are allowable on a "ports" line.

   range addr1 addr2
          This allows access to this range of ports

   ormask value
          The default is 0

   andmask value
          The default is 0xffff

   rdonly|wronly|rdwr
          This specifies what kind of access to allow to the ports. The
          default is "rdwr"

   fast
          Put port(s) in the ioperm bitmap (only valid for ports below
          0x400) An access doesn't trap and isn't logged, but as vm86()
          isn't interrupted, it's much faster. The default is not fast.

   device name
          If the ports are registered, open this device to block access.
          The open() must be successfull or access to the ports will be
          denied. If you know what you are doing, use /dev/null to fake a
          device to block

      ports { 0x388 0x389 }   # for SimEarth
      ports { 0x21e 0x22e 0x23e 0x24e 0x25e 0x26e 0x27e 0x28e 0x29e } # for jil
l
     _________________________________________________________________

2.2.20. Speaker

   These keywords are allowable on the "speaker" line:

   native
          Enable DOSEMU direct access to the speaker ports.

   emulated
          Enable simple beeps at the terminal.

   off
          Disable speaker emulation.

   Recommended:

         speaker off
     _________________________________________________________________

2.2.21. Hard disks

   WARNING: DAMAGE MIGHT RESULT TO YOUR HARD DISK (LINUX AND/OR DOS) IF
   YOU FIDDLE WITH THIS SECTION WITHOUT KNOWING WHAT YOU'RE DOING!

   The best way to get started with DOSEMU is to use use the bootdisk
   method (see doc/README.txt, chapter "Disks, boot directories and
   floppies"). Fiddling with hdimages and real harddisk is obsolete now.

   As a last resort, if you want DOSEMU to be able to access a DOS
   partition, the safer type of access is "partition" access, because
   "wholedisk" access gives DOSEMU write access to a whole physical disk,
   including any vulnerable Linux partitions on that drive!

   IMPORTANT

   You must not have LILO installed on the partition for dosemu to boot
   off. As of 04/26/94, doublespace and stacker 3.1 will work with
   wholedisk or partition only access. Stacker 4.0 has been reported to
   work with wholedisk access.

   Please read the documentation in the "doc" subdirectory for info on
   how to set up access to real hard disk.

   These are meanings of the keywords:

   image
          specifies a hard disk image file.

   partition
          specifies partition access, with device and partition number.

   wholedisk
          specifies full access to entire hard drive.

   readonly
          for read only access. A good idea to set up with.

   diskcyl4096
          INT13 support for more then 1024 cylinders, bits 6/7 of DH
          (head) used to build a 12 bit cylinder number.

   bootfile
          to specify an image of a boot sector to boot from.

   Use/modify one (or more) of the folling statements:

      disk { image "/var/lib/dosemu/hdimage" }      # use diskimage file.
      disk { partition "/dev/hda1" readonly }       # 1st partition on 1st IDE.
      disk { partition "/dev/hda1" bootfile "/var/lib/bootsect.dos" }
                                                    # 1st partition on 1st IDE
                                                    # booting from the specifie
d
                                                    # file.
      disk { partition "/dev/hda6" readonly }       # 6th logical partition.
      disk { partition "/dev/sdb1" readonly }       # 1st partition on 2nd SCSI
.
      disk { wholedisk "/dev/hda" }                 # Entire disk drive unit

   Recommended:

         disk { image "/var/lib/dosemu/hdimage" }
     _________________________________________________________________

2.2.22. DOSEMU boot

   Use the following option to boot from the specified file, and then
   once booted, have bootoff execute in autoexec.bat. Thanks Ted :-).
   Notice it follows a typical floppy spec. To create this file use:

       dd if=/dev/fd0 of=/var/lib/dosemu/bdisk bs=16k

      bootdisk { heads 2 sectors 18 tracks 80 threeinch file /var/lib/dosemu/bd
isk }

   Specify extensions for the CONFIG and AUTOEXEC files. If the below are
   uncommented, the extensions become CONFIG.EMU and AUTOEXEC.EMU. NOTE:
   this feature may affect file naming even after boot time. If you use
   MSDOS 6+, you may want to use a CONFIG.SYS menu instead.

      EmuSys EMU
      EmuBat EMU
     _________________________________________________________________

2.2.23. Floppy disks

   This part is fairly easy. Make sure that the first (/dev/fd0) and
   second (/dev/fd1) floppy drives are of the correct size, "threeinch"
   and/or "fiveinch". A floppy disk image can be used instead, however.

   FOR SAFETY, UNMOUNT ALL FLOPPY DRIVES FROM YOUR FILESYSTEM BEFORE
   STARTING UP DOSEMU! DAMAGE TO THE FLOPPY MAY RESULT OTHERWISE!

   Use/modify one of the below:

      floppy { device /dev/fd0 threeinch }
      floppy { device /dev/fd1 fiveinch }
      floppy { heads 2  sectors 18  tracks 80
               threeinch  file /var/lib/dosemu/diskimage }

   If floppy disk speed is very important, uncomment the following line.
   However, this makes the floppy drive a bit unstable. This is best used
   if the floppies are write-protected. Use an integer value to set the
   time between floppy updates.

         FastFloppy 8
     _________________________________________________________________

2.2.24. Printers

   Printer is emulated by piping printer data to a file or via a unix
   command such as "lpr". Don't bother fiddling with this configuration
   until you've got DOSEMU up and running already.

   NOTE: Printers are assigned to LPT1:, LPT2:, and LPT3: on a one for
   one basis with each line below. The first printer line is assigned to
   LPT1:, second to LPT2:, and third to LPT3:. If you do not specify a
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
   applications that continue to use the standard bios calls. These
   applications will continue to send the output piped to the file or
   unix command.
     _________________________________________________________________

2.2.25. Sound

   The sound driver is capable of emulating Sound Blaster cards up to and
   including the SB16. It works for most programs, but there are a few
   that cause problems.

   sb_base
          base address of the SB (HEX)

   sb_irq
          IRQ for the SB

   sb_dma
          Low 8-bit DMA channel for the SB

   sb_hdma
          High 16-bit DMA channel for the SB

   sb_dsp
          Path to the sound device

   sb_mixer
          path to the mixer control

   mpu_base
          base address for the MPU-401 chip (HEX) (Not Implemented)

   Use this to disable sound support even if it is configured in

         sound_emu off

   Linux defaults

      sound_emu { sb_base 0x220 sb_irq 5 sb_dma 1 sb_hdma 5 sb_dsp /dev/dsp
                   sb_mixer /dev/mixer mpu_base 0x330 }

   NetBSD defaults

      sound_emu { sb_base 0x220 sb_irq 5 sb_dma 1 sb_hdma 5 sb_dsp /dev/sound
                  sb_mixer /dev/mixer mpu_base 0x330 }
     _________________________________________________________________

3. Accessing ports with dosemu

   This section written by Alberto Vignani <vignani@mbox.vol.it> , Aug
   10, 1997 and updated by Bart Oldeman, June 2003.
     _________________________________________________________________

3.1. General

   For port I/O access the type ioport_t has been defined; it should be
   an unsigned short, but the previous unsigned int is retained for
   compatibility. Also for compatibility, the order of parameters has
   been kept as-is; new code should use the port_real_xxx(port,value)
   function. The new port code is selected at configuration time by the
   parameter
           --enable-new-port

   which will define the macro NEW_PORT_CODE. Files: portss.c is now no
   more used and has been replaced by n_ports.c; all functions used by
   'old' port code have been moved to ports.c. Note that the code in
   portss.c (retrieved from Scott Buchholz's pre-0.61) was previously
   disabled (not used), because it had problems with older versions of
   dosemu.

   The rep;in,out instructions will been optimized so to call iopl() only
   once.
     _________________________________________________________________

3.2. Port I/O access

   Every process under Linux has a map of ports it is allowed to access.
   Too bad this map covers only the first 1024 (0x400) ports. For all the
   ports whose access permission is off, and all ports over 0x400, an
   exception is generated and trapped by dosemu.

   When the I/O permission (ioperm) bit for a port is ON, the time it
   takes to access the port is much lower than a microsecond (30 cycles
   on a P5-150); when the port is accessed from dosemu through the
   exception mechanism, access times are in the range of tenths of us
   (3000 cycles on the P5-150) instead. It is easy to show that 99% of
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

     * the port table, a 64k char array indexed by port number and
       storing the number of the handle to be used for that port. 0 means
       no handle defined, valid range is 1-253, 0xfe and 0xff are
       reserved.
     * the handle table, an array of structures describing the properties
       for a port group and its associated functions: static struct
       _port_handler { unsigned char(*read_portb) (ioport_t port_addr);
       void (*write_portb) (ioport_t port_addr, unsigned char byte);
       unsigned short(*read_portw) (ioport_t port_addr); void
       (*write_portw) (ioport_t port_addr, unsigned short word); char
       *handler_name; int irq, fd; } port_handler[EMU_MAX_IO_DEVICES];

   It works this way: when an I/O instruction is trapped (in do_vm86.c
   and dpmi.c) the standard entry points port_in[bwd],port_out[bwd] are
   called. They log the port access if specified and then perform a
   double indexed jump into the port table to the function responsible
   for the actual port access/emulation.

   Ports must be registered before they can be used. This is the purpose
   of the port_register_handler function, which is called from inside the
   various device initializations in dosemu itself, and of port_allow_io,
   which is used for user-specified ports instead. Some ports, esp. those
   of the video adapter, are registered an trapped this way only under X,
   while in console mode they are permanently enabled (ioperm ON). The
   ioperm bit is also set to ON for the user-specified ports below 0x400
   defined as fast. Of course, when a port has the ioperm bit ON, it is
   not trapped, and thus cannot be traced from inside dosemu.

   There are other things to consider:

     * system integrity
     * system security
     _________________________________________________________________

3.2.1. System Integrity

   To block two programs from accessing a port without knowing what the
   other program does, this is the strategy dosemu takes:

     * If the port is not listed in /proc/ioports, no other program
       should access the port. Dosemu will register these ports. This
       will also block a second dosemu-process from accessing these
       ports. Unfortunately there is no kernel call yet for registering a
       port range system-wide; see later for current hacks.
     * If the port is listed, there's probably a device that could use
       these ports. So we require the system administrator to give the
       name of the corresponding device. Dosemu tries to open this device
       and hopes this will block other from accessing. The parallel ports
       (0x378-0x37f) and /dev/lp1 act in this way.
       To allow access to a port registered in /proc/ioports, it is
       necessary that the open on the device given by the system
       administrator succeeds. An open on /dev/null will always succeed,
       but use it at your own risk.
     _________________________________________________________________

3.2.2. System Security

   If the strategy administrator did list ports in /etc/dosemu.conf and
   allows a user listed in /etc/dosemu.users to use dosemu, (s)he must
   know what (s)he is doing. Port access is inherently dangerous, as the
   system can easily be screwed up if something goes wrong: just think of
   the blank screen you get when dosemu crashes without restoring screen
   registers...
     _________________________________________________________________

3.2.3. The port server

   Starting with version 1.1.4.5 the exception mechanism uses a port
   server. If any slow ports from $_ports, any ports above 0x3ff
   (including some video cards and $_pci), or the native speaker are
   selected (timer 2 of port 0x43 cannot be fast), DOSEMU will fork. The
   main DOSEMU will then drop its root privileges and communicates via
   pipes with the (forked) privileged port server. The server then checks
   if it is allowed to access the port and acts appropriately. This way
   it is impossible for a DPMI program to manipulate any forbidden ports
   (separate address spaces). Fortunately the overhead of pipes and
   process switching seems to be negligible compared to the time it takes
   to trap the port access.

   If the speaker is emulated and all ports are "fast", or if DOSEMU is
   non-suid-root and run by a normal user, then the above forking is
   unnecessary and does not occur.
     _________________________________________________________________

4. The Virtual Flags

   Info written by Hans <lermen@fgan.de> to describe the virtual flags
   used by DOSEMU

     * DOS sees only IF, but IF will never really set in the CPU
       flagregister, because this would block Linux. So Linus maintains a
       virtual IF (VIF), which he sets and resets accordingly to CLI,
       STI, POPFx, IRETx. Because the DOS programm cannot look directly
       into the flagregister (exception the low 8 bits, but the IF is in
       bit 9), it does not realize, that the IF isn't set. To see it, it
       has to perform PUSHF and look at the stack.
       Well, but PUSHF or INTxx is intercepted by vm86 and then Linus
       looks at his virtual IF to set the IF on the stack. Also, if IRETx
       or POPFx happen, Linus is getting the IF from the stack, sets VIF
       accordingly, but leaves the real IF untouched.
     * Now, how is this realized? This is a bit more complicated. We have
       3 places were the eflag register is stored in memory:

        vm86s.regs.eflags
                in user space, seen by dosemu

        current->tss.v86flags
                virtual flags, macro VEFLAGS

        current->tss.vm86_info->regs.eflags
                the real flags, CPU reg. VM86

       The last one is a kernel space copy of vm86_struct.regs.eflags.
       Also there are some masks to define, which bits of the flag should
       be passed to DOS and which should be taken from DOS:

        current->tss.v86mask
                CPU model dependent bits

        SAFE_MASK (0xDD5)
                used the way to DOS

        RETURN_MASK (0xDFF)
                used the way from DOS

       When sys_vm86 is entered, it first makes a copy of the whole
       vm86_struct vm86s (containing regs.eflags) and saves a pointer to
       it to current->tss.vm86_info. It merges the flags from 32-bit user
       space (NOTE: IF is always set) over SAFE_MASK and
       current->tss.v86mask into current->tss.v86mask. From this point
       on, all changes to VIF, VIP (virtual interrupt pending) are only
       done in VEFLAGS. To handle the flag setting there are macros
       within vm86.c, which do the following:

        set_IF, clear_IF
                only modifies VEFLAGS;

        clear_TF
                sets a bit in the real flags;

        set_vflags(x)
                set flags x over SAFE_MASK to real flags (IF is not
                touched)

        x=get_vflags
                returns real flags over RETURN_MASK and translates VIF of
                VEFLAGS to IF in x;

       When it's time to return from vm86() to user space, the real flags
       are merged together with VIF and VIP from VEFLAGS and put into the
       userspace vm86s.regs.eflags. This is done by save_v86_state() and
       this does not translate the VIF to IF, it should be as it was on
       entry of sys_vm86: set to 1.
     * Now what are we doing with eflags in dosemu ? Well, this I don't
       really know. I saw IF used (told it Larry), I saw VIF tested an
       set, I saw TF cleared, and NT flag e.t.c.
       But I know what Linus thinks that we should do: Always interpret
       and set VIF, and let IF untouched, it will nevertheless set to 1
       at entry of sys_vm86.
       How I think we should proceed? Well this I did describe in my last
       mail.
       ,,,, and this from a follow-up mail:
       NOTE VIF and VIP in DOS-CPU-flagsregister are inherited from
       32-bit, so actually they are both ZERO.
       On return to 32-bit, only VIF will appear in vm86s.regs.eflags !
       VIP will be ZERO, again: VIP will be used only once !!!!
       [ ,,, ]
       I have to add, that VIP will be cleared, because it is not in any
       of the masks of vm86.
     _________________________________________________________________

5. New Keyboard Code

   This file describes the keyboard code which was written in 1999

   It was last updated by Eric Biederman <ebiederm@xmission.com> on 22
   April 2000
     _________________________________________________________________

5.1. Whats New

   What's new in the new keyboard code?

   Virtually all of the interface code gets keystrokes has been
   rewritten. While the actual emulation of the hardware has been fairly
   static.

   To the user:

     * The terminal interface has been internationalized.
     * Keymaps can now be written in unicode making them character set
       independant.
     * On non-us keyboard layouts the scan codes should always be correct
       now.
     * The X { keycode } option is now fully supported and portable, to
       any X server that implements the X keyboard extension.

   To the dosemu hacker:

     * There is a compile-time option USE_UNICODE_KEYB (on by default) to
       active the While the old code already claimed to be
       "client-server" (and was, to some extent), the new code introduces
       a clean, well-defined interface between the `server', which is the
       interface to DOS (int9, bios etc.), and the `clients', which are
       the interfaces to the user frontends supported by dosemu.
       Currently, clients are `raw', `slang' (i.e. terminal), and `X'.
       Clients send keystrokes to the server through the interface
       mentioned above (which is defined in "keyboard.h"), the most
       important functions being `putkey()' and `putrawkey()'.
     * The keyboard server was rewritten from scratch, the clients were
       heavily modified.
     * There is now general and efficient support for pasting large text
       objects. Simply call paste_text().
     * The keyboard-related code is now largely confined to
       base/keyboard, rather than scattered around in various files.

   There is a compile-time option NEW_KBD_CODE (on by default) to
   activate the new keyboard code. Once the new code is reasonably well
   tested I'll remove it.

   Just like the old keyboard code, we still have the rawkeyboard=on/off
   modes. The keybint=on/off modes have gone away.
     _________________________________________________________________

5.2. Status

   Almost everything seems to work well now.

   The keyboard server should now quite accurately emulate all key
   combinations described the `MAKE CODES' & `SCAN CODES' tables of
   HelpPC 2.1, which I used as a reference.

   See below for a list of known bugs.

   What I need now is YOUR beta-testing... please go ahead and try if all
   your application's wierd key combinations work, and let me know if
   they don't.
     _________________________________________________________________

5.3. Known bugs & incompatibilites

     * behaviour wrt. cli/sti is inaccurate, because the PIC code
       currently doesn't allow un-requesting if IRQ's.
     * emulation of special 8042 and keyboard commands is incomplete and
       probably still somewhat faulty.
     * the 'internal' keyboard flags in seg 0x40, like E0 prefix received
       etc. are never set. This shouldn't hurt, for all but the most
       braindead TSRs.
     * the Pause key works in terms of raw scancodes, however it's
       function is not implemented (i.e. it doesn't actually halt DOS
       execution.)
     * If the interrupt is not acknowledged and the keyboard port is read
       we don't eventually give up like a real keyboard and deliver the
       next byte in the keyboard buffer.
     _________________________________________________________________

5.4. TODO

     * Implement better multinational cut/paste in X
     * Implement timeouts on the length of type a byte is available in
       the keyboard data port.
     * implement pause key
     * once everything is proved to work, remove the old keyboard code
     * impelemnt utf8 and possibly iso2022 terminal support
     _________________________________________________________________

6. Old Keyboard Code

   This file describes the old keyboard code which was written in late
   '95 for scottb's dosemu-0.61, and adapted to the mainstream 0.63 in
   mid-'96.

   It was last updated by R.Zimmermann
   <zimmerm@mathematik.uni-marburg.de> on 18 Sep 96 and updated by Hans
   <lermen@fgan.de> on 17 Jan 97. ( correction notes marked *HH -- Hans )
     _________________________________________________________________

6.1. Whats New

   What's new in the new keyboard code? A lot.

   To the user:

     * Most of the keyboard-related bugs should have gone away. Hope I
       didn't introduce too many new ones (-: Keyboard emulation should
       be more accurate now; some keys are supported that weren't before,
       e.g. Pause.
     * The X { keycode } option is now obsolete. This was basically a bad
       hack to make things work, and was incompatible to X servers other
       than XFree86.

   To the dosemu hacker:

     * While the old code already claimed to be "client-server" (and was,
       to some extent), the new code introduces a clean, well-defined
       interface between the `server', which is the interface to DOS
       (int9, bios etc.), and the `clients', which are the interfaces to
       the user frontends supported by dosemu. Currently, clients are
       `raw', `slang' (i.e. terminal), and `X'.
       Clients send keystrokes to the server through the interface
       mentioned above (which is defined in "keyboard.h"), the most
       important functions being `putkey()' and `putrawkey()'.
     * The keyboard server was rewritten from scratch, the clients were
       heavily modified.
     * There is now general and efficient support for pasting large text
       objects. Simply call paste_text().
     * The keyboard-related code is now largely confined to
       base/keyboard, rather than scattered around in various files.

   There is a compile-time option NEW_KBD_CODE (on by default) to
   activate the new keyboard code. The old stuff is still in there, but I
   haven't recently checked whether it still works, or even compiles.
   Once the new code is reasonably well tested I'll remove it. ( *HH: the
   old code is made workeable and remains ON per default, it will stay
   maintained for a while, so we can easily check where the bugs come
   from )
     _________________________________________________________________

6.2. Status

   Almost everything seems to work well now.

   The keyboard server should now quite accurately emulate all key
   combinations described the `MAKE CODES' & `SCAN CODES' tables of
   HelpPC 2.1, which I used as a reference.

   See below for a list of known bugs.

   What I need now is YOUR beta-testing... please go ahead and try if all
   your application's wierd key combinations work, and let me know if
   they don't.
     _________________________________________________________________

6.3. Keyboard server interface

   This is all you should need to know if you just want to send
   keystrokes to DOS.

   Use the functions

     * putrawkey(t_rawkeycode code);
     * putkey(Boolean make, t_keysym key)
     * putkey_shift(Boolean make, t_keysym key, t_shiftstate shiftstate)

   You may also read (but not write!) the variable 'shiftstate' if
   necessary.

   ehm... see the DANG comments in base/newkbd-server.c for more
   information...

   NOTE: the server's queue is limited to a relatively small number of
   keyboard events (currently 15). IMO, it is not a good idea to let the
   queue be arbitrarily long, as this would render the behaviour more
   incontrollable if the user typed a lot of mad things while a dos
   program wasn't polling the keyboard.

   For pasting, there is special support in base/keyboard/keyb_clients.c
   which runs on top of the server.
     _________________________________________________________________

6.4. Keyboard server structure

   [NOTE: you won't need to read this unless you actually want to modify
   the keyboard server code. In that case, however, you MUST read it!]

   [Note: I'll have to update this. The queue backend works somewhat
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
   shift state after this event was processed. Note that the bios_key
   field can be empty (=0), e.g. for shift keys, while the raw field
   should always contain something.
     _________________________________________________________________

6.4.1. queue handling functions

     * static inline Boolean queue_empty(void);
     * static inline void clear_queue(void);
     * static inline void write_queue(Bit16u bios_key,t_shiftstate
       shift,Bit32u raw);
     * static void read_queue(Bit16u *bios_key, t_shiftstate *shift,
       t_rawkeycode *raw);

   Accordingly, the keyboard code is largely divided into two parts,

     * the 'front end' of the queue, responsible for translating keyboard
       events into the 'queue entry' format.
     * the 'back end' of the queue, which reads the queue and sends
       keycodes to DOS
     _________________________________________________________________

6.4.2. The Front End


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
     _________________________________________________________________

6.4.2.1. Functions in serv_xlat.c

     * static Boolean do_shift_keys(Boolean make, t_keysym key);
     * static Bit16u make_bios_code(Boolean make, t_keysym key, uchar
       ascii);
     * static uchar translate(t_keysym key);
     * static Boolean handle_dosemu_keys(t_keysym key);
     * void putrawkey(t_rawkeycode code);
     * void putkey(Boolean make, t_keysym key, uchar ascii);
     * void putkey_shift(Boolean make, t_keysym key, uchar ascii,
       t_shiftstate s);

   Any keyboard client or other part of dosemu wishing to send keyboard
   events to DOS will do so by calling one of the functions putrawkey,
   putkey, and putkey_shift.
     _________________________________________________________________

6.4.2.1.1. putrawkey

   is called with a single raw scancode byte. Scancodes from subsequent
   calls are assembled into complete keyboard events, translated and
   placed into the queue.
     _________________________________________________________________

6.4.2.1.2. putkey & others

   ,,,to be documented.
     _________________________________________________________________

6.4.3. The Back End

6.4.3.1. Queue Back End


                       EMULATOR SIDE        |    x86 SIDE
                                        |
                          ....[through PIC].|....................
                          :                 |           :        v
    QUEUE      .....> out_b_8042() --> [ port 60h ] ----:---> other_int9_handle
r
    |         :                             |        \  `.......    (:) (|)
    |         :                             |         \         v   (v) (|)
    +->int_chk_q()-> bios_buffer----> [ get_bios_key ]-----> default_int9_handl
er
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
     _________________________________________________________________

6.4.3.2. Functions in newkbd-server.c

     * void do_irq1();
     * void clear_keybuf();
     * static inline Boolean keybuf_full(void);
     * static inline void put_keybuf(Bit16u scancode);
     * void copy_shift_state(t_shiftstate shift);
     * static void kbd_process(void);

   Transfer of the keyboard events from the dosemu queue to DOS is done
   as follows:

   As soon as a key is stored into the empty queue, kbd_process()
   triggers IRQ1 through the PIC emulation, which some time later will
   call do_irq1().

   do_irq1() will prepare for the interrupt execution by reading from the
   queue and storing the values in the variables raw_buffer,
   shiftstate_buffer, and bios_buffer, and then call run_irq() to run the
   actual DOS interrupt handler.

   There are two cases:

     * the default int09 handler in the dosemu bios (base/bios_emu.S)
       will call the helper function get_bios_key(), which returns the
       translated bios keycode from bios_buffer and copies the shiftstate
       from shiftstate_buffer. The raw keycodes are not used.
       get_bios_key() may also return 0 if no translated keycode is
       ready.
       The int9 handler will also call the `keyboard hook' int15h,
       ax=????.
     * if a dos application or TSR has redirected the keyboard interrupt,
       its handler might read from port 60h to get raw scancodes. Port
       60h is of course virtualized, and the read returns the value from
       raw_buffer.
       Note that a mix between the two cases is also possible, e.g. a
       TSR's int9 handler first reads port 60h to check if a particular
       key was pressed, then gives over to the default int9 handler. Even
       these cases should be (and are, I think) handled properly.
       Note also that in any case, int9 is called once for each raw
       scancode byte. Eg.,suppose the user pressed the PgDn key, whose
       raw scancode is E0 51:
          + first call to int9: read port 60h = 0xe0 read port 60h = 0xe0
            (**) call get_bios_key() = 0 iret do_irq1() reschedules IRQ1
            because further scancodes are in the queue
          + second call to int9 read port 60h = 0x51 call get_bios_key()
            = 0x5100 (bios scancode of PgDn) iret
       (** multiple port 60h reads during the same interrupt yield the
       same result.)

   This is not a complete documentation. If you actually want to hack the
   keyboard server, you can't avoid reading the code, I'm afraid ;-)
     _________________________________________________________________

6.5. Known bugs & incompatibilites

     * behaviour wrt. cli/sti is inaccurate, because the PIC code
       currently doesn't allow un-requesting if IRQ's.
     * emulation of special 8042 and keyboard commands is incomplete and
       probably still somewhat faulty.
     * the 'internal' keyboard flags in seg 0x40, like E0 prefix received
       etc. are never set. This shouldn't hurt, for all but the most
       braindead TSRs.
     * CAPS LOCK uppercase translation may be incorrect for some
       (non-german) national characters.
     * typematic codes in X and non-raw modes are Make+Break, not just
       Make. This shouldn't hurt, though.
     * in X mode, shift+Gray cursor keys deliver numbers if NumLock is
       off. This is an X problem, and AFIK nothing can be done about it.
     * in X, something may be wrong with F11+F12 handling (and again,
       possibly necessarily wrong).
     * the Pause key works in terms of raw scancodes, however it's
       function is not implemented (i.e. it doesn't actually halt DOS
       execution.)
     * in terminal (i.e. slang) mode, several things might be wrong or at
       least improveable.
     * there is no difference between the int16h functions 0,1 and the
       extended functions 0x10,0x11 - i.e. 0,1 don't filter out extended
       keycodes.
     * keyb.exe still doesn't work (hangs) - most probably due to the
       above.
     _________________________________________________________________

6.6. Changes from 0.61.10

     * adapted to 0.63.55
     * adapted to 0.63.33
     * renamed various files
     * various minor cleanups
     * removed putkey_shift, added set_shiftstate
     * in RAW mode, read current shiftstate at startup
     * created base/keyboard/keyb_client.c for general client
       initialisation and paste support.
     _________________________________________________________________

6.7. TODO

     * find what's wrong with TC++ 1.0
     * implement pause key
     * adapt x2dos (implement interface to send keystrokes from outside
       dosemu)
     * once everything is proved to work, remove the old keyboard code
     _________________________________________________________________

7. Setting HogThreshold

   Greetings DOSEMU fans,

   Hogthreshold is a value that you may modify in your DOSEMU.CONF file.
   It is a measure of the "niceness" of Dosemu. That is to say, it
   attempts to return to Linux while DOS is 'idling' so that DOSEMU does
   not hog all the CPU cycles while waiting at the DOS prompt.

   Determining the optimal Hogthreshold value involves a little bit of
   magic (but not so much really.) One way is to try different values and
   look at the 'top' reading in another console. Setting the value too
   low may mildly slow Dosemu performance. Setting the value too high
   will keep the idling code from working.

   That said, a good basic value to try is "half of your Bogo-Mips
   value". (The Bogo-Mips value is displayed when the kernel is booting,
   it's an imaginary value somewhat related to CPU performance.)

   Setting the value to 0 will disable idling entirely. The default value
   is 10.

   This files is some kind of FAQ on how to use the 'HogThreshold' value
   in the dosemu config file.

   In case you have more questions feel free to ask me (
   <andi@andiunx.m.isar.de>).

   Those of you who simply want to have DOSEMU running at highest
   possible speed simply leave the value to zero, but if you are
   concerned about DOSEMU eating too much CPU time it's worth playing
   with the HogThreshold value.

   Why do I need to set the HogThreshold value, why can't DOSEMU just
          stop if it is waiting for a keystroke ?
          The reason is the way how DOS and a lot of applications have
          implemented `waiting for a keystroke'.

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

          This means that the application is busy waiting for the
          keystroke.

   What is a good value for HogThreshold to start with ?
          On a 40 MHZ 486 start with a value of 10. Increase this value
          if you to have your DOS application run faster, decrease it if
          you think too much CPU time is used.

   It does not work on my machine.
          You need to have at least dosemu0.53pl40 in order to have the
          anti-hog code in effect.

   Why not simply use a very low value of "HogThreshold" ? Do I really
          have to try an individual value of HogThreshold ?
          This would slow down your DOS application. But why not, DOS is
          slow anyway :-).

   How do I found out about CPU usage of DOSEMU ?
          Simply use `top'. It displays cpu and memory usage.

   P.S. If you want to change the HogThreshold value during execution,
   simply call
     mov al,12h
     mov bx,the_new_value
     int e6h

   This is what speed.com does. If you are interested, please take a look
   at speed.c.

   Notes: If your application is unkind enough to do waits using an
   int16h fcn 1h loop without calling the keyboard idle interrupt (int
   28h), this code is not going to help much. If someone runs into a
   program like this, let me ( <scottb@eecs.nwu.edu> ) know and I'll
   rewrite something into the int16 bios.
     _________________________________________________________________

8. Privileges and Running as User

   This section written by Hans Lermen <lermen@fgan.de> , Apr 6, 1997.
   And updated by Eric Biederman <ebiederm+eric@npwt.net> 30 Nov 1997.
     _________________________________________________________________

8.1. What we were suffering from

   Well, I got sick with the complaints about 'have problems running as
   user' and did much effort to 'learn' about what we were doing with
   priv-settings. To make it short: It was a real mess, there was the
   whole dosemu history of different strategies to reach the same aim.
   Well, this didn't make it clearer. I appreciate all the efforts that
   were done by a lot of people in the team to make that damn stuff
   working, and atleast finding all those places where we need to handle
   privs was a great work and is yet worth. ... but sorry, how it was
   handled didn't work.

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
   compile time option and 'run as user' the default one.

   (promise: I'll also will run dosemu as user before releasing, so that
   I can see, if something goes wrong with it ;-)

   Enter Eric:

   What we have been suffering from lately is that threads were added to
   dosemu, and the stacks Hans added when he did it 'the right way (tm)'
   were all of a sudden large static variables that could not be kept
   consistent. That and hans added caching of the curent uids for
   efficiency in dosemu, again more static variables.

   When I went through the first time and added all of the strange
   unmaintainable things Hans complains of above, and found where
   priveleges where actually needed I hand't thought it was as bad as
   Hans perceived it, so I had taken the lazy way out. That and my main
   concern was to make the privelege setting consistent enough to not
   give mysterous erros in dosemu. But I had comtemplated doing it 'the
   right way (tm)' and my scheme for doing it was a bit different from
   the way Hans did it.

   With Hans the stack was explicit but hidden behind the scenes. With my
   latest incarnation the stack is even more explicit. The elements of
   the privelege stack are now local variables in subroutines. And these
   local variables need to be declared explicitly in a given subroutine.
   This method isn't quite a fool proof as Han's method, but a fool could
   mess up Hans's method up as well. And any competent person should be
   able to handle a local variable, safely.

   For the case of the static cached uid I have simply placed them in the
   thread control block. The real challenge in integrating the with the
   thread code is the thread code was using root priveleges and changing
   it's priveleges with the priv code during it's initialization. For the
   time being I have disabled all of the thread code's mucking with root
   priveleges, and placed it's initialization before the privelege code's
   initialization. We can see later if I can make Han's thread code work
   `the right way (tm)' as he did for my privelege code.

   ReEnter Hans: ;-)

   In order to have more checking wether we (not an anonymous fool) are
   forgetting something on the road, I modified Erics method such that
   the local variable is declared by a macro and preset with a magic,
   that is checked in priv.c. The below explanations reflect this change.

   ReReEnter Hans (2000/02/28): ;-)

   The treads code has definitively gone (no one made serious use of it),
   so the above thread related arguments too.
     _________________________________________________________________

8.2. The new 'priv stuff'

   This works as follows

     * All settings have to be done in 'pairs' such as

            enter_priv_on();   /* need to have root access for 'do_something' *
/
            do_something();
            leave_priv_setting();

       or

            enter_priv_off();  /* need pure user access for 'do_something' */
            do_something();
            leave_priv_setting();

     * On enter_priv_XXX() the current state will be saved (pushed) on a
       local variable on the stack and later restored from that on
       leave_priv_setting(). This variable is has to be defined at entry
       of each function (or block), that uses a enter/leave_priv bracket.
       To avoid errors it has to be defined via the macro PRIV_SAVE_AREA.
       The 'stack depth' is just _one_ and is checked to not overflow.
       The enter/leave_priv_* in fact are macros, that pass a pointer to
       the local privs save area to the appropriate 'real_' functions. (
       this way, we can't forget to include priv.h and PRIV_SAVE_AREA )
       Hence, you never again have to worry about previous priv settings,
       and whenever you feel you need to switch off or on privs, you can
       do it without coming into trouble.
     * We now have the system calls (getuid, setreuid, etc.) only in
       src/base/misc/priv.c. We cash the setting and don't do unnecessary
       systemcalls. Hence NEVER call 'getuid', 'setreuid' etc. yourself,
       instead use the above supplied functions. The only places where I
       broke this 'holy law' myself was when printing the log, showing
       both values (the real and the cached one).
     * In case of dosemu was started out of a root login, we skip all
       priv-settings. There is a new variable 'under_root_login' which is
       only set when dosemu is started from a root login.
     * On all places were iopl() is called outside a
       enter_priv...leave_priv bracket, the new priv_iopl() function is
       to be used in order to force privileges.

   This is much cleaner and 'automagically' also solves the problem with
   the old 'priv_off/default' sheme. Because, 'running as user' just
   needs to do _one_ priv_off at start of dosemu, if we don't do it we
   are 'running as root' ;-)

   A last remark: Though it may be desirable to have a non suid root
   dosemu, this is not possible. Even if we get GGI in the official
   kernel, we can't solve the problems with port access (which we need
   for a lot of DOS apps) ... and ioperm() / iopl() need root privileges.
   This leads to the fact that 'i_am_root' will be always '1', makeing it
   a macro gives GCC a changes optimize away the checks for it. We will
   leave 'i_am_root' in, maybe there is some day in the future that gives
   us a kernel allowing a ports stuff without root privilege, ... never
   say never ;-)

   Enter Eric:

   The current goal is to have a non suid-root dosemu atleast in X,
   lacking some features. But the reason I disabled it when I first
   introduced the infamous in this file priv_on/priv_default mechanism is
   that it hasn't been tested yet, still stands. What remains to do is an
   audit of what code _needs_ root permissions, what code could benefit
   with a sgid dosemu, and what code just uses root permissions because
   when we are suid root some operations can only been done as root.

   When the audit is done (which has started with my most recent patch (I
   now know what code to look at)). It should be possible to disable
   options that are only possible when we are suid root, at dosemu
   configuration time. Then will be the task of finding ways to do things
   without root permissions.

   Enter Hans (960626):

   Last act of this story: DOSEMU now can run non-suid root and though it
   looses a lot of features because of not beeing able to run on console
   in this mode or not able to do any ports stuff e.t.c., its quite
   useable under X (I was even able to run win31, hence DPMI is secure
   now with the s-bit off).

   The names of some variables where changed, such that it conforms more
   to what they now do: i_am_root became can_do_root_stuff, and
   under_root_login was splitted into the static skip_priv_setting, while
   the global valiable under_root_login keeps its meaning.

   skip_priv_setting is set for both: running as root and running as user
   non-suid root. can_do_root_stuff is global and used in the same places
   were i_am_root was used before.

   On start of DOSEMU, we now call parse_dosemu_users() directly after
   priv_init(), and this is top of main(). This way we catch any hacker
   attack quite early and should be much securer as ever before.

   Additionally, parse_dosemu_users now checks for the keyword
   `nosuidroot' in /etc/dosemu.users and if this is found for a user,
   this user is not allowed to execute DOSEMU on a suid root binary
   (though he can use a non-suid root copy of it) and dosemu will exit in
   this case. Dropping all priviledges on a suid root binary and continue
   (as if the -s bit wasn't set) is not possible, because /proc/self/mem
   has the ownership of the suid binary (hence root) and dosemu would
   crash later, when trying to open it. Do a chown on /proc/self/mem
   doesn't work either.

   The last action to make the non-suid root stuff working, was moving
   the files from /var/run/* to some user accessable location. This now
   is ~/.dosemu/* and dosdebug was adapted to support that.
     _________________________________________________________________

9. Timing issues in dosemu

   This section written by Alberto Vignani <vignani@mbox.vol.it> , Aug
   10, 1997
     _________________________________________________________________

9.1. The 64-bit timers

   The idea for the 64-bit timers came, of course, from using the pentium
   cycle counter, and has been extended in dosemu to the whole timing
   system.

   All timer variables are now defined of the type hitimer_t (an unsigned
   long long, see include/types.h).

   Timers in dosemu can have one of the following resolutions:

        MICROSECOND, for general-purpose timing
        TICK (838ns), for PIT/PIC emulation

   You can get the current time with the two functions

        GETusTIME    for 1-usec resolution
        GETtickTIME  for 838ns resolution

   The actual timer used (kernel or pentium TSC) is configured at runtime
   (it defaults to on for 586 and higher CPUs, but can be overridden both
   by the -[2345] command-line switch or by the "rdtsc on|off" statement
   in the config file).

   The DOS time is now kept in the global variable pic_sys_time. See the
   DANG notes about emu-i386/cputime.c for details.
     _________________________________________________________________

9.2. DOS 'view of time' and time stretching

   The time stretcher implements DOS 'view of time', as opposed to the
   system time. It would be very easy to just give DOS its time, by
   incrementing it only when the dosemu process is active. To do that, it
   is enough to get SIGPROF from the kernel and (with some adjustments
   for sleeps) calculate the CPU% used by dosemu. The real tricky part
   comes when we try to get this stuff back in sync with real time. We
   must 'stretch' the time, slowing it down and speeding it up, so that
   the DOS time is always trying to catch the real one.

   To enable the time stretcher start dosemu with the command-line option
   -t. If your CPU is not a 586 or higher, dosemu will exit with an error
   message. Not that this algorithm doesn't run on a 486 - it was tested
   and was quite successful - but the use of gettimeofday() makes it a
   bit too heavy for such machines. Hence, it has been restricted to the
   pentium-class CPUs only, which can use the CPU timer to reduce use of
   kernel resources.
     _________________________________________________________________

9.3. Non-periodic timer modes in PIT

   Normally, PIT timer 0 runs in a periodic mode (mode 2 or 3), it counts
   down to 0 then it issues an int8 and reinitializes itself. But many
   demos and games use it in one of the non-periodic (NP) modes (0 or 1):
   the timer counts down to 0, issues the interrupt and then stops. In NP
   modes, software intervention is required to keep the timer running.
   The NP modes were not implemented by dosemu-0.66, and this is the
   reason why some programs failed to run or required strange hacks in
   the dosemu code. To be fair, most of these games ran also in
   dosemu-0.66 without any specific hack, but looking at the debug log
   always made me call for some type of divine intervention :-)

   The most common situation is: the DOS program first calibrates itself
   by calculating the time between two vertical screen retraces, then
   programs the timer 0 in a NP mode with a time slightly less than this
   interval; the int8 routine looks at the screen retrace to sync itself
   and then restarts the timer. You can understand how this kind of setup
   is very tricky to emulate in a multitasking environment, and sometimes
   requires using the emuretrace feature. But, at the end, many demos
   that do not run under Win95 actually run under dosemu.
     _________________________________________________________________

9.4. Fast timing

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
     _________________________________________________________________

9.5. PIC/PIT synchronization and interrupt delay

   Another tricky issue... There are actually two timing systems for
   int8, the one connected to the interrupt in PIC, the other to port
   0x43 access in PIT. Things are not yet perfectly clean, but at least
   now the two timings are kept in sync and can help one another.

   One of the new features in PIC is the correct emulation of interrupt
   delay when for any reason the interrupt gets masked for some time; as
   the time for int8 is delayed, it loses its sync, so the PIT value will
   be useful for recalculating the correct int8 time. (This case was the
   source for many int bursts and stack overflows in dosemu-0.66).

   The contrary happens when the PIT is in a NP mode; any time the timer
   is restarted, the PIT must be reset too. And so on.
     _________________________________________________________________

9.6. The RTC emulation

   There is a totally new emulation of the CMOS Real Time Clock, complete
   with alarm interrupt. A process ticks at exactly 1sec, always in real
   (=system) time; it is normally implemented with a thread, but can be
   run from inside the SIGALRM call when threads are not used.

   Also, int0x1a has been mostly rewritten to keep in sync with the new
   CMOS and time-stretching features.
     _________________________________________________________________

9.7. General warnings

   Do not try to use games or programs with hi-freq timers while running
   heavy tasks in the background. I tried to make dosemu quite robust in
   such respects, so it will probably NOT crash, but, apart being
   unplayable, the game could behave in unpredictable ways.
     _________________________________________________________________

10. Pentium-specific issues in dosemu

   This section written by Alberto Vignani <vignani@mbox.vol.it> , Aug
   10, 1997
     _________________________________________________________________

10.1. The pentium cycle counter

   On 586 and higher CPUs the 'rdtsc' instruction allows access to an
   internal 64-bit TimeStamp Counter (TSC) which increments at the CPU
   clock rate. Access to this register is controlled by bit 2 in the
   config register cr4; hopefully under Linux this bit is always off,
   thus allowing access to the counter also from user programs at CPL 3.

   The TSC is part of a more general group of performance evaluation
   registers, but no other feature of these registers is of any use for
   dosemu. Too bad the TSC can't raise an interrupt!

   Advantages of the TSC: it is extremely cheap to access (11 clock
   cycles, no system call).

   Drawbacks of using the TSC: you must know your CPU speed to get the
   absolute time value, and the result is machine-class specific, i.e.
   you can't run a binary compiled for pentium on a 486 or lower CPU
   (this is not the case for dosemu, as it can dynamically switch back to
   486-style code).
     _________________________________________________________________

10.2. How to compile for pentium

   There are no special options required to compile for pentium, the CPU
   selection is done at runtime by parsing /proc/cpuinfo. You can't
   override the CPU selection of the real CPU, only the emulated one.
     _________________________________________________________________

10.3. Runtime calibration

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
     _________________________________________________________________

10.4. Timer precision

   I found no info about this issue. It is reasonable to assume that if
   your CPU is specified to run at 100MHz, it should run at that exact
   speed (within the tolerances of quartz crystals, which are normally
   around 10ppm). But I suspect that the exact speed depends on your
   motherboard. If you are worried, there are many small test programs
   you can run under plain DOS to get the exact speed. Anyway, this
   should be not a very important point, since all the file timings are
   done by calling the library/kernel time routines, and do not depend on
   the TSC.
     _________________________________________________________________

10.5. Additional points

   The experimental 'time stretching' algorithm is only enabled when
   using the pentium (with or without TSC). I found that it is a bit
   'heavy' for 486 machines and disallowed it.

   If your dosemu was compiled with pentium features, you can switch back
   to the 'standard' (gettimeofday()) timing at runtime by adding the
   statement

           rdtsc off

   in your configuration file.
     _________________________________________________________________

11. The DANG system

11.1. Description

   The DANG compiler is a perl script which produces a linuxdoc-sgml
   document from special comments embedded in the DOSEMU source code.
   This document is intended to give an overview of the DOSEMU code for
   the benefit of hackers.
     _________________________________________________________________

11.2. Changes from last compiler release

     * Recognizes 'maintainer:' information.
     * '-i' (irritating) flag.
     * Corrections to sgml special character protection.
     _________________________________________________________________

11.3. Using DANG in your code

   THE FOLLOWING MAY NOT SOUND VERY NICE, BUT IS INTENDED TO KEEP DANG AS
   IT WAS DESIGNED TO BE - A GUIDE FOR NOVICES.

   DANG is not intended to be a vehicle for copyrights, gratuitous plugs,
   or anything similar. It is intended to guide hackers through the maze
   of DOSEMU source code. The comments should be short and informative. I
   will mercilessly hack out comments which do not conform to this. If
   this damages anything, or annoys anyone, so be it.

   I spent hours correcting the DOSEMU 0.63.1 source code because some
   developers didn't follow the rules. They are very simple, and I'll
   remind you of the below. (I am here to co-ordinate this, not do the
   work. The comments are the responsibility of all of us.)
     _________________________________________________________________

11.4. DANG Markers

   Some initial comments:

     * All the text is passed through a text formatting system. This
       means that text may not be formatted how you want it to be. If you
       insist on having something formatted in a particular way, then it
       probably shouldn't be in DANG. Try a README instead. Embedding
       sgml tags in the comment will probably not work.
     * Copyrights and long detailed discussions should not be in DANG. If
       there is a good reason for including a copyright notice, then it
       should be included ONCE, prefereably in the group summary file
       (see './src/doc/DANG/DANG_CONFIG' for the locations)
     * If I say something must be done in a particular way, then it MUST.
       There is no point in doing it differently because the script will
       not work correctly (and changing the script won't help anyone
       else.) In most cases the reason for a particular style is to help
       users who are actually reading the SOURCE file.
     _________________________________________________________________

11.4.1. DANG_BEGIN_MODULE / DANG_END_MODULE

   These should enclose a piece of summary text at the start of a file.
   It should explain the purpose of the file. Anything on the same line
   as DANG_BEGIN_MODULE is ignored. A future version may use the rest of
   this line to provide a user selected name for the module. There may be
   any number of lines of text. To include information on the
   maintainer(s) of a module, put 'maintainer:' on a blank line. The
   following lines should contain the maintainers details in form:

       name ..... <user@address>

   There should only be one maintainer on a line. There can be no other
   lines of description before DANG_END_MODULE.

   Example:
    /*
     * DANG_BEGIN_MODULE
     *
     * This is the goobledygook module. It provides BAR services. Currently the
re
     * are no facilities for mixing a 'FOO' cocktail. The stubs are in 'mix_foo
()'.
     *
     * Maintainer:
     *  Alistair MacDonald      <alistair@slitesys.demon.co.uk>
     *  Foo Bar                 <foobar@inter.net.junk>
     *
     * DANG_END_MODULE
     */
     _________________________________________________________________

11.4.2. DANG_BEGIN_FUNCTION / DANG_END_FUNCTION

   This is used to describe functions. Ideally this should only be
   complicated or important functions as this avoids cluttering DANG with
   lots of descriptions for trivial functions. Any text on the same line
   as 'DANG_BEGIN_FUNCTION' is taken as the name of the function.

   There are two optional sub-markers: 'description:' & 'arguments:'
   These can be included at any point, and the default marker is a
   description. The difference is that arguments are converted into a
   bulleted list. For this reason, there should be only one argument (&
   it's description) per line. I suggest that arguments should be in the
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
     _________________________________________________________________

11.4.3. DANG_BEGIN_REMARK / DANG_END_REMARK

   This is used to provide in-context comments within the code. They can
   be used to describe some particularly interesting or complex code. It
   should be borne in mind that this will be out of context in DANG, and
   that DANG is intended for Novice DOSEMU hackers too ...

   Example:
    /*
     * DANG_BEGIN_REMARK
     *
     * We select the method of preparation of the cocktail, according to the ty
pe
     * of cocktail being prepared. To do this we divide the cocktails up into :
     * VERY_ALCHOHOLIC, MILDLY_ALCHOHOLIC & ALCOHOL_FREE
     *
     * DANG_END_REMARK
     */
     _________________________________________________________________

11.4.4. DANG_BEGIN_NEWIDEA / DANG_END_NEWIDEA

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
     _________________________________________________________________

11.4.5. DANG_FIXTHIS

   This is for single line comments on an area which needs fixing. This
   should say where the fix is required. This may become a multi-line
   comment in the future.

   Example:
    /* DANG_FIXTHIS The mix_foo() function should be written to replace the stu
b */
     _________________________________________________________________

11.4.6. DANG_BEGIN_CHANGELOG / DANG_END_CHANGELOG

   This is not currently used. It should be placed around the CHANGES
   section in the code. It was intended to be used along with the DPR.

   * No Example *
     _________________________________________________________________

11.5. Usage

   The current version of the configuration file resides in
   './src/doc/DANG'. The program needs to be told where to find this
   using '-c <file>'. On my system I run it in the './src/doc/DANG'
   directory using './DANG_c.pl -c DANG_CONFIG', but its really up to
   you.

   The other options are '-v' for verbose mode and '-i'. -v isn't really
   useful, but it does tell you what it is doing at all times. -i is
   'irritating mode' and makes comments in the DANG file about missing
   items, such as no MODULE information, no FUNCTION information, etc.

   If the program is executed with a list of files as parameters then the
   files will be processed, but no sgml output produced. This can be used
   for basic debugging, as it warns you when there are missing BEGIN/END
   markers, etc.
     _________________________________________________________________

11.6. Future

   I have vaguelly started writing the successor of this program. This
   will be a more general program with a more flexible configuration
   system. Don't hold your breath for an imminent release though. The new
   version will make it easier to change the markers & document
   structure, as well as providing feedback on errors.
     _________________________________________________________________

12. mkfatimage -- Make a FAT hdimage pre-loaded with files

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
     _________________________________________________________________

13. mkfatimage16 -- Make a large FAT hdimage pre-loaded with files

   I have also attached (gzipped in MIME format) mkfatimage16.c, a
   modified version of mkfatimage which takes an additional switch [-t
   number_of_tracks] and automatically switches to the 16-bit FAT format
   if the specified size is > 16MB. I have deleted the "real" DOS
   partition from my disk, and so I am now running an entirely "virtual"
   DOS system within a big hdimage!
     _________________________________________________________________

14. Documenting DOSEMU

   Effective from dosemu-1.0.1 the documentation format for DOSEMU is
   DocBook. Currently this is DocBook 3.0, but formatted in lower case
   for XML compatibility. This will probably be migrated to DocBook 4,
   but the changes are unlikely to affect most authors.

   Every programmer can handle the few basic DocBook commands that are
   need to make some really good documents! There are many similarities
   with HTML, which is hardly surprising as they are both SGML narkup
   languages. The source to this document may make useful reading (This
   is the file ./src/doc/README/doc)
     _________________________________________________________________

14.1. Sections

   There are 5 section levels you can use. They are all automatically
   numbered. Your choices are:

   <sect1>
          A top level section. This is indexed.

   <sect2>
          A 2nd level section. This is indexed.

   <sect3>
          A 3rd level section. This is indexed.

   <sect4>
          A 4th level section. This is not indexed.

   <sect5>
          A 5th level section. This is not indexed.

     * You cannot skip section levels on the way up or down (ie you must
       go <sect>,<sect1>,<sect2> and not <sect>,<sect2>). You must also
       close each section when you finish it. This includes the enclosing
       sections.
     * The title of the section must be enclosed within
       <title>...</title> tags.
     * Each paragraph of text must be enclosed within <para>...</para>.
     _________________________________________________________________

14.2. Emphasising text

   There is only one way to do this.

   <em>...</em>
          Emphasises text like this.
     _________________________________________________________________

14.3. Lists

   Here we have 3 useful types:

   itemizedList
          A standard bulletted list

   orderedList
          A "numbered" list

   variableList
          A description list (like this)

   For ``itemizedList'' and ``orderedList'' the items are marked with
   <listitem>. eg:
    <itemizedList>
    <listitem><para> item 1 </para></listitem>
    <listitem><para> item 2 </para></listitem>
    </itemize>

   For the ``variableList'' the items have two additional markers. Each
   entry in the list is enclosed in <varlistentry>...</varlistentry>.
   Each ``term'' is marked using <term>...</term> and then the content is
   marked with listitem, as above. eg:
    <variableList>
    <varlistentry><term>item 1</term><listitem><para> is here</para></listitem>
</varlistentry>
    <varlistentry><term>item 2</term><listitem><para> is here!</para></listitem
></varlistentry>
    </variableList>
     _________________________________________________________________

14.4. Quoting stuff

   If you want to quote a small amount use <literal>...</literal>. eg:
       This is <literal>./src/doc/README/doc</literal>

   To quote a large section, such as part of a file or an example use
   <screen>. eg:
    <screen>
    ...
    </screen>

     Note: You still need to ``quote'' any <, > or & characters although
     most other characters should be OK.
     _________________________________________________________________

14.5. Special Characters

   Obviously some characters are going to need to be quoted. In general
   these are the same ones as HTML. The most common ones are:

   <
          &lt;

   >
          &gt;

   &
          &amp;
     _________________________________________________________________

14.6. Cross-References & URLs

   One of the extra feature that this lets us do is include URLs and
   cross-references.
     _________________________________________________________________

14.6.1. Cross-References

   These have 2 parts: a label, and a reference.

   The label is <... id="...">. ie an id on the element you want to refer
   to.

   The reference is <xref linkend="..." >. The linkend should refer to an
   id somewhere else in the same document (not necessarily the same file
   as the README's consist of multiple files). The section naem will be
   inserted automatically at site of the <xref>.
     _________________________________________________________________

14.6.2. URLs

   This is <ulink url="...">...</ulink>. The url will be active only for
   HTML. The text will be used at all times. eg:

       See the <ulink url="http://www.dosemu.org/">DOSEMU website</ulink>.

   Which will appear as See the DOSEMU website.
     _________________________________________________________________

14.6.3. Email addresses

   Whilst it is possible to do insert email addresses as URLs there is a
   better way. Simply enclose the address in <email>...</email>. eg:

       <email>alistair@slitesys.demon.co.uk</email>
     _________________________________________________________________

14.7. References

   Here are a few other places to find information about writing using
   DocBook.

   http://www.docbook.org/tdg/
          ``DocBook: The Definitive Guide'' is a complete reference to
          DocBook. It is an O'Reilly book, which is also readable online.

   http://metalab.unc.edu/godoy/using-docbook/using-docbook.html
          This is a basic guide to DocBook.

   http://www.linuxdoc.org/HOWTO/HOWTO-HOWTO/index.html
          Part of the Linux Documentation Project (LDP) this is a guide
          to writing HOWTO's specifically, but covers DocBook in general
          as this is now the preferred MarkUp for the LDP.
     _________________________________________________________________

15. Sound Code

15.1. Current DOSEMU sound code

   Unfortunately I haven't documented this yet. However, the current code
   has been completely rewritten and has been designed to support
   multiple operating systems and sound systems.

   For details of the internal interface and any available patches see my
   WWW page at http://www.slitesys.demon.co.uk/a.macdonald/dosemu/sound/

   At a later stages the code was extensively reworked (again) by Stas
   Sergeev. Instead of direct pipes connected to /dev/dsp the DMA code
   now uses callbacks. If pipes will be advantageous for other types of
   DMA in the future, they will be readded. Opinions differ on this very
   subject (whether pipes are more flexible than callbacks in certain
   cases).
     _________________________________________________________________

15.2. Original DOSEMU sound code

        Copyright (C) 1995  Joel N. Weber II

        This sound code is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2 of the License, or
        (at your option) any later version.

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

   This is a very incomplete sound blaster pro emulator. I recomend you
   get the PC Game Programmer's Encycolpedia; I give the URL at the end
   of this document.

   The mixer emulator probably doesn't do the math precisely enough (why
   did Hannu use a round base 10 number when all the sound boards I know
   use base 2?) It ignores obscure registers. So repeatedly reading and
   writing data will zero the volume. If you want to fix this, send Hannu
   a message indicating that you want the volume to be out of 255. It
   will probably fix the problem that he advertises with his driver: if
   you read the volume and write it back as is, you'll get zero volume in
   the end. And on the obscure registers issue, it only pays attention to
   volumes and the dsp stereo bit. The filters probably don't matter
   much, but the record source will need to be added some day.

   The dsp only supports reset and your basic dma playpack. Recording
   hasn't been implemented, directly reading and writing samples won't
   work too well because it's too timing sensitive, and compression isn't
   supported.

   FM currently has been ignored. Maybe there's a PCGPE newer than 1.0
   which describes the OPL3. But I have an OPL3, and it would be nice if
   it emulated that.

   MIDI and joystick functions are ignored. And I think that DOSEMU is
   supposed to already have good CDROM support, but I don't know how well
   audio CD players will work.

   If you're having performance problems with wavesample playback, read
   /usr/src/linux/drivers/sound/Readme.v30, which describes the fragment
   parameter which you currently can adjust in ../include/sound.h

   I haven't tested this code extensively. All the software that came
   with my system is for Windows. (My father claimed that one of Compaq's
   advantages is user friendlyness. Well, user friendlyness and
   hackerfriendlyness don't exactly go hand in hand. I also haven't found
   a way to disable bios shadowing or even know if it's happening...) I
   can't get either of my DOS games to work, either (Descent and Sim City
   2000). Can't you guys support the Cirrus? (Oh, and while I'm
   complaining, those mystery ports that SimEarth needs are for the FM
   synthesiser. Watch it guys, you might generate interrupts with
   that....)
     _________________________________________________________________

15.2.1. Reference

   PC Game Programers Encyclopedia
   ftp://teeri.oulu.fi/pub/msdos/programming/gpe/

   Joel Weber (age 15)

   July 17, 1995
     _________________________________________________________________

16. DMA Code

16.1. Current DOSEMU DMA code

   Unfortunately I haven't documented this yet. However, the current code
   has been completely rewritten from this.
     _________________________________________________________________

16.2. Original DOSEMU DMA code

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

     * There is no command register. Half the bits control hardware
       details, two bits are related to memory to memory transfer, and
       two bits have misc. functions that might be useful execpt that
       they are global, so probably only the bios messes with them. And
       dosemu's psuedo-bios certainly doesn't need them.
     * 16 bit DMA isn't supported, because I'm not quite sure how it's
       supposed to work. DMA channels 4-7 are treated the same way as 0-3
       execpt that they have different address. Also, my currnet goal is
       a sb pro emulator, and the sb pro doesn't use 16 bit DMA. So it's
       really a combination of lack of information and lack of need.
     * Verify is not supported. It doesn't actually move any data, so how
       can it possibly be simulated?
     * The mode selection that determines whether to transfer one byte at
       a time or several is ignored because we want to feed the data to
       the kernel efficiently and let it control the speed.
     * Cascade mode really isn't supported; however all the channels are
       available so I don't consider it nessisary.
     * Autoinitialization may be broken. From the docs I have, the
       current and reload registers might be written at the same time???
       I can't tell.
     * The docs I have refer to a "temporary register". I can't figure
       out what it is, so guess what: It ain't implemented. (It's only a
       read register).

   The following is known to be broken. It should be fixed. Any
   volunteers? :-)

     * dma_ff1 is not used yet
     * Address decrement is not supported. As far as I know, there's no
       simple, effecient flag to pass to the kernel.
     * I should have used much more bitwise logic and conditional
       expressions.

   This is my second real C program, and I had a lot of experience in
   Pascal before that.
     _________________________________________________________________

16.2.1. Adding DMA devices to DOSEMU

   Read include/dma.h. In the dma_ch[] struct, you'll find some fields
   that don't exist on the real DMA controller itself. Those are for you
   to fill in. I trust that they are self-explainatory.

   One trick that you should know: if you know you're writing to a device
   which will fill up and you want the transfer to occur in the
   background, open the file with O_NONBLOCK.
     _________________________________________________________________

16.2.2. References

   PC Game Programers Encyclopedia
   ftp://teeri.oulu.fi/pub/msdos/programming/gpe/

   The Intel Microprocessors: 8086/8088, 80186, 80286, 80386, 80486,
   Barry B. Brey,ISBN 0-02-314250-2,1994,Macmillan

   (The only reason I use this book so extensively is it's the only one
   like it that I can find in the Hawaii State Library System :-)
     _________________________________________________________________

17. DOSEMU Programmable Interrupt Controller

   This emulation, in files picu.c and picu.h, emulates all of the useful
   features of the two 8259 programmable interrupt controllers. Support
   includes these i/o commands:

   ICW1 bits 0 and 1
          number of ICWs to expect

   ICW2 bits 3 - 7
          base address of IRQs

   ICW3 no bits
          accepted but ignored

   ICW4 no bits
          accepted but ignored

   OCW1 all bits
          sets interrupt mask

   OCW2 bits 7,5-0
          EOI commands only

   OCW3 bits 0,1,5,6
          select read register, select special mask mode

   Reads of both PICs ports are supported completely.
     _________________________________________________________________

17.1. Other features

     * Support for 16 additional lower priority interrupts. Interrupts
       are run in a fully nested fashion. All interrupts call dosemu
       functions, which may then determine if the vm mode interrupt
       should be executed. The vm mode interrupt is executed via a
       function call.
     * Limited support for the non-maskable interrupt. This is handled
       the same way as the extra low priority interrupts, but has the
       highest priority of all.
     * Masking is handled correctly: masking bit 2 of PIC0 also masks all
       IRQs on PIC1.
     * An additional mask register is added for use by dosemu itself.
       This register initially has all interrupts masked, and checks that
       a dosemu function has been registered for the interrupt before
       unmasking it.
     * Dos IRQ handlers are deemed complete when they notify the PIC via
       OCW2 writes. The additional lower priority interrupts are deemed
       complete as soon as they are successfully started.
     * Interrupts are triggered when the PIC emulator, called in the vm86
       loop, detects a bit set in the interrupt request register. This
       register is a global variable, so that any dosemu code can easily
       trigger an interrupt.
     _________________________________________________________________

17.2. Caveats

   OCW2 support is not exactly correct for IRQs 8 - 15. The correct
   sequence is that an OCW2 to PIC0 enables IRQs 0, 1, 3, 4, 5, 6, and 7;
   and an OCW2 to PIC1 enables IRQs 8-15 (IRQ2 is really IRQ9). This
   emulation simply enables everything after two OCW2s, regardless of
   which PIC they are sent to.

   OCW2s reset the currently executing interrupt, not the highest
   priority one, although the two should always be the same, unless some
   dos programmer tries special mask mode. This emulation works correctly
   in special mask mode: all types of EOIs (specific and non-specific)
   are treated as specific EOIs to the currently executing interrupt. The
   interrupt specification in a specific EOI is ignored.

   Modes not supported:

     * Auto-EOI (see below)
     * 8080-8085
     * Polling
     * Rotating Priority

   None of these modes is useable in a PC, anyway. The 16 additional
   interrupts are run in Auto-EOI mode, since any dos code for them
   shouldn't include OCW2s. The level 0 (NMI) interrupt is also handled
   this way.
     _________________________________________________________________

17.3. Notes on theory of operation:

   The documentation refers to levels. These are priority levels, the
   lowest (0) having highest priority. Priority zero is reserved for use
   by NMI. The levels correspond with IRQ numbers as follows:

      IRQ0=1  IRQ1=2   IRQ8=3  IRQ9=4  IRQ10=5 IRQ11=6 IRQ12=7 IRQ13=8
      IRQ14=9 IRQ15=10 IRQ3=11 IRQ4=12 IRQ5=13 IRQ6=14 IRQ7=15

   There is no IRQ2; it's really IRQ9.

   There are 16 more levels (16 - 31) available for use by dosemu
   functions.

   If all levels were activated, from 31 down to 1 at the right rate, the
   two functions run_irqs() and do_irq() could recurse up to 15 times. No
   other functions would recurse; however a top level interrupt handler
   that served more than one level could get re-entered, but only while
   the first level started was in a call to do_irq(). If all levels were
   activated at once, there would be no recursion. There would also be no
   recursion if the dos irq code didn't enable interrupts early, which is
   quite common.
     _________________________________________________________________

17.3.1. Functions supported from DOSEMU side

17.3.1.1. Functions that Interface with DOS:

   unsigned char read_picu0(port), unsigned char read_picu1(port)
          should be called by the i/o handler whenever a read of the PIC
          i/o ports is requested. The "port" parameter is either a 0 or a
          1; for PIC0 these correspond to i/o addresses 0x20 and 0x21.
          For PIC1 they correspond with i/o addresses 0xa0 and 0xa1. The
          returned value is the byte that was read.

   void write_picu0(port,unsigned char value), void
          write_picu1(port,unsigned char value)
          should be called by the i/o handler whenever a write to a PIC
          port is requested. Port mapping is the same as for read_picu,
          above. The value to be written is passed in parameter "value".

   int do_irq()
          is the function that actually executes a dos interrupt.
          do_irq() looks up the correct interrupt, and if it points to
          the dosemu "bios", does nothing and returns a 1. It is assumed
          that the calling function will test this value and run the
          dosemu emulation code directly if needed. If the interrupt is
          revectored to dos or application code, run_int() is called,
          followed by a while loop executing the standard run_vm86() and
          other calls. The in-service register is checked by the while
          statement, and when the necessary OCW2s have been received, the
          loop exits and a 0 is returned to the caller. Note that the 16
          interrupts that are not part of the standard AT PIC system will
          not be writing OCW2s; for these interrupts the vm86 loop is
          executed once, and control is returned before the corresponding
          dos code has completed. If run_int() is called, the flags, cs,
          and ip are pushed onto the stack, and cs:ip is pointed to
          PIC_SEG:PIC_OFF in the bios, where there is a hlt instruction.
          This is handled by pic_iret.

          Since the while loop above checks for interrupts, this function
          can be re-entered. In the worst case, a total of about 1k of
          process stack space could be needed.

   pic_sti(), pic_cli()
          These are really macros that should be called when an sti or
          cli instruction is encountered. Alternately, these could be
          called as part of run_irqs() before anything else is done,
          based on the condition of the vm86 v_iflag. Note that pic_cli()
          sets the pic_iflag to a non-zero value, and pic_sti() sets
          pic_iflag to zero.

   pic_iret()
          should be called whenever it is reasonably certain that an iret
          has occurred. This call need not be done at exactly the right
          time, its only function is to activate any pending interrupts.
          A pending interrupt is one which was self-scheduled. For
          example, if the IRQ4 code calls pic_request(PIC_IRQ4), the
          request won't have any effect until after pic_iret() is called.
          It is held off until this call in an effort to avoid filling up
          the stack. If pic_iret() is called too soon, it may aggravate
          stack overflow problems. If it is called too late, a
          self-scheduled interrupt won't occur quite as soon. To
          guarantee that pic_iret will be called, do_irq saves the real
          return address on the stack, then pushes the address of a hlt,
          which generates a SIGSEGV. Because the SIGSEGV comes before the
          hlt, pic_iret can be called by the signal handler, as well as
          from the vm86 loop. In either case, pic_iret looks for this
          situation, and pops the real flags, cs, and ip.
     _________________________________________________________________

17.3.2. Other Functions

   void run_irqs()
          causes the PIC code to run all requested interrupts, in
          priority order. The execution of this function is described
          below.

          First we save the old interrupt level. Then *or* the interrupt
          mask and the in-service register, and use the complement of
          that to mask off any interrupt requests that are inhibited.
          Then we enter a loop, looking for any bits still set. When one
          is found, it's interrupt level is compared with the old level,
          to make sure it is a higher priority. If special mask mode is
          set, the old level is biased so that this test can't fail. Once
          we know we want to run this interrupt, the request bit is
          atomicly cleared and double checked (in case something else
          came along and cleared it somehow). Finally, if the bit was
          actually cleared by this code, the appropriate bit is set in
          the in-service register. The second pic register (for PIC1) is
          also set if needed. Then the user code registered for this
          interrupt is called. Finally, when the user code returns, the
          in-service register(s) are reset, the old interrupt level is
          restored, and control is returned to the caller. The assembly
          language version of this funcction takes up to about 16 486
          clocks if no request bits are set, plus 100-150 clocks for each
          bit that is set in the request register.

   void picu_seti(unsigned int level,void (*func), unsigned int ivec)
          sets the interrupt vector and emulator function to be called
          then the bit of the interrupt request register corresponding to
          level is set. The interrupt vector is ignored if level<16,
          since those vectors are under the control of dos. If the
          function is specified as NULL, the mask bit for that level is
          set.

   void picu_maski(int level)
          sets the emulator mask bit for that level, inhibiting its
          execution.

   void picu_unmask(int ilevel)
          checks if an emulator function is assigned to the given level.
          If so, the mask bit for the level is reset.

   void pic_watch()
          runs periodically and checks for 'stuck' interrupt requests.
          These can happen if an irq causes a stack switch and enables
          interrupts before returning. If an interrupt is stuck this way
          for 2 successive calls to pic_watch, that interrupt is
          activated.
     _________________________________________________________________

17.4. A (very) little technical information for the curious

   There are two big differences when using pic. First, interrupts are
   not queued beyond a depth of 1 for each interrupt. It is up to the
   interrupt code to detect that further interrupts are required and
   reschedule itself. Second, interrupt handlers are designated at dosemu
   initialization time. Triggering them involves merely specifying the
   appropriate irq.

   Since timer interrupts are always spaced apart, the lack of queueing
   has no effect on them. The keyboard interrupts are re-scheduled if
   there is anything in the scan_queue.
     _________________________________________________________________

18. DOSEMU debugger v0.6

   This section written on 98/02/08. Send comments to Max Parke
   <mhp@light.lightlink.com> and to Hans Lermen <lermen@fgan.de>
     _________________________________________________________________

18.1. Introduction

   This is release v0.6 of the DOSEMU debugger, with the following
   features:

     * interactive
     * DPMI-support
     * display/disassembly/modify of registers and memory (DOS and DPMI)
     * display/disassembly memory (dosemu code and data)
          + read-only access to DOSEMU kernel via memory dump and
            disassembly
          + uses /usr/src/dosemu/dosemu.map for above (can be changed via
            runtime configuration)
     * breakpoints (int3-style, breakpoint on INT xx and DPMI-INT xx)
          + DPMI-INT breakpoints can have an AX value for matching. (e.g.
            'bpintd 31 0203' will stop _before_ DPMI function 0x203)
     * breakpoints via monitoring DOSEMU's logoutput using regular
       expressions
     * on-the-fly changing amount of logoutput (-D debugflags)
     * (temporary) redirect logoutput to debugger terminal.
     * single stepping (traceing).
     * dump parts of DOS mem to file.
     * symbolic debugging via microsoft linker .MAP file support
     * access is via the 'dosdebug' client from another virtual console.
       So, you have a "debug window" and the DOS window/keyboard, etc.
       are undisturbed. VM86 execution can be started, stopped, etc.
     * If dosemu 'hangs' you can use the 'kill' command from dosbugger to
       recover.
     * code base is on dosemu-0.97.2.1
     _________________________________________________________________

18.2. Usage

   To run, start up DOSEMU. Then switch to another virtual console (or
   remote login or use another xterm) and do:
         dosdebug

   If there are more then one dosemu process running, you will need to
   pass the pid to dosdebug, e.g:

         dosdebug 2134

   NOTE: You must be the owner of the running dosemu to 'debug-login'.

   You should get connected and a banner message. If you type 'q', only
   the terminal client will terminate, if you type 'kill', both dosemu
   and the terminal client will be terminated.

   It may be desirable to debug the DOS or its drivers itself during
   startup, to realize that you need to synchronize DOSEMU and your
   debugger terminal. This can be done using the -H1 command line option
   of DOSEMU:
         $ dosemu -H1

   DOSEMU will then lock before jumping into the loaded bootsector
   waiting for dosdebug to connect. Once connected you are in `stopped'
   state and can set breakpoints or singlestep through the bootstrap
   code.
     _________________________________________________________________

18.3. Commands

   See mhpdbgc.c for code and cmd table.

   (all numeric args in hex)

   ?
          Print a help page

   q
          Quit the debug session

   kill
          Kill the dosemu process (this may take a while, so be patient)
          See also Section 20

   console n
          Switch to console n

   r
          list regs

   r reg val
          change contents of 'reg' to 'val' (e.g: r AX 1234)

   e ADDR valuelist
          modify memory (0-1Mb) `ADDR' maybe just a `-' (minus), then
          last (incremented) address from a previous `e' or `ed' command
          is used (this allowes consecutive writes).

          `valuelist' is a blank separated list of

        hexnumber
                such as 0F or C800

        char
                enclosed in single quotes such as 'A' or 'b'

        register
                any valid register symbol, in this case the current value
                (and size) of that registe is take (e.g AX is 2 bytes,
                EAX is 4 bytes)

        string
                enclosed in double quotes such "this is a string"

          The default size of each value is one byte (except registers),
          this size can be overridden by suffixing a `W' (word, 2 bytes)
          or `L' (long, 4 bytes) such as C800w or F0008123L

   ed ADDR valuelist
          same as above `e' command, except that the numbers are expected
          as decimals per default. To write a hexvalue with `ed' you may
          prefix it with `0x' as in C or write an octal value prefixing a
          `0'.

   d ADDR SIZE
          dump memory (no limit)

   u ADDR SIZE
          unassemble memory (no limit)

   g
          go (if stopped)

   stop
          stop (if running)

   mode 0|1|+d|-d
          set mode (0=SEG16, 1=LIN32) for u and d commands +d enables
          DPMI mode (default on startup), -d disables DPMI mode.

   t
          single step (may jump over IRET or POPF)

   tc
          single step, loop forever until key pressed

   tf
          single step, force over IRET and POPF NOTE: the scope of 't'
          'tf' or a 'come back for break' is either 'in DPMI' or
          realmode, depending on wether a DPMI-client is active
          (in_dpmi).

   r32
          dump regs in 32 bit format

   bp addr
          set int3 style breakpoint NOTE: the scope is defined wether a
          DPMI-client is active (in_dpmi). The resulting 'come back' will
          force the mode that was when you defined the breakpoint.

   bc breakp.No.
          Clear a breakpoint.

   bpint xx
          set breakpoint on INT xx

   bcint xx
          clr breakpoint on INT xx

   bpintd xx [ax]
          set breakpoint on DPMI INT xx optionaly matching ax.

   bcintd xx [ax]
          clear breakpoint on DPMI INT xx.

   bpload
          set one shot breakpoint at entry point of the next loaded
          DOS-program.

   bl
          list active breakpoints

   bplog regex
          set a breakpoint on logoutput using regex. With this the normal
          DOSEMU log output (enabled via the -D commandline option or the
          dosdebug `log' command) is monitored via the regular expression
          `regex' (look at GNU regex manual) and when a match is found
          emulation is set into `stopped' mode. There may be 8 log
          breakpoint active simultaneously. Without the `regex' given
          `bplog' such prints the current active breakpoints.

   bpclog number
          clears a log break point.

   log [flags]
          get/set debug-log flags (e.g 'log +M-k')

   log on|off
          redirect dbug-log output to the dosdebug terminal

   ldt sel [lines]
          dump ldt starting at selector 'sel' for 'lines' 'sel' may be a
          symbolic register name.

   (rmapfile)
          (internal command to read /usr/src/dosemu/dosemu.map at startup
          time)

   rusermap org fn
          read microsoft linker format .MAP file "fn" code origin =
          "org". for example if your code is at 1234:0, org would be
          12340.

   Addresses may be specified as:

    1. a linear address. Allows 'd' and 'u' commands to look at both
       DOSEMU kernel and DOS box memory (0-1Mb).
    2. a seg:off address (0-1Mb) seg as well as off can be a symbolic
       registers name (e.g cs:eip) 'seg' under DPMI is resolved via LDT,
       if so a numeric 'seg' value is prefixed by # (e.g. #00af:0000. You
       may force a seg to treaten as LDT selector by prefixing the '#'.
       Accordingly to the default address mode 'off' under DPMI is 16 or
       32 bit. When in DPMI mode, and you want to address/display
       realmode stuff, then you must switch off DPMI mode ('mode -d')
    3. a symbolic address. usermap is searched first, then dosemu map. (
       not for DPMI programms )
    4. an asterisk(*): CS:IP (cs:eip)
    5. a dollar sign($): SS:SP (ss:esp)
     _________________________________________________________________

18.4. Performance

   If you have dosemu compiled with the debugger support, but the
   debugger is not active and/or the process is not stopped, you will not
   see any great performance penalty.
     _________________________________________________________________

18.5. Wish List

   Main wish is to add support for hardware debug registers (if someone
   would point me in the direction, what syscalls to use, etc.) Then you
   could breakpoint on memory reads/writes, etc!
     _________________________________________________________________

18.6. BUGS

   There must be some.
     _________________________________________________________________

18.6.1. Known bugs

     * Though you may set breakpoints and do singlestep in Windows31,
       this is a 'one shot': It will bomb after you type 'g' again. ( I
       suspect this is a timer problem, we really should freeze the timer
       and all hardware/mouse IRQs while the program is in 'stop').
       Debugging and singlestepping through DJGPP code doesn't have any
       problems.
     * INT3 type breakpoints in DPMI code are very tricky, because you
       never know when the client has remapped/freed the piece of code
       that is patched with 0xCC ( the one byte INT3 instruction ). Use
       that with caution !!
     * Single stepping doesn't work correctly on call's. May be the
       trap-flag is lost. However, when in DPMI the problems are minor.
     * popf sometime clears the trap-flag, so single stepping results in
       a 'go' command. 'tf' works around, but we should do it better.
     * When stopped for a long period, the BIOS-timer will be updated to
       fast and may result in stack overflow. We need to also stop the
       timer for dosemu.
     * When not stopped, setting break points doesn't work properly. So,
       as a work around: Setting breakpoints while not in stop is
       disabled.
     _________________________________________________________________

19. MARK REJHON'S 16550 UART EMULATOR

   The ./src/base/serial directory contains Mark Rejhon's 16550A UART
   emulation code.

   Please send bug reports related to DOSEMU serial to either
   <marky@magmacom.com> or <ag115@freenet.carleton.ca>

   The files in this subdirectory are:

   Makefile
          The Makefile for this subdir.

   ser_init.c
          Serial initialization code.

   ser_ports.c
          Serial Port I/O emulation code.

   ser_irq.c
          Serial Interrupts emulation code.

   int14.c
          BIOS int14 emulation code.

   fossil.c
          FOSSIL driver emulation code.

   ../commands/fossil.S
          DOS part of FOSSIL emulation.

   ser_defs.h
          Include file for above files only.

   ../include/serial.h
          General include file for all DOSEMU code.

   Redistribution of these files is permitted under the terms of the GNU
   Public License (GPL). See end of this file for more information.
     _________________________________________________________________

19.1. PROGRAMMING INFORMATION

   Information on a 16550 is based on information from HELPPC.EXE 2.1 and
   results from National Semiconductor's COMTEST.EXE diagnostics program.
   This code aims to emulate a 16550A as accurately as possible, using
   just reasonably POSIX.2 compliant code. In some cases, this code does
   a better job than OS/2 serial emulation (in non-direct mode) done by
   its COM.SYS driver! There may be about 100 kilobytes of source code,
   but nearly 50% of this size are comments!

   This 16550A emulator never even touches the real serial ports, it
   merely traps port writes, and does the I/O via file functions on a
   device in /dev. Interrupts are also simulated as necessary.

   One of the most important things to know before programming this
   serial code, is to understand how the com[] array works.

   Most of the serial variables are stored in the com[] array. The com[]
   array is a structure in itself. Take a look at the 'serial_t' struct
   declaration in the ../include/serial.h module for more info about
   this. Only the most commonly referenced global variables are listed
   here:

   config.num_ser
          Number of serial ports active.

   com[x].base_port
          The base port address of emulated serial port.

   com[x].real_comport
          The COM port number.

   com[x].interrupt
          The PIC interrupt level (based on IRQ number)

   com[x].mouse
          Flag mouse (to enable extended features)

   com[x].fd
          File descriptor for port device

   com[x].dev[]
          Filename of port port device

   com[x].dev_locked
          Flag whether device has been locked

   The arbritary example variable 'x' in com[x] can have a minimum value
   of 0 and a maximum value of (config.numser - 1). There can be no gaps
   for the value 'x', even though gaps between actual COM ports are
   permitted. It is strongly noted that the 'x' does not equal the COM
   port number. This example code illustrates the fact, and how the
   com[x] array works:
          for (i = 0; i < config.numser; i++)
            s_printf("COM port number %d has a base address of %x",
                     com[i].real_comport, com[i].base_port);
     _________________________________________________________________

19.2. DEBUGGING HELP

   If you require heavy debugging output for serial operations, please
   take a look in ./ser_defs.h for the following defines:

   SER_DEBUG_MAIN (0 or 1)
          extra debug output on the most critical information.

   SER_DEBUG_HEAVY (0 or 1)
          super-heavy extra debug output, including all ports reads and
          writes, and every character received and transmitted!

   SER_DEBUG_INTERRUPT (0 or 1)
          additional debug output related to serial interrupt code,
          including flagging serial interrupts, or PIC-driven code.

   SER_DEBUG_FOSSIL_RW (0 or 1)
          heavy FOSSIL debug output, including all reads and writes.

   SER_DEBUG_FOSSIL_STATUS (0 or 1)
          super-heavy FOSSIL debug output, including all status checks.
     _________________________________________________________________

19.3. FOSSIL EMULATION

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
     _________________________________________________________________

19.4. COPYRIGHTS

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
     _________________________________________________________________

20. Recovering the console after a crash

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
     _________________________________________________________________

20.1. The mail message

    Date: Fri, 21 Apr 95 14:16 CDT
    To: tegla@katalin.csoma.elte.hu
    Cc: linux-msdos@vger.rutgers.edu
    In-Reply-To: <Pine.LNX.3.91.950421163705.1348B-100000@katalin.csoma.elte.hu
> (message from Nagy Peter on Fri, 21 Apr 1995 16:51:27 +0200 (MET DST))
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
     _________________________________________________________________

21. Net code

     * Added support for multiple type handling. So it does type
       demultiplexing within dosemu.
       So we are able to run telbin and pdipx/netx simultaneously ;-)
     * Changed the code for F_DRIVER_INFO. If the handle is '0', it need
       not be valid handle (when input to this function). At least CUTCP
       simply set this value to zero when it requests for info for the
       first time. The current code wrongly treated it as valid handle,
       and returned the class of first registered type.
     * Some things still to be done ...
          + Novell Hack is currently disabled.
          + Compare class along with type when a new packet arrives.
          + Lots of clean ups required. (I have put in lots of
            pd_printf's.) Need to be removed even if debug is not used
            because function call within packet handler is very costly
            ...
          + change all 'dsn0' / 'dosnet' to vnet (i.e. virtual net.) or
            so.
       and
          + Use of SIGIO ...
          + Special protocol stack to do demultiplexing of types??
          + Use of devices instead of the current mechanism of
            implementing v-net?

   NOTE: THERE ARE STILL BUGS. The dosemu session just hangs. -- Usually
   when in CUTCP. (And specially when I open two sessions.) What is
   strange is, it seems to work fine for some time even when two sessions
   are opened. I strongly feel it is due to new PIC code because it is
   used heavily by the network code ... (especially in interactive telnet
   sessions.)

   Vinod <vinod@cse.iitb.ernet.in>
     _________________________________________________________________

22. Software X386 emulation

   This section written in a hurry by Alberto Vignani
   <vignani@mbox.vol.it> , Oct 20, 1997
     _________________________________________________________________

22.1. The CPU emulator

   The CPU emulator has been derived from <the Twin Willows libraries>.
   Only the relevant parts of the library, namely the /intp32
   subdirectory and the needed include files, have been extracted from
   the Twin sources into the src/twin directory. The Twin reference
   version is 3.1.1. In the Twin code, changes needed for the dosemu
   interface have been marked with
           #ifdef DOSEMU

   Here is a summary of the changes I made in the Twin libraries:

     * I added vm86 mode, and related exception handling.
     * I made a first attempt to entry-point symmetry; the final goal is
       to have an 'invoke_code32' in interp_32_32.c, which can reach the
       16-bit code using 0x66,0x67 prefixes, the same way the 16-bit code
       is currently doing the other way. The variables 'code32' and
       'data32' are used for prefix control.
     * some optimizations to memory access and multiplication code for
       little-endian machines and GNU compiler.
     * dosemu-style debug output; this is the biggest part of the patch
     * bugfixes. These are NOT marked with #ifdef DOSEMU!

   The second part of the cpuemu patch is the interface to dosemu, which
   is controlled by the X86_EMULATOR macro. This macro was probably part
   of a very old attempt to interface dosemu with Bochs, I deleted the
   old code and replaced it with the Twin interface.

   The X86_EMULATOR macro enables the compilation of the two files
   (cpu-emu.c and emu-utils.c) in the src/emu-i386/intp32 directory,
   which contain the vm86 emulator call (taken from the kernel sources)
   and some utility/debug functions. These files are kept separate from
   the Twin directory but need it to compile.

   For controlling the emulator behaviour, the file include/cpu-emu.h
   provides three macros:

   DONT_START_EMU
          if undefined, the emulator starts immediately; otherwise, a
          call to int 0xe6 al=0x90 is required to switch from the
          standard vm86 to it. To switch in and out from the emulator,
          the small utilities 'ecpuon.com' and 'ecpuoff.com' are
          provided.

   TRACE_HIGH
          controls the memory areas you want to include into the debug
          trace. The default value excludes the video BIOS and the HMA,
          but feel free to change it following your needs.

   VT_EMU_ONLY
          if defined, use of the emulator forces VT console mode, by
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
     _________________________________________________________________

23. MFS and National Language Support

   This section written by Oleg V. Zhirov <O.V.Zhirov@inp.nsk.su> , Aug
   3, 1998
     _________________________________________________________________

23.1. MFS and National Language Support

   Main problem is that *nix and DOS uses codesets, which can differ. So,
   in Russia the most popular codeset for *nix is koi8-r, while DOS
   standard used so called `alternative' codeset cp866.

   While DOSEMU access DOS partitions directly, through original DOS
   (V)FAT drivers, it doesn't matter, that linux locales are set to
   koi8-r, and all works correctly. However, in this way you cannot start
   more than one copy of DOSEMU at the same time.

   A more elegant solution (which in fact heavily supported by recent
   development of DOSEMU) is to mount all DOS partitions into linux
   filesystem and access them through the `internal network' system MFS
   (Mach File System). It allows use the security of real *nix (linux)
   filesystem, and one can have simultaneously so many DOSEMU sessions,
   as he wants ;-).

   However, new problems occur:

    1. Mount DOS partitions into Linux. Currently, linux kernel
       linux-2.0.34 support mounting both MSDOS and VFAT partitions. No
       problem is, if your DOS partition has true MSDOS format. More
       ambigious case is if your have VFAT partition. You can mount it as
       VFAT and get access to 'long names', which looks as very
       attractive option. But in this case you *have lost* any access to
       _short_filenames_ - DOS aliases of long filenames. To access
       short_filenames you need mount VFAT partition as MSDOS (!) (I am
       not even sure, that this is completely safe (!)).
    2. IMPORTANT: mounting DOS partition, you optionally make filenames
       convertion from DOS codeset to Linux one, if you want to see
       filenames correctly (in Russia cp866 -> koi8-r). Otherwise I need
       to support both codesets in my Linux. (Currently I do no
       conversion and have on my console both codesets).
    3. Bug in MFS: transfer directory/file names from Linux to DOS via
       MFS includes also some name conversion from *nix standard to DOS
       one. In original (current in dosemu-0.97.10) release of MFS a lot
       of corresponding locale-dependent (!!!) char/string operations on
       _DOS_names_ were performed with _Linux_ locale setting (in my case
       koi8-r). As a result, one get a garbage instead of original
       filenames.
    4. If you mount VFAT partition in LINUX as VFAT, some of filenames
       are long. I fact, curent dosemu MFS system can `mangle' them,
       converting into short aliases. There is NO vay to reconstruct the
       true DOS alias from long_name_, since in DOS created SFNAME~index
       index value depends exceptionaly on the _history_ of file
       creation. As a result, `mangled' names differs from true DOS
       shortnames.
     _________________________________________________________________

23.2. Patching of MFS

   Presented patch of MFS cures problem (3) only. Summary of
   modification:

     * all locale-dependent code is removed from mfs.c and mangle.c and
       is located in file util.c only. Here I add a set of routins:

              islowerDOS(...)(+), tolowerDOS(...)(+),
              isupperDOS(...)(+), toupperDOS()(+),
              isalphaDOS(...)(+), isalnumDOS(...)(+),
              is_valid_DOS_char(...)(+), chrcmpDOS(...)(+),

              strlowerDOS(...), strhaslowerDOS(...),
              strupperDOS(...), strhasupperDOS(...),
              chrcmpDOS(...)(+), strncmpDOS(...), strcmpDOS(...),
              strncasecmpDOS(...), strcasecmpDOS(...).

       Upper part (marked by (+)) has all the information on the
       codesets, while lower part is derived from the upper one.
       Currently codepage 866 implementation is completely sufficient for
       Russia (e.g for Ukrainian some definitions should be added). As
       for Western Europe latin codepages, some sort of support existed
       before is conserved. In any case, further addition of other
       codepages is quite easy.
     * file mangle.c was revised, and some dead code was excluded. To my
       opinion, this file need to be more critically revized: some stuff,
       (e.g. verification of 8.3 format) is duplicated in mfs.c, and
       other stuf looks too overcomplicated (am I right?). I have a
       suspection, that most of codes overlaps in functionality with
       those of mfs.c. Unfortunately, I have no time to examine the file
       mfs.c more carefully.
     * At last, I replace all locale-dependent operation in mangle.c and
       mfs.c by their DOS partners: func() -> funcDOS(), defined in
       util.c.

   To patch MFS put patch.mfs into directory
   dosemu-0.97.10/src/dosext/mfs/ and issue a command
       patch -p1 <patch.mfs

   and then compile dosemu as usual.

   To mount dos VFAT partition in Linux filesystem I do like as

    mount -t vfat -o noexec,umask=022,gid=107,codepage=866,iocharset=cp866 /dev
/hda1 /dos_C

   (or corresponding line in /etc/fstab). In this vay I have dos names in
   the `alternative' codepage 866. To operate them in Linux I turn
   console in `altenative' charset mode (see, e.g. CYRILLIC-HOWTO).

   NOTE: VFAT module in linux-2.0.34 seems to be buggy: Creating via
   Linux filename with lowercase russian letters, you obtain file not
   accessible by DOS (or even Win95) - probably, people, who makes VFAT
   support have forgotten creating shortname DOS alias from long names or
   short names turn to upper case chars with ASCII codes > 127 ? (To my
   first look, they use tolower() and toupper() functions which works for
   ascii<=127). Fortunately, DOS creates filenames properly, even with
   cyrillic letters, and creating files with russian names is completely
   safe.

   In DOSEMU I load DOS (DOS-7) from hdimage. To access dos partition in
   DOSEMU I have in config.sys two lines

         install=c:\subst.exe L: C:\
         install=c:\lredir.exe C: linux\fs/dos_C

   Obvious disadvantage of my approach: long filenames have short aliases
   slightly different than in direct partition access. In practice, this
   doesn't result in problem, since DOS stuff usually does not exploit
   long filenames, and files with long filenames are irrelevant.
     _________________________________________________________________

23.3. TODO:

    1. In Linux kernel: Cure in VFAT module upper/lowercase bugs for
       ascii>127.
    2. In Linux kernel: VFAT module should provide access to short DOS
       aliases of long names, even if VFAT partition is mounted in
       longname support mode (with -t vfat).
    3. In dosemu MFS: MFS should use short DOS aliases, actually existing
       for VFAT longnames.
