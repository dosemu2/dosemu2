
From lermen@elserv.ffm.fgan.de Fri Jan  9 02:02:28 1998
Date: Sun, 4 Jan 1998 16:30:24 +0100 (MET)
From: Hans Lermen <lermen@elserv.ffm.fgan.de>
To: Marty Leisner <leisner@sdsp.mc.xerox.com>
Cc: Pat Villani <patv@iop.com>, dosemu developers <dosemu-devel@suse.com>
Subject: Re: Ideas for debugging 

On Sat, 3 Jan 1998, Marty Leisner wrote:

> 
> In order to run gdb under dosemu:
> 	attach (compile with the -g option)
> 	do
> 		handle SIGSEGV nostop noprint
> 
> then you're fine

Yup, that's the trick ;-)
However, to 'compile with -g' one has to do:

    make pristine
    ./default-configure --enable-debug
    make

> (but its very difficult to debug programs under
> dosemu).

its nearly impossible, for that we have 'dosdebug', the bultin debugger.

> 
> Several years we discussed this...has any work been done essentially
> having dosemu act as a gdbserver, which can talk to gdb...??

hmm, can gdb handle 16 bit code or even segmented code?

Hans
<lermen@fgan.de>

Bart:
It can handle 16 bit code but you have to handle the segmentation yourself,
e.g. use
  set architecture i8086
  x/20i 0x9089*16+0x18e
to dump 9089:019e

Another issue is that whilst single stepping the SIGALRM handler may
disturb. You can avoid that using hooks, as below, and paste everything
into a .gdbinit file.

define hook-stop
     handle SIGALRM nopass
end
     
define hook-run
     handle SIGALRM pass
end
     
define hook-continue
     handle SIGLARM pass
end

handle SIGSEGV nostop noprint
