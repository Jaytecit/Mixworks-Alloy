@echo off
setlocal
rem Read the versioned release folder selected by Alloy's local launcher.
for /f "tokens=2 delims=\" %%R in ('findstr /b /c:"start " "%~dp0..\out\Launch.cmd"') do set "STUDIO_RELEASE=%%R"
set "STUDIO_APP=%~dp0..\out\releases\%STUDIO_RELEASE%"
set "STUDIO_PY=%STUDIO_APP%\visual-python\pythonw.exe"
set "STUDIO_RUNTIME=%STUDIO_APP%\original-visualiser-runtime"
if not exist "%STUDIO_PY%" goto missing
if not exist "%STUDIO_RUNTIME%\studio_engine\studio.py" goto missing
start "" /belownormal "%STUDIO_PY%" -B "%STUDIO_RUNTIME%\studio_engine\studio.py" --projectm "%STUDIO_RUNTIME%\milkdrop" %*
exit /b %errorlevel%

:missing
echo Visual Studio could not find the active Alloy release.
pause
exit /b 1
