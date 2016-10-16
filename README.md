# fplog
JSON-based logging solution for PC and embedded devices that uses NoSQL for logs storage.

# building

###### Windows
First of all you need to have all dependencies built. In order to do that please open command prompt but not just any command prompt, you will need ‘Developer Command Prompt for VS2015’. Afterwards get to the autobuild folder inside fplog solution top directory.

There is a ‘win’ subfolder, just cd there and run ‘make_dep.bat’. If you’re lucky, after about half an hour (or much faster on 4 or 8 cored CPUs) there are no error messages inside command prompt window and a new folder sitting inside ‘autobuild’ directory, called ‘build’. This is a temporary folder that can be safely deleted, freeing up couple of gigabytes.

Now just launch Visual Studio 2015 and build the *fplog* solution, everything should be ready for building.

In case there were errors, you’re on your own, pal, I’ve tried to make dependency building script as transparent as possible, read ‘make_dep.bat’ and try to understand what went wrong.

###### Linux
CodeLite IDE is used for building the solution, however there is no automatic building of dependencies yet. If you would like to build dependencies manually then you would need same libraries and their versions as in Windows build, take a look at ‘make_dep.bat’, build everything and replicate the resulting folder structure as on Windows.

I will introduce automatic dependency building on Linux too, just not there yet.
