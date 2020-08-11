@echo off
rem generic autoexec.bat for DOSEMU + any DOS
rem must be edited in most circumstances
path c:\dos;d:\dosemu
set TEMP=c:\tmp
emusound /e
prompt $P$G
rem uncomment to load another bitmap font
rem mode con codepage prepare=((850) c:\dos\ega.cpi)
rem mode con codepage select 850
rem chcp 850
system -s DOSEMU_VERSION
echo "Welcome to dosemu2 %DOSEMU_VERSION%!"
call exechlp.bat -ep
