@echo off
rem autoexec.bat for DOSEMU2 + FDPP
path %DOSEMUDRV%:\dosemu
if not "%SHELLDRV%" == "" path %PATH%;%SHELLDRV%:\
if not "%FREEDOSDRV%" == "" path %PATH%;%FREEDOSDRV%:\bin;%FREEDOSDRV%:\gnu
if not "%USERDRV%" == "" set TEMP=%USERDRV%:\tmp
prompt $P$G
for %%b in (%DOSEMUDRV%:\dosemu\dosrc.d\*.bat) do call %%b
if exist %USERDRV%:\userhook.bat call %USERDRV%:\userhook.bat
