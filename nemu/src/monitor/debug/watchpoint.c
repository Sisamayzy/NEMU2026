#include "monitor/watchpoint.h"
#include "monitor/expr.h"
#include "nemu.h"
#define NR_WP 32

static WP wp_pool[NR_WP];
static WP *head, *free_;

void init_wp_pool()
{
	int i;
	for (i = 0; i < NR_WP; i++)
	{
		wp_pool[i].NO = i;
		wp_pool[i].next = &wp_pool[i + 1];
	}
	wp_pool[NR_WP - 1].next = NULL;

	head = NULL;
	free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */
WP *new_wp()
{
	WP *wp;
	if (free_ == NULL)
	{
		printf("No free watchpoint\n");
		return NULL;
	}
	wp = free_;
	free_ = free_->next;
	wp->next = head;
	head = wp;
	return wp;
}

WP *find_wp(int NO)
{
	WP *p = head;
	while (p != NULL)
	{
		if (p->NO == NO)
		{
			return p;
		}
		p = p->next;
	}
	return NULL;
}

void free_wp(WP *wp)
{
	WP *p;
	if (wp == NULL)
	{
		printf("Invalid watchpoint\n");
		return;
	}
	if (head == wp)
	{
		head = head->next;
	}
	else
	{
		p = head;
		while (p != NULL && p->next != wp)
		{
			p = p->next;
		}
		if (p == NULL)
		{
			printf("Watchpoint not found\n");
			return;
		}
		p->next = wp->next;
	}
	wp->next = free_;
	free_ = wp;
}

bool check_watchpoints()
{
	WP *p = head;
	bool success;
	uint32_t new_value;
	while (p != NULL)
	{
		new_value = expr(p->expr, &success);
		if (success && new_value != p->value)
		{
			printf("Hint watchpoint %d at address 0x%08x\n", p->NO, cpu.eip);
			printf("Old value = %u\n", p->value);
			printf("New value = %u\n", new_value);
			p->value = new_value;
			return true;
		}
		p = p->next;
	}
	return false;
}

void print_watchpoints()
{
	WP *p = head;
	if (p == NULL)
	{
		printf("No watchpoints\n");
		return;
	}
	printf("Num\tExpression\tValue\n");
	while (p != NULL)
	{
		printf("%d\t%s\t%u\n", p->NO, p->expr, p->value);
		p = p->next;
	}
}
