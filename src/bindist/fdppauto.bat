@echo off
rem autoexec.bat for DOSEMU2 + FDPP
path %DOSEMUDRV%:\dosemu
if not "%SHELLDRV%" == "" path %PATH%;%SHELLDRV%:\
if not "%FREEDOSDRV%" == "" path %PATH%;%FREEDOSDRV%:\bin;%FREEDOSDRV%:\gnu
if not "%USERDRV%" == "" set TEMP=%USERDRV%:\tmp
for %%b in (%DOSEMUDRV%:\dosemu\dosrc.d\*.bat) do call %%b
if "%XBATDRV%" == "" goto noxbat
path %PATH%;%XBATDRV%:\
for %%b in (%XBATDRV%:\dosrc.d\*.bat) do call %%b
:noxbat
if exist %USERDRV%:\userhook.bat call %USERDRV%:\userhook.bat
