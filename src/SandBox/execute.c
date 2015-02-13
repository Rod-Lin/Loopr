#include <stdio.h>
#include <string.h>
#include "DBG.h"
#include "UTL.h"
#include "MEM.h"
#include "SandBox_pri.h"

#define ST(stack, offset) \
	((stack).value[(stack).stack_pointer + (offset)])
#define ST_i(stack, p) \
	((stack).value[p])

#define ST_TYPE(stack, offset) \
	((stack).value[(stack).stack_pointer + (offset)]->type)

#define ST_BYTE(stack, offset) \
	((stack).value[(stack).stack_pointer + (offset)]->u.byte_value)
#define ST_SBYTE(stack, offset) \
	((stack).value[(stack).stack_pointer + (offset)]->u.sbyte_value)
#define ST_INT16(stack, offset) \
	((stack).value[(stack).stack_pointer + (offset)]->u.int16_value)
#define ST_UINT16(stack, offset) \
	((stack).value[(stack).stack_pointer + (offset)]->u.uint16_value)
#define ST_INT32(stack, offset) \
	((stack).value[(stack).stack_pointer + (offset)]->u.int32_value)
#define ST_UINT32(stack, offset) \
	((stack).value[(stack).stack_pointer + (offset)]->u.uint32_value)
#define ST_STRING(stack, offset) \
	((stack).value[(stack).stack_pointer + (offset)]->u.string_value)
#define ST_INT64(stack, offset) \
	((stack).value[(stack).stack_pointer + (offset)]->u.int64_value)
#define ST_UINT64(stack, offset) \
	((stack).value[(stack).stack_pointer + (offset)]->u.uint64_value)
#define ST_SINGLE(stack, offset) \
	((stack).value[(stack).stack_pointer + (offset)]->u.single_value)
#define ST_DOUBLE(stack, offset) \
	((stack).value[(stack).stack_pointer + (offset)]->u.double_value)
#define ST_FLOAT(stack, offset) \
	(ST_TYPE((stack), (offset)) == LPR_SINGLE \
	? (stack).value[(stack).stack_pointer + (offset)]->u.single_value \
	: (stack).value[(stack).stack_pointer + (offset)]->u.double_value)
#define ST_STRING(stack, offset) \
	((stack).value[(stack).stack_pointer + (offset)]->u.string_value)
#define ST_OBJECT(stack, offset) \
	((stack).value[(stack).stack_pointer + (offset)]->u.object_value)

#define ST_WRITE(stack, offset, r) \
	((stack).value[(stack).stack_pointer + (offset)] = r)
#define ST_WRITE_BYTE(stack, offset, r) \
	((stack).value[(stack).stack_pointer + (offset)]->u.byte_value = r)
#define ST_WRITE_SBYTE(stack, offset, r) \
	((stack).value[(stack).stack_pointer + (offset)]->u.sbyte_value = r)
#define ST_WRITE_INT16(stack, offset, r) \
	((stack).value[(stack).stack_pointer + (offset)]->u.int16_value = r)
#define ST_WRITE_UINT16(stack, offset, r) \
	((stack).value[(stack).stack_pointer + (offset)]->u.uint16_value = r)
#define ST_WRITE_INT32(stack, offset, r) \
	((stack).value[(stack).stack_pointer + (offset)]->u.int32_value = r)
#define ST_WRITE_UINT32(stack, offset, r) \
	((stack).value[(stack).stack_pointer + (offset)]->u.uint32_value = r)
#define ST_WRITE_INT64(stack, offset, r) \
	((stack).value[(stack).stack_pointer + (offset)]->u.int64_value = r)
#define ST_WRITE_UINT64(stack, offset, r) \
	((stack).value[(stack).stack_pointer + (offset)]->u.uint64_value = r)
#define ST_WRITE_SINGLE(stack, offset, r) \
	((stack).value[(stack).stack_pointer + (offset)]->u.single_value = r)
#define ST_WRITE_DOUBLE(stack, offset, r) \
	((stack).value[(stack).stack_pointer + (offset)]->u.double_value = r)

#define TYPE_CMP(t1, t2) \
	((t1) >= (t2) ? (t1) : (t2))
#define GET_BIT(num, type) \
	((num) << (Loopr_Type_Info[LPR_INT64].size - Loopr_Type_Info[type].size) * 8 \
		   >> (Loopr_Type_Info[LPR_INT64].size - Loopr_Type_Info[type].size) * 8)

#define get_visual_string(src) ((src) ? (src)->u.string_value : NULL_VISUAL)
#define is_unsigned(type) \
	(type % 2 != 0 ? LPR_True : LPR_False)

static Loopr_Char *
chain_string(Loopr_Value *str1, Loopr_Value *str2)
{
	int len;
	Loopr_Char *ret;
	Loopr_Char *right;
	Loopr_Char *left;

	right = get_visual_string(str1);
	left = get_visual_string(str2);

	len = Loopr_wcslen(right) + Loopr_wcslen(left);
	ret = MEM_malloc(sizeof(Loopr_Char) * (len + 1));

	Loopr_wcscpy(ret, right);
	Loopr_wcscat(ret, left);

	if (str1 && str1->u.string_value) {
		MEM_free(str1->u.string_value);
	}

	return ret;
}

static Loopr_Value *ret_value;

static Loopr_Value *
Private_do_push_byte(Loopr_BasicType basic_type, Loopr_Byte *code, int *offset)
{
	ret_value = Loopr_alloc_value(basic_type);
	Loopr_byte_deserialize(&ret_value->u.int64_value,
						   code, Loopr_Type_Info[basic_type].size);
	*offset = Loopr_Type_Info[basic_type].size;
	return ret_value;
}

static void
Private_init_local_variable(ExeEnvironment *env, Loopr_BasicType type, int index)
{
#ifdef SENSLTIVE
	if (index < 0 && env->wflag != LPR_NOTHING) {
		DBG_panic(("init: Cannot find local variable\n"));
		return;
	} else if (index >= env->local_variable_count) {
		/*env->local_variable = MEM_realloc(env->local_variable,
									  	  sizeof(LocalVariable) * (env->local_variable_count + 1));
		env->local_variable[index].identifier = NULL;
		env->local_variable_count++;*/
		DBG_panic(("init: Local variable overflow\n"));
		return;
	}
#endif

	env->local_variable[index].value = Loopr_get_init_value(type);
	return;
}

static Loopr_Value *
Private_copy_variable(Loopr_Value *src)
{
	ret_value = NULL;
	if (src) {
		ret_value = Loopr_alloc_value(src->type);
		ret_value->u = src->u;
		if (src->type == LPR_STRING) {
			if (src->u.string_value) {
				ret_value->u.string_value = MEM_malloc(sizeof(Loopr_Char) * (Loopr_wcslen(src->u.string_value) + 1));
				Loopr_wcscpy(ret_value->u.string_value, src->u.string_value);
			}
		}
	}

	return ret_value;
}

static void
Private_assign_local_variable(ExeEnvironment *env, int index, Loopr_Value *value)
{
#ifdef SENSLTIVE
	if ((index < 0 || index >= env->local_variable_count)
		&& env->wflag != LPR_NOTHING) {
		DBG_panic(("stloc: Cannot find local variable\n"));
		return;
	}
#endif

	env->local_variable[index].identifier = NULL;
	env->local_variable[index].value = value;

	return;
}

static Loopr_Value *
Private_load_local_variable(ExeEnvironment *env, int index)
{
#ifdef SENSLTIVE
	if ((index < 0 || index >= env->local_variable_count)
		&& env->wflag != LPR_NOTHING) {
		DBG_panic(("ldloc: Cannot find local variable\n"));	
		return NULL;
	}
#endif

	return Private_copy_variable(env->local_variable[index].value);
}

static void
Private_expand_stack(Loopr_Stack *stack, int add)
{
	int rest;

	rest = stack->alloc_size - stack->stack_pointer;
	if (rest < add) {
		stack->value = MEM_realloc(stack->value, sizeof(Loopr_Value *) * (stack->alloc_size + add - rest));
		stack->alloc_size += add - rest;
	}

	return;
}

static void
Private_mark_value(Loopr_Value *obj)
{
	if (!obj) {
		return;
	}

	obj->marked = Walle_get_alive_period();
	if (obj->type == LPR_OBJECT) {
		Private_mark_value(obj->u.object_value);
	}

	return;
}

static ExeEnvironment *current_top_level = NULL;
#define Private_set_top_level(env) (current_top_level = env)
#define Private_get_top_level() (current_top_level)

static void
Private_walle_marker(ExeEnvironment *env)
{
	int i;

	for (i = 0; i < env->local_variable_count; i++) {
		Private_mark_value(env->local_variable[i].value);
	}

	for (i = 0; i <= env->stack.stack_pointer; i++) {
		Private_mark_value(env->stack.value[i]);
	}

	return;
}

static ExeEnvironment *callee;
static ExeEnvironment *outer;
static int pri_i, pri_j;
#include <time.h>

static void
Private_invoke_function(ExeEnvironment *env, int index, int argc,
						int *pc_p, int *base_p, int *local_p)
{
	CallInfo *call_info;
	ExeEnvironment *callee;

	call_info = malloc(sizeof(CallInfo));
	outer = Private_get_top_level();
	callee = outer->function[index];

	/* record info */
	call_info->pc = *pc_p + 1 + (sizeof(Loopr_Int32) * 2);
	call_info->caller = env->exe;

	Private_expand_stack(&env->stack, callee->stack.alloc_size);
	call_info->base = *base_p;
	call_info->local = *local_p;

	/* reset stack */
	env->stack.stack_pointer = env->stack.stack_pointer - argc + 1;
	*base_p = env->stack.stack_pointer;
	*local_p = env->local_variable_count;

	/* invoke */
	if (callee->local_variable_count > 0) {
		env->local_variable_count += callee->local_variable_count;
		env->local_variable = MEM_realloc(env->local_variable,
										  sizeof(LocalVariable) * env->local_variable_count);
	}
	for (pri_i = 0; pri_i < argc; pri_i++) {
		Private_assign_local_variable(env, *local_p + pri_i, ST_i(env->stack, *base_p + pri_i));
	}

	env->stack.value[*base_p] = (Loopr_Value *)call_info;

	env->exe = callee->exe;
	*pc_p = callee->exe->entrance;
	return;
}

static Loopr_Value *
Private_do_return(ExeEnvironment *env, int *pc_p, int *base_p, int *local_p)
{
	CallInfo *call_info;

	call_info = (CallInfo *)env->stack.value[*base_p];
	ret_value = env->stack.value[env->stack.stack_pointer];

	env->stack.stack_pointer = *base_p - 1;

	env->exe = call_info->caller;
	*pc_p = call_info->pc;
	*base_p = call_info->base;
	*local_p = call_info->local;
	env->local_variable_count = *local_p;
	free(call_info);

	return ret_value;
}


#define LOWER_LEVEL ("|--\t")

Loopr_Value *
Loopr_execute(ExeEnvironment *env, Loopr_Boolean top_level)
{
	int arg1;
	int arg2;
	int base = 0;
	int local_base = 0;
	int pc = env->exe->entrance;

	if (top_level) {
		env->local_variable = MEM_malloc(sizeof(LocalVariable) * env->local_variable_count);
		for (pri_i = 0; pri_i < env->local_variable_count; pri_i++) {
			env->local_variable[pri_i].value = Loopr_create_null();
		}
		env->stack.value = MEM_malloc(sizeof(Loopr_Value *) * (env->stack.alloc_size + 1));
		Private_set_top_level(env);
	}
	for (; pc < env->exe->length;) {
		/*printf("%s%4d:\t%-15ssp(%d)\n", (!top_level ? LOWER_LEVEL : ""), pc,
			   Loopr_Byte_Info[env->exe->code[pc]].assembly_name,
			   env->stack.stack_pointer);*/
#ifdef SENSLTIVE
		if (env->stack.stack_pointer < (Loopr_Byte_Info[env->exe->code[pc]].need_stack - 1)
			&& env->wflag != LPR_NOTHING) {
			DBG_panic(("Too few stack for code \"%s\"\n", Loopr_Byte_Info[env->exe->code[pc]].assembly_name));
		}

		if (env->stack.stack_pointer >= env->stack.alloc_size - 2) {
			if (env->wflag > LPR_JUST_PANIC) {
				DBG_panic(("Stack overflow\n"));
			} else {
				Private_expand_stack(&env->stack, 1);
			}
		}
#endif

		switch (env->exe->code[pc]) {
			case LPR_LD_BYTE: {
				ST(env->stack, 1) = Private_do_push_byte(env->exe->code[pc + 1], &env->exe->code[pc + 2], &arg1);
				env->stack.stack_pointer++;
				pc += 2 + arg1;
				break;
			}
			case LPR_LD_NULL: {
				ST(env->stack, 1) = Loopr_create_null();
				env->stack.stack_pointer++;
				pc++;
				break;	
			}
			case LPR_LD_STRING: {
				ST(env->stack, 1) = Loopr_create_string(&(env->exe->code[pc + 1]), &arg1);
				env->stack.stack_pointer++;
				pc += 1 + arg1;
				break;				
			}
			case LPR_LD_LOC: {
				Loopr_byte_deserialize(&arg1,
						  			   &env->exe->code[pc + 1], sizeof(Loopr_Int32));
				ST(env->stack, 1) = Private_load_local_variable(env, arg1 + local_base);
				env->stack.stack_pointer++;
				pc += 1 + sizeof(Loopr_Int32);
				break;
			}
			case LPR_BOXING: {
				ST(env->stack, 0) = Loopr_create_object(ST(env->stack, 0));
				pc++;
				break;
			}
			case LPR_UNBOXING: {
				ST(env->stack, 0) = (ST(env->stack, 0) ? ST_OBJECT(env->stack, 0) : NULL);
				pc++;
				break;
			}
			case LPR_POP: {
				env->stack.stack_pointer--;
				pc++;
				break;
			}
			case LPR_POP_BYTE: {
				Loopr_BasicType type = ST_TYPE(env->stack, 0);
				if (is_unsigned(type)) {
					printf("%s#pop_%s: %lu\n", (!top_level ? LOWER_LEVEL : ""), Loopr_Type_Info[type].assembly_name
										   , GET_BIT(ST_UINT64(env->stack, 0), type));
				} else {
					printf("%s#pop_%s: %ld\n", (!top_level ? LOWER_LEVEL : ""), Loopr_Type_Info[type].assembly_name
										   , GET_BIT(ST_INT64(env->stack, 0), type));
				}
				env->stack.stack_pointer--;
				pc++;
				break;
			}
			case LPR_POP_FLOAT: {
				if (ST_TYPE(env->stack, 0) == LPR_SINGLE) {
					printf("%s#pop_single: %f\n", (!top_level ? LOWER_LEVEL : ""), ST_SINGLE(env->stack, 0));
				} else {
					printf("%s#pop_double: %lf\n", (!top_level ? LOWER_LEVEL : ""), ST_DOUBLE(env->stack, 0));
				}
				env->stack.stack_pointer--;
				pc++;
				break;
			}
			case LPR_POP_STRING: {
				printf("%s#pop_string: %ls\n", (!top_level ? LOWER_LEVEL : ""), get_visual_string(ST(env->stack, 0)));
				env->stack.stack_pointer--;
				pc++;
				break;
			}
			case LPR_STORE_LOC: {
				Loopr_byte_deserialize(&arg1,
						  			   &env->exe->code[pc + 1], sizeof(Loopr_Int32));
				Private_assign_local_variable(env, arg1 + local_base, ST(env->stack, 0));
				env->stack.stack_pointer--;
				pc += 1 + sizeof(Loopr_Int32);
				break;
			}
			case LPR_BRANCH: {
				if (env->exe->code[pc + 1] == (ST_INT64(env->stack, 0) == env->exe->code[pc + 2])) {
					Loopr_byte_deserialize(&pc, &env->exe->code[pc + 3], sizeof(Loopr_Int32));
				} else {
					pc += 3 + sizeof(Loopr_Int32);
				}
				env->stack.stack_pointer--;
				break;
			}
			case LPR_LOAD_ARG: /* fallthrough */
			case LPR_DUPLICATE: {
				ST(env->stack, 1) = Private_copy_variable(ST_i(env->stack, base - env->exe->code[pc + 1]));
				env->stack.stack_pointer++;
				pc += 2;
				break;
			}
			case LPR_ADD_BYTE: {
				ST_WRITE_INT64(env->stack, -1, ST_INT64(env->stack, -1) + ST_INT64(env->stack, 0));
				ST_TYPE(env->stack, -1) = TYPE_CMP(ST_TYPE(env->stack, -1),
												   ST_TYPE(env->stack, 0));
				env->stack.stack_pointer--;
				pc++;
				break;
			}
			case LPR_ADD_FLOAT: {
				ST_WRITE_DOUBLE(env->stack, -1, ST_DOUBLE(env->stack, -1) + ST_DOUBLE(env->stack, 0));
				ST_TYPE(env->stack, -1) = TYPE_CMP(ST_TYPE(env->stack, -1),
												   ST_TYPE(env->stack, 0));
				env->stack.stack_pointer--;
				pc++;
				break;
			}
			case LPR_ADD_STRING: {
				ST_STRING(env->stack, -1) = chain_string(ST(env->stack, -1), ST(env->stack, 0));
				env->stack.stack_pointer--;
				pc++;
				break;
			}
			case LPR_SUB_BYTE: {
				ST_WRITE_INT64(env->stack, -1, ST_INT64(env->stack, -1) - ST_INT64(env->stack, 0));
				ST_TYPE(env->stack, -1) = TYPE_CMP(ST_TYPE(env->stack, -1),
												   ST_TYPE(env->stack, 0));
				env->stack.stack_pointer--;
				pc++;
				break;
			}
			case LPR_MUL_BYTE: {
				ST_WRITE_INT64(env->stack, -1, ST_INT64(env->stack, -1) * ST_INT64(env->stack, 0));
				ST_TYPE(env->stack, -1) = TYPE_CMP(ST_TYPE(env->stack, -1),
												   ST_TYPE(env->stack, 0));
				env->stack.stack_pointer--;
				pc++;
				break;
			}
			case LPR_DIV_BYTE: {
				ST_WRITE_INT64(env->stack, -1, ST_INT64(env->stack, -1) / ST_INT64(env->stack, 0));
				ST_TYPE(env->stack, -1) = TYPE_CMP(ST_TYPE(env->stack, -1),
												   ST_TYPE(env->stack, 0));
				env->stack.stack_pointer--;
				pc++;
				break;
			}
			case LPR_INC: {
				ST_INT64(env->stack, 0)++;
				pc++;
				break;
			}
			case LPR_DEC: {
				ST_INT64(env->stack, 0)--;
				pc++;
				break;
			}
			case LPR_CALL: {
				Private_invoke_function(env, env->exe->code[pc + 1],
										env->exe->code[pc + 1 + sizeof(Loopr_Int32)], &pc, &base, &local_base);
				break;
			}
			case LPR_GOTO: {
				Loopr_byte_deserialize(&pc, &env->exe->code[pc + 1], sizeof(Loopr_Int32));
				printf("%s#goto: %d\n", (!top_level ? LOWER_LEVEL : ""), pc);
				break;
			}
			case LPR_RETURN: {
				ret_value = Private_do_return(env, &pc, &base, &local_base);
				ST(env->stack, 1) = ret_value;
				env->stack.stack_pointer++;
				break;
			}
			case LPR_NOP: {
				pc++;
				break;
			}
			default: {
				if (env->wflag != LPR_NOTHING) {
					DBG_panic(("Undefined bytecode %d in pc %d\n", env->exe->code[pc], pc));
				} else {
					pc++;
				}
			}
		}
		if (Walle_get_alloc_size() >= Walle_get_threshold()) {
			Walle_update_alive_period();
			Private_walle_marker(Private_get_top_level());
			Walle_check_mem();
		}
	}
	EXECUTE_END:;

	return (env->stack.stack_pointer >= 0 ? ST(env->stack, 0) : NULL);
}
