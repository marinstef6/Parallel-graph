// SPDX-License-Identifier: BSD-3-Clause

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

/* Create a task that would be executed by a thread. */
os_task_t *create_task(void (*action)(void *), void *arg, void (*destroy_arg)(void *))
{
	os_task_t *t;

	t = malloc(sizeof(*t));
	DIE(t == NULL, "malloc");

	t->action = action;		// the function
	t->argument = arg;		// arguments for the function
	t->destroy_arg = destroy_arg;	// destroy argument function

	return t;
}

/* Destroy task. */
void destroy_task(os_task_t *t)
{
	if (t->destroy_arg != NULL)
		t->destroy_arg(t->argument);
	free(t);
}

/* Put a new task to threadpool task queue. */
void enqueue_task(os_threadpool_t *tp, os_task_t *t)
{
	assert(tp != NULL);
	assert(t != NULL);

	/* DONE: Enqueue task to the shared task queue. Use synchronization. */
	//blocam mutexul pentru a nu mai avea acces la coada
	pthread_mutex_lock(&tp->mutex);

	int was_empty = list_empty(&tp->head);
	//adaugam task-ul in coada
	list_add_tail(&tp->head, &t->list);

	if (was_empty)
		pthread_cond_signal(&tp->cond);

	pthread_mutex_unlock(&tp->mutex);
}

/*
 * Check if queue is empty.
 * This function should be called in a synchronized manner.
 */
static int queue_is_empty(os_threadpool_t *tp)
{
	return list_empty(&tp->head);
}

//functie pentru a semnala conditia de stop
void signal_stop_condition_if_needed(os_threadpool_t *tp)
{
	pthread_mutex_lock(&tp->mutex);

	pthread_cond_signal(&tp->stop_cond);

	pthread_mutex_unlock(&tp->mutex);
}

/*
 * Get a task from threadpool task queue.
 * Block if no task is available.
 * Return NULL if work is complete, i.e. no task will become available,
 * i.e. all threads are going to block.
 */
os_task_t *dequeue_task(os_threadpool_t *tp)
{
	os_task_t *t;

	/* TODO: Dequeue task from the shared task queue. Use synchronization. */
	pthread_mutex_lock(&tp->mutex);
	//cat timp coada este goala, asteptam
	while (queue_is_empty(tp)) {
		if	(tp->stop) {
			pthread_mutex_unlock(&tp->mutex);
			return NULL;
		}
		pthread_cond_wait(&tp->cond, &tp->mutex);
	}
	//extragem task-ul din coada
	t = list_entry(tp->head.next, os_task_t, list);
	//il stergem din coada
	list_del(tp->head.next);

	pthread_mutex_unlock(&tp->mutex);

	return t;
}

/* Loop function for threads */
static void *thread_loop_function(void *arg)
{
	os_threadpool_t *tp = (os_threadpool_t *) arg;

	while (1) {
		os_task_t *t;

		t = dequeue_task(tp);
		if (t == NULL)
			break;
		t->action(t->argument);
		destroy_task(t);

		signal_stop_condition_if_needed(tp);
	}

	return NULL;
}

/* Wait completion of all threads. This is to be called by the main thread. */
void wait_for_completion(os_threadpool_t *tp)
{
	/* DONE: Wait for all worker threads. Use synchronization. */
	pthread_mutex_lock(&tp->mutex);

	//cat timp coada nu este goala, thread-ul principal asteapta
	while (!tp->stop || !queue_is_empty(tp)) {
		if (queue_is_empty(tp)) {
			tp->stop = 1;
			//semnalam toate thread-urile care asteapta condiția tp->cond
			pthread_cond_broadcast(&tp->cond);
			break;
		}
		pthread_cond_wait(&tp->stop_cond, &tp->mutex);
	}

	pthread_cond_broadcast(&tp->cond);

	pthread_mutex_unlock(&tp->mutex);

	/* Join all worker threads. */
	for (unsigned int i = 0; i < tp->num_threads; i++)
		pthread_join(tp->threads[i], NULL);
}

/* Create a new threadpool. */
os_threadpool_t *create_threadpool(unsigned int num_threads)
{
	os_threadpool_t *tp = NULL;
	int rc;

	tp = malloc(sizeof(*tp));
	DIE(tp == NULL, "malloc");

	list_init(&tp->head);

	/* DONE: Initialize synchronization data. */
	tp->stop = 0;
	pthread_mutex_init(&tp->mutex, NULL);
	pthread_cond_init(&tp->cond, NULL);
	pthread_cond_init(&tp->stop_cond, NULL);

	tp->num_threads = num_threads;
	tp->threads = malloc(num_threads * sizeof(*tp->threads));
	DIE(tp->threads == NULL, "malloc");
	for (unsigned int i = 0; i < num_threads; ++i) {
		rc = pthread_create(&tp->threads[i], NULL, &thread_loop_function, (void *) tp);
		DIE(rc < 0, "pthread_create");
	}

	return tp;
}

/* Destroy a threadpool. Assume all threads have been joined. */
void destroy_threadpool(os_threadpool_t *tp)
{
	os_list_node_t *n, *p;

	/* DONE: Cleanup synchronization data. */
	pthread_mutex_destroy(&tp->mutex);
	pthread_cond_destroy(&tp->cond);
	pthread_cond_destroy(&tp->stop_cond);

	list_for_each_safe(n, p, &tp->head) {
		list_del(n);
		destroy_task(list_entry(n, os_task_t, list));
	}

	free(tp->threads);
	free(tp);
}
