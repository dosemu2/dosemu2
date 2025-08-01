@echo off
rem loads user shell like NC or DN
if exist %USERDRV%:\usershel.bat call %USERDRV%:\usershel.bat
