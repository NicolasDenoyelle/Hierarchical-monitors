#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"

/* An array of objects */
struct array{
    void **  cell;
    unsigned length;
    unsigned allocated_length;
    void     (*delete_element)(void*);
    unsigned iterator;
};

struct array * 
new_array(size_t elem_size, unsigned max_elem, void (*delete_element)(void*)){
    unsigned i;
    struct array * array;
    malloc_chk(array, sizeof(*array));
    malloc_chk(array->cell, elem_size*max_elem);
    for(i=0;i<max_elem;i++){
	array->cell[i] = NULL;
    }
    array->length = 0;
    array->iterator = 0;
    array->allocated_length = max_elem;
    array->delete_element = delete_element;
    return array;
}

struct array * array_dup(struct array * array)
{
    unsigned i;
    struct array * copy;
    copy = new_array(sizeof(*(array->cell)), array->allocated_length, array->delete_element);
    memcpy(copy->cell, array->cell,sizeof(*(array->cell))*array->length);
    copy->length = array->length;
    for(i=copy->length; i<array->allocated_length; i++)
	copy->cell[i] = NULL;
    return copy;
}

void 
delete_array(struct array * array){
    unsigned i;
    if(array==NULL)
	return;
    if(array->delete_element!=NULL){
	for(i=0;i<array->length;i++){
	    if(array->cell[i]!=NULL)
		array->delete_element(array->cell[i]);
	}
    }
    free(array->cell);
    free(array);
}

void 
empty_array(struct array * array){
    unsigned i;
    if(array==NULL)
	return;
    if(array->delete_element!=NULL){
	for(i=0;i<array->length;i++){
	    if(array->cell[i]!=NULL)
		array->delete_element(array->cell[i]);
	}
    }
    array->length = 0;
}

static void array_chk_length(struct array * array, unsigned length){
    if(length>=array->allocated_length){
	while((array->allocated_length*=2)<length);
	realloc_chk(array->cell,sizeof(*(array->cell))*array->allocated_length);
	memset(&(array->cell[array->length]), 0, sizeof(*array->cell) * (array->allocated_length-array->length));
    }
}

void * array_get(struct array * array, unsigned i){
    if(array==NULL){return NULL;}
    if(i>=array->length){return NULL;}
    else{return array->cell[i];}
}

void * array_set(struct array * array, unsigned i, void * element){
    void * ret;
    array_chk_length(array,i);
    ret = array->cell[i];
    array->cell[i] = element;
    if(array->length<=i){array->length = i+1;}
    return ret;
}

inline unsigned array_length(struct array * array){
    return array->length;
}

inline void array_push(struct array * array, void * element){
    array_set(array,array->length,element);
}

void * array_pop(struct array * array){
    if(array->length == 0){return NULL;}
    array->length--;
    return array->cell[array->length];
}

void array_insert(struct array * array, unsigned i, void * element){
    if(i>=array->length){return;}
    array_chk_length(array, array->length+1);
    array->length++;
    memmove(&array->cell[i+1], &array->cell[i], (array->length-i-1)*sizeof(*array->cell));
    array->cell[i] = element;
}

void * array_remove(struct array * array, int i){
    void * ret;
    if(i<0 || (unsigned)i >= array->length){return NULL;}
    else{ret = array->cell[i];}
    if(array->length){
	memmove(&array->cell[i], &array->cell[i+1], (array->length-i-1)*sizeof(*array->cell));
	array->length--;
	if(array->iterator == (unsigned)(i+1)){array->iterator--;}
    }	
    return ret;
}

int array_find(struct array * array, void * key, int (* compare)(void*, void*)){
    void * found = bsearch(&key,array->cell, array->length, sizeof(*(array->cell)), (__compar_fn_t)compare);
    if(found == NULL){return -1;}
    return (found - (void*)array->cell) / sizeof(*(array->cell));
}

unsigned array_insert_sorted(struct array * array, void * element, int (* compare)(void*, void*)){
    unsigned insert_index = array->length/2;
    unsigned left_bound = 0,right_bound = array->length-1;
    int comp = compare(element, array->cell[insert_index]);
    while(comp || (insert_index > left_bound && insert_index < right_bound)){
	if(comp > 0){
	    left_bound = insert_index;
	    insert_index += (right_bound - insert_index)/2;
	}
	else{
	    right_bound = insert_index;
	    insert_index -= (insert_index - left_bound)/2;
	}
	comp = compare(element, array->cell[insert_index]);
    }
    array_insert(array, insert_index, element);
    return insert_index;
}

inline void array_sort(struct array * array, int (* compare)(void*, void*)){
    qsort(array->cell, array->length, sizeof(*(array->cell)), (__compar_fn_t)compare);
}


void * array_iterate(struct array * array){
    void * ret;
    if(array == NULL || array->length == 0)
	return NULL;
    if(array->iterator == array->length){array->iterator = 0; return NULL;}

    ret = array->cell[array->iterator];
    array->iterator++;
    return ret;
}

