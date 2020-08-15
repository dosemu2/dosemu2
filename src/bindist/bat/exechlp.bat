@echo off
system %1 %2 %3
if "%DOSEMU_SYS_CMD%" == "" goto done
if not "%DOSEMU_SYS_DRV%" == "" %DOSEMU_SYS_DRV%:
if ERRORLEVEL 1 exitemu 1
if not "%DOSEMU_SYS_DIR%" == "" cd %DOSEMU_SYS_DIR%
if ERRORLEVEL 1 exitemu 1
%COMSPEC% /E:1024 /C %DOSEMU_SYS_CMD%
if "%DOSEMU_EXIT%" == "1" exitemu %ERRORLEVEL%
C:
:done
