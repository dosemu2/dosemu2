  DOSEmu
  The DOSEmu team Edited by Alistair MacDonald  <alis-
  tair@slitesys.demon.co.uk>
  For DOSEMU v0.99 pl11.0

  This document is the amalgamation of a series of README files which
  were created to deal with the lack of DOSEmu documentation.
  ______________________________________________________________________

  Table of Contents
























































  1. Introduction

  2. Runtime Configuration Options

     2.1 Format of /etc/dosemu.conf
        2.1.1 Controling amount of debug output
        2.1.2 Basic emulaton settings
        2.1.3 Code page and character set
        2.1.4 Terminals
        2.1.5 Keyboard settings
        2.1.6 X Support settings
        2.1.7 Video settings ( console only )
        2.1.8 Disks, boot directories and floppies
        2.1.9 COM ports and mices
        2.1.10 Printers
        2.1.11 Networking under DOSEMU
        2.1.12 Sound
        2.1.13 Builtin ASPI SCSI Driver

  3. Security

  4. Directly executable DOS applications using dosemu (DEXE)

     4.1 Making and Using DEXEs
     4.2 Using binfmt_misc to spawn DEXEs directly
     4.3 Making a bootable hdimage for general purpose
     4.4 Accessing hdimage files using mtools

  5. Sound

     5.1 Using the MPU-401 "Emulation".
     5.2 The MIDI daemon
     5.3 Disabling the Emulation at Runtime

  6. Using Lredir

     6.1 how do you use it?
     6.2 Other alternatives using Lredir

  7. Running dosemu as a normal user

  8. Using CDROMS

     8.1 The built in driver

  9. Logging from DOSEmu

  10. Using X

     10.1 Latest Info
     10.2 Slightly older information
     10.3 Status of X support (Sept 5, 1994)
        10.3.1 Done
        10.3.2 ToDo (in no special order)
     10.4 The appearance of Graphics modes (November 13, 1995)
        10.4.1 vgaemu
        10.4.2 vesa
        10.4.3 X
     10.5 The new VGAEmu/X code (July 11, 1997)

  11. Running Windows under DOSEmu

     11.1 Windows 3.0 Real Mode
     11.2 Windows 3.1 Protected Mode
     11.3 Windows 3.x in xdos

  12. Mouse Garrot

  13. Running a DOS-application directly from Unix shell

     13.1 Using the keystroke and commandline options.
     13.2 Using an input file
     13.3 Running DOSEMU within a cron job

  14. Commands & Utilities

     14.1 Programs
     14.2 Drivers

  15. Keymaps

  16. Networking using DOSEmu

     16.1 The DOSNET virtual device.
     16.2 Setup for virtual TCP/IP
     16.3 Full Details
        16.3.1 Introduction
        16.3.2 Design
        16.3.3 Implementation
        16.3.4 Virtual device 'dsn0'
        16.3.5 Packet driver code
        16.3.6 Conclusion
           16.3.6.1 Telnetting to other Systems
           16.3.6.2 Accessing Novell netware

  17. Using Windows and Winsock

     17.1 LIST OF REQUIRED SOFTWARE
     17.2 STEP BY STEP OPERATION (LINUX SIDE)
     17.3 STEP BY STEP OPERATION (DOS SIDE)


  ______________________________________________________________________

  11..  IInnttrroodduuccttiioonn

  This documentation is derived from a number of smaller documents. This
  makes it easier for individuals to maintain the documentation relevant
  to their area of expertise. Previous attempts at documenting DOSEmu
  failed because the documentation on a large project like this quickly
  becomes too much for one person to handle.

  22..  RRuunnttiimmee CCoonnffiigguurraattiioonn OOppttiioonnss


  This section of the document by Hans, <lermen@fgan.de>. Last updated
  on Feb 12, 1999.


  Most of DOSEMU configuration is done during runtime and per default it
  expects the system wide configuration file /etc/dosemu.conf optionally
  followed by the users  /.dosemurc and additional configurations
  statements on the commandline (-I option). The builtin configuration
  file of a DEXE file is passed using the -I technique, hence the rules
  of -I apply.

  In fact /etc/dosemu.conf and  /.dosemurc (which have identical syntax)
  are included by the systemwide configuration script
  /var/lib/dosemu/global.conf, but as a normal user you won't ever think
  on editing this, only dosemu.conf and your personal  /.dosemurc.  The
  syntax of global.conf is described in detail in README-tech.txt, so
  this is skipped here. However, the option -I string too uses the same
  syntax as global.conf, hence, if you are doing some special stuff
  (after you got familar with DOSEMU) you may need to have a look there.

  In DOSEMU prior to 0.97.5 the private configuration file was called
  /.dosrc (not to be confused with the new  /.dosemurc). This will work
  as expected formerly, but is subject to be nolonger supported in the
  near future.  This (old)  /.dosrc is processed _a_f_t_e_r global.conf and
  follows (same as -I) the syntax of global.conf (see README-tech.txt).



  The first file expected (and interpreted) before any other
  configuration (such as global.conf, dosemu.conf and  /.dosemurc) is
  /etc/dosemu.users.  Within /etc/dosemu.users the general permissions
  are set:


  +o  which users are allowed to use DOSEMU.

  +o  which users are allowed to use DOSEMU suid root.

  +o  which users are allowed to have a private lib dir.

  +o  what kind of access class the user belongs to.

  +o  what special configuration stuff the users needs

  and further more:

  +o  wether the lib dir (/var/lib/dosemu) resides elsewhere.

  +o  setting the loglevel.


  Except for lines starting with `xxx=' (explanation below), each line
  in dosemu.user corresponds to exactly _o_n_e valid user count, the
  special user `all' means any user not mentioned earlier. Format:


         [ <login> | all ] [ confvar [ confvar [ ... ] ] ]




  The below example is from etc/dosemu.users.secure, which you may copy
  to /etc/dosemu.users.


         root c_all     # root is allowed to do all weird things
         nobody nosuidroot guest # variable 'guest' is checked in global.conf
                                 # to allow only DEXE execution
         guest nosuidroot guest  # login guest treated as `nobody'
         myfriend c_all unrestricted private_setup
         myboss nosuidroot restricted private_setup
         all nosuidroot restricted # all other users have normal user restrictions




  Note that the above `restricted' is checked in global.conf and will
  disable all secure relevant feature. Setting `guest' will force set-
  ting `restricted' too.

  The use of `nosuidroot' will force a suid root dosemu binary to exit,
  the user may however use a non-suid root copy of the binary.  For more
  information on this look at README-tech, chapter 11.1 `Privileges and
  Running as User'

  Giving the keyword `private_setup' to a user means he/she can have a
  private DOSEMU lib under $HOME/.dosemu/lib. If this directory is
  existing, DOSEMU will expect all normally under /var/lib/dosemu within
  that directory, including `global.conf'. As this would be a security
  risc, it only will be allowed, if the used DOSEMU binary is non-suid-
  root. If you realy trust a user you may additionally give the keyword
  `unrestricted', which will allow this user to execute a suid-root
  binary even on a private lib directory (though, be aware).

  In addition, dosemu.users can be used to define some global settings,
  which must be known before any other file is accessed, such as:


         default_lib_dir= /opt/dosemu  # replaces /var/lib/dosemu
         log_level= 2                  # highest log level




  With `default_lib_dir=' you may move /var/lib/dosemu elsewere, this
  mostly is interesting for distributors, who want it elswere but won't
  patch the DOSEMU source just for this purpose. But note, the dosemu
  supplied scripts and helpers may need some adaption too in order to
  fit your new directory.

  The `log_level=' can be 0 (never log) or 1 (log only errors) or 2 (log
  all) and controls the ammount written to the systems log facility
  (notice).  This keyword replaces the former /etc/dosemu.loglevel file,
  which now is obsolete.

  Nevertheless, for a first try of DOSEMU you may prefer
  etc/dosemu.users.easy, which just contains


         root c_all
         all c_all




  to allow evrybody all weird things. For more details on security
  issues have a look at README-tech.txt chapter 2.



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

  For more details on security setings/issues look at README-tech.txt,
  for now (using DOSEMU the first time) you should need only the below
  description of /etc/dosemu.conf ( /.dosemurc)


  22..11..  FFoorrmmaatt ooff //eettcc//ddoosseemmuu..ccoonnff

  All settings in dosemu.conf are just variables, that are interpreted
  in /var/lib/dosemu/global.conf and have the form of



         $_xxx = (n)
       or
         $_zzz = "s"




  where `n' ist a numerical or boolean value and `s' is a string.  Note
  that the brackets are important, else the parser won't decide for a
  number expression. For numers you may have complete expressions ( such
  as (2*1024) ) and strings may be concatenated such as



         $_zzz = "This is a string containing '", '"', "' (quotes)"




  Hence a comma separated list of strings is concatenated.


  22..11..11..  CCoonnttrroolliinngg aammoouunntt ooff ddeebbuugg oouuttppuutt


  DOSEMU will help you finding problems, when you enable its debug
  messages.  These will go into the file, that you defined via the `-o
  file' or `-O' commandline option (the later prints to stderr). In
  dosemu.conf you can preset this via



         $_debug = "-a"




  where the string contains all you normally may pass to the `-D' com-
  mandline option (look at the man page for details).


  22..11..22..  BBaassiicc eemmuullaattoonn sseettttiinnggss


  To enable INT08 type timer interrupts set the below on or off


         $_timint = (on)




  Wether a numeric processor should be shown to the DOS space

    $_mathco = (on)




  Which type of CPU should be emulated (NOTE: this is not the one you
  are running on, but your setting may not exeed the capabilities of the
  running CPU). Valid values are:  80[345]86


         $_cpu = (80386)




  To let DOSEMU use the Pentium cycle counter (if availabe) to do better
  timing use the below



         $_rdtsc = (on)   # or off




  For the above `rdtsc' feature DOSEMU needs to know the exact CPU
  clock, it normally calibrates it itself, but is you encounter a wrong
  mesurement you may overide it such as


         $_cpuspeed = (166.666)  # 0 = let DOSEMU calibrate




  NOTE: `$_rdtsc' and `$_cpuspeed' can _n_o_t be overwritten by
  /.dosemurc.

  If you have a PCI board you may allow DOSEMU to access the PCI
  configuration space by defining the below


         $_pci = (on)    # or off





  Defining the memory layout, which DOS should see:


         $_xms = (1024)          # in Kbyte
         $_ems = (1024)          # in Kbyte
         $_ems_frame = (0xe000)
         $_dpmi = (off)          # in Kbyte
         $_dosmem = (640)        # in Kbyte, < 640




  Note that (other as in native DOS) each piece of mem is separate,
  hence DOS perhaps will show other values for 'extended' memory. To
  enable DPMI (by giving it memory) is a security concern, so you should
  either _n_o_t give access to dosemu for normal users (via
  /etc/dosemu.users) or give those users the `restricted' attribute (see
  above).
  There are some features in DOSEMU, that may violate system security
  and which you should not use on machines, which are `net open'. To
  have atleast a minimum of protection against intruders, use the
  folling:


         $_secure ="ngd"  # secure for: n (normal users), g (guest), d (dexe)
                          # empty string: depending on 'restricted'




  The above is a string of which may be given or not, hence


         $_secure ="d"




  would only effect execution of DEXEs. If you are not a `restricted'
  user (as given via /etc/dosemu.users) the above settings won't apply.
  To disable security checking atall set


         $_secure ="0"




  NOTE: `$_secure' can _n_o_t be overwritten by  /.dosemurc.

  For the similar reasons you may `backout' some host, which you don't
  like to have access to dosemu


         $_odd_hosts = ""    # black list such as
                             #      "lucifer.hell.com billy.the.cat"
         $_diskless_hosts="" # black list such as "hacker1 newbee gateway1"




  The items in the lists are blank separated, `odd_hosts' checks for
  remote logins, `diskless_hosts' are meant to be maschines, that mount
  a complete tree, hence the checked host is the host DOSEMU is running
  on (not the remote host). However, read README-tech,txt for more
  details on what actually is disabled.

  NOTE: `$_*_hosts' can _n_o_t be overwritten by  /.dosemurc.

  If you want mixed operation on the filesystem, from which you boot
  DOSEMU (native and via DOSEMU), it may be necessary to have two
  separate sets of `config.sys,autoexec.bat,system.ini'. DOSEMU can fake
  a different file extension, so DOS will get other files when running
  under DOSEMU.



         $_emusys = ""    # empty or 3 char., config.sys   -> config.XXX
         $_emubat = ""    # empty or 3 char., autoexec.bat -> autoexec.XXX
         $_emuini = ""    # empty or 3 char., system.ini   -> system.XXX




  As you would realize at the first glance: DOS will _n_o_t have the the
  CPU for its own. But how much it gets from Linux, depends on the
  setting of `hogthreshold'.  The HogThreshold value determines how nice
  Dosemu will be about giving other Linux processes a chance to run.



         $_hogthreshold = (1)   # 0 == all CPU power to DOSEMU
                                # 1 == max power for Linux
                                # >1   the higher, the slower DOSEMU will be




  If you have hardware, that is not supported under Linux but you have a
  DOS driver for, it may be necessary to enable IRQ passing to DOS.


         $_irqpassing = ""  # list of IRQ number (2-15) to pass to DOS such as
                            # "3 8 10"




  Here you tell DOSEMU what to do when DOS wants let play the speaker:


         $_speaker = ""     # or "native" or "emulated"




  And with the below may gain control over _r_e_a_l ports on you machine.
  But: WWAARRNNIINNGG:: GGIIVVIINNGG AACCCCEESSSS TTOO PPOORRTTSS IISS BBOOTTHH AA SSEECCUURRIITTYY CCOONNCCEERRNN AANNDD
  SSOOMMEE PPOORRTTSS AARREE DDAANNGGEERROOUUSS TTOO UUSSEE..  PPLLEEAASSEE SSKKIIPP TTHHIISS SSEECCTTIIOONN,, AANNDD DDOONN''TT
  FFIIDDDDLLEE WWIITTHH TTHHIISS SSEECCTTIIOONN UUNNLLEESSSS YYOOUU KKNNOOWW WWHHAATT YYOOUU''RREE DDOOIINNGG..



         $_ports = ""  # list of portnumbers such as "0x1ce 0x1cf 0x238"
                       # or "0x1ce range 0x280,0x29f 310"
                       # or "range 0x1a0,(0x1a0+15)"




  NOTE: `$_ports' can _n_o_t be overwritten by  /.dosemurc.



  22..11..33..  CCooddee ppaaggee aanndd cchhaarraacctteerr sseett

  To select the character set and code page for use with DOSEMU you have


         $_term_char_set = "XXX"




  where XXX is one of

     iibbmm
        the text is taken whithout translation, it is to the user to
        load a proper DOS font (cp437.f16, cp850.f16 or cp852.f16 on the
        console).
     llaattiinn
        the text is processed using cp437->iso-8859-1 translation, so
        the font used must be iso-8859-1 (eg iso01.f16 on console);
        which is the default for unix in western languages countries.

     llaattiinn11
        like latin, but using cp850->iso-8859-1 translation (the
        difference between cp437 and cp850 is that cp437 uses some chars
        for drawing boxes while cp850 uses them for accentuated letters)

     llaattiinn22
        like latin1 but uses cp852->iso-8859-2 translation, so
        translates the default DOS charset of eastern european countries
        to the default unix charset for those countries.

  The default one is ``latin'' and if the string is empty, then an auto-
  matic attempt is made: ``ibm'' for remote console and ``latin'' for
  anything else.  Depending on the charset setting the (below described)
  keyboard layouts and/or the terminal behave may vary. You need to know
  the correct code page your DOS is configured for in order to get the
  correct results.  For most western european countries 'latin' should
  be the correct setting.



  22..11..44..  TTeerrmmiinnaallss

  This section applies whenever you run DOSEMU remotely or in an xterm.
  Color terminal support is now built into DOSEMU.  Skip this section
  for now to use terminal defaults, until you get DOSEMU to work.



         $_term_color = (on)   # terminal with color support
         $_term_updfreq = (4)  # time between refreshs (units: 20 == 1 second)
         $_escchar = (30)      # 30 == Ctrl-^, special-sequence prefix




  `term_updfreq' is a number indicating the frequency of terminal
  updates of the screen. The smaller the number, the more frequent.  A
  value of 20 gives a frequency of about one per second, which is very
  slow.  `escchar' is a number (ascii code below 32) that specifies the
  control character used as a prefix character for sending alt, shift,
  ctrl, and function keycodes.  The default value is 30 which is Ctrl-^.
  So, for example,



         F1 is 'Ctrl-^1', Alt-F7 is 'Ctrl-^s Ctrl-^7'.
         For online help, press 'Ctrl-^h' or 'Ctrl-^?'.





  22..11..55..  KKeeyybbooaarrdd sseettttiinnggss


  When running DOSEMU from console (also remote from console) or X you
  may need to define a proper keyboard layout. Its possible to let
  DOSEMU do this work automatically for you (see _a_u_t_o below), however,
  this may fail and you'll end up defining it explicitely. This is done
  either by choosing one on the internal keytables or by loading an
  external keytable from /var/lib/dosemu/keymap/* (which you may modify
  according to your needs). Both sets have identical names (though you
  may add any new one to /var/lib/dosemu/keymap/*):


         be              finnish         hu-latin2       sg-latin1
         de              finnish-latin1  it              sw
         de-latin1       fr              keyb-no         uk
         dk              fr-latin1       no-latin1       us
         dk-latin1       hr-cp852        po
         dvorak          hr-latin2       sf
         es              hu              sf-latin1
         es-latin1       hu-cwi          sg              jp106
         cz-qwerty       cz-qwertz




  You define an internal keytable such as


         $_layout = "name"




  where `name' is one of the above. To load a keytable you just prefix
  the string with "load" such as


         $_layout = "load de-latin1"




  Note, however, that you have to set



         $_X_keycode = (on)




  to use this feature under X, because per default the keytable is
  forced to be neutral (us). Normally you will have the correct settings
  of your keyboard given by the X-server.

  The most comfortable method, however, is to first let DOSEMU set the
  keyboard layout itself. This involves 2 parts and can be done by
  setting


         $_X_keycode = (auto)




  which  checks for existence of a  XFree/Xaccel-Server and if yes, it
  sets $_X_keycode to 'on', that means the DOSEMU keytables are active.
  The second part (which is independent from $_X_keycode) can be set by


         $_layout = "auto"



  DOSEMU then queries the keyboard layout from the kernel (which only
  does work on console or non-remote X) and generates a new DOSEMU
  keytable out of the kernel information. This currently seems only to
  work for latin-1 layouts, the latin-2 type of accents seem not to
  exist so far in the kernel (linux/keyboard.h).  The resulting table
  can be monitor with DOSEMU 'keytable dump' feature (see README-
  tech.txt) for details).

  When being on console you might wish to use raw keyboard, especially
  together with some games, that don't use the BIOS/DOS to get their
  keystrokes.


         $_rawkeyboard = (1)




  However, be carefull, when the application locks, you may not be able
  to switch your console and recover from this. For details on recover-
  ing look at README-tech.txt (Recovering the console after a crash).

  NOTE: `$_rawkeyboard' can _n_o_t be overwritten by  /.dosemurc.


  The `keybint (on)' allows more accurate of keyboard interrupts, It is
  a bit unstable, but makes keyboard work better when set to "on".


         $_keybint = (on)     # emulate PCish keyboard interrupt







  22..11..66..  XX SSuuppppoorrtt sseettttiinnggss

  If DOSEMU is running in its own X-window (not xterm), you may need to
  tailor it to your needs. Here a summary of the settings and a brief
  description what they mean. A more detailed description of values one
  can be found at chapter 2.2.14 (X Support settings) of README-tech.txt























  $_X_updfreq = (5)       # time between refreshs (units: 20 == 1 second)
  $_X_title = "DOS in a BOX" # Title in the top bar of the window
  $_X_icon_name = "xdos"  # Text for icon, when minimized
  $_X_keycode = (off)     # on == translate keybord via dosemu keytables
  $_X_blinkrate = (8)     # blink rate for the cursor
  $_X_font = ""           # basename from /usr/X11R6/lib/X11/fonts/misc/*
                          # (without extension) e.g. "vga"
  $_X_mitshm = (on)       # Use shared memory extensions
  $_X_sharecmap = (off)   # share the colormap with other applications
  $_X_fixed_aspect = (on) # Set fixed aspect for resize the graphics window
  $_X_aspect_43 = (on)    # Always use an aspect ratio of 4:3 for graphics
  $_X_lin_filt = (off)    # Use linear filtering for >15 bpp interpol.
  $_X_bilin_filt = (off)  # Use bi-linear filtering for >15 bpp interpol.
  $_X_mode13fact = (2)    # initial factor for video mode 0x13 (320x200)
  $_X_winsize = ""        # "x,y" of initial windows size
  $_X_gamma = (1.0)       # gamma correction
  $_X_vgaemu_memsize = (1024) # size (in Kbytes) of the frame buffer
                          # for emulated vga
  $_X_lfb = (on)  # use linear frame buffer in VESA modes
  $_X_pm_interface = (on) # use protected mode interface for VESA modes
  $_X_mgrab_key = ""      # KeySym name to activate mouse grab, empty == off
  $_X_vesamode = ""       # "xres,yres ... xres,yres"
                          # List of vesamodes to add. The list has to contain
                          # SPACE separated "xres,yres" pairs






  22..11..77..  VViiddeeoo sseettttiinnggss (( ccoonnssoollee oonnllyy ))


  _!_!_W_A_R_N_I_N_G_!_!_: _I_F _Y_O_U _E_N_A_B_L_E _G_R_A_P_H_I_C_S _O_N _A_N _I_N_C_O_M_P_A_T_I_B_L_E _A_D_A_P_T_O_R_, _Y_O_U
  _C_O_U_L_D _G_E_T _A _B_L_A_N_K _S_C_R_E_E_N _O_R _M_E_S_S_Y _S_C_R_E_E_N _E_V_E_N _A_F_T_E_R _E_X_I_T_I_N_G _D_O_S_E_M_U_.
  _R_e_a_d _d_o_c_/_R_E_A_D_M_E_-_t_e_c_h_._t_x_t _(_R_e_c_o_v_e_r_i_n_g _t_h_e _c_o_n_s_o_l_e _a_f_t_e_r _a _c_r_a_s_h_)_.

  Start with only text video using the following setup in dosemu.conf



         $_video = "vga"         # one of: plainvga, vga, ega, mda, mga, cga
         $_console = (0)         # use 'console' video
         $_graphics = (0)        # use the cards BIOS to set graphics
         $_videoportaccess = (1) # allow videoportaccess when 'graphics' enabled
         $_vbios_seg = (0xc000)  # set the address of your VBIOS (e.g. 0xe000)
         $_vbios_size = (0x10000)# set the size of your BIOS (e.g. 0x8000)
         $_vmemsize = (1024)     # size of regen buffer
         $_chipset = ""
         $_dualmon = (0)         # if you have one vga _plus_ one hgc (2 monitors)




  After you get it `somehow' working and you have one of the DOSEMU
  supported graphic cards you may switch to graphics mode changing the
  below



         $_graphics = (1)        # use the cards BIOS to set graphics





  If you have a 100% compatible standard VGA card that _m_a_y work,
  however, you get better results, if your card has one of the DOSEMU
  supported video chips and you tell DOSEMU to use it such as



         $_chipset = "s3"        # one of: plainvga, trident, et4000, diamond, s3,
                                 # avance, cirrus, matrox, wdvga, paradise, ati




  Note, `s3' is only an example, you _m_u_s_t set the correct video chip
  else it most like will crash your screen.

  NOTE: `video setting' can _n_o_t be overwritten by  /.dosemurc.




  22..11..88..  DDiisskkss,, bboooott ddiirreeccttoorriieess aanndd ffllooppppiieess


  The parameter settings via dosemu.conf are tailored to fit the
  recommended usage of disk and floppy access. There are other methods
  too, but for these you have to look at README-tech.txt (and you may
  need to modify global.conf).  We strongly recommend that you use the
  proposed techique. Here the normal setup:



         $_vbootfloppy = ""    # if you want to boot from a virtual floppy:
                               # file name of the floppy image under /var/lib/dosemu
                               # e.g. "floppyimage" disables $_hdimage
                               #      "floppyimage +hd" does _not_ disable $_hdimage
         $_floppy_a ="threeinch" # or "fiveinch" or empty, if not existing
         $_floppy_b = ""       # dito for B:

         $_hdimage = "hdimage.first" # list of hdimages or boot directories
                               # under /var/lib/dosemu assigned in this order
                               # such as "hdimage_c hdimage_d hdimage_e"
         $_hdimage_r = $_hdimage # hdimages for 'restricted access (if different)




  When you installed DOSEMU (`make install') you will have a bootable
  hdimage in the file /var/lib/dosemu/hdimage.first. It contains a very
  tiny installation of FreeDos, just to show you that it works. A
  hdimage is a file containing a virtual image of a DOS-FAT filesystem.
  Once you have booted it, you (or autoexec.bat) can use `lredir' to
  access any directory in your Linux tree as DOS drive (a -t msdos
  mounted too). Note, however, that the DosC kernel (FreeDos) is _n_o_t
  capable yet for redirection, for this you need a proprietary DOS such
  as MSDOS, PCDOS, DRDOS.  Look at chapter 6 (Using Lredir) and for more
  details on creating your own (better) hdimage look at chapter 4.3 of
  this README (Making a bootable hdimage for general purpose). Chapter
  4.4 also describes how to import/export files from/to a hdimage.

  Starting with dosemu-0.99.8, there is a more convenient method
  available: you just can have a Linux directory containing all what you
  want to have under your DOS C:. Copy your IO.SYS, MSDOS.SYS or what
  ever to that directory (e.g. /var/lib/dosemu/bootdir), put



     $_hdimage = "bootdir"




  into your /etc/dosemu.conf, and up it goes. DOSEMU makes a lredir'ed
  drive out of it and can boot from it. You can edit the config.sys and
  the autoexec.bat within this directory before you start dosemu.  Fur-
  ther more, you may have a more sohisticated setup. Given you want to
  run the same DOS drive as you normal have when booting into native
  DOS, then you just mount you DOS partition under Linux (say to /dos)
  and put links to its subdirectories into the boot dir. This way you
  can decide which files/directories have to be visible under DOSEMU and
  which have to be different. Here a small and not complete example
  bootdir setup:


         config.sys
         autoexec.bat
         command.com -> /dos/command.com
         io.sys -> /dos/io.sys
         msdos.sys -> /dos/msdos.sys
         dos -> /dos/dos
         bc -> /dos/bc
         windows -> /dos/windows




  There is, however, one drawback, you can't use the DosC kernel
  (FreeDos) for it, because it hasn't yet a working redirector (will
  hopefully be available some time in the future).

  As a further enhancement of your drives setup you may even use the
  following strategie: Given you have the following directory structure
  under /var/lib/dosemu


         /var/lib/dosemu/drives/C
         /var/lib/dosemu/drives/D
         /var/lib/dosemu/drives/E




  and the _C, em/D/, em/E/ beeing symlinks to appropriate DOS useable
  directories, then the following single statement in dosemu.conf


         $_hdimage = "drives/*"




  will assign all these directories to drive C:, D:, E: respectively.
  Note, however, that the _o_r_d_e_r in which the directories under drives/*
  are assigned comes from the order given by /bin/ls. Hence the folling


         /var/lib/dosemu/drives/x
         /var/lib/dosemu/drives/a




  will assign C: to drives/a and D: to drives/x, keep that in mind.
  Now, what does the above `vbootfloppy' mean? Alternatively of booting
  from a virtual `disk' you may have an image of a virtual `floppy'
  which you just created such as `dd if=/dev/fd0 of=floppy_image'. If
  this floppy contains a bootable DOS, then



         $_vbootfloppy = "floppy_image"




  will boot that floppy. Once running in DOS you can make the floppy
  available by (virtually) removing the `media' via `bootoff.com'.  If
  want the disk access specified via `$_hdimage' anyway, you may add the
  keyword `+hd' such as



         $_vbootfloppy = "floppy_image +hd"




  In some rare cases you may have problems accessing Lredir'ed drives
  (especially when your DOS application refuses to run on a 'network
  drive'), though I personally never happened to fall into one of this.
  For this to overcome you may need to use socalled `partition access'.
  The odd with this kind of access is, that you _n_e_v_e_r should have those
  partition mounted in the Linux file system at the same time as you use
  it in DOSEMU (which is quite uncomfortable and dangerous on a
  multitasking OS such as Linux ). Though global.conf checks for mounted
  partitions, there may be races that are not caught. In addition, when
  your DOSEMU crashes, it may leave some FAT sectors unflushed to the
  disk, hence destroying the partition.  Anyway, if you think you need
  it, here is how you `assign' real DOS partitions to DOSEMU:



         $_hdimage = "hdimage.first /dev/hda1 /dev/sdc4:ro"




  The above will have `hdimage.first' as booted drive C:, /dev/hda1 as
  D: (read/write) and /dev/sdc4 as E: (readonly). You may have any kind
  of order within `$_hdimage', hence



         $_hdimage = "/dev/hda1 hdimage.first /dev/sdc4:ro"




  would have /dev/hda1 as booted drive C:. Note that the access to the
  /dev/* devices _m_u_s_t be exclusive (no other process should use it)
  except for `:ro'.



  22..11..99..  CCOOMM ppoorrttss aanndd mmiicceess


  We have simplified the configuration for mices and serial ports and
  check for depencies between them. If all strings in the below example
  are empty, then no mouse and/or COM port is available. Note. that you
  need _n_o mouse.com driver installed in your DOS environment, DOSEMU has
  the mousedriver builtin. The below example is such a setup



         $_com1 = ""           # e.g. "/dev/mouse" or "/dev/cua0"
         $_com2 = "/dev/modem" # e.g. "/dev/modem" or "/dev/cua1"

         $_mouse = "microsoft" # one of: microsoft, mousesystems, logitech,
                               # mmseries, mouseman, hitachi, busmouse, ps2
         $_mouse_dev = "/dev/mouse" # one of: com1, com2 or /dev/mouse
         $_mouse_flags = ""        # list of none or one or more of:
                               # "emulate3buttons cleardtr"
         $_mouse_baud = (0)    # baudrate, 0 == don't set




  The above example lets you have your modem on COM2, COM1 is spare (as
  you may have your mouse under native DOS there and don't want to
  change the configuration of your modem software between boots of
  native DOS and Linux)

  However, you may use your favorite DOS mousedriver and directly let it
  drive COM1 by changing the below variables (rest of variables
  unchanged)



         $_com1 = "/dev/mouse"
         $_mouse_dev = "com1"




  And finaly, when having a PS2 mouse running on your Linuxbox you use
  the builtin mousedriver (not your mouse.com) to get it work: ( again
  leaving the rest of variables unchanged)



         $_mouse = "ps2"
         $_mouse_dev = "/dev/mouse"




  When using a PS2 mouse or when having more then 2 serial ports you may
  of course assign _any_ free serialdevice to COM1, COM2. The order
  doesn't matter:



         $_com1 = "/dev/cua2"
         $_com2 = "/dev/cua0"










  22..11..1100..  PPrriinntteerrss


  Printer is emulated by piping printer data to your normal Linux
  printer.  The belows tells DOSEMU which printers to use. The `timeout'
  tells DOSEMU how long to wait after the last output to LPTx before
  considering the print job as `done' and to to spool out the data to
  the printer.



       $_printer = "lp"        # list of (/etc/printcap) printer names to appear as
                               # LPT1 ... LPT3 (not all are needed, empty for none)
       $_printer_timeout = (20)# idle time in seconds before spooling out





  22..11..1111..  NNeettwwoorrkkiinngg uunnddeerr DDOOSSEEMMUU


  Turn the following option `on' if you require IPX/SPX emulation, there
  is no need to load IPX.COM within the DOS session.  ( the option does
  not emulate LSL.COM, IPXODI.COM, etc. ) And NOTE: You must have IPX
  protocol configured into the kernel.



         $_ipxsupport = (on)




  Enable Novell 8137->raw 802.3 translation hack in new packet driver.



         $_novell_hack = (on)




  If you make use of the dosnet device driver, you may turn on 'multi'
  packet driver support via



         $_vnet = (on)




  For more on this look at chapter 24 (Net code)


  22..11..1122..  SSoouunndd


  The sound driver is more or less likely to be broken at the moment.
  Anyway, here are the settings you would need to emulate a SB-sound
  card by passing the control to the Linux soundrivers.




  $_sound = (off)           # sound support on/off
  $_sb_base = (0x220)       # base IO-address (HEX)
  $_sb_irq = (5)            # IRQ
  $_sb_dma = (1)            # DMA channel
  $_sb_dsp = "/dev/dsp"     # Path the sound device
  $_sb_mixer = "/dev/mixer" # path to the mixer control
  $_mpu_base = "0x330"      # base address for the MPU-401 chip (HEX)
                            # (not yet implemented)





  22..11..1133..  BBuuiillttiinn AASSPPII SSCCSSII DDrriivveerr


  The builtin ASPI driver (a SCSI interface protocol defined by Adaptec)
  can be used to run DOS based SCSI drivers that use this standard (most
  SCSI devices ship with such a DOS driver). This enables you to run
  hardware on Linux, that normally isn't supported otherwise, such as CD
  writers, Scanners e.t.c.  The driver was successfully tested with Dat-
  streamers, EXABYTE tapedrives, JAZ drives (from iomega) and CD
  writers. To make it work under DOSEMU you need

  +o  to configure $_aspi in /etc/dosemu.conf to define which of the
     /dev/sgX devices you want to show up in DOSEMU.

  +o  to load the DOSEMU aspi.sys stub driver within config.sys (e.g.
     DEVICE=ASPI.SYS) _b_e_f_o_r_e any ASPI using driver.

  The $_aspi variable in dosemu.conf takes strings listing all generic
  SCSI devices, that you want give to DOSEMU. NOTE: You should make
  sure, that they are _n_o_t used by Linux elsewere, else you would come
  into _m_u_c_h trouble. To help you not doing the wrong thing, DOSEMU can
  check the devicetype of the SCSI device such as


       $_aspi = "sg2:WORM"




  in which case you define /dev/sg2 beeing a CD writer device. If you
  omit the type,



       $_aspi = "sg2 sg3 sg4"




  DOSEMU will refuse any device that is a disk drive (imagine, what
  would happen if you try to map a CD writer to the disk which contains
  a mounted Linux FS?).  If you _w_a_n_t to map a disk drive to DOSEMU's
  ASPI driver, you need to tell it explicitely


       $_aspi = "sg1:Direct-Access"
       or
       $_aspi = "sg1:0"




  and as you can see, `Direct-Access' is the devicetype reported by
       $ cat /proc/scsi/scsi




  which will list all SCSI devices in the order they are assigned to the
  /dev/sgX devices (the first being /dev/sg0). You may also use the
  DOSEMU supplied tool `scsicheck' (in src/tools/peripher), which helps
  a lot to get the configuration right:


       $ scsicheck
       sg0 scsi0 ch0 ID0 Lun0 ansi2 Direct-Access(0) IBM DCAS-34330 S61A
           $_aspi = "sg0:Direct-Access:0" (or "0/0/0/0:Direct-Access:0")
       sg1 scsi0 ch0 ID5 Lun0 ansi2 Direct-Access(0) IOMEGA ZIP 100 D.08
           $_aspi = "sg1:Direct-Access:5" (or "0/0/5/0:Direct-Access:5")
       sg2 scsi0 ch0 ID6 Lun0 ansi2 CD-ROM(5) TOSHIBA CD-ROM XM-5701TA 0167
           $_aspi = "sg2:CD-ROM:6" (or "0/0/6/0:CD-ROM:6") <== multiple IDs
       sg3 scsi1 ch0 ID4 Lun0 ansi2 Sequential-Access(1) HP C1533A 9503
           $_aspi = "sg3:Sequential-Access:4" (or "1/0/4/0:Sequential-Access:4")
       sg4 scsi1 ch0 ID6 Lun0 ansi1 WORM(4) IMS CDD522/10 1.07
           $_aspi = "sg4:WORM:6" (or "1/0/6/0:WORM:6") <== multiple IDs




  In the above example there are two scsi hostadapters (scsi0 and scsi1)
  and DOSEMU will not show more than _o_n_e hostadapter to DOS (mapping
  them all into one), hence you would get problems accessing sg2 and
  sg4. For this you may remap a different targetID such as


       $_aspi = "sg2:CD-ROM:5 sg4:WORM"




  and all would be fine. From the DOS side the CD-ROM appears as target
  5 and the WORM (CD writer) as target 6.  Also from the above scsicheck
  output, you can see, that you can opt to use a `host/channel/ID/LUN'
  construct in place of `sgX' such as


       $_aspi = "0/0/6/0:CD-ROM:5 1/0/6/0:WORM"




  which is exactly the same as the above example, exept it will assign
  the right device, even if for some reasons you have changed the order
  in which sgX device are assigned by the kernel. Those changes happen,
  if you turned off power of one device `between' or if you play with
  dynamic allocation of scsi devices via the /proc/scsi interface such
  as


       echo "scsi remove-single-device 0 0 5 0" >/proc/scsi/scsi




  to delete a device and


       echo "scsi add-single-device 0 0 5 0" >/proc/scsi/scsi

  to add a device. HOWEVER, we _s_t_r_o_n_g_l_y discourage you to use these ker-
  nel feature for temporaryly switching off power of connected devices
  or even unplugging them: normal SCSI busses are _n_o_t hotpluggable. Dam-
  age may happen and uncontroled voltage bursts during power off/on may
  lock your system !!!

  Coming so far, _o_n_e big problem remains: the (hard coded) buffersize
  for the sg devices in the Linux kernel (default 32k) may be to small
  for DOS applications and, if your distributor yet didn't it, you may
  need to recompile your kernel with a bigger buffer. The buffer size is
  defined in linux/include/scsi/sg.h and to be on the secure side you
  may define


       #define SG_BIG_BUFF (128*1024-512)  /* 128 Kb buffer size */




  though, CD writers are reported to work with 64Kb and the `Iomega
  guest' driver happily works with the default size of 32k.


  33..  SSeeccuurriittyy

  This part of the document by Hans Lermen, <lermen@fgan.de> on Apr 6,
  1997.


  These are the hints we give you, when running dosemu on a machine that
  is (even temporary) connected to the internet or other machines, or
  that otherwise allows 'foreign' people login to your machine.


  +o  Don't set the -s bit, as of dosemu-0.97.10 DOSEMU can run in
     lowfeature mode without the -s bit set. If you want fullfeatures
     for some of your users, just use the keyword `nosuidroot' in
     /etc/dosemu.users to forbid some (or all) users execution of a suid
     root running dosemu (they may use a non-suid root copy of the
     binary though).

  +o  Use proper file permissions to restrict access to a suid root
     DOSEMU binary in addition to /etc/dosemu.users `nosuidroot'.  (
     double security is better ).

  +o  _N_E_V_E_R let foreign users execute dosemu under root login !!!
     (Starting with dosemu-0.66.1.4 this isn't necessary any more, all
     functionality should also be available when running as user)

  +o  Do _n_o_t configure dosemu with the --enable-runasroot option.
     Normally dosemu will switch privileges off at startup and only set
     them on, when it needs them. With '--enable-runasroot' it would
     permanently run under root privileges and only disable them when
     accessing secure relevant resources, ... not so good.

  +o  Never allow DPMI programms to run, when dosemu is suid root.


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
     completely drops prililege, ... hence no danger (will no longer be
     disbaled by 'secure on').

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


         mkdir -p ~/.dosemu/run           # if it doesen't exist
         rm -f ~/.dosemu/run/dosemu-midi
         mknod ~/.dosemu/run/dosemu-midi p





  Then you can use the midi daemon like this:


         ./midid < ~/.dosemu/run/dosemu-midi &; dos





  (Assuming that you put the midid executeable in the directory you run
  DOSEmu from.)


  55..33..  DDiissaabblliinngg tthhee EEmmuullaattiioonn aatt RRuunnttiimmee

  You can now disable the code after it is compiled in by setting the
  IRQ or DMA channels to invalid values (ie IRQ = 0 or > 15, or DMA = 4
  or > 7) The simplest way, however, is to say 'sound_emu off' in
  /etc/dosemu.conf

  66..  UUssiinngg LLrreeddiirr


  This section of the document by Hans, <lermen@fgan.de>. Last updated
  on April, 18 1999.



  What is it? Well, its simply a small DOS program that tells the MFS
  (Mach File System) code what 'network' drives to redirect.  With this
  you can 'mount' any Linux directory as a virtual drive into DOS. In
  addition to this, Linux as well as multiple dosemu sessions may
  simultaneously access the same drives, what you can't when using
  partition access.


  66..11..  hhooww ddoo yyoouu uussee iitt??


  First make sure you aren't using DosC (the FreeDos kernel), because
  unfortunately this can't yet cope with the redirector stuff.

  Mount your dos hard disk partition as a Linux subdirectory.  For
  example, you could create a directory in Linux such as /dos (mkdir -m
  755 /dos) and add a line like

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

  You may even redirect to a NFS mounted volume on a remote machine with
  a /etc/fstab entry like this


          otherhost:      /dos     nfs     nolock




  Note that the _n_o_l_o_c_k> option is _n_e_e_d_e_d for 2.2.x kernels, because
  apparently the locks do not propagate fast enough and DOSEMU's (MFS
  code) share emulation will fail (seeing a lock on its own files).


  In addition, you may want to have your native DOS partion as C: under
  dosemu.  To reach this aim you also can use Lredir to turn off the
  'virtual' hdimage and switch on the _r_e_a_l drive C: such as this:

  Assuming you have a c:\dosemu directory on both drives (the virtual
  and the real one) _a_n_d have mounted your DOS partition as /dosc, you
  then should have the following files on the virtual drive:

  autoexec.bat:



         lredir z: linux\fs\dosc
         copy c:\dosemu\auto2.bat z:\dosemu\auto2.bat
         lredir del z:
         c:\dosemu\auto2.bat




  dosemu\auto2.bat:

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




  1. You have to make 'dos' suid root, if want a fullfeature DOSEMU,
     this normally is done when you run 'make install'.  But as od
     dosemu-0.97.10, you need not if you don't access to ports, external
     DOSish hardware and won't use the console other then in normal
     terminal mode.

  2. You can restrict access to the suid root binary via
     /etc/dosemu.user.  by specifying `nosuidroot' for a given user (or
     all).
  3. Have the users that are allow to execute dosemu in
     /etc/dosemu.user.  The format is:



            loginname [ c_strict ] [ classes ...] [ c_dexeonly ] [ other ]





  For details see ``Configuring DOSEmu''. For a first time _e_a_s_y instal-
  lation you may set it as follows (but don't forget to enhance it later
  !!!):



            root c_all
            lermen c_all
            all nosuidroot c_all





  4. The msdos partitions, that you want to be accessable through
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

  5. Make sure you have read/write permissions of the devices you
     configured (in /etc/dosemu.conf) for serial and mouse.

  Starting with dosemu-0.66.1.4 there should be no reason against
  running dosemu as a normal user. The privilege stuff has been
  extensively reworked, and there was no program that I tested under
  root, that didn't also run as user. However, if you suspect problems
  resulting from the fact that you run as user, you should first try
  configuring dosemu with the as (suid) root and only use user
  privileges when accessing secure relevant resources. Normally dosemu
  will permanently run as user and only temporarily use root privilege
  when needed and in case of non-suid root (as of dosemu-0.97.10), it
  will run in lowfeature mode without any priviledges.


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

  Support for up to 4 drives:

  If you have more then one cdrom, you have to use the cdrom statement
  in global.conf multiple times such as


         cdrom { /dev/cdrom }
         cdrom { /dev/cdrom2 }




  and have multiple instancies of the DOS driver too:


         device=cdrom.sys
         device=cdrom.sys 2




  The one and only parameter to the device driver is a digit between 1
  and 4, (if its missing then 1 is assumed) for the DOS devices
  MSCD0001, MSCD0002 ... MSCD0004 respectively. You then also need to
  tell MSCDEX about these drivers such as



           mscdex /d:mscd0001 /d:mscd0002 /l:driveletter




  In this case the /l: argument defines the driveletter of the first
  /d:, the others will get assigned successive driveletters.

  History: Release with dosemu.0.60.0 Karsten Rucker (rucker@astro.uni-
  bonn.de) April 1995

  Additional remarks for mcd.c and aztcd.c Werner Zimmermann
  (zimmerma@rz.fht-esslingen.de) May 30, 1995

  Release with dosemu-0.99.5 Manuel Villegas Marin espanet.com> Support
  for up to 4 drives December 4, 1998

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

  Steffen Winterfeldt <wfeldt@suse.de>


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

  +o  Unpack the OS2WIN31 files into your WINDOWS\SYSTEM directory.
     (Infact you only need WINDOWS/SYSTEM/os2k386.exe and the mouse
     driver)

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
  DOS applications will accept them, even interactive ones. A '\' is
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


  1133..33..  RRuunnnniinngg DDOOSSEEMMUU wwiitthhiinn aa ccrroonn jjoobb

  When you try to use one of the above to start dosemu out of a crontab,
  then you have to asure, that the process has a proper environement set
  up ( especially the TERM and/or TERMCAP variable ).

  Normally cron would setup TERM=dumb, this is fine because DOSEMU
  recognizes it and internally sets it's own TERMCAP entry and TERM to
  `dosemu-none'.  You may also configure your video to


          # dos ... -I 'video { none }'

  or have a TERM=none to force the same setting.  In all other crontab
  run cases you may get nasty error messages either from DOSEMU or from
  Slang.

  1144..  CCoommmmaannddss && UUttiilliittiieess

  These are some utitlies to assist you in using Dosemu.


  1144..11..  PPrrooggrraammss



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

     +o  lredir D: LINUX\FS\tmp -- redirects /tmp to drive D:

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




  1144..22..  DDrriivveerrss


  These are useful drivers for Dosemu


     ccddrroomm..ssyyss
        allow direct access to CD-ROM drives from Dosemu


     eemmss..ssyyss
        enable EMM in Dosemu


     eemmuuffss..ssyyss
        redirect Unix directory to Dosemu

  1155..  KKeeyymmaappss

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




  1166..  NNeettwwoorrkkiinngg uussiinngg DDOOSSEEmmuu

  A mini-HOWTO from Bart Hartgers <barth@stack.nl> ( for the detailed
  original description see below )


  1166..11..  TThhee DDOOSSNNEETT vviirrttuuaall ddeevviiccee..

  Dosnet.o is a kernel module that implements a special virtual network
  device. In combination with pktdrv.c and libpacket.c, this will enable
  multiple dosemu sessions and the linux kernel to be on a virtual
  network. Each has it's own network device and ethernet address.

  This means that you can telnet or ftp from the dos-session to your
  telnetd/ftpd running in linux and, with IP forwarding enabled in the
  kernel, connect to any host on your network.


  1166..22..  SSeettuupp ffoorr vviirrttuuaall TTCCPP//IIPP

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



  1166..33..  FFuullll DDeettaaiillss

  Modified description of Vinod G Kulkarni <vinod@cse.iitb.ernet.in>

  Allowing a program to have its own network protocol stacks.

  Resulting in multiple dosemu's to use netware, ncsa telnet etc.

  1166..33..11..  IInnttrroodduuccttiioonn

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


  1166..33..22..

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



  1166..33..33..  IImmpplleemmeennttaattiioonn

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


  1166..33..44..  VViirrttuuaall ddeevviiccee ''ddssnn00''

  Compile the module dosnet and insmod it, and give it an IP address,
  with a new IP network number. And You have to set up proper routing
  tables on all machines you want to connect to.  So linux side
  interface is easy to set up.

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

  1166..33..55..  PPaacckkeett ddrriivveerr ccooddee

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


  1166..33..66..  CCoonncclluussiioonn

  So at last one can open multiple DOSEMU's and access network from each
  of them ...  However, you HAVE TO set up ROUTING TABLES etc.

  Vinod G Kulkarni <vinod@cse.iitb.ernet.in>


  1166..33..66..11..  TTeellnneettttiinngg ttoo ootthheerr SSyysstteemmss

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


  1166..33..66..22..  AAcccceessssiinngg NNoovveellll nneettwwaarree

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

  1177..  UUssiinngg WWiinnddoowwss aanndd WWiinnssoocckk

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



  1177..11..  LLIISSTT OOFF RREEQQUUIIRREEDD SSOOFFTTWWAARREE


  +o  The WINPKT.COM virtual packet driver, version 11.2 I have found
     this little tsr in the Crynwr packet driver distribution file
     PKTD11.ZIP

  +o  The Trumpet Winsock 2.0 revision B winsock driver for windows.


  1177..22..  SSTTEEPP BBYY SSTTEEPP OOPPEERRAATTIIOONN ((LLIINNUUXX SSIIDDEE))


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





  1177..33..  SSTTEEPP BBYY SSTTEEPP OOPPEERRAATTIIOONN ((DDOOSS SSIIDDEE))


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








