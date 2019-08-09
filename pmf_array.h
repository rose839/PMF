/* Define an array of variable lengths */

#ifndef PMF_ARRAY_H
#define PMF_ARRAY_H

#include <stdlib.h>
#include <string.h>

typedef struct _pmf_array_s {
	void *data;
	size_t member_size;
	size_t used_num;
	size_t allocated_num;
}PMF_ARRAY_S;

static inline PMF_ARRAY_S *pmf_array_init(PMF_ARRAY_S *array, unsigned int member_size, unsigned int initial_num) {
	void *allocated = NULL;

	if (!array) {
		array = malloc(sizeof(PMF_ARRAY_S));

		if (!array) {
			return NULL;
		}
		allocated = array;
	}

	array->member_size = member_size;
	array->data = calloc(initial_num, member_size);
	if (!array->data) {
		free(allocated);
		return NULL;
	}

	array->allocated_num = initial_num;
	array->used_num = 0;

	return array;
}

static inline void *pmf_array_item(PMF_ARRAY_S *array, unsigned int n) {
	if (n > array->used_num-1) {
		return NULL;
	}

	return (void*)((char*)array->data + array->member_size*n);
}

static inline void *pmf_array_item_last(PMF_ARRAY_S *array) {
	return pmf_array_item(array, array->used_num - 1);	
}

static inline int pmf_array_remove_item(PMF_ARRAY_S *array, unsigned int n) {
	int ret = -1;

	if (n > array->used_num-1) {
		return ret;
	}

	if (n < array->used_num-1) {
		void *last = pmf_array_item_last(array);
		void *to_remove = pmf_array_item(array, n);

		memcpy(to_remove, last, array->member_size);
		ret = n;
	}
	array->used_num--;

	return ret;
}

static inline void *pmf_array_push(PMF_ARRAY_S *array) {
	void *ret;

	if (array->used_num == array->allocated_num) {
		size_t new_allocated_num = array->allocated_num ? array->allocated_num*2 : 20;
		void *new_ptr = realloc(array->data, array->member_size*new_allocated_num);

		if (!new_ptr) {
			return NULL;
		}
		array->data = new_ptr;
		array->allocated_num= new_allocated_num;
	}
	
	ret = pmf_array_item(array, array->used_num);
	++array->used_num;

	return ret;
}

static inline void pmf_array_free(PMF_ARRAY_S *array) /* {{{ */
{
	free(array->data);
	array->data = 0;
	array->member_size = 0;
	array->used_num = array->allocated_num = 0;
}

#endif