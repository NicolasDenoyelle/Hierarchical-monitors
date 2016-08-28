#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <hmon/harray.h>
#include "internal.h"

/* An harray of objects */
harray 
new_harray(size_t elem_size, unsigned max_elem, void (*delete_element)(void*)){
    unsigned i;
    harray array;
    malloc_chk(array, sizeof(*array));
    malloc_chk(array->cell, elem_size*max_elem);
    for(i=0;i<max_elem;i++){
	array->cell[i] = NULL;
    }
    array->length = 0;
    array->allocated_length = max_elem;
    array->delete_element = delete_element;
    return array;
}

harray harray_dup(harray array)
{
    unsigned i;
    harray copy;
    copy = new_harray(sizeof(*(array->cell)), array->allocated_length, array->delete_element);
    memcpy(copy->cell, array->cell,sizeof(*(array->cell))*array->length);
    copy->length = array->length;
    for(i=copy->length; i<array->allocated_length; i++)
	copy->cell[i] = NULL;
    return copy;
}

void 
delete_harray(harray array){
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
empty_harray(harray array){
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

static void harray_chk_length(harray array, unsigned length){
    if(length>=array->allocated_length){
	while((array->allocated_length*=2)<length);
	realloc_chk(array->cell,sizeof(*(array->cell))*array->allocated_length);
	memset(&(array->cell[array->length]), 0, sizeof(*array->cell) * (array->allocated_length-array->length));
    }
}

void * harray_get(harray array, unsigned i){
    if(array==NULL){return NULL;}
    if(i>=array->length){return NULL;}
    else{return array->cell[i];}
}

void ** harray_get_data(harray array){
    return array->cell;
}

void * harray_set(harray array, unsigned i, void * element){
    void * ret;
    harray_chk_length(array,i);
    ret = array->cell[i];
    array->cell[i] = element;
    if(array->length<=i){array->length = i+1;}
    return ret;
}

inline unsigned harray_length(harray array){
    return array->length;
}

inline void harray_push(harray array, void * element){
    harray_set(array,array->length,element);
}

void * harray_pop(harray array){
    if(array->length == 0){return NULL;}
    array->length--;
    return array->cell[array->length];
}

void harray_insert(harray array, unsigned i, void * element){
    if(i>=array->length){return;}
    harray_chk_length(array, array->length+1);
    array->length++;
    memmove(&array->cell[i+1], &array->cell[i], (array->length-i-1)*sizeof(*array->cell));
    array->cell[i] = element;
}

void * harray_remove(harray array, int i){
    void * ret;
    if(i<0 || (unsigned)i >= array->length){return NULL;}
    else{ret = array->cell[i];}
    if(array->length){
	memmove(&array->cell[i], &array->cell[i+1], (array->length-i-1)*sizeof(*array->cell));
	array->length--;
    }	
    return ret;
}

int harray_find(harray array, void * key, int (* compare)(void*, void*)){
    void * found = bsearch(&key,array->cell, array->length, sizeof(*(array->cell)), (__compar_fn_t)compare);
    if(found == NULL){return -1;}
    return (found - (void*)array->cell) / sizeof(*(array->cell));
}

int harray_find_unsorted(harray array, void * key){
    for(unsigned i = 0; i<harray_length(array); i++){
	if(harray_get(array, i) == key){return (int)i;}
    }
    return -1;
}


unsigned harray_insert_sorted(harray array, void * element, int (* compare)(void*, void*)){
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
    harray_insert(array, insert_index, element);
    return insert_index;
}

inline void harray_sort(harray array, int (* compare)(void*, void*)){
    qsort(array->cell, array->length, sizeof(*(array->cell)), (__compar_fn_t)compare);
}

