#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint32_t);

/* We use the `readline' library to provide more flexibility to read from stdin. */
char *rl_gets()
{
	static char *line_read = NULL;

	if (line_read)
	{
		free(line_read);
		line_read = NULL;
	}

	line_read = readline("(nemu) ");

	if (line_read && *line_read)
	{
		add_history(line_read);
	}

	return line_read;
}

static int cmd_c(char *args)
{
	cpu_exec(-1);
	return 0;
}

static int cmd_si(char *args)
{
	int n = 1;

	if (args != NULL)
	{
		sscanf(args, "%d", &n);
	}

	cpu_exec(n);

	return 0;
}

static int cmd_q(char *args)
{
	return -1;
}

static int cmd_info(char *args)
{
	if (args == NULL)
	{
		printf("Usage: info r\n");
		return 0;
	}

	if (strcmp(args, "r") == 0)
	{
		printf("eax\t0x%08x\n", cpu.eax);
		printf("ecx\t0x%08x\n", cpu.ecx);
		printf("edx\t0x%08x\n", cpu.edx);
		printf("ebx\t0x%08x\n", cpu.ebx);
		printf("esp\t0x%08x\n", cpu.esp);
		printf("ebp\t0x%08x\n", cpu.ebp);
		printf("esi\t0x%08x\n", cpu.esi);
		printf("edi\t0x%08x\n", cpu.edi);
		printf("eip\t0x%08x\n", cpu.eip);
	}

	return 0;
}

static int cmd_x(char *args)
{
	int n;
	uint32_t addr;

	if (args == NULL)
	{
		printf("Usage: x N EXPR\n");
		return 0;
	}

	if (sscanf(args, "%d %x", &n, &addr) != 2)
	{
		printf("Usage: x N EXPR\n");
		return 0;
	}

	int i;

	for (i = 0; i < n; i++)
	{
		uint32_t data = swaddr_read(addr + i * 4, 4);

		printf("0x%08x: 0x%08x\n",
			   addr + i * 4,
			   data);
	}

	return 0;
}

static int cmd_p(char *args)
{
	if (args == NULL)
	{
		printf("Please specify an expression to evaluate.\n");
		return 0;
	}

	bool success;
	uint32_t result = expr(args, &success);

	if (success)
	{
		printf("%d\n", (int32_t)result);
	}
	else
	{
		printf("Invalid expression: %s\n", args);
	}

	return 0;
}

static int cmd_w(char *args)
{
	bool success;
	uint32_t value;
	WP *wp;
	if (args == NULL)
	{
		printf("Please specify an expression to watch.\n");
		return 0;
	}
	// 计算表达式的值
	value = expr(args, &success);
	if (!success)
	{
		printf("Invalid expression: %s\n", args);
		return 0;
	}
	// 建新的监视点
	wp = new_wp();
	if (wp == NULL)
	{
		printf("Failed to create a new watchpoint.\n");
		return 0;
	}
	// 保存表达式和值
	strncpy(wp->expr, args, sizeof(wp->expr) - 1);
	wp->expr[sizeof(wp->expr) - 1] = '\0'; // 确保字符串以空字符结尾

	wp->value = value;
	printf("Watchpoint %d: %s\n", wp->NO, wp->expr);

	return 0;
}

static int cmd_d(char *args)
{
	int NO;
	WP *wp;
	if (args == NULL)
	{
		printf("Please specify the watchpoint number to delete.\n");
		return 0;
	}
	if (sscanf(args, "%d", &NO) != 1)
	{
		printf("Invalid watchpoint number: %s\n", args);
		return 0;
	}

	wp = find_wp(NO);
	if (wp == NULL)
	{
		printf("Watchpoint %d not found.\n", NO);
		return 0;
	}

	free_wp(wp);
	printf("Watchpoint %d deleted.\n", NO);
	return 0;
}

static int cmd_help(char *args);

static struct
{
	char *name;
	char *description;
	int (*handler)(char *);
} cmd_table[] = {
	{"help", "Display informations about all supported commands", cmd_help},
	{"c", "Continue the execution of the program", cmd_c},
	{"q", "Exit NEMU", cmd_q},

	/* TODO: Add more commands */
	{"si", "Step instruction", cmd_si},
	{"info", "Print register information", cmd_info},
	{"x", "Examine memory", cmd_x},
	{"p", "Evaluate expression", cmd_p},
	{"w", "Set watchpoint", cmd_w},
	{"d", "Delete watchpoint", cmd_d},

};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_help(char *args)
{
	/* extract the first argument */
	char *arg = strtok(NULL, " ");
	int i;

	if (arg == NULL)
	{
		/* no argument given */
		for (i = 0; i < NR_CMD; i++)
		{
			printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
		}
	}
	else
	{
		for (i = 0; i < NR_CMD; i++)
		{
			if (strcmp(arg, cmd_table[i].name) == 0)
			{
				printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
				return 0;
			}
		}
		printf("Unknown command '%s'\n", arg);
	}
	return 0;
}

void ui_mainloop()
{
	while (1)
	{
		char *str = rl_gets();
		char *str_end = str + strlen(str);

		/* extract the first token as the command */
		char *cmd = strtok(str, " ");
		if (cmd == NULL)
		{
			continue;
		}

		/* treat the remaining string as the arguments,
		 * which may need further parsing
		 */
		char *args = cmd + strlen(cmd) + 1;
		if (args >= str_end)
		{
			args = NULL;
		}

#ifdef HAS_DEVICE
		extern void sdl_clear_event_queue(void);
		sdl_clear_event_queue();
#endif

		int i;
		for (i = 0; i < NR_CMD; i++)
		{
			if (strcmp(cmd, cmd_table[i].name) == 0)
			{
				if (cmd_table[i].handler(args) < 0)
				{
					return;
				}
				break;
			}
		}

		if (i == NR_CMD)
		{
			printf("Unknown command '%s'\n", cmd);
		}
	}
}
