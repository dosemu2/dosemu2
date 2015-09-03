@echo off
::
:: mount.bat - for compatiblity with dosbox, calls out to lredir
::
::
:: Uses these environment variables internally:
::
:: __DE__FULLPATH__
:: __DE_LREDIR_PARAMS__
::
:: For full options of dosbox mount see  http://www.dosbox.com/wiki/MOUNT
:: Note - most options are not supported

if "%1"=="/?" goto usage
if "%1"=="-h" goto usage
if "%1"=="-H" goto usage
if "%1"=="/h" goto usage
if "%1"=="/H" goto usage

if "%1"=="-u" goto unmount
if "%1"=="-U" goto unmount
if "%1"=="-u" goto unmount

if "%1"=="-cd" goto list_cdroms

goto mount


:list_cdroms
echo "Listing cdroms is currently unsupported."
goto end


:mount
if "%1"=="" goto usage
if "%2"=="" goto usage

:: expand path, to support paths like ~/dosprogs
SET /e __DE_FULLPATH__=UNIX realpath ~
SET __DE_LREDIR_PARAMS__=%1: LINUX\FS%__DE_FULLPATH__%

:: check for -t cdrom|floppy
if "%3"=="" goto mount_do_lredir
if "%3"=="-t" if "%4"=="cdrom" goto mount_cdrom
if "%3"=="-t" if "%4"=="floppy" goto mount_floppy
goto mount_do_lredir


:mount_floppy   
echo Mounting floppy is not currently supported.
goto mount_end


:mount_cdrom
::  mount d /media/cdrom -t cdrom
:: lredir d LINUX\FS/media/cdrom C
SET __DE_LREDIR_PARAMS__=%__DE_LREDIR_PARAMS__% C
echo %__DE_LREDIR_PARAMS__%
if "%5"=="" goto mount_do_lredir

:: mount and lredir and  can specify which cdrom, but are 0 and 1 based respectively
echo "Parameters to cd mount are not supported"
goto mount_end


:mount_do_lredir
:: - note - LREDIR output isn't e same as MOUNT:
LREDIR %__DE_LREDIR_PARAMS__%


:mount_end
if ""=="%__DE_FULLPATH__%" SET __DE_FULLPATH__ > NUL
if ""=="%__DE_LREDIR_PARAMS__%" SET __DE_LREDIR_PARAMS__ > NUL
goto end


:unmount
shift
if "%1"=="" goto usage

:: - note - LREDIR output isn't e same as MOUNT:
LREDIR DEL "%1:"

goto end


:usage
echo DOSEMU MOUNT
echo.
echo Provided for compatibility with DOSBOX does not support
echo all options documented at http://www.dosbox.com/wiki/MOUNT
echo.
echo Usage: MOUNT [[-u] DRIVE DIRECTORY [-t cdrom]]
echo.
echo MOUNT q ~/dosprogs
echo   Mount directory ~/dosprogs to drive Q:
echo.
echo MOUNT -u Q
echo   Unmount drive Q:
echo.
echo MOUNT d /media/cdrom -t cdrom
echo   Mount cdrom at /media as cdrom on drive D:
echo.


:end
