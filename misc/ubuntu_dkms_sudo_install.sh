#!/bin/bash
# This is a script to install the asus-fan module from github using DKMS. https://github.com/daringer/asus-fan - Frederick Henderson
PS4=':${LINENO} + '
#set -x

# Console colors
red='\033[0;31m'
green='\033[0;32m'
yellow='\033[1;33m'
NC='\033[0m'

# function to run commands as super user. This will keep give the user
# option to re-enter the password till they get it right or allow them
# to exit if something goes wrong instead of continuing on. 
# Usage:
#  run_run_sudo_command_command [COMMANDS...] -FJH 2010.03.17
function run_sudo_command() {
# grab the commands passed to the function and put them in a variable for safe keeping
echo -en "${red}"
sudocommand=$*
sudo $sudocommand
# Check the exit status if it is not 0 (good) then assume that the password was not entered correctly and loop them till they get it right or cancel the running of this script.
echo -en "${NC}"
while [ ! $? = 0 ]; do
echo -e -n "${yellow}>>>${NC} Something is not right here. (Did you correctly enter your password? Is the Caps-Locks on?) Do you want to try to enter the password again${green}(Yes)${NC} or exit this script${red}(No)${NC}?"
  read -r response
response=${response,,}    # tolower
  if [[ $response !=  "y" && $response != "Y"  && $response != "yes" && $response != "Yes" ]]; then
    echo -e "${yellow}>>>${NC} ${red}Exiting! User aborted the script!${NC}"
		exit
		else
		echo -en ${red}
		sudo $sudocommand
		echo -en "${NC}"
	fi
	echo -en "${NC}"
done
}
function checkforasusfanmodule(){
  echo -e "${yellow}>>>> ${NC}Checking setup . . ."
  # Is the module loaded?
  ismoduleloaded=$(lsmod 2>/dev/null |grep asus_fan >/dev/null)$?
  [ $ismoduleloaded = 0 ] && echo -e "${yellow}>>>> ${NC}asus_fan module is currently loaded."
  # Was the module previously installed manually?
  wasmoduleloadedmanually=$(ls /lib/modules/$(uname -r)/kernel/drivers/thermal/asus_fan.ko &>/dev/null)$?
  [ $wasmoduleloadedmanually = 0 ] && echo -e "${yellow}>>>> ${NC}The asus_fan module was loaded manually for this kernel."
  # Was the module loaded with DKMS?
  #ls /lib/modules/3.13.0-79-generic/updates/dkms/asus_fan.ko &>/dev/null
  wasmoduleloadewithdkms=$(dkms status asus_fan |grep asus_fan &>/dev/null)$?
  [ $wasmoduleloadewithdkms = 0 ] && echo -e "${yellow}>>>> ${NC}The asus_fan module was loaded with DKMS."
}
function renamemanuallyinstalledmodule(){
  echo -e "${yellow}>>>> ${NC}Will now rename manually installed module by adding .OLD to it."
  run_sudo_command mv /lib/modules/$(uname -r)/kernel/drivers/thermal/asus_fan.ko /lib/modules/$(uname -r)/kernel/drivers/thermal/asus_fan.ko.OLD
  echo -e "${yellow}>>>> ${NC}Renamed to ${yellow}/lib/modules/$(uname -r)/kernel/drivers/thermal/asus_fan.ko.OLD${NC}."
}
function removeasusfandkms(){
  echo -e "${yellow}>>>> ${NC}Now removing previous install of asus_fan via DKMS."
  run_sudo_command dkms remove asus_fan/master --all
  run_sudo_command sudo rm -rf /usr/src/asus_fan*
}
function unloadasufanmodule(){
  echo -e "${yellow}>>>> ${NC}Now removing running asus_fan module."
  run_sudo_command rmmod asus_fan
}
function downloadandunpack(){
echo -e "${yellow}>>>> ${NC}I will now download and install the asus_fan module. I will need super user permissions. You will be asked for the sudo password."
  cd /usr/src
echo -e "${yellow}>>>> ${NC}Now downloading to module src to /usr/scr."
run_sudo_command wget -O asus_fan.tar.gz https://github.com/frederickjh/asus-fan/tarball/master
run_sudo_command mkdir asus_fan-master
cd asus_fan-master
run_sudo_command tar xpvf ../asus_fan.tar.gz --strip-components=1
run_sudo_command mv dkms.conf dkms.conf.archlinux # Rename Archlinux version to get it out of the way
run_sudo_command mv dkms-ubuntu.conf dkms.conf # Rename Ubuntu version to the correct name
cd ..
}
function installsymlinkcreationscript(){
  if [ ! -f  /usr/local/sbin/asus-fan-create-symlinks.sh ]; then
    run_sudo_command cp misc/asus-fan-create-symlinks.sh /usr/local/sbin/
  fi
}
function buildmodule(){
  echo -e "${yellow}>>>> ${NC}Now building the asus_fan module."
  run_sudo_command dkms add -m asus_fan -v master
  run_sudo_command dkms install -m asus_fan -v master
}
function addtoetcmodules(){
  echo -e "${yellow}>>>> ${NC}Now adding asus_fan module to /etc/modules if not found."
  # If asus_fan is not found in /etc/modules then add it.
  cat  /etc/modules | grep asus_fan >/dev/null ||  echo asus_fan | run_sudo_command tee -a /etc/modules
}
function modprobing(){
  echo -e "${yellow}>>>> ${NC}Now modprob-ing to load module without restart."
  # Get the module up and running without a restart.
  run_sudo_command modprobe asus_fan
}
function finished(){
  echo ""
  echo -e "${yellow}>>> ${NC}All done setting up the ${yellow}asus-fan module${NC}."
  echo -e "${yellow}>>> ${NC}If ${yellow}psensor${NC} was running you will need to restart it to see fan speed."
  echo -e "${yellow}>>> ${NC}It is advisable to restart your computer."
  echo -e "${yellow}>>> ${NC}If the fan speed does not seem to change after rebooting, restart thermald with ${red}sudo restart thermald${NC}."
  echo -e "${yellow}>>> ${NC}${green}Hopefully you will now have a ${yellow}cool${green} running ${red}Asus laptop!${NC}"
}

### MAIN PROGRAM ####

checkforasusfanmodule
[ $wasmoduleloadewithdkms = 0 ] && removeasusfandkms
downloadandunpack
installsymlinkcreationscript
[ $wasmoduleloadedmanually = 0 ] && renamemanuallyinstalledmodule
buildmodule
addtoetcmodules
[ $ismoduleloaded = 0 ] && unloadasufanmodule
modprobing
finished
