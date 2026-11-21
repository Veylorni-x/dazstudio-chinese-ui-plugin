@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start.ps1" -English
if errorlevel 1 pause
