
@echo off
REM ===== HLSL (.hlsl) -> SPIR-V via DXC =====
 
set "DXC=C:\env\VulkanSDK\1.4.321.1\Bin\dxc.exe"
set "SCRIPT_DIR=%~dp0"
set "OUT_DIR=%SCRIPT_DIR%..\shaderBin"
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
 
set "DXC_FLAGS=-spirv -fspv-target-env=vulkan1.3 -fvk-use-gl-layout -E main -Zi"
 
echo Compiling *_vert.hlsl (vertex stage)...
for %%F in ("%SCRIPT_DIR%*_vert.hlsl") do (
    echo   %%~nxF
    "%DXC%" %DXC_FLAGS% -T vs_6_7 "%%F" -Fo "%OUT_DIR%\%%~nF.spv"
)
 
echo Compiling *_frag.hlsl (pixel stage)...
for %%F in ("%SCRIPT_DIR%*_frag.hlsl") do (
    echo   %%~nxF
    "%DXC%" %DXC_FLAGS% -T ps_6_7 "%%F" -Fo "%OUT_DIR%\%%~nF.spv"
)

echo Compiling *_compute.hlsl (compute shaders)...
for %%F in ("%SCRIPT_DIR%*_comp.hlsl") do (
    echo   %%~nxF
    "%DXC%" %DXC_FLAGS% -T cs_6_7 "%%F" -Fo "%OUT_DIR%\%%~nF.spv"
)
 
echo.
echo Done.
pause