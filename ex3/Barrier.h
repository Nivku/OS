#ifndef BARRIER_H
#define BARRIER_H
#include <pthread.h>

#define MUTEX_LOCK_ERROR "system error: pthread_mutex_lock failed"
#define MUTEX_UNLOCK_ERROR "system error: pthread_mutex_unlock failed"
#define MUTEX_DESTROY_ERROR "system error: pthread_mutex_destroy failed"
#define PTHREAD_JOIN_ERROR "system error: pthread_join failed"

// a multiple use barrier

class Barrier {
public:
	Barrier(int numThreads);
	~Barrier();
	void barrier();

private:
	pthread_mutex_t mutex;
	pthread_cond_t cv;
	int count;
	int numThreads;
};

#endif //BARRIER_H
