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
set "EXE_FILE=C:\Program Files (x86)\CommonDriver\recreate\AddMutilVirtualPrinter.exe"
echo *******************************Delete Printers Start********************************
if exist "%TARGET_FILE%" (  
    ECHO "%TARGET_FILE%"存在
) else ( 
	ECHO "%TARGET_FILE%"不存在
	call :func
	set /p result=<choice_result.txt
	:: 根据结果执行操作  
	if /i "!result!"=="Y" (  
		ECHO 继续执行...............  
	) else (  
		ECHO 用户选择了否
		del temp.vbs  
		del choice_result.txt
		exit
	)
	del temp.vbs  
	del choice_result.txt
)


if exist "!EXE_FILE!" (  
	ECHO "!EXE_FILE!"存在
) else (  
	ECHO "!EXE_FILE!"不存在,操作中断
	pause>nul
	exit	
)
if exist "!BAT_FILE!" (  
	ECHO "!BAT_FILE!"存在,打印机删除操作开始执行
	call "!BAT_FILE!" 
) else (  
	ECHO "!BAT_FILE!"删除批处理文件不存在
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

::用户能用打印机名称，如果用户只有一个打印机，那么只需要填写PRINTER1即可，删除其他的PRINTER2~5,如果有N个打印机可以使用，可以追加到PRINTERN，并填写相应的数据
::请注意：打印机名字一定要确保正确，不要在结尾多出空格
set PRINTER1=测试打印机
set PRINTER2=实验室A4打印机
set PRINTER3=TestPrinter
set PRINTER4=Printer_A4
set PRINTER5=实验室A3打印机

if exist "%TARGET_FILE%" (  
    del "%TARGET_FILE%"
)
type nul >"%TARGET_FILE%"

::括号内的值要确保和前面打印机的个数相同，前面PRINTER后面的数到几，括号内的值就要从1~几
if exist "%TARGET_FILE%" (  
    FOR %%i IN (1 2 3 4 5) DO (
		set LINE=!PRINTER%%i!=http://%IP_ADDRESS%:%PORT%/printers/!PRINTER%%i!
		echo !LINE!>> "%TARGET_FILE%"
	)
) else (
	ECHO "%TARGET_FILE%"创建失败
	pause>nul
	exit
)

echo *******************************Update Config End*********************************
echo *******************************Create Printers Start********************************
"!EXE_FILE!" 
echo *******************************Create Printers End*********************************
::pause>nul
exit
:func
set tempFile=temp.vbs
if exist "%tempFile%" (  
	del "%tempFile%"
)
type nul >"%tempFile%"
echo Set objShell = WScript.CreateObject("WScript.Shell")>> "%tempFile%"
echo intChoice = objShell.Popup("配置文件不存在,继续将会删除所有驱动的注册表。请问是否继续？", 0, "请注意", 4+32) >> "%tempFile%"
echo If intChoice = 6 Then >> "%tempFile%"
echo     WScript.Echo "Y">> "%tempFile%"
echo Else>> "%tempFile%"
echo    WScript.Echo "N" >> "%tempFile%"
echo End If >> "%tempFile%"
cscript //nologo temp.vbs > choice_result.txt
goto:eof
