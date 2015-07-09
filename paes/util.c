
/*
 * util.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>

#include "util.h"

#include "hidapi.h"

extern int end_point;

void * monitor_power(void *args)
{
	hid_device *device;
	unsigned char buf[65];

	unsigned int n_samples;
	double *avg_watts;
	unsigned int interval;
	powermon_args *pa;

	static double watts[MAX_POWER_MON_PERIODS];
	static int cur_watt_ptr=0;
	
	pa = (powermon_args *) args;
	n_samples = pa->n_samples;
	avg_watts = pa->avg_watts;
	interval = pa->interval;

	assert( n_samples > 0);
	assert(interval >0);

	buf[0]=0x00;
	memset((void*)&buf[2],0x00,sizeof(buf)-2);
	device = hid_open(0x04d8, 0x003f, NULL);
	
	if (device){
		hid_set_nonblocking(device, 1);
	} else {
		printf("no device\n");
		exit(1);
	}

	for (;end_point != TRUE;){
		buf[1] = 0x37;
		double sum=0;
		unsigned int i;

		if (hid_write(device, buf, sizeof(buf)) == -1){
			printf("write device error\n");
			exit(1);
		}
		if (hid_read(device, buf, sizeof(buf)) == -1){
			printf("read device error\n");
			exit(1);
		}
		if (buf[0] == 0x37){
			char s[7]={'\0'};
			strncpy(s, (char*)&buf[17],5);
			watts[cur_watt_ptr++] = atof(s);
			cur_watt_ptr=cur_watt_ptr % n_samples;
		}
		for (i=0; i < n_samples;i++){
			sum = sum+ watts[i];
		}
		*avg_watts = (double) sum / n_samples;

		sleep(interval);
	}
} 

void *periodic_control(void *args)
{
	control_thr_args *ca;
	unsigned int interval;
	void (*callback)(void)=NULL;

	ca = (control_thr_args *) args;
	interval = ca->interval;
	callback = ca->callback;
	
	assert(interval > 0);

	for (;end_point != TRUE;){
		if (callback != NULL){
			(*callback)();
		}
		sleep(interval);
	}
} 
