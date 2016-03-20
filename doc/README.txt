DOSEMU

The DOSEMU team

Edited by

Alistair MacDonald

   For DOSEMU v1.4 pl0.0

   This document is the amalgamation of a series of README files which
   were created to deal with the lack of DOSEMU documentation.
     __________________________________________________________________

   Table of Contents
   1. Introduction

        1.1. DOSEMU modes of operation

              1.1.1. Terminal mode
              1.1.2. Dumb mode
              1.1.3. SDL mode
              1.1.4. Console graphics mode

        1.2. Running a DOS program directly from Linux.
        1.3. Using a different DOS
        1.4. About this document

   2. Runtime Configuration Options

        2.1. Format of dosemu.conf and ~/.dosemurc

              2.1.1. Disks, boot directories and floppies
              2.1.2. Controlling amount of debug output
              2.1.3. Basic emulation settings
              2.1.4. Code page and character set
              2.1.5. Terminals
              2.1.6. Keyboard settings
              2.1.7. X Support settings
              2.1.8. Builtin ASPI SCSI Driver
              2.1.9. COM ports and mice
              2.1.10. Printers
              2.1.11. Sound
              2.1.12. Joystick
              2.1.13. Networking under DOSEMU
              2.1.14. Settings for enabling direct hardware access
              2.1.15. Video settings ( console only )
              2.1.16. Time settings

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

        7.1. The built-in driver

   8. Using X

        8.1. Basic information
        8.2. More detailed information, hints and tips
        8.3. The VGA Emulator

   9. Running Windows under DOSEMU

        9.1. Mouse in Windows under DOSEMU
        9.2. Windows 3.x in SVGA modes
        9.3. VxD support
        9.4. DOS shell in Windows

   10. The DOSEMU mouse

        10.1. Setting up the emulated mouse in DOSEMU
        10.2. Problems

   11. Mouse Garrot
   12. Running a DOS application directly from Unix shell

        12.1. Using unix -e in autoexec.bat
        12.2. Using the keystroke facility.
        12.3. Using an input file
        12.4. Running DOSEMU within a cron job

   13. Commands & Utilities

        13.1. Programs
        13.2. Drivers

   14. Keymaps
   15. Networking using DOSEMU

        15.1. Direct NIC access
        15.2. Virtual networking

              15.2.1. Bridging
              15.2.2. IP Routing

        15.3. VDE networking backend

   16. Using Windows and Winsock

        16.1. LIST OF REQUIRED SOFTWARE
        16.2. STEP BY STEP OPERATION (LINUX SIDE)
        16.3. STEP BY STEP OPERATION (DOS SIDE)

1. Introduction

   You can start DOSEMU using
        $ dosemu

   If you have never used DOSEMU before, and FreeDOS is present, then
   DOSEMU will boot, and present you with a welcome screen and a C:\>
   command prompt.

   If for some reason it does not start, or DOSEMU crashes somewhere, look
   at ~/.dosemu/boot.log for details.

   Remember, that you can't use <Ctrl>-C within DOS to exit from DOS. For
   this you need to execute exitemu or, when using the 'DOS in a BOX'
   <Ctrl><Alt><PgDn>.

   Your DOS drives are set up as follows:
     A: floppy drive (if it exists)
     C: points to the Linux directory ~/.dosemu/drive_c. It contains the
        files config.sys, autoexec.bat and a directory for temporary files.
        It is available for general DOS use.
     D: points to your Linux home directory
     E: points to your CD-ROM drive, if it is mounted at /media/cdrom
     Z: points to the read-only DOSEMU and FreeDOS commands directory
        It actually points to ~/mydos/dosemu/drive_z; it appears read-only
        inside DOSEMU.

   You can use the LREDIR DOSEMU command to adjust these settings, or edit
   /etc/dosemu/dosemu.conf, ~/.dosemurc, c:\config.sys, or
   c:\autoexec.bat, or change the symbolic links in ~/.dosemu/drives.

   Enter HELP for more information on DOS and DOSEMU commands. Note that
   FreeDOS COMMAND.COM DIR command shows long file names if you type
   DIR/LFN.

   Other useful keys are:
    <Ctrl><Alt><F>    toggle full-screen mode in X
    <Ctrl><Alt><K>    grab the keyboard in X
    <Ctrl><Alt><Home> grab the mouse in X
    <Ctrl><Alt><Del>  reboot
    <Ctrl><^>         use special keys on terminals (dosemu -t)
     __________________________________________________________________

1.1. DOSEMU modes of operation

   There exist various ways of starting DOSEMU, depending on the
   environment and certain command line options. By default, in X, it will
   start using a special 'DOS in a Box' which provides a usual PC setup,
   using a 80x25 text mode. It also supports graphics. The box can be
   rescaled by dragging the window borders using the mouse.

   However, in certain situation you may want to use a different mode.
     __________________________________________________________________

1.1.1. Terminal mode

   Terminal mode is automatically entered if you do not have X available,
   for instance when logging in remotely from a Windows system or at the
   Linux console. You can force it using:
         $ dosemu -t

   In this mode the display of graphics is impossible, but you can use
   full-screen DOS text mode applications. It is advisable to give the
   terminal window a size of 80 by 25 characters, or use "stty cols 80
   rows 25" on the Linux console, before starting it because many DOS
   applications are confused about other sizes.

   You can use the $_internal_char_set option in ~/.dosemurc or
   dosemu.conf to change the code page that DOSEMU thinks that DOS is
   using.

   Many terminals do not support various function key combinations. On the
   Linux console you can work around that by using the raw keyboard mode
   (-k flag, or $_rawkeyboard). xterm's support many key combinations. In
   other cases you'll have to work around it using the special Ctrl-^
   shortcut (Ctrl-6 on US keyboards). Press Ctrl-^ h for help.
     __________________________________________________________________

1.1.2. Dumb mode

   For DOS applications that only read from standard input and write to
   standard output, without any full-screen usage, you can use dumb mode.
   To use this you must invoke DOSEMU like
         $ dosemu -dumb

   this has the advantage that (A) the output of the DOS application
   stacks up in your scroll buffer and (B) you can redirect it to a file
   such as
         $ dosemu -dumb dir > listing

   Note that editing is often restricted to BACKSPACE'ing.
     __________________________________________________________________

1.1.3. SDL mode

   You can start dosemu with the "-S" option to use the SDL library. In X
   it will just look like a regular DOS in a Box but with a different
   shaped text mode mouse cursor. You can also use this mode on frame
   buffer consoles.
     __________________________________________________________________

1.1.4. Console graphics mode

   Console graphics mode is the hardest to setup and may potentially lock
   up your system, but if it works it gives you direct VGA hardware access
   which may be quicker and more accurate than the emulation used in X.

   You need root rights to use it. To enable it, it is recommended to use
   "sudo":

     * install sudo if you haven't already done so
     * use visudo as root to add entries such as

        joeuser   hostname=(root) PASSWD: /usr/local/bin/dosemu.bin

       to your /etc/sudoers file, where "joeuser" is the user who is
       allowed to run privileged DOSEMU and "hostname" is the name of your
       current host (use "ALL" for any host).
     * if you change PASSWD to NOPASSWD then joeuser does not need to type
       the user's password (not root's password) when invoking DOSEMU (a
       little less secure, if somebody hacks into joeuser's account).
     * now invoke DOSEMU using dosemu -s
     __________________________________________________________________

1.2. Running a DOS program directly from Linux.

   You can use something like
              dosemu "/home/clarence/games/commander keen/keen1.exe"

   which will automatically cause the DOS in DOSEMU to

     * "cd" to the correct directory,
     * execute the program automagically,
     * and quit DOSEMU when finished.
     __________________________________________________________________

1.3. Using a different DOS

   It is possible to use a different DOS than the supplied FreeDOS in
   DOSEMU. A straightforward way is to just copy the relevant system files
   (io.sys, msdos.sys, etc.) to ~/.dosemu/drive_c, and then the next time
   you run dosemu it will automatically use them. You may need to edit
   config.sys and autoexec.bat though, if the DOS complains.

   Another way is to boot directly from a Linux mounted FAT partition,
   with Windows 9x or any DOS installed. You can change the C: drive to
   point to that by using dosemu -i.

   In that case the DOSEMU support commands are available on drive D:
   instead of drive Z:. You might want to use different config.sys and
   autoexec.bat files with your DOS. For example, you can try to copy
   D:\config.emu and D:\autoemu.bat to C:\, adjust them, and use the
   $_emusys option in ~/.dosemurc or dosemu.conf.

   Manual adjustment of the C: drive is also possible, by changing the
   ~/.dosemu/drives/c symbolic link or by specifying it explicitly using
   the $_hdimage run-time option.
     __________________________________________________________________

1.4. About this document

   The rest of this document goes into more detail about all the different
   settings and possibilities. This documentation is derived from a number
   of smaller documents. This makes it easier for individuals to maintain
   the documentation relevant to their area of expertise. Previous
   attempts at documenting DOSEMU failed because the documentation on a
   large project like this quickly becomes too much for one person to
   handle.
     __________________________________________________________________

2. Runtime Configuration Options

   This section of the document by Hans, <lermen@fgan.de>. Last updated on
   May 4, 2007, by Bart Oldeman.

   Most of DOSEMU configuration is done during runtime and per default it
   can use the system wide configuration file dosemu.conf (which is often
   situated in /etc or /etc/dosemu) optionally followed by the users
   ~/.dosemurc and additional configurations statements on the commandline
   (-I option). If /etc/dosemu.users exists then dosemu.conf is searched
   for in /etc and otherwise in /etc/dosemu (or an alternative sysconfdir
   compiletime-setting).

   The default dosemu.conf and ~/.dosemurc have all settings commented out
   for documentation purposes; the commented out values are the ones that
   DOSEMU uses by default. Note that a non-suid type of installation does
   not need the dosemu.users and dosemu.conf files, and the main per-user
   configuration file is $HOME/.dosemurc. However, for security reasons, a
   suid-root installation will not run without dosemu.users, and in that
   case certain dosemu.conf settings are ignored by ~/.dosemurc.

   In fact dosemu.conf and ~/.dosemurc (which have identical syntax) are
   included by the systemwide configuration script global.conf which, by
   default, is built into the DOSEMU binary. As a normal user you won't
   ever think on editing this, only dosemu.conf and your personal
   ~/.dosemurc. The syntax of global.conf is described in detail in
   README-tech.txt, so this is skipped here. However, the option -I string
   too uses the same syntax as global.conf, hence, if you are doing some
   special stuff (after you got familar with DOSEMU) you may need to have
   a look there.

   The first file expected (and interpreted) before any other
   configuration (such as global.conf, dosemu.conf and ~/.dosemurc) is
   /etc/dosemu.users or /etc/dosemu/dosemu.users. As mentioned above, this
   file is entirely optional for non-suid-root (default) installations.
   Within this file the general permissions are set:

     * which users are allowed to use DOSEMU.
     * which users are allowed to use DOSEMU suid root.
     * which users are allowed to have a private lib dir.
     * what kind of access class the user belongs to.
     * what special configuration stuff the users needs

   and further more:

     * whether the lib dir (DOSEMU_LIB_DIR) resides elsewhere.
     * setting the loglevel.

   Except for lines starting with `xxx=' (explanation below), each line in
   dosemu.user corresponds to exactly one valid user count, the special
   user `all' means any user not mentioned earlier. Format:
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
   disable all secure relevant feature. Setting `guest' will force setting
   `restricted' too.

   The use of `nosuidroot' will force a suid root dosemu binary to exit,
   the user may however use a non-suid root copy of the binary. For more
   information on this look at README-tech, chapter 11.1 `Privileges and
   Running as User'

   Giving the keyword `private_setup' to a user means he/she can have a
   private DOSEMU lib under $HOME/.dosemu/lib. If this directory is
   existing, DOSEMU will expect all normally under DOSEMU_LIB_DIR within
   that directory. As this would be a security risk, it only will be
   allowed, if the used DOSEMU binary is non-suid-root. If you really
   trust a user you may additionally give the keyword `unrestricted',
   which will allow this user to execute a suid-root binary even on a
   private lib directory (though, be aware).

   In addition, dosemu.users can be used to define some global settings,
   which must be known before any other file is accessed, such as:
      default_lib_dir= /opt/dosemu  # replaces DOSEMU_LIB_DIR
      log_level= 2                  # highest log level

   With `default_lib_dir=' you may move DOSEMU_LIB_DIR elsewhere, this
   mostly is interesting for distributors, who want it elsewhere but won't
   patch the DOSEMU source just for this purpose. But note, the dosemu
   supplied scripts and helpers may need some adaption too in order to fit
   your new directory.

   The `log_level=' can be 0 (never log) or 1 (log only errors) or 2 (log
   all) and controls the ammount written to the systems log facility
   (notice). This keyword replaces the former /etc/dosemu.loglevel file,
   which now is obsolete.

   Nevertheless, for a first try of DOSEMU you may prefer
   etc/dosemu.users.easy, which just contains
      root c_all
      all c_all

   to allow everybody all weird things. For more details on security
   issues have a look at chapter 3.

   After the file dosemu.users, the file dosemu.conf (via global.conf,
   which may be built-in) is interpreted, and only during global.conf
   parsing the access to all configuration options is allowed. dosemu.conf
   normally lives in the same directory as dosemu.users, for instance
   /etc/dosemu or /etc (that is, sysconfdir in compiletime-settings). Your
   personal ~/.dosemurc is included directly after dosemu.conf, but has
   less access rights (in fact the lowest level), all variables you define
   within ~/.dosemurc transparently are prefixed with `dosemu_' such that
   the normal namespace cannot be polluted (and a hacker cannot overwrite
   security relevant enviroment variables). Within global.conf only those
   ~/.dosemurc created variables, that are needed are taken over and may
   overwrite those defined in dosemu.conf.

   The dosemu.conf (global.conf) may check for the configuration
   variables, that are set in dosemu.users and optionaly include further
   configuration files. But once dosemu.conf (global.conf) has finished
   interpretation, the access to secure relevant configurations is
   (class-wise) restricted while the following interpretation of (old)
   .dosrc and -I statements.

   For more details on security settings/issues look at README-tech.txt,
   for now (using DOSEMU the first time) you should need only the below
   description of dosemu.conf (~/.dosemurc)
     __________________________________________________________________

2.1. Format of dosemu.conf and ~/.dosemurc

   All settings are variables, and have the form of

         $_xxx = (n)

   or
         $_zzz = "s"

   where `n' is a numerical or boolean value and `s' is a string. Note
   that the brackets are important, else the parser will not decide for a
   number expression. For numbers you may have complete expressions ( such
   as (2*1024) ) and strings may be concatenated such as

         $_zzz = "This is a string containing '", '"', "' (quotes)"

   Hence a comma separated list of strings is concatenated. All these
   settings are also environment variables. You can override them by
   prefixing with dosemu_, e.g.
       dosemu__X_title="DOS was in the BOX" dosemu

   temporarily changes the X window title.
     __________________________________________________________________

2.1.1. Disks, boot directories and floppies

   The parameter settings are tailored to fit the recommended usage of
   disk and floppy access. There are other methods too, but for these you
   have to look at README-tech.txt (and you may need to modify
   global.conf). We strongly recommend that you use the proposed techique.
   Here the normal setup:

    # List of hdimages or boot directories under
    # ~/.dosemu, the system config directory (/etc/dosemu by default), or
    # syshdimagedir (/var/lib/dosemu by default) assigned in this order
    # such as "hdimage_c directory_d hdimage_e"
    # Absolute pathnames are also allowed.
      $_hdimage = "drives/*"
      $_vbootfloppy = ""    # if you want to boot from a virtual floppy:
                            # file name of the floppy image under DOSEMU_LIB_DIR
                            # e.g. "floppyimage" disables $_hdimage
                            #      "floppyimage +hd" does _not_ disable $_hdimag
e
      $_floppy_a ="threeinch" # or "fiveinch" or empty, if not existing
      $_floppy_b = ""       # dito for B:
      $_cdrom = "/dev/cdrom" # list of CDROM devices

   A hdimage is a file containing a virtual image of a DOS-FAT filesystem.
   Once you have booted it, you (or autoexec.bat) can use `lredir' to
   access any directory in your Linux tree as DOS drive (a -t msdos
   mounted too). Look at chapter 5 (Using Lredir) for more details. If you
   want to create your own hdimage use "mkfatimage16" (see the manual
   page). To make it bootable you can make it, say, drive F:, and use "SYS
   F:" at the DOS prompt. The DOSEMU-HOWTO explains how to manipulate it
   using mtools.

   You can also specify a Linux directory containing all what you want to
   have under your DOS C:. Copy your IO.SYS, MSDOS.SYS or equivalent to
   that directory (e.g. DOSEMU_LIB_DIR/bootdir), set
          $_hdimage = "bootdir"

   and up it goes. Alternatively you can specify an absolute path such as
   "/dos" or "/home/username/dosemu/freedos". DOSEMU makes a lredir'ed
   drive out of it and can boot from it. You can edit the config.sys and
   the autoexec.bat within this directory before you start dosemu. Further
   more, you may have a more sohisticated setup. Given you want to run the
   same DOS drive as you normal have when booting into native DOS, then
   you just mount you DOS partition under Linux (say to /dos) and put
   links to its subdirectories into the boot dir. This way you can decide
   which files/directories have to be visible under DOSEMU and which have
   to be different. Here a small and not complete example bootdir setup:
      config.sys
      autoexec.bat
      command.com -> /dos/command.com
      io.sys -> /dos/io.sys
      msdos.sys -> /dos/msdos.sys
      dos -> /dos/dos
      bc -> /dos/bc
      windows -> /dos/windows

   As a further enhancement of your drives setup you may even use the
   following strategy: given you have the following directory structure in
   one the directories where the $_hdimage setting applies (this is done
   by default in ~/.dosemu and in /etc/dosemu)
      drives/c
      drives/d

   where c and d are symbolic links to appropriate DOS useable
   directories, then the following single statement
         $_hdimage = "drives/*"

   will assign all these directories to drive C: and D: respectively.
   Note, however, that the order in which the directories under drives/*
   are assigned comes from the order given by /bin/ls. Hence the folling
      drives/x
      drives/a

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
   drive'), For this to overcome you may need to use so-called `partition
   access', use a floppy (or a "vbootfloppy"), or a special-purpose
   hdimage. The odd with partition access is, that you never should have
   those partition mounted in the Linux file system at the same time as
   you use it in DOSEMU (which is quite uncomfortable and dangerous on a
   multitasking OS such as Linux ). Though DOSEMU checks for mounted
   partitions, there may be races that are not caught. In addition, when
   your DOSEMU crashes, it may leave some FAT sectors unflushed to the
   disk, hence destroying the partition. Anyway, if you think you need it,
   you must have r/w access to the partition in /dev, and you have to
   `assign' real DOS partitions as follows:

         $_hdimage = "hdimage.first /dev/hda1 /dev/sdc4:ro"

   The above will have `hdimage.first' as booted drive C:, /dev/hda1 as D:
   (read/write) and /dev/sdc4 as E: (readonly). You may have any kind of
   order within `$_hdimage', hence

         $_hdimage = "/dev/hda1 hdimage.first /dev/sdc4:ro"

   would have /dev/hda1 as booted drive C:. Note that the access to the
   /dev/* devices must be exclusive (no other process should use it)
   except for `:ro'.
     __________________________________________________________________

2.1.2. Controlling amount of debug output

   DOSEMU will help you find problems, when you enable its debug messages.
   These will go into the file, that you defined via the `-o file' or `-O'
   commandline option (the latter prints to stderr). If you do not specify
   any -O or -o switch, then the log output will be written to
   ~/.dosemu/boot.log. You can preset the kind of debug output via

         $_debug = "-a"

   where the string contains all you normally may pass to the `-D'
   commandline option (look at the man page for details).
     __________________________________________________________________

2.1.3. Basic emulation settings

   Whether a numeric processor should be shown to the DOS space
         $_mathco = (on)

   Which type of CPU should be emulated (NOTE: this is not the one you are
   running on, but your setting may not exeed the capabilities of the
   running CPU). Valid values are: 80[345]86
         $_cpu = (80386)

   To let DOSEMU use the Pentium cycle counter (if availabe) to do better
   timing use the below

         $_rdtsc = (off)   # or on

   Note that the RDTSC can be unreliable on SMP systems, and in
   combination with power management (APM/ACPI).

   For the above `rdtsc' feature DOSEMU needs to know the exact CPU clock,
   it normally calibrates it itself, but is you encounter a wrong
   mesurement you may overide it such as
         $_cpuspeed = (166.666)  # 0 = let DOSEMU calibrate

   If you have a PCI board you may allow DOSEMU to access the PCI
   configuration space by defining the below
         $_pci = (auto)    # or auto, or off

   NOTE: `$_pci' can not be overwritten by ~/.dosemurc. The "on" setting
   can be very dangerous because it gives DOSEMU complete write access;
   you need to edit dosemu.users to enable it. In console graphics mode,
   some video card BIOSes need some PCI configuration space access, which
   is enabled by the default (auto) setting. This setting is far more
   restricted and less dangerous.

   Starting with dosemu-1.0 there is a flexible way to handle the mapping
   strategy used by DOSEMU, which is needed by video emulation, EMS, DPMI
   and XMS support and other things to map a given page of memory to the
   required virtual DOS address space.

   Normally DOSEMU will detect the proper mapping driver for the kernel
   you are using, however, in some cases you may want to define it
   explicitely to overcome eventual problems. For this you can specify
         $_mapping= "mapfile"

   to force the use of the driver, which uses a temporary file.

   If you are using a kernel above 2.3.40, you may use
         $_mapping= "mapshm"

   which uses a POSIX shared memory object (the default) or
         $_mapping= "mapashm"

   which uses anonymous shared memory (in case the above gives problems).

   Note, that in case of `mapfile' and `mapshm' the size of the file or
   the segment depend on how much memory you configured for XMS, EMS and
   DPMI (see below). You should take care yourself that you have enough
   diskspace for 'mapfile'. For 'mapshm' the tmpfs mount option
   'size=nbytes' controls the amount of space; by default it is half of
   the (real machine) memory.

   Defining the memory layout, which DOS should see:
      $_xms = (8192)          # in Kbyte
      $_ems = (2048)          # in Kbyte
      $_ems_frame = (0xe400)
      $_dpmi = (0x5000)       # in Kbyte
      $_dosmem = (640)        # in Kbyte, < 640

   Note that (other as in native DOS) each piece of mem is separate, hence
   DOS perhaps will show other values for 'extended' memory. To use EMS
   memory you must load the supplied ems.sys device driver. For XMS memory
   you must either use a DOS XMS driver such as himem.sys, himem.exe,
   fdxms.sys, or fdxxms.sys, or the internal XMS driver via ems.sys.

   If you want mixed operation on the filesystem, from which you boot
   DOSEMU (native and via DOSEMU), it may be necessary to have two
   separate sets of config.sys and system.ini. DOSEMU can fake a different
   file extension, so DOS will get other files when running under DOSEMU.
   Faking autoexec.bat cannot happen in a reliable fashion, so if you
   would like to use an autoexec.bat replacement then just use the SHELL
   command in config.XXX, like this:

   SHELL=COMMAND.COM /P /K AUTOEMU.BAT

      $_emusys = ""    # empty or 3 char., config.sys   -> config.XXX
      $_emuini = ""    # empty or 3 char., system.ini   -> system.XXX

   As you would realize at the first glance: DOS will not have the the CPU
   for its own. But how much it gets from Linux, depends on the setting of
   `hogthreshold'. The HogThreshold value determines how nice Dosemu will
   be about giving other Linux processes a chance to run.

      $_hogthreshold = (1)   # 0 == all CPU power to DOSEMU
                             # 1 == max power for Linux
                             # >1   the higher, the faster DOSEMU will be
     __________________________________________________________________

2.1.4. Code page and character set

   To select the character set and code page for use with DOSEMU you have
         $_external_char_set = "XXX"

   where XXX is one of
    "cp437", "cp737", "cp773", "cp775", "cp850", "cp852", "cp857", "cp860",
    "cp861", "cp862", "cp863", "cp864", "cp865", "cp866", "cp869", "cp874",
    "cp1125", "cp1251"
    "iso8859-1", "iso8859-2", "iso8859-3", "iso8859-4", "iso8859-5", "iso8859-6"
,
    "iso8859-7", "iso8859-8", "iso8859_9", "iso8859-14", "iso8859-15", "koi8-r"
    "koi8-u", "koi8-ru", "utf8"

   The external character set is used to:

     * compute the unicode values of characters coming in from the
       terminal
     * compute the character set index of unicode characters output to a
       terminal display screen.

   The default is to use "", which denotes the current locale, and is
   usually the right setting.

   If you set a DOS external character set, then it is to the user to load
   a proper DOS font (cp437.f16, cp850.f16 or cp852.f16 on the console).

         $_internal_char_set = "XXX"

   where XXX is one of:
    "cp437", "cp737", "cp773", "cp775", "cp850", "cp852", "cp857", "cp860",
    "cp861", "cp862", "cp863", "cp864", "cp865", "cp866", "cp869", "cp874"
    "cp895", "cp1125", "cp1251", "bg-mik"

   The internal character set is used to:

     * compute the unicode value of characters of video memory, when using
       DOSEMU in a terminal or using X with a unicode font.
     * compute the character set index of unicode characters returned by
       bios keyboard translation services.
     * compute the unicode values of characters in file names.
     __________________________________________________________________

2.1.5. Terminals

   This section applies whenever you run DOSEMU remotely, in an xterm or
   on the Linux console without graphics. Color terminal support is now
   built into DOSEMU. Skip this section for now to use terminal defaults,
   until you get DOSEMU to work.
      $_term_color = (on)   # terminal with color support
      $_term_updfreq = (4)  # time between refreshs (units: 20 == 1 second)
      $_escchar = (30)      # 30 == Ctrl-^, special-sequence prefix

   `term_updfreq' is a number indicating the frequency of terminal updates
   of the screen. The smaller the number, the more frequent. A value of 20
   gives a frequency of about one per second, which is very slow.
   `escchar' is a number (ascii code below 32) that specifies the control
   character used as a prefix character for sending alt, shift, ctrl, and
   function keycodes. The default value is 30 which is Ctrl-^. So, for
   example,
      F1 is 'Ctrl-^1', Alt-F7 is 'Ctrl-^s Ctrl-^7'.
      For online help, press 'Ctrl-^h' or 'Ctrl-^?'.
     __________________________________________________________________

2.1.6. Keyboard settings

   When running DOSEMU from console (also remote from console) or X you
   may need to define a proper keyboard layout. It is possible to let
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

   to use this feature under X, because by default the keytable is forced
   to be neutral (us). Normally you will have the correct settings of your
   keyboard given by the X-server.

   The most comfortable method, however, is to first let DOSEMU set the
   keyboard layout itself. This involves 2 parts and can be done by
   setting
         $_X_keycode = (auto)

   which checks for existence of the X keyboard extension and if yes, it
   sets $_X_keycode to 'on', that means the DOSEMU keytables are active.
   The second part (which is independent from $_X_keycode) can be set by
         $_layout = "auto"

   DOSEMU then queries the keyboard layout from the kernel or X (which
   only does work on the console or X, but not in remote text terminals)
   and generates a new DOSEMU keytable out of the kernel information. This
   currently seems only to work for latin-1 layouts, the latin-2 type of
   accents seem not to exist so far in the kernel (linux/keyboard.h). The
   resulting table can be monitor with DOSEMU 'keytable dump' feature (see
   README-tech.txt) for details).

   When being on console you might wish to use raw keyboard, especially
   together with some games, that don't use the BIOS/DOS to get their
   keystrokes.
         $_rawkeyboard = (1)

   However, be carefull, when the application locks, you may not be able
   to switch your console and recover from this. For details on recovering
   look at README-tech.txt (Recovering the console after a crash).
     __________________________________________________________________

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
     __________________________________________________________________

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
   that you want give to DOSEMU. NOTE: You should make sure, that they are
   not used by Linux elsewhere, else you would come into much trouble. To
   help you not doing the wrong thing, DOSEMU can check the devicetype of
   the SCSI device such as
       $_aspi = "sg2:WORM"

   in which case you define /dev/sg2 being a CD writer device. If you omit
   the type,

    $_aspi = "sg2 sg3 sg4"

   DOSEMU will refuse any device that is a disk drive (imagine, what would
   happen if you try to map a CD writer to the disk which contains a
   mounted Linux FS?). If you want to map a disk drive to DOSEMU's ASPI
   driver, you need to tell it explicitely
       $_aspi = "sg1:Direct-Access"

   or
       $_aspi = "sg1:0"

   and as you can see, `Direct-Access' is the devicetype reported by
       $ cat /proc/scsi/scsi

   which will list all SCSI devices in the order they are assigned to the
   /dev/sgX devices (the first being /dev/sg0). You may also use the
   DOSEMU supplied tool `scsicheck' (in src/tools/peripher), which helps a
   lot to get the configuration right:
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
   and DOSEMU will not show more than one hostadapter to DOS (mapping them
   all into one), hence you would get problems accessing sg2 and sg4. For
   this you may remap a different targetID such as
       $_aspi = "sg2:CD-ROM:5 sg4:WORM"

   and all would be fine. From the DOS side the CD-ROM appears as target 5
   and the WORM (CD writer) as target 6. Also from the above scsicheck
   output, you can see, that you can opt to use a `host/channel/ID/LUN'
   construct in place of `sgX' such as
       $_aspi = "0/0/6/0:CD-ROM:5 1/0/6/0:WORM"

   which is exactly the same as the above example, exept it will assign
   the right device, even if for some reasons you have changed the order
   in which sgX device are assigned by the kernel. Those changes happen,
   if you turned off power of one device `between' or if you play with
   dynamic allocation of scsi devices via the /proc/scsi interface such as
       echo "scsi remove-single-device 0 0 5 0" >/proc/scsi/scsi

   to delete a device and
       echo "scsi add-single-device 0 0 5 0" >/proc/scsi/scsi

   to add a device. HOWEVER, we strongly discourage you to use these
   kernel feature for temporaryly switching off power of connected devices
   or even unplugging them: normal SCSI busses are not hotpluggable.
   Damage may happen and uncontroled voltage bursts during power off/on
   may lock your system !!!

   Coming so far, one big problem remains: the (hard coded) buffersize for
   the sg devices in the Linux kernel (default 32k) may be to small for
   DOS applications and, if your distributor yet didn't it, you may need
   to recompile your kernel with a bigger buffer. The buffer size is
   defined in linux/include/scsi/sg.h and to be on the secure side you may
   define
       #define SG_BIG_BUFF (128*1024-512)  /* 128 Kb buffer size */

   though, CD writers are reported to work with 64Kb and the `Iomega
   guest' driver happily works with the default size of 32k.
     __________________________________________________________________

2.1.9. COM ports and mice

   We have simplified the configuration for mice and serial ports and
   check for depencies between them. If all strings in the below example
   are empty, then no mouse and/or COM port is available. Note. that you
   need no mouse.com driver installed in your DOS environment, DOSEMU has
   the mousedriver builtin. The mouse settings below only apply to DOSEMU
   in the Linux console; in X and terminals the mouse is detected
   automatically. The below example is such a setup

      $_com1 = ""           # e.g. "/dev/mouse" or "/dev/ttyS0"
      $_com2 = "/dev/modem" # e.g. "/dev/modem" or "/dev/ttyS1"

      $_mouse = "microsoft" # one of: microsoft, mousesystems, logitech,
                            # mmseries, mouseman, hitachi, busmouse, ps2
      $_mouse_dev = "/dev/mouse" # one of: com1, com2 or /dev/mouse
      $_mouse_flags = ""        # list of none or one or more of:
                        # "emulate3buttons cleardtr"
      $_mouse_baud = (0)        # baudrate, 0 == don't set

   The above example lets you have your modem on COM2, COM1 is spare (as
   you may have your mouse under native DOS there and don't want to change
   the configuration of your modem software between boots of native DOS
   and Linux)

   However, you may use your favorite DOS mouse driver and directly let it
   drive COM1 by changing the below variables (rest of variables
   unchanged)

      $_com1 = "/dev/mouse"
      $_mouse_dev = "com1"

   And finaly, when you have a PS/2 or USB mouse on your machine you use
   the built-in mouse driver (not your mouse.com) to get it work: ( again
   leaving the rest of variables unchanged)

      $_mouse = "ps2"
      $_mouse_dev = "/dev/mouse"

   When using a PS/2 or USB mouse or when having more than 2 serial ports
   you may of course assign _any_ free serialdevice to COM1, COM2. The
   order doesn't matter:

      $_com1 = "/dev/ttyS2"
      $_com2 = "/dev/ttyS0"
     __________________________________________________________________

2.1.10. Printers

   Printer is emulated by piping printer data to your normal Linux
   printer. The belows tells DOSEMU which printers to use. The `timeout'
   tells DOSEMU how long to wait after the last output to LPTx before
   considering the print job as `done' and to close it.

    # Print commands to use for LPT1, LPT2 and LPT3.
    # Default: "lpr -l, lpr -l -P lpt2, lpr -l P lpt3"
    # Which means: use the default print queue for LPT1, "lpt2" queue for LPT2,
    # "lpt3" queue for LPT3. "-l" means raw printing mode (no preprocessing).

    $_lpt1 = "lpr -l"
    $_lpt2 = "lpr -l -P lpt2"
    $_lpt3 = "lpr -l -P lpt3"

    $_printer_timeout = (20)# idle time in seconds before spooling out
     __________________________________________________________________

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
     __________________________________________________________________

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
                              # increases performance if >0 and processor>=Penti
um
                              # recommended: 1-50ms or 0 if unsure
     __________________________________________________________________

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
     __________________________________________________________________

2.1.14. Settings for enabling direct hardware access

   The following settings (together with the direct console video settings
   below make it possible for DOSEMU to access your real (non-emulated)
   computer hardware directly. Because Linux does not permit this for
   ordinary users, DOSEMU needs to be run as root, via sudo or suid-root
   to be able to use these settings. They can NOT be overwritten by the
   user configuration file ~/.dosemurc. You must also run dosemu with the
   "-s" switch. This activates sudo when appropriate; without it, even
   root will not get direct hardware access.

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
   DOS driver for, it may be necessary to enable IRQ passing to DOS. Note
   that IRQ passing does not work on x86-64.
      $_irqpassing = ""  # list of IRQ number (2-15) to pass to DOS such as
                         # "3 8 10"
     __________________________________________________________________

2.1.15. Video settings ( console only )

   !!WARNING!!: IF YOU ENABLE GRAPHICS ON AN INCOMPATIBLE ADAPTOR, YOU
   COULD GET A BLANK SCREEN OR MESSY SCREEN EVEN AFTER EXITING DOSEMU.
   Read doc/README-tech.txt (Recovering the console after a crash).

   Start with only text video using the following setup

      $_video = "vga"         # one of: plainvga, vga, ega, mda, mga, cga
      $_console = (0)         # use 'console' video
      $_graphics = (0)        # use the cards BIOS to set graphics
      $_vbios_seg = (0xc000)  # set the address of your VBIOS (e.g. 0xe000)
      $_vbios_size = (0x10000)# set the size of your BIOS (e.g. 0x8000)
      $_vmemsize = (0)        # amount of video RAM to save/restore
      $_chipset = ""
      $_dualmon = (0)         # if you have one vga _plus_ one hgc (2 monitors)

   After you get it `somehow' working and you have one of the DOSEMU
   supported graphic cards you may switch to graphics mode changing the
   below

         $_graphics = (1)        # use the cards BIOS to set graphics

   If you have a 100% compatible standard VGA card that may work, however,
   you get better results, if your card has one of the DOSEMU supported
   video chips and you tell DOSEMU to use it such as

      $_chipset = "s3"        # one of: plainvga, trident, et4000, diamond, s3,
                              # avance, cirrus, matrox, wdvga, paradise, ati, si
s,
                          # svgalib, vesa

   Note, `s3' is only an example, you must set the correct video chip else
   it most like will crash your screen.

   The 'svgalib' setting uses svgalib 1.9.21 or greater for determining
   the correct video chip.

   The 'vmemsize' setting's default of 0 causes DOSEMU to try to
   autodetect the amount of video memory on the graphics card. This amount
   is saved and restored when you switch away from and back to the virtual
   console DOSEMU runs in using the Ctrl-Alt-Fn key combination. Since
   saving video memory can be a very slow operation you may want to
   restrict 'vmemsize' to a value that was more common when DOS was still
   mainstream. For instance 1024 covers it if you want to be able to save
   and restore modes up to 1024x768x256.

   NOTE: `video setting' can not be overwritten by ~/.dosemurc.
     __________________________________________________________________

2.1.16. Time settings

   By Paul Crawford. This is a short guide to the time modes supported in
   DOSEMU; you can get a more detailed description of how and why they
   operate at http://www.sat.dundee.ac.uk/~psc/dosemu_time_advanced.html.
   There are three modes currently supported using the $_timemode
   variable:
      $_timemode = "bios"
      $_timemode = "pit"
      $_timemode = "linux"

   Most users will only ever have a need for either BIOS or LINUX mode,
   and the decision comes down to the two basic characteristics:

     * In BIOS mode, the DOSEMU session begins with the current (local)
       date & time, and once running it behaves fully like an emulated PC.
       You can set the date & time using the normal DOS methods as you
       would expect. However, time keeping accuracy in the long term under
       DOS was never very good, and under DOSEMU it is typically poorer.
     * In LINUX mode, the emulated PC always reports the current host
       time, and so you no longer have the option to set the DOS time (the
       date can be set, which is odd, as this is kept in DOS and not in
       the emulator). In this mode you get the long term accuracy of the
       host, so if you are running NTP or similar, you always get accurate
       time.

   The PIT mode is an odd compromise, it provides BIOS-like time
   (settable, decoupled from host time), but with slightly higher
   accuracy. In summary, if you need accurate time, choose LINUX mode,
   otherwise if you need time setting capabilities or close 'real' PC
   emulation, choose BIOS mode.

   A reasonable question for some applications is "how do I get accurate
   UTC time?", of course, the more general question is about running the
   emulated DOS session in a different timezone and/or with differing
   "daylight saving" options to the host computer. This is quite simple in
   fact, just start DOSEMU with the environment variable TZ set to the
   required zone, and don't forget to have the corresponding command in
   the DOS autoexec.bat file (otherwise Microsoft products all assume -8
   hours PST!). The DOSEMU emulation of file system access is also based
   on the local timezone it is running in, so you will get consistent file
   time stamps compared to the emulated time. So in the UTC case, you
   would start DOSEMU in a shell with TZ=GMT0, and have a line in
   autoexec.bat with "set TZ=GMT0" before any of your programs are run.
     __________________________________________________________________

3. Security

   This part of the document by Hans Lermen, <lermen@fgan.de> on Apr 6,
   1997.

   These are the hints we give you, when running dosemu on a machine that
   is (even temporary) connected to the internet or other machines, or
   that otherwise allows 'foreign' people login to your machine.

     * Don't set the "s" bit, as DOSEMU can run in lowfeature mode without
       the "s" bit set. If you want fullfeatures for some of your users,
       it is recommended to use "sudo". Alternatively you can just use the
       keyword `nosuidroot' in /etc/dosemu.users to forbid some (or all)
       users execution of a suid root running dosemu (they may use a
       non-suid root copy of the binary though). DOSEMU now drops its root
       privileges before booting; however there may still be security
       problems in the initialization code, and by making DOSEMU suid-root
       you can give users direct access to resources they don't normally
       have access too, such as selected I/O ports, hardware IRQs and
       hardware RAM. For any direct hardware access you must always use
       the "-s" switch.
       If DOSEMU is invoked via "sudo" then it will automatically switch
       to the user who invoked "sudo". An example /etc/sudoers entry is
       this:

    joeuser  hostname=(root) NOPASSWD: /usr/local/bin/dosemu.bin

       If you use PASSWD instead of NOPASSWD then users need to type their
       own passwords when sudo asks for it. The "dosemu" script can be
       invoked using the "-s" option to automatically use sudo.
     * Use proper file permissions to restrict access to a suid root
       DOSEMU binary in addition to /etc/dosemu.users `nosuidroot'. (
       double security is better ).
     * NEVER let foreign users execute dosemu under root login !!!
       (Starting with dosemu-0.66.1.4 this isn't necessary any more, all
       functionality should also be available when running as user)
     __________________________________________________________________

4. Sound

   The SB code is currently in a state of flux. Some changes to the code
   have been made which mean that I can separate the DSP handling from the
   rest of the SB code, making the main case statements simpler. In the
   meantime, Rutger Nijlunsing has provided a method for redirecting
   access to the MPU-401 chip into the main OS.
     __________________________________________________________________

4.1. Using the MPU-401 "Emulation"

   The Sound driver opens "/var/run/dosemu-midi" and writes the Raw MIDI
   data to this. A daemon is provided which can be can be used to seletc
   the instruments required for use on some soundcards. It is also
   possible to get various instruments by redirecting
   '/var/run/dosemu-midi' to the relevant part of the sound driver eg:

       % ln -s /dev/midi /var/run/dosemu-midi

   This will send all output straight to the default midi device and use
   whatever instruments happen to be loaded.
     __________________________________________________________________

4.2. The MIDI daemon

         make midid

   This compiles and installs the midi daemon. The daemon currently has
   support for the 'ultra' driver and partial support for the 'OSS' driver
   (as supplied with the kernel) and for no midi system. Support for the
   'ultra' driver will be compiled in automatically if available on your
   system.

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
     __________________________________________________________________

4.3. Disabling the Emulation at Runtime

   You can disable the SB emulation by changing the 'sound' variable in
   /etc/dosemu.conf to 'off'. There is currently no way to specify at
   runtime which SB model DOSEMU should emulate; the best you can do is
   set the T value of the BLASTER environment variable (see
   sound-usage.txt), but not all programs will take note of this.
     __________________________________________________________________

5. Using Lredir

   This section of the document by Hans, <lermen@fgan.de>. Last updated on
   October, 23 2002.

   What is it? Well, its simply a small DOS program that tells the MFS
   (Mach File System) code what 'network' drives to redirect. With this
   you can 'mount' any Linux directory as a virtual drive into DOS. In
   addition to this, Linux as well as multiple dosemu sessions may
   simultaneously access the same drives, what you can't when using
   partition access.
     __________________________________________________________________

5.1. how do you use it?

   Mount your dos hard disk partition as a Linux subdirectory. For
   example, you could create a directory in Linux such as /dos (mkdir -m
   755 /dos) and add a line like

          /dev/hda1       /dos     msdos   umask=022

   to your /etc/fstab. (In this example, the hard disk is mounted read-
   only. You may want to mount it read/write by replacing "022" with "000"
   and using the -m 777 option with mkdir). Now mount /dos. Now you can
   add a line like

         lredir d: linux\fs/dos

   to the AUTOEXEC.BAT file in your hdimage (but see the comments below).
   On a multi-user system you may want to use

         lredir d: linux\fs\${home}

   where "home" is the name of an environmental variable that contains the
   location of the dos directory (/dos in this example)

   You may even redirect to a NFS mounted volume on a remote machine with
   a /etc/fstab entry like this
          otherhost:      /dos     nfs     nolock

   Note that the nolock> option might be needed for 2.2.x kernels, because
   apparently the locks do not propagate fast enough and DOSEMU's (MFS
   code) share emulation will fail (seeing a lock on its own files).

   In addition, you may want to have your native DOS partion as C: under
   dosemu. To reach this aim you also can use Lredir to turn off the
   'virtual' hdimage and switch on the real drive C: such as this:

   Assuming you have a c:\dosemu directory on both drives (the virtual and
   the real one) and have mounted your DOS partition as /dosc, you then
   should have the following files on the virtual drive:

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
     __________________________________________________________________

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
     __________________________________________________________________

6. Running dosemu as a normal user

   This section of the document by Hans, <lermen@fgan.de>. Last updated on
   Jan 21, 2003.

    1. In the default setup, DOSEMU does not have root privileges. This
       means it will not have direct access to ports, external DOSish
       hardware and won't use the console other than in normal terminal
       mode, but is fully capable to do anything else. See the previous
       section on how to enable privileged operation if you really need
       to.
    2. If a user needs access to privileged resources other than console
       graphics, then you may need to explicitly allow the user to do so
       by editing the file /etc/dosemu.users (or
       /etc/dosemu/dosemu.users). The format is:

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

       Note: the changes to /etc/passwd and /etc/group only take place the
       next time you login, so don't forget to re-login.
       The fstab entry will mount /dosc such that is has the proper
       permissions

       ( drwxrwxr-x  22 root     dosemu      16384 Jan  1  1970 /dosc )

       You can do the same with an explicit mount command:

          mount -t msdos -o gid=200,umask=002 /dev/hda1 /dosc

       Of course normal lredir'ed unix directories should have the same
       permissions.
    4. Make sure you have read/write permissions of the devices you
       configured (in /etc/dosemu.conf) for serial and mouse.

   Starting with dosemu-0.66.1.4 there should be no reason against running
   dosemu as a normal user. The privilege stuff has been extensively
   reworked, and there was no program that I tested under root, that
   didn't also run as user. Normally dosemu will permanently run as user
   and only temporarily use root privilege when needed (during
   initialization) and then drop its root privileges permanently. In case
   of non-suid root (as of dosemu-0.97.10), it will run in lowfeature mode
   without any privileges.
     __________________________________________________________________

7. Using CDROMS

7.1. The built-in driver

   This documents the cdrom extension rucker@astro.uni-bonn.de has written
   for Dosemu.

   An easy way to access files on a CDROM is to mount it in Linux and use
   Lredir to refer to it. However, any low-level access, often used by
   games is impossible that way, unless you use the C option. For that you
   need to load some drivers in DOS. CDROM image files (ISOs) can be used
   in a similar fashion.

   The driver consists of a server on the Linux side
   (src/dosext/drivers/cdrom.c, accessed via int 0xe6 handle 0x40) and a
   device driver (src/commands/cdrom.S) on the DOS side.

   Please send any suggestions and bug reports to
   <rucker@astro.uni-bonn.de>

   To install it:

     * Create a (symbolic) link /dev/cdrom to the device file of your
       drive or use the cdrom statement in dosemu.conf to define it.
     * Make sure that you have read/write access to the device file of
       your drive, otherwise you won't be able to use the cdrom under
       Dosemu directly because of security reasons.
     * Load cdrom.sys within your config.sys file with e.g.:

        devicehigh=d:\dosemu\cdrom.sys

     * Mount the CD-ROM in Linux (some distributions do this
       automatically), and use

        lredir e: linux\fs/media/cdrom c

       to access the CD-ROM as drive E:. The "C" option specifies that the
       low-level access provided via cdrom.sys is used. Or ... start
       Microsoft cdrom extension as follows:

        mscdex /d:mscd0001 /l:driveletter

       or

        shsucdex /d:mscd0001 /l:driveletter

   To change the cd while Dosemu is running, use the DOS program
   'eject.com'. If is not possible to change the disk, when the drive has
   been opened by another process (e.g. mounted), then you need to unmount
   it first!

   Lredir has the advantage of supporting long file names, and not using
   any DOS memory, whereas MS/SHSUCDX are more low-level and perhaps more
   compatible. You would need to use a DOS TSR driver such as DOSLFN to
   use long file names with SHSUCDX.

   Remarks by zimmerma@rz.fht-esslingen.de:

   This driver has been successfully tested with Linux' SCSI CDROMS by the
   author, with the Mitsumi driver mcd.c and with the
   Aztech/Orchid/Okano/Wearnes- CDROM driver aztcd.c by me. With the
   latter CDROM-drives changing the CD-ROM is not recognized correctly by
   the drive under all circumstances and is therefore disabled. So
   eject.com will not work. For other CD-ROM drives you may enable this
   feature by setting the variable 'eject_allowed = 1' in file
   dosemu/drivers/cdrom.c (you'll find it near the top of the file). With
   the mcd.c and aztcd.c Linux drivers this may cause your system to hang
   for some 30 seconds (or even indefinitely), so don't change the default
   value 'eject_allowed = 0'.

   Support for up to 4 drives:

   If you have more then one cdrom, you can use the cdrom statement in
   dosemu.conf like this:
         $_cdrom = "/dev/cdrom /dev/cdrom2 image.iso"

   and have multiple instancies of the DOS driver:
      device=cdrom.sys
      device=cdrom.sys 2
      device=cdrom.sys 3

   The one and only parameter to the device driver is a digit between 1
   and 4, (if its missing then 1 is assumed) for the DOS devices MSCD0001,
   MSCD0002 ... MSCD0004 respectively. You then also need to use lredir or
   tell MSCDEX about these drivers such as

        lredir e: linux\fs/media/cdrom c
        lredir f: linux\fs/media/cdrom2 c 2
        lredir g: linux\fs/media/cdrom3 c 3

   where you need to loop-mount the image file, or

        mscdex /d:mscd0001 /d:mscd0002 /l:driveletter

   In this case the /l: argument defines the driveletter of the first /d:,
   the others will get assigned successive driveletters.

   History:

   Release with dosemu.0.60.0 Karsten Rucker (rucker@astro.uni-bonn.de)
   April 1995

   Additional remarks for mcd.c and aztcd.c Werner Zimmermann
   (zimmerma@rz.fht-esslingen.de) May 30, 1995

   Release with dosemu-0.99.5 Manuel Villegas Marin (manolo@espanet.com)
   Support for up to 4 drives December 4, 1998
     __________________________________________________________________

8. Using X

   This chapter provides some hints and tips for using DOSEMU in X.
     __________________________________________________________________

8.1. Basic information

   If you start dosemu in X it brings up its own window, in which you can
   also execute graphical programs such as games. To force text-only
   execution of DOSEMU in an xterm or other terminal (konsole,
   gnome-terminal, and so on), you need to run dosemu -t.

   Use dosemu -w to start DOSEMU in fullscreen mode. When running DOSEMU,
   you can flip between fullscreen and windowed mode by pressing
   <Ctrl><Alt><F>. The graphics window is resizeable.

   Some DOS applications want precise mouse control that is only possible
   if the mouse cursor is trapped in the DOSEMU window. To enable this you
   need to grab the mouse by pressing <Ctrl><Alt><Home>. Similarly, you
   can grab the keyboard by pressing <Ctrl><Alt><K>, or
   <Ctrl><Alt><Shift><K> if your window manager already grabs
   <Ctrl><Alt><K>. After you grab the keyboard all key combinations
   (including <Alt><Tab> and so on) are passed to DOSEMU. In fullscreen
   mode the mouse and keyboard are both automatically grabbed.

   Use <Ctrl><Alt><Pause> to pause and unpause the DOSEMU session, which
   is useful if you want it to sit silently in the background when it is
   eating too much CPU time. Press <Ctrl><Alt><PgDn> or click the close
   button of the window to exit DOSEMU.

   DOSEMU uses bitmapped internal fonts by default, so it can accurately
   simulate a real VGA card text mode. It is also possible to use X fonts.
   The advantages of these is that they may be easier on the eyes, and are
   faster, in particular if you use DOSEMU remotely. Any native DOS font
   setting utilities do not work, however. To set an X font use the
   provided xmode.com utility, using
       xmode -font vga

   at the DOS prompt or
       $_X_font = "vga"

   in dosemu.conf. The provided fonts are vga, vga8x19, vga11x19,
   vga10x24, vga12x30, vga-cp866, and vga10x20-cp866.

   If the mouse is not grabbed, then you can copy and paste text if the
   DOSEMU window is in text mode. This uses the standard X mechanism:
   select by dragging the left mouse button, and paste by pressing the
   middle mouse button.
     __________________________________________________________________

8.2. More detailed information, hints and tips

   What you might take care of:

     * If backspace and delete do not work, you can try 'xmodmap -e
       "keycode 22 = 0xff08"' to get use of your backspace key, and
     * 'xmodmap -e "keycode 107 = 0xffff"' to get use of your delete key.
     * Make sure DOSEMU has X support compiled in. The configure script
       complains loudly if it does not find X development libraries.
     * There are some X-related configuration options for dosemu.conf. See
       the file itself for details.
     * Keyboard support of course depends on your X keyboard mappings
       (xmodmap). If certain keys don't work (like Pause, Backspace,...),
       it *might* be because you haven't defined them in your xmodmap, or
       defined to something other than DOSEMU expects.
     * using the provided icon (dosemu.xpm):
          + you need the xpm (pixmaps) package. If you're not sure, look
            for a file like /usr/X11R6/lib/libXpm.so.*
          + you also need a window manager which supports pixmaps. Fvwm is
            fine, but I can't tell you about others. Twm probaby won't do.
          + copy dosemu.xpm to where you usually keep your pixmap (not
            bitmap!) files (perhaps /usr/share/pixmaps)
          + tell your window manager to use it. For fvwm, add the
            following line to your fvwmrc file:


         Icon "xdosemu"   dosemu.xpm

            This assumes you have defined a PixmapPath. Otherwise, specify
            the entire pathname.
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
     __________________________________________________________________

8.3. The VGA Emulator

   In X, a VGA card is emulated. The same happens if you use the SDL
   library using dosemu -S. This emulation (vgaemu) enables X to run
   graphics modes.

   Some features:

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
     * The attribute controller is emulated.
     * The emulator emulates a basic Trident TVGA8900C graphics card. All
       standard VGA modes are emulated, most VGA hardware features
       (mode-X, 320x240 and so on), some Trident extensions, and on top of
       that many high-resolution VESA 2.0 modes, that were not present in
       the original Trident card. Some (very few) programs, such as Fast
       Tracker, play intimately with the VGA hardware and may not work. As
       vgaemu improves these problems should disappear.
     * Nearly full VESA 2.0 support.
     * A VESA compatible video BIOS is mapped at 0xc00000. It is small
       because it only contains some glue code and the BIOS fonts (8x8,
       8x14, 8x16).
     * support for hi- and true-color modes (using Trident SVGA mode
       numbers and bank switching)
     * Support for mode-X type graphics modes (non-chain4 modes as used by
       e.g. DOOM)
     * gamma correction for graphics modes
     * video memory size is configurable via dosemu.conf
     * initial graphics window size is configurable
     * The current hi- (16bpp) and true (24/32bpp) color support does not
       allow resizing of the graphics window and gamma correction is
       ignored.

   As the typical graphics mode with 320x200x8 will be used often with
   large scalings and modern graphics boards are pretty fast, Steffen
   Winterfeldt added something to eat up your CPU time: you can turn on
   the bilinear interpolation. It greatly improves the display quality
   (but is rather slow as I haven't had time yet to implement an optimized
   version - it's plain C for now). If the bilinear filter is too slow,
   you might instead try the linear filter which interpolates only
   horizontally.

   Note that (bi)linear filtering is not available on all VGA/X display
   combinations. The standard drawing routines are used instead in such
   cases.

   If a VGA mode is not supported on your current X display, the graphics
   screen will just remain black. Note that this does not mean that DOSEMU
   has crashed.

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
   marked as unsupported in the VBE mode list (but are still in it).

   The current interface between VGAEmu and X will try to update all
   invalid video pages at a time. This may, particularly in hi-res
   VBE/SVGA modes, considerably disturb DOSEMU's signal handling. That
   cannot be helped for the moment, but will be addressed soon (by running
   an extra update thread).

   If you really think that this is the cause of your problem, you might
   try to play with veut.max_max_len in env/video/render.c. This variable
   limits the amount of video memory that is updated during one timer
   interrupt. This way you can dramatically reduce the load of screen
   updates, but at the same rate reduce your display quality.

   Gamma correction works in both 4 and 8 bit modes. It must be specified
   as a float value, e.g. $_X_gamma=(1.0). Higher values give brighter
   graphics, lower make them darker. Reasonable values are within a range
   of 0.5 ... 2.0.

   You can specify the video memory size that the VGA emulator should use
   in dosemu.conf. The value will be rounded up to the nearest 256 kbyte
   boundary. You should stick to typical values like 1024, 2048 or 4096 as
   not to confuse DOS applications. Note that whatever value you give, 4
   bit modes are only supported up to a size of 800x600.

   You can influence the initial size of the graphics window in various
   ways. Normally it will have the same size (in pixel) as the VGA
   graphics mode, except for mode 0x13 (320x200, 256 colors), which will
   be scaled by the value of mode13fact (defaults to 2). Alternatively,
   you can directly specify a window size in dosemu.conf via $_X_winsize.
   You can still resize the window later.

   The config option $_X_fixed_aspect allows you to fix the aspect ratio
   of the graphics window while resizing it. Alternatively, $_X_aspect_43
   ties the aspect ratio to a value of 4:3. The idea behind this is that,
   whatever the actual resolution of a graphics mode is in DOS, it is
   displayed on a 4:3 monitor. This way you won't run into problems with
   modes such as 640x200 (or even 320x200) which would look somewhat
   distorted otherwise.

   For planar modes (for instance, most 16 colour modes, but also certain
   256-colour modes: mode-X), vgaemu has to switch to partial cpu
   emulation. This can be slow, but expect it to improve over time.
     __________________________________________________________________

9. Running Windows under DOSEMU

   DOSEMU can run any 16bit Windows up to 3.1, and has limited support for
   Windows 3.11. You should be able to install and run these versions of
   Windows without any extra work. There are still a few caveats however,
   and if you have some problems, read on.
     __________________________________________________________________

9.1. Mouse in Windows under DOSEMU

   When using Windows 3.1, the windows mouse cursor can get out of sync
   with the mouse pointer of your X session. To solve this problem you
   need to install an alternative mouse driver:
   http://www.pocketdos.com/Misc/PocketDOS_Win3xMousePM.zip This archive
   contains the file pdmouse.drv, which you can simply copy over the
   existing mouse.drv in your WINDOWS\SYSTEM directory. Alternatively you
   can use the install procedure documented in the driver's ReadMeEN.txt
   file.
     __________________________________________________________________

9.2. Windows 3.x in SVGA modes

   If you want to run Windows in SVGA mode, you can either use the Trident
   drivers, or the patched SVGA drivers of Windows 3.11.

   The Trident drivers for Windows are in the files tvgaw31a.zip and/or
   tvgaw31b.zip. Search www.winsite.com to get them. Unpack the archive.
   In Windows setup, install the Trident "800x600 256 color for 512K
   boards" driver.

   Windows 3.11 comes with SVGA drivers. You can also download them:
   search www.winsite.com for svga.exe and install the drivers. Then go to
   http://www.japheth.de/dwnload1.html and get the SVGAPatch tool. This
   tool causes the above drivers to work with most of the video cards in a
   real DOS environment, and makes them DOSEMU-compatible too.
     __________________________________________________________________

9.3. VxD support

   By the time of writing this, DOSEMU does not have support for Windows
   ring-0 device drivers (.vxd, .386). Fortunately, most of Windows 3.1
   drivers are ring-3 ones (.drv), so you can easily install the Sound
   Blaster drivers, for instance. This is not the case with Windows 3.11.
   Its network drivers are ring-0 ones, so the native Winsock does not
   work. In order to get networking operational, you need to get the
   Trumpet Winsock package. Refer to chapter "Using Windows and Winsock"
   for more info on this.
     __________________________________________________________________

9.4. DOS shell in Windows

   There is probably little use of a DOS shell under Windows under
   DOSEMU... But for those who need it, here are some basic hints. The DOS
   shell is supported by DOSEMU only if Windows is running in Standard
   mode. The Enhanced mode DOS shell is currently unsupported. Note
   however, that unlike in a real DOS environment, under DOSEMU the DOS
   shell of the Windows in Standard mode allows you to run protected mode
   applications (in the real DOS environment only the DOS shell of the
   Enhanced mode allows this).
     __________________________________________________________________

10. The DOSEMU mouse

   This section written by Eric Biederman <eric@dosemu.org>
     __________________________________________________________________

10.1. Setting up the emulated mouse in DOSEMU

   For most dos applications you should be able to use the internal mouse
   with very little setup, and very little trouble.

   Under X, or in terminal mode, you don't need to do anything, unless you
   want to use the middle button then you need to add to autoexec.bat:
       emumouse 3

   On the console, in text mode, without root, the GPM library can be
   used, and no extra setup is necessary. Otherwise, especially with
   console graphics (sudo/suid/root, the -s switch, and $_graphics=(1)),
   it takes just a tad bit more work:

   in dosemu.conf:
    $_mouse = "mousesystems"
    $_mouse_dev = "/dev/gpmdata"

   And in autoexec.bat:
       emumouse 3

   This sets you up to use the gpm mouse repeater if you don't use the
   repeater, you need to set $_mouse and $_mouse_dev to different values.
   The GPM repeater might be configured to use a different protocol than
   the default. If you are having problems, check the 'repeat_type'
   setting in your gpm.conf. These are the mappings from the GPM
   repeat_type to the DOSEMU $_mouse for common settings:
    GPM setting      DOSEMU setting
    -------------------------------
    msc (default)    mousesystems
    ms3              microsoft
    raw              select type of your real mouse
     __________________________________________________________________

10.2. Problems

   In X there are 2 ways applications can get into trouble.

   The most common way is if they don't trust the mouse driver to keep
   track of where the mouse is on the screen, and insist on doing it
   themselves. win31 & geoworks are too applications in this category.
   They read mouse movement from the mouse driver in turns of mickeys i.e.
   they read the raw movement data.

   To support this mouse driver then tracks where you were and where you
   move to, in terms of x,y screen coordinates. Then works the standard
   formulas backwards that calculate screen coordinates from mickeys to
   generate mickeys from screen coordinates. And it feeds these mickeys to
   the application. As long as the application and dosemu agree on the
   same formulas for converting mickeys to screen coordinates all is good.

   The only real problem with this is sometimes X mouse and the
   application mouse get out of sync. Especially if you take your mouse
   cursor out of the dosemu window, and bring it back in again.

   To compensate for getting out of sync what we do is whenever we reenter
   the Xdos window is send a massive stream of mickeys heading for the
   upper left corner of the screen. This should be enough to kick us any
   an good mouse drivers screen limits. Then once we know where we are we
   simulate movement for 0,0.

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
   $_X_mgrab_key contains an X keysym of a key that when pressed with both
   Ctrl & Alt held down will turn on the mouse grab, which restricts the X
   mouse to the dosemu window, and gives dosemu complete control over it.
   Ctrl-Alt-$_X_mgrab_key will then release the mouse grab returning
   things to normal.

   I like: $_X_mgrab_key="Scroll_Lock" (Ctrl-Alt-Scroll_Lock) but
   $_X_mgrab_key="a" is a good conservative choice. (Ctrl-Alt-A) You can
   use xev to see what keysyms a key generates.

   Currently the way the X mouse code and the mouse grab are structured
   the internal mouse driver cannot display the mouse when the mouse grab
   is active. In particular without the grab active to display the mouse
   cursor we just let X draw the mouse for us, (as normal). When the mouse
   grab is active we restrict the mouse to our current window, and
   continually reset it to the center of the current screeen (allowing us
   to get relative amounts of movement). A side effect of this is that the
   the position of the X cursor and the dos cursor _not_ the same. So we
   need a different strategy to display the dos cursor.

   The other way an application can get into trouble in X, and also on the
   console for that matter is if it is partially broken. In particular the
   mouse driver is allowed to return coordinates that have little to no
   connection with the actual screen resolution. So an application mouse
   ask the mouse driver it's maximums and then scale the coordinates it
   gets into screen positions. The broken applications don't ask what the
   maximum & minimum resolutions are and just assume that they know what
   is going on.

   To keep this problem from being too severe in mouse.c we have attempted
   to match the default resolutions used by other mouse drivers. However
   since this is up to the choice of an individual mouse driver there is
   doubtless code out there developed with different resolutions in mind.

   If you get stuck with such a broken application we have developed a
   work around, that is partially effective. The idea being that if the
   application draws it's own mouse pointer it really doesn't matter where
   the dos mouse driver thinks the mouse is things should work. So with
   emumouse it is possible to set a minimum resolution to return to an
   application. By setting this minimum resolution to as big or bigger
   than the application expect to see it should work. The side effect of
   setting a minimum resolution bigger than the application expects to see
   in X is that there will be some edges to the of the screen where the
   application draws the cursor at the edge of the window, and yet you
   need to continue scrolling a ways before the cursor comes out there. In
   general this will affect the right and bottom edges of the screen.

   To read the current minimum use:
       emumouse i

   The default is 640x200

   To set the minimum resolution use:
       emumouse Mx 640 My 200

   If you application doesn't draw it's own mouse cursor a skew of this
   kind can be nasty. And there is no real good fix. You can set the
   mininum down as well as up and so it may be possible to match what the
   app expects as an internal resolution. However there is only so much we
   can do to get a borken application to run and that appears to be the
   limit.
     __________________________________________________________________

11. Mouse Garrot

   This section, and Mouse Garrot were written by Ed Sirett
   <ed@cityscape.co.uk> on 30 Jan 1995.

   Mouse Garrot is an extension to the Linux Dos Emulator that tries to
   help the emulator give up CPU time to the rest of the Linux system.

   It tries to detect the fact the the mouse is not doing anything and
   thus to release up CPU time, as is often the case when a mouse-aware
   program is standing idle, such programs are often executing a loop
   which is continually checking the mouse buttons & position.

   Mouse Garrot is built directly into dosemu if and only if you are using
   the internal mouse driver of dosemu. All you have to do is make sure
   that the HOGTHRESHOLD value in the config file is non-zero.

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
     __________________________________________________________________

12. Running a DOS application directly from Unix shell

   This part of the document was written by Hans <lermen@fgan.de>.

   This chapter deals with starting DOS commands directly from Linux. You
   can use this information to set up icons or menu items in X. Using the
   keystroke, input and output redirection facilities you can use DOS
   commands in shell scripts, cron jobs, web services, and so on.
     __________________________________________________________________

12.1. Using unix -e in autoexec.bat

   The default autoexec.bat file has a statement unix -e at the end. This
   command executes the DOS program or command that was specified on the
   dosemu command line.

   For example:
       dosemu "/home/clarence/games/commander keen/keen1.exe"

   will automatically:

     * Lredir "/" if the specified program is not available from an
       already-redirected drive,
     * "cd" to the correct directory,
     * execute the program automagically,
     * and quit DOSEMU when finished, all without any typing in DOS.

   Using "-E" at the command line causes DOSEMU to continue after the DOS
   program finishes.

   You can also specify a DOS command, such as "dir". A combination with
   the -dumb command-line option is useful if you want to retrieve the
   output of the DOS command, such as
       dosemu -dumb dir > listing

   In this case (using -dumb, but not -E) all the startup messages that
   DOSEMU and DOS generate are suppressed and you only get the output of
   "dir". The output file contains DOS end-of-line markers (CRLF).
     __________________________________________________________________

12.2. Using the keystroke facility.

   Make use of the -input command-line option, such as
       dosemu -input 'dir > C:\\garbage\rexitemu\r'

   The '...' will be 'typed in' by DOSEMU exactly as if you had them typed
   at the keyboard. The advantage of this technique is, that all DOS
   applications will accept them, even interactive ones. A '\' is
   interpreted as in C and leads in ESC-codes. Here is a list of the
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
     __________________________________________________________________

12.3. Using an input file

     * Make a file "FILE" containing all keystrokes you need to boot
       DOSEMU and to start your dos-application, ... and don't forget to
       have CR (literal ^M) for 'ENTER'. FILE may look like this (as on my
       machine):

    2^Mdir > C:\garbage^Mexitemu^M

       which could choose point 2 of the boot menu, execute 'dir' with
       output to 'garbage', and terminate DOSEMU, where the ^M stands for
       CR.
     * and execute DOSEMU like this:

    dosemu -dumb < FILE

   In bash you can also use
       echo -e 'dir \gt; c:\\garbage\rexitemu\r' | dosemu -dumb

   or, when your dos-app does only normal printout (text), then you may
   even do this
       echo -e 'dir\rexitemu\r' | dosemu -dumb > FILE.out

   FILE.out then contains the output from the DOS application, but (unlike
   the unix -e technique, merged with all startup messages.

   You may elaborate this technique by writing a script, which gets the
   dos-command to execute from the commandline and generate 'FILE' for
   you.
     __________________________________________________________________

12.4. Running DOSEMU within a cron job

   When you try to use one of the above to start DOSEMU out of a crontab,
   then you have to asure, that the process has a proper environment set
   up ( especially the TERM and/or TERMCAP variable ).

   Normally cron would setup TERM=dumb, this is fine because DOSEMU
   recognizes it and internally sets it's own TERMCAP entry and TERM to
   `dosemu-none'. You may also configure your video to
          dosemu ... -dumb

   or have a TERM=none to force the same setting. In all other crontab run
   cases you may get nasty error messages either from DOSEMU or from
   Slang.
     __________________________________________________________________

13. Commands & Utilities

   These are some utitlies to assist you in using Dosemu.
     __________________________________________________________________

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
     __________________________________________________________________

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
     __________________________________________________________________

14. Keymaps

   This keymap is for using dosemu over telnet, and having *all* your keys
   work. This keymap is not complete. But hopefully with everyones help it
   will be someday :)

   There are a couple of things that are intentionally broken with this
   keymap, most noteably F11 and F12. This is because they are not working
   though slang correctly at the current instant. I have them mapped to
   "greyplus" and "greyminus". Also the scroll lock is mapped to shift-f3.
   This is because the scroll lock dosn't work right at all. Please feel
   free to send keymap patches in that fix anything but these.

   If you want to patch dosemu to fix either of those problems, i'd be
   glad to accept those :)

   to figure out how to edit this, read the keystroke-howto.

   as of 3/30/95, control/shift/alternate
   home/insert/delete/end/pageup/pagedown should work.

   Major issues will be:

   Do we move "alt-<fkey>" to "control-<fkey>" to switch virtual consoles?

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
     __________________________________________________________________

15. Networking using DOSEMU

15.1. Direct NIC access

   The easiest (and not recommended) way to set up the networking in
   DOSEMU is to use the direct NIC access. It means that DOSEMU will
   exclusively use one of your network interfaces, say eth1. No other
   processes will be able to use that interface. If they try to, the data
   exchange will became unreliable. So you have to make sure that this
   network interface is not used by anything including the kernel itself,
   before starting DOSEMU. The settings for this method are as follows:
    $_pktdriver = (on)
    $_ethdev = "eth1"
    $_vnet = "eth"

   Note that this method requires root privileges.

   As you can see, this simple method has many shortcomings. If you don't
   have the network card dedicated specially for dosemu, consider using
   more advanced method called "Virtual Networking".
     __________________________________________________________________

15.2. Virtual networking

   Virtual networking is a mechanism that allows to overcome all the
   limitations of the direct NIC access method, but it requires more work
   to set up everything properly. A special virtual network devices can be
   created using TUN/TAP interface. This will enable multiple dosemu
   sessions and the linux kernel to be on a separate virtual network. Each
   dosemu will have its own network device and ethernet address.

   First make sure that your Linux kernel comes with support for TUN/TAP;
   for details check Documentation/networking/tuntap.txt in the Linux
   kernel source. The user who runs DOSEMU should have read/write access
   to /dev/net/tun. Then either:

    1. Set

    $_pktdriver=(on)
    $_vnet = "tap"
    $_tapdev = ""

       Start DOSEMU as usual and configure the network device while DOSEMU
       is running (using ifconfig manually as root, a script, or
       usernetctl if your distribution supplies that), e.g.

    ifconfig tap0 up 192.168.74.1

       Configure the DOS TCP/IP network clients to have another IP address
       in the subnet you just configured. This address should be unique,
       i.e. no other dosemu, or the kernel, should have this address. For
       the example addresses given above, 192.168.74.2-192.168.74.254
       would be good. Your network should now be up and running and you
       can, for example, use a DOS SSH client to ssh to your own machine,
       but it will be down as soon as you exit DOSEMU.
    2. Or set

    $_pktdriver=(on)
    $_vnet = "tap"
    $_tapdev = "tap0"

       Obtain tunctl from the user mode linux project. Then set up a
       persistent TAP device using tunctl (use the -u owner option if you
       do that as root). Configure the network using ifconfig as above,
       but now before starting DOSEMU. Now start DOSEMU as often as you
       like and you can use the network in the same way as you did above.
       Note, however, that multiple DOSEMU sessions that run at the same
       time need to use multiple tapxx devices. $_netdev can be changed
       without editing dosemu.conf/~./dosemurc (if you leave it commented
       out there) by using [x]dosemu -I "netdev tap1".

   With the above you did set up a purely virtual internal network between
   the DOSEMU and the real Linux box. This is why in the above example
   192.168.74.1 should *not* be a real IP address of the Linux box, and
   the 192.168.74 network should not exist as a real network. To enable
   DOS programs to talk to the outside world you have to set up Ethernet
   bridging or IP routing.
     __________________________________________________________________

15.2.1. Bridging

   Bridging, using brctl (look for the bridge-utils package if you don't
   have it), is somewhat easier to accomplish than the IP routing. You set
   up a bridge, for example named "br0" and connect eth0 and tap0 to it.
   Suppose the Linux box has IP 192.168.1.10 on eth0, where 192.168.1.x
   can be a real LAN, and the uid of the user who is going to use DOSEMU
   is 500, then you can do (as root):
    brctl addbr br0
    ifconfig eth0 0.0.0.0 promisc up
    brctl addif br0 eth0
    ifconfig br0 192.168.1.10 netmask 255.255.255.0 up
    tunctl -u 500
    ifconfig tap0 0.0.0.0 promisc up
    brctl addif br0 tap0

   Now the DOSEMU's IP can be set to (for example) 192.168.1.11. It will
   appear in the same network your linux machine is.
     __________________________________________________________________

15.2.2. IP Routing

   If you like to use IP routing, note that unlike with bridging, each
   DOSEMU box will reside in a separate IP subnet, which consists only of
   2 nodes: DOSEMU itself and the corresponding TAP device on Linux side.
   You have to choose an IP address for that subnet. If your LAN has the
   address 192.168.1.0 and the netmask is 255.255.255.0, the dosemu subnet
   can have the address 192.168.74.0 and tap0 can have the address
   192.168.74.1:
       ifconfig tap0 192.168.74.1 netmask 255.255.255.0 up

   Choose a valid IP address from that subnet for DOSEMU box. It can be
   192.168.74.2. Configure your DOS client to use that IP. Configure your
   DOS client to use a gateway, which is the TAP device with IP
   192.168.74.1. Then you have to add the proper entry to the routing
   table on your Linux box:
       route add -net 192.168.74.0 netmask 255.255.255.0 dev tap0

   The resulting entry in the routing table will look like this:
    Destination   Gateway  Genmask         Flags Metric Ref    Use Iface
    192.168.74.0  *        255.255.255.0   U     0      0        0 tap0

   Then, unless the Linux box on which DOSEMU is running is a default
   gateway for the rest of you LAN, you will have to also add an entry to
   the routing table on each node of your LAN:
       route add -net 192.168.74.0 netmask 255.255.255.0 gw 192.168.1.10

   (192.168.1.10 is the IP of the box DOSEMU is running on). Also you have
   to check whether IP forwarding is enabled, and if not - enable it:
       echo 1 > /proc/sys/net/ipv4/ip_forward

   Now DOSEMU will be able to access any node of your LAN and vice versa.

   Yet one more thing have to be done if you want dosemu to be able to
   access Internet. Unlike in your LAN, you are not supposed to change the
   routing tables on an every Internet host, so how to make them to direct
   the IP packets back to dosemu's virtual network? To accomplish this,
   you only have to enable the IP Masquerading on the network interface
   that looks into Internet. If your machine serves as a gateway in a LAN,
   then the masquerading is most likely already enabled, and no further
   work is required. Otherwise you must add the following into your
   iptables configuration file (assuming the eth0 interface serves the
   Internet connection):
    *nat
    -A POSTROUTING -o eth0 -j MASQUERADE
    COMMIT

   After restarting iptables, you'll find your dosemu session being able
   to access Internet. For more info about the IP Masquerading have a look
   into IP-Masquerade-HOWTO.
     __________________________________________________________________

15.3. VDE networking backend

   If you just need to have a working internet access from within your
   DOSEMU environment, then you might want to consider using VDE
   networking. slirpvde is a user mode networking utility providing a
   NAT-like service, DHCP server, as well as a virtualized network in the
   10.0.2.0/24 range, with a default gateway and a DNS server. Note that
   dosemu uses VDE backend as a fallback when other methods are
   unavailable. It also tries to start all the vde utilities, so no
   configuration efforts are usually needed.

   Use the following command to get VDE sources:
       svn checkout svn://svn.code.sf.net/p/vde/svn/trunk vde-svn

   You may also need to apply the following patches: patch1 patch2 patch3
   patch4

   VDE-specific config options:
    $_pktdriver=(on)
    $_vnet = "vde"
    $_vdeswitch = "/tmp/switch1
    $_slirpargs = "--dhcp"

   Your wattcp.cfg file will look as simple as this:
       my_ip=dhcp
     __________________________________________________________________

16. Using Windows and Winsock

   This is the Windows Net Howto by Frisoni Gloriano <gfrisoni@hi-net.it>
   on 15 may 1997

   This document tries to describe how to run the Windows trumpet winsock
   over the dosemu built-in packet driver, and then run all TCP/IP
   winsock-based application (netscape, eudora, mirc, free agent .....) in
   windows environment.

   This is a very long step-by-step list of operation, but you can make
   little scripts to do all very quickly ;-)

   In this example, I use the dosnet based packet driver. It is very
   powerful because you can run a "Virtual net" between your dos-windows
   session and the linux, and run tcp/application application without a
   real (hardware) net.
     __________________________________________________________________

16.1. LIST OF REQUIRED SOFTWARE

     * The WINPKT.COM virtual packet driver, version 11.2 I have found
       this little tsr in the Crynwr packet driver distribution file
       PKTD11.ZIP
     * The Trumpet Winsock 2.0 revision B winsock driver for windows.
     __________________________________________________________________

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
       with the ip address 144.16.112.1 and add a route to this interface.
       This is a good example to make a "virtul network" from your
       dos/windows environment and the linux environment.

          ifconfig dsn0 144.16.112.1 broadcast 144.16.112.255 netmask 255.255.25
5.0
          route add -net 144.16.112.0 dsn0
     __________________________________________________________________

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
