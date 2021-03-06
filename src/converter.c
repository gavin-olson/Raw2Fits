/* 
   converter.c
    - prepare all data and manage conversion threads

   Copyright 2017  Oleg Kutkov <elenbert@gmail.com>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  
 */

#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include "list.h"
#include "converter.h"
#include "file_utils.h"
#include "thread_pool.h"
#include "raw2fits.h"

static list_node_t *file_list = NULL;
static int total_files_counter = 0;

typedef struct thread_arg {
	converter_params_t *conv_param;
	list_node_t *filelist;
	int file_list_offset;
	int file_list_len;
} thread_arg_t;

void convert_one_file(char *file, void *arg)
{
	converter_params_t *params = (converter_params_t *) arg;

	params->logger_msg(params->logger_arg, "\nWorking %s\n", file);

	raw2fits(file, params);

	params->progress.progr_update(&params->progress);

	task_enter_critical_section();

	total_files_counter--;

	if (total_files_counter == 0) {
		params->complete(params->done_arg);
	}

	task_exit_critical_section();
}

void *thread_func(void *arg)
{
	thread_arg_t th_arg_local;
	thread_arg_t *th_arg = (thread_arg_t *) arg;
	converter_params_t *params;

	memcpy(&th_arg_local, th_arg, sizeof(thread_arg_t));

	free(th_arg);

	params = th_arg_local.conv_param;

	iterate_list_cb(th_arg_local.filelist, &convert_one_file, params, th_arg_local.file_list_offset, th_arg_local.file_list_len, &params->converter_run);

	return NULL;
}

void convert_files(converter_params_t *params)
{
	int file_count = 0;
	DIR *dp;
	struct dirent *ep;
	long int cpucnt;
	int files_per_cpu_int, left_files, i;
	int file_list_offset_next = 0;
	thread_arg_t *thread_params;

	params->logger_msg(params->logger_arg, "Reading directory %s\n", params->inpath);

	dp = opendir(params->inpath);

	if (dp == NULL) {
		return;
	}

	if (file_list) {
		free_list(file_list);
		file_list = NULL;
	}	

	while ((ep = readdir(dp))) {
		size_t inpath_len = strlen(params->inpath);
		size_t fname_len = strlen(ep->d_name);
		char *full_path = (char *) malloc(inpath_len + fname_len + 2);

		strncpy(full_path, params->inpath, inpath_len);
		full_path[inpath_len] = '/';
		strncpy(full_path + inpath_len + 1, ep->d_name, fname_len);
		full_path[inpath_len + fname_len + 1] = '\0';

		file_info_t finfo;
		get_file_info(full_path, &finfo);

		if (!finfo.file_supported) {
			free(full_path);
			continue;
		}

		params->logger_msg(params->logger_arg, " Found %s raw file %s  size: %liK\n",
							finfo.file_vendor, ep->d_name, finfo.file_size / 1024);
	
		file_list = add_object_to_list(file_list, full_path);

		free(full_path);

		file_count++;
	}

	closedir (dp);

	if (file_count == 0) {
		params->logger_msg(params->logger_arg, "Can't find RAW files, sorry\n");
		free_list(file_list);
		file_list = NULL;
		return;
	}

	params->progress.progr_setup(&params->progress, file_count);

	cpucnt = sysconf(_SC_NPROCESSORS_ONLN);

	params->logger_msg(params->logger_arg, "\nStarting conveter on %li processor cores...\n", cpucnt);

	if (cpucnt > file_count) {
		files_per_cpu_int = 1;
		left_files = 0;
	} else {
		files_per_cpu_int = file_count / cpucnt;
		left_files = file_count % cpucnt;
	}

	params->logger_msg(params->logger_arg, "Total files to convert: %i\n", file_count);
	params->logger_msg(params->logger_arg, "Files per CPU core: %i, left: %i\n", files_per_cpu_int, left_files);

	total_files_counter = file_count;

	init_thread_pool(cpucnt);
	
	for (i = 0; i < cpucnt; i++) {
		thread_params = (thread_arg_t*) malloc(sizeof(thread_arg_t));

		thread_params->conv_param = params;
		thread_params->filelist = file_list;

		thread_params->file_list_offset = file_list_offset_next;
		thread_params->file_list_len = files_per_cpu_int;

		if (i == cpucnt - 1) {
			thread_params->file_list_len += left_files;
		}
 
		thread_pool_add_task(thread_func, thread_params);

		file_list_offset_next += files_per_cpu_int;
	}
}

void converter_stop(converter_params_t *params)
{
	params->converter_run = 0;

	converter_cleanup();
}

void converter_cleanup()
{
	cleanup_thread_pool();

	if (file_list) {
		free_list(file_list);
		file_list = NULL;
	}
}

