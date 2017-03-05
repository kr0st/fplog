#!/bin/bash

rm -rf ../../Stage/fplogd.app

mkdir ../../Stage/fplogd.app
mkdir ../../Stage/fplogd.app/Contents
mkdir ../../Stage/fplogd.app/Contents/MacOS
mkdir ../../Stage/fplogd.app/Contents/Resources

cp -r ../../Stage/fplogd ../../Stage/fplogd.app/Contents/Resources/
cp -r ../../fplogd/Info.plist ../../Stage/fplogd.app/Contents/

touch ../../Stage/fplogd.app/Contents/MacOS/fplogd.sh
chmod 777 ../../Stage/fplogd.app/Contents/MacOS/fplogd.sh

echo "#!/bin/bash" > ../../Stage/fplogd.app/Contents/MacOS/fplogd.sh
echo "MYPWD=\$(cd \"\$(dirname \"\$0\")\"; pwd)" >> ../../Stage/fplogd.app/Contents/MacOS/fplogd.sh
echo "cmd=\"tell application \\\"Terminal\\\" to do script \\\"\$MYPWD/../Resources/fplogd\\\"\"" >> ../../Stage/fplogd.app/Contents/MacOS/fplogd.sh
echo "osascript -e \"\$cmd\"" >> ../../Stage/fplogd.app/Contents/MacOS/fplogd.sh

install_name_tool -add_rpath "/Library/Frameworks/fplog.framework/Versions/A/" ../../Stage/fplogd.app/Contents/Resources/fplogd

codesign -s "Developer ID Application: Rostislav Kuratch" -f ../../Stage/fplogd.app
