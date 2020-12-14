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




//static void
//build_test
//(
//cskiplist_t *list,
//int len,
//char* s,
//int reversed
//)
//{
//  printf("Filling list of length %d ...\n", len);
//
//  printf("before ...\n");
//  cskl_dump(list, interval_cskiplist_node_tostr);
//
//  ptrdiff_t i;
//  for (i = len; i > 0; i = i -10) {
//    ptrdiff_t lo = reversed ? i: len - i;
//    interval_t *e = interval_new((uintptr_t)(s+lo), (uintptr_t)(s+lo+8));
//    cskl_insert(list, e, malloc);
//  }
//
//  printf("after ...\n");
//  cskl_dump(list, interval_cskiplist_node_tostr);
//}



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
  cskiplist_t *cslist = cskiplist_create(MAX_HEIGHT, m_alloc);

  cskiplist_print(cslist);


#if 1
#pragma omp parallel
  {
    int id = omp_get_thread_num();

    int lo = id;
    int hi;
    for (int i = 0; i < 10; ++i) {
      lo += 10;
      hi = lo + 10;
      cskiplist_put(cslist, lo, interval_new(lo, hi));
      cskiplist_print(cslist);

    }

    cskiplist_delete_node(cslist, 20);
    cskiplist_delete_node(cslist, 70);
    cskiplist_put(cslist, 20, interval_new(20, 30));
  }
  cskiplist_print(cslist);
  cskiplist_free(cslist);

#else

//  build_test(cs, 100, 0, 4);

  cskl_check_dump(cs, interval_cskiplist_node_tostr);

#endif

  return 0;
}
