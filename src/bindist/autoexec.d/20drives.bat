@rem Set up drives specified with -cdrom and DOSDRIVE_EXTRA environment variable (see dosemu start-up script)
system -s CDROM_PATH
if "%CDROM_PATH%" == "" goto nocdrom
lredir2 -nC "\\linux\fs%CDROM_PATH%"
:nocdrom
system -s DOSDRIVE_EXTRA
if "%DOSDRIVE_EXTRA%" == "" goto nodrived
lredir2 -n "\\linux\fs%DOSDRIVE_EXTRA%"
:nodrived