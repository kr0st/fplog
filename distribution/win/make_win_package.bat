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
