//
// Created by dejan on 12/13/20.
//

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "cskiplist.h"


//******************************************************************************
// macros
//******************************************************************************


// number of bytes in a skip list node with L levels
#define SIZEOF_CSKLNODE_T(L) (sizeof(csklnode_t) + sizeof(void*) * L)
#define mcs_nil (struct mcs_node_s*) 0
#define MAX_LEVEL 10


//******************************************************************************
// implementation types
//******************************************************************************

static struct {
  csklnode_t *nodes[MAX_LEVEL];
}csklnodes_t;

//******************************************************************************
// private operations
//******************************************************************************

/*
 * allocate a node of a specified height.
 */
static csklnode_t *
csklnode_create(int key, void *item, int height, mem_alloc m_alloc)
{
  csklnode_t* node = (csklnode_t*)m_alloc(SIZEOF_CSKLNODE_T(height));
  node->key = key;
  node->item = item;
  node->height = 0;
  node->deleted = false;
  mcs_init(&(node->lock));
  for (int i = 0; i < height; ++i) {
    node->nexts[i] = NULL;
  }
  return node;
}

static void
csklnode_set_next(csklnode_t* node, int lev, csklnode_t* next)
{
  if (node->nexts[lev] == NULL){
    node->height++;
  }
  node->nexts[lev] = next;

}

static void
cskipnode_free(csklnode_t *node)
{
  free(node->item);
}


static csklnode_t *
cskipnode_next(csklnode_t *node, int lev){
  return node->nexts[lev];
}


static csklnode_t *
cskipnode_next_alive(csklnode_t *node, int lev){
  node = node->nexts[lev];
  while (node->deleted) node = node->nexts[lev];
  return node;
}


/*
 * Returns previous nodes per level. Note: if find your key it will be on level[0]
 */
static csklnode_t **
cskip_find_prev_nodes(cskiplist_t* cskl, int key)
{
  csklnode_t **prev_nodes = cskl->m_alloc(sizeof(csklnode_t *) * cskl->max_height);

  csklnode_t *prev_node = cskl->head_ptr;
  csklnode_t *cur_node;

  // For every level
  for (int lev = cskl->max_height-1; lev >= 0 ; --lev) {

    cur_node = cskipnode_next(prev_node, lev);
    // if cur_node->key is bigger than key, prev_node is what we want to return
    while (cur_node->key <= key){
      prev_node = cskipnode_next(prev_node, lev);
      cur_node = cskipnode_next(cur_node, lev);
    }
    prev_nodes[lev] = prev_node;

  }
  return prev_nodes;
}


static void
cskiplist_buld_tower(csklnode_t *new_node, int tower_height, csklnode_t **prev_nodes)
{

  csklnode_t *cur_node = cskipnode_next(prev_nodes[0], 0);

  for (int lev = 0; lev < tower_height; ++lev) {

    if (lev < cur_node->height){
      csklnode_set_next(new_node, lev, cur_node);
      csklnode_set_next(prev_nodes[lev], lev, new_node);
    }else{
      cur_node = cskipnode_next(cur_node, lev);
    }
  }
}

static void
cskip_insert_dumy_nodes(cskiplist_t *cskl)
{
  csklnode_t *head = csklnode_create(INT_MIN, NULL, cskl->max_height, cskl->m_alloc);
  csklnode_t *tail = csklnode_create(INT_MAX, NULL, cskl->max_height, cskl->m_alloc);

  for (int i = 0; i < cskl->max_height; ++i) {
    csklnode_set_next(head, i, tail);
    csklnode_set_next(tail, i, NULL);
  }
  cskl->head_ptr = head;
}



//******************************************************************************
// interface operations
//******************************************************************************


cskiplist_t *
cskiplist_create
(
  int max_height,
  mem_alloc m_alloc
)
{
  cskiplist_t* cskl = (cskiplist_t*)m_alloc(sizeof(cskiplist_t));

  cskl->length = 0;
  cskl->total_length = 2;
  cskl->m_alloc = m_alloc;
  cskl->max_height = max_height;
  mcs_init(&cskl->lock);

  cskip_insert_dumy_nodes(cskl);

  return cskl;
}


void
cskiplist_free(cskiplist_t* cskl)
{
  csklnode_t *node = cskl->head_ptr;
  csklnode_t *node_next = cskl->head_ptr;

  //TODO: Lock this
//  mcs_lock(&cskl->lock);
  while (node->nexts[0] != NULL){ // only on last node
    node_next = node->nexts[0];
    cskipnode_free(node);
    node = node_next;
  }
//  cskipnode_free(node);
//  mcs_unlock(&cskl->lock);
}


void
cskiplist_put(cskiplist_t* cskl, int key, void *item)
{
  // allocate my node
  csklnode_t *new_node = csklnode_create(key, item, cskl->max_height, cskl->m_alloc);

  csklnode_t **prev_nodes = cskip_find_prev_nodes(cskl, key);
  csklnode_t *next_node = prev_nodes[0]->nexts[0];

  if (prev_nodes[0]->deleted || prev_nodes[0]->key == key){
    prev_nodes[0]->item = item;
    prev_nodes[0]->deleted = false;
  }else if (next_node->deleted){
    next_node->item = item;
    next_node->deleted = false;
  }else{
    int tower_height = rand() % (cskl->max_height - 1)  + 1 ;
    printf("Tower height = %d\n", tower_height);
    cskiplist_buld_tower(new_node, tower_height, prev_nodes);
    cskl->total_length++;

  }
  cskl->length++;

  free(prev_nodes);
}


void *
cskiplist_get(cskiplist_t* cskl, int key) // CAS, no ABA problem as no deletion
{
  csklnode_t *prev_node = cskip_find_prev_nodes(cskl, key)[0];
  if (prev_node->key == key) {
    return prev_node->item;
  }else{
    return NULL;
  }
}


void
cskiplist_delete_node(cskiplist_t* cskl, int key)
{
  csklnode_t *prev_node = cskip_find_prev_nodes(cskl, key)[0];
  if (prev_node->key == key && !prev_node->deleted) {
    prev_node->deleted = true;
    cskl->length--;
  }
}


void
cskiplist_print(cskiplist_t* cskl)
{
  int full_len = cskl->total_length;

  char **str_nodes = (char**) malloc(sizeof(char*) * cskl->max_height);
  char *str_deleted = (char*) malloc(sizeof(char) * (full_len + 1));

  for (int i = 0; i < cskl->max_height; ++i) {
    str_nodes[i] = (char*) malloc(sizeof(char) * (full_len + 1));

    for (int j = 0; j < full_len; ++j) {
      str_nodes[i][j] = '.';
    }
    str_nodes[i][full_len] = '\0';
  }
  str_deleted[full_len] = '\0';

  int id = 0;
  for (csklnode_t *cur_node=cskl->head_ptr; id < full_len; cur_node = cur_node->nexts[0]){
    for (int lev = 0; lev < cur_node->height ; ++lev) {
      str_nodes[lev][id] = 'X';
    }
    if (cur_node->deleted){
      str_deleted[id] = 'd';
    }else{
      str_deleted[id] = '_';
    }
    ++id;
  }

  printf("********************** Concurrent Skip List *********************\n");
  printf("Elements = %d\n", full_len);
  for (int lev = cskl->max_height - 1; 0 <= lev ; --lev) {
    printf("L%d: %s\n", lev, str_nodes[lev]);
  }
  printf("Del:%s\n", str_deleted);


  for (int i = 0; i < cskl->max_height; ++i) {
    free(str_nodes[i]);
  }
  free(str_nodes);
  free(str_deleted);
}
