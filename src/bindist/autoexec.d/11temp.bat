@rem Point %TEMP% to a valid directory
if exist c:\tmp\nul set TEMP=c:\tmp
if not %TEMP% == "" goto end
lredir2 t: linux\fs\tmp
set TEMP=t:\
:end