@echo off
set JSF="%TMP%\EC_URI.JS"
echo=WScript.echo(encodeURIComponent(WScript.Arguments(0)));>%JSF%
set/pSTR=ÇëÊäÈë×Ö·û´®
cscript //nologo %JSF% "%STR%"
pause

http://bbs.bathome.net/thread-6084-1-1.html




@echo off  
setlocal enabledelayedexpansion  
  
:: 设置 JScript 文件的路径  
set JSF="EC_URI.JS"  
  
:: 创建 JScript 文件  
echo WScript.echo(encodeURIComponent(WScript.Arguments(0)));>%JSF%  
  
:: 提示用户输入字符串  
set /p STR=请输入字符串:   
  
:: 使用 for /f 循环捕获 cscript 的输出  
for /f "delims=" %%i in ('cscript //nologo "%JSF%" "!STR!"') do set ENCODED=%%i  
  
:: 显示编码后的字符串  
echo 编码后的字符串是: !ENCODED!  
  
:: 清理临时文件  
del "%JSF%"  
  
pause
