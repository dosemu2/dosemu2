@echo off
rem generic autoexec.bat for DOSEMU + any DOS
rem must be edited in most circumstances
path c:\dos;c:\windows\command;d:\dosemu
set TEMP=c:\tmp
emusound /e
prompt $P$G
system -s DOSDRIVE_EXTRA
if "%DOSDRIVE_EXTRA%" == "" goto nodrived
lredir -n \\linux\fs%DOSDRIVE_EXTRA%
:nodrived
rem uncomment to load another bitmap font
rem mode con codepage prepare=((850) c:\dos\ega.cpi)
rem mode con codepage select 850
rem chcp 850
system -s CDROM_PATH
if "%CDROM_PATH%" == "" goto nocdrom
lredir -nC \\linux\fs%CDROM_PATH%
:nocdrom
system -s DOSEMU_VERSION
echo "Welcome to dosemu2 %DOSEMU_VERSION%!"
system -ep
