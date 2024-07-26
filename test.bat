@echo off
set JSF="%TMP%\EC_URI.JS"
echo=WScript.echo(encodeURIComponent(WScript.Arguments(0)));>%JSF%
set/pSTR=ÇëÊäÈë×Ö·û´®
cscript //nologo %JSF% "%STR%"
pause

http://bbs.bathome.net/thread-6084-1-1.html
