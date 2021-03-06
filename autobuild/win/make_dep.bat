@echo off

call python --version 2> nul
if errorlevel 1 (
echo ******* ERROR *******
echo Please install Python 2.7.x, make sure it is added to the PATH variable and restart.
echo https://www.python.org/ftp/python/2.7.12/python-2.7.12.msi
echo *********************
exit )

call scons --version 2> nul
if errorlevel 1 (
echo ******* ERROR *******
echo Please install SCons 2.5.x
echo http://freefr.dl.sourceforge.net/project/scons/scons/2.5.0/scons-2.5.0-setup.exe
echo *********************
exit )

call cmake --version 2> nul
if errorlevel 1 (
echo ******* ERROR *******
echo Please install CMake
echo *********************
exit )

curl --cacert ./cacert.crt --output cacert.pem https://curl.haxx.se/ca/cacert.pem
if errorlevel 0 (
del cacert.crt
ren cacert.pem cacert.crt )

rmdir ..\build /S /Q
md ..\build

curl --cacert ./cacert.crt -o ../build/mongo_driver.zip https://codeload.github.com/mongodb/mongo-cxx-driver/zip/legacy-1.1.2
7za x -y -o"..\build\" ..\build\mongo_driver.zip

curl -o ../build/boost.7z http://netcologne.dl.sourceforge.net/project/boost/boost/1.63.0/boost_1_63_0.7z
7za x -y -o"..\build\" ..\build\boost.7z

curl --cacert ./cacert.crt -o ../build/gtest.zip https://codeload.github.com/google/googletest/zip/release-1.8.0
7za x -y -o"..\build\" ..\build\gtest.zip

cd ..
cd build

cd googletest-release-1.8.0
cd googletest

rmdir ..\..\..\..\gtest /S /Q

md ..\..\..\..\gtest
md ..\..\..\..\gtest\include
md ..\..\..\..\gtest\lib
md ..\..\..\..\gtest\lib\x64
md ..\..\..\..\gtest\lib\x86

cmake . -Dgtest_force_shared_crt=ON
"C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild" gtest.sln /p:Configuration=Debug /p:Platform=Win32 /t:Rebuild
"C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild" gtest.sln /p:Configuration=Release /p:Platform=Win32 /t:Rebuild

if errorlevel 1 (
echo ****************************************** ERROR ******************************************
echo unable to build gtest libraries, please inspect console output for clues.
echo *******************************************************************************************
exit )

echo f | xcopy /Y Debug\gtest.lib ..\..\..\..\gtest\lib\x86\gtestd.lib
echo f | xcopy /Y Debug\gtest_main.lib ..\..\..\..\gtest\lib\x86\gtest_maind.lib
xcopy /Y Release\gtest.lib ..\..\..\..\gtest\lib\x86
xcopy /Y Release\gtest_main.lib ..\..\..\..\gtest\lib\x86

xcopy /E /I /Y include ..\..\..\..\gtest\include

del CMakeCache.txt
rmdir CMakeFiles /S /Q

cmake . -G"Visual Studio 14 Win64" -Dgtest_force_shared_crt=ON
"C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild" gtest.sln /p:Configuration=Debug /p:Platform=x64 /t:Rebuild
"C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild" gtest.sln /p:Configuration=Release /p:Platform=x64 /t:Rebuild

if errorlevel 1 (
echo ****************************************** ERROR ******************************************
echo unable to build gtest libraries, please inspect console output for clues.
echo *******************************************************************************************
exit )

echo f | xcopy /Y Debug\gtest.lib ..\..\..\..\gtest\lib\x64\gtestd.lib
echo f | xcopy /Y Debug\gtest_main.lib ..\..\..\..\gtest\lib\x64\gtest_maind.lib
xcopy /Y Release\gtest.lib ..\..\..\..\gtest\lib\x64
xcopy /Y Release\gtest_main.lib ..\..\..\..\gtest\lib\x64

cd ..
cd ..

cd boost_1_63_0

call bootstrap.bat

if errorlevel 1 (
echo ******* ERROR *******
echo unable to build boost libraries, please inspect the bootsrap.bat output for clues.
echo *********************
exit )

call b2 -j8 --without-python --build-type=complete --toolset=msvc architecture=x86 address-model=32

if errorlevel 1 (
echo ******* ERROR *******
echo unable to build boost libraries, please inspect the build log for clues.
echo *********************
exit )

md ..\..\..\boost
md ..\..\..\boost\boost
md ..\..\..\boost\stage
md ..\..\..\boost\stage\lib
md ..\..\..\boost\stage\lib\x86
md ..\..\..\boost\stage\lib\x64

xcopy /E /I /Y boost ..\..\..\boost\boost\
xcopy /E /I /Y stage\lib ..\..\..\boost\stage\lib\x86

cd ..
rmdir boost_1_63_0 /S /Q
cd ..
cd win
7za x -y -o"..\build\" ..\build\boost.7z
cd ..
cd build 
cd boost_1_63_0

call bootstrap.bat
call b2 -j8 --without-python --build-type=complete --toolset=msvc architecture=x86 address-model=64

xcopy /E /I /Y stage\lib ..\..\..\boost\stage\lib\x64

cd ..

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
cd ..
cd ..
rmdir build /S /Q
cd win
