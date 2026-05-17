@echo off
:: mingw32-make.exe clean
mingw32-make.exe
copy libcurl.dll %cd%\out\libcurl.dll /y
out\market.exe
pause