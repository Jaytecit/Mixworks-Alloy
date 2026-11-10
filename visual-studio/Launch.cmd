@echo off
setlocal
set "STUDIO_PY=%~dp0..\.local\visual-studio-env\Scripts\pythonw.exe"
if not exist "%STUDIO_PY%" (
  echo Visual Studio needs its local Python environment. See visual-studio\README.md.
  pause
  exit /b 1
)
set "STUDIO_APP=%~dp0..\.local\visual-studio-releases\0.1.18"
if exist "%STUDIO_APP%\studio.py" (
  start "" /belownormal "%STUDIO_PY%" -B "%STUDIO_APP%\studio.py" --projectm "%STUDIO_APP%\projectm" %*
) else (
  start "" /belownormal "%STUDIO_PY%" -B "%~dp0studio.py" %*
)
