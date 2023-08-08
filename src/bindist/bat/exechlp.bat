@echo off
system %1 %2 %3
if not "%DOSEMU_SYS_DRV%" == "" %DOSEMU_SYS_DRV%:
if ERRORLEVEL 1 exitemu 1
if not "%DOSEMU_SYS_DIR%" == "" cd %DOSEMU_SYS_DIR%
if ERRORLEVEL 1 exitemu 1
if "%DOSEMU_SYS_CMD%" == "exit" goto cont
if "%DOSEMU_SYS_CMD%" == "" goto done
call %DOSEMU_SYS_CMD%
set SHELL_LOADHIGH_DEFAULT=
:cont
if "%DOSEMU_EXIT%" == "1" exitemu %ERRORLEVEL%
C:
:done
