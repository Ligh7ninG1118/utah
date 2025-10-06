@echo off
setlocal EnableDelayedExpansion

REM Work relative to this script's directory
cd /d "%~dp0"

REM Use VULKAN_SDK env var
set "GLSLC=%VULKAN_SDK%\Bin\glslc.exe"

REM Create shaders/ if missing
if not exist "shaders" mkdir "shaders"

REM Compile all .vert / .frag, appending -vert / -frag to output name
for /f "delims=" %%F in ('dir /b *.vert *.frag 2^>nul') do (
  set "EXT=%%~xF"
  set "SUFFIX="
  if /I "!EXT!"==".vert" set "SUFFIX=-vert"
  if /I "!EXT!"==".frag" set "SUFFIX=-frag"
  "%GLSLC%" "%%F" -o "shaders\%%~nF!SUFFIX!.spv"
)

pause