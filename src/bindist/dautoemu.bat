@echo off
rem autoexec.bat for DOSEMU + DR/Novell DOS
rem dont set path to d:\dosemu as Novell command.com has bug
set TEMP=c:\tmp
rem emufs not needed on DR-DOS
emufs
emusound -e
prompt $P$G
system -s DOSEMU_VERSION
echo "Welcome to dosemu2 %DOSEMU_VERSION%!"
call d:\bat\exechlp.bat -ep
