#!/bin/bash

rm -rf ../../Stage/dist

mkdir ../../Stage/dist
mkdir ../../Stage/dist/fplog_1.0-1
mkdir ../../Stage/dist/fplog_1.0-1/usr
mkdir ../../Stage/dist/fplog_1.0-1/usr/local
mkdir ../../Stage/dist/fplog_1.0-1/usr/local/bin
mkdir ../../Stage/dist/fplog_1.0-1/usr/local/lib
mkdir ../../Stage/dist/fplog_1.0-1/usr/local/include
mkdir ../../Stage/dist/fplog_1.0-1/DEBIAN

touch ../../Stage/dist/fplog_1.0-1/DEBIAN/control

echo "Package: fplog" > ../../Stage/dist/fplog_1.0-1/DEBIAN/control
echo "Version: 1.0-1" >> ../../Stage/dist/fplog_1.0-1/DEBIAN/control
echo "Section: base" >> ../../Stage/dist/fplog_1.0-1/DEBIAN/control
echo "Priority: optional" >> ../../Stage/dist/fplog_1.0-1/DEBIAN/control
echo "Architecture: amd64" >> ../../Stage/dist/fplog_1.0-1/DEBIAN/control
echo "Depends: " >> ../../Stage/dist/fplog_1.0-1/DEBIAN/control
echo "Maintainer: kr0st <kr0st@bioreactor.me>" >> ../../Stage/dist/fplog_1.0-1/DEBIAN/control
echo "Description: JSON-based logging solution for PC and embedded devices that uses NoSQL for logs storage." >> ../../Stage/dist/fplog_1.0-1/DEBIAN/control

cp -r ../../Stage/*.so* ../../Stage/dist/fplog_1.0-1/usr/local/lib/
cp -r ../../Stage/fplog_wrapper.py ../../Stage/dist/fplog_1.0-1/usr/local/lib/

cp -r ../../Stage/json.lua ../../Stage/dist/fplog_1.0-1/usr/local/bin/
cp -r ../../Stage/fplogd ../../Stage/dist/fplog_1.0-1/usr/local/bin/
cp -r ../../Stage/fpcollect ../../Stage/dist/fplog_1.0-1/usr/local/bin/

cp -r ../../common/fplog_exceptions.h ../../Stage/dist/fplog_1.0-1/usr/local/include/
cp -r ../../common/fplog_transport.h ../../Stage/dist/fplog_1.0-1/usr/local/include/
cp -r ../../fplog/fplog.h ../../Stage/dist/fplog_1.0-1/usr/local/include/

find ../../libjson -type d -exec bash -c 'rm -rf {}/._*' \;

mkdir ../../Stage/dist/fplog_1.0-1/usr/local/include/libjson

cp -r ../../libjson/* ../../Stage/dist/fplog_1.0-1/usr/local/include/libjson
find ../../Stage/dist/fplog_1.0-1/usr/local/include/libjson/ -type d -exec bash -c 'rm -f {}/*[!.h]' \;

cp ../../libjson/License.txt ../../Stage/dist/fplog_1.0-1/usr/local/include/libjson
rm -rf "../../Stage/dist/fplog_1.0-1/usr/local/include/libjson/Getting Started"
rm -rf ../../Stage/dist/fplog_1.0-1/usr/local/include/libjson/libjson.xcodeproj
rm -rf ../../Stage/dist/fplog_1.0-1/usr/local/include/libjson/_internal/TestSuite
rm -rf ../../Stage/dist/fplog_1.0-1/usr/local/include/libjson/_internal/TestSuite2

find ../../Stage/dist/fplog_1.0-1 -exec chmod 644 {} +

chmod 0755 ../../Stage/dist/fplog_1.0-1
chmod 0755 ../../Stage/dist/fplog_1.0-1/usr
chmod 0755 ../../Stage/dist/fplog_1.0-1/usr/local
chmod 0755 ../../Stage/dist/fplog_1.0-1/usr/local/bin
chmod 0755 ../../Stage/dist/fplog_1.0-1/usr/local/lib
chmod 0755 ../../Stage/dist/fplog_1.0-1/usr/local/include
chmod 0755 ../../Stage/dist/fplog_1.0-1/usr/local/include/libjson
chmod 0755 ../../Stage/dist/fplog_1.0-1/usr/local/include/libjson/_internal
chmod 0755 ../../Stage/dist/fplog_1.0-1/usr/local/include/libjson/_internal/Dependencies
chmod 0755 ../../Stage/dist/fplog_1.0-1/usr/local/include/libjson/_internal/Dependencies/libbase64++
chmod 0755 ../../Stage/dist/fplog_1.0-1/usr/local/include/libjson/_internal/Dependencies/mempool++
chmod 0755 ../../Stage/dist/fplog_1.0-1/usr/local/include/libjson/_internal/Source
chmod 0755 ../../Stage/dist/fplog_1.0-1/usr/local/include/libjson/_internal/Source/JSONDefs
chmod 0755 ../../Stage/dist/fplog_1.0-1/DEBIAN

find ../../Stage/dist/fplog_1.0-1/usr/local/bin -exec chmod 0755 {} +
chmod 644 ../../Stage/dist/fplog_1.0-1/usr/local/bin/json.lua

find ../../Stage/dist/fplog_1.0-1 -exec chown "root:root" {} +

dpkg-deb --build ../../Stage/dist/fplog_1.0-1

