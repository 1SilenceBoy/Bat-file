@ECHO OFF
setlocal EnableDelayedExpansion
color 3e
title Recreate Printers By User
PUSHD %~DP0 & cd /d "%~dp0"
%1 %2
mshta vbscript:createobject("shell.application").shellexecute("%~s0","goto :runas","","runas",1)(window.close)&goto :eof
:runas

set TARGET_FILE=C:\PrintToCups\printInspect.conf
echo *******************************Delete Printers Start********************************
call "C:\Program Files (x86)\CommonDriver\recreate\delete.bat"
net stop spooler
net start spooler
echo *******************************Delete Printers End*********************************
echo *******************************Update Config Start********************************
::以下内容需要手动计入

::CUPS的ip地址
set IP_ADDRESS=192.168.50.188

::CUPS的端口号
set PORT=631

::用户能用的打印机名称 用户如果只有一个打印机能用，那就只写PRINTER1即可，如果有n个打印机，那就记载PRINTER1~	PRINTERN
set PRINTER1=test
set PRINTER2=testt
set PRINTER3=testtt
set PRINTER4=testttt
set PRINTER5=testttt

if exist "%TARGET_FILE%" (  
    del "%TARGET_FILE%"
)
type nul >"%TARGET_FILE%"

::括号里的数值要和前面记载的打印机名称个数要对上  前面PRINTER的后面的数是几，这里就要从1~几
FOR %%i IN (1 2 3 4 5) DO (
    set LINE=!PRINTER%%i!=http://%IP_ADDRESS%:%PORT%/printers/!PRINTER%%i!
    echo !LINE!>> "%TARGET_FILE%"
)
echo *******************************Update Config End*********************************
echo *******************************Create Printers Start********************************
"C:\Program Files (x86)\CommonDriver\recreate\AddMutilVirtualPrinter.exe"
echo *******************************Create Printers End*********************************
::pause>nul
exit