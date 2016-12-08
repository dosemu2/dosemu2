@ECHO Off
PATH D:\OPENDOS;E:\DOSEMU;D:\
VERIFY OFF
PROMPT [OpenDOS] $P$G
SET OPENDOSCFG=D:\OPENDOS
CALL D:\EMUBIN\UXMACROS.BAT
system -s CDROM_PATH
if "%CDROM_PATH%" == "" goto nocdrom
lredir2 -nC linux\fs%CDROM_PATH%
:nocdrom
system -s DOSDRIVE_EXTRA
if "%DOSDRIVE_EXTRA%" == "" goto nodrived
lredir2 -n linux\fs%DOSDRIVE_EXTRA%
:nodrived