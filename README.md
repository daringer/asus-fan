asus-fan
========

ASUS  fan(s) control kernel module.
The following Notebooks should be supported - be aware that it's alpha

Single Fan | Two Fans (NVIDIA)
-----------|-------------------
UX21E      | UX32VD
UX31E      | UX42VS
UX21A      | UX52VS
UX31A      | U500VZ
UX32A      | NX500
UX301LA    | UX32LN
UX302LA    |
N551JK     |
N56JN      |

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

- [**thermal_daemon**](https://github.com/01org/thermal_daemon) [**config file(s)**](https://github.com/daringer/asus-fan/tree/master/misc/thermald) may be found in `misc/thermald/` (experimental, not fully finished). To use this config, create control symlinks first using: [misc/create_symlinks.sh](https://github.com/daringer/asus-fan/blob/master/misc/create_symlinks.sh)

- **Max fan speed**- There is an additional file for controling the maximum fan speed. It's r/w and controls both, automatic mode and manual mode maximum speed. Value range: 0-255 reset value:256

**TODOs**:
----------
- do a code review and clean it up
- check with more models and the dual fan models
- add an included fan controller
- submit an upstream patch - any howtos?? wtf, write acpi-devel kernel-mailinglist ??


**THANKS TO**:
--------------
- To Felipe Contreras (felipec) for providing the initial version (https://gist.github.com/felipec/6169047)
- To Markus Meissner (daringer) for the asus_fan version with "thermal" interface - the hwmon version is a port of that one
