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
  for (int i = 10; i >= 0; i--) {
//  for (int i = 0;  i < 10; i++) {
    cskiplist_put(cskl, i, interval_new(i, i + 10));
  }
}


// Test deletion
void
test_delete
(
  cskiplist_t *cskl
)
{
  // keys are going from 1 to 10
  int rand_key[] = {3, 15, 2, 3, 7,
                    7,  7, 7, 7, 0};

  printf("Delete Keys: ");
  for (int i = 0; i < 10; ++i) {
    printf("%d, ", rand_key[i]);
  }
  printf("\n");

#pragma omp for
  for (int i = 0; i < 10; i++) {
    cskiplist_delete_node(cskl, rand_key[i]);
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
    printf("%s: %sPASSED%s\n", test_name, KGRN, KNRM);
  else
    printf("%s: %sFAILED%s\n", test_name, KRED, KNRM);
  printf("__________________________________________________________\n\n");

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
  cskiplist_t *cskl = cskiplist_create(MAX_HEIGHT, &malloc, &interval_copy, NULL);

//#pragma omp parallel
//  {
//    test_insert(cskl);
//  }

  run_test(cskl, test_insert, "test_insert");
//  run_test(cskl, test_delete, "test_delete");

  cskiplist_free(cskl);


  return 0;
}
