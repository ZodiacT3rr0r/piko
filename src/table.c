#include "table.h"

#include "memory.h"
#include "object.h"

#define TABLE_MAX_LOAD 0.75

uint32_t table_hash_string(const char *key, int length) {
	uint32_t hash = 2166136261u;
	for (int i = 0; i < length; i++) {
		hash ^= (uint8_t)key[i];
		hash *= 16777619u;
	}
	return hash;
}

void table_init(Table *table) {
	table->count = 0;
	table->capacity = 0;
	table->entries = NULL;
}

void table_free(Table *table) {
	if (table->entries != NULL) {
		piko_reallocate(table->entries,
			sizeof(TableEntry) * (size_t)table->capacity, 0);
	}
	table_init(table);
}

static TableEntry *find_entry(TableEntry *entries, int capacity, ObjString *key) {
	uint32_t mask = (uint32_t)(capacity - 1);
	uint32_t index = key->hash & mask;
	TableEntry *tombstone = NULL;

	for (;;) {
		TableEntry *entry = &entries[index];
		if (entry->key == NULL) {
			if (IS_NIL(entry->value)) {
				return tombstone != NULL ? tombstone : entry;
			}
			if (tombstone == NULL) {
				tombstone = entry;
			}
		} else if (entry->key == key ||
				   (entry->key->hash == key->hash &&
					entry->key->length == key->length &&
					memcmp(entry->key->chars, key->chars, (size_t)key->length) == 0)) {
			return entry;
		}

		index = (index + 1) & mask;
	}
}

static void adjust_capacity(Table *table, int new_capacity) {
	TableEntry *entries = (TableEntry *)piko_reallocate(
		NULL, 0, sizeof(TableEntry) * (size_t)new_capacity);
	for (int i = 0; i < new_capacity; i++) {
		entries[i].key = NULL;
		entries[i].value = NIL_VAL;
	}

	table->count = 0;
	for (int i = 0; i < table->capacity; i++) {
		TableEntry *entry = &table->entries[i];
		if (entry->key == NULL) {
			continue;
		}
		TableEntry *dest = find_entry(entries, new_capacity, entry->key);
		dest->key = entry->key;
		dest->value = entry->value;
		table->count++;
	}

	if (table->entries != NULL) {
		piko_reallocate(table->entries,
			sizeof(TableEntry) * (size_t)table->capacity, 0);
	}
	table->entries = entries;
	table->capacity = new_capacity;
}

bool table_get(const Table *table, ObjString *key, Value *out_value) {
	if (table->count == 0 || table->capacity == 0) {
		return false;
	}

	TableEntry *entry = find_entry(table->entries, table->capacity, key);
	if (entry->key == NULL) {
		return false;
	}

	if (out_value != NULL) {
		*out_value = entry->value;
	}
	return true;
}

bool table_set(Table *table, ObjString *key, Value value) {
	if (table->count + 1 > (int)((double)table->capacity * TABLE_MAX_LOAD)) {
		int capacity = table->capacity < 8 ? 8 : table->capacity * 2;
		adjust_capacity(table, capacity);
	}

	TableEntry *entry = find_entry(table->entries, table->capacity, key);
	bool is_new_key = entry->key == NULL;
	if (is_new_key && IS_NIL(entry->value)) {
		table->count++;
	}

	entry->key = key;
	entry->value = value;
	return is_new_key;
}

bool table_delete(Table *table, ObjString *key) {
	if (table->count == 0 || table->capacity == 0) {
		return false;
	}

	TableEntry *entry = find_entry(table->entries, table->capacity, key);
	if (entry->key == NULL) {
		return false;
	}

	/* Place a tombstone sentinel so linear probe chains stay intact. */
	entry->key = NULL;
	entry->value = BOOL_VAL(true);
	return true;
}

ObjString *table_find_string(const Table *table, const char *chars, int length, uint32_t hash) {
	if (table->count == 0 || table->capacity == 0) {
		return NULL;
	}

	uint32_t mask = (uint32_t)(table->capacity - 1);
	uint32_t index = hash & mask;
	for (;;) {
		TableEntry *entry = &table->entries[index];
		if (entry->key == NULL) {
			if (IS_NIL(entry->value)) {
				return NULL;
			}
		} else if (entry->key->length == length &&
				   entry->key->hash == hash &&
				   memcmp(entry->key->chars, chars, (size_t)length) == 0) {
			return entry->key;
		}
		index = (index + 1) & mask;
	}
}

void table_mark(Table *table) {
	for (int i = 0; i < table->capacity; i++) {
		TableEntry *entry = &table->entries[i];
		if (entry->key != NULL) {
			gc_mark_object((Obj *)entry->key);
			gc_mark_value(entry->value);
		}
	}
}

void table_remove_white(Table *table) {
	for (int i = 0; i < table->capacity; i++) {
		TableEntry *entry = &table->entries[i];
		if (entry->key != NULL && !entry->key->obj.is_marked) {
			table_delete(table, entry->key);
		}
	}
}
