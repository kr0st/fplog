REM @echo off

cd ..
cd build

cd mongo-cxx-driver-legacy-1.1.2

md ..\..\..\mongo
md ..\..\..\mongo\include
md ..\..\..\mongo\lib
md ..\..\..\mongo\lib\x86
md ..\..\..\mongo\lib\x64

cd ..
cd ..
cd ..
cd openssl
ren lib lib_ex
cd ..
cd autobuild
cd build
cd mongo-cxx-driver-legacy-1.1.2

set REL_PATH=.\
set ABS_PATH=.\

rem // Save current directory and change to target directory
pushd %REL_PATH%

rem // Save value of CD variable (current directory)
set ABS_PATH=%CD%

rem // Restore original directory
popd

echo ABS_PATH=
echo %ABS_PATH%

mklink /D ..\..\..\openssl\lib lib_ex\x86

call scons install -j 8 --ssl --32 --extrapath=%ABS_PATH%\..\..\..\openssl\ --cpppath=%ABS_PATH%\..\..\..\boost\ --libpath=%ABS_PATH%\..\..\..\boost\stage\lib\x86
call scons install -j 8 --ssl --32 --dynamic-windows --sharedclient --extrapath=%ABS_PATH%\..\..\..\openssl\ --cpppath=%ABS_PATH%\..\..\..\boost\ --libpath=%ABS_PATH%\..\..\..\boost\stage\lib\x86
call scons install -j 8 --ssl --32 --dbg=on --dynamic-windows --sharedclient --extrapath=%ABS_PATH%\..\..\..\openssl\ --cpppath=%ABS_PATH%\..\..\..\boost\ --libpath=%ABS_PATH%\..\..\..\boost\stage\lib\x86

if errorlevel 1 (
echo ******* ERROR *******
echo unable to build mongo libraries, please inspect the build log for clues.
echo *********************
exit )

rmdir /Q ..\..\..\openssl\lib

cd build

xcopy /E /I /Y install\include ..\..\..\..\mongo\include
xcopy /E /I /Y install\lib ..\..\..\..\mongo\lib\x86

cd ..

mklink /D ..\..\..\openssl\lib lib_ex\x64

call scons install -j 8 --ssl --64 --extrapath=%ABS_PATH%\..\..\..\openssl\ --cpppath=%ABS_PATH%\..\..\..\boost\ --libpath=%ABS_PATH%\..\..\..\boost\stage\lib\x64
call scons install -j 8 --ssl --64 --dynamic-windows --sharedclient --extrapath=%ABS_PATH%\..\..\..\openssl\ --cpppath=%ABS_PATH%\..\..\..\boost\ --libpath=%ABS_PATH%\..\..\..\boost\stage\lib\x64
call scons install -j 8 --ssl --64 --dbg=on --dynamic-windows --sharedclient --extrapath=%ABS_PATH%\..\..\..\openssl\ --cpppath=%ABS_PATH%\..\..\..\boost\ --libpath=%ABS_PATH%\..\..\..\boost\stage\lib\x64

if errorlevel 1 (
echo ******* ERROR *******
echo unable to build mongo libraries, please inspect the build log for clues.
echo *********************
exit )

if errorlevel 0 (
echo *********************
echo Praise the sun!
echo ********************* )

rmdir /Q ..\..\..\openssl\lib

cd ..
cd ..
cd ..
cd openssl
ren lib_ex lib
cd ..
cd autobuild
cd build
cd mongo-cxx-driver-legacy-1.1.2

cd build

xcopy /E /I /Y install\lib ..\..\..\..\mongo\lib\x64

cd ..
