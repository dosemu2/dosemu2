@echo off
rem autoexec.bat for DOSEMU2 + FDPP
path %DOSEMUDRV%:\dosemu;%SHELLDRV%:\;
if not "%FREEDOSDRV%" == "" path %PATH%%FREEDOSDRV%:\bin;%FREEDOSDRV%:\gnu;
if not "%USERDRV%" == "" set TEMP=%USERDRV%:\tmp
prompt $P$G
sound /e
system -s CDROM_PATH
if "%CDROM_PATH%" == "" goto nocdrom
lredir -nC \\linux\fs%CDROM_PATH%
:nocdrom
system -s DOSDRIVE_EXTRA
if "%DOSDRIVE_EXTRA%" == "" goto nodrived
lredir -n \\linux\fs%DOSDRIVE_EXTRA%
:nodrived
echo Welcome to dosemu2!
system -s DOSEMU_VERSION
echo     Build %DOSEMU_VERSION%
system -ep
if not "%DOSEMU_EXIT%" == "1" goto noexit
exitemu %ERRORLEVEL%
:noexit
if exist %USERDRV%:\userhook.bat %USERDRV%:\userhook.bat
