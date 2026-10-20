@echo off
rem Plays Our Last Echo online straight from the project (no packaging): sign in with Epic, then host or join.
rem See Scripts\play_online.ps1 for what it does and its options.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Scripts\play_online.ps1" %*
if errorlevel 1 pause
