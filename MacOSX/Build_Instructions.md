# Building QuakeSpasm
## Prerequisites
Since Apple has dropped support for Mac OS 10.4 and PowerPC from XCode 4, you need to hack your developer tools a bit. You need XCode 3.2.6 from here:

http://connect.apple.com/cgi-bin/WebObjects/MemberSite.woa/wa/getSoftware?bundleID=20792

Then, you need the XCode 4.2 installer from the Mac App Store. Don't start the installation yet.

If you already have XCode 4 installed, you must uninstall it using the following command from a terminal:

sudo /<XCode 4 path>/Library/uninstall-devtools --mode=all

You must reboot your machine after that.

Now you need to first install XCode 3.2.6 and then XCode 4.2. Then, you will add support for Mac OS 10.4 and 10.5 as well as PowerPC from XCode 3 to XCode 4. But first things first:

### Install XCode 3
If you are on Mac OS 10.7 (Lion), you must launch the XCode 3.2.6 installer from the terminal like so:

export COMMAND_LINE_INSTALL=1
open "/Volumes/Xcode and iOS SDK/Xcode and iOS SDK.mpkg"

Otherwise, the installation will fail. Do not install "System Tools" or "Unix Development". You probably don't want to install the iOS SDKs either. You do want to install "Mac OS X 10.4 SDK" however - this is essential. Set "/XCode3" as the destination folder.

### Install XCode 4
Launch the installer from the App Store. Be aware that on some systems, you need to launch the installer manually: Right Click on "Install Xcode" and select Show Package Contents. Then navigate to "Contents/Resources" and double click on "Xcode.mpkg". Select "/XCode4" as the destination folder.

### Restore 10.4 and 10.5 SDK support
Open a terminal and run the following commands:

cd /XCode4/SDKs
sudo ln -s /Xcode3/SDKs/MacOSX10.4u.sdk .
sudo ln -s /Xcode3/SDKs/MacOSX10.5.sdk .

### Restore GCC 4.0 support (which gives you PowerPC support)
In your terminal, run the following commands:

cd /XCode4/usr/bin
sudo ln -s /Xcode3/usr/bin/*4.0* .

sudo ln -s "/XCode3/Library/Xcode/Plug-ins/GCC 4.0.xcplugin" "/XCode4/Library/Xcode/PrivatePlugIns/Xcode3Core.ideplugin/Contents/SharedSupport/Developer/Library/Xcode/Plug-ins"

sudo mkdir -p /XCode4/usr/libexec/gcc
sudo ln -sf /XCode3/usr/libexec/gcc/powerpc-apple-darwin10 /XCode4/usr/libexec/gcc/powerpc-apple-darwin10

sudo mkdir -p /XCode4/usr/lib/gcc
sudo ln -sf /XCode3/usr/lib/gcc/powerpc-apple-darwin10 /XCode4/usr/lib/gcc/powerpc-apple-darwin10

This should be it. The guides in the link collection at the end of this documents contain information about changing the "as" command also, but I didn't have to do this on my systems. If you run into problems, let me know: kristian.duske@gmail.com

## Building QuakeSpasm
Now you can build QuakeSpasm. Simply open the project file using XCode 4.2 and make sure that "QuakeSpasm > My Mac 64-bit" is selected as the scheme in the toolbar. Select "Product > Run" from the menu to run and debug QuakeSpasm. This will not produce a universal binary however. To produce a universal binary for Intel 64bit, Intel 32bit and PowerPC 32bit, you need to select "Product > Archive" from the menu. This will create an application archive that contains the universal binary.

# Useful Links
- XCode 3.2.6:
http://connect.apple.com/cgi-bin/WebObjects/MemberSite.woa/wa/getSoftware?bundleID=20792

- Install XCode 3 on 10.7:
http://anatomicwax.tumblr.com/post/8064949186/installing-xcode-3-2-6-on-lion-redux

- Restore support for 10.4, 10.5 and PPC to XCode 4.0:
http://stackoverflow.com/questions/5333490/how-can-we-restore-ppc-ppc64-as-well-as-full-10-4-10-5-sdk-support-to-xcode-4

- Scripts that automate the above:
https://github.com/thinkyhead/Legacy-XCode-Scripts

# Author
Kristian Duske, kristian.duske@gmail.com
