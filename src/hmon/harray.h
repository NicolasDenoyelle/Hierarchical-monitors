#ifndef HARRAY_H
#define HARRAY_H

/************************************************ Array utils **************************************************/
typedef struct hmon_array * harray;

struct hmon_array{
  void **  cell;
  unsigned length;
  unsigned allocated_length;
  void     (*delete_element)(void*);
};

harray         new_harray          (size_t elem_size, unsigned max_elem, void (*delete_element)(void*));
harray         harray_dup          (harray);
void           delete_harray       (harray);
void           empty_harray        (harray);
unsigned       harray_length       (harray);
void *         harray_get          (harray, unsigned);
void **        harray_get_data(harray harray);
void *         harray_set          (harray, unsigned, void *);
void *         harray_pop          (harray);
void           harray_push         (harray, void *);
void *         harray_remove       (harray, int);
void           harray_insert       (harray, unsigned, void *);
unsigned       harray_insert_sorted(harray array, void * element, int (* compare)(void*, void*));
void           harray_sort         (harray array, int (* compare)(void*, void*));
int            harray_find         (harray array, void * key, int (* compare)(void*, void*));
int            harray_find_unsorted(harray harray, void * key);
/***************************************************************************************************************/

#endif

