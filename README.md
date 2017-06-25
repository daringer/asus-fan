# **asus-fan**

ASUS (Zenbook) fan(s) control and monitor kernel module.

- [Compatibilty](#compatibilty)
- [Installation with DKMS](#installation-with-dkms)
  - [ArchLinux](#archlinux)
  - [Ubuntu](#ubuntu)
- [Quickstart](#quickstart)
- [Short Comparison To Other Similar Projects](#short-comparison-to-other-similar-projects)
- [Tools/Configs/Misc](#tools--configs--simple-fancontrol-script)
  - [fancontrol script](#fancontrol-script)
  - [build module in debug-mode](#debug-module)
  - [force loading of module](#force-load)
- [ToDos](#todos)
- [ThanksTo](#thanks-to)

## Compatibilty
The following Notebooks should be supported - be aware that it's still not in production state

Single Fan | Two Fans (NVIDIA)
-----------|-------------------
UX21E      | UX32VD
UX31E      | UX42VS
UX21A      | UX52VS
UX31A      | U500VZ
UX32A      | NX500
UX301LA    | UX32LN
UX302LA    | UX303LB
N551JK     | N552VX
N56JN      |

## Installation with DKMS
Dynamic Kernel Module Support (DKMS) is a program/framework that enables generating Linux kernel modules whose sources generally reside outside the kernel source tree. The concept is to have DKMS modules automatically rebuilt when a new kernel is installed. -  [Wikipedia](https://en.wikipedia.org/wiki/Dynamic_Kernel_Module_Support)

Installing the asus-fan kernel module with DKMS means that when you upgrade to a new kernel you do not need to repeat the process outlined below in the Quickstart section each time. The asus-fan kernel module will automatically be rebuilt when a new kernel is installed.

More information on DKMS: [Ubuntu Help - DKMS](https://help.ubuntu.com/community/DKMS)

Scripted Ubuntu DKMS Setup for Asus Fan Module
--------------------
* Download the `ubuntu_dkms_sudo_install.sh` script from the `misc` folder of this repository. (ie. right-click over **Raw** > select **Save link as...**)
* Make sure the script is executable (ie. `chmod +x ubuntu_dkms_sudo_install.sh`)
* Run the script (ie. `./ubuntu_dkms_sudo_install.sh`)
* The script will need super user powers and will ask you to enter your password to get sudo permissions
* Check that the module has been built and installed with `lsmod | grep asus_fan`. If you get something lik **asus_fan               14880  0** you are good. If you get nothing the module is not loaded.

<a name="ubuntu-symlink">Ubuntu - Symlink Creation on reboot</a>
---------------------
Symlinks will need to be created each time. The `asus-fan-create-symlinks.sh` is designed for this purpose, however it must be run after each reboot to create these links.

If you used the `ubuntu_dkms_sudo_install.sh` installation script above the `asus-fan-ubuntu-create-symlinks.sh` will have been installed at `/usr/local/sbin/asus-fan-create-symlinks.sh`.

For Ubuntu 14.04 using the Upstart init system add the following to the Thermald upstart configuration file, `/etc/init/thermald.conf

``` bash
pre-start script
if [ ! -d  /tmp/asus-fan-shm ]; then
/usr/local/sbin/asus-fan-create-symlinks.sh
echo " * creating symlinks ..."
fi
end script
```

My full `/etc/init/thermald.conf` looks like this:

``` bash
# thermald - thermal daemon
# Upstart configuration file
# Manages platform thermals

description	"thermal daemon"

start on runlevel [2345] and started dbus
stop on stopping dbus

#
# don't respawn on error
#
normal exit 1

### ArchLinux
Just get the [PKGBUILD](https://raw.githubusercontent.com/daringer/asus-fan/master/buildscripts/archlinux/asus-fan-dkms-git/PKGBUILD) and the [install script](https://raw.githubusercontent.com/daringer/asus-fan/master/buildscripts/archlinux/asus-fan-dkms-git/asus-fan-dkms-git.install) and run ``makepkg``:
    
    cd /tmp
    mkdir asus-fan-build 
    cd asus-fan-build 
    wget https://raw.githubusercontent.com/daringer/asus-fan/master/buildscripts/archlinux/PKGBUILD
    wget https://raw.githubusercontent.com/daringer/asus-fan/master/buildscripts/archlinux/asus-fan-dkms-git/asus-fan-dkms-git.install 
    makepkg
    sudo pacman -U asus-fan-dkms-git-*.pkg.tar.xz

### Ubuntu
As the superuser (root):

respawn

#
# consider something wrong if respawned 10 times in 1 minute
#
respawn limit 10 60

pre-start script
if [ ! -d  /tmp/asus-fan-shm ]; then
/usr/local/sbin/asus-fan-create-symlinks.sh
echo " * creating symlinks ..."
fi
end script

exec thermald --no-daemon --dbus-enable
```

Troubleshooting Symlinks
---------------------
Runing `ls -l /tmp/asus-fan-shm/` will list the sysmlinks created. If it looks like you are missing some (this depends on the Asus laptop you have as to how many and which ones you will have) you can run `sudo asus-fan-create-symlinks.sh`. This will delete the links in the folder and recreate them. A number of them are only created if they are readable.


Manual Ubuntu DKMS Setup for Asus Fan Module
---------------------
``` bash
cd /usr/src
sudo wget -o asus-fan-master.tar.gz  https://github.com/daringer/asus-fan/archive/master.tar.gz
sudo mkdir asus_fan-master
cd asus_fan-master
sudo tar xpvf ../asus_fan.tar.gz --strip-components=1
sudo  tar xpvf ../asus_fan.tar.gz --strip-component
sudo  mv dkms.conf dkms.conf.archlinux
sudo mv dkms-ubuntu.conf dkms.conf
cd ..
sudo dkms add -m asus_fan -v master
sudo dkms install -m asus_fan -v master
sudo echo asus_fan >>/etc/modules
sudo cp misc/asus-fan-create-symlinks.sh /usr/local/sbin/asus-fan-create-symlinks.sh
```

For Ubuntu 14.04 you will also need to add the lines mention above in the **[Ubuntu - Symlink Creation on reboot](#ubuntu-symlink)** section to  `/etc/init/thermald.conf` so that symlinks get created at each reboot by the Upstart init system. Newer versions of Ubuntu use the Systemd init system and not Upstart.

Quickstart - Manual installation
----------

- **Build** - just run ```make``` inside the directory
- **Install** - run ```sudo make install``` inside the directory
- **Load** - simply as usual:
```bash
modprobe asus_fan
```
- **Interface** - the fan(s) is/are exposed as ```hwmon```, thus available in:
```bash
fpath="/sys/class/hwmon/hwmonX"
```
- **Monitor Fan speed** - simply use [xsensors](http://www.linuxhardware.org/xsensors/) or [psensors](http://wpitchoune.net/psensor/) for graphical, or [sensors](http://www.lm-sensors.org/) for console monitoring. My personal preference is [conky](https://github.com/brndnmtthws/conky)---there are alot [linux system monitors](https://lmgtfy.com/?q=linux+system+monitor).

- **Set Fan Speed** - write anything from 0 to 255 to ```pwmX```, like:
```bash
echo 123 > ${fpath}/pwmX   # set to 123
echo 0 > ${fpath}/pwmX     # switch fan off (DANGEROUS!)
echo 255 > ${fpath}/pwmX   # set to max speed
```
- **ATTENTION** - the fan is now in manual mode - do not burn your machine!
- **Set Auto-Fan(s)**: to reactivate the automatic fan control write "0" to ```pwmX_enable```:
```bash
echo 0 > ${fpath}/pwmX_enable   # reset to auto-mode (always for all fans)
```

- **Max fan speed** There is an additional file for controling the maximum fan speed. It's r/w and controls both, automatic mode and manual mode maximum speed. Value range: 0-255 reset value:256


## Short Comparison To Other Similar Projects
- [asus-fancontrol](https://github.com/nicolai-rostov/asus-fancontrol) userland application, acpi_call based, limited number of (offical/tested) compatible zenbook models, no (standard) interface exposed, no support for second fan
- [zenbook](https://github.com/juyrjola/zenbook) userland appliaction, acpi_call based, only UX32VD supported, no (standard) interface exposed, no support for second fan
- [asusfan](https://github.com/gpiemont/asusfan) kernelspace driver, model support: A8J, A8JS, N50V[cn], no (standard) interface exposed, interface based on module parameters, replaces automatic control with own realization, no support for second fan
- [asusfan (original)](https://code.google.com/p/asusfan/) kernelspace driver, Model support A8J and A8JS, no (standard) interface, automatic fan speed adjustment, no support for second fan
- [asusfanctrld](https://github.com/nflx/asusfanctrld) userland (bash) application, acpi_call  based, only UX32VD, second fan support?, no (standard) interface exposed
- [asus_ux32v_fan_control](https://github.com/chrischdi/asus_ux32v_fan_control) is actually a fork of this project undergone a renaming (hmm), very old state, no hwmon (interface) support, thermal_device interface, most likely as buggy as this project was one year ago
- [asus_wmi](https://github.com/KastB/asus_wmi) realization as a part of the exisiting `asus_wmi` module, which is maybe the only way to submit it for the upstream kernel, nevertheless support for the second fan is not available through wmi. Sister project, closely related to the one shown here.

So this [project](https://github.com/daringer/asus-fan) distinguishes itself from the others by providing a **hwmon standard state-of-art interface** with a wide variety of **supported zenbook models** realized as a **kernelspace driver**. Guess you found the *right* one ;)


## Tools / Configs / Simple Fancontrol Script
### fancontrol script
There is a script called "fancontrol" that can be configured according to temperature source, fans to control, minimum and maximum temperature, thats done by "pwmconfig"
Nevertheless that script did it worse for me than the original controller - thus you can tell it to stop the fan completely...

### debug module
`DEBUG=1` may be passed to make to build the module in debug-mode. `dmesg` will then contain alot more debug output for the module:

```bash
  make DEBUG=1
  sudo rmmod asus-fan
  sudo insmod asus-fan.ko
```
### force load module 
**USE THIS ONLY WITH CAUTION FOR TESTING, UNFORSEEABLE THINGS MAY HAPPEN! YOU'VE BEEN WARNED...**
`force_load=1` may be passed as an argument during module loading to skip the device checks and tests:

```bash
  sudo rmmod asus-fan
  sudo insmod asus-fan.ko force_load=1
```

## TODOs:
- do a code review and clean it up
- check with more models
- add an included fan controller
- submit an upstream patch - any howtos?? wtf, write acpi-devel kernel-mailinglist ??


## THANKS TO:
- To Felipe Contreras [felipec](https://github.com/felipec) for providing the initial version (https://gist.github.com/felipec/6169047)
- To Markus Meissner [daringer](https://github.com/daringer) for the asus_fan version using the "thermal_device" interface
- To Bernd Kast [KastB](https://github.com/KastB) for the port using the hwmon interface
- To Frederick Henderson [frederickjh](https://github.com/frederickjh) for ubuntu dkms support
- To [Tharre](https://github.com/Tharre) for the never ending stream of useful hints and information
- Various testers and users providing valuable information and thus increasing the list of compatible devices
