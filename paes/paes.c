/*
    PAES - Parallel AES for CPUs and GPUs
    Copyright (C) 2009  Paolo Bernardi <paolo.bernardi@gmx.it>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/** 
 * \file paes.c
 *
 * This is the main program; basically it contains only the \ref main function
 * and some command line parsing code.
 * Every function that's not necessarily tied with the user interface is defined
 * in the \ref paes_functions.h file.
 */

#define _POSIX_C_SOURCE 199309

#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "paes_constants_and_datatypes.h"
#include "paes_functions.h"
#include "sha256.h"

#include "paes_size.h"
#include "util.h"

#define TIMER_SIG SIGRTMIN
//#define TIMER_SIG SIGALRM
/**
 * The default key size in bits, if not specified by the user
 */
const unsigned short default_key_size_bits = 128;


int end_point=FALSE;
//int thread_select=0;
char *input_file;

// statistics
double avg_watts=0;
double total_resp_time=0;
double cnt_resp_time=0;
int current_clock=0;


//structure using thread function
typedef struct _thread_data
{
	cl_uchar *buffer;
	aes_mode mode;
} Thdata;

//init mutex
pthread_mutex_t mutex_lock1 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t thread_cond1 = PTHREAD_COND_INITIALIZER;

/**
 * Shows some short program usage instructions and exits
 * \param argv the value of command line arguments, including the executable file itself
 */

//thread func
void *read_thread(void *parms)
{
	//wchkang
	struct itimerspec ts;
	struct sigevent sev;
	timer_t tid;
	int file_cnt=0;
	char decrypt_file_name_buf[200]="";
	
        sigset_t        sigset;
        int             sig_no=0;
        FILE * fp;
	struct stat file_stat;

//	char *dir_path = NULL;
	char dir_path[200] = "";
	char dir_path_buf1[200] = "";
	char *dir_path_buf2 = NULL;
	char * token=NULL;
	
	struct dirent * dir_ent;
	DIR *dp;
	
	int count=0;
	//parms -> Thdata sturcture
	Thdata *buf = (Thdata*)parms;
	aes_mode mode = buf->mode;
	//signal configure
	sigemptyset(&sigset);
	sigaddset(&sigset, TIMER_SIG);
	sigaddset(&sigset, SIGALRM);

	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = TIMER_SIG;

	ts.it_interval.tv_sec=0;
	ts.it_interval.tv_nsec=6000000;
	ts.it_value.tv_sec=1;
	ts.it_value.tv_nsec=0;

	if (timer_create(CLOCK_REALTIME, &sev, &tid) == -1){
		perror("timer_create");
		exit(1);
	}
	if (timer_settime(tid, 0, &ts, NULL) == -1){
		perror("timer_settime");
		exit(1);
	}
		
	if(mode==AES_MODE_DECRYPT)
	{

		strcpy(dir_path_buf1, input_file);

		token = strtok(dir_path_buf1,"/");
		
		while(token != NULL)
		{
			dir_path_buf2 = token;
			token = strtok(NULL, "/");	
		}

		strncpy(dir_path,input_file,sizeof(char)*strlen(input_file)-sizeof(char)*strlen(dir_path_buf2));
		
		
		if((dp = opendir(dir_path)) == NULL)
		{
			fprintf(stderr,"opendir error!\n");
			exit(1);
		}
		while(1)
		{
            if (sigwait(&sigset, &sig_no) != 0)
                fprintf(stderr, "Failed to wait for next clock tick\n");
			count++;
			if((dir_ent = readdir(dp)) == NULL)
			{
				fclose(fp);
				end_point=TRUE;
				pthread_cond_signal(&thread_cond1);	
				break;
			}
			if(strcmp(dir_ent->d_name, ".") == 0 || strcmp(dir_ent->d_name, "..")==0)
				continue;	

			file_cnt++;
			sprintf(decrypt_file_name_buf, "%s%d",input_file,file_cnt);			
		    fprintf(stderr,"%s\n",decrypt_file_name_buf);	
			fp = fopen(decrypt_file_name_buf, "r");
			
			fstat(fileno(fp),&file_stat);	
		
			
			pthread_mutex_lock(&mutex_lock1);
			fread((cl_uchar*)buf->buffer, sizeof(cl_uchar),file_stat.st_size, fp);
			
			pthread_mutex_unlock(&mutex_lock1);
			pthread_cond_signal(&thread_cond1);
		}
	}
	else if(mode == AES_MODE_ENCRYPT)
	{
        	fp = fopen(input_file, "r");
	
		while(1)
        	{
			if (feof(fp))
			{
				fclose(fp);
				end_point=TRUE;
				pthread_cond_signal(&thread_cond1);
				break;
			}
			//read input file
                if (sigwait(&sigset, &sig_no) != 0)
                    fprintf(stderr, "Failed to wait for next clock tick\n");
			//fprintf(stderr, "got timer signal");
				pthread_mutex_lock(&mutex_lock1);
				fgets((char*)buf->buffer, sizeof(cl_uchar)*700, fp);
				pthread_mutex_unlock(&mutex_lock1);		
				pthread_cond_signal(&thread_cond1);
	        }
	
	}	
	pthread_exit((void *) 0);
}


void show_help(char *argv[])
{
	printf("\nUsage: %s -i INPUT -o OUTPUT -m MODE [-k KEY_SIZE] [-p PASSWD] [-d DEV] [-g GSIZE] [-l LSIZE]\n\n", argv[0]);
	printf("  -i INPUT         the input file\n");
	printf("  -o OUTPUT        the output file\n");
	printf("  -m MODE          MODE can be encrypt or decrypt\n");
	printf("  -k KEY_SIZE      the key size can be 128, 192 or 256 (default is %d)\n", default_key_size_bits);
	printf("  -p PASSWD        the password; if unspecified the user will be asked to type it\n");
	printf("  -d DEV           DEV can be cpu or gpu (default is %s)\n", get_opencl_device_name(DEFAULT_DEVICE));
	printf("  -g GSIZE         the OpenCL global work size (default is decided by OpenCL)\n");
	printf("  -l LSIZE         the OpenCL local work size (default is %u)\n", (unsigned) OPENCL_DEFAULT_LOCAL_SIZE);
	printf("\n");
	exit(EXIT_SUCCESS);
}

/**
 * Parses the command line options and uses them to set some variables
 * \param argc the number of command line arguments, including the executable file itself
 * \param argv the value of command line arguments, including the executable file itself
 * \param input_file_name the pointer to the string where the input file name specified by the user will be stored
 * \param output_file_name the pointer to the string where the output file name specified by the user will be stored
 * \param mode the pointer to the AES mode (keeps track if the task will be to encrypt or to decrypt)
 * \param key_size_bits the pointer to the key size value, in bits
 * \param password the pointer to the password string
 * \param device the pointer to the OpenCL device to be used (cpu or gpu)
 */
void parse_command_line(int argc, char *argv[], char **input_file_name, char **output_file_name, aes_mode * mode, unsigned short *key_size_bits, char **password, opencl_device * device)
{
	/* If the user called the program with no arguments, he has no clue and 
	   needs some tutoring. */
	if (argc == 1)
		show_help(argv);

	int c;

	// Default values
	*key_size_bits = default_key_size_bits;
	*password = NULL;
	*device = DEFAULT_DEVICE;
	*mode = AES_MODE_NONE;

	do {
		c = getopt(argc, argv, "hi:o:m:k:p:d:g:l:");
		switch (c) {
		case 'i':
			*input_file_name = (char *) malloc(sizeof(char) * strlen(optarg) + 1);
			strcpy(*input_file_name, optarg);
			break;
		case 'o':
			*output_file_name = (char *) malloc(sizeof(char) * strlen(optarg) + 1);
			strcpy(*output_file_name, optarg);
			break;
		case 'd':
			if (strcmp(optarg, "cpu") == 0)
				*device = OPENCL_DEVICE_CPU;
			else if (strcmp(optarg, "gpu") == 0)
				*device = OPENCL_DEVICE_GPU;
			else
				*device = OPENCL_DEVICE_NONE;
			break;
		case 'k':
			*key_size_bits = atoi(optarg);
			break;
		case 'p':
			*password = (char *) malloc(sizeof(char) * strlen(optarg) + 1);
			strcpy(*password, optarg);
			break;
		case 'm':
			if (strcmp(optarg, "encrypt") == 0)
				*mode = AES_MODE_ENCRYPT;
			else if (strcmp(optarg, "decrypt") == 0)
				*mode = AES_MODE_DECRYPT;
			else
				*mode = AES_MODE_NONE;
			break;
		case 'h':
			show_help(argv);
		}
	} while (c != -1);
}

/**
 * Checks if the command line arguments are valid.
 * It doesn't include the I/O files because they'll be checked later in the program, when they'll be used.
 * \param mode the AES mode (keeps track if the task will be to encrypt or to decrypt)
 * \param key_size_bits the key size
 * \param device the OpenCL device to be used (cpu or gpu)
 */
void check_arguments(aes_mode mode, unsigned short key_size_bits, opencl_device device)
{
	if (mode == AES_MODE_NONE) {
		fprintf(stderr, "ERROR: wrong AES mode, it should be encrypt or decrypt.\n");
		exit(EXIT_FAILURE);
	}

	if (key_size_bits != 128 && key_size_bits != 192 && key_size_bits != 256) {
		fprintf(stderr, "ERROR: wrong key size, it should be 128, 192 or 256.\n");
		exit(EXIT_FAILURE);
	}

	if (device == OPENCL_DEVICE_NONE) {
		fprintf(stderr, "ERROR: wrong OpenCL device, it should be cpu or gpu.\n");
		exit(EXIT_FAILURE);
	}
}

/** 
 * Returns an hashed version of the given password, truncard to size bytes.
 * \param password the password to be hashed
 * \param size the size of the hashed password
 * \return the hashed password
 */
cl_uchar *hash_password(char *password, size_t size)
{
	cl_uchar *hash = (cl_uchar *) malloc(size * sizeof(cl_uchar));

	SHA256_CONTEXT context;
	sha256_init(&context);
	sha256_write(&context, (unsigned char *) password, strlen(password));
	sha256_final(&context);

	memcpy(hash, sha256_read(&context), size);

	return hash;
}

void monitor_and_control()
{
	//monitor
	double avg_resp_time= total_resp_time /cnt_resp_time;
    int dvfs_table[7]={600, 543, 480, 420, 350, 266, 177};
    char cmd_buffer[255]="";
    int set_clock;
    int set_num;
	cnt_resp_time=0;
	total_resp_time=0;
    
	printf("%lf ms\t%lf watt\n", avg_resp_time, avg_watts);

	//control
    switch(current_clock){
        case 600:
            set_num = 0;
            break;
        case 543:
            set_num = 1;
            break;
        case 480:
            set_num = 2;
            break;
        case 420:
            set_num = 3;
            break;
        case 350:
            set_num = 4;
            break;
        case 266:
            set_num = 5;
            break;
        case 177:
            set_num = 6;
            break;
    }


    if (avg_resp_time < 4)
    {
        if(current_clock != 177)
        {
            set_clock=dvfs_table[set_num+1];
            sprintf(cmd_buffer, "echo %d > /sys/class/misc/mali0/device/clock",set_clock);
            system(cmd_buffer);
        }
        else        
            set_clock = dvfs_table[set_num];   
    }
    else
    {
        if(current_clock != 600)
        {
            set_clock=dvfs_table[set_num-1];
            sprintf(cmd_buffer, "echo %d > /sys/class/misc/mali0/device/clock",set_clock);
            system(cmd_buffer);
        }
        else
            set_clock = dvfs_table[set_num];   
    }
    current_clock = set_clock;
    printf("current clock : %d\n",current_clock);
}

/** 
 * The main program.
 * \param argc the number of command line arguments (the first is the executable file's name)
 * \param argv the value of command line arguments (the first is the executable file's name)
 */
int main(int argc, char *argv[])
{
	char *input_file_name = NULL, *output_file_name = NULL;
	aes_mode mode;
	unsigned short key_size_bits;
	char *password = NULL;
	cl_uchar *password_hash = NULL;
	opencl_device device;
	
	
	cl_uchar *buffer = NULL;	
	size_t global_size, local_size;
	
	//
	Thdata *thread_data;
	thread_data = (struct _thread_data*) malloc(sizeof(struct _thread_data));

	pthread_t control_thr, powermon_thr;
	powermon_args	pa;
	control_thr_args	ca;

	cl_context context=NULL;
	cl_program program=NULL;
	cl_command_queue command_queue=NULL;
	cl_kernel kernel=NULL;
	cl_mem cl_buffer=NULL;
	cl_mem cl_round_key=NULL;
	cl_ulong blocks;

	char output_file_name_buf[200]="";
	int cnt=0;
	//thread
	pthread_t	thread;
	sigset_t		sigset;
	int			rc;

    //dvfs_initializing
    dvfs_init();

	sigfillset(&sigset);
	sigaddset(&sigset, TIMER_SIG);
//	sigaddset(&sigset, SIGALRM);
	pthread_sigmask(SIG_SETMASK, &sigset, NULL);
	
	printf("\n\n-------- PAES --------\n\n\n");

	parse_command_line(argc, argv, &input_file_name, &output_file_name, &mode, &key_size_bits, &password, &device);
	check_arguments(mode, key_size_bits, device);
	input_file = input_file_name;
	size_t size = 700;
	size_t dec_size;
	if (password == NULL) {
		char *getpass(const char *prompt);
		password = getpass("\nPlease type the password: ");
	}
	password_hash = hash_password(password, key_size_bits / 8);

    	

	printf("PARAMETERS:\n");
	printf("   Input file: %s\n", input_file_name);
	printf("   Output file: %s\n", output_file_name);
	printf("   AES mode: %s\n", get_aes_mode_name(mode));
	printf("   Key size: %u\n", key_size_bits);
	printf("   Device: %s\n", get_opencl_device_name(device));
	printf("   File size: %u bytes\n", (unsigned) size);
	printf("\n\n");

	blocks = size / AES_BLOCK_SIZE;

#ifdef PAES_DYNAMIC_SIZE
        global_size = blocks;
        if (global_size > 4194304)
                global_size = 4194304;

        local_size = global_size / 8;
        if (local_size < 1)
                local_size = 1;
        else if (local_size > PAES_MAX_LOCAL_SIZE)
                local_size = PAES_LOCAL_SIZE;
#elif defined(PAES_STATIC_SIZE)
        global_size = PAES_GLOBAL_SIZE;
        local_size = PAES_LOCAL_SIZE;
#endif

	printf("Global work size is %lu\n", (long unsigned) global_size);
        printf("Local work size is %lu\n", (long unsigned) local_size);

	//init aes
	if (init_aes(&context, &program, &command_queue, &kernel, &cl_buffer, &cl_round_key, password_hash, size, device, key_size_bits) != 1)
	{
		if (buffer)
	                free(buffer);
	        if (input_file_name)
	                free(input_file_name);
	        if (output_file_name)
	                free(output_file_name);
	        if (password)
	                free(password);
	        if (password_hash)
	                free(password_hash);
		return 0;
	}
	
	//set buffer size
	buffer = (cl_uchar *)calloc(size,sizeof(cl_uchar));
	
	//buffer connection
	thread_data->buffer=buffer;
	thread_data->mode = mode;
	//thread create
	rc = pthread_create(&thread, NULL, read_thread, (void*)thread_data);
	if (rc)
		printf("read_thread : %s\n",strerror(errno));

	/* create a power monitor thread. */
    pa.n_samples = 5;
    pa.interval = 1;	//seconds
    pa.avg_watts = &avg_watts;
    if (pthread_create(&powermon_thr, NULL, monitor_power, &pa) != 0)
    {
		perror("creating powermon_thr");
		exit(1);
    }

	/* create a control thread. */
    ca.interval = 1;	//seconds
	ca.callback = &monitor_and_control;
    if (pthread_create(&control_thr, NULL, periodic_control, &ca) != 0)
    {
		perror("creating control_thr");
		exit(1);
    }


	
	while (1)
	{
		struct timespec ts, te;
		double total_time;

		//end of input_file
		if(end_point)
			break;		

		pthread_mutex_lock(&mutex_lock1);
		pthread_cond_wait(&thread_cond1, &mutex_lock1);
		clock_gettime(CLOCK_REALTIME, &ts);
		
		//fprintf(stderr ,"output file name : %s\n",output_file_name_buf);
		
		//apply aes
		if (apply_aes(buffer, &command_queue, &kernel, &cl_buffer, &cl_round_key, size, blocks, key_size_bits, global_size, local_size, mode) != -1)
		{
		}	
		if(mode == AES_MODE_ENCRYPT)
		{		
			if(end_point==1)
				break;
			//setting output file name
			sprintf(output_file_name_buf, "%s%d",output_file_name,++cnt);
			write_file(output_file_name_buf, buffer, size);	
		}
		else if(mode == AES_MODE_DECRYPT)
		{
			if(end_point==1)
				break;
			strtok((char*)buffer,"\n");
			dec_size = sizeof(cl_uchar)*strlen((char*)buffer);
			write_file_append(output_file_name, buffer, dec_size);
		}
		

		clock_gettime(CLOCK_REALTIME, &te);
		total_time = (te.tv_sec - ts.tv_sec)*1000 + (te.tv_nsec - ts.tv_nsec)/1000000;
		total_resp_time=total_resp_time + total_time;
		cnt_resp_time++;
		//printf("-----------------%f ms \n",total_time);
		pthread_mutex_unlock(&mutex_lock1);		
	}

	if (buffer)
		free(buffer);
	if (input_file_name)
		free(input_file_name);
	if (output_file_name)
		free(output_file_name);
	if (password)
		free(password);
	if (password_hash)
		free(password_hash);
	printf("\n\n----- It ends here... -----\n\n\n");

	return EXIT_SUCCESS;
}
