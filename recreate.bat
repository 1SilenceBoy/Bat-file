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
    ECHO "%TARGET_FILE%"����
) else ( 
	ECHO "%TARGET_FILE%"������
	call :func
	set /p result=<choice_result.txt
	:: ���ݽ��ִ�в���  
	if /i "!result!"=="Y" (  
		ECHO ����ִ��...............  
	) else (  
		ECHO �û�ѡ���˷�
		del temp.vbs  
		del choice_result.txt
		exit
	)
	del temp.vbs  
	del choice_result.txt
)


if exist "!EXE_FILE!" (  
	ECHO "!EXE_FILE!"����
) else (  
	ECHO "!EXE_FILE!"������,�����ж�
	pause>nul
	exit	
)
if exist "!BAT_FILE!" (  
	ECHO "!BAT_FILE!"����,��ӡ��ɾ��������ʼִ��
	call "!BAT_FILE!" 
) else (  
	ECHO "!BAT_FILE!"ɾ���������ļ�������
	pause>nul
	exit  
)  

net stop spooler
net start spooler
echo *******************************Delete Printers End*********************************
echo *******************************Update Config Start*********************************

::����������Ҫ�ֶ��޸�

::CUPS��������IP��ַ
set IP_ADDRESS=192.168.50.188

::CUPS�������Ķ˿ں�
set PORT=631

::�û����ô�ӡ�����ƣ�����û�ֻ��һ����ӡ������ôֻ��Ҫ��дPRINTER1���ɣ�ɾ��������PRINTER2~5,�����N����ӡ������ʹ�ã�����׷�ӵ�PRINTERN������д��Ӧ������
::��ע�⣺��ӡ������һ��Ҫȷ����ȷ����Ҫ�ڽ�β����ո�
set PRINTER1=���Դ�ӡ��
set PRINTER2=ʵ����A4��ӡ��
set PRINTER3=TestPrinter
set PRINTER4=Printer_A4
set PRINTER5=ʵ����A3��ӡ��

if exist "%TARGET_FILE%" (  
    del "%TARGET_FILE%"
)
type nul >"%TARGET_FILE%"

::�����ڵ�ֵҪȷ����ǰ���ӡ���ĸ�����ͬ��ǰ��PRINTER������������������ڵ�ֵ��Ҫ��1~��
if exist "%TARGET_FILE%" (  
    FOR %%i IN (1 2 3 4 5) DO (
		set LINE=!PRINTER%%i!=http://%IP_ADDRESS%:%PORT%/printers/!PRINTER%%i!
		echo !LINE!>> "%TARGET_FILE%"
	)
) else (
	ECHO "%TARGET_FILE%"����ʧ��
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
echo intChoice = objShell.Popup("�����ļ�������,��������ɾ������������ע��������Ƿ������", 0, "��ע��", 4+32) >> "%tempFile%"
echo If intChoice = 6 Then >> "%tempFile%"
echo     WScript.Echo "Y">> "%tempFile%"
echo Else>> "%tempFile%"
echo    WScript.Echo "N" >> "%tempFile%"
echo End If >> "%tempFile%"
cscript //nologo temp.vbs > choice_result.txt
goto:eof
