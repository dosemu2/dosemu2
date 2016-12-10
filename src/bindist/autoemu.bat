@echo off
set PATH=d:\dos;e:\dosemu
set TEMP=c:\tmp
sound /e
prompt $P$G
system -s CDROM_PATH
if "%CDROM_PATH%" == "" goto nocdrom
lredir2 -nC "linux\fs%CDROM_PATH%"
:nocdrom
system -s DOSDRIVE_EXTRA
if "%DOSDRIVE_EXTRA%" == "" goto nodrived
lredir2 -n "linux\fs%DOSDRIVE_EXTRA%"
:nodrived
mode con codepage prepare=((850) d:\dos\ega.cpi)
mode con codepage select=850
LH nlsfunc
chcp 850
echo Welcome to dosemu2!
system -s DOSEMU_VERSION
echo     Build %DOSEMU_VERSION%
system -e
