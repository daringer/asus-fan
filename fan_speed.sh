#!/bin/bash 

startpath="/sys/devices/virtual/thermal/"
path1=$(grep -r '^Fan$' ${startpath}cooling_device*/type 2> /dev/null | \
	cut -d ":" -f 1 | xargs --no-run-if-empty dirname)

path2=$(grep -r '^GFX Fan$' ${startpath}cooling_device*/type 2> /dev/null | \
	cut -d ":" -f 1 | xargs --no-run-if-empty dirname)

function usage() {
  echo "Usage: `basename $0` [fan index] <action>"
  echo ""
  echo "<< Arguments help >>"
  echo "" 
  echo "  [fan index]: (optional)" 
  echo "  - defaults to 0 (regular, system fan index)"
  echo "  - if gfx-fan is available, set to '1' to use gfx-fan"
  echo "  <action>: (mandatory)"
  echo "  - get / cur[rent]      => return current speed"
  echo "  - set <value>          => set speed (0-255)"
  echo "                         => set 256 to reactivate 'auto'-mode"
  echo "  - [get_]max            => returns maximum speed for fan"
}


if [[ "$1" = "" ]]; then
	usage
  	exit 1;
fi

# catch gfx fan switch
path=""
if [[ "$1" = "1" ]]; then
	path="${path2}"
	shift
elif [[ "$1" = "0" ]]; then
	path="${path1}"
	shift
else
	path="${path1}"
fi



if [[ "$1" = "max" || "$1" = "full" || "$1" = "get_max" ]]; then
	cat ${path}/max_state 
elif [[ "$1" = "cur" || "$1" = "get" || "$1" = "current" ]]; then
	cat ${path}/cur_state
elif [[ "$1" = "set" && "$2" != "" ]]; then
	echo $2 > ${path}/cur_state
else 
	echo "Could not understand arguments - exiting..."
	usage
	exit 1;
fi 

exit 0;
