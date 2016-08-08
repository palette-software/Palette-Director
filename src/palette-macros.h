#pragma once

#define PAL__SLICE_TYPE(type, entity_name)                                     \
  typedef struct entity_name {                                                 \
    type* entries;                                                             \
    size_t count;                                                              \
  } entity_name;                                                               \
                                                                               \
  entity_name malloc_##entity_name(size_t count);                              \
                                                                               \
  void free_##entity_name(entity_name* e);                                     \
                                                                               \
  entity_name entity_name##_from_array(type* arr, size_t count);               \
                                                                               \
  typedef int (*entity_name##_filter_fn)(const type* e, void* state);          \
                                                                               \
  entity_name entity_name##_filter(const entity_name e,                        \
                                   entity_name##_filter_fn filter_fn,          \
                                   const size_t max_buffer_size, void* state); \
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
  entity_name entity_name##_filter(                               \
      const entity_name e, entity_name##_filter_fn filter_fn,     \
      const size_t max_buffer_size, void* state) {                \
    type* buffer = (type*)malloc(sizeof(type) * max_buffer_size); \
    size_t i, len = e.count, buffer_size = 0;                     \
    entity_name out = empty_##entity_name;                        \
    for (i = 0; i < len; ++i) {                                   \
      type entry = e.entries[i];                                  \
      if (filter_fn(&entry, state) != 0) {                        \
        buffer[buffer_size] = entry;                              \
        buffer_size++;                                            \
      }                                                           \
    }                                                             \
                                                                  \
    out = entity_name##_from_array(buffer, buffer_size);          \
    free(buffer);                                                 \
    return out;                                                   \
  }                                                               \
                                                                  \
  const entity_name empty_##entity_name = {NULL, 0};