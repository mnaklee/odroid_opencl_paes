/*	
 *	util.h
 */

#ifndef _UTIL_H_
#define _UTIL_H_ 

#include <sys/types.h>
#include <unistd.h>

#define TRUE 1
#define FALSE 0 

#define MAX_POWER_MON_PERIODS 5

#define NUM_ELEMS(x) (sizeof(x)/sizeof(x[0]))

/* Arguments for power monitoring thread. */
typedef struct {
        unsigned int n_samples;
		unsigned int interval;
        double *avg_watts;
} powermon_args;

/* Arguments for control thread. */
typedef struct {
        unsigned int interval;
        void (*callback)(void);
} control_thr_args;

void *monitor_power(void *args);
void *periodic_control(void *args);


#endif	/* _UTIL_H_ */
