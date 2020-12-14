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


typedef void* (*mem_alloc)(size_t size);


typedef struct csklnode_t {
  int key;
  void *item;
  int height;
  volatile bool deleted;
  mcs_lock_t lock;
  // memory allocated for a node will include space for its vector of  pointers
  struct csklnode_t *nexts[];
} csklnode_t;



typedef struct cskiplist_t{
  csklnode_t *head_ptr;
  int max_height;
  int length;
  int total_length;
  mem_alloc m_alloc;
  mcs_lock_t lock;
} cskiplist_t;


cskiplist_t *
cskiplist_create
(
  int max_height,
  mem_alloc m_alloc
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


void
cskiplist_print
(
  cskiplist_t* cskl
);

#endif //PROJECT_CSKIPLIST_H
