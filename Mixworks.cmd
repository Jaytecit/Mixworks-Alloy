@echo off
setlocal
if exist "%~dp0out\Launch.cmd" (
  call "%~dp0out\Launch.cmd"
) else if exist "%~dp0out\current\Mixworks.exe" (
  start "" "%~dp0out\current\Mixworks.exe"
) else if exist "%~dp0out\Mixworks.exe" (
  start "" "%~dp0out\Mixworks.exe"
) else (
  echo Build Mixworks first with scripts\build.ps1, then run this launcher again.
  pause
)
