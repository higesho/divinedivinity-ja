@echo off
rem 引数なし: 配布版 ..\build\version.dll ／ build.bat test: 試験版 ..\build\test\version.dll（配布しない）
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d "%~dp0"
set O=..\build\release
set OUT=..\build\version.dll
set DEF=
set EXTRA=
if "%1"=="test" (
  set O=..\build\test
  set OUT=..\build\test\version.dll
  set DEF=/DDDJP_TEST
  set EXTRA=testio.c
)
if not exist %O% mkdir %O%
rc /nologo /fo %O%\ddjp.res ddjp.rc || exit /b 1
cl /nologo /utf-8 /O2 /MT /W3 /LD /D_CRT_SECURE_NO_WARNINGS %DEF% /Fo%O%\ proxy.c ddjp.c font.c text.c %EXTRA% %O%\ddjp.res /Fe%OUT% /link /DEF:version.def kernel32.lib user32.lib gdi32.lib || exit /b 1
