@echo off
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\push_itch.ps1" %*
set "PUSH_EXIT_CODE=%ERRORLEVEL%"
if not "%PUSH_EXIT_CODE%"=="0" echo itch.io upload failed.
pause
exit /b %PUSH_EXIT_CODE%
