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

#define DEBUG 0

// number of bytes in a skip list node with L levels
#define SIZEOF_CSKLNODE_T(L) (sizeof(csklnode_t) + sizeof(unsigned long) * L)
#define mcs_nil (struct mcs_node_s*) 0
#define MAX_LEVEL 10

#define SHELL_COPY 0
#define DEEP_COPY 1

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"


#define CAS(obj, exp, des) \
      atomic_compare_exchange_strong(obj, exp, des)
#define atomic_add(loc, value) \
	atomic_fetch_add_explicit(loc, value, memory_order_relaxed)

//******************************************************************************
// implementation types
//******************************************************************************

typedef struct{
  csklnode_t **ptrs; // pointer for each level of previous node
  csklnode_t **old_nexts; // each level nexts
}cskl_prev_nodes_t;

static cskl_prev_nodes_t *
new_prev_nodes
(
  mem_alloc_fn m_alloc,
  int max_height
)
{
  cskl_prev_nodes_t *prev_nodes = (cskl_prev_nodes_t *)m_alloc(sizeof(cskl_prev_nodes_t));
  prev_nodes->ptrs = (csklnode_t **) m_alloc(sizeof(csklnode_t *) * max_height);
  prev_nodes->old_nexts = (csklnode_t **) m_alloc(sizeof(csklnode_t *) * max_height);
  return prev_nodes;
}

//******************************************************************************
// private operations
//******************************************************************************

static void
csklitem_put
(
  cskiplist_t* cskl,
  csklnode_t* node,
  void *item,
  bool copy_deep_flag
)
{
  void *node_item = copy_deep_flag && item ? cskl->i_copy(item):(item);
  atomic_store(&node->item, node_item);
}


/*
 * allocate a node of a specified max_height.
 * If item_deep_copy provided make a deep copy, otherwise just copy void *
 */
static csklnode_t *
csklnode_create
(
  cskiplist_t* cskl,
  int key,
  void *item,
  int max_height,
  bool copy_deep_flag
)
{
  csklnode_t* node = (csklnode_t*)cskl->m_alloc(SIZEOF_CSKLNODE_T(max_height));
  node->key = key;
  csklitem_put(cskl, node, item, copy_deep_flag);
  node->height = 0;
  for (int i = 0; i < max_height; ++i) {
    atomic_store(&node->nexts[i], NULL);
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
  if (atomic_load(&node->nexts[lev]) == NULL){
    node->height++;
  }
  atomic_store(&node->nexts[lev], next);

}


static csklnode_t *
cskipnode_next
(
  csklnode_t *node,
  int lev
)
{
  return atomic_load(&node->nexts[lev]);
}


static csklnode_t *
cskipnode_next_alive
(
  csklnode_t *node,
  int lev
)
{
  node = atomic_load(&node->nexts[lev]);
  while (atomic_load(&node->item) != NULL)
    node = atomic_load(&node->nexts[lev]);
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
  if (node1->key == node2->key) {
    void *item1 = atomic_load(&node1->item);
    void *item2 = atomic_load(&node2->item);

    if (item1 == item2)
      return true;

    if (i_cmp != NULL)
      return i_cmp(item1, item2);

  }


  return false;
}


static void
cskipnode_free
(
  csklnode_t *node
)
{
  free(node);
  free(node->item);
}


/*
 * Returns previous nodes per level. Note: if find your key it will be on level[0]
 */
static cskl_prev_nodes_t *
cskip_find_prev_nodes
(
  cskiplist_t* cskl,
  int key
)
{
  cskl_prev_nodes_t *prev_nodes = new_prev_nodes( cskl->m_alloc, cskl->max_height);

  csklnode_t *prev_node = cskl->head_ptr;
  csklnode_t *cur_node;

  // For every level
  for (int lev = cskl->max_height-1; lev >= 0 ; --lev) {

    cur_node = cskipnode_next(prev_node, lev);
    // if cur_node->key is bigger than key, prev_node is what we want to return
    while (cur_node->key <= key){
      prev_node = cskipnode_next(prev_node, lev);
      cur_node = cskipnode_next(prev_node, lev);
    }
    prev_nodes->ptrs[lev] = prev_node;
    prev_nodes->old_nexts[lev] = atomic_load(&prev_node->nexts[lev]);

  }
  return prev_nodes;
}


static void
cskip_insert_dumy_nodes
(
  cskiplist_t *cskl
)
{
  csklnode_t *head = csklnode_create(cskl, INT_MIN, NULL, cskl->max_height, SHELL_COPY);
  csklnode_t *tail = csklnode_create(cskl, INT_MAX, NULL, cskl->max_height, SHELL_COPY);

  for (int i = 0; i < cskl->max_height; ++i) {
    csklnode_set_next(head, i, tail);
    csklnode_set_next(tail, i, NULL);
  }
  cskl->head_ptr = head;
}


static void
cskiplist_buld_tower
(
  cskiplist_t* cskl,
  csklnode_t *new_node,
  int tower_height,
  cskl_prev_nodes_t *prev_nodes
)
{
  csklnode_t *cur_prev;

  for (int lev = 0; lev < tower_height; ++lev) {

    csklnode_set_next(new_node, lev, cskipnode_next(prev_nodes->ptrs[lev], lev));

    while (!CAS(&prev_nodes->ptrs[lev]->nexts[lev],
                &prev_nodes->old_nexts[lev], new_node)){

      // New node is not positioned well
      do {
        cur_prev = cskipnode_next(prev_nodes->ptrs[lev], lev); // move cur_prev to next node

        if (cur_prev->key > new_node->key){ // Insert new_node before cursor
          csklnode_set_next(new_node, lev, cur_prev);
          prev_nodes->old_nexts[lev] = cur_prev;

        }else if(cur_prev->key == new_node->key){ // Insert new_node instead of cursor
          csklitem_put(cskl, cur_prev, new_node->item, DEEP_COPY);
          cskipnode_free(new_node);
          return;

        }else{ // cur_prev->key < new_node->key // Insert new_node after cursor
          prev_nodes->ptrs[lev] = cskipnode_next(prev_nodes->ptrs[lev], lev);
        }
      }while(prev_nodes->ptrs[lev]->nexts[lev] != prev_nodes->old_nexts[lev]);

    }

  }
}


//atomic_compare_exchange_strong
static void
cskiplist_put_specific
(
  cskiplist_t* cskl,
  int key,
  void *item,
  int tower_height,
  bool copy_deep_flag
)
{
  // allocate my node
  csklnode_t *new_node = csklnode_create(cskl, key, item, cskl->max_height, copy_deep_flag);

  cskl_prev_nodes_t *prev_nodes = cskip_find_prev_nodes(cskl, key);

  //Reuse prev_node if has your key
  csklnode_t *prev_node = prev_nodes->ptrs[0];
  if (prev_node->key == key){
    atomic_store(&prev_node->item, item);
  }else{
    //Insert new node
    if (tower_height <= 0) tower_height = rand() % (cskl->max_height - 1)  + 1 ;

    cskiplist_buld_tower(cskl, new_node, tower_height, prev_nodes);
    atomic_add(&cskl->total_length, 1);
  }


  atomic_add(&cskl->length, 1);
  free(prev_nodes->ptrs);
  free(prev_nodes->old_nexts);
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

  atomic_store(&cskl->length, 0);
  atomic_store(&cskl->total_length, 2);
  cskl->m_alloc = m_alloc;
  cskl->i_copy = i_copy;
  cskl->i_cmp = i_cmp;
  cskl->max_height = max_height;

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


  while (node->nexts[0] != NULL){ // only on last node
    node_next = node->nexts[0];
    cskipnode_free(node);
    node = node_next;
  }
//  cskipnode_free(node);
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
  cskiplist_put_specific(cskl, key, item, 0, SHELL_COPY);
}


void *
cskiplist_get
(
  cskiplist_t* cskl,
  int key
)
{
  csklnode_t *prev_node = cskip_find_prev_nodes(cskl, key)->ptrs[0];

  if (prev_node->key == key) {
    return atomic_load(&prev_node->item);
  }
  return NULL;
}


void
cskiplist_delete_node
(
  cskiplist_t* cskl,
  int key
)
{

  csklnode_t *prev_node = cskip_find_prev_nodes(cskl, key)->ptrs[0];
  void *item;

  if (prev_node->key == key) {
    do{
      item = atomic_load(&prev_node->item);
    }while(!CAS(&prev_node->item, &item, NULL));

    free(item);
    atomic_add(&cskl->length, -1);
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

  for (csklnode_t *cur_node=cskl->head_ptr->nexts[0]; cur_node->nexts[0] != NULL; cur_node = atomic_load(&cur_node->nexts[0])) {
    cskiplist_put_specific(new_cskl, cur_node->key, atomic_load(&cur_node->item), cur_node->height, SHELL_COPY);
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
  for (csklnode_t *cur_node=cskl->head_ptr->nexts[0]; cur_node->nexts[0] != NULL; cur_node = atomic_load(&cur_node->nexts[0])) {
    cskiplist_put_specific(new_cskl, cur_node->key, atomic_load(&cur_node->item), cur_node->height, DEEP_COPY);
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
  if (atomic_load(&cskl1->length) != atomic_load(&cskl2->length)){
    return false;
  }

  // Start comparing from first valid node
  csklnode_t *node1=cskl1->head_ptr->nexts[0];
  csklnode_t *node2=cskl2->head_ptr->nexts[0];

  while (node1->nexts[0] != NULL || node2->nexts[0] != NULL) {
    if (csklnode_compare(node1, node2, cskl1->i_cmp) == false){
      return false;
    }
    node1 = cskipnode_next_alive(node1, 0);
    node2 = cskipnode_next_alive(node2, 0);

  }
  return true;
}



void
cskiplist_print(cskiplist_t* cskl)
{
  int full_len = atomic_load(&cskl->total_length);

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

    if (atomic_load(&cur_node->item) == NULL){
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
    for (int i = 0; i < full_len; ++i) {
      printf("\t%c", str_nodes[lev][i]);
    }
    printf("\n");
  }


  printf("%sDel:", KCYN);
  for (int i = 0; i < full_len; ++i) {
    printf("\t%c", str_deleted[i]);
  }
  printf("%s\n", KNRM);


  printf("%sKeys:", KYEL);
  printf("\t-inf");
  for (int i = 1; i < full_len-1; ++i) {
    printf("\t%d", keys[i]);
  }
  printf("\t+inf%s\n", KNRM);




  // Free dynamic allocation
  for (int i = 0; i < cskl->max_height; ++i) {
    free(str_nodes[i]);
  }
  free(str_nodes);
  free(str_deleted);
}
