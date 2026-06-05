@echo off

REM Path to glslc.exe (ships with Vulkan SDK)
set "GLSLC=C:\env\VulkanSDK\1.4.321.1\Bin\glslc.exe"

REM Directory of this script
set "SCRIPT_DIR=%~dp0"

REM Output directory: one level up + shaderBin
set "OUT_DIR=%SCRIPT_DIR%..\shaderBin"

REM Create output directory if it doesn't exist
if not exist "%OUT_DIR%" (
    mkdir "%OUT_DIR%"
)

echo Compiling all .vert and .frag files in "%SCRIPT_DIR%"...
echo Output will go to "%OUT_DIR%"
echo.

for %%F in ("%SCRIPT_DIR%*.vert") do (
    echo Compiling %%~nxF...
    "%GLSLC%" "%%F" --target-env=vulkan1.3 --target-spv=spv1.4 -o "%OUT_DIR%\%%~nF_vert.spv"
)

for %%F in ("%SCRIPT_DIR%*.frag") do (
    echo Compiling %%~nxF...
    "%GLSLC%" "%%F" --target-env=vulkan1.3 --target-spv=spv1.4 -o "%OUT_DIR%\%%~nF_frag.spv"
)

echo.
echo Done.
pause
