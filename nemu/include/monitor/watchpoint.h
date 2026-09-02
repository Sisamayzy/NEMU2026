#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"

typedef struct watchpoint
{
	int NO; // 监视点序号
	struct watchpoint *next;
	/* TODO: Add more members if necessary */
	char expr[32];	// 监视点表达式
	uint32_t value; // 表达式上一次的值
} WP;

bool check_watchpoints();
void init_wp_pool();
WP *new_wp();
WP *find_wp(int no);
void free_wp(WP *wp);
void print_watchpoints();

#endif
