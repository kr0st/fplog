#!/bin/bash

rm -rf ../../Stage/fplog_1.0-1

mkdir ../../Stage/fplog_1.0-1
mkdir ../../Stage/fplog_1.0-1/usr
mkdir ../../Stage/fplog_1.0-1/usr/local
mkdir ../../Stage/fplog_1.0-1/usr/local/bin
mkdir ../../Stage/fplog_1.0-1/usr/local/lib
mkdir ../../Stage/fplog_1.0-1/usr/local/include
mkdir ../../Stage/fplog_1.0-1/DEBIAN

touch ../../Stage/fplog_1.0-1/DEBIAN/control

echo "Package: fplog" > ../../Stage/fplog_1.0-1/DEBIAN/control
echo "Version: 1.0-1" >> ../../Stage/fplog_1.0-1/DEBIAN/control
echo "Section: base" >> ../../Stage/fplog_1.0-1/DEBIAN/control
echo "Priority: optional" >> ../../Stage/fplog_1.0-1/DEBIAN/control
echo "Architecture: amd64" >> ../../Stage/fplog_1.0-1/DEBIAN/control
echo "Depends: " >> ../../Stage/fplog_1.0-1/DEBIAN/control
echo "Maintainer: kr0st <kr0st@bioreactor.me>" >> ../../Stage/fplog_1.0-1/DEBIAN/control
echo "Description: JSON-based logging solution for PC and embedded devices that uses NoSQL for logs storage." >> ../../Stage/fplog_1.0-1/DEBIAN/control

cp -r ../../Stage/*.so* ../../Stage/fplog_1.0-1/usr/local/lib/
cp -r ../../Stage/json.lua ../../Stage/fplog_1.0-1/usr/local/bin/
cp -r ../../Stage/fplogd ../../Stage/fplog_1.0-1/usr/local/bin/
cp -r ../../Stage/fpcollect ../../Stage/fplog_1.0-1/usr/local/bin/

cp -r ../../common/fplog_exceptions.h ../../Stage/fplog_1.0-1/usr/local/include/
cp -r ../../common/fplog_transport.h ../../Stage/fplog_1.0-1/usr/local/include/
cp -r ../../fplog/fplog.h ../../Stage/fplog_1.0-1/usr/local/include/
mkdir ../../Stage/fplog_1.0-1/usr/local/include/libjson
cp -r ../../libjson/* ../../Stage/fplog_1.0-1/usr/local/include/libjson

find ../../Stage/fplog_1.0-1 -exec chmod 644 {} +

chmod 0755 ../../Stage/fplog_1.0-1
chmod 0755 ../../Stage/fplog_1.0-1/usr
chmod 0755 ../../Stage/fplog_1.0-1/usr/local
chmod 0755 ../../Stage/fplog_1.0-1/usr/local/bin
chmod 0755 ../../Stage/fplog_1.0-1/usr/local/include
chmod 0755 ../../Stage/fplog_1.0-1/usr/local/include/libjson
chmod 0755 ../../Stage/fplog_1.0-1/usr/local/include/libjson/_internal
chmod 0755 ../../Stage/fplog_1.0-1/usr/local/include/libjson/_internal/Source
chmod 0755 ../../Stage/fplog_1.0-1/usr/local/include/libjson/_internal/Source/JSONDefs
chmod 0755 ../../Stage/fplog_1.0-1/DEBIAN

find ../../Stage/fplog_1.0-1/usr/local/bin -exec chmod 0755 {} +

find ../../Stage/fplog_1.0-1 -exec chown "root:root" {} +

dpkg-deb --build ../../Stage/fplog_1.0-1

