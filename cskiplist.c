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
 * allocate a node of a specified max_height.
 * If item_deep_copy provided make a deep copy, otherwise just copy void *
 */
static csklnode_t *
csklnode_create
(
  int key,
  void *item,
  int max_height,
  mem_alloc_fn m_alloc,
  item_copy_fn item_deep_copy
)
{
  csklnode_t* node = (csklnode_t*)m_alloc(SIZEOF_CSKLNODE_T(max_height));
  node->key = key;
  node->item = item_deep_copy && item ? item_deep_copy(item) : item; // check if both deep_copy and items are defined
  node->height = 0;
  node->deleted = false;
  mcs_init(&(node->lock));
  for (int i = 0; i < max_height; ++i) {
    node->nexts[i] = NULL;
  }
  return node;
}

static void
csklnode_set_next
(
  csklnode_t* node,
  int lev,
  csklnode_t* next
)
{
  if (node->nexts[lev] == NULL){
    node->height++;
  }
  node->nexts[lev] = next;
}


static csklnode_t *
cskipnode_next
(
  csklnode_t *node,
  int lev
)
{
  return node->nexts[lev];
}


static csklnode_t *
cskipnode_next_alive
(
  csklnode_t *node,
  int lev
)
{
  node = node->nexts[lev];
  while (node->deleted) node = node->nexts[lev];
  return node;
}

static bool
csklnode_compare
(
  csklnode_t *node1,
  csklnode_t *node2,
  item_compare_fn i_cmp
)
{
  if (node1->key == node2->key &&
      node1->deleted == node2->deleted &&
      node1->item && node2->item && // check just in case that items are not NULL
      i_cmp(node1->item, node2->item)){
    return true;
  }
  return false;
}


static void
cskipnode_free
(
csklnode_t *node
)
{
  free(node->item);
}


/*
 * Returns previous nodes per level. Note: if find your key it will be on level[0]
 */
static csklnode_t **
cskip_find_prev_nodes
(
  cskiplist_t* cskl,
  int key
)
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
cskiplist_buld_tower
(
  csklnode_t *new_node,
  int tower_height,
  csklnode_t **prev_nodes
)
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
cskip_insert_dumy_nodes
(
  cskiplist_t *cskl
)
{
  csklnode_t *head = csklnode_create(INT_MIN, NULL, cskl->max_height, cskl->m_alloc, NULL);
  csklnode_t *tail = csklnode_create(INT_MAX, NULL, cskl->max_height, cskl->m_alloc, NULL);

  for (int i = 0; i < cskl->max_height; ++i) {
    csklnode_set_next(head, i, tail);
    csklnode_set_next(tail, i, NULL);
  }
  cskl->head_ptr = head;
}


static void
cskiplist_put_specific
(
  cskiplist_t* cskl,
  int key,
  void *item,
  int tower_height,
  item_copy_fn item_deep_copy
)
{
  // allocate my node
  csklnode_t *new_node = csklnode_create(key, item, cskl->max_height, cskl->m_alloc, item_deep_copy);

  csklnode_t **prev_nodes = cskip_find_prev_nodes(cskl, key);
  csklnode_t *next_node = prev_nodes[0]->nexts[0];

  if (prev_nodes[0]->deleted || prev_nodes[0]->key == key){
    prev_nodes[0]->item = item;
    prev_nodes[0]->deleted = false;
  }else if (next_node->deleted){
    next_node->item = item;
    next_node->deleted = false;
  }else{
    if (tower_height <= 0) tower_height = rand() % (cskl->max_height - 1)  + 1 ;
    printf("KEY: %d | height = %d\n", key, tower_height);
    cskiplist_buld_tower(new_node, tower_height, prev_nodes);
    cskl->total_length++;

  }
  cskl->length++;

  free(prev_nodes);
}

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
)
{
  cskiplist_t* cskl = (cskiplist_t*)m_alloc(sizeof(cskiplist_t));

  cskl->length = 0;
  cskl->total_length = 2;
  cskl->m_alloc = m_alloc;
  cskl->i_copy = i_copy;
  cskl->i_cmp = i_cmp;
  cskl->max_height = max_height;
  mcs_init(&cskl->lock);

  cskip_insert_dumy_nodes(cskl);

  return cskl;
}


void
cskiplist_free
(
  cskiplist_t* cskl
)
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


/*
 * Inserts node instead of deleted node, or node with the same key with their height,
 * or creates new node with specified/random height(if spec==0)
 * Create a shell copy of item
 */

void
cskiplist_put
(
  cskiplist_t* cskl,
  int key,
  void *item
)
{
  cskiplist_put_specific(cskl, key, item, 0, NULL);
}


// CAS, no ABA problem as no deletion
void *
cskiplist_get
(
  cskiplist_t* cskl,
  int key
)
{
  csklnode_t *prev_node = cskip_find_prev_nodes(cskl, key)[0];
  if (prev_node->key == key && !prev_node->deleted) {
    return prev_node->item;
  }else{
    return NULL;
  }
}


void
cskiplist_delete_node
(
  cskiplist_t* cskl,
  int key
)
{
  csklnode_t *prev_node = cskip_find_prev_nodes(cskl, key)[0];
  if (prev_node->key == key && !prev_node->deleted) {
    prev_node->deleted = true;
    cskl->length--;
  }
}


/*
 * This is shallow copy, as the copy nodes will point to the same items
 */
cskiplist_t *
cskiplist_copy
(
  cskiplist_t* cskl
)
{
  cskiplist_t* new_cskl = cskiplist_create(cskl->max_height, cskl->m_alloc, cskl->i_copy, cskl->i_cmp);

  for (csklnode_t *cur_node=cskl->head_ptr->nexts[0]; cur_node->nexts[0] != NULL; cur_node = cur_node->nexts[0]) {
    cskiplist_put_specific(new_cskl, cur_node->key, cur_node->item, cur_node->height, NULL);
  }
  return new_cskl;
}


cskiplist_t *
cskiplist_copy_deep
(
cskiplist_t* cskl
)
{
  cskiplist_t* new_cskl = cskiplist_create(cskl->max_height, cskl->m_alloc, cskl->i_copy, cskl->i_cmp);

  // Dummy nodes are already there, so start inserting from node 1
  for (csklnode_t *cur_node=cskl->head_ptr->nexts[0]; cur_node->nexts[0] != NULL; cur_node = cur_node->nexts[0]) {
    cskiplist_put_specific(new_cskl, cur_node->key, cur_node->item, cur_node->height, cskl->i_copy);
  }
  return new_cskl;
}


bool
cskiplist_compare
(
  cskiplist_t* cskl1,
  cskiplist_t* cskl2
)
{
  if (cskl1->length != cskl2->length){
    return false;
  }

  // Start comparing from first valid node
  csklnode_t *node1=cskl1->head_ptr->nexts[0];
  csklnode_t *node2=cskl2->head_ptr->nexts[0];

  while (node1->nexts[0] != NULL || node2->nexts[0] != NULL) {
    if (csklnode_compare(node1, node2, cskl1->i_cmp) == false){
      return false;
    }
    node1 = node1->nexts[0];
    node2 = node2->nexts[0];

  }
  return true;
}



void
cskiplist_print(cskiplist_t* cskl)
{
  int full_len = cskl->total_length;

  char **str_nodes = (char**) malloc(sizeof(char*) * cskl->max_height);
  char *str_deleted = (char*) malloc(sizeof(char) * (full_len + 1));
  int *keys = (int*) malloc(sizeof(int) * (full_len + 1));


  // Initialize maps
  for (int i = 0; i < cskl->max_height; ++i) {
    str_nodes[i] = (char*) malloc(sizeof(char) * (full_len + 1));

    for (int j = 0; j < full_len; ++j) {
      str_nodes[i][j] = '.';
    }
    str_nodes[i][full_len] = '\0';
  }
  str_deleted[full_len] = '\0';


  // Update maps
  int id = 0;
  for (csklnode_t *cur_node=cskl->head_ptr; id < full_len; cur_node = cur_node->nexts[0]){
    for (int lev = 0; lev < cur_node->height ; ++lev) {
      str_nodes[lev][id] = 'X';
    }

    keys[id] = cur_node->key;

    if (cur_node->deleted){
      str_deleted[id] = 'd';
    }else{
      str_deleted[id] = '_';
    }
    ++id;
  }

  // Print Elements
  printf("********************** Concurrent Skip List *********************\n");
  printf("Elements = %d\n", full_len);
  for (int lev = cskl->max_height - 1; 0 <= lev ; --lev) {
    printf("L%d: ", lev);
    for (int i = 0; i < cskl->total_length; ++i) {
      printf("\t%c", str_nodes[lev][i]);
    }
    printf("\n");
  }


  printf("Del:");
  for (int i = 0; i < cskl->total_length; ++i) {
    printf("\t%c", str_deleted[i]);
  }
  printf("\n");


  printf("Keys:");
  printf("\t-inf");
  for (int i = 1; i < cskl->total_length-1; ++i) {
    printf("\t%d", keys[i]);
  }
  printf("\t+inf\n");




  // Free dynamic allocation
  for (int i = 0; i < cskl->max_height; ++i) {
    free(str_nodes[i]);
  }
  free(str_nodes);
  free(str_deleted);
}
