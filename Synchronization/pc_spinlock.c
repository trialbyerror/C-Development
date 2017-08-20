#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "uthread.h"
#include "uthread_mutex_cond.h"
#include "spinlock.h"

#define MAX_ITEMS 10
const int NUM_ITERATIONS = 200;
const int NUM_CONSUMERS  = 2;
const int NUM_PRODUCERS  = 2;

int producer_wait_count;     // # of times producer had to wait
int consumer_wait_count;     // # of times consumer had to wait
int histogram [MAX_ITEMS+1]; // histogram [i] == # of times list stored i items

int items = 0;
spinlock_t lock;

void* producer (void* v) {
  for (int i=0; i<NUM_ITERATIONS; i++) {
    while (items == MAX_ITEMS){
      producer_wait_count++;
      spinlock_lock(&lock);
        if (items == MAX_ITEMS) {
          //printf("checking condition producer: items is %d\n", items);
          spinlock_unlock(&lock);
          //printf("unlocked producer \n");
        } else {
          break;
        }
    }
    //printf("condition passed in producer: items is %d\n", items);

      items++;
      //printf("prod inc: items is %d\n", items);
      histogram[items]++;
      //printf("cons histogram inc: hist[%d] is %d\n", items, histogram[items]);
    spinlock_unlock(&lock);
    assert(0 <= items <= MAX_ITEMS);
  }
  return NULL;
}

void* consumer (void* v) {
  for (int i=0; i<NUM_ITERATIONS; i++) {
    while (items == 0){
      consumer_wait_count++;
      spinlock_lock(&lock);
        if (items == 0) {
          //printf("checking condition consumer: items is %d\n", items);
          spinlock_unlock(&lock);
          //printf("unlocked consumer\n");
        } else {
          break;
        }
    }
     //printf("condition passed in consumer: items is %d\n", items);
        items--;
        //printf("consumer dec: items is %d\n", items);
        histogram[items]++;
        //printf("cons histogram inc: hist[%d] is %d\n", items, histogram[items]);
        spinlock_unlock(&lock);
       // printf("unlocked consumer\n");
      //}

    assert(0 <= items <= MAX_ITEMS);
  }
  return NULL;
}

int main (int argc, char** argv) {
  uthread_t t[4];

  uthread_init (4);

  spinlock_create(&lock);
  for(int i=0; i<NUM_CONSUMERS; i++){
    t[i] = uthread_create(consumer, 0);
    //printf("created consumer %d\n", i);
  } for(int i=NUM_CONSUMERS; i<sizeof(t)/sizeof(uthread_t); i++){
    t[i] = uthread_create(producer, 0);
    //printf("created producer %d\n", i);
  }

  for(int i=0; i<sizeof(t)/sizeof(uthread_t); i++){
    uthread_join(t[i], 0);
    //printf("joined thread %d\n", i);
  }

  printf ("producer_wait_count=%d, consumer_wait_count=%d\n", producer_wait_count, consumer_wait_count);
  printf ("items value histogram:\n");
  int sum=0;
  for (int i = 0; i <= MAX_ITEMS; i++) {
    printf ("  items=%d, %d times\n", i, histogram [i]);
    sum += histogram [i];
  }
  assert (sum == sizeof (t) / sizeof (uthread_t) * NUM_ITERATIONS);
}
