@echo off
rem autoexec.bat for DOSEMU + FreeDOS
path e:\dosemu;d:\bin;d:\gnu
set HELPPATH=d:\help
set TEMP=c:\tmp
sound /e
prompt $P$G
system -s CDROM_PATH
if "%CDROM_PATH%" == "" goto nocdrom
lredir2 -nC \\linux\fs%CDROM_PATH%
:nocdrom
system -s DOSDRIVE_EXTRA
if "%DOSDRIVE_EXTRA%" == "" goto nodrived
lredir2 -n \\linux\fs%DOSDRIVE_EXTRA%
:nodrived
rem uncomment to load another bitmap font
rem lh display con=(vga,437,2)
rem mode con codepage prepare=((850) d:\cpi\ega.cpx)
rem mode con codepage select 850
rem chcp 850
echo Welcome to dosemu2!
system -s DOSEMU_VERSION
echo     Build %DOSEMU_VERSION%
system -e
