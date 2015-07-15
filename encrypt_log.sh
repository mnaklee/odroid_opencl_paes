#!/bin/bash

cpu_dir="/sys/devices/system/cpu"
set_gov="userspace"
cpu_freq=(1400000 1000000 200000)

gpu_dir="/sys/class/misc/mali0/device/clock" 
gpu_freq=(600 543 480 420 350 266 177)

key_size=(128 192 256)


#setting file permission and governor 
for set_cpu_freq in $cpu_freq
do
    for cpu_num in $cpu_dir/cpu?
    do  
        echo $set_cpu_freq
        echo $set_cpu_freq > $cpu_num/cpufreq/scaling_min_freq
        echo $set_cpu_freq > $cpu_num/cpufreq/scaling_max_freq
    done

    for set_gpu_freq in $gpu_freq
    do
        echo $set_gpu_freq
        echo $gpu_dir
        echo $set_gpu_freq > $gpu_dir

        for set_key_size in $key_size
        do
           echo "./paes/paes -i ./paes/data/samples.txt -o ./paes/output/output -m encrypt -d gpu -k $set_key_size -p odroid > log_cpu$set_cpu_freq"_gpu"$set_gpu_freq"_key"$set_key_size"
           /home/odroid/task/openclqos/paes/paes "-i" /home/odroid/task/openclqos/paes/data/samples.txt "-o" /home/odroid/task/openclqos/paes/output/output "-m" encrypt "-d" gpu "-k" $set_key_size "-p" odroid > log_cpu$set_cpu_freq"_gpu"$set_gpu_freq"_key"$set_key_size
            echo $set_key_size
        done
    done
done
