#include "Barrier.h"
#include <cstdlib>
#include <cstdio>
#include <iostream>

Barrier::Barrier(int numThreads)
		: mutex(PTHREAD_MUTEX_INITIALIZER)
		, cv(PTHREAD_COND_INITIALIZER)
		, count(0)
		, numThreads(numThreads)
{ }


Barrier::~Barrier()
{
	if (pthread_mutex_destroy(&mutex) != 0) {
	    std::cout << MUTEX_DESTROY_ERROR << std::endl;
		exit(1);
	}
	if (pthread_cond_destroy(&cv) != 0){
		std::cout << "system error: error on pthread_cond_destroy" << std::endl;
		exit(1);
	}
}


void Barrier::barrier()
{
	if (pthread_mutex_lock(&mutex) != 0){
	    std::cout << MUTEX_LOCK_ERROR << std::endl;
		exit(1);
	}
	if (++count < numThreads) {
		if (pthread_cond_wait(&cv, &mutex) != 0){
		    std::cout << "system error: error on pthread_cond_wait" << std::endl;
			exit(1);
		}
	} else {
		count = 0;
		if (pthread_cond_broadcast(&cv) != 0) {
		    std::cout << "system error: error on pthread_cond_broadcast" << std::endl;
			exit(1);
		}
	}
	if (pthread_mutex_unlock(&mutex) != 0) {
	    std::cout << MUTEX_UNLOCK_ERROR << std::endl;
		exit(1);
	}
}
