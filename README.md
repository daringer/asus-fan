asus-fan
========

ASUS (Zenbook) fan(s) control kernel module.
The following Zenbooks are supported:

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
- **Interface** - the fan(s) is/are exposed as ```cooling_device```, thus available in:
```bash
/sys/devices/virtual/thermal/cooling_deviceX/
```
- **Find X** - the ```X``` changes from boot to boot, a small bash script is found inside
  the repository as a simpler interface:
```bash
basepath=/sys/devices/virtual/thermal/
fpath=$(grep -r '^Fan$' ${basepath}/cooling_device*/type 2> /dev/null | \
        cut -d ":" -f 1 | xargs  --no-run-if-empty dirname)
gfxfanpath=$(grep -r '^GFX Fan$' ${basepath}/cooling_device*/type 2> /dev/null | \
        cut -d ":" -f 1 | xargs  --no-run-if-empty dirname)
```
- **Read Fan** - the files ```cur_state```, ```max_state``` provide the obvious, ranging from 0 - 255:
```bash
cat ${fpath}/cur_state          # get current fan speed
cat ${fpath}/max_state          # get max fan speed
```
- **Set Fan Speed** - write anything from 0 to 255 to ```cur_state```, like:
```bash
echo 123 > ${fpath}/cur_state   # set to 123
echo 0 > ${fpath}/cur_state     # switch fan off (DANGEROUS!)
echo 255 > ${fpath}/cur_state   # set to max speed
```
- **ATTENTION** - the fan is now in manual mode - do not burn your machine!
- **Set Auto-Fan(s)**: to reactivate the automatic fan control write "256" to ```cur_state```:
```bash
echo 256 > ${fpath}/cur_state   # reset to auto-mode (always for all fans)
```

**TODOs**:
----------
- expose fan RPMs (once I get to know where, cooling_device api allows only cur/max_state)
- read and set threshold for maximum speed while automatically controlled (actually works, but have no idea how to interface with the userland, thermal_cooling_device_* function family seems to not provide anything beside cur-/max_state files inside /sys/...
- submit an upstream patch - any howtos?? wtf, write acpi-devel kernel-mailinglist ??


**THANKS TO**:
--------------
- To Felipe Contreras (felipec) for providing the initial version (https://gist.github.com/felipec/6169047)
