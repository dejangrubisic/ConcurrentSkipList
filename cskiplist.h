//
// Created by dejan on 12/13/20.
//


/*
 * This concurrent skip list is designed for supporting loadmap table as ordered map.
 * Table is initialized serially, while multiple threads can insert and read from the
 * table in parallel. Deletion is implemented with setting deletion_bit which avoids
 * ABA problem. Amortized costs for insert, read and delete are O(logN). Number of
 * elements in the skiplist will be around 200. No need for cache behavior.
 */

#ifndef PROJECT_CSKIPLIST_H
#define PROJECT_CSKIPLIST_H

#include <stddef.h>
#include <stdbool.h>
#include "mcs-lock.h"
#include <stdatomic.h>


//******************************************************************************
// implementation types
//******************************************************************************

typedef void* (*mem_alloc_fn)(size_t size);
typedef void* (*item_copy_fn)(void *item);
typedef bool (*item_compare_fn)(void *item1, void *item2);


typedef struct {
  unsigned long deleted : 1;  // delete flag
  unsigned long ptr : 63;     // void *
}node_item_t;


typedef struct csklnode_t {
  int key;
  _Atomic(void *) item; // the lowest bit will be delete bit 0 - alive, 1 - deleted
  int height;
  // memory allocated for a node will include space for its vector of  pointers
  _Atomic(struct csklnode_t *) nexts[];

} csklnode_t;


typedef struct cskiplist_t{
  csklnode_t *head_ptr;
  int max_height;
  int length;
  int total_length;
  mem_alloc_fn m_alloc;
  item_copy_fn i_copy;
  item_compare_fn i_cmp;
  mcs_lock_t lock;
} cskiplist_t;



//******************************************************************************
// interface operations
//******************************************************************************

cskiplist_t *
cskiplist_create
(
  int max_height,
  mem_alloc_fn m_alloc,
  item_copy_fn i_copy,
  item_compare_fn i_cmp
);


void
cskiplist_free
(
  cskiplist_t* cskl
);


void
cskiplist_put
(
  cskiplist_t* cskl,
  int key,
  void *item
);


void *
cskiplist_get
(
  cskiplist_t* cskl,
  int key
);


void
cskiplist_delete_node
(
  cskiplist_t* cskl,
  int key
);


cskiplist_t *
cskiplist_copy
(
  cskiplist_t* cskl
);


cskiplist_t *
cskiplist_copy_deep
(
cskiplist_t* cskl
);


bool
cskiplist_compare
(
  cskiplist_t* cskl1,
  cskiplist_t* cskl2
);


void
cskiplist_print
(
  cskiplist_t* cskl
);

#endif //PROJECT_CSKIPLIST_H
