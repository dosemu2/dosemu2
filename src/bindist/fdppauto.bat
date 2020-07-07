@echo off
rem autoexec.bat for DOSEMU2 + FDPP
path %DOSEMUDRV%:\dosemu;%SHELLDRV%:\;
if not "%FREEDOSDRV%" == "" path %PATH%%FREEDOSDRV%:\bin;%FREEDOSDRV%:\gnu;
if not "%USERDRV%" == "" set TEMP=%USERDRV%:\tmp
prompt $P$G
emusound /e
echo Welcome to dosemu2!
system -s DOSEMU_VERSION
echo     Build %DOSEMU_VERSION%
system -ep
if not "%DOSEMU_EXIT%" == "1" goto noexit
exitemu %ERRORLEVEL%
:noexit
if exist %USERDRV%:\userhook.bat %USERDRV%:\userhook.bat
