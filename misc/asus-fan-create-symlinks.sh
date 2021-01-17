#!/bin/bash 
#
# - Create 'human readable' symlinks inside arbitrary directory passed 
#   as 1st commandline argument.
#
# - Solves the 'hwmon' module load order dependence problem 
#

d="/tmp/asus-fan-shm"
function usage() {
   echo "Usage: `basename $0` [target_directory] [-h]"
   echo ""
   echo "target_directory default: ${d}"
   exit;
}

[[ "$1" = "-h" || "$1" = "--help" ]] && usage
[[ "$1" != "" ]] && d="$1"

echo "[i] starting, target directory: ${d}"

hwmon_dev=$(dmesg | grep asus-fan | grep hwmon | tail -n 1 | cut -d ":" -f 2 | sed -e 's/ //g')
hwmon_dir="/sys/devices/platform/asus_fan/hwmon/${hwmon_dev}"

mkdir -p ${d}

############### find correct hwmon paths and set links from shm acordingly 
# cleanup
rm -f ${d}/*_temp ${d}/fan_* 

# temps
ln -s /sys/devices/virtual/hwmon/hwmon*/temp1_input ${d}/tz1_temp
ln -s /sys/devices/platform/asus-nb-wmi/hwmon/hwmon*/temp1_input ${d}/tz2_temp
ln -s $(find /sys/devices/platform/coretemp.0/ -name "temp1_input") ${d}/die_temp
ln -s $(find /sys/devices/platform/coretemp.0/ -name "temp2_input") ${d}/core1_temp
ln -s $(find /sys/devices/platform/coretemp.0/ -name "temp3_input") ${d}/core2_temp

# fan interface
[ -r ${hwmon_dir}/pwm1 ] && \
	ln -s ${hwmon_dir}/pwm1 ${d}/fan_cpu_speed
[ -r ${hwmon_dir}/pwm2 ] && \
	ln -s ${hwmon_dir}/pwm2 ${d}/fan_gfx_speed

[ -r ${hwmon_dir}/pwm1_enable ] && \
	ln -s ${hwmon_dir}/pwm1_enable ${d}/fan_cpu_manual_mode
[ -r ${hwmon_dir}/pwm2_enable ] && \
	ln -s ${hwmon_dir}/pwm2_enable ${d}/fan_gfx_manual_mode

# fan inputs ( -> pwn1 )
[ -r ${hwmon_dir}/fan1_input ] && \
	ln -s ${hwmon_dir}/fan1_input ${d}/fan_cpu_rpm
[ -r ${hwmon_dir}/fan2_input ] && \
	ln -s ${hwmon_dir}/fan2_input ${d}/fan_gfx_rpm

# gfx temp ( temp1_input )
[ -r ${hwmon_dir}/temp1_input ] && \
	ln -s ${hwmon_dir}/temp1_input ${d}/gfx_temp

echo "[+] finished...."
