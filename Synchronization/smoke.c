#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "uthread.h"
#include "uthread_mutex_cond.h"

#define NUM_ITERATIONS 1000

#ifdef VERBOSE
#define VERBOSE_PRINT(S, ...) printf (S, ##__VA_ARGS__);
#else
#define VERBOSE_PRINT(S, ...) ;
#endif

struct Agent {
  uthread_mutex_t mutex;
  uthread_cond_t  match;
  uthread_cond_t  paper;
  uthread_cond_t  tobacco;
  uthread_cond_t  smoke;
};

struct Agent* createAgent() {
  struct Agent* agent = malloc (sizeof (struct Agent));
  agent->mutex   = uthread_mutex_create();
  agent->paper   = uthread_cond_create (agent->mutex);
  agent->match   = uthread_cond_create (agent->mutex);
  agent->tobacco = uthread_cond_create (agent->mutex);
  agent->smoke   = uthread_cond_create (agent->mutex);
  return agent;
}

//
// TODO
// You will probably need to add some procedures and struct etc.
//

/**
 * You might find these declarations helpful.
 *   Note that Resource enum had values 1, 2 and 4 so you can combine resources;
 *   e.g., having a MATCH and PAPER is the value MATCH | PAPER == 1 | 2 == 3
 */
enum Resource            {    MATCH = 1, PAPER = 2,   TOBACCO = 4};
char* resource_name [] = {"", "match",   "paper", "", "tobacco"};


int currentResource = 0;
int haveTwoThings = 0;
int threadsStarted = 0;

uthread_cond_t readyTSmoker;
uthread_cond_t readyPSmoker;
uthread_cond_t readyMSmoker;

int signal_count [5];  // # of times resource signalled
int smoke_count  [5];  // # of times smoker with resource smoked

void* tSmoker(void* v) {
  struct Agent* a = v;

  uthread_mutex_lock(a->mutex);

  while(1){
      threadsStarted++;
      uthread_cond_wait(readyTSmoker);
      currentResource = 0;
      uthread_cond_signal(a->smoke);
      smoke_count[TOBACCO]++;
  }

  uthread_mutex_unlock(a->mutex);
}

void* pSmoker(void* v){
  struct Agent* a = v;

  uthread_mutex_lock(a->mutex);

  while(1){
      threadsStarted++;
      uthread_cond_wait(readyPSmoker);
      currentResource = 0;
      uthread_cond_signal(a->smoke);
      smoke_count[PAPER]++;
  }

  uthread_mutex_unlock(a->mutex);
}

void* mSmoker(void* v){
  struct Agent* a = v;

  uthread_mutex_lock(a->mutex);

  while(1){
      threadsStarted++;
      uthread_cond_wait(readyMSmoker);
      currentResource = 0;
      uthread_cond_signal(a->smoke);
      smoke_count[MATCH]++;
  }

  uthread_mutex_unlock(a->mutex);
}

void* match(void* v){

  struct Agent* a = v;

  uthread_mutex_lock(a->mutex);

  while(1){
    threadsStarted++;
    uthread_cond_wait(a->match);
    currentResource += MATCH;
    //currentResource indicates which resources available
    haveTwoThings += 1;
    //haveTwoThings indicates whether two resources are available

    if (haveTwoThings == 2) {
      //true when I'm the second one incremented
      //otherwise skip this loop and wait again
      if (currentResource == MATCH + PAPER){
        haveTwoThings = 0;
        uthread_cond_signal(readyTSmoker);
      } else if (currentResource == MATCH + TOBACCO){
        haveTwoThings = 0;
        uthread_cond_signal(readyPSmoker);
      }
    }
  }

  uthread_mutex_unlock(a->mutex);
}

void* paper(void* v){
  struct Agent* a = v;

  uthread_mutex_lock(a->mutex);

  while(1){
    threadsStarted++;
    uthread_cond_wait(a->paper);
    currentResource += PAPER;
    //currentResource indicates which resources available
    haveTwoThings += 1;
    //haveTwoThings indicates whether two resources are available

    if (haveTwoThings == 2) {
      //true when I'm the second one incremented
      //otherwise skip this loop and wait again
      if (currentResource == MATCH + PAPER){
        haveTwoThings = 0;
        uthread_cond_signal(readyTSmoker);

      } else if (currentResource == TOBACCO + PAPER){
        haveTwoThings = 0;
        uthread_cond_signal(readyMSmoker);
      }
    }
  }

  uthread_mutex_unlock(a->mutex);
}

void* tobacco(void* v){

  struct Agent* a = v;

  uthread_mutex_lock(a->mutex);

  while(1){
    threadsStarted++;
    uthread_cond_wait(a->tobacco);
    currentResource+=TOBACCO;
    //currentResource indicates which resources available
    haveTwoThings += 1;
    //haveTwoThings indicates whether two resources are available

    if (haveTwoThings == 2) {
      //true when I'm the second one incremented
      //otherwise skip this loop and wait again
      if(currentResource == MATCH + TOBACCO){
        haveTwoThings = 0;
        uthread_cond_signal(readyPSmoker);

      } else if (currentResource == PAPER + TOBACCO){
        haveTwoThings = 0;
        uthread_cond_signal(readyMSmoker);
      }
    }
  }

  uthread_mutex_unlock(a->mutex);
}



/**
 * This is the agent procedure.  It is complete and you shouldn't change it in
 * any material way.  You can re-write it if you like, but be sure that all it does
 * is choose 2 random reasources, signal their condition variables, and then wait
 * wait for a smoker to smoke.
 */
void* agent (void* av) {
  struct Agent* a = av;
  static const int choices[]         = {MATCH|PAPER, MATCH|TOBACCO, PAPER|TOBACCO};
  static const int matching_smoker[] = {TOBACCO,     PAPER,         MATCH};

  uthread_mutex_lock (a->mutex);
    for (int i = 0; i < NUM_ITERATIONS; i++) {
      int r = rand() % 3;
      signal_count [matching_smoker [r]] ++;
      int c = choices [r];
      if (c & MATCH) {
        VERBOSE_PRINT ("match available\n");
        uthread_cond_signal (a->match);
      }
      if (c & PAPER) {
        VERBOSE_PRINT ("paper available\n");
        uthread_cond_signal (a->paper);
      }
      if (c & TOBACCO) {
        VERBOSE_PRINT ("tobacco available\n");
        uthread_cond_signal (a->tobacco);
      }
      VERBOSE_PRINT ("agent is waiting for smoker to smoke\n");
      uthread_cond_wait (a->smoke);
    }
  uthread_mutex_unlock (a->mutex);
  return NULL;
}

int main (int argc, char** argv) {
  uthread_init (7);
  srand(time(0));
  struct Agent* a = createAgent();

  readyTSmoker = uthread_cond_create(a->mutex);
  readyPSmoker = uthread_cond_create(a->mutex);
  readyMSmoker = uthread_cond_create(a->mutex);

  uthread_t m1 = uthread_create(mSmoker, a);
  uthread_t p1 = uthread_create(pSmoker, a);
  uthread_t t1 = uthread_create(tSmoker, a);
  uthread_t m0 = uthread_create(match, a);
  uthread_t p0 = uthread_create(paper, a);
  uthread_t t0 = uthread_create(tobacco, a);

  while(threadsStarted < 6) ; //ensure agent routine starts last

  uthread_join (uthread_create (agent, a), 0);
  assert (signal_count [MATCH]   == smoke_count [MATCH]);
  assert (signal_count [PAPER]   == smoke_count [PAPER]);
  assert (signal_count [TOBACCO] == smoke_count [TOBACCO]);
  assert (smoke_count [MATCH] + smoke_count [PAPER] + smoke_count [TOBACCO] == NUM_ITERATIONS);
  printf ("Smoke counts: %d matches, %d paper, %d tobacco\n",
          smoke_count [MATCH], smoke_count [PAPER], smoke_count [TOBACCO]);
}
