@ECHO Off
PATH C:\OPENDOS;E:\DOSEMU;C:\
VERIFY OFF
PROMPT [OpenDOS] $P$G
SET OPENDOSCFG=C:\OPENDOS
CALL C:\EMUBIN\UXMACROS.BAT
system -s CDROM_PATH
if "%CDROM_PATH%" == "" goto nocdrom
lredir2 -nC "linux\fs%CDROM_PATH%"
:nocdrom
system -s DOSDRIVE_EXTRA
if "%DOSDRIVE_EXTRA%" == "" goto nodrived
lredir2 -n "linux\fs%DOSDRIVE_EXTRA%"
:nodrived