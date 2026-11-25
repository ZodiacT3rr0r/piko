#ifndef PIKO_TABLE_H
#define PIKO_TABLE_H

#include "common.h"
#include "value.h"

typedef struct ObjString ObjString;

typedef struct {
	ObjString *key;
	Value value;
} TableEntry;

typedef struct Table {
	int count;
	int capacity;
	TableEntry *entries;
} Table;

uint32_t table_hash_string(const char *key, int length);

void table_init(Table *table);
void table_free(Table *table);
bool table_get(const Table *table, ObjString *key, Value *out_value);
bool table_set(Table *table, ObjString *key, Value value);
bool table_delete(Table *table, ObjString *key);
ObjString *table_find_string(const Table *table, const char *chars, int length, uint32_t hash);
void table_mark(Table *table);
void table_remove_white(Table *table);

#endif
