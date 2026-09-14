@echo off
setlocal
set "ROOT=%~dp0.."
if not exist "%ROOT%\.env" (
    echo [-] missing %ROOT%\.env - copy .env.example and fill it in
    exit /b 1
)
for /f "usebackq eol=# tokens=1,* delims==" %%A in ("%ROOT%\.env") do set "%%A=%%B"
if not defined FL_GAME_EXE (
    echo [-] FL_GAME_EXE missing in .env
    exit /b 1
)
if not defined FL_GHIDRA_PATH (
    echo [-] FL_GHIDRA_PATH missing in .env
    exit /b 1
)
if not defined FL_GHIDRA_MAXMEM set "FL_GHIDRA_MAXMEM=16G"
set "GHIDRA_HEADLESS_MAXMEM=%FL_GHIDRA_MAXMEM%"
rmdir /s /q "%ROOT%\ghidra_proj" 2>nul
mkdir "%ROOT%\ghidra_proj" 2>nul
call "%FL_GHIDRA_PATH%\support\analyzeHeadless.bat" "%ROOT%\ghidra_proj" FarlightAnalysis -import "%FL_GAME_EXE%" -preScript LightAnalysis.java -postScript FindCryptoVerdict.java -scriptPath "%~dp0ghidra_scripts" -analysisTimeoutPerFile 5400 > "%~dp0ghidra_analysis.log" 2>&1
