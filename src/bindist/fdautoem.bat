@echo off
rem autoexec.bat for DOSEMU + FreeDOS
path d:\dosemu
rem this is needed when booting from dosemu-freedos-bin
if exist f:\gnu\nul path %PATH%;f:\bin;f:\gnu
if exist c:\bin\nul path %PATH%;c:\bin
set HELPPATH=f:\help
if exist c:\help\nul set HELPPATH=c:\help
set TEMP=c:\tmp
if exist e:\tmp\nul set TEMP=e:\tmp
emusound -e
prompt $P$G
rem uncomment to load another bitmap font
rem lh display con=(vga,437,2)
rem mode con codepage prepare=((850) f:\cpi\ega.cpx)
rem mode con codepage select 850
rem chcp 850
echo Welcome to dosemu2!
system -s DOSEMU_VERSION
echo     Build %DOSEMU_VERSION%
call exechlp.bat -ep
