asus-fan
========

ASUS  fan(s) control kernel module.
The following Notebooks are supported:

Single Fan | Two Fans (NVIDIA)
-----------|-------------------
UX21E      | UX32VD
UX31E      | UX42VS
UX21A      | UX52VS
UX31A      | U500VZ
UX32A      | NX500
UX301LA    |
UX302LA    |
N551JK     |
N56JN      |

Quickstart
----------

- **Build** - just run ```make``` inside the directory
- **Install** - copy the resulting ```asus_fan.ko``` file somewhere below
  ```/lib/modules/$(uname -r)/``` and rebuild module dependencies:
```bash
cp asus_fan.ko /lib/modules/$(uname -r)/
depmod -a
```
- **Load** - simply as usual:
```bash
modprobe asus_fan
```
- **Interface** - the fan(s) is/are exposed as ```hwmon```, thus available in:
```bash
/sys/class/hwmon/hwmonX
```
- **Monitor Fan speed** -simply use xsensors for graphical, or sensors for console monitoring
```

- **Set Fan Speed** - write anything from 0 to 255 to ```pwmX```, like:
```bash
echo 123 > ${fpath}/pwmX   # set to 123
echo 0 > ${fpath}/pwmX     # switch fan off (DANGEROUS!)
echo 255 > ${fpath}/pwmX   # set to max speed
```
- **ATTENTION** - the fan is now in manual mode - do not burn your machine!
- **Set Auto-Fan(s)**: to reactivate the automatic fan control write "256" to ```cur_state```:
```bash
echo 256 > ${fpath}/pwmX   # reset to auto-mode (always for all fans)
```

- **fancontrol** - There is a script called "fancontrol" that can be configured according to temperature source, fans to control, minimum and maximum temperature...
thats done by "pwmconfig"
Nevertheless that script did it worse for me than the original controller - thus you can tell it to stop the fan completely...

**TODOs**:
----------
- read and set threshold for maximum speed while automatically controlled (actually works, but file has to be added to expose to userland)
- submit an upstream patch - any howtos?? wtf, write acpi-devel kernel-mailinglist ??


**THANKS TO**:
--------------
- To Felipe Contreras (felipec) for providing the initial version (https://gist.github.com/felipec/6169047)
-To Markus Meissner (daringer) for the asus_fan version with "thermal" interface - the hwmon version is a port of that one
