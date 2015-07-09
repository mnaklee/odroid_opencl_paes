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
#define _POSIX_C_SOURCE 199309
#include <time.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include "paes_functions.h"
#include "paes_size.h"

#define MALLOC_CHECK_ 1

/**************************** MISCELLANEOUS FUNCTIONS ****************************/

size_t read_file(char *file_name, cl_uchar ** buffer)
{
	int fd = open(file_name, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "ERROR: unable to open input file '%s'.\n", file_name);
		exit(EXIT_FAILURE);
	}

	struct stat status_buf;
	fstat(fd, &status_buf);
	size_t size = (size_t) status_buf.st_size;

	*buffer = (cl_uchar *) calloc(size, sizeof(cl_uchar));
	if (read(fd, *buffer, size) == -1) {
		fprintf(stderr, "ERROR: unable to read from input file '%s'.\n", file_name);
		close(fd);
		exit(EXIT_FAILURE);
	}

	close(fd);

	return size;
}

void write_file(char *file_name, cl_uchar * buffer, size_t size)
{
	int fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, FILE_WRITE_MASK);
	if (fd == -1) {
		fprintf(stderr, "ERROR: unable to open output file '%s'.\n", file_name);
		exit(EXIT_FAILURE);
	}

	if (write(fd, buffer, size) == -1) {
		fprintf(stderr, "ERROR: unable to write to output file '%s'.\n", file_name);
		close(fd);
		exit(EXIT_FAILURE);
	}

	close(fd);
}

void write_file_append(char *file_name, cl_uchar * buffer, size_t size)
{
	int fd = open(file_name, O_WRONLY | O_CREAT | O_APPEND, FILE_WRITE_MASK);
	if (fd == -1) {
		fprintf(stderr, "ERROR: unable to open output file '%s'.\n", file_name);
		exit(EXIT_FAILURE);
	}
/*	
	//strtok(buffer,"\n");
	if (write(fd, buffer, size) == -1) {
		fprintf(stderr, "ERROR: unable to write to output file '%s'.\n", file_name);
		close(fd);
		exit(EXIT_FAILURE);
	}
	write(fd, "\n", 1);
*/
    write(fd, "xx\n",3);
	close(fd);
}

/**************************** AES HOST FUNCTIONS ****************************/

char *get_aes_mode_name(aes_mode mode)
{
	static char *aes_mode_name[] = { "encrypt", "decrypt", "unspecified" };
	return aes_mode_name[mode];
}

static inline cl_uint get_rounds_number(unsigned key_size_bits)
{
	cl_uint nk = key_size_bits / 32;
	cl_uint nr = nk + 6;
	return nr;
}

static inline cl_uint get_round_key_size(unsigned key_size_bits)
{
	cl_uint nr = get_rounds_number(key_size_bits);
	return AES_STATE_SIDE * AES_STATE_SIDE * (nr + 1);
}

// This function produces nb(nr+1) round keys. The round keys are used in each round to encrypt the states.
static unsigned char *key_expansion(unsigned char *key, unsigned key_size_bits)
{
	size_t i, j;
	unsigned char temp[4], k;

	unsigned char rcon[] = ROUND_CONSTANT;
	unsigned char sbox_encrypt[] = AES_SBOX_ENCRYPT;

	unsigned nk = key_size_bits / 32;

	unsigned round_key_size = get_round_key_size(key_size_bits);
	unsigned char *round_key = (unsigned char *) malloc(round_key_size * sizeof(unsigned char));

	// The first round key is the key itself.
	for (i = 0; i < nk; i++) {
		round_key[i * 4] = key[i * 4];
		round_key[i * 4 + 1] = key[i * 4 + 1];
		round_key[i * 4 + 2] = key[i * 4 + 2];
		round_key[i * 4 + 3] = key[i * 4 + 3];
	}

	// All other round keys are found from the previous round keys.
	while (i < round_key_size / AES_STATE_SIDE) {
		for (j = 0; j < 4; j++) {
			temp[j] = round_key[(i - 1) * AES_STATE_SIDE + j];
		}
		if (i % nk == 0) {
			// Rotates the 4 bytes in a word to the left once
			k = temp[0];
			temp[0] = temp[1];
			temp[1] = temp[2];
			temp[2] = temp[3];
			temp[3] = k;

			// Subword (using the encryption S-Box)
			temp[0] = sbox_encrypt[temp[0]];
			temp[1] = sbox_encrypt[temp[1]];
			temp[2] = sbox_encrypt[temp[2]];
			temp[3] = sbox_encrypt[temp[3]];

			temp[0] = temp[0] ^ rcon[i / nk];

		} else if (nk > 6 && i % nk == 4) {
			// Subword (using the encryption S-Box)
			temp[0] = sbox_encrypt[temp[0]];
			temp[1] = sbox_encrypt[temp[1]];
			temp[2] = sbox_encrypt[temp[2]];
			temp[3] = sbox_encrypt[temp[3]];
		}
		round_key[i * 4] = round_key[(i - nk) * 4] ^ temp[0];
		round_key[i * 4 + 1] = round_key[(i - nk) * 4 + 1] ^ temp[1];
		round_key[i * 4 + 2] = round_key[(i - nk) * 4 + 2] ^ temp[2];
		round_key[i * 4 + 3] = round_key[(i - nk) * 4 + 3] ^ temp[3];
		i++;
	}

	return round_key;
}

/**************************** OPENCL SUPPORT FUNCTIONS ****************************/

char *get_opencl_device_name(opencl_device device)
{
	static char *opencl_device_name[] = { "cpu", "gpu", "unspecified" };
	return opencl_device_name[device];
}

static void print_device_informations(cl_device_id device)
{
	char device_string[1024];
	clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(device_string), &device_string, NULL);
	printf("\nDevice: %s\n\n", device_string);
}

static double execution_time_msecs(cl_event event)
{
	cl_ulong start, end;
	clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
	clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
	return (end - start) * 1.0E-6;
}
int init_aes(cl_context * context, cl_program * program, cl_command_queue * command_queue, cl_kernel * kernel, cl_mem * cl_buffer, cl_mem * cl_round_key, cl_uchar * key, size_t size, opencl_device device, unsigned key_size_bits)
{
	unsigned char *source = NULL;
        cl_uint num_platforms;
        cl_platform_id *platforms = NULL;
        cl_int error, error2;
        cl_device_id *devices = NULL;
        static const cl_device_type device_type[] = { CL_DEVICE_TYPE_CPU, CL_DEVICE_TYPE_GPU };
        bool ok = 1;            // By default, everything is fine.

        printf("Loading OpenCL source code...\n");
        size_t source_size = read_file(OPENCL_SOURCE, &source);

        error = clGetPlatformIDs(0, NULL, &num_platforms);
        if (error != CL_SUCCESS) {
                fprintf(stderr, "ERROR: clGetPlatformIDs (num_platforms), error code %d\n", error);
                ok = 0;
                goto cleanup;
        }

        platforms = (cl_platform_id *) malloc(sizeof(cl_platform_id) * num_platforms);
        error = clGetPlatformIDs(num_platforms, platforms, NULL);
        if (error != CL_SUCCESS) {
                fprintf(stderr, "ERROR: clGetPlatformIDs (platforms), error code %d\n", error);
                ok = 0;
                goto cleanup;
        }

        cl_context_properties cps[3] = { CL_CONTEXT_PLATFORM, (cl_context_properties) platforms[0], 0 };
        cl_context_properties *cprops = (NULL == platforms[0]) ? NULL : cps;
        *context = clCreateContextFromType(cprops, device_type[device], NULL, NULL, &error);
        printf("clCreateContextFromType...\n");
        if (error != CL_SUCCESS) {
                fprintf(stderr, "ERROR: clCreateContextFromType, error code %d\n", error);
                ok = 0;
                goto cleanup;
        }

        size_t context_information_size;
        error = clGetContextInfo(*context, CL_CONTEXT_DEVICES, 0, NULL, &context_information_size);
        devices = (cl_device_id *) malloc(context_information_size);
        error |= clGetContextInfo(*context, CL_CONTEXT_DEVICES, context_information_size, devices, NULL);
        printf("clGetContextInfo...\n");
        if (error != CL_SUCCESS) {
                fprintf(stderr, "ERROR: clGetContextInfo, error code %d\n", error);
                ok = 0;
                goto cleanup;
        }
	print_device_informations(devices[0]);

        *command_queue = clCreateCommandQueue(*context, devices[0], CL_QUEUE_PROFILING_ENABLE, &error);
        printf("clCreateCommandQueue...\n");
        if (error != CL_SUCCESS) {
                fprintf(stderr, "ERROR: clCreateCommandQueue, error code %d\n", error);
                ok = 0;
                goto cleanup;
        }

        cl_uint round_key_size = get_round_key_size(key_size_bits);
        cl_uchar *round_key = key_expansion(key, key_size_bits);
        printf("Generating the round keys...\n");


	*program = clCreateProgramWithSource(*context, 1, (const char **) &source, &source_size, &error);
        printf("clCreateProgramWithSource...\n");
        if (error != CL_SUCCESS) {
                fprintf(stderr, "ERROR: clCreateProgramWithSource, error code %d\n", error);
                ok = 0;
                goto cleanup;
        }

        error = clBuildProgram(*program, 1, devices, 0, NULL, NULL);
        printf("clBuildProgram...\n");
        if (error != CL_SUCCESS) {
                fprintf(stderr, "ERROR: clBuildProgram, error code %d\n", error);
                ok = 0;
                char *build_log = NULL;
                size_t build_log_size = 0;
                clGetProgramBuildInfo (*program, devices[0], CL_PROGRAM_BUILD_LOG, build_log_size, build_log, &build_log_size);
                build_log = (char *)malloc(build_log_size);
                clGetProgramBuildInfo(*program, devices[0], CL_PROGRAM_BUILD_LOG, build_log_size, build_log, NULL);
                printf("\nBuild log:\n%s\n", build_log);
                goto cleanup;
        }
        clUnloadCompiler();

        *kernel = clCreateKernel(*program, "kernel_aes", &error);
        printf("clCreateKernel...\n");
        if (error != CL_SUCCESS) {
                fprintf(stderr, "ERROR: clCreateKernel, error code %d\n", error);
                ok = 0;
                goto cleanup;
        }
	//create double buffer
	printf("clCreateBuffer & co...\n");
	*cl_buffer = clCreateBuffer(*context, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, sizeof(cl_uchar)*size, NULL, &error);
	*cl_round_key = clCreateBuffer(*context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, sizeof(cl_uchar) * round_key_size, round_key, &error2);
	cleanup:
	if(!ok)
	{		
	       // printf("Cleanup... \n");
	        if (platforms)
	                free(platforms);
	        if (round_key)
	                free(round_key);
	        if (source)
	                free(source);
	        if (devices)
	                free(devices);
		return -1;
	}else
		return 1;

}


int apply_aes(cl_uchar * buffer, cl_command_queue * command_queue, cl_kernel * kernel, cl_mem * cl_buffer, cl_mem * cl_round_key, size_t size, cl_ulong blocks, unsigned key_size_bits, size_t global_size, size_t local_size, aes_mode mode)
{
	//cl_int error, error1;
	cl_int error, error1 = CL_SUCCESS;
        cl_event event_write, event_execute, event_read;
	bool ok=1;
	cl_uchar * temp = (cl_uchar*)clEnqueueMapBuffer(*command_queue, *cl_buffer, CL_TRUE, CL_MAP_WRITE, 0, sizeof(cl_uchar) * size, 0, NULL, &event_write, &error1);
	memcpy(temp, buffer, sizeof(cl_uchar)*size+1);
//	printf("clEnqueueMapBuffer...\n");
	if (error1 != CL_SUCCESS) {
		fprintf(stderr, "ERROR: clEnqueueMapBuffer, error code %d\n", error1);
		ok = 0;
		goto cleanup;
	}
	error1 = clEnqueueUnmapMemObject(*command_queue, *cl_buffer, temp, 0, NULL,NULL);
	if (error1 != CL_SUCCESS) {
		fprintf(stderr, "ERROR: clEnqueueUnmapMemObject, error code %d\n", error1);
		ok = 0;
		goto cleanup;
	}
	
	error = clSetKernelArg(*kernel, 0, sizeof(cl_mem), (void *) cl_buffer);
	error |= clSetKernelArg(*kernel, 1, sizeof(cl_ulong), (void *) &blocks);
	error |= clSetKernelArg(*kernel, 2, sizeof(cl_uint), (void *) &mode);
	error |= clSetKernelArg(*kernel, 3, sizeof(cl_mem), (void *) cl_round_key);
	cl_uint rounds = get_rounds_number(key_size_bits);
	error |= clSetKernelArg(*kernel, 4, sizeof(cl_uint), (void *) &rounds);
	//cl_uint round = 0;

	struct timespec ts, te;
	double total_time;

	double execution_time = 0;

	clock_gettime(CLOCK_REALTIME, &ts);

	error = clEnqueueNDRangeKernel(*command_queue, *kernel, 1, NULL, &global_size, &local_size, 0, NULL, &event_execute);
	if (error != CL_SUCCESS) {
		fprintf(stderr, "ERROR: clEnqueueNDRangeKernel, error code %d\n", error);
		ok = 0;
		goto cleanup;
	}
	//clFinish(*command_queue);

	//printf("round execution time: %lf\n", execution_time_msecs(event_execute));
	execution_time += execution_time_msecs(event_execute);

	temp = (cl_uchar*)clEnqueueMapBuffer(*command_queue, *cl_buffer, CL_TRUE, CL_MAP_READ, 0, sizeof(cl_uchar) * size, 0, NULL, &event_read, &error);
	
        clock_gettime(CLOCK_REALTIME, &te);
	total_time = (te.tv_sec - ts.tv_sec)*1000 + (te.tv_nsec - ts.tv_nsec)/1000000;
        //printf("total in kernel:%lf ms \n", total_time);

	memcpy(buffer, temp, sizeof(cl_uchar)*size+1);
//	printf("clEnqueueReadBuffer...\n\n");
	if (error != CL_SUCCESS) {
		fprintf(stderr, "ERROR: clEnqueueReadBuffer, error code %d\n", error);
		ok = 0;
		goto cleanup;
	}
	error1 = clEnqueueUnmapMemObject(*command_queue, *cl_buffer, temp, 0, NULL,NULL);
	if (error1 != CL_SUCCESS) {
		fprintf(stderr, "ERROR: clEnqueueUnmapMemObject(output), error code %d\n", error1);
		ok = 0;
		goto cleanup;
	}
	
//	printf("kernel excution time:\t%.3f ms\n", execution_time);
//	printf("Write time:\t%.3f ms\n", execution_time_msecs(event_write));
//	printf("Read time:\t%.3f ms\n", execution_time_msecs(event_read));

       cleanup:
//	printf("Cleanup... \n");

	if (event_write)
		clReleaseEvent(event_write);
	if (event_execute)
		clReleaseEvent(event_execute);
	if (event_read)
		clReleaseEvent(event_read);
	if (!ok) {
		return -1;
	} else {
		return 0;
	}
}
