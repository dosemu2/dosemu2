@echo off
rem autoexec.bat for DOSEMU + FreeDOS
prompt $P$G
rem uncomment to load another bitmap font
rem lh display con=(vga,437,2)
rem mode con codepage prepare=((850) d:\cpi\ega.cpx)
rem mode con codepage select 850
rem chcp 850
if exist e:\autoexec.d\*.bat for %%B in (e:\autoexec.d\*.bat) do call %%B
