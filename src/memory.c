#include "memory.h"

#include "object.h"
#include "table.h"
#include "vm.h"

#define GC_INITIAL_THRESHOLD (64 * 1024)
#define GC_GROWTH_FACTOR     2

typedef struct {
	Obj *objects;
	size_t bytes_allocated;
	size_t next_gc;
	size_t collections;

	int gray_count;
	int gray_capacity;
	Obj **gray_stack;

	VM *active_vm;
	bool enabled;
	bool in_gc;
} GCState;

static GCState gc = {
	.objects = NULL,
	.bytes_allocated = 0,
	.next_gc = GC_INITIAL_THRESHOLD,
	.collections = 0,
	.gray_count = 0,
	.gray_capacity = 0,
	.gray_stack = NULL,
	.active_vm = NULL,
	.enabled = false,
	.in_gc = false,
};

void *piko_reallocate(void *pointer, size_t old_size, size_t new_size) {
	if (new_size >= old_size) {
		gc.bytes_allocated += new_size - old_size;
	} else {
		size_t diff = old_size - new_size;
		gc.bytes_allocated = gc.bytes_allocated > diff ? gc.bytes_allocated - diff : 0;
	}

	if (new_size > old_size && gc.enabled && !gc.in_gc && gc.active_vm != NULL) {
		if (gc.bytes_allocated > gc.next_gc) {
			gc_collect();
		}
	}

	if (new_size == 0) {
		free(pointer);
		return NULL;
	}

	void *result = realloc(pointer, new_size);
	if (result == NULL) {
		fprintf(stderr, "piko: out of memory\n");
		exit(1);
	}
	return result;
}

void gc_bind_vm(VM *vm) {
	gc.active_vm = vm;
}

void gc_unbind_vm(VM *vm) {
	if (gc.active_vm == vm) {
		gc.active_vm = NULL;
		gc.enabled = false;
	}
}

void gc_set_enabled(bool enabled) {
	gc.enabled = enabled;
}

void gc_register_object(Obj *obj, size_t size) {
	obj->is_marked = false;
	obj->next = gc.objects;
	gc.objects = obj;
	(void)size;
}

ObjString *gc_intern_find(const char *chars, int length, uint32_t hash) {
	if (gc.active_vm == NULL) {
		return NULL;
	}
	return table_find_string(&gc.active_vm->strings, chars, length, hash);
}

void gc_intern_add(ObjString *string) {
	if (gc.active_vm == NULL) {
		return;
	}
	/* Temporarily disable GC while inserting into the intern table so
	 * the newly allocated string cannot be swept before being rooted. */
	bool prev = gc.enabled;
	gc.enabled = false;
	table_set(&gc.active_vm->strings, string, NIL_VAL);
	gc.enabled = prev;
}

void gc_mark_object(Obj *obj) {
	if (obj == NULL || obj->is_marked) {
		return;
	}

	obj->is_marked = true;
	if (gc.gray_count + 1 > gc.gray_capacity) {
		int new_cap = gc.gray_capacity < 8 ? 8 : gc.gray_capacity * 2;
		Obj **grown = (Obj **)realloc(gc.gray_stack, sizeof(Obj *) * (size_t)new_cap);
		if (grown == NULL) {
			fprintf(stderr, "piko: out of memory\n");
			exit(1);
		}
		gc.gray_stack = grown;
		gc.gray_capacity = new_cap;
	}
	gc.gray_stack[gc.gray_count++] = obj;
}

void gc_mark_value(Value value) {
	if (IS_OBJ(value)) {
		gc_mark_object(AS_OBJ(value));
	}
}

static void mark_value_array(const ValueArray *array) {
	for (int i = 0; i < array->count; i++) {
		gc_mark_value(array->values[i]);
	}
}

static void blacken_object(Obj *obj) {
	switch (obj->type) {
		case OBJ_STRING:
		case OBJ_NATIVE:
			break;
		case OBJ_UPVALUE: {
			ObjUpvalue *up = (ObjUpvalue *)obj;
			gc_mark_value(up->closed);
			break;
		}
		case OBJ_FUNCTION: {
			ObjFunction *fn = (ObjFunction *)obj;
			mark_value_array(&fn->chunk.constants);
			break;
		}
		case OBJ_CLOSURE: {
			ObjClosure *closure = (ObjClosure *)obj;
			gc_mark_object((Obj *)closure->function);
			for (int i = 0; i < closure->capture_count; i++) {
				gc_mark_object((Obj *)closure->captures[i]);
			}
			break;
		}
		case OBJ_ARRAY: {
			ObjArray *array = (ObjArray *)obj;
			for (int i = 0; i < array->count; i++) {
				gc_mark_value(array->items[i]);
			}
			break;
		}
		case OBJ_MAP: {
			ObjMap *map = (ObjMap *)obj;
			table_mark(&map->table);
			for (int i = 0; i < map->order_count; i++) {
				gc_mark_object((Obj *)map->keys_order[i]);
			}
			break;
		}
	}
}

static void mark_roots(VM *vm) {
	for (Value *slot = vm->stack; slot < vm->stack_top; slot++) {
		gc_mark_value(*slot);
	}

	for (int i = 0; i < vm->frame_count; i++) {
		if (vm->frames[i].closure != NULL) {
			gc_mark_object((Obj *)vm->frames[i].closure);
		}
		if (vm->frames[i].chunk != NULL) {
			mark_value_array(&vm->frames[i].chunk->constants);
		}
	}

	for (ObjUpvalue *up = vm->open_upvalues; up != NULL; up = up->next) {
		gc_mark_object((Obj *)up);
	}

	for (int i = 0; i < vm->temp_root_count; i++) {
		gc_mark_value(vm->temp_roots[i]);
	}

	table_mark(&vm->globals);
}

static void trace_references(void) {
	while (gc.gray_count > 0) {
		Obj *obj = gc.gray_stack[--gc.gray_count];
		blacken_object(obj);
	}
}

static void sweep(void) {
	Obj *prev = NULL;
	Obj *curr = gc.objects;
	while (curr != NULL) {
		if (curr->is_marked) {
			curr->is_marked = false;
			prev = curr;
			curr = curr->next;
		} else {
			Obj *unreached = curr;
			curr = curr->next;
			if (prev != NULL) {
				prev->next = curr;
			} else {
				gc.objects = curr;
			}
			obj_free(unreached);
		}
	}
}

void gc_collect(void) {
	if (gc.active_vm == NULL || gc.in_gc) {
		return;
	}

	gc.in_gc = true;
	mark_roots(gc.active_vm);
	trace_references();
	table_remove_white(&gc.active_vm->strings);
	sweep();

	gc.collections++;
	size_t next = gc.bytes_allocated * GC_GROWTH_FACTOR;
	gc.next_gc = next < GC_INITIAL_THRESHOLD ? GC_INITIAL_THRESHOLD : next;
	gc.in_gc = false;
}

void gc_free_all(void) {
	gc.in_gc = true;
	gc.enabled = false;

	Obj *curr = gc.objects;
	while (curr != NULL) {
		Obj *next = curr->next;
		obj_free(curr);
		curr = next;
	}
	gc.objects = NULL;

	free(gc.gray_stack);
	gc.gray_stack = NULL;
	gc.gray_count = 0;
	gc.gray_capacity = 0;
	gc.bytes_allocated = 0;
	gc.next_gc = GC_INITIAL_THRESHOLD;
	gc.in_gc = false;
}

size_t gc_bytes_allocated(void) {
	return gc.bytes_allocated;
}

size_t gc_collections(void) {
	return gc.collections;
}
