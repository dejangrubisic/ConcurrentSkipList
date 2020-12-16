//
// Created by dejan on 12/13/20.
//


//******************************************************************************
// global include files
//******************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <assert.h>
#include <math.h>



//******************************************************************************
// local include files
//******************************************************************************

#include "cskiplist.h"
#include "timer.h"



//******************************************************************************
// macros
//******************************************************************************

#define DEBUG 0

#if DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif



#define STRMAX     8096
#define MAX_HEIGHT 4
#define GET_NAME(var)  #var

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

//******************************************************************************
// types
//******************************************************************************

typedef struct {
  long start;
  long end;
} interval_t;

typedef void (*test_fn)(cskiplist_t *cskl, int *rand_keys, int num_range );

//******************************************************************************
// private operations
//******************************************************************************

static void
exchange_int
(
  int *a,
  int *b
)
{
  int tmp = *a;
  *a = *b;
  *b = tmp;
}


static int *
create_random_keys
(
  int num_range
)
{
  int *rand_keys = (int*) malloc(sizeof(int)*num_range);

  for (int i = 0; i < num_range; ++i) {
    rand_keys[i] = i;
  }

  int rand_id;
  for (int i = 0; i < num_range; ++i) {
    rand_id = rand() % num_range;
    exchange_int(&rand_keys[i], &rand_keys[rand_id]);
  }

  return rand_keys;
}

static interval_t *
interval_new
(
  long start_l,
  long end_l
)
{
  interval_t *result = (interval_t *) malloc(sizeof(interval_t));

  result->start = start_l;
  result->end = end_l;

  return result;
}


void *
interval_copy
(
  void *interval_in
)
{
  interval_t *interval = (interval_t *) interval_in;
  return (void *) interval_new(interval->start, interval->end);
}


static bool
interval_compare
(
  void *interval1_in,
  void *interval2_in
)
{

  interval_t *interval1 = (interval_t *) interval1_in;
  interval_t *interval2 = (interval_t *) interval2_in;

  if (interval1->start == interval2->start &&
      interval1->end   == interval2->end){
    return true;
  }
  return false;
}


// Test insertion
void
test_insert
(
  cskiplist_t *cskl,
  int *rand_keys,
  int num_range
)
{

#pragma omp single
  PRINT("Insert Keys: ");


#pragma omp for
  for (int i = 0; i < num_range; i++) {

  PRINT("%d, ", rand_keys[i]);

    cskiplist_put(cskl, rand_keys[i], interval_new(rand_keys[i], rand_keys[i] + 10));
  }

#pragma omp single
  PRINT("\n");
}


// Test deletion
void
test_delete
(
  cskiplist_t *cskl,
  int *rand_keys,
  int num_range
)
{

#pragma omp single
{
  PRINT("Delete Keys: ");
  for (int i = 0; i < num_range; ++i) {
    PRINT("%d, ", rand_keys[i]);
  }
  PRINT("\n");
}

#pragma omp for
  for (int i = 0; i < num_range; i++) {
    cskiplist_delete_node(cskl, rand_keys[i]);
  }

}


// Test both insert and delete
void
test_random
(
  cskiplist_t *cskl,
  int *rand_keys,
  int num_range
)
{
#pragma omp single
  {
    PRINT("RANDOM TEST\n\n");
  }

  // keys are going from 0 to num_range
  int my_id = omp_get_thread_num();
  int num_threads = omp_get_num_threads();
  int block_size =  num_range / num_threads ;

  assert(num_range % num_threads == 0);

  bool *visited = (bool*)calloc(block_size, sizeof(bool));
  int *my_head = rand_keys + my_id * block_size;

  int i = 0;
  while (i < block_size) {
    int key = my_head[i];
    int rand_offset = rand() % (block_size - i);

    if (!visited[key]){
      PRINT("T%d: put [ key = %d]\n", my_id, key);
      cskiplist_put(cskl, key, interval_new(key, key + 10));

      visited[key] = true;
      exchange_int(&my_head[i], &my_head[i+rand_offset]);
    }else{
      PRINT("T%d: delete [ key = %d]\n", my_id, key);
      cskiplist_delete_node(cskl, key);
      i++;
    }
  }

}


static void
run_test
(
  cskiplist_t *cskl,
  test_fn test,
  char* test_name,
  int num_range
)
{
  cskiplist_t *new_cskl = cskiplist_copy_deep(cskl);
  int *rand_keys = create_random_keys(num_range);
  int num_threads;
  PRINT("STARTING Length1 = %d, Length2 = %d\n",
         atomic_load(&cskl->length), atomic_load(&new_cskl->length));


  timer_start();
#pragma omp parallel
  {
    num_threads = omp_get_num_threads();
    test(cskl, rand_keys, num_range);
  }
  printf("%d thread(s) TIME = %f\n", num_threads, timer_elapsed());


  timer_start();
  test(new_cskl, rand_keys, num_range);
  printf("Serial TIME = %f\n", timer_elapsed());

#if DEBUG
  cskiplist_print(cskl);
  cskiplist_print(new_cskl);
#endif

  if (cskiplist_compare(cskl, new_cskl))
    printf("%s: %sPASSED%s\n", test_name, KGRN, KNRM);
  else
    printf("%s: %sFAILED%s\n", test_name, KRED, KNRM);
  printf("__________________________________________________________\n\n");

  cskiplist_free(new_cskl);
  free(rand_keys);
}


//******************************************************************************
// interface operations
//******************************************************************************


int
main
(
  int argc,
  char **argv
)
{
  int num_range = 200;

  cskiplist_t *cskl = cskiplist_create(MAX_HEIGHT, &malloc, &interval_copy, interval_compare);

  run_test(cskl, test_insert, "test_insert", num_range);
  run_test(cskl, test_delete, "test_delete", num_range);

  run_test(cskl, test_random, "test_random", num_range);


  cskiplist_free(cskl);


  return 0;
}
