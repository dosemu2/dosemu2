@echo off
system %1 %2 %3
if "%DOSEMU_SYS_CMD%" == "" goto done
if not "%DOSEMU_SYS_DRV%" == "" %DOSEMU_SYS_DRV%:
if ERRORLEVEL 1 exitemu 1
if not "%DOSEMU_SYS_DIR%" == "" cd %DOSEMU_SYS_DIR%
if ERRORLEVEL 1 exitemu 1
if "%COMCOM_VER%" == "" goto not_comcom
set SHELL_CALL_DEFAULT=1
lh %DOSEMU_SYS_CMD%
set SHELL_CALL_DEFAULT=
goto cont
:not_comcom
call %DOSEMU_SYS_CMD%
:cont
if "%DOSEMU_EXIT%" == "1" exitemu %ERRORLEVEL%
C:
:done
