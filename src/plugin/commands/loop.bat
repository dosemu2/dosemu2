rem time /q
rem before echo
@echo off
rem after echo
shift
echo at beginning %1 %2 %3 %4
for %%f in ( a b)  dO echo %COMSPEC% %f
rem pause
rem call x2
if errorlevel 1 goto ende
goto label2
echo soll nicht sein
:label1 echo here 1
goto ende
:label2
echo here 2
if a == a echo the compare was positive
rem if a == a goto end
goto marke1
:end
rem call x
echo at the end
rem time /q
