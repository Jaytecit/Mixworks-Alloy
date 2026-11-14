@echo off
setlocal
set "VIEWER_PY=%~dp0..\.local\visual-studio-env\Scripts\python.exe"
if not exist "%VIEWER_PY%" (
  echo The Scene Viewer needs the local Visual Studio Python environment.
  echo See visual-studio\README.md for setup.
  pause
  exit /b 1
)
start "" /belownormal "%~dp0..\.local\visual-studio-env\Scripts\pythonw.exe" -B "%~dp0scene_viewer.py" %*
