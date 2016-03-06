#!/usr/bin/python2 

import sys 
import os

from subprocess import check_output, CalledProcessError 
from time import sleep

if len(sys.argv) != 3 or sys.argv[1] not in ["1", "2"] or \
        not sys.argv[1].isdigit() or not os.path.exists(sys.argv[2]) or \
        not os.path.isdir(sys.argv[2]):

    exe = os.path.basename(sys.argv[0])
    print 
    print "Usage: python2 " + exe + " <fan count> <hwmon path>"
    print 
    print " <fan count>   => either '1' or '2', if with Optimus."
    print " <hwmon path>  => full path to hwmonX (dir) from the module.." 
    print 
    sys.exit(1)

GFX = True if sys.argv[1] == "2" else False
HWMON_DIR = sys.argv[2]

#######################################
## fan speed positions
FAN1_MSB = (9, 4)  # 0x94
FAN1_LSB = (9, 3)  # 0x93
# only if actually needed
FAN2_MSB = (9, 6)  # 0x96
FAN2_LSB = (9, 5)  # 0x95

def get_acpi():
    """returns ((hex,int), (hex,int)) raw acpi-speed values"""

    # get fan value(s) from acpi-table
    regs = None
    try:
        cmd = ["perl", "acer_ec.pl", "regs"]
        regs = check_output(" ".join(cmd), shell=True).split("\n")[3:]
    except CalledProcessError as e:
        print e
        print "Cannot get ACPI registers... exiting..."
        sys.exit(1)

    for line in regs:
        # find correct line
        if line.startswith( str(FAN1_MSB[0]) ):
            
            # get "left" 8 bytes in line
            line = line.replace("\t", " ").split("|")[1].split(" ")[1:-1]

            # convert/concat decimal-vals (to hex, then concat)
            raw1 = "0x{}{}".format(
                hex(int(line[FAN1_MSB[1]]))[2:].zfill(2),
                hex(int(line[FAN1_LSB[1]]))[2:].zfill(2) )
            raw2 = 0
            if GFX:
                raw2 = "0x{}{}".format(
                    hex(int(line[FAN2_MSB[1]]))[2:].zfill(2),
                    hex(int(line[FAN2_LSB[1]]))[2:].zfill(2) )

            # interpret as number with base 16 
            return ((raw1, int(raw1, 16)), 
                    (raw2, int(raw2, 16) if raw2 != 0 else 0))

def get_rpm(fan1, fan2=None):
    """fan1 (fan2) -> integers, returns RPM(s)"""

    return ((0x0041CDB4 / (fan1*2)),
            ( (0x0041CDB4 / (fan2*2)) if fan2 is not None else 0) )

def get_hwmon():
    """returns the rpm for the fan(s)"""

    fn1 = os.path.join(HWMON_DIR, "fan1_input")
    fn2 = os.path.join(HWMON_DIR, "fan2_input") 

    # be lazy and use cat ;)
    return ( int(check_output("cat " + fn1, shell=True)), 
             int(check_output("cat " + fn2, shell=True)) )

while True:

    print "-"*35 + "collecting fan info:"
    
    vals = get_acpi()
    rpms = get_rpm(vals[0][1], vals[1][1])
    hwrpm = get_hwmon()

    ifgfx = lambda x: x if GFX else ""

    print (" "*12) + "{:>8} {:>8}".format("fan1", ifgfx("fan2"))
    print "{:<11} {:>8} {:>8}".format("acpi  (hex)", vals[0][0], ifgfx(vals[1][0]))
    print "{:<11} {:>8} {:>8}".format("      (dec)", vals[0][1], ifgfx(vals[1][1]))
    print "{:<11} {:>8} {:>8}".format("      (rpm)", rpms[0], ifgfx(rpms[1]))
    print 
    print "{:<11} {:>8} {:>8}".format("hwmon (rpm)", hwrpm[0], ifgfx(hwrpm[1]))
    sleep(1)

