#!/bin/bash

cpu_dir="/sys/devices/system/cpu"
set_gov="userspace"
cpu_freq=(1400000 1000000 200000)

gpu_dir="/sys/class/misc/mali0/device/clock" 
gpu_freq=(600 543 480 420 350 266 177)

key_size=(128 192 256)


#setting file permission and governor 
for ((i=0;i<3;i++))
do
    for cpu_num in $cpu_dir/cpu?
    do  
        echo $cpu_freq
        echo ${cpu_freq[$i]} > $cpu_num/cpufreq/scaling_min_freq
        echo ${cpu_freq[$i]} > $cpu_num/cpufreq/scaling_max_freq
    done

    for ((j=0;j<7;j++))
    do
        echo ${gpu_freq[$j]}
        echo $gpu_dir
        echo ${gpu_freq[$j]} > $gpu_dir

#        for set_key_size in $key_size
        for ((k=0;k<3;k++))
        do
           ./paes -i ./data/samples.txt -o ./output/output -m encrypt -d gpu -k ${key_size[$k]} -p odroid > ./log_wb/log_wb_cpu${cpu_freq[$i]}"_gpu"${gpu_freq[$j]}"_key"${key_size[$k]}
           echo "./paes -i ./data/samples.txt -o ./output/output -m encrypt -d gpu -k ${key_size[$k]} -p odroid > ./log_wb/log_wb_cpu${cpu_freq[$i]}"_gpu"${gpu_freq[$j]}"_key"${key_size[$k]}"
        done
    done
done

for cpu_num in $cpu_dir/cpu?
do  
    echo $cpu_freq
    echo 200000 > $cpu_num/cpufreq/scaling_min_freq
    echo 1400000 > $cpu_num/cpufreq/scaling_max_freq
done
