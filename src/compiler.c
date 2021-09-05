#include "compiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "statement.h"

#include "debug.h"
#include "memory.h"
#include "runtime.h"


/* --------------------------| identifiers |--------------------------------------------- */
uint8_t cw_make_constant(cwRuntime* cw, cwValue value)
{
    int constant = cw_chunk_add_constant(cw->chunk, value);
    if (constant > UINT8_MAX)
    {
        cw_syntax_error_at(cw, &cw->previous, "Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

uint8_t cw_identifier_constant(cwRuntime* cw, cwToken* name)
{
    return cw_make_constant(cw, MAKE_OBJECT(cw_str_copy(cw, name->start, name->end - name->start)));
}

bool cw_identifiers_equal(const cwToken* a, const cwToken* b)
{
    int a_len = a->end - a->start;
    int b_len = b->end - b->start;

    return (a_len == b_len) ? memcmp(a->start, b->start, a_len) == 0 : false;
}

/* --------------------------| locals |-------------------------------------------------- */
void cw_add_local(cwRuntime* cw, cwToken* name)
{
    if (cw->local_count > UINT8_MAX)
    {
        cw_syntax_error_at(cw, &cw->previous, "Too many variables in scope.");
        return;
    }

    cwLocal* local = &cw->locals[cw->local_count++];
    local->name = *name;
    local->depth = -1;
}

int cw_resolve_local(cwRuntime* cw, cwToken* name)
{
    for (int i = cw->local_count - 1; i >= 0; i--)
    {
        cwLocal* local = &cw->locals[i]; 
        if (cw_identifiers_equal(name, &local->name))
        {
            if (local->depth < 0) 
                cw_syntax_error_at(cw, name, "Can not read local variable in its own initializer.");
            return i;
        } 
    }
    return -1;
}

/* --------------------------| chunk |--------------------------------------------------- */
void cw_chunk_init(cwChunk* chunk)
{
    chunk->bytes = NULL;
    chunk->lines = NULL;
    chunk->len = 0;
    chunk->cap = 0;
    chunk->constants = NULL;
    chunk->const_len = 0;
    chunk->const_cap = 0;
}

void cw_chunk_free(cwChunk* chunk)
{
    CW_FREE_ARRAY(uint8_t, chunk->bytes, chunk->cap);
    CW_FREE_ARRAY(int, chunk->lines, chunk->cap);
    CW_FREE_ARRAY(cwValue, chunk->constants, chunk->const_cap);
    cw_chunk_init(chunk);
}

void cw_chunk_write(cwChunk* chunk, uint8_t byte, int line)
{
    if (chunk->cap < chunk->len + 1)
    {
        int old_cap = chunk->cap;
        chunk->cap = CW_GROW_CAPACITY(old_cap);
        chunk->bytes = CW_GROW_ARRAY(uint8_t, chunk->bytes, old_cap, chunk->cap);
        chunk->lines = CW_GROW_ARRAY(int, chunk->lines, old_cap, chunk->cap);
    }

    chunk->bytes[chunk->len] = byte;
    chunk->lines[chunk->len] = line;
    chunk->len++;
}

int cw_chunk_add_constant(cwChunk* chunk, cwValue val)
{
    if (chunk->const_cap < chunk->const_len + 1)
    {
        int old_cap = chunk->const_cap;
        chunk->const_cap = CW_GROW_CAPACITY(old_cap);
        chunk->constants = CW_GROW_ARRAY(cwValue, chunk->constants, old_cap, chunk->const_cap);
    }

    chunk->constants[chunk->const_len] = val;
    return chunk->const_len++;
}

/* --------------------------| writing byte code |--------------------------------------- */
void cw_emit_byte(cwRuntime* cw, uint8_t byte)
{
    cw_chunk_write(cw->chunk, byte, cw->previous.line);
}

void cw_emit_bytes(cwRuntime* cw, uint8_t a, uint8_t b)
{
    cw_emit_byte(cw, a);
    cw_emit_byte(cw, b);
}

int cw_emit_jump(cwRuntime* cw, uint8_t instruction)
{
    cw_emit_byte(cw, instruction);
    cw_emit_byte(cw, 0xff);
    cw_emit_byte(cw, 0xff);
    return cw->chunk->len - 2;
}

void cw_patch_jump(cwRuntime* cw, int offset)
{
    /* -2 to adjust for the bytecode for the jump offset itself. */
    int jump = cw->chunk->len - offset - 2;

    if (jump > UINT16_MAX)
        cw_syntax_error_at(cw, &cw->previous, "Too much code to jump over.");

    cw->chunk->bytes[offset] = (jump >> 8) & 0xff;
    cw->chunk->bytes[offset + 1] = jump & 0xff;
}

/* --------------------------| compiling |----------------------------------------------- */
static void cw_compiler_end(cwRuntime* cw)
{
    cw_emit_byte(cw, OP_RETURN);
#ifdef DEBUG_PRINT_CODE
    if (!cw->error) cw_disassemble_chunk(cw->chunk, "code");
#endif 
}

bool cw_compile(cwRuntime* cw, const char* src, cwChunk* chunk)
{
    /* init first token */
    cw->current.type = TOKEN_NULL;
    cw->current.start = src;
    cw->current.end = src;
    cw->current.line = 1;

    /* init compiler */
    cw->chunk = chunk;
    cw->local_count = 0;
    cw->scope_depth = 0;
    cw->error = false;
    cw->panic = false;

    cw_advance(cw);

    while (!cw_match(cw, TOKEN_EOF))
    {
        cw_parse_declaration(cw);
    }

    cw_compiler_end(cw);
    return !cw->error;
}