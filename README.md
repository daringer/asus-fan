asus-fan
========

ASUS (Zenbook) fan(s) control and monitor kernel module.

- [Compatibilty](#compatibilty)
- [Quickstart](#quickstart)
- [Short Comparison To Other Similar Projects](#short-comparison-to-other-similar-projects)
- [Tools/Configs/Misc](#tools--configs--simple-fancontrol-script)
- [ToDos](#todos)
- [ThanksTo](#thanks-to)

Compatibilty
------------
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
N551JK     |
N56JN      |

Installation with DKMS
----------------------
Dynamic Kernel Module Support (DKMS) is a program/framework that enables generating Linux kernel modules whose sources generally reside outside the kernel source tree. The concept is to have DKMS modules automatically rebuilt when a new kernel is installed. -  [Wikipedia](https://en.wikipedia.org/wiki/Dynamic_Kernel_Module_Support)

Installing the asus-fan kernel module with DKMS means that when you upgrade to a new kernel you do not need to repeat the process outlined below in the Quickstart section each time. The asus-fan kernel module will automatically be rebuilt when a new kernel is installed.

More information on DKMS: [Ubuntu Help - DKMS](https://help.ubuntu.com/community/DKMS)

As the superuser (root):

    cd /usr/src && \
      wget https://github.com/daringer/asus-fan/archive/master.tar.gz && \
      tar xvf master.tar.gz
    dkms add -m asus_fan -v master
    dkms install -m asus_fan -v master
    echo asus_fan >>/etc/modules

Using sudo:

    cd /usr/src
    sudo wget https://github.com/daringer/asus-fan/archive/master.tar.gz && \
      sudo tar xvf master.tar.gz
    sudo dkms add -m asus_fan -v master
    sudo dkms install -m asus_fan -v master
    sudo echo asus_fan >>/etc/modules

Quickstart
----------

- **Build** - just run ```make``` inside the directory
- **Install** - run ```sudo make install``` inside the directory
- **Load** - simply as usual:
```bash
modprobe asus_fan
```
- **Interface** - the fan(s) is/are exposed as ```hwmon```, thus available in:
```bash
/sys/class/hwmon/hwmonX
```
- **Monitor Fan speed** -simply use xsensors or psensors for graphical, or sensors for console monitoring
```

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


Short Comparison To Other Similar Projects
------------------------------------------
- [asus-fancontrol](https://github.com/nicolai-rostov/asus-fancontrol) userland application, acpi_call based, limited number of (offical/tested) compatible zenbook models, no (standard) interface exposed, no support for second fan
- [zenbook](https://github.com/juyrjola/zenbook) userland appliaction, acpi_call based, only UX32VD supported, no (standard) interface exposed, no support for second fan
- [asusfan](https://github.com/gpiemont/asusfan) kernelspace driver, model support: A8J, A8JS, N50V[cn], no (standard) interface exposed, interface based on module parameters, replaces automatic control with own realization, no support for second fan
- [asusfan (original)](https://code.google.com/p/asusfan/) kernelspace driver, Model support A8J and A8JS, no (standard) interface, automatic fan speed adjustment, no support for second fan
- [asusfanctrld](https://github.com/nflx/asusfanctrld) userland (bash) application, acpi_call  based, only UX32VD, second fan support?, no (standard) interface exposed
- [asus_ux32v_fan_control](https://github.com/chrischdi/asus_ux32v_fan_control) is actually a fork of this project undergone a renaming (hmm), very old state, no hwmon (interface) support, thermal_device interface, most likely as buggy as this project was one year ago
- [asus_wmi](https://github.com/KastB/asus_wmi) realization as a part of the exisiting `asus_wmi` module, which is maybe the only way to submit it for the upstream kernel, nevertheless support for the second fan is not available through wmi. Sister project, closely related to the one shown here.

So this [project](https://github.com/daringer/asus-fan) distinguishes itself from the others by providing a **hwmon standard state-of-art interface** with a wide variety of **supported zenbook models** realized as a **kernelspace driver**. Guess you found the *right* one ;)


Tools / Configs / Simple Fancontrol Script
------------------------------------------

- **fancontrol** - There is a script called "fancontrol" that can be configured according to temperature source, fans to control, minimum and maximum temperature...
thats done by "pwmconfig"
Nevertheless that script did it worse for me than the original controller - thus you can tell it to stop the fan completely...

**TODOs**:
----------
- do a code review and clean it up
- check with more models
- add an included fan controller
- setting 'fanX_max' to 256 in order to reset all values to default, seems not a standard behavior --- where to put this functionality?
- submit an upstream patch - any howtos?? wtf, write acpi-devel kernel-mailinglist ??


**THANKS TO**:
--------------
- To Felipe Contreras [felipec](https://github.com/felipec) for providing the initial version (https://gist.github.com/felipec/6169047)
- To Markus Meissner [daringer](https://github.com/daringer) for the asus_fan version using the "thermal_device" interface
- To Bernd Kast [KastB](https://github.com/KastB) for the port using the hwmon interface
- To [Tharre](https://github.com/Tharre) for the never ending stream of useful hints and information
- Various testers and users providing valuable information and thus increasing the list of compatible devices
