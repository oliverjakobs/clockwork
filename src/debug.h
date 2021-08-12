#ifndef CLOCKWORK_DEBUG_H
#define CLOCKWORK_DEBUG_H

#include "chunk.h"

void cw_disassemble_chunk(const Chunk* chunk, const char* name);
int  cw_disassemble_instruction(const Chunk* chunk, int offset);

void print_value(Value val);
void print_object(Value val);

#endif /* !CLOCKWORK_DEBUG_H */