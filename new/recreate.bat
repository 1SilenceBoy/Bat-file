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
	echo ɾ���������ļ�������
    pause>nul  
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

::�û����õĴ�ӡ������ �û����ֻ��һ����ӡ�����ã��Ǿ�ֻдPRINTER1����,����Ҫɾ��,�����N����ӡ�����Ǿͼ���PRINTER1~PRINTERN������  ��ע�⣺��ӡ������һ��Ҫ��֤��ȷ����β��֤��Ҫ����ո�
set PRINTER1=���Դ�ӡ��
set PRINTER2=ʵ���Ҵ�ӡ��
set PRINTER3=TestPrinter
set PRINTER4=Printer��OKAY
set PRINTER5=�����ӡ��

if exist "%TARGET_FILE%" (  
    del "%TARGET_FILE%"
)
type nul >"%TARGET_FILE%"

::���������ֵҪ��ǰ��Ĵ�ӡ���������ϣ�ǰ��PRINTER������������������������Ҫ����
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