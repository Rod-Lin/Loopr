#include <string.h>
#include "SandBox_pri.h"
#include "MEM.h"
#include "DBG.h"

ByteInfo Loopr_Byte_Info[] = {
	{"dummy",	0,	0},

	{"ldb",		0,	1},
	{"ldnull",	0,	1},
	{"ldstr",	0,	1},
	{"ldloc",	0,	1},

	{"initloc",	0,	0},
	{"conv",	1,	0},
	{"bx",		1,	0},
	{"unbx",	1,	0},

	{"popb",	1,	-1},
	{"popf",	1,	-1},
	{"popstr",	1,	-1},

	{"stloc",	1,	-1},

	{"br",		1,	-1},
	{"dup",		1,	1},

	{"addb",	2,	-1},
	{"addf",	2,	-1},
	{"addstr",	2,	-1},
	{"subb",	2,	-1},
	{"mulb",	2,	-1},
	{"divb",	2,	-1},
	{"inc",		1,	0},
	{"dec",		1,	0},

	{"call",	0,	1},
	{"ldarg",	0,	1},

	{"goto",	0,	0},
	{"ret",		0,	0},
	{"nop",		0,	0}
};

Loopr_Byte *
Coding_alloc_byte(int length)
{
	Loopr_Byte *ret = MEM_malloc(sizeof(Loopr_Byte) * length);

	return ret;
}

ByteContainer *
Coding_init_coding_env(void)
{
	ByteContainer *env;

	env = MEM_malloc(sizeof(ByteContainer));
	env->name = NULL;
	env->next = 0;
	env->alloc_size = 0;
	env->hinted = LPR_False;
	env->stack_size = 0;
	env->entrance = 0;
	env->code = NULL;

	env->local_variable_count = 0;
	env->local_variable = NULL;

	env->function_count = 0;
	env->function = NULL;

	env->outer_env = NULL;

	return env;
}

int
Coding_init_local_variable(ByteContainer *env, char *identifier)
{
	int i;
	for (i = 0; i < env->local_variable_count; i++) {
		if (env->local_variable[i].identifier
			&& !strcmp(env->local_variable[i].identifier,
					   identifier)) {
			DBG_panic(("Duplicated variable name \"%s\"\n", identifier));
			return -1;
		}
	}

	env->local_variable = MEM_realloc(env->local_variable,
									  sizeof(LocalVariable) * (env->local_variable_count + 1));
	env->local_variable[env->local_variable_count].value = NULL;
	env->local_variable[env->local_variable_count].identifier = identifier ? MEM_strdup(identifier) : NULL;
	env->local_variable_count++;

	return i;
}

int
Coding_get_local_variable_index(ByteContainer *env, char *name)
{
	int i;
	for (i = 0; i < env->local_variable_count; i++) {
		if (env->local_variable[i].identifier
			&& !strcmp(env->local_variable[i].identifier,
					   name)) {
			return i;
		}
	}
	DBG_panic(("Cannot find local variable \"%s\"\n", name));

	return -1;
}

void
Coding_byte_cat(ByteContainer *env, Loopr_Byte *src, int count)
{
	env->alloc_size += count;

	env->code = MEM_realloc(env->code,
							sizeof(Loopr_Byte) * env->alloc_size);

	memcpy(&env->code[env->next], src, count);
	env->next += count;

	return;
}

#define check_is_code(c) ((c) > 0 \
						  && (c) < LPR_CODE_PLUS_1 \
						  && (c) != LPR_NULL_CODE ? (c) : LPR_False)
#define check_is_negative(num) ((num) < 0 ? 0 : (num))

void
Coding_push_code(ByteContainer *env, Loopr_Byte code, Loopr_Byte *args, int args_count)
{
	if (check_is_code(code)) {
		Coding_byte_cat(env, &code, 1);
		if (!env->hinted) {
			env->stack_size += check_is_negative(Loopr_Byte_Info[code].stack_regulator);
		}
	}
	Coding_byte_cat(env, args, args_count);

	return;
}

ExeEnvironment *
Coding_init_exe_env(ByteContainer *env, WarningFlag wflag)
{
	int i;
	ExeEnvironment *ret;
	Loopr_Value **stack_value;

	ret = MEM_malloc(sizeof(ExeEnvironment));
	ret->wflag = wflag;
	ret->entrance = env->entrance;
	ret->code_length = env->alloc_size;
	ret->code = env->code;
	if (env->stack_size < 0) {
		DBG_panic(("negative stack size\n"));
	}
	stack_value = MEM_malloc(sizeof(Loopr_Value *) * (env->stack_size + 1));

	ret->stack.alloc_size = env->stack_size + 1;
	ret->stack.stack_pointer = -1;
	ret->stack.value = stack_value;
	ret->local_variable_count = 0;
	ret->local_variable = NULL;

	ret->outer_env = NULL;

	ret->function_count = env->function_count;
	ret->function = NULL;
	if (env->function_count > 0) {
		ret->function = MEM_malloc(sizeof(ExeEnvironment *) * ret->function_count);

		for (i = 0; i < env->function_count; i++) {
			ret->function[i] = Coding_init_exe_env(env->function[i], wflag);
			ret->function[i]->outer_env = ret;
		}
	}

	return ret;
}
