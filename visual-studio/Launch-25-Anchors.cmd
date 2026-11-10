@echo off
setlocal
set "STUDIO_PY=%~dp0..\.local\visual-studio-env\Scripts\pythonw.exe"
set "STUDIO_APP=%~dp0..\.local\visual-studio-releases\0.1.12"
if not exist "%STUDIO_PY%" (
  echo Visual Studio needs its local Python environment. See visual-studio\README.md.
  pause
  exit /b 1
)
if not exist "%STUDIO_APP%\studio.py" (
  echo Visual Studio preview 0.1.12 is missing from the local releases folder.
  pause
  exit /b 1
)
start "" /belownormal "%STUDIO_PY%" -B "%STUDIO_APP%\studio.py" --projectm "%STUDIO_APP%\projectm" "%STUDIO_APP%\scenes\fault-ledger"
