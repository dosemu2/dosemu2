@echo off
system %1 %2 %3
if "%DOSEMU_SYS_CMD%" == "" goto done
if not "%DOSEMU_SYS_DRV%" == "" %DOSEMU_SYS_DRV%:
if ERRORLEVEL 1 exitemu 1
if not "%DOSEMU_SYS_DIR%" == "" cd %DOSEMU_SYS_DIR%
if ERRORLEVEL 1 exitemu 1
if "%DOSEMU_SYS_CMD%" == "exit" goto cont
set SHELL_LOADHIGH_DEFAULT=1
call %DOSEMU_SYS_CMD%
set SHELL_LOADHIGH_DEFAULT=
:cont
if "%DOSEMU_EXIT%" == "1" exitemu %ERRORLEVEL%
C:
:done
