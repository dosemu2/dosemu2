
DOSEMU

The DOSEMU team

Edited by

Alistair MacDonald

   For DOSEMU v1.2 pl0.0

   This document is the amalgamation of a series of README files which
   were created to deal with the lack of DOSEMU documentation.
     _________________________________________________________________

   Table of Contents
   1. Introduction
   2. Runtime Configuration Options

        2.1. Format of dosemu.conf and ~/.dosemurc

              2.1.1. Disks, boot directories and floppies
              2.1.2. Controling amount of debug output
              2.1.3. Basic emulation settings
              2.1.4. Code page and character set
              2.1.5. Terminals
              2.1.6. Keyboard settings
              2.1.7. X Support settings
              2.1.8. Builtin ASPI SCSI Driver
              2.1.9. COM ports and mices
              2.1.10. Printers
              2.1.11. Sound
              2.1.12. Joystick
              2.1.13. Networking under DOSEMU
              2.1.14. Settings for enabling direct hardware access
              2.1.15. Video settings ( console only )

   3. Security
   4. Sound

        4.1. Using the MPU-401 "Emulation"
        4.2. The MIDI daemon
        4.3. Disabling the Emulation at Runtime

   5. Using Lredir

        5.1. how do you use it?
        5.2. Other alternatives using Lredir

   6. Running dosemu as a normal user
   7. Using CDROMS

        7.1. The built in driver

   8. Using X

        8.1. Latest Info
        8.2. Older information
        8.3. The appearance of Graphics modes (November 13, 1995)

              8.3.1. vgaemu
              8.3.2. vesa
              8.3.3. X

        8.4. The new VGAEmu/X code (July 11, 1997)
        8.5. Planar (16 color and mode-X) modes and fonts. (May 2000)

   9. Running Windows under DOSEMU

        9.1. Windows 3.0 Real Mode
        9.2. Windows 3.1 Protected Mode
        9.3. Windows 3.x in xdos

   10. The DOSEMU mouse

        10.1. Setting up the emulated mouse in DOSEMU
        10.2. Problems

   11. Mouse Garrot
   12. Running a DOS-application directly from Unix shell

        12.1. Using the keystroke and commandline options.
        12.2. Using an input file
        12.3. Running DOSEMU within a cron job

   13. Commands & Utilities

        13.1. Programs
        13.2. Drivers

   14. Keymaps
   15. Networking using DOSEMU

        15.1. Virtual networking.
        15.2. TUN/TAP support.
        15.3. The DOSNET virtual device (deprecated).
        15.4. Full Details

              15.4.1. Introduction
              15.4.2. Design
              15.4.3. Implementation
              15.4.4. Virtual device 'dsn0'
              15.4.5. Packet driver code
              15.4.6. Conclusion

   16. Using Windows and Winsock

        16.1. LIST OF REQUIRED SOFTWARE
        16.2. STEP BY STEP OPERATION (LINUX SIDE)
        16.3. STEP BY STEP OPERATION (DOS SIDE)

1. Introduction

   This documentation is derived from a number of smaller documents. This
   makes it easier for individuals to maintain the documentation relevant
   to their area of expertise. Previous attempts at documenting DOSEMU
   failed because the documentation on a large project like this quickly
   becomes too much for one person to handle.
     _________________________________________________________________

2. Runtime Configuration Options

   This section of the document by Hans, <lermen@fgan.de>. Last updated
   on Sep 27, 2003, by Bart Oldeman.

   Most of DOSEMU configuration is done during runtime and per default it
   can use the system wide configuration file dosemu.conf (which is often
   situated in /etc or /etc/dosemu) optionally followed by the users
   ~/.dosemurc and additional configurations statements on the
   commandline (-I option). If /etc/dosemu.users exists then dosemu.conf
   is searched for in /etc and otherwise in /etc/dosemu (or an
   alternative sysconfdir compiletime-setting).

   The default dosemu.conf and ~/.dosemurc have all settings commented
   out for documentation purposes; the commented out values are the ones
   that DOSEMU uses by default. Note that a non-suid type of installation
   does not need the dosemu.users and dosemu.conf files, and the main
   per-user configuration file is $HOME/.dosemurc. However, for security
   reasons, a suid-root installation will not run without dosemu.users,
   and in that case certain dosemu.conf settings are ignored by
   ~/.dosemurc.

   In fact dosemu.conf and ~/.dosemurc (which have identical syntax) are
   included by the systemwide configuration script global.conf which, by
   default, is built into the DOSEMU binary. As a normal user you won't
   ever think on editing this, only dosemu.conf and your personal
   ~/.dosemurc. The syntax of global.conf is described in detail in
   README-tech.txt, so this is skipped here. However, the option -I
   string too uses the same syntax as global.conf, hence, if you are
   doing some special stuff (after you got familar with DOSEMU) you may
   need to have a look there.

   In DOSEMU prior to 0.97.5 the private configuration file was called
   ~/.dosrc (not to be confused with the new ~/.dosemurc). This will work
   as expected formerly, but is subject to be nolonger supported in the
   near future. This (old) ~/.dosrc is processed after global.conf and
   follows (same as -I) the syntax of global.conf (see README-tech.txt).

   The first file expected (and interpreted) before any other
   configuration (such as global.conf, dosemu.conf and ~/.dosemurc) is
   /etc/dosemu.users or /etc/dosemu/dosemu.users. As mentioned above,
   this file is entirely optional for non-suid-root (default)
   installations. Within this file the general permissions are set:

     * which users are allowed to use DOSEMU.
     * which users are allowed to use DOSEMU suid root.
     * which users are allowed to have a private lib dir.
     * what kind of access class the user belongs to.
     * what special configuration stuff the users needs

   and further more:

     * whether the lib dir (DOSEMU_LIB_DIR) resides elsewhere.
     * setting the loglevel.

   Except for lines starting with `xxx=' (explanation below), each line
   in dosemu.user corresponds to exactly one valid user count, the
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
   disable all secure relevant feature. Setting `guest' will force
   setting `restricted' too.

   The use of `nosuidroot' will force a suid root dosemu binary to exit,
   the user may however use a non-suid root copy of the binary. For more
   information on this look at README-tech, chapter 11.1 `Privileges and
   Running as User'

   Giving the keyword `private_setup' to a user means he/she can have a
   private DOSEMU lib under $HOME/.dosemu/lib. If this directory is
   existing, DOSEMU will expect all normally under DOSEMU_LIB_DIR within
   that directory. As this would be a security risk, it only will be
   allowed, if the used DOSEMU binary is non-suid-root. If you realy
   trust a user you may additionally give the keyword `unrestricted',
   which will allow this user to execute a suid-root binary even on a
   private lib directory (though, be aware).

   In addition, dosemu.users can be used to define some global settings,
   which must be known before any other file is accessed, such as:
      default_lib_dir= /opt/dosemu  # replaces DOSEMU_LIB_DIR
      log_level= 2                  # highest log level

   With `default_lib_dir=' you may move DOSEMU_LIB_DIR elsewhere, this
   mostly is interesting for distributors, who want it elsewhere but
   won't patch the DOSEMU source just for this purpose. But note, the
   dosemu supplied scripts and helpers may need some adaption too in
   order to fit your new directory.

   The `log_level=' can be 0 (never log) or 1 (log only errors) or 2 (log
   all) and controls the ammount written to the systems log facility
   (notice). This keyword replaces the former /etc/dosemu.loglevel file,
   which now is obsolete.

   Nevertheless, for a first try of DOSEMU you may prefer
   etc/dosemu.users.easy, which just contains
      root c_all
      all c_all

   to allow everybody all weird things. For more details on security
   issues have a look at README-tech.txt chapter 2.

   After the file dosemu.users, the file dosemu.conf (via global.conf,
   which may be built-in) is interpreted, and only during global.conf
   parsing the access to all configuration options is allowed.
   dosemu.conf normally lives in the same directory as dosemu.users, for
   instance /etc/dosemu or /etc (that is, sysconfdir in
   compiletime-settings). Your personal ~/.dosemurc is included directly
   after dosemu.conf, but has less access rights (in fact the lowest
   level), all variables you define within ~/.dosemurc transparently are
   prefixed with `dosemu_' such that the normal namespace cannot be
   polluted (and a hacker cannot overwrite security relevant enviroment
   variables). Within global.conf only those ~/.dosemurc created
   variables, that are needed are taken over and may overwrite those
   defined in dosemu.conf.

   The dosemu.conf (global.conf) may check for the configuration
   variables, that are set in dosemu.users and optionaly include further
   configuration files. But once dosemu.conf (global.conf) has finished
   interpretation, the access to secure relevant configurations is
   (class-wise) restricted while the following interpretation of (old)
   .dosrc and -I statements.

   For more details on security settings/issues look at README-tech.txt,
   for now (using DOSEMU the first time) you should need only the below
   description of dosemu.conf (~/.dosemurc)
     _________________________________________________________________

2.1. Format of dosemu.conf and ~/.dosemurc

   All settings are variables, and have the form of

         $_xxx = (n)

   or
         $_zzz = "s"

   where `n' ist a numerical or boolean value and `s' is a string. Note
   that the brackets are important, else the parser won't decide for a
   number expression. For numers you may have complete expressions ( such
   as (2*1024) ) and strings may be concatenated such as

         $_zzz = "This is a string containing '", '"', "' (quotes)"

   Hence a comma separated list of strings is concatenated.
     _________________________________________________________________

2.1.1. Disks, boot directories and floppies

   The parameter settings are tailored to fit the recommended usage of
   disk and floppy access. There are other methods too, but for these you
   have to look at README-tech.txt (and you may need to modify
   global.conf). We strongly recommend that you use the proposed
   techique. Here the normal setup:

    # List of hdimages or boot directories under
    # ~/.dosemu, the system config directory (/etc/dosemu by default), or
    # syshdimagedir (/var/lib/dosemu by default) assigned in this order
    # such as "hdimage_c directory_d hdimage_e"
    # Absolute pathnames are also allowed.
      $_hdimage = "freedos"
      $_vbootfloppy = ""    # if you want to boot from a virtual floppy:
                            # file name of the floppy image under DOSEMU_LIB_DI
R
                            # e.g. "floppyimage" disables $_hdimage
                            #      "floppyimage +hd" does _not_ disable $_hdima
ge
      $_floppy_a ="threeinch" # or "fiveinch" or empty, if not existing
      $_floppy_b = ""       # dito for B:

   A hdimage is a file containing a virtual image of a DOS-FAT
   filesystem. Once you have booted it, you (or autoexec.bat) can use
   `lredir' to access any directory in your Linux tree as DOS drive (a -t
   msdos mounted too). Look at chapter 5 (Using Lredir) and for more
   details. If you want to create your own hdimage use "mkfatimage16"
   (see the manual page). To make it bootable you can make it, say, drive
   F:, and use "SYS F:" at the DOS prompt. The DOSEMU-HOWTO explains how
   to manipulate it using mtools.

   Starting with dosemu-0.99.8, there is a more convenient method
   available: you just can have a Linux directory containing all what you
   want to have under your DOS C:. Copy your IO.SYS, MSDOS.SYS or what
   ever to that directory (e.g. DOSEMU_LIB_DIR/bootdir), set
          $_hdimage = "bootdir"

   and up it goes. Alternatively you can specify an absolute path such as
   "/dos" or "/home/username/dosemu/freedos". DOSEMU makes a lredir'ed
   drive out of it and can boot from it. You can edit the config.sys and
   the autoexec.bat within this directory before you start dosemu.
   Further more, you may have a more sohisticated setup. Given you want
   to run the same DOS drive as you normal have when booting into native
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

   As a further enhancement of your drives setup you may even use the
   following strategie: Given you have the following directory structure
   under DOSEMU_LIB_DIR
      DOSEMU_LIB_DIR/drives/C
      DOSEMU_LIB_DIR/drives/D
      DOSEMU_LIB_DIR/drives/E

   and the C, D, E are symlinks to appropriate DOS useable directories,
   then the following single statement
         $_hdimage = "drives/*"

   will assign all these directories to drive C:, D:, E: respectively.
   Note, however, that the order in which the directories under drives/*
   are assigned comes from the order given by /bin/ls. Hence the folling
      DOSEMU_LIB_DIR/drives/x
      DOSEMU_LIB_DIR/drives/a

   will assign C: to drives/a and D: to drives/x, keep that in mind.

   Now, what does the above `vbootfloppy' mean? Instead of booting from a
   virtual `disk' you may have an image of a virtual `floppy' which you
   just created such as `dd if=/dev/fd0 of=floppy_image'. If this floppy
   contains a bootable DOS, then

         $_vbootfloppy = "floppy_image"

   will boot that floppy. Once running in DOS you can make the floppy
   available by (virtually) removing the `media' via `bootoff.com'. If
   want the disk access specified via `$_hdimage' anyway, you may add the
   keyword `+hd' such as

         $_vbootfloppy = "floppy_image +hd"

   In some rare cases you may have problems accessing Lredir'ed drives
   (especially when your DOS application refuses to run on a 'network
   drive'), For this to overcome you may need to use socalled `partition
   access', use a floppy (or a "vbootfloppy"), or a special-purpose
   hdimage. The odd with partition access is, that you never should have
   those partition mounted in the Linux file system at the same time as
   you use it in DOSEMU (which is quite uncomfortable and dangerous on a
   multitasking OS such as Linux ). Though global.conf checks for mounted
   partitions, there may be races that are not caught. In addition, when
   your DOSEMU crashes, it may leave some FAT sectors unflushed to the
   disk, hence destroying the partition. Anyway, if you think you need
   it, you must have r/w access to the partition in /dev, and you have to
   `assign' real DOS partitions as follows:

         $_hdimage = "hdimage.first /dev/hda1 /dev/sdc4:ro"

   The above will have `hdimage.first' as booted drive C:, /dev/hda1 as
   D: (read/write) and /dev/sdc4 as E: (readonly). You may have any kind
   of order within `$_hdimage', hence

         $_hdimage = "/dev/hda1 hdimage.first /dev/sdc4:ro"

   would have /dev/hda1 as booted drive C:. Note that the access to the
   /dev/* devices must be exclusive (no other process should use it)
   except for `:ro'.
     _________________________________________________________________

2.1.2. Controling amount of debug output

   DOSEMU will help you finding problems, when you enable its debug
   messages. These will go into the file, that you defined via the `-o
   file' or `-O' commandline option (the later prints to stderr). If you
   do not specify any -O or -o switch, then the log output will be
   written to ~/.dosemu/boot.log. You can preset the kind of debug output
   via

         $_debug = "-a"

   where the string contains all you normally may pass to the `-D'
   commandline option (look at the man page for details).
     _________________________________________________________________

2.1.3. Basic emulation settings

   Whether a numeric processor should be shown to the DOS space
         $_mathco = (on)

   Which type of CPU should be emulated (NOTE: this is not the one you
   are running on, but your setting may not exeed the capabilities of the
   running CPU). Valid values are: 80[345]86
         $_cpu = (80386)

   To let DOSEMU use the Pentium cycle counter (if availabe) to do better
   timing use the below

         $_rdtsc = (on)   # or off

   For the above `rdtsc' feature DOSEMU needs to know the exact CPU
   clock, it normally calibrates it itself, but is you encounter a wrong
   mesurement you may overide it such as
         $_cpuspeed = (166.666)  # 0 = let DOSEMU calibrate

   If you have a PCI board you may allow DOSEMU to access the PCI
   configuration space by defining the below
         $_pci = (on)    # or off

   NOTE: `$_pci' can not be overwritten by ~/.dosemurc.

   Starting with dosemu-1.0 there is a flexible way to handle the mapping
   strategy used by DOSEMU, which is needed by video emulation, EMS, DPMI
   and XMS support and other stuff to map a given page of memory to the
   required virtual DOS address space.

   Normally DOSEMU will detect the proper mapping driver for the kernel
   you are using, however, in some cases you may want to define it
   explicitely to overcome eventual problems. For this you can specify
         $_mapping= "mapfile"

   to force the use of the driver, which uses a temporary file. Or on
   kernels up to 2.2.23 and up to 2.3.27.
         $_mapping= "mapself"

   to use mapping on the /proc/self/mem file, which is a lot faster the
   the `mapfile' driver, but maybe not reliable in conjunction with glibc
   (NOTE: the kernel since ages has bugs on /proc/self/mem mapping and
   our workaround doesn't fit exactly together glibc). Kernel 2.2.24
   dropped /proc/self/mem mapping because of security issues.

   Last but not least, if you are using a kernel above 2.3.40, you may
   use

      $_mapping= "mapshm"

   which uses one big IPC shared memory segment as memory pool and the
   new extended mremap() functionality for the mapping.

   Note, that in case of `mapfile' and `mapshm' the size of the file or
   the segment depend on how much memory you configured for XMS, EMS and
   DPMI (see below) and that you should take care yourself that you have
   enough diskspace or have the IPC limits high enough. You can control
   IPC memory limits with
         # ipcs -m -l

   and (on kernels above 2.3.x) you can increase the segment limit with
         # echo "66904064" >/proc/sys/kernel/shmmax

   `66904064' being 64 Mbytes in this example.

   Defining the memory layout, which DOS should see:
      $_xms = (8192)          # in Kbyte
      $_ems = (2048)          # in Kbyte
      $_ems_frame = (0xe000)
      $_dpmi = (0x5000)       # in Kbyte
      $_dosmem = (640)        # in Kbyte, < 640

   Note that (other as in native DOS) each piece of mem is separate,
   hence DOS perhaps will show other values for 'extended' memory.

   If you want mixed operation on the filesystem, from which you boot
   DOSEMU (native and via DOSEMU), it may be necessary to have two
   separate sets of config.sys and system.ini. DOSEMU can fake a
   different file extension, so DOS will get other files when running
   under DOSEMU. Faking autoexec.bat cannot happen in a reliable fashion,
   so if you would like to use an autoexec.bat replacement then just use
   the SHELL command in config.XXX, like this:

   SHELL=COMMAND.COM /P /K AUTOEMU.BAT

      $_emusys = ""    # empty or 3 char., config.sys   -> config.XXX
      $_emuini = ""    # empty or 3 char., system.ini   -> system.XXX

   As you would realize at the first glance: DOS will not have the the
   CPU for its own. But how much it gets from Linux, depends on the
   setting of `hogthreshold'. The HogThreshold value determines how nice
   Dosemu will be about giving other Linux processes a chance to run.

      $_hogthreshold = (1)   # 0 == all CPU power to DOSEMU
                             # 1 == max power for Linux
                             # >1   the higher, the faster DOSEMU will be
     _________________________________________________________________

2.1.4. Code page and character set

   To select the character set and code page for use with DOSEMU you have
         $_term_char_set = "XXX"

   where XXX is one of

   ibm
          With the kbd_unicode plugin (the default), the text is
          processed using cp437->cp437 for the display, so the font used
          must be cp437 (eg cp437.f16 on the console). The text is no
          longer taken without translation. It never really was but it
          was close enough it apparently was used that way. When reading
          characters they are assumed to be in iso-8859-1 from the
          terminal.

          If the old keyboard code is used, then the text is taken
          whithout translation. It is to the user to load a proper DOS
          font (cp437.f16, cp850.f16 or cp852.f16 on the console).

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

         $_external_char_set = "XXX"

   where XXX is one of
    "cp437", "cp737", "cp773", "cp775", "cp850", "cp852", "cp857", "cp860",
    "cp861", "cp862", "cp863", "cp864", "cp865", "cp866", "cp869", "cp874",
    "iso8859-1", "iso8859-2", "iso8859-3", "iso8859-4", "iso8859-5",
    "iso8859-6", "iso8859-7", "iso8859-8", "iso8859-9", "iso8859-14",
    "iso8859-15"

   The external character set is used to:

     * compute the unicode values of characters coming in from the
       terminal
     * compute the character set index of unicode characters output to a
       terminal display screen.
     * compute the unicode values of characters pasted into dosemu.

         $_internal_char_set = "XXX"

   where XXX is one of:
    "cp437", "cp737", "cp773", "cp775", "cp850", "cp852", "cp857", "cp860",
    "cp861", "cp862", "cp863", "cp864", "cp865", "cp866", "cp869", "cp874"

   The internal character set is used to:

     * compute the unicode value of characters of video memory
     * compute the character set index of unicode characters returned by
       bios keyboard translation services.
     _________________________________________________________________

2.1.5. Terminals

   This section applies whenever you run DOSEMU remotely or in an xterm.
   Color terminal support is now built into DOSEMU. Skip this section for
   now to use terminal defaults, until you get DOSEMU to work.
      $_term_color = (on)   # terminal with color support
      $_term_updfreq = (4)  # time between refreshs (units: 20 == 1 second)
      $_escchar = (30)      # 30 == Ctrl-^, special-sequence prefix

   `term_updfreq' is a number indicating the frequency of terminal
   updates of the screen. The smaller the number, the more frequent. A
   value of 20 gives a frequency of about one per second, which is very
   slow. `escchar' is a number (ascii code below 32) that specifies the
   control character used as a prefix character for sending alt, shift,
   ctrl, and function keycodes. The default value is 30 which is Ctrl-^.
   So, for example,
      F1 is 'Ctrl-^1', Alt-F7 is 'Ctrl-^s Ctrl-^7'.
      For online help, press 'Ctrl-^h' or 'Ctrl-^?'.
     _________________________________________________________________

2.1.6. Keyboard settings

   When running DOSEMU from console (also remote from console) or X you
   may need to define a proper keyboard layout. Its possible to let
   DOSEMU do this work automatically for you (see auto below), however,
   this may fail and you'll end up defining it explicitely. This is done
   either by choosing one on the internal keytables or by loading an
   external keytable from DOSEMU_LIB_DIR/keymap/* (which you may modify
   according to your needs). Both sets have identical names (though you
   may add any new one to DOSEMU_LIB_DIR/keymap/*):
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

   which checks for existence of the X keyboard extension and if yes, it
   sets $_X_keycode to 'on', that means the DOSEMU keytables are active.
   The second part (which is independent from $_X_keycode) can be set by
         $_layout = "auto"

   DOSEMU then queries the keyboard layout from the kernel (which only
   does work on console or non-remote X) and generates a new DOSEMU
   keytable out of the kernel information. This currently seems only to
   work for latin-1 layouts, the latin-2 type of accents seem not to
   exist so far in the kernel (linux/keyboard.h). The resulting table can
   be monitor with DOSEMU 'keytable dump' feature (see README-tech.txt)
   for details).

   When being on console you might wish to use raw keyboard, especially
   together with some games, that don't use the BIOS/DOS to get their
   keystrokes.
         $_rawkeyboard = (1)

   However, be carefull, when the application locks, you may not be able
   to switch your console and recover from this. For details on
   recovering look at README-tech.txt (Recovering the console after a
   crash).
     _________________________________________________________________

2.1.7. X Support settings

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
     _________________________________________________________________

2.1.8. Builtin ASPI SCSI Driver

   The builtin ASPI driver (a SCSI interface protocol defined by Adaptec)
   can be used to run DOS based SCSI drivers that use this standard (most
   SCSI devices ship with such a DOS driver). This enables you to run
   hardware on Linux, that normally isn't supported otherwise, such as CD
   writers, Scanners e.t.c. The driver was successfully tested with
   Dat-streamers, EXABYTE tapedrives, JAZ drives (from iomega) and CD
   writers. To make it work under DOSEMU you need

     * to configure $_aspi to define which of the /dev/sgX devices you
       want to show up in DOSEMU.
     * to load the DOSEMU aspi.sys stub driver within config.sys (e.g.
       DEVICE=ASPI.SYS) before any ASPI using driver.

   The $_aspi variable takes strings listing all generic SCSI devices,
   that you want give to DOSEMU. NOTE: You should make sure, that they
   are not used by Linux elsewhere, else you would come into much
   trouble. To help you not doing the wrong thing, DOSEMU can check the
   devicetype of the SCSI device such as
       $_aspi = "sg2:WORM"

   in which case you define /dev/sg2 being a CD writer device. If you
   omit the type,

    $_aspi = "sg2 sg3 sg4"

   DOSEMU will refuse any device that is a disk drive (imagine, what
   would happen if you try to map a CD writer to the disk which contains
   a mounted Linux FS?). If you want to map a disk drive to DOSEMU's ASPI
   driver, you need to tell it explicitely
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
   and DOSEMU will not show more than one hostadapter to DOS (mapping
   them all into one), hence you would get problems accessing sg2 and
   sg4. For this you may remap a different targetID such as
       $_aspi = "sg2:CD-ROM:5 sg4:WORM"

   and all would be fine. From the DOS side the CD-ROM appears as target
   5 and the WORM (CD writer) as target 6. Also from the above scsicheck
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

   to add a device. HOWEVER, we strongly discourage you to use these
   kernel feature for temporaryly switching off power of connected
   devices or even unplugging them: normal SCSI busses are not
   hotpluggable. Damage may happen and uncontroled voltage bursts during
   power off/on may lock your system !!!

   Coming so far, one big problem remains: the (hard coded) buffersize
   for the sg devices in the Linux kernel (default 32k) may be to small
   for DOS applications and, if your distributor yet didn't it, you may
   need to recompile your kernel with a bigger buffer. The buffer size is
   defined in linux/include/scsi/sg.h and to be on the secure side you
   may define
       #define SG_BIG_BUFF (128*1024-512)  /* 128 Kb buffer size */

   though, CD writers are reported to work with 64Kb and the `Iomega
   guest' driver happily works with the default size of 32k.
     _________________________________________________________________

2.1.9. COM ports and mices

   We have simplified the configuration for mice and serial ports and
   check for depencies between them. If all strings in the below example
   are empty, then no mouse and/or COM port is available. Note. that you
   need no mouse.com driver installed in your DOS environment, DOSEMU has
   the mousedriver builtin. The below example is such a setup

      $_com1 = ""           # e.g. "/dev/mouse" or "/dev/cua0"
      $_com2 = "/dev/modem" # e.g. "/dev/modem" or "/dev/cua1"

      $_mouse = "microsoft" # one of: microsoft, mousesystems, logitech,
                            # mmseries, mouseman, hitachi, busmouse, ps2
      $_mouse_dev = "/dev/mouse" # one of: com1, com2 or /dev/mouse
      $_mouse_flags = ""        # list of none or one or more of:
                        # "emulate3buttons cleardtr"
      $_mouse_baud = (0)        # baudrate, 0 == don't set

   The above example lets you have your modem on COM2, COM1 is spare (as
   you may have your mouse under native DOS there and don't want to
   change the configuration of your modem software between boots of
   native DOS and Linux)

   However, you may use your favorite DOS mousedriver and directly let it
   drive COM1 by changing the below variables (rest of variables
   unchanged)

      $_com1 = "/dev/mouse"
      $_mouse_dev = "com1"

   And finaly, when you have a PS2 mouse on your machine you use the
   builtin mousedriver (not your mouse.com) to get it work: ( again
   leaving the rest of variables unchanged)

      $_mouse = "ps2"
      $_mouse_dev = "/dev/mouse"

   When using a PS2 mouse or when having more then 2 serial ports you may
   of course assign _any_ free serialdevice to COM1, COM2. The order
   doesn't matter:

      $_com1 = "/dev/cua2"
      $_com2 = "/dev/cua0"
     _________________________________________________________________

2.1.10. Printers

   Printer is emulated by piping printer data to your normal Linux
   printer. The belows tells DOSEMU which printers to use. The `timeout'
   tells DOSEMU how long to wait after the last output to LPTx before
   considering the print job as `done' and to to spool out the data to
   the printer.

    $_printer = "lp"        # list of (/etc/printcap) printer names to appear a
s
                            # LPT1 ... LPT3 (not all are needed, empty for none
)
    $_printer_timeout = (20)# idle time in seconds before spooling out
     _________________________________________________________________

2.1.11. Sound

   The following settings will tell DOSEMU to emulate an SB16 sound card
   using your Linux sound drivers. For more information see
   sound-usage.txt.

    $_sound = (on)            # sound support on/off
    $_sb_base = (0x220)       # base IO-address (HEX)
    $_sb_irq = (5)            # IRQ
    $_sb_dma = (1)            # Low 8-bit DMA channel
    $_sb_hdma = (5)           # High 16-bit DMA channel
    $_sb_dsp = "/dev/dsp"     # Path to the sound device
    $_sb_mixer = ""       # path to the mixer control
    $_mpu_base = (0x330)      # base address for the MPU-401 chip (HEX)
     _________________________________________________________________

2.1.12. Joystick

   Here are the settings for Joystick emulation.

    $_joy_device = "/dev/js0 /dev/js1"
                          # 1st and 2nd joystick device
                              # e.g. "/dev/js0" or "/dev/js0 /dev/js1"
                              #      (or "" if you don't want joystick support)
                          #
    $_joy_dos_min = (1)   # range for joystick axis readings, must be > 0
    $_joy_dos_max = (150)         # avoid setting this to > 250
    $_joy_granularity = (1)   # the higher, the less sensitive -
                          # useful if you have a wobbly joystick
                              #
    $_joy_latency = (1)       # delay between nonblocking linux joystick reads
                              # increases performance if >0 and processor>=Pent
ium
                              # recommended: 1-50ms or 0 if unsure
     _________________________________________________________________

2.1.13. Networking under DOSEMU

   Turn the following option `on' if you require IPX/SPX emulation, there
   is no need to load IPX.COM within the DOS session. ( the option does
   not emulate LSL.COM, IPXODI.COM, etc. ) And NOTE: You must have IPX
   protocol configured into the kernel.

         $_ipxsupport = (on)

   Enable Novell 8137->raw 802.3 translation hack in new packet driver.

         $_novell_hack = (on)

   If you make use of TUN/TAP or the dosnet device driver, then you can
   select it via

         $_vnet = "tap"

   or
         $_vnet = "dosnet"

   But if you just need 'single' packet driver support that talks to, for
   instance, your ethernet card eth0 then you need to set

         $_netdev = "eth0"

   Note that dosnet and eth0 require raw packet access, and hence
   (suid)-root. If $_vnet = "dosnet", then $_netdev will default to
   "dsn0". If you would like to use persistent TUN/TAP devices then you
   need to specifify the TAP device in $_netdev. For more on this look at
   chapter 15 (Networking using DOSEMU).
     _________________________________________________________________

2.1.14. Settings for enabling direct hardware access

   The following settings (together with the direct console video
   settings below make it possible for DOSEMU to access your real
   (non-emulated) computer hardware directly. Because Linux does not
   permit this for ordinary users, DOSEMU needs to be run as root or
   suid-root to be able to use these settings. They can NOT be
   overwritten by the user configuration file ~/.dosemurc.

   Here you tell DOSEMU what to do when DOS wants let play the speaker:
         $_speaker = ""     # or "native" or "emulated"

   And with the below may gain control over real ports on you machine.
   But:

   WARNING: GIVING ACCESS TO PORTS IS BOTH A SECURITY CONCERN AND SOME
   PORTS ARE DANGEROUS TO USE. PLEASE SKIP THIS SECTION, AND DON'T FIDDLE
   WITH THIS SECTION UNLESS YOU KNOW WHAT YOU'RE DOING.

      $_ports = ""  # list of portnumbers such as "0x1ce 0x1cf 0x238"
                    # or "0x1ce range 0x280,0x29f 310"
                    # or "range 0x1a0,(0x1a0+15)"

   If you have hardware, that is not supported under Linux but you have a
   DOS driver for, it may be necessary to enable IRQ passing to DOS.
      $_irqpassing = ""  # list of IRQ number (2-15) to pass to DOS such as
                         # "3 8 10"
     _________________________________________________________________

2.1.15. Video settings ( console only )

   !!WARNING!!: IF YOU ENABLE GRAPHICS ON AN INCOMPATIBLE ADAPTOR, YOU
   COULD GET A BLANK SCREEN OR MESSY SCREEN EVEN AFTER EXITING DOSEMU.
   Read doc/README-tech.txt (Recovering the console after a crash).

   Start with only text video using the following setup

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

   If you have a 100% compatible standard VGA card that may work,
   however, you get better results, if your card has one of the DOSEMU
   supported video chips and you tell DOSEMU to use it such as

      $_chipset = "s3"        # one of: plainvga, trident, et4000, diamond, s3,
                              # avance, cirrus, matrox, wdvga, paradise, ati, s
is,
                          # svgalib

   Note, `s3' is only an example, you must set the correct video chip
   else it most like will crash your screen.

   The 'svgalib' setting uses svgalib 1.4.2 or greater for determining
   the correct video chip. It should work with all svgalib drivers,
   except for "vesa" and "ati", which are dangerous with respect to
   opening IO-ports.

   NOTE: `video setting' can not be overwritten by ~/.dosemurc.
     _________________________________________________________________

3. Security

   This part of the document by Hans Lermen, <lermen@fgan.de> on Apr 6,
   1997.

   These are the hints we give you, when running dosemu on a machine that
   is (even temporary) connected to the internet or other machines, or
   that otherwise allows 'foreign' people login to your machine.

     * Don't set the -s bit, as of dosemu-0.97.10 DOSEMU can run in
       lowfeature mode without the -s bit set. If you want fullfeatures
       for some of your users, just use the keyword `nosuidroot' in
       /etc/dosemu.users to forbid some (or all) users execution of a
       suid root running dosemu (they may use a non-suid root copy of the
       binary though) or let them use DOSEMU though "sudo". DOSEMU now
       drops it root privileges before booting; however there may still
       be security problems in the initialization code, and by making
       DOSEMU suid-root you can give users direct access to resources
       they don't normally have access too, such as selected I/O ports,
       hardware IRQs and hardware RAM.
       If DOSEMU is invoked via "sudo" then it will automatically switch
       to the user who invoked "sudo". An example /etc/sudoers entry is
       this:

    joeuser  hostname=(root) NOPASSWD: /usr/local/bin/dosemu.bin

       If you use PASSWD instead of NOPASSWD then users need to type
       their own passwords when sudo asks for it. The "dosemu" script can
       be invoked using the "-s" option to automatically use sudo.
     * Use proper file permissions to restrict access to a suid root
       DOSEMU binary in addition to /etc/dosemu.users `nosuidroot'. (
       double security is better ).
     * NEVER let foreign users execute dosemu under root login !!!
       (Starting with dosemu-0.66.1.4 this isn't necessary any more, all
       functionality should also be available when running as user)
     _________________________________________________________________

4. Sound

   The SB code is currently in a state of flux. Some changes to the code
   have been made which mean that I can separate the DSP handling from
   the rest of the SB code, making the main case statements simpler. In
   the meantime, Rutger Nijlunsing has provided a method for redirecting
   access to the MPU-401 chip into the main OS.
     _________________________________________________________________

4.1. Using the MPU-401 "Emulation"

   The Sound driver opens "/var/run/dosemu-midi" and writes the Raw MIDI
   data to this. A daemon is provided which can be can be used to seletc
   the instruments required for use on some soundcards. It is also
   possible to get various instruments by redirecting
   '/var/run/dosemu-midi' to the relevant part of the sound driver eg:

       % ln -s /dev/midi /var/run/dosemu-midi

   This will send all output straight to the default midi device and use
   whatever instruments happen to be loaded.
     _________________________________________________________________

4.2. The MIDI daemon

         make midid

   This compiles and installs the midi daemon. The daemon currently has
   support for the 'ultra' driver and partial support for the 'OSS'
   driver (as supplied with the kernel) and for no midi system. Support
   for the 'ultra' driver will be compiled in automatically if available
   on your system.

   Copy the executable './bin/midid' so that it is on your path, or
   somewhere you can run it easily.

   Before you run DOSEMU for the first time, do the following:
      mkdir -p ~/.dosemu/run           # if it doesen't exist
      rm -f ~/.dosemu/run/dosemu-midi
      mknod ~/.dosemu/run/dosemu-midi p

   Then you can use the midi daemon like this:
         ./midid < ~/.dosemu/run/dosemu-midi &; dosemu

   (Assuming that you put the midid executeable in the directory you run
   DOSEMU from.)
     _________________________________________________________________

4.3. Disabling the Emulation at Runtime

   You can disable the SB emulation by changing the 'sound' variable in
   /etc/dosemu.conf to 'off'. There is currently no way to specify at
   runtime which SB model DOSEMU should emulate; the best you can do is
   set the T value of the BLASTER environment variable (see
   sound-usage.txt), but not all programs will take note of this.
     _________________________________________________________________

5. Using Lredir

   This section of the document by Hans, <lermen@fgan.de>. Last updated
   on October, 23 2002.

   What is it? Well, its simply a small DOS program that tells the MFS
   (Mach File System) code what 'network' drives to redirect. With this
   you can 'mount' any Linux directory as a virtual drive into DOS. In
   addition to this, Linux as well as multiple dosemu sessions may
   simultaneously access the same drives, what you can't when using
   partition access.
     _________________________________________________________________

5.1. how do you use it?

   Mount your dos hard disk partition as a Linux subdirectory. For
   example, you could create a directory in Linux such as /dos (mkdir -m
   755 /dos) and add a line like

          /dev/hda1       /dos     msdos   umask=022

   to your /etc/fstab. (In this example, the hard disk is mounted read-
   only. You may want to mount it read/write by replacing "022" with
   "000" and using the -m 777 option with mkdir). Now mount /dos. Now you
   can add a line like

         lredir d: linux\fs/dos

   to the AUTOEXEC.BAT file in your hdimage (but see the comments below).
   On a multi-user system you may want to use

         lredir d: linux\fs\${home}

   where "home" is the name of an environmental variable that contains
   the location of the dos directory (/dos in this example)

   You may even redirect to a NFS mounted volume on a remote machine with
   a /etc/fstab entry like this
          otherhost:      /dos     nfs     nolock

   Note that the nolock> option might be needed for 2.2.x kernels,
   because apparently the locks do not propagate fast enough and DOSEMU's
   (MFS code) share emulation will fail (seeing a lock on its own files).

   In addition, you may want to have your native DOS partion as C: under
   dosemu. To reach this aim you also can use Lredir to turn off the
   'virtual' hdimage and switch on the real drive C: such as this:

   Assuming you have a c:\dosemu directory on both drives (the virtual
   and the real one) and have mounted your DOS partition as /dosc, you
   then should have the following files on the virtual drive:

   autoexec.bat:

      lredir z: linux\fs\dosc
      copy c:\dosemu\auto2.bat z:\dosemu\auto2.bat
      lredir del z:
      c:\dosemu\auto2.bat

   dosemu\auto2.bat:

      lredir c: linux\fs\dosc
      rem further autoexec stuff

   To make the reason clear why the batch file (not necessaryly
   autoexec.bat) must be identical:

   Command.com, which interpretes the batchfile keeps a position pointer
   (byte offset) to find the next line within this file. It opens/closes
   the batchfile for every new batchline it reads from it. If the
   batchfile in which the 'lredir c: ...' happens is on c:, then
   command.com suddenly reads the next line from the batchfile of that
   newly 'redired' drive. ... you see what is meant?
     _________________________________________________________________

5.2. Other alternatives using Lredir

   To have a redirected drive available at time of config.sys you may
   either use emufs.sys such as

          device=c:\emufs.sys /dosc

   or make use of the install instruction of config.sys (but not both)
   such as

          install=c:\lredir.exe c: linux\fs\dosc

   The later has the advantage, that you are on your native C: from the
   beginning, but, as with autoexec.bat, both config.sys must be
   identical.

   For information on using 'lredired' drives as a 'user' (ie having the
   right permissions), please look at the section on Running dosemu as a
   normal user.
     _________________________________________________________________

6. Running dosemu as a normal user

   This section of the document by Hans, <lermen@fgan.de>. Last updated
   on Jan 21, 2003.

    1. In the default setup, DOSEMU does not have root privileges. This
       means it will not have direct access to ports, external DOSish
       hardware and won't use the console other than in normal terminal
       mode, but is fully capable to do anything else. See the previous
       section on how to enable privileged operation if you really need
       to.
    2. If a user needs access to privileged resources other than console
       graphics, then you need to explicitly allow the user to do so by
       editing the file /etc/dosemu.users (or /etc/dosemu/dosemu.users).
       The format is:

         loginname [ c_strict ] [ classes ...] [ other ]

       For example, to allow joeuser full access you can use

         joeuser c_all

    3. The msdos partitions, that you want to be accessable through
       Section 5 should be mounted with proper permissions. I recommend
       doing this via 'group's, not via user ownership. Given you have a
       group 'dosemu' for this and want to give the user 'lermen' access,
       then the following should be
          + in /etc/passwd:

         lermen:x:500:100:Hans Lermen:/home/lermen:/bin/bash
                      ^^^-- note: this is NOT the group id of 'dosemu'

          + in /etc/group:

         users:x:100:
         dosemu:x:200:dosemu,lermen
                  ^^^

          + in /etc/fstab:

         /dev/hda1 /dosc msdos defaults,gid=200,umask=002 0 0
                                            ^^^

       Note: the changes to /etc/passwd and /etc/group only take place
       the next time you login, so don't forget to re-login.
       The fstab entry will mount /dosc such that is has the proper
       permissions

       ( drwxrwxr-x  22 root     dosemu      16384 Jan  1  1970 /dosc )

       You can do the same with an explicit mount command:

          mount -t msdos -o gid=200,umask=002 /dev/hda1 /dosc

       Of course normal lredir'ed unix directories should have the same
       permissions.
    4. Make sure you have read/write permissions of the devices you
       configured (in /etc/dosemu.conf) for serial and mouse.

   Starting with dosemu-0.66.1.4 there should be no reason against
   running dosemu as a normal user. The privilege stuff has been
   extensively reworked, and there was no program that I tested under
   root, that didn't also run as user. Normally dosemu will permanently
   run as user and only temporarily use root privilege when needed
   (during initialization) and then drop its root privileges permanently.
   In case of non-suid root (as of dosemu-0.97.10), it will run in
   lowfeature mode without any privileges.
     _________________________________________________________________

7. Using CDROMS

7.1. The built in driver

   This documents the cdrom extension rucker@astro.uni-bonn.de has
   written for Dosemu.

   The driver consists of a server on the Linux side
   (dosemu/drivers/cdrom.c, accessed via int 0xe6 handle 0x40) and a
   device driver (dosemu/commands/cdrom.S) on the DOS side.

   Please send any suggestions and bug reports to
   <rucker@astro.uni-bonn.de>

   To install it:

     * Create a (symbolic) link /dev/cdrom to the device file of your
       drive or use the cdrom statement in /etc/dosemu.conf to define it.
     * Make sure that you have read/write access to the device file of
       your drive, otherwise you won't be able to use the cdrom under
       Dosemu directly because of security reasons.
     * Load cdrom.sys within your config.sys file with e.g.:

        devicehigh=c:\emu\cdrom.sys

     * Start Microsoft cdrom extension as follows:

        mscdex /d:mscd0001 /l:driveletter

   To change the cd while Dosemu is running, use the DOS program
   'eject.com'. It is not possible to change the disk, when the drive has
   been opened by another process (e.g. mounted)!

   Remarks by zimmerma@rz.fht-esslingen.de:

   This driver has been successfully tested with Linux' SCSI CDROMS by
   the author, with the Mitsumi driver mcd.c and with the
   Aztech/Orchid/Okano/Wearnes- CDROM driver aztcd.c by me. With the
   latter CDROM-drives changing the CD-ROM is not recognized correctly by
   the drive under all circumstances and is therefore disabled. So
   eject.com will not work. For other CD-ROM drives you may enable this
   feature by setting the variable 'eject_allowed = 1' in file
   dosemu/drivers/cdrom.c (you'll find it near the top of the file). With
   the mcd.c and aztcd.c Linux drivers this may cause your system to hang
   for some 30 seconds (or even indefinitely), so don't change the
   default value 'eject_allowed = 0'.

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

   History:

   Release with dosemu.0.60.0 Karsten Rucker (rucker@astro.uni-bonn.de)
   April 1995

   Additional remarks for mcd.c and aztcd.c Werner Zimmermann
   (zimmerma@rz.fht-esslingen.de) May 30, 1995

   Release with dosemu-0.99.5 Manuel Villegas Marin (manolo@espanet.com)
   Support for up to 4 drives December 4, 1998
     _________________________________________________________________

8. Using X

   Please read all of this for a more complete X information ;-)
     _________________________________________________________________

8.1. Latest Info

   From Uwe Bonnes <bon@elektron.ikp.physik.th-darmstadt.de>:

   xdosemu (dosemu.bin -X) is now a lot more capable. In particular it
   should now understand the keys from the keypad-area (the keys the most
   right on a MF-Keyboard) and numlock and keyevents in the range of the
   latin characters, even when you run xdosemu from a remote X-terminal.
   It also is capable to handle PL4 color modes in addition to 256 color
   modes, so bgidemo from Borland runs fine.
     _________________________________________________________________

8.2. Older information

   From Rainer Zimmermann <zimmerm@mathematik.uni-marburg.de>

   Some basic information about DOSEMU's X support. Sometimes it's even
   sort of useable now.

   What you should take care of:

     * If backspace and delete do not work, you can try 'xmodmap -e
       "keycode 22 = 0xff08"' to get use of your backspace key, and
     * 'xmodmap -e "keycode 107 = 0xffff"' to get use of your delete key.
     * Make sure DOSEMU has X support compiled in. (X_SUPPORT = 1 in the
       Makefile)
     * you should have the vga font installed. See README.ncurses.
     * start DOSEMU with `xdosemu' (or `dosemu.bin -X' when bypassing the
       wrapper) from an X terminal window. Or make a link to 'dosemu.bin'
       named 'xdos' - when called by that name, DOSEMU will automatically
       assume -X. There is also a new debug flag 'X' for X-related
       messages. To exit xdosemu, use 'exitemu' or select 'Close' aka
       'Delete' (better not 'Destroy') from the 'Window' menu. It is also
       save to use <Ctrl><Alt><PgDn> within the window to exit DOSEMU.
       Use <Ctrl><Alt><Pause> to pause and unpause the DOSEMU session,
       which is useful if you want it to sit silently in the background
       when it is eating too much CPU time.
     * there are some X-related configuration options for dosemu.conf.
       See ./etc/dosemu.conf for details.
     * Keyboard support of course depends on your X keyboard mappings
       (xmodmap). If certain keys don't work (like Pause, Backspace,...),
       it *might* be because you haven't defined them in your xmodmap, or
       defined to something other than DOSEMU expects.
     * using the provided icon (dosemu.xpm):
          + you need the xpm (pixmaps) package. If you're not sure, look
            for a file like /lib/libXpm.so.*
          + you also need a window manager which supports pixmaps. Fvwm
            is fine, but I can't tell you about others. Twm probaby won't
            do.
          + copy dosemu.xpm to where you usually keep your pixmap (not
            bitmap!) files (perhaps /usr/include/X11/pixmaps)
          + tell your window manager to use it. For fvwm, add the
            following line to your fvwmrc file:


         Icon "xdosemu"   dosemu.xpm

            This assumes you have defined a PixmapPath. Otherwise,
            specify the entire pathname.
          + note that if you set a different icon name than "xdosemu" in
            your dosemu.conf, you will also have to change the fvwmrc
            entry.
          + restart the window manager. There's usually an option in the
            root menu to do so.
       Now you should see the icon whenever you iconify xdosemu.
       Note that the xdosemu program itself does not include the icon -
       that's why you have to tell the window manager. I chose to do it
       this way to avoid xdosemu requiring the Xpm library.
     * If anything else does not work as expected, don't panic :-)
       Remember the thing is still under construction. However, if you
       think it's a real bug, please tell me.
     _________________________________________________________________

8.3. The appearance of Graphics modes (November 13, 1995)

   Erik Mouw <J.A.K.Mouw@et.tudelft.nl> & Arjan Filius
   <I.A.Filius@et.tudelft.nl>

   We've made some major changes in X.c that enables X to run graphics
   modes. Unfortunately, this disables the cut-and-paste support, but we
   think the graphics stuff is much more fun (after things have
   established, we'll put the cut-and-paste stuff back). The graphics is
   done through vgaemu, the VGA emulator. Status of the work:
     _________________________________________________________________

8.3.1. vgaemu

     * Video memory. 1 Mb is allocated. It is mapped with mmap() in the
       VGA memory region of DOSEMU (0xa00000-0xbfffff) to support bank
       switching. This is very i386-Linux specific, don't be surprised if
       it doesn't work under NetBSD or another Linux flavour
       (Alpha/Sparc/MIPS/etc).
     * The DAC (Digital to Analog Converter). The DAC is completely
       emulated, except for the pelmask. This is not difficult to
       implement, but it is terribly slow because a change in the pelmask
       requires a complete redraw of the screen. Fortunately, the pelmask
       changes aren't used often so nobody will notice ;-)
     * The attribute controller is partially emulated. (Actually, only
       reads and writes to the ports are emulated)
     * The working modes are 0x13 (320x200x256) and some other 256 color
       modes.
     * To do (in no particular order): font support in graphics modes
       (8x8, 8x16, 9x16, etc), text mode support, 16, 4 and 2 color
       support, better bank switching, write the X code out of vgaemu to
       get it more generic.
     _________________________________________________________________

8.3.2. vesa

     * VESA set/get mode, get information and bankswitch functions work.
     * All VESA 256 color (640x480, 800x600, 1024x768) modes work, but
       due to bad bank switch code in vgaemu they won't display right.
     * A VESA compatible video BIOS is mapped at 0xc00000. It's very
       small, but in future it's a good place to store the BIOS fonts
       (8x8, 8x16) in.
     * To do: implement the other VESA functions.
     _________________________________________________________________

8.3.3. X

     * Added own colormap support for the 256 color modes.
     * Support for vgaemu.
     * Some cleanups.
     * To do: remove text support and let vgaemu do the text modes, put
       back the cut-and-paste stuff, more cleanups.
     * NOTE: we've developed on X servers with 8 bit pixel depths
       (XF86_SVGA) so we don't know how our code behaves on other pixel
       depths. We don't even know if it works.

   As stated before, this code was written for Linux (tested with 1.2.13
   and 1.3.39) and we don't know if it works under NetBSD. The mmap() of
   /proc/self/mem and mprotect() magic in vgaemu are very (i386) Linux
   specific.

   Erik
     _________________________________________________________________

8.4. The new VGAEmu/X code (July 11, 1997)

   Steffen Winterfeldt <wfeldt@suse.de>

   I've been working on the X code and the VGA emulation over the last
   few months. This is the outcome so far:

     * graphics support in X now works on all X servers with color depth
       >= 8
     * the graphics window is resizeable
     * support for hi- and true-color modes (using Trident SVGA mode
       numbers and bank switching)
     * some basic support for mode-X type graphics modes (non-chain4
       modes as used by e.g. DOOM)
     * some even more basic support for 16 color modes
     * nearly full VESA 2.0 support
     * gamma correction for graphics modes
     * video memory size is configurable via dosemu.conf
     * initial graphics window size is configurable

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
   it's plain C for now). If the bilinear filter is too slow, you might
   instead try the linear filter which interpolates only horizontally.

   Note that (bi)linear filtering is not available on all VGA/X display
   combinations. The standard drawing routines are used instead in such
   cases.

   If a VGA mode is not supported on your current X display, the graphics
   screen will just remain black. Note that this does not mean that
   xdosemu has crashed.

   The VESA support is (or should be) nearly VBE 2.0 compatible. As a
   reference I used several documents including the unofficial VBE 2.0
   specs made available by SciTech Software. I checked this against some
   actual implementations of the VBE 2.0 standard, including SciTech's
   Display Doctor (formerly known as UniVBE). Unfortunately
   implementations and specs disagree at some points. In such cases I
   assumed the actual implementation to be correct.

   The only unsupported VBE function is VGA state save/restore. But this
   functionality is rarely used and its lack should not cause too much
   problems.

   VBE allows you to use the horizontal and vertical scrolling function
   even in text modes. This feature is not implemented.

   If you think it causes problems, the linear frame buffer (LFB) can be
   turned of via dosemu.conf as well as the protected mode interface.
   Note, however, that LFB modes are faster than banked modes, even in
   DOSEMU.

   The default VBE mode list defines a lot of medium resolution modes
   suitable for games (like Duke3D). You can still create your own modes
   via dosemu.conf. Note that you cannot define the same mode twice; the
   second (and all subsequent) definitions will be ignored.

   Modes that are defined but cannot be supported due to lack of video
   memory or because they cannot be displayed on your X display, are
   marked as unsupported in the VBE mode list (but are still in it). Note
   that there is currently no support of 4 bit VESA modes.

   The current interface between VGAEmu and X will try to update all
   invalid video pages at a time. This may, particularly in hi-res
   VBE/SVGA modes, considerably disturb DOSEMU's signal handling. That
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
   as not to confuse DOS applications. Note that whatever value you give,
   4 bit modes are only supported up to a size of 800x600.

   You can influence the initial size of the graphics window in various
   ways. Normally it will have the same size (in pixel) as the VGA
   graphics mode, except for mode 0x13 (320x200, 256 colors), which will
   be scaled by the value of mode13fact (defaults to 2). Alternatively,
   you can directly specify a window size in dosemu.conf via winsize. You
   can still resize the window later.

   The config option fixed_aspect allows you to fix the aspect ratio of
   the graphics window while resizing it. Alternatively, aspect_43 ties
   the aspect ratio to a value of 4:3. The idea behind this is that,
   whatever the actual resolution of a graphics mode is in DOS, it is
   displayed on a 4:3 monitor. This way you won't run into problems with
   modes such as 640x200 (or even 320x200) which would look somewhat
   distorted otherwise.
     _________________________________________________________________

8.5. Planar (16 color and mode-X) modes and fonts. (May 2000)

   All standard vga modes work now. For planar modes, vgaemu has to
   switch to part ial cpu emulation. This can be slow, but expect it to
   improve over time. The mode-X applications work fine as well, since
   the cpu emulation they need is exactly the same as for the planar 16
   color modes. Applications that ask for a 320x240 or other non-standard
   mode, for instance, have this request honoured by vgaemu and should
   work fine as well.

   Fonts are present in the VESA bios now. They should work fine in all
   graphics modes.
     _________________________________________________________________

9. Running Windows under DOSEMU

   Okay, perhaps you've heard the hooplah. DOSEMU can run Windows (sort
   of.)
     _________________________________________________________________

9.1. Windows 3.0 Real Mode

   DOSEMU has been able to run Windows 3.0 in Real Mode for some time
   now.
     _________________________________________________________________

9.2. Windows 3.1 Protected Mode

   It is possible to boot WINOS2 (the modified version of Windows 3.1
   that OS/2 uses) under DOSEMU. Many kudos to Lutz & Dong! But, as far
   as we know, you cannot install Windows 3.1 in DOSEMU, but you must do
   that in real mode DOS. Windows 3.1 also does not run in FreeDOS.

   However, YOU NEED BOTH LICENSES, for WINDOWS-3.1 as well OS/2 !!!

     * Get DOSEMU & the Linux source distributions.
     * Unpack DOSEMU.
     * Configure DOSEMU typing './default-configure'.
     * Compile DOSEMU typing 'make'.
     * Get the OS2WIN31.ZIP distribution from somehere. One place that
       distributes it is http://www.funet.fi/pub/os2/32bit/win_os2
     * Unpack the OS2WIN31 files into your WINDOWS\SYSTEM directory.
       (Infact you only need WINDOWS/SYSTEM/os2k386.exe and the mouse
       driver)
     * Startup dosemu (make certain that DPMI is set to a value such as
       4096)
     * Copy the file winemu.bat to your c: drive.
     * Cross your fingers.

   Good luck!
     _________________________________________________________________

9.3. Windows 3.x in xdos

   As of version 0.64.3 DOSEMU is able to run Windows in xdosemu. It is
   safer then starting windows-31 on the console, because when it
   crashes, it doesn't block your keyboard or freeze your screen.

   Hints:

     * Get Dosemu & Linux source.
     * Unpack dosemu.
     * Run "./default-configure" to configure Dosemu.
     * Type "make" to compile.
     * For faster graphics (256 colors instead of 16 is faster in
       xdosemu), get a Trident SVGA drivers for Windows. The files are
       tvgaw31a.zip and/or tvgaw31b.zip. They are available at
       garbo.uwasa.fi in /windows/drivers (any mirrors?).
     * Unpack the Trident drivers.
     * In Windows setup, install the Trident "800x600 256 color for 512K
       boards" driver.
     * Do the things described above to get and install OS2WIN31.
     * Start xdos.
     * In Dosemu, go to windows directory and start winemu.
     * Cross your fingers.

   Notes for the mouse under win31-in-xdos:

     * In order to let the mouse properly work you need the following in
       your win.ini file:

         [windows]
         MouseThreshold1=0
         MouseThreshold2=0
         MouseSpeed=0

     * The mouse cursor gets not painted by X, but by windows itself, so
       it depends on the refresh rate how often it gets updated, though
       the mouse coordinates movement itself will not get delayed. ( In
       fact you have 2 cursors, but the X-cursor is given an 'invisible'
       cursor shape while within the DOS-Box. )
     * Because the coordinates passed to windows are interpreted
       relatively, we need to calibrate the cursor. This is done
       automatically whenever you enter the DOS-Box window: The cursor
       gets forced to 0,0 and then back to its right coordinates. Hence,
       if you want to re-calibrate the cursor, just move the cursor
       outside and then inside the DOS-Box again.
     _________________________________________________________________

10. The DOSEMU mouse

   This section written by Eric Biederman <eric@dosemu.org>
     _________________________________________________________________

10.1. Setting up the emulated mouse in DOSEMU

   For most dos applications you should be able to use the internal mouse
   with very little setup, and very little trouble.

   Under X you don't need to do anything, unless you want to use the
   middle button then you need to add to autoexec.bat:
       emumouse 3

   On the console it takes just a tad bit more work:

   in /etc/dosemu.conf:
    $_mouse = "mousesystems"
    $_mouse_dev = "/dev/gpmdata"

   And in autoexec.bat:
       emumouse 3

   This sets you up to use the gpm mouse repeater if you don't use the
   repeater, you need to set $_mouse and $_mouse_dev to different values.
     _________________________________________________________________

10.2. Problems

   In X there are 2 ways applications can get into trouble.

   The most common way is if they don't trust the mouse driver to keep
   track of where the mouse is on the screen, and insist on doing it
   themselves. win31 & geoworks are too applications in this category.
   They read mouse movement from the mouse driver in turns of mickeys
   i.e. they read the raw movement data.

   To support this mouse driver then tracks where you were and where you
   move to, in terms of x,y screen coordinates. Then works the standard
   formulas backwards that calculate screen coordinates from mickeys to
   generate mickeys from screen coordinates. And it feeds these mickeys
   to the application. As long as the application and dosemu agree on the
   same formulas for converting mickeys to screen coordinates all is
   good.

   The only real problem with this is sometimes X mouse and the
   application mouse get out of sync. Especially if you take your mouse
   cursor out of the dosemu window, and bring it back in again.

   To compensate for getting out of sync what we do is whenever we
   reenter the Xdos window is send a massive stream of mickeys heading
   for the upper left corner of the screen. This should be enough to kick
   us any an good mouse drivers screen limits. Then once we know where we
   are we simulate movement for 0,0.

   In practice this isn't quite perfect but it does work reasonably well.

   The tricky part then is to get the application and dosemu to agree on
   the algorithm. The algorithm we aim at is one mickey one pixel. Dosemu
   does this by default under X but you can force it with.
       emumouse x 8 y 8 a

   To do this in various various applications generally falls under the
   category of disable all mouse accelration.

   for win31 you need
      MouseThreshold1=0
      MouseThreshold2=0
      MouseSpeed=0

   in the '[windows]' section of win.ini

   The fool proof solution is to take total control of the mouse in X.
   This is controlled by the $_X_mgrab_key in /etc/dosemu.conf
   $_X_mgrab_key contains an X keysym of a key that when pressed with
   both Ctrl & Alt held down will turn on the mouse grab, which restricts
   the X mouse to the dosemu window, and gives dosemu complete control
   over it. Ctrl-Alt-$_X_mgrab_key will then release the mouse grab
   returning things to normal.

   I like: $_X_mgrab_key="Scroll_Lock" (Ctrl-Alt-Scroll_Lock) but
   $_X_mgrab_key="a" is a good conservative choice. (Ctrl-Alt-A) You can
   use xev to see what keysyms a key generates.

   Currently the way the X mouse code and the mouse grab are structured
   the internal mouse driver cannot display the mouse when the mouse grab
   is active. In particular without the grab active to display the mouse
   cursor we just let X draw the mouse for us, (as normal). When the
   mouse grab is active we restrict the mouse to our current window, and
   continually reset it to the center of the current screeen (allowing us
   to get relative amounts of movement). A side effect of this is that
   the the position of the X cursor and the dos cursor _not_ the same. So
   we need a different strategy to display the dos cursor.

   The other way an application can get into trouble in X, and also on
   the console for that matter is if it is partially broken. In
   particular the mouse driver is allowed to return coordinates that have
   little to no connection with the actual screen resolution. So an
   application mouse ask the mouse driver it's maximums and then scale
   the coordinates it gets into screen positions. The broken applications
   don't ask what the maximum & minimum resolutions are and just assume
   that they know what is going on.

   To keep this problem from being too severe in mouse.c we have
   attempted to match the default resolutions used by other mouse
   drivers. However since this is up to the choice of an individual mouse
   driver there is doubtless code out there developed with different
   resolutions in mind.

   If you get stuck with such a broken application we have developed a
   work around, that is partially effective. The idea being that if the
   application draws it's own mouse pointer it really doesn't matter
   where the dos mouse driver thinks the mouse is things should work. So
   with emumouse it is possible to set a minimum resolution to return to
   an application. By setting this minimum resolution to as big or bigger
   than the application expect to see it should work. The side effect of
   setting a minimum resolution bigger than the application expects to
   see in X is that there will be some edges to the of the screen where
   the application draws the cursor at the edge of the window, and yet
   you need to continue scrolling a ways before the cursor comes out
   there. In general this will affect the right and bottom edges of the
   screen.

   To read the current minimum use:
       emumouse i

   The default is 640x200

   To set the minimum resolution use:
       emumouse Mx 640 My 200

   If you application doesn't draw it's own mouse cursor a skew of this
   kind can be nasty. And there is no real good fix. You can set the
   mininum down as well as up and so it may be possible to match what the
   app expects as an internal resolution. However there is only so much
   we can do to get a borken application to run and that appears to be
   the limit.
     _________________________________________________________________

11. Mouse Garrot

   This section, and Mouse Garrot were written by Ed Sirett
   <ed@cityscape.co.uk> on 30 Jan 1995.

   Mouse Garrot is an extension to the Linux Dos Emulator that tries to
   help the emulator give up CPU time to the rest of the Linux system.

   It tries to detect the fact the the mouse is not doing anything and
   thus to release up CPU time, as is often the case when a mouse-aware
   program is standing idle, such programs are often executing a loop
   which is continually checking the mouse buttons & position.

   Mouse Garrot is built directly into dosemu if and only if you are
   using the internal mouse driver of dosemu. All you have to do is make
   sure that the HOGTHRESHOLD value in the config file is non-zero.

   If you are loading a (DOS based) mouse driver when you boot then you
   will also need to load MGARROT.COM It is essential that you load
   MGARROT.COM after you load the mouse driver. MGARROT will tell you if
   you are (a) running real DOS not dosemu, or (b) have requested a zero
   value for the HOGTHRESHOLD in the config file. In either case MGARROT
   fails to install and no memory will be lost. When MGARROT is installed
   it costs about 0.4k. MGARROT may loaded `high' if UMBs are available.

   Mouse Garrot will work with many (maybe most) mouse-aware programs
   including Works, Dosshell and the Norton Commander. Unfortunately,
   there are some programs which get the mouse driver to call the program
   whenever the mouse moves or a button is pressed. Programs that work in
   this way are immune to the efforts of Mouse Garrot.
     _________________________________________________________________

12. Running a DOS-application directly from Unix shell

   This part of the document was written by Hans <lermen@fgan.de>.
     _________________________________________________________________

12.1. Using the keystroke and commandline options.

   Make use of the keystroke configure option and the -I commandline
   option of DOSEMU (>=dosemu-0.66.2) such as

       dosemu.bin -D-a -I 'keystroke "dir > C:\\garbage\rexitemu\r"'

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
           This is useful, when the DOS-application flushes the
           keyboard buffer on startup. Your strokes would be discarded,
           if you don't wait.

   When using X, the keystroke feature can be used to directly fire up a
   DOS application with one click, if you have the right entry in your
   .fvwmrc
     _________________________________________________________________

12.2. Using an input file

     * Make a file "FILE" containing all keystrokes you need to boot
       dosemu and to start your dos-application, ... and don't forget to
       have CRLF for 'ENTER'. FILE may look like this (as on my machine):

         2^M                    <== this chooses point 2 of the boot menu
         dir > C:\garbage^M     <== this executes 'dir', result to 'garbage'
         exitemu^M              <== this terminates dosemu

       (the ^M stands for CR)
     * execute dosemu on a spare (not used) console, maybe /dev/tty20
       such like this:

       # dosemu.bin -D-a 2>/dev/null <FILE >/dev/tty20

       This will _not_ switch to /dev/tty20, but silently execute dosemu
       and you will get the '#' prompt back, when dosemu returns.

   I tested this with dosemu-0.64.4/Linux-2.0.28 and it works fine.

   When your dos-app does only normal printout (text), then you may even
   do this

          # dosemu.bin -D-a 2>/dev/null <FILE >FILE.out

   FILE.out then contains the output from the dos-app, but merged with
   ESC-sequences from Slang.

   You may elaborate this technique by writing a script, which gets the
   dos-command to execute from the commandline and generate 'FILE' for
   you.
     _________________________________________________________________

12.3. Running DOSEMU within a cron job

   When you try to use one of the above to start dosemu out of a crontab,
   then you have to asure, that the process has a proper environement set
   up ( especially the TERM and/or TERMCAP variable ).

   Normally cron would setup TERM=dumb, this is fine because DOSEMU
   recognizes it and internally sets it's own TERMCAP entry and TERM to
   `dosemu-none'. You may also configure your video to
          # dosemu.bin ... -I 'video { none }'

   or have a TERM=none to force the same setting. If you use the wrapper
   script, there is save way to do the same:
          # dosemu -dumb

   In all other crontab run cases you may get nasty error messages either
   from DOSEMU or from Slang.
     _________________________________________________________________

13. Commands & Utilities

   These are some utitlies to assist you in using Dosemu.
     _________________________________________________________________

13.1. Programs

   bootoff.com
          switch off the bootfile to access disk see examples/config.dist
          at bootdisk option

   booton.com
          switch on the bootfile to access bootfile see
          examples/config.dist at bootdisk option

   uchdir.com
          change the Unix directory for Dosemu (use chdir(2))

   dosdbg.com
          change the debug setting from within Dosemu

          + dosdbg -- show current state
          + dosdbg <string>
          + dosdbg help -- show usage information

   eject.com
          eject CD-ROM from drive

   emumouse.com
          fine tune internal mousedriver of Dosemu

          + emumouse h -- display help screen

   exitemu.com
          terminate Dosemu

   ugetcwd.com
          get the Unix directory for Dosemu (use getcwd(2))

   isemu.com
          detects Dosemu version and returns greater 0 if running under
          Dosemu

   lancheck.exe
          ???

   lredir.com
          redirect Linux directory to Dosemu

          + lredir -- show current redirections
          + lredir D: LINUX\FS\tmp -- redirects /tmp to drive D:
          + lredir help -- display help screen

   mgarrot.com
          give up cpu time when Dosemu is idle

   speed.com
          set cpu usage (HogThreshold) from inside Dosemu

   system.com
          interface to system(2)...

   unix.com
          execute Unix commands from Dosemu

          + unix -- display help screen
          + unix ls -al -- list current Linux directory
          + caution! try "unix" and read the help screen

   cmdline.exe
          Read /proc/self/cmdline and put strings such as "var=xxx" found
          on the commandline into the DOS enviroment. Example having this
          as the dosemu commandine:

                     dosemu.bin "autoexec=echo This is a test..."

          then doing


                     C:\cmdline < D:\proc\self\cmdline
                     %autoexec%

          would display "This is a test..." on the DOS-Terminal

   vgaoff.com
          disable vga option

   vgaon.com
          enable vga option

   xmode.exe
          set special X parameter when running as xdos
     _________________________________________________________________

13.2. Drivers

   These are useful drivers for Dosemu

   cdrom.sys
          allow direct access to CD-ROM drives from Dosemu

   ems.sys
          enable EMM in Dosemu

   emufs.sys
          redirect Unix directory to Dosemu

   aspi.sys
          ASPI conform SCSI driver

   fossil.com
          FOSSIL serial driver (TSR)
     _________________________________________________________________

14. Keymaps

   This keymap is for using dosemu over telnet, and having *all* your
   keys work. This keymap is not complete. But hopefully with everyones
   help it will be someday :)

   There are a couple of things that are intentionally broken with this
   keymap, most noteably F11 and F12. This is because they are not
   working though slang correctly at the current instant. I have them
   mapped to "greyplus" and "greyminus". Also the scroll lock is mapped
   to shift-f3. This is because the scroll lock dosn't work right at all.
   Please feel free to send keymap patches in that fix anything but
   these.

   If you want to patch dosemu to fix either of those problems, i'd be
   glad to accept those :)

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

   when your done, find your old keymap, and load it back, cause
   control-home won't work in emacs anymore (or any other special key in
   any applicaion that uses xlate)

   if you find a key missing, please add it and send me the patch. (test
   it first! :)

   if you find a key missing, and don't feel like coding it, don't tell
   me! I already know that there are a lot of keys missing.

   corey sweeney <corey@interaccess.com >

   Sytron Computers
     _________________________________________________________________

15. Networking using DOSEMU

   A mini-HOWTO from Bart Hartgers <barth@stack.nl> ( for the detailed
   original description see below ) updated by Bart Oldeman, Jan 2004.
     _________________________________________________________________

15.1. Virtual networking.

   A special virtual network device can be created using TUN/TAP or the
   obsolete dosnet module. In combination with pktdrv.c and libpacket.c,
   this will enable multiple dosemu sessions and the linux kernel to be
   on a virtual network. Each has its own network device and ethernet
   address.

   This means that you can ping/ssh/telnet/ftp from the dos-session to
   your telnetd/ftpd/... running in linux and, with IP forwarding enabled
   in the kernel, or by bridging, connect to any host on your network.

   If you, on the other hand, just want one DOSEMU session which talks to
   your ethernet card through the packet driver, then you don't need to
   use $_vnet and can set $_netdev to "eth0" or similar. Note that this
   direct "eth0" access requires root privileges because the DOSEMU code
   needs to manipulate raw sockets.
     _________________________________________________________________

15.2. TUN/TAP support.

   Using TUN/TAP it is possible to have a networked DOSEMU without
   needing root privileges, although some setup (as root, depending on
   the distribution) is still needed.

   First make sure that your Linux kernel comes with support for TUN/TAP;
   for details check Documentation/networking/tuntap.txt in the Linux
   kernel source. The user who runs DOSEMU should have read/write access
   to /dev/net/tun. Then either:

    1. set $_pktdriver=(on), $_vnet = "tap" and $_netdev = "". Start
       DOSEMU as usual and configure the network device while DOSEMU is
       running (using ifconfig manually as root, a script, or usernetctl
       if your distribution supplies that), e.g. ifconfig tap0
       192.168.74.1. Configure the DOS TCP/IP network clients to have
       another IP address in the subnet you just configured. This address
       should be unique, i.e. no other dosemu, or the kernel, should have
       this address. For the example addresses given above,
       192.168.74.2-192.168.74.254 would be good. Your network should now
       be up and running and you can, for example, use a DOS SSH client
       to ssh to your own machine, but it will be down as soon as you
       exit DOSEMU.
    2. or set $_pktdriver=(on), $_vnet = "tap" and $_netdev = "tap0".
       Obtain tunctl from the user mode linux project. Then set up a
       persistent TAP device using tunctl (use the -u owner option if you
       do that as root). Configure the network using ifconfig as above,
       but now before starting DOSEMU. Now start DOSEMU as often as you
       like and you can use the network in the same way as you did above.

   Note, however, that multiple DOSEMU sessions that run at the same time
   need to use multiple tapxx devices. $_netdev can be changed without
   editing dosemu.conf/~./dosemurc (if you leave it commented out there)
   by setting the dosemu__netdev environment variable or by using
   something like [x]dosemu -I "netdev tap1".

   With the above you did set up a purely virtual internal network
   between the DOSEMU virtual PC and the real Linux box. This is why, in
   the above example, 192.168.74.1 should *not* be a real IP address of
   the Linux box, and the 192.168.74 network should not exist as a real
   network. To enable DOS programs to talk to the outside world you have
   to set up IP Forwarding or bridging.

   Bridging, using brctl (look for the bridge-utils package if you don't
   have it), is somewhat easier to accomplish than IP forwarding. You set
   up a bridge, for example named "br0" and connect eth0 and tap0 to it.
   Suppose the Linux box has IP 192.168.1.10, where 192.168.1.x can be a
   real LAN, and the uid of the user who is going to use DOSEMU is 500,
   then you can do (as root):
         host# brctl addbr br0
         host# ifconfig eth0 0.0.0.0 promisc up
         host# brctl addif br0 eth0
         host# ifconfig br0 192.168.1.10 netmask 255.255.255.0 up
         host# tunctl -u 500
         host# ifconfig tap0 0.0.0.0 promisc up
         host# brctl addif br0 tap0

   Now the DOSEMU's IP can be (for example) 192.168.1.11. If you still
   like to use IP forwarding instead (where the DOSEMU box' IP appears to
   the outside world as the Linux box' IP), then check out the IP
   forwarding HOWTO.
     _________________________________________________________________

15.3. The DOSNET virtual device (deprecated).

   Dosnet.o is a kernel module that implements the special virtual
   network device (predecessor of tun/tap support). To set it up, go to
   ./src/dosext/net/v-net and make dosnet.o. As root, insmod dosnet.o.
   Now as root, configure the dsn0 interface (for example: ifconfig dsn0
   192.168.74.1 netmask 255.255.255.0), and add a route for it (for
   example: route add -net 192.168.74.0 netmask 255.255.255.0 dsn0).
     _________________________________________________________________

15.4. Full Details

   Modified description of Vinod G Kulkarni <vinod@cse.iitb.ernet.in>

   Allowing a program to have its own network protocol stacks.

   Resulting in multiple dosemu's to use netware, ncsa telnet etc.
     _________________________________________________________________

15.4.1. Introduction

   Allowing network access from dosemu is an important functionality. For
   pc based network product developers, it will offer an easy development
   environment will full control over all the traffic without having to
   run around and use several machines. It will allow already available
   client-server based "front-ends" to run on dosemulator. (Assuming that
   they are all packet driver based -- as of now ;-) )

   To accomplish that, we require independent protocol stacks to coexist
   along with linux' IP stack. One way is to add independent network
   card. However, it is cumbersome and allows at most only 2-3 stacks.
   Other option is to use the a virtual network device that will route
   the packets to the actual stacks which run as user programs.
     _________________________________________________________________

15.4.2. Design

   Have a virtual device which provides routing interface at one end (so
   it is a network device from linux side) and at other end, it
   sends/receives packets from/to user stacks.

   All the user stacks AND virtual device are virtually connected by a
   network (equavalent to a physical cable). Any broadcast packet (sent
   by either user stack or router interface of the virtual device) should
   be sent to all the user stacks and router. All non-broadcast packets
   can be sent by communicating with each other.

   Each user stack (here dosemu process) will have an base interface
   which allows sending and receiving of packets. On the top of this, a
   proper interface (such as packet driver interface) can be built. In
   dosemu, a packet driver interface is emulated.

   Every user stack will have a unique virtual ethernet address.
     _________________________________________________________________

15.4.3. Implementation

   This package includes:

    1. dosnet module. Acts as virtual network device introducing 'dsn0'
       interface. It provides usual network interface AND also facility
       to communicate with dosemu's.
    2. Modified packet driver code (pktnew.c and libdosemu.c) to enable
       the above. Modifications include these:
         a. Generate an unique ethernet address for each dosemu . I have
            used minor no. of the tty in use as part of ethernet address.
            This works unless you start two dosemu's in same tty.
         b. Communication with dosnet device is done by opening a
            SOCK_PACKET socket of special type.
    3. IPX bridge code. Between eth0 and dsn0 so that multiple lan
       accesses can be made. 0.1 is non-intelligent. (both versions are
       alpha codes.) Actually IPX routing code is there in kernel. Has
       anyone been successful in using this? Yet another alternative is
       to use IPTunnelling of IPX packets (rfc 1234). Novell has NLMs for
       this on the netware side. On linux, we should run a daemon
       implementing this rfc.
     _________________________________________________________________

15.4.4. Virtual device 'dsn0'

   Compile the module dosnet and insmod it, and give it an IP address,
   with a new IP network number. And You have to set up proper routing
   tables on all machines you want to connect to. So linux side interface
   is easy to set up.

   This device is assigned a virtual ethernet address, defined in
   dosnet.h.

   This device is usual loadable module. (Someone please check if it can
   be made more efficient.) However, what is interesting is the way it
   allows access to user stacks (i.e. dosemu's.) i.e. its media
   interface.

   A packet arrives to dosnet from linux for our virtual internal network
   (after routing process). If it is broadcast packet, dosnet should send
   it to all dosemu's/user stacks. If it is normal packet, it should send
   it only particular user stack which has same destination ethernet
   address .

   It performs this process by the following method, using SOCK_PACKET
   interface , (and not introducing new devices).:

   The dosemu opens a SOCK_PACKET interface for type 'x' with the dosnet
   device. The result of this will be an addition of entry into type
   handler table for type 'x'. This table stores the type and
   corresponding handler function (called when a packet of this type
   arrives.)

   Each dosemu will open the interface with unique 'x' .

   sending packets from dosemu to dosnet
          SOCK_PACKET allows you to send the packet "as is". So not a
          problem at all.

   dosnet -> dosemu
          this is tricky. The packet is simply given by dosnet device to
          upper layers. However, the upper layer calls function to find
          out the type of the packet which is device specific (default is
          eth_type_trans().). This routine, which returns type of given
          packet, is to be implemented in each device. So in dosnet, this
          plays a special role. If the packet is identified as type 'x',
          the upper layers (net/inet/dev.c) call the type handler for
          'x'.

          Looking at destination ethernet address of a packet, we can say
          deduct that it is packet for dosemu, and its type is 'x' (i.e.
          'x' is "inserted" in dosemu's virtual ethernet address.) Type
          handler function for 'x' is essentially SOCK_PACKET receiver
          function which sends packet back to dosemu.

          NOTE: the "type" field is in destination ethernet address and
          not its usual place (which depends on frame type.) So the
          packet is left intact -- there is no wrapper function etc. We
          should use type "x" which is unused by others; so the packet
          can carry _ANY_ protocol since the data field is left
          untouched.

   Broadcast packets
          We use a common type "y" for dosnet broadcasts. Each dosemu
          "registers" for "y" along with usual "x" type packet using
          SOCK_PACKET. This "y" is same for all dosemu's. (The packet is
          duplicated if more than one SOCK_PACKET asks for same type. )
     _________________________________________________________________

15.4.5. Packet driver code

   I have add the code for handling multiple protocols.

   When a packet arrives, it arrives on one of the two SOCK_PACKET handle
   we need to find out which of the registered protocols should be
   handled this code. (Earlier code opened multiple sockets, one for each
   IPX type. However it is not useful now because we use *any* type.)
   When a new type is registered, it is added to a Type list. When a new
   packet arrives, first we find out the frame type(and hence the
   position of type field in the packet, and then try matching it with
   registered types. [ ---- I missed comparing class; I will add it
   later.] Then call the helper corresponding to the handle of that type.

   Rob, you should help in the following:

    1. Packet driver code ...
       We should now open only two sockets: one specific to dosemu and
       other broadcast. So we have to add code to demultiplex into packet
       types... I couldn't succeed. Even broadcast packets are not
       getting to dosemu.
    2. Which virtual ethernet addresses to use (officially)?
    3. Which special packet type can be used?
    4. Kernel overhead .. lots of packet types getting introduced in type
       handler table... how to reduce?
     _________________________________________________________________

15.4.6. Conclusion

   So at last one can open multiple DOSEMU's and access network from each
   of them ... However, you HAVE TO set up ROUTING TABLES etc.

   Vinod G Kulkarni <vinod@cse.iitb.ernet.in>
     _________________________________________________________________

15.4.6.1. Telnetting to other Systems

   Other systems need to have route to this "new" network. The easiest
   way to do this is to have static route for dosnet IP network included
   in remote machine you want to connect to. After all tests are carried
   out, one could include them permanently (i.e. in gated configurations
   etc.). However, the "new" IP address should only be internal to your
   organisation, and not allowed to route outside. There is some rfc in
   this regard, I will let you know later. For e.g., I am working on
   144.16.98.20. Internal network I created was 144.16.112.0. (See the
   above route command.) To connect to another linux system 144.16.98.26
   from dosemu, I include static route by running 'route add -net
   144.16.112.0 gw 144.16.98.20' on that system. It becomes more complex
   if you need to connect to outside of 144.16.98.0.
     _________________________________________________________________

15.4.6.2. Accessing Novell netware

   Since dosemu is now on "different device", IPX needs to be either
   bridged or routed. If it is bridged, then there is no requirement for
   any extra administration ; simply run 'ipxbridge' program supplied
   with the dosnet sources. (There are two versions of it; 0.1 copies all
   packets to from/to both interface. 0.2 is "intelligent bridge", it
   copies packet to other interface only if the destination lies on other
   interface. )

   If you instead want to use "routing" for IPX, then you need to enable
   IPX config option in the kernel. Next, you should select a network
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
     _________________________________________________________________

16. Using Windows and Winsock

   This is the Windows Net Howto by Frisoni Gloriano <gfrisoni@hi-net.it>
   on 15 may 1997

   This document tries to describe how to run the Windows trumpet winsock
   over the dosemu built-in packet driver, and then run all TCP/IP
   winsock-based application (netscape, eudora, mirc, free agent .....)
   in windows environment.

   This is a very long step-by-step list of operation, but you can make
   little scripts to do all very quickly ;-)

   In this example, I use the dosnet based packet driver. It is very
   powerful because you can run a "Virtual net" between your dos-windows
   session and the linux, and run tcp/application application without a
   real (hardware) net.
     _________________________________________________________________

16.1. LIST OF REQUIRED SOFTWARE

     * The WINPKT.COM virtual packet driver, version 11.2 I have found
       this little tsr in the Crynwr packet driver distribution file
       PKTD11.ZIP
     * The Trumpet Winsock 2.0 revision B winsock driver for windows.
     _________________________________________________________________

16.2. STEP BY STEP OPERATION (LINUX SIDE)

     * Enable "dosnet" based dosemu packet driver:

          cd ./src/dosext/net/net
          select_packet  (Ask  single or multi ->  m)

     * Make the dosnet linux module:

          cd ./src/dosext/net/v-net
          make

     * Make the new dosemu, with right packet driver support built-in:

          make
          make install

     * Now you must load the dosnet module:

          insmod ./src/dosext/net/v-net/dosnet.o

     * Some linux-side network setup (activate device, routing). This
       stuff depends from your environment, so I write an example setup.
       Here you configure a network interface dsn0 (the dosnet interface)
       with the ip address 144.16.112.1 and add a route to this
       interface.
       This is a good example to make a "virtul network" from your
       dos/windows environment and the linux environment.

          ifconfig dsn0 144.16.112.1 broadcast 144.16.112.255 netmask 255.255.2
55.0
          route add -net 144.16.112.0 dsn0
     _________________________________________________________________

16.3. STEP BY STEP OPERATION (DOS SIDE)

   I suppose you know how to run windows in dosemu. You can read the
   Section 9 document if you need more information. Windows is not very
   stable, but works.

     * start dosemu.
     * copy the winpkt.com driver and the trumpet winsock driver in some
       dos directory.
     * start the winpkt TSR. (dosemu assign the 0x60 interrupt vector to
       the built-in packet driver)

        winpkt 0x60

     * edit the trumpet winsock setup file trumpwsk.ini. Here is an
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

     * Now you can run windows, startup trumpet winsock and ..... enjoy
       with your windoze tcp/ip :-)

   Gloriano Frisoni. <gfrisoni@hi-net.it>
