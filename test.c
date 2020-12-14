//
// Created by dejan on 12/13/20.
//


//******************************************************************************
// global include files
//******************************************************************************

#include <stdio.h>
#include <stdlib.h>
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



//******************************************************************************
// types
//******************************************************************************

typedef struct {
void *start;
void *end;
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

  result->start = (void *) start_l;
  result->end = (void *) end_l;

  return result;
}



// Test insertion
void
test_insert
(
  cskiplist_t *cskl
)
{
    int id = omp_get_thread_num();

    int lo = id;
    int hi;
    for (int i = 0; i < 10; ++i) {
      lo += 10;
      hi = lo + 10;
      cskiplist_put(cskl, lo, interval_new(lo, hi), 0);
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
      cskiplist_put(cskl, lo, interval_new(lo, hi), 0);

    }

    cskiplist_delete_node(cskl, 20);
    cskiplist_delete_node(cskl, 70);

    interval_t  * item = cskiplist_get(cskl, 40);

    if (item != NULL)
      printf("item on key = %d is [ %d, %d]\n", 20, item->start, item->end);
    else
      printf("item on key = %d is NULL\n", 20);
//    cskiplist_put(cskl, 20, interval_new(20, 30), 0);
  }
}



static bool
check_test
(
  cskiplist_t *cskl,
  test_fn test
)
{

#pragma omp parallel
  {
    test(cskl);
  }
  cskiplist_print(cskl);

  cskiplist_t *new_cskl = cskiplist_copy(cskl);

  cskiplist_print(cskl);

//  test(cskl);
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
  mem_alloc m_alloc = &malloc;
  cskiplist_t *cskl = cskiplist_create(MAX_HEIGHT, m_alloc);

//#pragma omp parallel
//  {
//    test_insert(cskl);
//  }

  check_test(cskl, test_insert);

  cskiplist_print(cskl);
  cskiplist_free(cskl);


  return 0;
}
