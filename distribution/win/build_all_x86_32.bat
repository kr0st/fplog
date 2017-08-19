cd ..
cd ..
rmdir Stage /S /Q
md Stage
call "C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild" fplog.sln /p:Configuration=Release /p:Platform=Win32 /t:Rebuild
