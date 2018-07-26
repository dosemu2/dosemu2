@echo off
rem autoexec.bat for DOSEMU + FreeDOS
path e:\dosemu;f:\bin;f:\gnu
set HELPPATH=f:\help
set TEMP=c:\tmp
sound /e
prompt $P$G
system -s CDROM_PATH
if "%CDROM_PATH%" == "" goto nocdrom
lredir -nC \\linux\fs%CDROM_PATH%
:nocdrom
system -s DOSDRIVE_EXTRA
if "%DOSDRIVE_EXTRA%" == "" goto nodrived
lredir -n \\linux\fs%DOSDRIVE_EXTRA%
:nodrived
rem uncomment to load another bitmap font
rem lh display con=(vga,437,2)
rem mode con codepage prepare=((850) f:\cpi\ega.cpx)
rem mode con codepage select 850
rem chcp 850
echo Welcome to dosemu2!
system -s DOSEMU_VERSION
echo     Build %DOSEMU_VERSION%
system -ep
