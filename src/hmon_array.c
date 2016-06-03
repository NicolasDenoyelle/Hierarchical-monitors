#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "monitor_utils.h"

/* An hmon_array of objects */
struct hmon_array{
    void **  cell;
    unsigned length;
    unsigned allocated_length;
    void     (*delete_element)(void*);
};

struct hmon_array * 
new_hmon_array(size_t elem_size, unsigned max_elem, void (*delete_element)(void*)){
    unsigned i;
    struct hmon_array * hmon_array;
    malloc_chk(hmon_array, sizeof(*hmon_array));
    malloc_chk(hmon_array->cell, elem_size*max_elem);
    for(i=0;i<max_elem;i++){
	hmon_array->cell[i] = NULL;
    }
    hmon_array->length = 0;
    hmon_array->allocated_length = max_elem;
    hmon_array->delete_element = delete_element;
    return hmon_array;
}

struct hmon_array * hmon_array_dup(struct hmon_array * hmon_array)
{
    unsigned i;
    struct hmon_array * copy;
    copy = new_hmon_array(sizeof(*(hmon_array->cell)), hmon_array->allocated_length, hmon_array->delete_element);
    memcpy(copy->cell, hmon_array->cell,sizeof(*(hmon_array->cell))*hmon_array->length);
    copy->length = hmon_array->length;
    for(i=copy->length; i<hmon_array->allocated_length; i++)
	copy->cell[i] = NULL;
    return copy;
}

void 
delete_hmon_array(struct hmon_array * hmon_array){
    unsigned i;
    if(hmon_array==NULL)
	return;
    if(hmon_array->delete_element!=NULL){
	for(i=0;i<hmon_array->length;i++){
	    if(hmon_array->cell[i]!=NULL)
		hmon_array->delete_element(hmon_array->cell[i]);
	}
    }
    free(hmon_array->cell);
    free(hmon_array);
}

void 
empty_hmon_array(struct hmon_array * hmon_array){
    unsigned i;
    if(hmon_array==NULL)
	return;
    if(hmon_array->delete_element!=NULL){
	for(i=0;i<hmon_array->length;i++){
	    if(hmon_array->cell[i]!=NULL)
		hmon_array->delete_element(hmon_array->cell[i]);
	}
    }
    hmon_array->length = 0;
}

static void hmon_array_chk_length(struct hmon_array * hmon_array, unsigned length){
    if(length>=hmon_array->allocated_length){
	while((hmon_array->allocated_length*=2)<length);
	realloc_chk(hmon_array->cell,sizeof(*(hmon_array->cell))*hmon_array->allocated_length);
	memset(&(hmon_array->cell[hmon_array->length]), 0, sizeof(*hmon_array->cell) * (hmon_array->allocated_length-hmon_array->length));
    }
}

void * hmon_array_get(struct hmon_array * hmon_array, unsigned i){
    if(hmon_array==NULL){return NULL;}
    if(i>=hmon_array->length){return NULL;}
    else{return hmon_array->cell[i];}
}

void * hmon_array_set(struct hmon_array * hmon_array, unsigned i, void * element){
    void * ret;
    hmon_array_chk_length(hmon_array,i);
    ret = hmon_array->cell[i];
    hmon_array->cell[i] = element;
    if(hmon_array->length<=i){hmon_array->length = i+1;}
    return ret;
}

inline unsigned hmon_array_length(struct hmon_array * hmon_array){
    return hmon_array->length;
}

inline void hmon_array_push(struct hmon_array * hmon_array, void * element){
    hmon_array_set(hmon_array,hmon_array->length,element);
}

void * hmon_array_pop(struct hmon_array * hmon_array){
    if(hmon_array->length == 0){return NULL;}
    hmon_array->length--;
    return hmon_array->cell[hmon_array->length];
}

void hmon_array_insert(struct hmon_array * hmon_array, unsigned i, void * element){
    if(i>=hmon_array->length){return;}
    hmon_array_chk_length(hmon_array, hmon_array->length+1);
    hmon_array->length++;
    memmove(&hmon_array->cell[i+1], &hmon_array->cell[i], (hmon_array->length-i-1)*sizeof(*hmon_array->cell));
    hmon_array->cell[i] = element;
}

void * hmon_array_remove(struct hmon_array * hmon_array, int i){
    void * ret;
    if(i<0 || (unsigned)i >= hmon_array->length){return NULL;}
    else{ret = hmon_array->cell[i];}
    if(hmon_array->length){
	memmove(&hmon_array->cell[i], &hmon_array->cell[i+1], (hmon_array->length-i-1)*sizeof(*hmon_array->cell));
	hmon_array->length--;
    }	
    return ret;
}

int hmon_array_find(struct hmon_array * hmon_array, void * key, int (* compare)(void*, void*)){
    void * found = bsearch(&key,hmon_array->cell, hmon_array->length, sizeof(*(hmon_array->cell)), (__compar_fn_t)compare);
    if(found == NULL){return -1;}
    return (found - (void*)hmon_array->cell) / sizeof(*(hmon_array->cell));
}

unsigned hmon_array_insert_sorted(struct hmon_array * hmon_array, void * element, int (* compare)(void*, void*)){
    unsigned insert_index = hmon_array->length/2;
    unsigned left_bound = 0,right_bound = hmon_array->length-1;
    int comp = compare(element, hmon_array->cell[insert_index]);
    while(comp || (insert_index > left_bound && insert_index < right_bound)){
	if(comp > 0){
	    left_bound = insert_index;
	    insert_index += (right_bound - insert_index)/2;
	}
	else{
	    right_bound = insert_index;
	    insert_index -= (insert_index - left_bound)/2;
	}
	comp = compare(element, hmon_array->cell[insert_index]);
    }
    hmon_array_insert(hmon_array, insert_index, element);
    return insert_index;
}

inline void hmon_array_sort(struct hmon_array * hmon_array, int (* compare)(void*, void*)){
    qsort(hmon_array->cell, hmon_array->length, sizeof(*(hmon_array->cell)), (__compar_fn_t)compare);
}

