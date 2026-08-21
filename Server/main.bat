@echo off
title OneNET Bridge - STM32 ↔ OneNET
cd /d "%~dp0"
echo ======================================
echo   Optional FastAPI / AI service
echo   Main remote-control path now uses OneNET directly
echo ======================================
python main.py
pause
