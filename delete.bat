@echo off  
setlocal enabledelayedexpansion 

set "CONFIG_PATH=C:\PrintToCups\printInspect.conf"
if exist !CONFIG_PATH! (  
    for /f "tokens=1 delims==" %%i in (!CONFIG_PATH!) do ( 
		set "registryKey=HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Print\Environments\Windows x64\Drivers\Version-3\%%i"  
		reg query "!registryKey!" >nul 2>&1
		if !errorlevel! equ 0 (  
			reg delete "!registryKey!" /f  
			echo "!registryKey!" is Deleted!  
		) else (  
			echo "!registryKey!" is not exist!  
		)  
	)
) else (
    ECHO "!CONFIG_PATH!"配置文件不存在,将删除所有注册表
	set "registryKey=HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Print\Environments\Windows x64\Drivers\Version-3" 
	reg delete "!registryKey!" /va /f
	for /f "delims=" %%i in ('reg query "!registryKey!" /s')do (
	reg query "%%i" >nul 2>nul&&(
		reg delete "%%i"  /f
		reg delete "%%i" /va /f
	)
	)
)

for /f "skip=2 tokens=1,2* delims=," %%i in ('wmic path win32_printer get name /format:csv 2^>nul') do call :func %%j
echo.
::pause>nul
exit /b 0
goto :end
:func
set pn=%*
if "%pn:~0,2%"=="\\" (rundll32 printui.dll,PrintUIEntry /n"%pn%" /dn /q) else (rundll32 printui.dll,PrintUIEntry /n"%pn%" /dl /q)
goto :end
:end
