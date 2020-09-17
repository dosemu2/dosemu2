@echo off
rem autoexec.bat for DOSEMU + FreeDOS
path d:\dosemu;f:\bin;f:\gnu
set HELPPATH=f:\help
set TEMP=c:\tmp
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
call d:\bat\exechlp.bat -ep
