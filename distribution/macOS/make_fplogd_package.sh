#!/bin/bash

rm -rf ../../Stage/dist/fplogd.app

mkdir ../../Stage/dist/fplogd.app
mkdir ../../Stage/dist/fplogd.app/Contents
mkdir ../../Stage/dist/fplogd.app/Contents/MacOS
mkdir ../../Stage/dist/fplogd.app/Contents/Resources

cp -r ../../Stage/fplogd ../../Stage/dist/fplogd.app/Contents/Resources/
cp -r ../../fplogd/Info.plist ../../Stage/dist/fplogd.app/Contents/

touch ../../Stage/dist/fplogd.app/Contents/MacOS/fplogd.sh
chmod 777 ../../Stage/dist/fplogd.app/Contents/MacOS/fplogd.sh

echo "#!/bin/bash" > ../../Stage/dist/fplogd.app/Contents/MacOS/fplogd.sh
echo "MYPWD=\$(cd \"\$(dirname \"\$0\")\"; pwd)" >> ../../Stage/dist/fplogd.app/Contents/MacOS/fplogd.sh
echo "cmd=\"tell application \\\"Terminal\\\" to do script \\\"\$MYPWD/../Resources/fplogd\\\"\"" >> ../../Stage/dist/fplogd.app/Contents/MacOS/fplogd.sh
echo "osascript -e \"\$cmd\"" >> ../../Stage/dist/fplogd.app/Contents/MacOS/fplogd.sh

install_name_tool -add_rpath "/Library/Frameworks/fplog.framework/Versions/A" ../../Stage/dist/fplogd.app/Contents/Resources/fplogd

codesign -s "Developer ID Application: Rostislav Kuratch" -f ../../Stage/fplogd.app

