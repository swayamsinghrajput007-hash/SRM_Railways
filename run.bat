@echo off
 echo Compiling FINAL CLEAN...
 g++ -std=c++17 -O2 main_simple.cpp -o railway_server.exe -lws2_32 -lwsock32
 if %ERRORLEVEL% EQU 0 ( echo OK - Starting... & railway_server.exe ) else ( pause )
