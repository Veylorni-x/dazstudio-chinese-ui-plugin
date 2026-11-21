@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Check-Compatibility.ps1"
if errorlevel 1 pause
