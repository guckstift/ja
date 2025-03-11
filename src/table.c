#ifndef table_H
#define table_H

#ifndef IMPLEMENT_FLAG
#define IMPLEMENT_FLAG
#define table_C
#endif

#include "ast.c"
#include "array.c"

#define table_for(table, i) for(int64_t i = 0; i < array_length((table)->slots); i ++) if((table)->slots[i].key)

void table_init(Table *table);
void table_print(Table *table);
Token *table_add(Table *table, Token *key, void *value);

#endif
#ifdef table_C

#include <string.h>
#include "print.c"
#include "error.c"

static uint64_t get_hash(Token *ident)
{
	if(ident->hash) return ident->hash;
	uint64_t hash = ident->length;

	for(uint64_t i=0; i < ident->length; i++) {
		uint8_t c = ident->start[i];
		hash *= 63;
		if(c <= '9')      hash += c - '0';
		else if(c <= 'Z') hash += c - 'A' + 10;
		else if(c == '_') hash += 36;
		else              hash += c - 'a' + 37;
	}

	return ident->hash = hash;
}

void table_init(Table *table)
{
	array_resize(table->slots, 16);
	table->usage = 0;
}

void table_print(Table *table)
{
	debug_print("# table\n");

	array_for(table->slots, i) {
		Slot *slot = table->slots + i;
		debug_print("%i : ", i);

		if(slot->key) {
			debug_print("%n<%p> (probe=%i)", slot->key, slot->key->uid, slot->probe);
		}

		debug_print("\n");
	}
}

static void table_grow(Table *table)
{
	if(!table->slots) table_init(table);
	int64_t old_capacity = array_length(table->slots);
	int64_t capacity = old_capacity * 2;

	Slot *old_slots = table->slots;
	array_resize(table->slots, capacity);

	if(old_slots != table->slots)
		debug_error("INTERNAL: table slots relocated");

	for(int64_t old_index = 0; old_index < old_capacity; old_index ++) {
		Slot *old_slot = table->slots + old_index;
		Token *old_key = old_slot->key;
		void *old_value = old_slot->value;

		if(old_key) {
			uint64_t hash = get_hash(old_key);
			uint64_t new_index = hash % capacity;

			if(new_index != old_index) {
				old_slot->key = 0;
				old_slot->value = 0;

				for(uint64_t probe = 0; probe < capacity; probe ++) {
					uint64_t probe_index = (new_index + probe) % capacity;
					Slot *probe_slot = table->slots + probe_index;
					Token *probe_key = probe_slot->key;

					if(probe_key == 0) {
						probe_slot->key = old_key;
						probe_slot->value = old_value;
						probe_slot->probe = probe;
						break;
					}
				}
			}
		}
	}
}

Token *table_add(Table *table, Token *key, void *value)
{
	if(!table->slots) table_init(table);

	if(table->usage >= array_length(table->slots))
		table_grow(table);

	uint64_t capacity = array_length(table->slots);
	uint64_t hash = get_hash(key);

	//table_print(table);
	//debug_print("# add %n\n", key);

	for(uint64_t probe = 0; probe < capacity; probe ++) {
		uint64_t probe_index = (hash + probe) % capacity;
		Slot *probe_slot = table->slots + probe_index;
		Token *probe_key = probe_slot->key;

		if(probe_key == 0) {
			probe_slot->key = key;
			probe_slot->value = value;
			probe_slot->probe = probe;
			table->usage ++;
			return key;
		}

		if(key == probe_key) return probe_key;

		if(
			key->length == probe_key->length &&
			(key->start == probe_key->start || memcmp(key->start, probe_key->start, key->length) == 0)
		) {
			return probe_key;
		}
	}

	error("INTERNAL: could not add id to idset");
	return 0;
}

#endif