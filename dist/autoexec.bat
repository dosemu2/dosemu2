@echo off
rem autoexec.bat for DOSEMU + FreeDOS
path z:\fdos\bin;z:\fdos\gnu;z:\
set HELPPATH=z:\fdos\help
set TEMP=c:\tmp
blaster
prompt $P$G
unix -s DOSDRIVE_D
if "%DOSDRIVE_D%" == "" goto nodrived
lredir d: linux\fs%DOSDRIVE_D%
:nodrived
rem uncomment to load another bitmap font
rem loadhi display con=(vga,437,2)
rem mode con codepage prepare=((850) c:\cpi\ega.cpx)
rem mode con codepage select 850
rem chcp 850
lredir e: linux\fs/media/cdrom c
unix -s DOSEMU_VERSION
echo "Welcome to dosemu %DOSEMU_VERSION%!"
unix -e
