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

curl --cacert ./cacert.crt --output cacert.pem https://curl.haxx.se/ca/cacert.pem
if [ $? -eq 0 ]; then
rm cacert.crt
mv cacert.pem cacert.crt
fi

rm -rf ../build
mkdir ../build

curl --cacert ./cacert.crt -o ../build/mongo_driver.zip https://codeload.github.com/mongodb/mongo-cxx-driver/zip/legacy-1.1.2
curl -o ../build/boost.7z http://netcologne.dl.sourceforge.net/project/boost/boost/1.63.0/boost_1_63_0.7z

7za x -y -o"../build/" ../build/mongo_driver.zip
7za x -y -o"../build/" ../build/boost.7z

cd ..
cd build
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

./b2 -j4 --build-type=complete --layout=versioned --toolset=gcc architecture=x86 address-model=32

if [ $? -ne 0 ]; then
echo "****************************************** ERROR ******************************************"
echo "unable to build boost libraries, please inspect the build log for clues."
echo "*******************************************************************************************"
exit
fi

./b2 -j4 --with-python --build-type=complete --layout=versioned --toolset=gcc architecture=x86 address-model=32

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

./b2 -j4 --build-type=complete --layout=versioned --toolset=gcc address-model=64

if [ $? -ne 0 ]; then
echo "****************************************** ERROR ******************************************"
echo "unable to build boost libraries, please inspect the build log for clues."
echo "*******************************************************************************************"
exit
fi

./b2 -j4 --with-python --build-type=complete --layout=versioned --toolset=gcc address-model=64

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

rm -rf ../../../mongo
mkdir ../../../mongo
mkdir ../../../mongo/lib

mydir="$(pwd)"

scons install --32 --cpppath=$mydir/../../../boost --libpath=$mydir/../../../boost/stage/lib/x86 --boost-lib-search-suffixes=-gcc48-1_63,-gcc48-mt-1_63,-gcc48-mt-d-1_63,-gcc48-d-1_63,-gcc48-s-1_63,-gcc48-sd-1_63,-gcc48-mt-s-1_63,-gcc48-mt-sd-1_63
scons install --32 --dbg=on --cpppath=$mydir/../../../boost --libpath=$mydir/../../../boost/stage/lib/x86 --boost-lib-search-suffixes=-gcc48-1_63,-gcc48-mt-1_63,-gcc48-mt-d-1_63,-gcc48-d-1_63,-gcc48-s-1_63,-gcc48-sd-1_63,-gcc48-mt-s-1_63,-gcc48-mt-sd-1_63
scons install --32 --sharedclient --cpppath=$mydir/../../../boost --libpath=$mydir/../../../boost/stage/lib/x86 --boost-lib-search-suffixes=-gcc48-1_63,-gcc48-mt-1_63,-gcc48-mt-d-1_63,-gcc48-d-1_63,-gcc48-s-1_63,-gcc48-sd-1_63,-gcc48-mt-s-1_63,-gcc48-mt-sd-1_63
scons install --32 --sharedclient --dbg=on --cpppath=$mydir/../../../boost --libpath=$mydir/../../../boost/stage/lib/x86 --boost-lib-search-suffixes=-gcc48-1_63,-gcc48-mt-1_63,-gcc48-mt-d-1_63,-gcc48-d-1_63,-gcc48-s-1_63,-gcc48-sd-1_63,-gcc48-mt-s-1_63,-gcc48-mt-sd-1_63

cd ./build

cp -rf ./install/include ../../../../mongo
cp -rf ./install/lib ../../../../mongo/lib

mv ../../../../mongo/lib/lib ../../../../mongo/lib/x86

cd ..

scons install --64 --cpppath=$mydir/../../../boost --libpath=$mydir/../../../boost/stage/lib/x64 --boost-lib-search-suffixes=-gcc48-1_63,-gcc48-mt-1_63,-gcc48-mt-d-1_63,-gcc48-d-1_63,-gcc48-s-1_63,-gcc48-sd-1_63,-gcc48-mt-s-1_63,-gcc48-mt-sd-1_63
scons install --64 --dbg=on --cpppath=$mydir/../../../boost --libpath=$mydir/../../../boost/stage/lib/x64 --boost-lib-search-suffixes=-gcc48-1_63,-gcc48-mt-1_63,-gcc48-mt-d-1_63,-gcc48-d-1_63,-gcc48-s-1_63,-gcc48-sd-1_63,-gcc48-mt-s-1_63,-gcc48-mt-sd-1_63
scons install --64 --sharedclient --cpppath=$mydir/../../../boost --libpath=$mydir/../../../boost/stage/lib/x64 --boost-lib-search-suffixes=-gcc48-1_63,-gcc48-mt-1_63,-gcc48-mt-d-1_63,-gcc48-d-1_63,-gcc48-s-1_63,-gcc48-sd-1_63,-gcc48-mt-s-1_63,-gcc48-mt-sd-1_63
scons install --64 --sharedclient --dbg=on --cpppath=$mydir/../../../boost --libpath=$mydir/../../../boost/stage/lib/x64 --boost-lib-search-suffixes=-gcc48-1_63,-gcc48-mt-1_63,-gcc48-mt-d-1_63,-gcc48-d-1_63,-gcc48-s-1_63,-gcc48-sd-1_63,-gcc48-mt-s-1_63,-gcc48-mt-sd-1_63

if [ $? -eq 0 ]; then
echo "*******************************************************************************************"
echo "************************************* Praise the sun! *************************************"
echo "*******************************************************************************************"
fi

if [ $? -ne 0 ]; then
echo "****************************************** ERROR ******************************************"
echo "Error(s) were detected building mongo client libs, please inspect the build log"
echo "*******************************************************************************************"
exit
fi

cd ./build

cp -rf ./install/lib ../../../../mongo/lib

mv ../../../../mongo/lib/lib ../../../../mongo/lib/x64

cd ..
cd ..
cd ..
rm -rf ./build
