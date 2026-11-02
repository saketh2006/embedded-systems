@echo off
cd /d "%~dp0"
echo =========================================================================
echo   STM32F401CCU6 Smart Access Control - Research Laboratory Terminal
echo =========================================================================
echo Initializing local instrumentation server on port 8085...
echo Opening laboratory interface in browser...
echo.
python server.py
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Python was not found in PATH or failed to start.
    echo Opening interface directly in default browser...
    start "" "index.html"
)
pause
