@echo off
setlocal ENABLEDELAYEDEXPANSION

if ["%~1"]==[""] ( set /p comp_to="Compile To: ") else ( set comp_to=%~1)

for /f %%f in ('dir /s/b *.vert *.frag *.comp *.conf *.tesc *.tese *.geom ') do (

set outdir=%%~dpf
call set outdir=%%outdir:%~dp0=%comp_to%\%%

if not exist !outdir! mkdir !outdir!

set output=%%f.bytes
call set output=%%output:%~dp0=%comp_to%\%%

%VULKAN_SDK%/Bin32/glslangValidator.exe -V %%f -o !output!
echo to !output!
)

if ["%~1"]==[""] (
pause
)