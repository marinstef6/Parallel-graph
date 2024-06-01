// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include "os_graph.h"
#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

#define NUM_THREADS	4
#define NUMBER 1
#define NOT_VISITED 0
#define PROCESSING 1
#define DONE 2

static int sum;
static os_graph_t *graph;
static os_threadpool_t *tp;
/* TODO: Define graph synchronization mechanisms. */

static pthread_mutex_t graph_mutex;
static pthread_mutex_t sum_mutex;

/* TODO: Define graph task argument. */

// struct timespec ts = {
// 	.tv_sec = 0,
// 	.tv_nsec = 1000000
// };


void neighbour(void *arg)
{
	unsigned int idx = *(unsigned int *)arg;

	//blocam mutexul pentru a nu mai avea acces la graful nostru
	pthread_mutex_lock(&graph_mutex);

	//daca nodul a fost deja vizitat, nu mai facem nimic
	if (graph->visited[idx] != NOT_VISITED) {
		pthread_mutex_unlock(&graph_mutex);
		return;
	}

	//daca nodul nu a fost vizitat, il marcam ca fiind in procesare
	graph->visited[idx] = PROCESSING;
	os_node_t *node = graph->nodes[idx];

	//deblocam mutexul pentru a putea face operatii pe graful nostru
	pthread_mutex_unlock(&graph_mutex);

	for (unsigned int i = 0; i < node->num_neighbours; i++) {
		int neighbour_idx = node->neighbours[i];

		pthread_mutex_lock(&graph_mutex);

		//daca vecinul nu a fost vizitat, il procesam
		if (graph->visited[neighbour_idx] == NOT_VISITED) {
			//alocam memorie
			unsigned int *new_arg = malloc(sizeof(unsigned int));
			*new_arg = neighbour_idx;
			//cream un task pentru a procesa vecinul
			os_task_t *task = create_task(neighbour, new_arg, free);
			//adaugam task-ul in coada de executie
			enqueue_task(tp, task);
		}
		pthread_mutex_unlock(&graph_mutex);
	}

	pthread_mutex_lock(&sum_mutex);
	//adaugam la suma nodul curent
	sum += node->info;
	//nanosleep(&ts, NULL);
	pthread_mutex_unlock(&sum_mutex);

	pthread_mutex_lock(&graph_mutex);
	//marcam nodul ca fiind procesat
	graph->visited[idx] = DONE;
	pthread_mutex_unlock(&graph_mutex);
}

static void process_node(unsigned int idx)
{
    //alocam memoria pentru argumentul funcției neighbour
	unsigned int *arg = malloc(sizeof(unsigned int));

	//verificam daca s-a alocat memoria
	if (arg == NULL)
		return;

	*arg = idx;

	//cream un task pentru a procesa nodul și vecinii săi
	os_task_t *task = create_task(neighbour, arg, free);

	if (task == NULL) {
		free(arg);
		return;
	}

    //adaugam task-ul în coada de execuție
	enqueue_task(tp, task);
}

int main(int argc, char *argv[])
{
	FILE *input_file;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s input_file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	input_file = fopen(argv[1], "r");
	DIE(input_file == NULL, "fopen");

	graph = create_graph_from_file(input_file);

	/* DONE: Initialize graph synchronization mechanisms. */
	pthread_mutex_init(&graph_mutex, NULL);
	pthread_mutex_init(&sum_mutex, NULL);

	tp = create_threadpool(NUMBER);

	process_node(0);
	wait_for_completion(tp);
	destroy_threadpool(tp);

	//eliberam memoria
	pthread_mutex_destroy(&graph_mutex);
	pthread_mutex_destroy(&sum_mutex);

	printf("%d", sum);

	return 0;
}
