#include "osm.h"
#include <sys/time.h>

#define MICRO_TO_NANO 1000
#define SEC_TO_NANO 1000000000
#define FUNC_ERROR -1
#define GROWTH_FACTOR 5


double osm_operation_time(unsigned int iterations){
  struct timeval startTime{},endTime{};

  if (iterations == 0)
    return FUNC_ERROR;

  // set start time
  if (gettimeofday(&startTime, nullptr) == FUNC_ERROR )
    return FUNC_ERROR;

  // do iterations number of operations
  int temp = 0;
  for (int i=0; i< iterations ; i += GROWTH_FACTOR)
    {
      temp += 1;
      temp += 1;
      temp += 1;
      temp += 1;
      temp += 1;
    }

  // set end time
  if (gettimeofday(&endTime, nullptr) == FUNC_ERROR)
    return -1;

  double diffSec = (endTime.tv_sec - startTime.tv_sec) * SEC_TO_NANO;
  double diffMicroSec =  (endTime.tv_usec - startTime.tv_usec) * MICRO_TO_NANO;

  return (diffSec+diffMicroSec) / iterations;
}

void emptyFunc(){};


double osm_function_time(unsigned int iterations)
{
  struct timeval startTime{},endTime{};

  if (iterations == 0)
    return FUNC_ERROR;

  // set start time
  if (gettimeofday(&startTime, nullptr) == FUNC_ERROR)
    return FUNC_ERROR;

  // Empty functions calls
  for (int i=0; i< iterations ; i += GROWTH_FACTOR)
    {
      emptyFunc();
      emptyFunc();
      emptyFunc();
      emptyFunc();
      emptyFunc();
    }

  // set end time
  if (gettimeofday(&endTime, nullptr) == FUNC_ERROR)
    return FUNC_ERROR;

  double diffSec = (endTime.tv_sec - startTime.tv_sec) * SEC_TO_NANO;
  double diffMicroSec =  (endTime.tv_usec - startTime.tv_usec) * MICRO_TO_NANO;

  return (diffSec+diffMicroSec) / iterations;
}

double osm_syscall_time(unsigned int iterations)
{
  struct timeval startTime{},endTime{};

  if (iterations == 0)
    return FUNC_ERROR;

  // set start time
  if (gettimeofday(&startTime, nullptr) == FUNC_ERROR)
    return FUNC_ERROR;

  // do iterations number of sys.calls
  for (int i=0; i< iterations ; i += GROWTH_FACTOR)
    {
      OSM_NULLSYSCALL;
      OSM_NULLSYSCALL;
      OSM_NULLSYSCALL;
      OSM_NULLSYSCALL;
      OSM_NULLSYSCALL;
    }

  // set end time
  if (gettimeofday(&endTime, nullptr) == FUNC_ERROR)
    return FUNC_ERROR;

  double diffSec = (endTime.tv_sec - startTime.tv_sec) * SEC_TO_NANO;
  double diffMicroSec =  (endTime.tv_usec - startTime.tv_usec) * MICRO_TO_NANO;

  return (diffSec+diffMicroSec) / iterations;
}