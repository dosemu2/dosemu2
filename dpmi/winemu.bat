@echo off
set windir=C:\WINDOWS
set windrv=%WINDIR%
REM The device 'NUL' always exists, so it is a good directory-existence check.
if not exist %WINDIR%\NUL goto NOWINDIR
if not exist %WINDIR%\SYSTEM\OS2K386.EXE goto NOWINOS2

:BOOTWIN
echo Warning!  This may crash your system!  This is your chance to hit Ctrl-C now!
echo If Windows fails to start, please read the %WINDIR%\BOOTLOG.TXT file.
pause
echo Starting Windows...
c:
cd %WINDIR%
if exist BOOTLOG.OLD del BOOTLOG.OLD
if exist BOOTLOG.TXT ren BOOTLOG.TXT BOOTLOG.OLD
%WINDIR%\SYSTEM\OS2K386.EXE /B %1 %2 %3 %4 %5
goto END

:NOWINDIR
echo You don't have a %WINDIR% directory!  
echo Please edit WIN.BAT
goto END

:NOWINOS2
echo I am unable to find %WINDIR%\SYSTEM\OS2K386.EXE
echo Please install WINOS2 in the windows system dir.
goto END

:END
