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



//******************************************************************************
// local include files
//******************************************************************************

#include "cskiplist.h"



//******************************************************************************
// macros
//******************************************************************************

#define STRMAX     8096
#define MAX_HEIGHT 4
#define GET_NAME(var)  #var


//******************************************************************************
// types
//******************************************************************************

typedef struct {
  long start;
  long end;
} interval_t;

typedef void (*test_fn)(cskiplist_t *cskl);

//******************************************************************************
// private operations
//******************************************************************************


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
  cskiplist_t *cskl
)
{
#pragma omp for
  for (int i = 0; i < 10; i++) {
    cskiplist_put(cskl, i, interval_new(i, i + 10));
  }
}


// Test deletion
void
test1
(
  cskiplist_t *cskl
)
{

#pragma omp parallel
  {
    int id = omp_get_thread_num();

    int lo = id;
    int hi;
    for (int i = 0; i < 10; ++i) {
      lo += 10;
      hi = lo + 10;
      cskiplist_put(cskl, lo, interval_new(lo, hi));

    }

    cskiplist_delete_node(cskl, 20);
    cskiplist_delete_node(cskl, 70);

    interval_t  * item = cskiplist_get(cskl, 40);

    if (item != NULL)
      printf("item on key = %d is [ %d, %d]\n", 20, item->start, item->end);
    else
      printf("item on key = %d is NULL\n", 20);
//    cskiplist_put(cskl, 20, interval_new(20, 30));
  }
}


static void
run_test
(
  cskiplist_t *cskl,
  test_fn test,
  char* test_name
)
{
  cskiplist_t *new_cskl = cskiplist_copy_deep(cskl);

#pragma omp parallel
  {
    test(cskl);
  }
  test(new_cskl);

  cskiplist_print(cskl);
  cskiplist_print(new_cskl);

  if (cskiplist_compare(cskl, new_cskl))
    printf("%s: PASSED\n", test_name);
  else
    printf("%s: FAILED\n", test_name);

  cskiplist_free(new_cskl);
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
  cskiplist_t *cskl = cskiplist_create(MAX_HEIGHT, &malloc, &interval_copy, &interval_compare);

//#pragma omp parallel
//  {
//    test_insert(cskl);
//  }

  run_test(cskl, test_insert, "test_insert");

  cskiplist_print(cskl);
  cskiplist_free(cskl);


  return 0;
}
