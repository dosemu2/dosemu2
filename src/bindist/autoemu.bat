@echo off
rem generic autoexec.bat for DOSEMU + any DOS
rem must be edited in most circumstances
d:\lredir z: linux\fs\${DOSEMU_LIB_DIR}/drive_z ro
path c:\dos;c:\windows\command;z:\dosemu2
set TEMP=c:\tmp
sound /e
prompt $P$G
unix -s DOSDRIVE_D
if "%DOSDRIVE_D%" == "" goto nodrived
lredir d: linux\fs%DOSDRIVE_D%
:nodrived
rem uncomment to load another bitmap font
rem mode con codepage prepare=((850) c:\dos\ega.cpi)
rem mode con codepage select 850
rem chcp 850
lredir e: linux\fs/media/cdrom c
unix -s DOSEMU_VERSION
echo "Welcome to dosemu %DOSEMU_VERSION%!"
system -e
