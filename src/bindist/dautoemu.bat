@echo off
rem autoexec.bat for DOSEMU + DR/Novell DOS
path c:\dos;c:\windows\command;d:\dosemu
set TEMP=c:\tmp
rem emufs not needed on DR-DOS
emufs
emusound /e
prompt $P$G
system -s DOSEMU_VERSION
echo "Welcome to dosemu2 %DOSEMU_VERSION%!"
system -ep
if "%DOSEMU_EXIT%" == "1" exitemu %ERRORLEVEL%
