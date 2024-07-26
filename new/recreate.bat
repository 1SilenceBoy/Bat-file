@ECHO OFF
setlocal EnableDelayedExpansion
color 3e
title Recreate Printers By User
PUSHD %~DP0 & cd /d "%~dp0"
%1 %2
mshta vbscript:createobject("shell.application").shellexecute("%~s0","goto :runas","","runas",1)(window.close)&goto :eof
:runas

set TARGET_FILE=C:\PrintToCups\printInspect.conf
set "BAT_FILE=C:\Program Files (x86)\CommonDriver\recreate\delete.bat"
echo *******************************Delete Printers Start********************************
if exist !BAT_FILE! (  
	call !BAT_FILE! 
) else (  
	ECHO 删除批处理文件不存在
	pause>nul
	exit  
)  

net stop spooler
net start spooler
echo *******************************Delete Printers End*********************************
echo *******************************Update Config Start*********************************

::以下内容需要手动修改

::CUPS服务器的IP地址
set IP_ADDRESS=192.168.50.188

::CUPS服务器的端口号
set PORT=631

::用户能用的打印机名称 用户如果只有一个打印机能用，那就只写PRINTER1即可,其他要删掉,如果有N个打印机，那就记载PRINTER1~PRINTERN的数据  请注意：打印机名字一定要保证正确，结尾保证不要多出空格
set PRINTER1=测试打印机
set PRINTER2=实验室打印机
set PRINTER3=TestPrinter
set PRINTER4=Printer—OKAY
set PRINTER5=超贵打印机

if exist "%TARGET_FILE%" (  
    del "%TARGET_FILE%"
)
type nul >"%TARGET_FILE%"

::括号里的数值要和前面的打印机个数对上，前面PRINTER后面的数到几，括号里的数就要到几
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
