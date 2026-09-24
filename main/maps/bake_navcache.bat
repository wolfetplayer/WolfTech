@echo off
setlocal enabledelayedexpansion

set "SCRIPTDIR=%~dp0"
set "OUTDIR=%SCRIPTDIR%..\..\Main\maps\nav"

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

set FOUND=0
for %%F in ("%SCRIPTDIR%*.bsp") do (
	set FOUND=1
	echo.
	echo === Baking %%~nxF ===
	"%SCRIPTDIR%navgen.x64.exe" "%%F" "%OUTDIR%"
)

if "%FOUND%"=="0" (
	echo No .bsp files found next to this .bat - copy one in first.
)

echo.
echo Done. Output: %OUTDIR%
pause
