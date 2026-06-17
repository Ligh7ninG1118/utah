@echo off
set "DXC=C:\env\VulkanSDK\1.4.321.1\Bin\dxc.exe"
set "SCRIPT_DIR=%~dp0"
set "OUT_DIRS=%SCRIPT_DIR%..\build\Debug\shaderBin %SCRIPT_DIR%..\shaderBin"

for %%D in (%OUT_DIRS%) do if not exist "%%D" mkdir "%%D"

set "DXC_FLAGS=-spirv -fspv-target-env=vulkan1.3 -fvk-use-gl-layout -E main -Zi"

echo Compiling *_vert.hlsl...
for %%F in ("%SCRIPT_DIR%*_vert.hlsl") do (
    echo   %%~nxF
    for %%D in (%OUT_DIRS%) do "%DXC%" %DXC_FLAGS% -T vs_6_7 "%%F" -Fo "%%D\%%~nF.spv"
)

echo Compiling *_frag.hlsl...
for %%F in ("%SCRIPT_DIR%*_frag.hlsl") do (
    echo   %%~nxF
    for %%D in (%OUT_DIRS%) do "%DXC%" %DXC_FLAGS% -T ps_6_7 "%%F" -Fo "%%D\%%~nF.spv"
)

echo.
echo Done.