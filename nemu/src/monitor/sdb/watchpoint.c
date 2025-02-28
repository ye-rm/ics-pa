/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include "sdb.h"

#define NR_WP 32
typedef struct watchpoint {
  int NO;
  struct watchpoint *next;

  uint32_t addr;
  /* TODO: Add more members if necessary */

} WP;

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}

WP* new_wp(){
  if(free_!=NULL){
    WP* new = free_;
    free_=free_->next;
    new->next = head;
    head =new;
  }
  Assert(0,"break points num exceeded %d",NR_WP);
  return NULL;
}

void free_wp(WP *wp){
  // tmp_head for delete node
  WP tmp_head;
  tmp_head.next=head;
  WP* cur = &tmp_head;
  bool exist = false;
  while (cur->next!=NULL){
    if(cur->next==wp){
      exist = true;
      break;
    }
    cur=cur->next;
  }
  // move wp from head to free
  if (exist){
   WP* to_free = cur -> next;
   cur->next = to_free->next;
   to_free->next=free_;
   free_=to_free; 
  }
}

//todo: add more info
void show_watchpoint(){
  WP* cur = head;
  while (cur!=NULL)
  {
    printf("bp no. %d at \n",cur->NO);
  }
  if(head==NULL)
    printf("No watch point set\n"); 
}
