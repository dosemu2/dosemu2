@echo off
rem autoexec.bat for DOSEMU + FreeDOS
path e:\dosemu;z:\bin;z:\gnu;z:\dosemu
set HELPPATH=z:\help
set TEMP=c:\tmp
sound /e
prompt $P$G
system -s DOSDRIVE_D
if "%DOSDRIVE_D%" == "" goto nodrived
lredir2 -d d: > nul
lredir2 d: linux\fs%DOSDRIVE_D%
:nodrived
rem uncomment to load another bitmap font
rem lh display con=(vga,437,2)
rem mode con codepage prepare=((850) z:\cpi\ega.cpx)
rem mode con codepage select 850
rem chcp 850
lredir2 -nC linux\fs/media/cdrom
system -s DOSEMU_VERSION
echo "Welcome to dosemu %DOSEMU_VERSION%!"
system -e
