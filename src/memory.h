#ifndef PIKO_MEMORY_H
#define PIKO_MEMORY_H

#include "common.h"
#include "value.h"

typedef struct VM VM;
typedef struct ObjString ObjString;

void *piko_reallocate(void *pointer, size_t old_size, size_t new_size);
void gc_bind_vm(VM *vm);
void gc_unbind_vm(VM *vm);
void gc_set_enabled(bool enabled);

void gc_register_object(Obj *obj, size_t size);
ObjString *gc_intern_find(const char *chars, int length, uint32_t hash);
void gc_intern_add(ObjString *string);

void gc_mark_object(Obj *obj);
void gc_mark_value(Value value);
void gc_collect(void);
void gc_free_all(void);

size_t gc_bytes_allocated(void);
size_t gc_collections(void);

#endif
