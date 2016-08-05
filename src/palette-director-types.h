#ifndef HTTPD_PALETTE_DIRECTOR_TYPES_H
#define HTTPD_PALETTE_DIRECTOR_TYPES_H

#include <stddef.h>
#include <stdint.h>

#define PAL__SLICE_TYPE(type, entity_name) \
    typedef struct entity_name { type* entries; size_t count; } entity_name; \
    entity_name malloc_ ## entity_name ( size_t count ); \
    void free_ ## entity_name ( entity_name* e ); \
	entity_name copy_array( type* arr, size_t count); \
	extern const entity_name empty_ ## entity_name;

#define PAL__SLICE_TYPE_IMPL(type, entity_name) \
	entity_name malloc_ ## entity_name ( size_t count ) { entity_name o = { (type*)malloc(sizeof(type) * count), count }; return o; } \
	void free_ ## entity_name ( entity_name* e  ) { free(e->entries); } \
	entity_name copy_array( type* arr, size_t count) { entity_name o = malloc_ ## entity_name ( count ); memcpy(o.entries, arr, sizeof(type) * count); return o; } \
	const entity_name empty_ ## entity_name = { NULL, 0 };



enum {
    // The size of the stack buffers we'll use in storing temporary bindings
    kBINDINGS_BUFFER_SIZE = 512,

    // The size of the stack buffer for the workers lists
    kWORKERS_BUFFER_SIZE = 256,

    // The buffer size for each substring we'll be reading from a line
    kCONFIG_MAX_STRING_SIZE = 256,
};


// The handler name we'll use to display the status pages
static const char* PALETTE_DIRECTOR_STATUS_HANDLER = "palette-director-status";


typedef struct binding_row {

    // For identity checks
    size_t row_id;

    // We bind this site
    const char* site_name;

    // To this worker host
    const char* worker_host;

    // The base priority for the route.
    int priority;

    // True if this is a fallback route so we dont need to do
    // site name matching (and these routes are always lower priority
    // then the named routes)
    int is_fallback;

} binding_row;


PAL__SLICE_TYPE(binding_row, binding_rows );


#endif //HTTPD_PALETTE_DIRECTOR_TYPES_H