# fplog
JSON-based logging solution for PC and embedded devices that uses NoSQL for logs storage.

# building

### Windows
First of all you need to have all dependencies built. In order to do that please open command prompt but not just any command prompt, you will need ‘Developer Command Prompt for VS2015’. Afterwards get to the autobuild folder inside fplog solution top directory.

There is a ‘win’ subfolder, just cd there and run ‘make_dep.bat’. If you’re lucky, after about half an hour (or much faster on 4 or 8 cored CPUs) there are no error messages inside command prompt window and solution is ready to build. Now just launch Visual Studio 2015 and build the *fplog* solution.

In case there were errors, you’re on your own, pal, I’ve tried to make dependency building script as transparent as possible, read ‘make_dep.bat’ and try to understand what went wrong.

### Linux
CodeLite IDE is used for building the solution, so you would need to install it first. Then get to the autobuild folder inside fplog solution top directory.
There is a ‘linux’ subfolder, just cd there and run ‘make_dep.sh’.

### macOS
Please install Xcode and Homebrew. You will need to execute a number of commands in the terminal before attempting the build and run:

    brew install python
    brew install python3
    brew install scons

After running those commands you are ready to build dependencies. However prior to do that, open the autobuild/macos/project-config.jam file with text editor and find a line containing “using python : ” there.
Change this line in order for it to reflect the exact version and paths of the Python 3 that is installed on your system via Homebrew. Finally go to autobuild/macos folder and launch 'make_dependencies.sh'.

fplog is an Xcode workspace, so just load it up and select which project you'd like to build.


###### Fixing UDT
NOTE: you only need this fix if you intend to use UDT protocol, it is not used by default.

As many BSD-based systems, macOS has much smaller socket buffer initially, compared to Windows or Linux. We are going to fix it by launching a terminal and executing the below commands (spoiler alert - your Mac will reboot):

    sudo nvram boot-args="ncl=131072"
    sudo shutdown -r now

If you are on macOS Sierra or later the above will not work from normal mode, you will need to reboot into recovery mode (cmd +r) and run the above in the terminal there.

After the reboot open the terminal again and run these commands:

    sudo sysctl -w kern.ipc.maxsockbuf=16777216
    sudo sysctl -w net.inet.tcp.sendspace=1048576
    sudo sysctl -w net.inet.tcp.recvspace=1048576

However this will not fix the issue permanently, in order to that, you will have to put the above settings inside sysctl.conf file and put this file inside /etc/. Here is how the content of sysctl.conf should look like:

    kern.ipc.maxsockbuf=16777216
    net.inet.tcp.sendspace=1048576
    net.inet.tcp.recvspace=1048576

Most likely you do not have sysctl.conf inside your /etc/ folder, just create it and paste the settings there. You will need root access in order to save or edit the file inside /etc/, sudo should help you in this regard.

###### Distribution and usage
Install Packages tool: http://s.sudre.free.fr/Software/Packages/about.html

Launch “make_macos_package.sh” located under distribution/macOS folder. Stage directory will contain resulting fplog.pkg that should be installed on your OS. Package contains fplog.framework that will be installed into /Library/Frameworks.

In order to use fplog framework it is necessary not only to tell Xcode to include the framework into your project but also to add the following directory to “Runpath Search Paths” inside the build settings: “/Library/Frameworks/fplog.framework/Versions/Current”.
