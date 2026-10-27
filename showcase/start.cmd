@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "TARGET=%~dp0index.html"
if not exist "%TARGET%" (
  echo [showcase] index.html not found.
  echo [showcase] Build it first: uv run --python 3.12 showcase/build.py
  pause
  exit /b 1
)

set "EDGE=%ProgramFiles(x86)%\Microsoft\Edge\Application\msedge.exe"
if not exist "%EDGE%" set "EDGE=%ProgramFiles%\Microsoft\Edge\Application\msedge.exe"
if not exist "%EDGE%" (
  echo [showcase] Microsoft Edge not found in Program Files.
  pause
  exit /b 1
)

set "PROFILE=%~dp0edge-profile"
set "URI=file:///%TARGET:\=/%"

echo [showcase] Edge kiosk: %URI%
start "" "%EDGE%" --kiosk "%URI%" ^
  --autoplay-policy=no-user-gesture-required ^
  --user-data-dir="%PROFILE%" ^
  --no-first-run ^
  --disable-features=msEdgeFirstRunExperience

endlocal
