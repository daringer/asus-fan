asus-fan  --  ASUS (Zenbook) fan controller kernel module
=========================================================

Quickstart
----------

- **Build** - just run "make" inside the directory
- **Install** - copy the resulting asus_fan.ko file somewhere below 
  /lib/modules/<your-kernel>/ and rebuild module deps:
```bash
cp asus_fan.ko /lib/modules/$(uname -r)/
depmod -a
```
- **Load** - simply as usual: 
```bash
modprobe asus_fan
```
- **Interface** - the fan is exposed as "cooling_device", thus available in:
```bash
/sys/devices/virtual/thermal/cooling_deviceX/
```
- **Find X** - the X changes from boot to boot, a small bash script is found inside 
  the repository as a simpler interface, the directory is easy to get though:
```bash
basepath=/sys/devices/virtual/thermal/
fpath=$(grep -r Fan ${basepath}/cooling_device*/type 2> /dev/null | \
        cut -d ":" -f 1 | xargs dirname)
```
- **Read Fan** - the files "cur_state", "max_state" provide the obvious, ranging from 0 - 255:
```bash
cat ${fpath}/cur_state          # get current fan speed
cat ${fpath}/max_state          # get max fan speed
```
- **Set Fan Speed** - write anything from 0 to 255 to "cur_state", like: 
```bash
echo 123 > ${fpath}/cur_state   # set to 123
echo 0 > ${fpath}/cur_state     # switch fan off (DANGEROUS!)
echo 255 > ${fpath}/cur_state   # set to max speed
```
- **ATTENTION** - the fan is now in manual mode - do not burn your machine!
- **Set Auto-Fan(s)**: to reactivate the automatic fan control write "256" to cur_state: 
```bash
echo 256 > ${fpath}/cur_state   # reset to auto-mode
```


**TODOs**:
----------
- expose two cooling_devices, if a second fan exists (Zenbooks with an NVIDIA GPU)
- expose fan RPMs (once I get to know where, cooling_device api allows only cur/max_state)
- read and set threshold for maximum speed while automatically controlled
- submit an upstream patch - any howtos ??? 


**THANKS TO**:
--------------
- To Felipe Contreras (felipec) for providing the initial version (https://gist.github.com/felipec/6169047)
