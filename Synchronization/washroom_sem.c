#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "uthread.h"
#include "uthread_sem.h"

#ifdef VERBOSE
#define VERBOSE_PRINT(S, ...) printf (S, ##__VA_ARGS__);
#else
#define VERBOSE_PRINT(S, ...) ;
#endif

#define MAX_OCCUPANCY      3
#define NUM_ITERATIONS     200
#define NUM_PEOPLE         20
#define FAIR_WAITING_COUNT 4

/**
 * You might find these declarations useful.
 */
enum GenderIdentity {MALE = 0, FEMALE = 1};
const static enum GenderIdentity otherGender [] = {FEMALE, MALE};

struct Washroom {
  enum GenderIdentity currentGender;
  int numStallsOccupied;
  int genderFairWaitingCount[2];
  uthread_sem_t lock;
  uthread_sem_t stallAvailable[2];
};

struct Washroom* createWashroom() {
  struct Washroom* washroom = malloc (sizeof (struct Washroom));
  washroom->numStallsOccupied = 0;
  washroom->genderFairWaitingCount[MALE] = 0;
  washroom->genderFairWaitingCount[FEMALE] = 0;
  washroom->lock = uthread_sem_create(1);
  washroom->stallAvailable[MALE] = uthread_sem_create(MAX_OCCUPANCY);
  washroom->stallAvailable[FEMALE] = uthread_sem_create(MAX_OCCUPANCY);
  return washroom;
}

struct Washroom* washroom;

#define WAITING_HISTOGRAM_SIZE (NUM_ITERATIONS * NUM_PEOPLE)
int             entryTicker;                                          // incremented with each entry
int             waitingHistogram         [WAITING_HISTOGRAM_SIZE];
int             waitingHistogramOverflow;
uthread_sem_t   waitingHistogramMutex;
int             occupancyHistogram       [2] [MAX_OCCUPANCY + 1];

void updateTrackerValues (int waitingTime, enum GenderIdentity g, int numStallsOccupied) {

  uthread_sem_wait (waitingHistogramMutex);
    occupancyHistogram[g][numStallsOccupied]++;
    if (waitingTime < WAITING_HISTOGRAM_SIZE)
      waitingHistogram [waitingTime] ++;
    else
      waitingHistogramOverflow ++;
  uthread_sem_signal (waitingHistogramMutex);
}

void enterWashroom (enum GenderIdentity g) {
  int waitingTime = entryTicker;
  uthread_sem_wait (washroom->stallAvailable[g]);
  uthread_sem_wait (washroom->lock);
    enum GenderIdentity other = otherGender[g];

    washroom->genderFairWaitingCount[g]++;

    if (washroom->numStallsOccupied == 0) {
      washroom->currentGender = g;
      washroom->genderFairWaitingCount[g] = 0;
      for(int i=0; i<MAX_OCCUPANCY; i++)
        uthread_sem_wait(washroom->stallAvailable[other]);
    }

    washroom->numStallsOccupied++;
    entryTicker++;
    updateTrackerValues((entryTicker - waitingTime), g, washroom->numStallsOccupied);
   uthread_sem_signal (washroom->lock);
}

void leaveWashroom(enum GenderIdentity g) {
  uthread_sem_wait(washroom->lock);
    enum GenderIdentity other = otherGender[g];
    washroom->numStallsOccupied--;
    if (washroom->numStallsOccupied == 0) {
      //last one out: check if should signal other gender, or signal anyone
      if (washroom->genderFairWaitingCount[other] > FAIR_WAITING_COUNT){
        washroom->currentGender = other;
        washroom->genderFairWaitingCount[other] = 0;
        for(int i=0; i<MAX_OCCUPANCY; i++)
          uthread_sem_signal(washroom->stallAvailable[other]);
      } else {
        for(int i=0; i<MAX_OCCUPANCY; i++)
          uthread_sem_signal(washroom->stallAvailable[other]);
        for(int i=0; i<MAX_OCCUPANCY; i++)
          uthread_sem_signal(washroom->stallAvailable[g]);
      }

    } else if (washroom->genderFairWaitingCount[other] > FAIR_WAITING_COUNT) {
      //do nothing - wait for everyone else to leave
    } else {
      uthread_sem_signal(washroom->stallAvailable[g]);
    }
  uthread_sem_signal (washroom->lock);
}



void* person(void* v) {
  intptr_t gender = (intptr_t) v;
  enum GenderIdentity g = (enum GenderIdentity) gender;

  for(int i=0; i<NUM_ITERATIONS; i++) {
    //try
    enterWashroom(g);
    //spend time in washroom
    for (int i=0; i<NUM_PEOPLE; i++);
      uthread_yield();
    //I'm done here
    leaveWashroom(g);
    //wait to return to queue
    for (int i=0; i<NUM_PEOPLE; i++);
      uthread_yield();
  }
}

int main (int argc, char** argv) {
  srand(time(0));
  uthread_init (1);
  washroom = createWashroom();
  uthread_t pt [NUM_PEOPLE];
  waitingHistogramMutex = uthread_sem_create (1);

  for (int i=0; i<sizeof(pt)/sizeof(uthread_t); i++) {
    int gender = rand() % 2;
    intptr_t g = (intptr_t) gender;
    pt[i] = uthread_create(person, (void*) g);
  }

  for (int i=0; i<sizeof(pt)/sizeof(uthread_t); i++) {
    uthread_join(pt[i], 0);
  }

  printf ("Times with 1 person who identifies as male   %d\n", occupancyHistogram [MALE]   [1]);
  printf ("Times with 2 people who identifies as male   %d\n", occupancyHistogram [MALE]   [2]);
  printf ("Times with 3 people who identifies as male   %d\n", occupancyHistogram [MALE]   [3]);
  printf ("Times with 1 person who identifies as female %d\n", occupancyHistogram [FEMALE] [1]);
  printf ("Times with 2 people who identifies as female %d\n", occupancyHistogram [FEMALE] [2]);
  printf ("Times with 3 people who identifies as female %d\n", occupancyHistogram [FEMALE] [3]);
  printf ("Waiting Histogram\n");
  for (int i=0; i<WAITING_HISTOGRAM_SIZE; i++)
  if (waitingHistogram [i])
  printf ("  Number of times people waited for %d %s to enter: %d\n", i, i==1?"person":"people", waitingHistogram [i]);
  if (waitingHistogramOverflow)
  printf ("  Number of times people waited more than %d entries: %d\n", WAITING_HISTOGRAM_SIZE, waitingHistogramOverflow);
}
