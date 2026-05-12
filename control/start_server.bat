@echo off
setlocal

cd /d "%~dp0"
echo Starting ModuBotic Control...
echo.
echo PC browser:
echo   http://localhost:8080
echo.
echo UNIHIKER K10 / network devices:
echo   http://10.0.0.100:8080
echo.
echo Press Ctrl+C in this window to stop the server.
echo.

python server.py

pause
