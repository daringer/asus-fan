#!/bin/bash 
#
# ---------- asus-fan quick testing script ----------
#
# -> uses sudo for root actions
# -> flush/delete dmesg log contents
# -> remove module, if currently listed in lsmod
# -> modprobe module and start testing ...
#
# 
# THIS SCRIPT PARTLY EXECUTES ROOT COMMANDS USING 'sudo', TO THE BEST KNOWLEDGE
# OF THE AUTHOR THE AS ROOT EXECUTED COMMANDS ARE SANE. NEVERTHELESS, THERE MAY
# BE BUGS, SO BE AWARE THAT: THE AUTHOR IS NOT RESPONSIBLE FOR ANY DAMAGE CAUSED 
# TO YOUR COMPUTER OR SYSTEM! 
#
# USE AT YOUR OWN RISK! 
# - to print, instead of executing the sudo commands, 
#   just swap the commented lines in function sudo() 
# - try commenting the second line in function bad() to NOT exit on error!
#   - this will lead to a wrong result collection at the end of the script!

#function sudo() {
  #echo SUDO EXECUTE: "$@"
#}

function good() {
  echo [+++] $@
  return 0
}

function ok() {
  echo [+] $@
  return 0
}

function bad() {
  echo [---] $@
  exit 1
}

function info() {
  echo [i] $@
  return 0
}

function callres() {
  if [[ "$1" != "0" ]]; then 
	return 1
  else 
        return 0
  fi
}

function res2desc() {
  d=""
  if [[ "$1" = "1" ]]; then 
	  d="BAD"
  else
	  d="GOOD"
  fi
  shift

  printf '[RESULT]  %25s  ->  %s\n' "$@" $d
  #printf '%s\n' $d
  
}

_h=" ------------"
modname="asus-fan"
realmod="asus_fan"
hasfan2=0

# set this to change stepping during manual speed set test
USER_STEPPING=15

r_housekeeping=0
r_dmesginspection=0
r_searchsysfiles=0
r_testfan1func=0
r_testfan2func=0


info "$_h Starting '$modname' kernel module quick test"

( lsmod | grep $realmod &> /dev/null ) && 
    ( ok "$modname loaded! REMOVING (rmmod) $modname ($realmod) module";
      sudo rmmod $realmod )

info "Clearing 'dmesg' (kernel) log"
sudo dmesg -c > /dev/null

info "Trying to modprobe the module: '$realmod'"
sudo modprobe $realmod
if callres $?; then
	good "Successfully inserted module: '$realmod'"
else
	bad "Could not modprobe the module: '$realmod'"
	r_housekeeping=1
fi


echo
info "$_h Inspecting dmesg a.k.a. kernel log"
q="finished init"
if (dmesg | grep $modname | grep "$q" &> /dev/null); then
	good "'dmesg' reports finished module init"
else 
	bad "'dmesg' reports no finished module init!"
	r_dmesginspection=1
fi

if (dmesg | grep $modname | grep -v "$q" &> /dev/null); then
	info "Remaining dmesg lines with related information:"
	dmesg | grep $modname | grep -v "$q" 
fi 

echo
info "$_h Search /sys files"
rpath="/sys/devices/platform/asus_fan"
hpath=$(eval echo $rpath/hwmon/hwmon*)
f1_paths="$rpath $hpath $hpath/fan1_{input,label,min,speed_max} $hpath/{pwm1,pwm1_enable}"
f2_paths="$hpath/fan2_{input,label,min} $hpath/{pwm2,pwm2_enable}"

foundall_1=1
foundall_2=1
cur_fan=1
info "Checking for fan1 /sys/ files:"
for p in $f1_paths next $f2_paths; do 
	if [[ "$p" = "next" ]]; then
		cur_fan=2
		info "Checking for fan2 /sys/ files:"
		continue
	fi
	all=`eval echo $p 2> /dev/null`
	if [[ "$?" = "1" ]]; then 
		bad "Could not expand '$p'"
	        r_searchsysfiles=1
		continue
	fi

	for subp in $all; do 
		curp=$subp
		[[ "${curp:0:1}" != "/" ]] && curp=${curp} 

		if (stat $curp &> /dev/null); then
			ok "Found /sys path ($curp)"
		else 
			bad "Could not find the correct /sys path! ($curp)"
			[[ "$cur_fan" = "1" ]] && foundall_1=0
			[[ "$cur_fan" = "2" ]] && foundall_2=0
			r_searchsysfiles=1
		fi
	done
done

[[ "$foundall_1" = "1" ]] && good "All /sys files found for primary fan (no: 1)"
[[ "$foundall_2" = "1" ]] && 
          good "All /sys files found for secondary fan (no: 2)" &&
	  hasfan2=1 &&
	  info "Looks like this machine has two fans!"

function test_fan() {
   name=fan${1}

   pwm=$hpath/pwm${1}
   pwm_en=$hpath/pwm${1}_enable
   currpm=$hpath/${name}_input
   minspeedfn=$hpath/${name}_min
   label=`cat $hpath/${name}_label`

   callres $? || bad "Could not read ${name} label"


   info "Checking ${name} functionality! (label: '$label')"
   
   # read speed
   curspeed=`cat $currpm 2> /dev/null`
   changing=0
   if callres $?; then 
	   good "Reading fan speed: $curspeed" 
	   last=$curspeed
	   for i in `seq 10`; do
   	         curspeed=`cat $currpm 2> /dev/null`
	         if [[ "$curspeed" != "$last" ]]; then
			 good "${name} speed is updated (changed)"
			 ok "Change found in try no: $i! "
			 changing=1
			 break
		 fi 
		 sleep 1
	   done
	   if [[ "$changing" = "0" ]]; then 
	       bad "Looks like the ${name} speed is not updated ... ($currpm)"
	       ([[ "$1" = "1" ]] && r_testfan1func=1) || r_testfanfunc2=1
	   fi
   else
	   bad "Could not read ${name} speed!"
	   ([[ "$1" = "1" ]] && r_testfan1func=1) || r_testfanfunc2=1
   fi

   # read auto-mode 
   curmode=`cat $pwm_en 2> /dev/null`
   callres $? || bad "Could not read $pwm_en"
   if [[ "$curmode" = "0" ]]; then 
	   ok "Could read and verify (default) activated auto-mode: ${curmode}"
   else
	   bad "Could not verify (default) actived auto-mode: ${curmode}"
	   ([[ "$1" = "1" ]] && r_testfan1func=1) || r_testfanfunc2=1
   fi

   # read min-speed 
   mspeed=`cat ${minspeedfn}`
   callres $? || bad "Could not read ${name} min speed ($minspeedfn}"
   if [[ "$minspeed" = "" ]]; then 
       ok "Successfully read the min speed for ${name}: $mspeed"
   else 
       bad "Could not read min speed, file empty? tried to read: $minspeedfn"
       ([[ "$1" = "1" ]] && r_testfan1func=1) || r_testfanfunc2=1
   fi

   # set speed manually | auto-mode state change | read speed during manual mode 
   step=$USER_STEPPING
   info "Set fan speed manually starting with ${name}_min ($mspeed) using ${step} as stepping"
   info "You should hear/feel the changing $name speed!"
   maxspeed=255
   nowspeed=`cat $hpath/${name}_input`
   callres $? || bad "Could not read ${name}'s current fan speed"
   lastspeed=""
   setspeed="0"
   sleep_duration=0.30

   speed_read_fail=0
   speed_set_fail=0
   auto_mode_fail=0
   auto_mode_start_fail=0
   echo -n "[i] Set manual speed to: "
   while ((setspeed < maxspeed)); do
       nowspeed=`cat $hpath/${name}_input`
       callres $? || bad "Could not read ${name} speed from $hpath/${name}_input"
       [[ "$nowspeed" = "$lastspeed" ]] && speed_read_fail=1
         
       # calc target manual speed
       ((setspeed = setspeed + step))
       ((setspeed > maxspeed)) && setspeed=$maxspeed 
       ((setspeed < mspeed)) && setspeed=$mspeed 

       sudo bash -c "echo $setspeed > $pwm"
       echo -n "$setspeed "
       (((setspeed / step) % 10 == 0)) && echo && echo -n "                         "
       lastspeed=$nowspeed
       sleep $sleep_duration
   done 
   echo

   # check for manual mode flag
   [[ `cat $pwm_en` = "1" ]] || auto_mode_fail=1

   # resetting to auto-mode
   sudo bash -c "echo 0 > $pwm_en" 
   [[ `cat $pwm_en` = "0" ]] || auto_mode_start_fail=1

   # rpm read failed
   if [[ "$speed_read_fail" = "1" ]]; then
	   bad "Could not (always?) get $name speed during manual speed set" 
	   ([[ "$1" = "1" ]] && r_testfan1func=1) || r_testfanfunc2=1
   else 
	   good "Getting speed during manual mode for $name successfully done"
   fi 

   # manual set failed
   if [[ "$speed_set_fail" = "1" ]]; then
	   bad "Could not (always?) set $name speed!"
   else 
	   good "Setting manual speeds for $name successful!"
   fi

   # auto-mode behavior fail
   if [[ "$auto_mode_fail" = "1" ]]; then
	   bad "Auto-mode ($pwm_en) does not behave as excepted"
	   ([[ "$1" = "1" ]] && r_testfan1func=1) || r_testfanfunc2=1
	   info "       Has not changed in manual mode from 0 to 1"
   else 
	   ok "Auto-mode ($pwm_en) flag behaves as intended for $name!"
   fi

   # auto-mode resetting fail
   if [[ "$auto_mode_start_fail" = "1" ]]; then
	   bad "Auto-mode ($pwm_en) could not be resetted!"
	   ([[ "$1" = "1" ]] && r_testfan1func=1) || r_testfanfunc2=1
   else 
	   ok "Auto-mode ($pwm_en) was successfully reseted!"
   fi

}

echo 
info "$_h Test functionality"

test_fan 1
[[ "$hasfan2" = "1" ]] && test_fan 2

info $_h
info "$_h Final result overview:" 
res2desc $r_housekeeping "Houskeeping"
res2desc $r_dmesginspection "DMESG inspection"
res2desc $r_searchsysfiles "/sys files available"
res2desc $r_testfan1func "Functionality Fan 1"
res2desc $r_testfan2func "Functionality Fan 2"

