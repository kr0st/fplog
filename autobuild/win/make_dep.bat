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

curl --cacert ./cacert.crt --output cacert.pem https://curl.haxx.se/ca/cacert.pem
if errorlevel 0 (
del cacert.crt
ren cacert.pem cacert.crt )

del ..\build /F /Q
md ..\build

curl --cacert ./cacert.crt -o ../build/mongo_driver.zip https://codeload.github.com/mongodb/mongo-cxx-driver/zip/legacy-1.1.2
7za x -y -o"..\build\" ..\build\mongo_driver.zip

curl -o ../build/boost.7z http://netcologne.dl.sourceforge.net/project/boost/boost/1.61.0/boost_1_61_0.7z
7za x -y -o"..\build\" ..\build\boost.7z

cd ..
cd build
cd boost_1_61_0

call bootstrap.bat

if errorlevel 1 (
echo ******* ERROR *******
echo unable to build boost libraries, please inspect the bootsrap.bat output for clues.
echo *********************
exit )

call b2 -j8 --build-type=complete --toolset=msvc architecture=x86 address-model=32

if errorlevel 1 (
echo ******* ERROR *******
echo unable to build boost libraries, please inspect the build log for clues.
echo *********************
exit )

md ..\..\..\boost
md ..\..\..\boost\boost
md ..\..\..\boost\stage

xcopy /E /I /Y boost ..\..\..\boost\boost\
xcopy /E /I /Y stage ..\..\..\boost\stage\

cd ..
cd mongo-cxx-driver-legacy-1.1.2

set REL_PATH=.\
set ABS_PATH=

rem // Save current directory and change to target directory
pushd %REL_PATH%

rem // Save value of CD variable (current directory)
set ABS_PATH=%CD%

rem // Restore original directory
popd

call scons install --32 --cpppath=%ABS_PATH%\..\..\..\boost\ --libpath=%ABS_PATH%\..\..\..\boost\stage\lib
call scons install --32 --dbg=on --cpppath=%ABS_PATH%\..\..\..\boost\ --libpath=%ABS_PATH%\..\..\..\boost\stage\lib
call scons install --32 --dynamic-windows --sharedclient --cpppath=%ABS_PATH%\..\..\..\boost\ --libpath=%ABS_PATH%\..\..\..\boost\stage\lib
call scons install --32 --dynamic-windows --sharedclient --dbg=on --cpppath=%ABS_PATH%\..\..\..\boost\ --libpath=%ABS_PATH%\..\..\..\boost\stage\lib

cd build

xcopy /E /I /Y install ..\..\..\..\mongo
