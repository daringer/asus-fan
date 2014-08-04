#!/bin/bash 

startpath="/sys/devices/virtual/thermal/"
path=$(grep -r Fan ${startpath}/cooling_device*/type 2> /dev/null | \
	cut -d ":" -f 1 | xargs dirname)

if [[ "$1" = "" ]]; then
  echo "Usage: $0 <action>"
  exit;
fi

if [[ "$1" = "max" || "$1" = "full" || "$1" = "get_max" ]]; then
	cat ${path}/max_state 
elif [[ "$1" = "cur" || "$1" = "get" || "$1" = "current" ]]; then
	cat ${path}/cur_state
elif [[ "$1" = "set" && "$2" != "" ]]; then
	echo $2 > ${path}/cur_state
	echo "Set to 256 to reactivate automatic regulation!"
fi
