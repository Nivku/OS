/// INCLUDE ///
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <algorithm>
#include "Barrier.h"
#include "MapReduceFramework.h"

/// System Error MSG
#define PTHREAD_CREATE_ERR_MSG "system error: system failed to create pthread\n"
#define PTHREAD_JOIN_ERR_MSG "system error: system failed to join pthread\n"
#define MUTEX_LOCK_ERR_MSG "system error: system failed to lock mutex\n"
#define MUTEX_UNLOCK_ERR_MSG "system error: system failed to unlock mutex\n"
#define MUTEX_DESTROY_ERR_MSG "system error: system failed to destroy mutex\n"



typedef struct JobContext JobContext;
void *ThreadStartRoutine(void *arg);

///// STRUCTS /////

typedef struct ThreadContext {
    int id{};
    JobContext* jobContext{};
    IntermediateVec intermediatePairs; // vector of thread intermediatePairs
    pthread_t thread{};

    ThreadContext(int givenId,JobContext* Context):
    id(givenId),
    jobContext(Context)
    {
        thread = 0;
        if (pthread_create(&this->thread, nullptr, ThreadStartRoutine, this))
        {
            std::cerr << PTHREAD_CREATE_ERR_MSG << std::endl;
            exit(EXIT_FAILURE);
        }
    }
}ThreadContext;

struct JobContext
{
    const MapReduceClient& client ; // client of job
    int multiThreadLevel; // number of threads
    JobState jobState; // current state of job
    std::vector<ThreadContext*> threadsContextsVector; // vector of threads context pointers
    std::atomic<size_t> mappingAtomicCounter; // atomic counter shared between threads
    std::atomic<size_t> AtomicShuffleVSize;
    std::atomic<size_t> phaseAtomicCounter;
    const InputVec inputVector;
    OutputVec& outputVector;
    std::atomic<size_t> intermediaryPairsCounter;
    Barrier barrier;
    std::vector<IntermediateVec> ShuffleVector;
    pthread_mutex_t emitMutex;
    pthread_mutex_t getJobStateMutex;
    pthread_mutex_t updatePercentageMutex;
    std::atomic_flag waitFlag;

    /// Job context constructor
    JobContext(const MapReduceClient& givenClient, int threadsNum, const InputVec& inputVec, OutputVec& outputVec):
            client(givenClient),
            multiThreadLevel(threadsNum),
            jobState({UNDEFINED_STAGE,0}),
            threadsContextsVector(),
            mappingAtomicCounter(0),
            AtomicShuffleVSize(0),
            phaseAtomicCounter(0),
            inputVector(inputVec),
            outputVector(outputVec),
            intermediaryPairsCounter(0),
            barrier(multiThreadLevel),
            emitMutex(PTHREAD_MUTEX_INITIALIZER),
            getJobStateMutex(PTHREAD_MUTEX_INITIALIZER),
            updatePercentageMutex(PTHREAD_MUTEX_INITIALIZER),
            waitFlag{false}
    {}

    ~JobContext()
    {
        // destroy threads contexts
        for (ThreadContext* threadContext : threadsContextsVector)
            delete threadContext;

        // destroy mutex
        if (pthread_mutex_destroy(&emitMutex) || pthread_mutex_destroy(&getJobStateMutex)
        || pthread_mutex_destroy(&updatePercentageMutex))
        {
            std::cerr << MUTEX_DESTROY_ERR_MSG << std::endl;
            exit(EXIT_FAILURE);
        }
    }
};

///// METHODS /////


void UpdatePhasePercentage(ThreadContext *threadContext, stage_t stage, bool  reset = false)
{
    if (pthread_mutex_lock(&(threadContext->jobContext->updatePercentageMutex)))
    {
        std::cerr << MUTEX_LOCK_ERR_MSG << std::endl;
        exit(EXIT_FAILURE);
    }

    if (reset)
    {
        threadContext->jobContext->jobState.stage = stage;
        threadContext->jobContext->jobState.percentage = 0;
        threadContext->jobContext->phaseAtomicCounter = 0;

        if (pthread_mutex_unlock(&(threadContext->jobContext->updatePercentageMutex)))
        {
            std::cerr << MUTEX_UNLOCK_ERR_MSG << std::endl;
            exit(EXIT_FAILURE);
        }
        return;
    }

    unsigned long stageSize = 0;

    if (stage == MAP_STAGE)
    {
        threadContext->jobContext->phaseAtomicCounter++;
        stageSize = threadContext->jobContext->inputVector.size();
    }

    if (stage == REDUCE_STAGE)
    {
        // when the shuffleVec is empty at REDUCE_STAGE the percentage 100% directly
        if (threadContext->jobContext->ShuffleVector.empty())
        {
            threadContext->jobContext->jobState.percentage = 100;
            if (pthread_mutex_unlock(&(threadContext->jobContext->updatePercentageMutex)))
            {
                std::cerr << MUTEX_UNLOCK_ERR_MSG << std::endl;
                exit(EXIT_FAILURE);
            }
            return;
        }
        else
        {
            stageSize = threadContext->jobContext->intermediaryPairsCounter;
        }
    }

    if (stage == SHUFFLE_STAGE)
        stageSize = threadContext->jobContext->intermediaryPairsCounter;


    threadContext->jobContext->jobState.percentage = (((float) threadContext->jobContext->phaseAtomicCounter.load() /
                                                        ((float) stageSize)) * 100);

    if (pthread_mutex_unlock(&(threadContext->jobContext->updatePercentageMutex)))
    {
        std::cerr << MUTEX_UNLOCK_ERR_MSG << std::endl;
        exit(EXIT_FAILURE);
    }

}


bool IntermediatePairCmp(IntermediatePair pair1, IntermediatePair pair2)
{

    return *pair1.first < *pair2.first;
}

void ThreadMapPhase(ThreadContext *threadContext)
{
    // update job stage
    threadContext->jobContext->jobState.stage = MAP_STAGE;

    // use atomic counter to ensure each thread gets a unique task to map using client function
    size_t inputVectorSize = threadContext->jobContext->inputVector.size();
    size_t oldAtomicCounter = (threadContext->jobContext->mappingAtomicCounter)++;

    while (oldAtomicCounter < inputVectorSize)
    {
        // map task with client function
        InputPair inPair = threadContext->jobContext->inputVector.at(oldAtomicCounter);
        threadContext->jobContext->client.map(inPair.first, inPair.second, threadContext);

        // update stage percentage
        UpdatePhasePercentage(threadContext, MAP_STAGE);

        // increase old counter
        oldAtomicCounter =  (threadContext->jobContext->mappingAtomicCounter)++;
    }
}


/// Go over each threads IntermediatePairs, and return a pointer to the pair with the max key
IntermediatePair * GetMaxPair(ThreadContext *threadContext,unsigned long ThreadNum)
{

  IntermediatePair *maxPair = nullptr;
  unsigned long i = threadContext->jobContext->threadsContextsVector.size();
  for (unsigned long j= 0 ; j < ThreadNum ; j++)
    {
      // Check if IntermediateVec not empty
      //ThreadContext* curr = threadContext->jobContext->threadsContextsVector.at(j);

      // Check is there is problem with ContextVec Size and return if so.
      if (i < ThreadNum)
      {
          break;
      }

      if (!threadContext->jobContext->threadsContextsVector.at(j)->intermediatePairs.empty())
        {
            if (maxPair == nullptr)
            {
                maxPair = &threadContext->jobContext->threadsContextsVector.at(j)->intermediatePairs.back();
                continue;
            }
            if (IntermediatePairCmp (*maxPair,threadContext->jobContext->threadsContextsVector.at(j)->intermediatePairs.back()))
                maxPair = &threadContext->jobContext->threadsContextsVector.at(j)->intermediatePairs.back();
        }
    }
  return maxPair;
}


void ThreadShufflePhase(ThreadContext *threadContext,unsigned long ThreadNUm)
{
    // update job stage
    UpdatePhasePercentage(threadContext,SHUFFLE_STAGE, true);

    IntermediateVec PairsVec = {};     // PairsVec will hold all pairs with the same key
    IntermediatePair *maxKey = nullptr;   // maxKey will hold the pair with maximum key

    // while threads IntermediatePairs not empty, add to ShuffleVector vectors of pairs with the same key.
    while (true)
    {
      maxKey =  GetMaxPair(threadContext,ThreadNUm);
      if (maxKey == nullptr )
          {
          return;
          }

      PairsVec.clear();
      // add to pairsVec all the pairs with the same key as maxKey.
      unsigned long i = threadContext->jobContext->threadsContextsVector.size();
      for (unsigned long j= 0 ; j< i ; j++)
      {
          //unsigned long p = threadContext->jobContext->threadsContextsVector.size();

          // Checks if there is problem with the ContextVector
          if (ThreadNUm > i)
          {
              break;
          }
          ThreadContext* curr = threadContext->jobContext->threadsContextsVector.at(j);
          while(!curr->intermediatePairs.empty() &&
          (!(IntermediatePairCmp(curr->intermediatePairs.back(),*maxKey))))
          {
              PairsVec.push_back (curr->intermediatePairs.back ());
              threadContext->jobContext->phaseAtomicCounter++;
              curr->intermediatePairs.pop_back ();
          }
      }
      if (!(PairsVec.empty()))
      {
          threadContext->jobContext->ShuffleVector.push_back (PairsVec);
      }

      // update phase percentage
      UpdatePhasePercentage(threadContext, SHUFFLE_STAGE);
    }

}

void ThreadReducePhase(ThreadContext *threadContext)
{

    // if ShuffleVec is empty Percentage drops to 100%

    if (threadContext->jobContext->ShuffleVector.empty())
    {
        UpdatePhasePercentage(threadContext, REDUCE_STAGE);
        return;
    }

    IntermediateVec pairsVec;
    size_t oldAtomicCounter = (threadContext->jobContext->AtomicShuffleVSize)++;
    size_t ShuffleVectorSize = threadContext->jobContext->ShuffleVector.size();

    while (oldAtomicCounter < ShuffleVectorSize)
    {
        pairsVec = threadContext->jobContext->ShuffleVector[oldAtomicCounter];
        threadContext->jobContext->client.reduce(&pairsVec, threadContext);

        // update stage percentage
        threadContext->jobContext->phaseAtomicCounter += pairsVec.size();
        UpdatePhasePercentage(threadContext, REDUCE_STAGE);

        oldAtomicCounter = threadContext->jobContext->AtomicShuffleVSize++;
    }
}


void* ThreadStartRoutine(void* arg)
{

    /// cast arg to thread context
    auto threadContext = (ThreadContext*) arg;

    /// start map phase on thread
    ThreadMapPhase(threadContext);

    /// Sort phase and activate thread barrier
    std::sort(threadContext->intermediatePairs.begin(),threadContext->intermediatePairs.end(),IntermediatePairCmp);
    threadContext->jobContext->barrier.barrier();

    /// Shuffle phase
    // active this phase only on thread 0
    if (threadContext->id == 0)
    {
         ThreadShufflePhase(threadContext,threadContext->jobContext->multiThreadLevel);
        // after finishing shuffle phase, change job state to REDUCE_PHASE
        UpdatePhasePercentage(threadContext,REDUCE_STAGE,true);
    }
    // wait until thread 0 finish shuffle phase
    threadContext->jobContext->barrier.barrier();

    /// Reduce phase
    ThreadReducePhase(threadContext);
    return nullptr;
}

///// MapReduceClient.h /////

JobHandle startMapReduceJob(const MapReduceClient& client, const InputVec& inputVec,
                            OutputVec& outputVec, int multiThreadLevel)
{
    // create new job context object
    auto *jobContext = new JobContext(client,multiThreadLevel,inputVec,outputVec);

    // create multiThreadLevel number of threads contexts, and add them to the job context
    for (int i = 0; i < multiThreadLevel; i++)
    {
        jobContext->threadsContextsVector.push_back(new ThreadContext(i,jobContext));
    }

    return jobContext;
}


void getJobState(JobHandle job, JobState* state)
{
    auto *jobContext = static_cast<JobContext*>(job);

    if (pthread_mutex_lock(&jobContext->updatePercentageMutex))
    {
        std::cerr << MUTEX_LOCK_ERR_MSG << std::endl;
        exit(EXIT_FAILURE);
    }

    state->stage = jobContext->jobState.stage;
    state->percentage = jobContext->jobState.percentage;

    if (pthread_mutex_unlock(&jobContext->updatePercentageMutex))
    {
        std::cerr << MUTEX_UNLOCK_ERR_MSG << std::endl;
        exit(EXIT_FAILURE);
    }
}

void emit2 (K2* key, V2* value, void* context)
{
    auto threadContext = (ThreadContext *) context;
    IntermediatePair pair;
    pair.first = key;
    pair.second = value;
    threadContext->intermediatePairs.push_back(pair);
    threadContext->jobContext->intermediaryPairsCounter++;
}


void emit3 (K3* key, V3* value, void* context)
{
  auto threadContext = (ThreadContext *) context;

  if (pthread_mutex_lock(&threadContext->jobContext->emitMutex))
  {
      std::cerr << MUTEX_LOCK_ERR_MSG << std::endl;
      exit(EXIT_FAILURE);
  }

  OutputPair pair;
  pair.first = key;
  pair.second = value;
  threadContext->jobContext->outputVector.push_back(pair);

  if (pthread_mutex_unlock(&threadContext->jobContext->emitMutex))
  {
      std::cerr << MUTEX_UNLOCK_ERR_MSG << std::endl;
      exit(EXIT_FAILURE);
  }
}


void waitForJob(JobHandle job)
{
    auto *jobContext = (JobContext *) job;

    // use atomic flag to make sure use of pthread_join only once
    if (jobContext->waitFlag.test_and_set())
        return;

    else
    {
        for (int i = 0; i < jobContext->multiThreadLevel; i++)
        {
            if (pthread_join(jobContext->threadsContextsVector[i]->thread, nullptr))
            {
                std::cerr << PTHREAD_JOIN_ERR_MSG << std::endl;
                exit(EXIT_FAILURE);
            }
        }
    }
}


void closeJobHandle(JobHandle job)
{
    auto jobContext = (JobContext *) job;
    waitForJob(job);
    delete jobContext;
}


