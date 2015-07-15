#!/bin/bash

cpu_dir="/sys/devices/system/cpu"
set_gov="userspace"
cpu_freq=(1800000, 1000000, 200000)

gpu_dir="/sys/class/misc/mali0/device" 
gpu_freq=(600, 543, 480, 420, 350, 266, 177)

key_size=(128, 192, 256)


#setting file permission and governor 
for file in $cpu_dir/cpu?
do
    if [ -d $file ]
    then
#        chmod a+rwx $file/cpufreq/scaling_governor
#        chmod a+rw $file/cpufreq/scaling_setspeed
        chmod a+rw $file/cpufreq/scaling_max_freq
        chmod a+rw $file/cpufreq/scaling_min_freq
#        echo "$set_gov" | sudo tee $file/cpufreq/scaling_governor
#        cpufreq-set 
    fi
done

chmod a+rw $gpu_dir/dvfs
chmod a+rw $gpu_dir/clock

echo 0 > $gpu_dir/dvfs
