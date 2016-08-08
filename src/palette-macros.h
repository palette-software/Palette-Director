#pragma once

#define PAL__SLICE_TYPE(type, entity_name)                       \
  typedef struct entity_name {                                   \
    type* entries;                                               \
    size_t count;                                                \
  } entity_name;                                                 \
																 \
  entity_name malloc_##entity_name(size_t count);                \
																 \
  void free_##entity_name(entity_name* e);                       \
																 \
  entity_name entity_name##_from_array(type* arr, size_t count); \
																 \
  extern const entity_name empty_##entity_name;


/*

	Implements the slice types and methods

*/

#define PAL__SLICE_TYPE_IMPL(type, entity_name)                   \
  entity_name malloc_##entity_name(size_t count) {                \
    entity_name o = {(type*)malloc(sizeof(type) * count), count}; \
    return o;                                                     \
  }                                                               \
																  \
  void free_##entity_name(entity_name* e) { free(e->entries); }   \
																  \
  entity_name entity_name##_from_array(type* arr, size_t count) { \
    entity_name o = malloc_##entity_name(count);                  \
    memcpy(o.entries, arr, sizeof(type) * count);                 \
    return o;                                                     \
  }                                                               \
																  \
  const entity_name empty_##entity_name = {NULL, 0};