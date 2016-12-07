# fplog
JSON-based logging solution for PC and embedded devices that uses NoSQL for logs storage.

# building

### Windows
First of all you need to have all dependencies built. In order to do that please open command prompt but not just any command prompt, you will need ‘Developer Command Prompt for VS2015’. Afterwards get to the autobuild folder inside fplog solution top directory.

There is a ‘win’ subfolder, just cd there and run ‘make_dep.bat’. If you’re lucky, after about half an hour (or much faster on 4 or 8 cored CPUs) there are no error messages inside command prompt window and a new folder sitting inside ‘autobuild’ directory, called ‘build’. This is a temporary folder that can be safely deleted, freeing up couple of gigabytes.

Now just launch Visual Studio 2015 and build the *fplog* solution, everything should be ready for building.

In case there were errors, you’re on your own, pal, I’ve tried to make dependency building script as transparent as possible, read ‘make_dep.bat’ and try to understand what went wrong.

### Linux
CodeLite IDE is used for building the solution, however there is no automatic building of dependencies yet. If you would like to build dependencies manually then you would need same libraries and their versions as in Windows build, take a look at ‘make_dep.bat’, build everything and replicate the resulting folder structure as on Windows.

I will introduce automatic dependency building on Linux too, just not there yet.

### macOS
Please install Xcode and Homebrew. fplog is an Xcode workspace, so just load it up and select which project you'd like to build.
You will need to execute a number of commands in the terminal before attempting the build and run:

    brew install coreutils
    brew install https://raw.githubusercontent.com/Homebrew/homebrew-core/d6cad1de88cbca5ffc7028328997198fa826be61/Formula/boost.rb
    brew install https://raw.githubusercontent.com/Homebrew/homebrew-core/8b0c14c9a96b32d981786cd65cbcc8961df20e46/Formula/boost-python.rb

The last 2 lines install boost (and boost-python) of a specific version, 1.62.0. At the moment of writing this instruction there is no simpler or proper Homebrew way of installing a specific version of any given package. What I am doing is using commit history on GitHub in order to install the package at the moment in history when it referenced Boost v1.62.0.

###### Building MongoDB client

Execute the following commands from the root folder of fplog repo:

    git clone -b releases/legacy https://github.com/mongodb/mongo-cxx-driver.git
    cd ./mongo-cxx-driver
    brew install scons
    scons install
    cp -r ./build/install/ ../mongo/

Note that it depends on Boost, so you have to install Boost libraries first.

###### Fixing UDT

As many BSD-based systems, macOS has much smaller socket buffer initially, compared to Windows or Linux. We are going to fix it by launching a terminal and executing the below commands (spoiler alert - your Mac will reboot):

    sudo nvram boot-args="ncl=131072"
    sudo shutdown -r now

After the reboot open the terminal again and run these commands:

    sudo sysctl -w kern.ipc.maxsockbuf=16777216
    sudo sysctl -w net.inet.tcp.sendspace=1048576
    sudo sysctl -w net.inet.tcp.recvspace=1048576

However this will not fix the issue permanently, in order to that, you will have to put the above settings inside sysctl.conf file and put this file inside /etc/. Here is how the content of sysctl.conf should look like:

    kern.ipc.maxsockbuf=16777216
    net.inet.tcp.sendspace=1048576
    net.inet.tcp.recvspace=1048576

Most likely you do not have sysctl.conf inside your /etc/ folder, just create it and paste the settings there. You will need root access in order to save or edit the file inside /etc/, sudo should help you in this regard.
