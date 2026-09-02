#include "nemu.h"
#include <stdlib.h>
#include "memory/memory.h"
/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>

enum
{
	NOTYPE = 256,
	EQ,
	TK_NUM,
	TK_HEX,
	TK_REG,
	TK_NEQ,
	TK_AND,
	TK_NEG,
	TK_OR,
	TK_NOT,
	TK_DEREF
	/* TODO: Add more token types */
};

static struct rule
{
	char *regex;
	int token_type;
} rules[] = {

	/* TODO: Add more rules.
	 * Pay attention to the precedence level of different rules.
	 */

	{" +", NOTYPE}, // 空格

	{"0[xX][0-9a-fA-F]+", TK_HEX}, // 十六进制
	{"[0-9]+", TK_NUM},			   // 十进制
	{"\\$[a-zA-Z]+", TK_REG},	   // 寄存器

	{"==", EQ},		// ==
	{"!=", TK_NEQ}, // !=
	{"&&", TK_AND}, // &&

	{"\\+", '+'},	   // +
	{"-", '-'},		   // -
	{"\\*", '*'},	   // *
	{"/", '/'},		   // /
	{"\\|\\|", TK_OR}, // ||
	{"!", TK_NOT},	   // !

	{"\\(", '('}, // (
	{"\\)", ')'}  // )
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]))

static regex_t re[NR_REGEX];

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex()
{
	int i;
	char error_msg[128];
	int ret;

	for (i = 0; i < NR_REGEX; i++)
	{
		ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
		if (ret != 0)
		{
			regerror(ret, &re[i], error_msg, 128);
			Assert(ret == 0, "regex compilation failed: %s\n%s", error_msg, rules[i].regex);
		}
	}
}

typedef struct token
{
	int type;
	char str[32];
} Token;

Token tokens[32];
int nr_token;

static void identify_unary_minus()
{
	int i;
	for (i = 0; i < nr_token; i++)
	{

		// 负号和减号的区分
		if (tokens[i].type == '-')
		{
			if (i == 0 ||
				tokens[i - 1].type == '(' ||
				tokens[i - 1].type == '+' ||
				tokens[i - 1].type == '-' ||
				tokens[i - 1].type == '*' ||
				tokens[i - 1].type == '/' ||
				tokens[i - 1].type == EQ ||
				tokens[i - 1].type == TK_NEQ ||
				tokens[i - 1].type == TK_AND)
			{
				tokens[i].type = TK_NEG;
			}
		}

		// 指针解引用与乘法的区分
		else if (tokens[i].type == '*')
		{
			if (i == 0 ||
				tokens[i - 1].type == '(' ||
				tokens[i - 1].type == '+' ||
				tokens[i - 1].type == '-' ||
				tokens[i - 1].type == '*' ||
				tokens[i - 1].type == '/' ||
				tokens[i - 1].type == EQ ||
				tokens[i - 1].type == TK_NEQ ||
				tokens[i - 1].type == TK_AND)
			{
				tokens[i].type = TK_DEREF;
			}
		}

		// 逻辑非
		else if (tokens[i].type == '!')
		{
			if (i == 0 ||
				tokens[i - 1].type == '(' ||
				tokens[i - 1].type == '+' ||
				tokens[i - 1].type == '-' ||
				tokens[i - 1].type == '*' ||
				tokens[i - 1].type == '/' ||
				tokens[i - 1].type == EQ ||
				tokens[i - 1].type == TK_NEQ ||
				tokens[i - 1].type == TK_AND)
			{
				tokens[i].type = TK_NOT;
			}
		}
	}
}
static bool make_token(char *e)
{
	int position = 0;
	int i;
	regmatch_t pmatch;

	nr_token = 0;

	while (e[position] != '\0')
	{
		/* Try all rules one by one. */
		for (i = 0; i < NR_REGEX; i++)
		{
			if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0)
			{
				char *substr_start = e + position;
				int substr_len = pmatch.rm_eo;

				Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s", i, rules[i].regex, position, substr_len, substr_len, substr_start);
				position += substr_len;

				/* TODO: Now a new token is recognized with rules[i]. Add codes
				 * to record the token in the array `tokens'. For certain types
				 * of tokens, some extra actions should be performed.
				 */

				switch (rules[i].token_type)
				{
				case NOTYPE:
					break;
				default:
					if (nr_token >= 32)
					{
						printf("too many tokens\n");
						return false;
					}

					if (substr_len >= sizeof(tokens[nr_token].str))
					{
						printf("token too long\n");
						return false;
					}

					tokens[nr_token].type = rules[i].token_type;
					strncpy(tokens[nr_token].str, substr_start, substr_len);
					tokens[nr_token].str[substr_len] = '\0';
					nr_token++;
					break;
				}

				break;
			}
		}

		if (i == NR_REGEX)
		{
			printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
			return false;
		}
	}

	return true;
}

static bool check_parentheses(int p, int q)
{
	int i;
	int balance = 0;
	if (tokens[p].type != '(' || tokens[q].type != ')')
	{
		return false;
	}
	for (i = p; i <= q; i++)
	{
		if (tokens[i].type == '(')
		{
			balance++;
		}
		else if (tokens[i].type == ')')
		{
			balance--;
			if (balance == 0 && i < q)
			{
				return false;
			}
		}
		if (balance < 0)
		{
			return false;
		}
	}
	return balance == 0;
}

// 确定优先级
static int get_priority(int type)
{
	switch (type)
	{
	case TK_OR:
		return 1;

	case TK_AND:
		return 2;

	case EQ:
	case TK_NEQ:
		return 3;

	case '+':
	case '-':
		return 4;

	case '*':
	case '/':
		return 5;

	case TK_NEG:
	case TK_DEREF:
	case TK_NOT:
		return 6;

	default:
		return 100;
	}
}

static int dominant_operator(int p, int q)
{
	int i;
	int op = -1;
	int min_priority = 100;
	int balance = 0;
	int priority;
	for (i = p; i <= q; i++)
	{
		if (tokens[i].type == '(')
		{
			balance++;
			continue;
		}
		if (tokens[i].type == ')')
		{
			balance--;
			if (balance < 0)
			{
				return -1;
			}
			continue;
		}
		if (balance != 0)
		{
			continue;
		}
		priority = get_priority(tokens[i].type);
		if (priority < min_priority && priority < 100)
		{
			min_priority = priority;
			op = i;
		}
	}
	return op;
}

static uint32_t eval(int p, int q)
{
	int op;
	uint32_t val1, val2;
	if (p > q)
	{
		panic("Bad expression");
	}
	else if (p == q)
	{
		if (tokens[p].type == TK_NUM)
		{
			return atoi(tokens[p].str);
		}
		else if (tokens[p].type == TK_HEX)
		{
			return strtoul(tokens[p].str, NULL, 0);
		}
		else if (tokens[p].type == TK_REG)
		{
			if (strcmp(tokens[p].str, "$eax") == 0)
				return cpu.eax;
			else if (strcmp(tokens[p].str, "$ecx") == 0)
				return cpu.ecx;
			else if (strcmp(tokens[p].str, "$edx") == 0)
				return cpu.edx;
			else if (strcmp(tokens[p].str, "$ebx") == 0)
				return cpu.ebx;
			else if (strcmp(tokens[p].str, "$esp") == 0)
				return cpu.esp;
			else if (strcmp(tokens[p].str, "$ebp") == 0)
				return cpu.ebp;
			else if (strcmp(tokens[p].str, "$esi") == 0)
				return cpu.esi;
			else if (strcmp(tokens[p].str, "$edi") == 0)
				return cpu.edi;
			else if (strcmp(tokens[p].str, "$eip") == 0)
				return cpu.eip;
			else
			{
				printf("Unknown register: %s\n", tokens[p].str);
				return 0;
			}
		}
		else
		{
			panic("Invalid single token");
		}
	}
	else if (check_parentheses(p, q))
	{
		return eval(p + 1, q - 1);
	}
	else
	{
		op = dominant_operator(p, q);
		if (op == -1)
		{
			panic("No dominant operator found , bad expression");
		}
		if (tokens[op].type == TK_NEG)
		{
			return -eval(op + 1, q);
		}
		if (tokens[op].type == TK_DEREF)
		{
			uint32_t addr = eval(op + 1, q);
			return swaddr_read(addr, 4);
		}
		if (tokens[op].type == TK_NOT)
		{
			return !eval(op + 1, q);
		}
		val1 = eval(p, op - 1);
		val2 = eval(op + 1, q);
		switch (tokens[op].type)
		{
		case '+':
			return val1 + val2;
		case '-':
			return val1 - val2;
		case '*':
			return val1 * val2;
		case '/':
			return val1 / val2;
		case EQ:
			return val1 == val2;
		case TK_NEQ:
			return val1 != val2;
		case TK_AND:
			return val1 && val2;
		case TK_OR:
			return val1 || val2;
		default:
			panic("Unsupported operator");
		}
	}
}

uint32_t expr(char *e, bool *success)
{
	if (!make_token(e))
	{
		*success = false;
		return 0;
	}

	identify_unary_minus();

	*success = true;
	return eval(0, nr_token - 1);
}
