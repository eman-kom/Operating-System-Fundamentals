#!/bin/bash

# Fill the below up
hostname=$(hostname)
machine_hardware=$(uname -sm)
max_user_process_count=$(ulimit -u)
user_process_count=$(ps -x --no-headers | wc -l)
user_with_most_processes=$(ps -eo user | sort | uniq -c | sort -nr | awk 'NR==1{print $2}')
mem_free_percentage=$(cat /proc/meminfo | awk '/MemTotal/ {total = $2}; /MemFree/ {free = $2}; END {if (total == 0) {print 0} else {print free / total * 100}};')
swap_free_percentage=$(cat /proc/meminfo | awk '/SwapTotal/ {total = $2}; /SwapFree/ {free = $2}; END {if (total == 0) {print 0} else {print free / total * 100}};')

echo "Hostname: $hostname"
echo "Machine Hardware: $machine_hardware"
echo "Max User Processes: $max_user_process_count"
echo "User Processes: $user_process_count"
echo "User With Most Processes: $user_with_most_processes"
echo "Memory Free (%): $mem_free_percentage"
echo "Swap Free (%): $swap_free_percentage"
