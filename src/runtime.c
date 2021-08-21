#include "runtime.h"

#include <stdio.h>
#include <stdarg.h>

#include "debug.h"
#include "memory.h"
#include "compiler.h"

static void cw_runtime_error(cwRuntime* cw, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = cw->ip - cw->chunk->bytes - 1;
    int line = cw->chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    cw_reset_stack(cw);
}

void cw_init(cwRuntime* cw)
{
    cw->chunk = NULL;
    cw->ip = NULL;
    cw->objects = NULL;
    cw_table_init(&cw->strings);
    cw_reset_stack(cw);
}

void cw_free(cwRuntime* cw)
{
    cw_table_free(&cw->strings);
    cw_free_objects(cw);
}

static InterpretResult cw_run(cwRuntime* cw)
{
#define READ_BYTE()     (*cw->ip++)
#define READ_CONSTANT() (cw->chunk->constants[READ_BYTE()])
#define BINARY_OP(value_type, op)                                                   \
    do {                                                                            \
        if (!IS_NUMBER(cw_peek_stack(cw, 0)) || !IS_NUMBER(cw_peek_stack(cw, 1)))   \
        {                                                                           \
            cw_runtime_error(cw, "Operands must be numbers.");                      \
            return INTERPRET_RUNTIME_ERROR;                                         \
        }                                                                           \
        double b = AS_NUMBER(cw_pop_stack(cw));                                     \
        double a = AS_NUMBER(cw_pop_stack(cw));                                     \
        cw_push_stack(cw, value_type(a op b));                                      \
    } while (false)

    while (true)
    {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value* slot = cw->stack; slot < cw->stack + cw->stack_index; ++slot)
        {
            printf("[ ");
            cw_print_value(*slot);
            printf(" ]");
        }
        printf("\n");
        cw_disassemble_instruction(cw->chunk, (int)(cw->ip - cw->chunk->bytes));
#endif
        uint8_t instruction;
        switch (instruction = READ_BYTE())
        {
            case OP_CONSTANT:
            {
                Value constant = READ_CONSTANT();
                cw_push_stack(cw, constant);
                break;
            }
            case OP_NULL:     cw_push_stack(cw, MAKE_NULL()); break;
            case OP_TRUE:     cw_push_stack(cw, MAKE_BOOL(true)); break;
            case OP_FALSE:    cw_push_stack(cw, MAKE_BOOL(false)); break;
            case OP_POP:      cw_pop_stack(cw); break;
            case OP_EQ: case OP_NOTEQ:
            {
                Value b = cw_pop_stack(cw);
                Value a = cw_pop_stack(cw);
                bool eq = cw_values_equal(a, b);
                cw_push_stack(cw, MAKE_BOOL((instruction == OP_EQ ? eq : !eq)));
                break;
            }
            case OP_LT:       BINARY_OP(MAKE_BOOL, <); break;
            case OP_GT:       BINARY_OP(MAKE_BOOL, >); break;
            case OP_LTEQ:     BINARY_OP(MAKE_BOOL, <=); break;
            case OP_GTEQ:     BINARY_OP(MAKE_BOOL, >=); break;
            case OP_ADD:
            {
                if (IS_STRING(cw_peek_stack(cw, 0)) && IS_STRING(cw_peek_stack(cw, 1)))
                {
                    cwString* b = AS_STRING(cw_pop_stack(cw));
                    cwString* a = AS_STRING(cw_pop_stack(cw));
                    cw_push_stack(cw, MAKE_OBJECT(cw_str_concat(cw, a, b)));
                }
                else if (IS_NUMBER(cw_peek_stack(cw, 0)) && IS_NUMBER(cw_peek_stack(cw, 1)))
                {
                    double b = AS_NUMBER(cw_pop_stack(cw));
                    double a = AS_NUMBER(cw_pop_stack(cw));
                    cw_push_stack(cw, MAKE_NUMBER(a + b));
                }
                else
                {
                    cw_runtime_error(cw, "Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(MAKE_NUMBER, -); break;
            case OP_MULTIPLY: BINARY_OP(MAKE_NUMBER, *); break;
            case OP_DIVIDE:   BINARY_OP(MAKE_NUMBER, /); break;
            case OP_NOT:      cw_push_stack(cw, MAKE_BOOL(cw_is_falsey(cw_pop_stack(cw)))); break;
            case OP_NEGATE:   
                if (!IS_NUMBER(cw_peek_stack(cw, 0)))
                {
                    cw_runtime_error(cw, "Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                cw_push_stack(cw, MAKE_NUMBER(-AS_NUMBER(cw_pop_stack(cw))));
                break;
            case OP_PRINT:
                cw_print_value(cw_pop_stack(cw));
                printf("\n");
                break;
            case OP_RETURN:
                return INTERPRET_OK;
        }
    }

#undef BINARY_OP
#undef READ_CONSTANT
#undef READ_BYTE
}

InterpretResult cw_interpret(cwRuntime* cw, const char* src)
{
    Chunk chunk;
    cw_chunk_init(&chunk);

    InterpretResult result = INTERPRET_COMPILE_ERROR;
    if (cw_compile(cw, src, &chunk))
    {
        cw->chunk = &chunk;
        cw->ip = cw->chunk->bytes;

        result = cw_run(cw);
    }

    cw_chunk_free(&chunk);
    return result;
}

/* stack operations */
void  cw_push_stack(cwRuntime* cw, Value val)
{
    if (cw->stack_index >= CW_STACK_MAX)
    {
        cw_runtime_error(cw, "Stack overflow");
        return;
    }

    cw->stack[cw->stack_index++] = val;
}

Value cw_pop_stack(cwRuntime* cw)
{
    return cw->stack[--cw->stack_index];
}

void cw_reset_stack(cwRuntime* cw)
{
    cw->stack_index = 0;
}

Value cw_peek_stack(cwRuntime* cw, int distance)
{
    return cw->stack[cw->stack_index - 1 -distance];
}