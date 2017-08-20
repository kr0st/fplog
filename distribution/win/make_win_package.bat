@echo off

del ..\..\Stage\fplog_distro /F /Q
md ..\..\Stage\fplog_distro
md ..\..\Stage\fplog_distro\bin
md ..\..\Stage\fplog_distro\lib
md ..\..\Stage\fplog_distro\include
md ..\..\Stage\fplog_distro\include\libjson

copy ..\..\Stage\*.exe ..\..\Stage\fplog_distro\bin
copy ..\..\Stage\*.dll ..\..\Stage\fplog_distro\bin
copy ..\..\Stage\*.lua ..\..\Stage\fplog_distro\bin

copy ..\..\Stage\fplog.dll ..\..\Stage\fplog_distro\lib
copy ..\..\Stage\fplog.lib ..\..\Stage\fplog_distro\lib

copy ..\..\common\fplog_exceptions.h ..\..\Stage\fplog_distro\include
copy ..\..\common\fplog_transport.h ..\..\Stage\fplog_distro\include
copy ..\..\fplog\fplog.h ..\..\Stage\fplog_distro\include
xcopy /E /I /Y ..\..\libjson ..\..\Stage\fplog_distro\include\libjson

del ..\..\Stage\fplog_distro\include\libjson\Release /F /S /Q
for /f %%f in ('dir /ad /b ..\..\Stage\fplog_distro\include\libjson\Release\') do rd /s /q ..\..\Stage\fplog_distro\include\libjson\Release\%%f
rd /s /q ..\..\Stage\fplog_distro\include\libjson\Release

del ..\..\Stage\fplog_distro\include\libjson\Debug /F /S /Q
for /f %%f in ('dir /ad /b ..\..\Stage\fplog_distro\include\libjson\Debug\') do rd /s /q ..\..\Stage\fplog_distro\include\libjson\Debug\%%f
rd /s /q ..\..\Stage\fplog_distro\include\libjson\Debug

del ..\..\Stage\fplog_distro\include\libjson\x64 /F /S /Q
for /f %%f in ('dir /ad /b ..\..\Stage\fplog_distro\include\libjson\x64\') do rd /s /q ..\..\Stage\fplog_distro\include\libjson\x64\%%f
rd /s /q ..\..\Stage\fplog_distro\include\libjson\x64

del ..\..\Stage\fplog_distro\include\libjson\Win32 /F /S /Q
for /f %%f in ('dir /ad /b ..\..\Stage\fplog_distro\include\libjson\Win32\') do rd /s /q ..\..\Stage\fplog_distro\include\libjson\Win32\%%f
rd /s /q ..\..\Stage\fplog_distro\include\libjson\Win32

del ..\..\Stage\fplog_distro\include\libjson\libjson.xcodeproj /F /S /Q
for /f %%f in ('dir /ad /b ..\..\Stage\fplog_distro\include\libjson\libjson.xcodeproj\') do rd /s /q ..\..\Stage\fplog_distro\include\libjson\libjson.xcodeproj\%%f
rd /s /q ..\..\Stage\fplog_distro\include\libjson\libjson.xcodeproj

del ..\..\Stage\fplog_distro\include\libjson\libjson.vcx* /F /S /Q

call ..\..\autobuild\win\7za.exe a -r fplog_1.0-1.7z ..\..\Stage\fplog_distro\*.*
move /Y fplog_1.0-1.7z ..\..\Stage
