@echo off

md ..\build
rmdir ..\build\openssl-1.0.2n /S /Q
rmdir ..\..\openssl\lib\x86 /S /Q

curl --cacert ./cacert.crt -o ../build/openssl.tar.gz https://www.openssl.org/source/openssl-1.0.2n.tar.gz
7za x -y -o"..\build\" ..\build\openssl.tar.gz
7za x -y -o"..\build\" ..\build\openssl.tar

cd ..\build\openssl-1.0.2n

mkdir "..\..\..\openssl\"
mkdir "..\..\..\openssl\include\"
mkdir "..\..\..\openssl\lib\"
mkdir "..\..\..\openssl\lib\x64\"
mkdir "..\..\..\openssl\lib\x86\"

call perl Configure VC-WIN32
call ms\do_nasm.bat
call nmake -f ms\ntdll.mak

xcopy out32dll\*.lib ..\..\..\openssl\lib\x86 /Y
xcopy out32dll\*.dll ..\..\..\openssl\lib\x86 /Y
