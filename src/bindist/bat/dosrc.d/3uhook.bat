@echo off
rem loads user-provided TSRs
if exist %USERDRV%:\userhook.bat call %USERDRV%:\userhook.bat
