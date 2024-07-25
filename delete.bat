@echo off  
setlocal enabledelayedexpansion 

for /f "tokens=1 delims==" %%i in (C:\PrintToCups\printInspect.conf) do ( 
    set "registryKey=HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Print\Environments\Windows x64\Drivers\Version-3\%%i"  
    reg query "!registryKey!" >nul 2>&1
    if !errorlevel! equ 0 (  
        reg delete "!registryKey!" /f  
        echo "!registryKey!" is Deleted!  
    ) else (  
        echo "!registryKey!" is not exist!  
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