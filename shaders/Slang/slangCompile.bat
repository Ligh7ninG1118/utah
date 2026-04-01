@echo off

REM Path to slangc.exe
set "SLANGC=C:\env\VulkanSDK\1.4.321.1\Bin\slangc.exe"

REM Directory of this script
set "SCRIPT_DIR=%~dp0"

REM Output directory: one level up + shaderBin
set "OUT_DIR=%SCRIPT_DIR%..\shaderBin"

REM Create output directory if it doesn't exist
if not exist "%OUT_DIR%" (
    mkdir "%OUT_DIR%"
)

echo Compiling all .slang files in "%SCRIPT_DIR%"...
echo Output will go to "%OUT_DIR%"
echo.

for %%F in ("%SCRIPT_DIR%*.slang") do (
    echo Compiling %%~nxF...
    "%SLANGC%" "%%F" ^
        -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ^
        -entry vertMain -entry fragMain ^
        -o "%OUT_DIR%\%%~nF.spv"
)

echo.
echo Done.
pause