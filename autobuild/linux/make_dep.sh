#!/bin/bash

python --version 2> /dev/null
if [ $? -ne 0 ]; then
echo "************** ERROR **************"
echo "Please install Python 2.7.x"
echo "***********************************"
exit
fi

python3 --version 2> /dev/null
if [ $? -ne 0 ]; then
echo "**************************** ERROR ****************************"
echo "Please install Python 3.4.x with dev packages"
echo "***************************************************************"
exit
fi

scons --version > /dev/null
if [ $? -ne 0 ]; then
echo "************** ERROR **************"
echo "Please install SCons 2.5.x"
echo "***********************************"
exit
fi

curl --version > /dev/null
if [ $? -ne 0 ]; then
echo "************** ERROR **************"
echo "Please install curl"
echo "***********************************"
exit
fi

7za > /dev/null
if [ $? -ne 0 ]; then
echo "********************* ERROR *********************"
echo "Please install 7zip (p7zip-full package)"
echo "*************************************************"
exit
fi

cmake --version 2> /dev/null
if [ $? -ne 0 ]; then
echo "************** ERROR **************"
echo "Please install CMake"
echo "***********************************"
exit
fi

curl --cacert ./cacert.crt --output cacert.pem https://curl.haxx.se/ca/cacert.pem
if [ $? -eq 0 ]; then
rm cacert.crt
mv cacert.pem cacert.crt
fi

rm -rf ../build
mkdir ../build

curl --cacert ./cacert.crt -o ../build/mongo_driver.zip https://codeload.github.com/mongodb/mongo-cxx-driver/zip/legacy-1.1.2
curl -o ../build/boost.7z http://netcologne.dl.sourceforge.net/project/boost/boost/1.63.0/boost_1_63_0.7z
curl --cacert ./cacert.crt -o ../build/gtest.zip https://codeload.github.com/google/googletest/zip/release-1.8.0

7za x -y -o"../build/" ../build/gtest.zip
7za x -y -o"../build/" ../build/mongo_driver.zip
7za x -y -o"../build/" ../build/boost.7z

cd ..
cd build

cd googletest-release-1.8.0
cd googletest

rm -rf ../../../../gtest

mkdir ../../../../gtest
mkdir ../../../../gtest/include
mkdir ../../../../gtest/lib
mkdir ../../../../gtest/lib/x64

cmake ./
make

if [ $? -ne 0 ]; then
echo "****************************************** ERROR ******************************************"
echo "unable to build gtest libraries, please inspect console output for clues."
echo "*******************************************************************************************"
exit
fi

cp -rf ./libgtest.a ../../../../gtest/lib/x64
cp -rf ./libgtest_main.a ../../../../gtest/lib/x64
cp -rf ./include ../../../../gtest/

cd ..
cd ..

cd boost_1_63_0

./bootstrap.sh

if [ $? -ne 0 ]; then
echo "****************************************** ERROR ******************************************"
echo "unable to build boost libraries, please inspect the bootsrap.sh output for clues."
echo "*******************************************************************************************"
exit
fi

rm ./project-config.jam
cp ../../linux/project-config.jam ./

./b2 -j4 --variant=release --layout=tagged --toolset=gcc architecture=x86 address-model=32 cxxflags="-D_GLIBCXX_USE_CXX11_ABI=0"

if [ $? -ne 0 ]; then
echo "****************************************** ERROR ******************************************"
echo "unable to build boost libraries, please inspect the build log for clues."
echo "*******************************************************************************************"
exit
fi

./b2 -j4 --with-python --variant=release --layout=tagged --toolset=gcc architecture=x86 address-model=32 cxxflags="D_GLIBCXX_USE_CXX11_ABI=0"

if [ $? -ne 0 ]; then
echo "****************************************** ERROR ******************************************"
echo "unable to build boost libraries, please inspect the build log for clues."
echo "*******************************************************************************************"
exit
fi

rm -rf ../../../boost 

mkdir ../../../boost
mkdir ../../../boost/boost
mkdir ../../../boost/stage
mkdir ../../../boost/stage/lib

cp -rf ./boost ../../../boost

cp -rf ./stage/lib ../../../boost/stage/lib
mv ../../../boost/stage/lib/lib ../../../boost/stage/lib/x86

rm -rf ./stage
rm -rf ./bin.v2

./b2 -j4 --variant=release --layout=tagged --toolset=gcc address-model=64 cxxflags="-D_GLIBCXX_USE_CXX11_ABI=0"

if [ $? -ne 0 ]; then
echo "****************************************** ERROR ******************************************"
echo "unable to build boost libraries, please inspect the build log for clues."
echo "*******************************************************************************************"
exit
fi

./b2 -j4 --with-python --variant=release --layout=tagged --toolset=gcc address-model=64 cxxflags="-D_GLIBCXX_USE_CXX11_ABI=0"

if [ $? -ne 0 ]; then
echo "****************************************** ERROR ******************************************"
echo "unable to build boost libraries, please inspect the build log for clues."
echo "*******************************************************************************************"
exit
fi

cp -rf ./stage/lib ../../../boost/stage/lib
mv ../../../boost/stage/lib/lib ../../../boost/stage/lib/x64

cd ..
cd ./mongo-cxx-driver-legacy-1.1.2

rm ./SConstruct
cp ../../linux/SConstruct ./

rm -rf ../../../mongo
mkdir ../../../mongo
mkdir ../../../mongo/lib

mydir="$(pwd)"

scons install --32 --cpppath=$mydir/../../../boost --libpath=$mydir/../../../boost/stage/lib/x86
scons install --32 --sharedclient --cpppath=$mydir/../../../boost --libpath=$mydir/../../../boost/stage/lib/x86

if [ $? -ne 0 ]; then
echo "****************************************** ERROR ******************************************"
echo "Error(s) were detected building mongo client libs, please inspect the build log"
echo "*******************************************************************************************"
exit
fi

cd ./build

cp -rf ./install/include ../../../../mongo
cp -rf ./install/lib ../../../../mongo/lib

mv ../../../../mongo/lib/lib ../../../../mongo/lib/x86

cd ..

scons install --64 --cpppath=$mydir/../../../boost --libpath=$mydir/../../../boost/stage/lib/x64
scons install --64 --sharedclient --cpppath=$mydir/../../../boost --libpath=$mydir/../../../boost/stage/lib/x64

if [ $? -ne 0 ]; then
echo "****************************************** ERROR ******************************************"
echo "Error(s) were detected building mongo client libs, please inspect the build log"
echo "*******************************************************************************************"
exit
fi

echo "*******************************************************************************************"
echo "************************************* Praise the sun! *************************************"
echo "*******************************************************************************************"

cd ./build

cp -rf ./install/lib ../../../../mongo/lib

mv ../../../../mongo/lib/lib ../../../../mongo/lib/x64

cd ..
cd ..
cd ..
rm -rf ./build

