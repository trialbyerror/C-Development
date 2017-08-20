#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "uthread.h"
#include "uthread_sem.h"

#define MAX_ITEMS 10
const int NUM_ITERATIONS = 200;
const int NUM_CONSUMERS  = 2;
const int NUM_PRODUCERS  = 2;

int producer_wait_count;     // # of times producer had to wait
int consumer_wait_count;     // # of times consumer had to wait
int histogram [MAX_ITEMS+1]; // histogram [i] == # of times list stored i items

int items = 0;
uthread_sem_t itemSlots;
uthread_sem_t itemsReady;
uthread_sem_t itemsLock;

void* producer (void* v) {
  for (int i=0; i<NUM_ITERATIONS; i++) {
    uthread_sem_wait(itemSlots);
    producer_wait_count++;

    uthread_sem_wait(itemsLock);
      items++;
      histogram[items]++;
      assert(0 <= items <= 10);
    uthread_sem_signal(itemsLock);

    uthread_sem_signal(itemsReady);
  }
  return NULL;
}

void* consumer (void* v) {
  for (int i=0; i<NUM_ITERATIONS; i++) {
    uthread_sem_wait(itemsReady);
    consumer_wait_count++;

    uthread_sem_wait(itemsLock);
      items--;
      histogram[items]++;
      assert(0 <= items <= 10);
    uthread_sem_signal(itemsLock);

    uthread_sem_signal(itemSlots);
  }
  return NULL;
}

int main (int argc, char** argv) {
  uthread_t t[4];

  uthread_init (4);

  itemSlots = uthread_sem_create(MAX_ITEMS);
  itemsReady = uthread_sem_create(0);
  itemsLock = uthread_sem_create(1);

  t[0] = uthread_create(consumer, 0);
  t[1] = uthread_create(consumer, 0);
  t[2] = uthread_create(producer, 0);
  t[3] = uthread_create(producer, 0);

  uthread_join(t[0], 0);
  uthread_join(t[1], 0);
  uthread_join(t[2], 0);
  uthread_join(t[3], 0);

  printf ("producer_wait_count=%d, consumer_wait_count=%d\n", producer_wait_count, consumer_wait_count);
  printf ("items value histogram:\n");
  int sum=0;
  for (int i = 0; i <= MAX_ITEMS; i++) {
    printf ("  items=%d, %d times\n", i, histogram [i]);
    sum += histogram [i];
  }
  assert (sum == sizeof (t) / sizeof (uthread_t) * NUM_ITERATIONS);
}
