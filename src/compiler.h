#ifndef PIKO_COMPILER_H
#define PIKO_COMPILER_H

#include "ast.h"
#include "chunk.h"

bool compile_program(Node *program, Chunk *out_chunk);

#endif
