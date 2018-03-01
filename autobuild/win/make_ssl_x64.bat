@echo off

md ..\build
rmdir ..\build\openssl-1.0.2n /S /Q
rmdir ..\..\openssl\lib\x64 /S /Q

curl --cacert ./cacert.crt -o ../build/openssl.tar.gz https://www.openssl.org/source/openssl-1.0.2n.tar.gz
7za x -y -o"..\build\" ..\build\openssl.tar.gz
7za x -y -o"..\build\" ..\build\openssl.tar

cd ..\build\openssl-1.0.2n

call perl Configure VC-WIN64A
call ms\do_win64a.bat
call nmake -f ms\ntdll.mak

mkdir "..\..\..\openssl\"
mkdir "..\..\..\openssl\include\"
mkdir "..\..\..\openssl\lib\"
mkdir "..\..\..\openssl\lib\x64\"
mkdir "..\..\..\openssl\lib\x86\"

xcopy out32dll\*.lib ..\..\..\openssl\lib\x64 /Y
xcopy out32dll\*.dll ..\..\..\openssl\lib\x64 /Y
xcopy inc32 ..\..\..\openssl\include /E /Y
