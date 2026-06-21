@echo off
set "HERE=%~dp0"
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d "%HERE%"
cl /nologo /EHsc /std:c++17 /O2 "%HERE%%~1" /Fe:"%HERE%_test.exe" /Fo:"%HERE%_test.obj" >"%HERE%_build.log" 2>&1
if errorlevel 1 ( type "%HERE%_build.log" & exit /b 1 )
"%HERE%_test.exe"
