@echo off

call ..\..\autobuild\win\7za.exe a -r fplog_diagnostics.7z ..\..\Stage\*.txt
call ..\..\autobuild\win\7za.exe a -r fplog_diagnostics.7z ..\..\Stage\*.xml
call ..\..\autobuild\win\7za.exe a -r fplog_diagnostics.7z ..\..\Stage\*.log

move /Y fplog_diagnostics.7z ..\..\Stage

