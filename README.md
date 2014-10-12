asus-fan
========

- Build: just run "make" inside the directory
- Install: cp the resulting asus_fan.ko file somewhere below /lib/modules/<your-kernel>/ and run "depmod -a"
- Load: simply "modprobe asus_fan"
- Interface: the fan is exposed as "cooling_device", thus available in /sys/devices/virtual/thermal/cooling_deviceX/
- Find X: the X changes from boot to boot, a small bash script is found inside the repository as a simpler interface
- Read Fan: the files "cur_state", "max_state" provide the obvious, ranging from 0 - 255
- Set Fan: write anything from 0 to 255 to "cur_state", like: "echo 123 > cur_state"
- ATTENTION: the fan is now in manual mode - do not burn your machine!
- Set Auto-Fan(s): to reactivate the automatic fan control write "256" to cur_state: "echo 256 > cur_state"



TODOs:
- expose two cooling_devices, if a second fan exists (Zenbooks with an NVIDIA GPU)
- expose fan RPMs (once I get to know where, cooling_device api allows only cur/max_state)
- read and set threshold for maximum speed while automatically controlled


THANKS:
- To Felipe Contreras (felipec) for providing the initial version (https://gist.github.com/felipec/6169047)
