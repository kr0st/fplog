@echo off

md ..\build
rmdir ..\build\openssl-1.0.2n /S /Q

curl --cacert ./cacert.crt -o ../build/openssl.tar.gz https://www.openssl.org/source/openssl-1.0.2n.tar.gz
7za x -y -o"..\build\" ..\build\openssl.tar.gz
7za x -y -o"..\build\" ..\build\openssl.tar

cd ..\build\openssl-1.0.2n

call perl Configure VC-WIN64A
call ms\do_win64a.bat
call nmake -f ms\ntdll.mak
