#ifndef SCRIP_IMAGE_H
#define SCRIP_IMAGE_H
#include <stddef.h>
#include <stdint.h>
/*================================================================================================================================================================================*/
typedef enum {
    SEG_STUBS    = 0,
    SEG_DISPATCH = 1,
    SEG_CODE     = 2,
    SEG_DATA     = 3,
    SEG_COUNT    = 4
} scrip_seg_id;
#define SCRIP_SEG_STUBS_SIZE    (   64 * 1024)
#define SCRIP_SEG_DISPATCH_SIZE (  256 * 1024)
#define SCRIP_SEG_CODE_SIZE     (16 * 1024 * 1024)
#define SCRIP_SEG_DATA_SIZE     ( 4 * 1024 * 1024)
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
typedef struct {
    uint8_t    * base;
    uint8_t    * top;
    uint8_t    * limit;
    int          sealed;
    scrip_seg_id id;
} scrip_seg_t;
extern scrip_seg_t scrip_segs[SEG_COUNT];
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
int      sm_image_init    (void);
void     sm_image_destroy (void);
uint8_t * seg_alloc       (scrip_seg_id id, size_t size);
void      seg_byte        (scrip_seg_id id, uint8_t b);
void      seg_u32         (scrip_seg_id id, uint32_t v);
void      seg_u64         (scrip_seg_id id, uint64_t v);
size_t    seg_offset      (scrip_seg_id id);
void      seg_patch_u32   (scrip_seg_id id, size_t off, uint32_t v);
void      seg_seal        (scrip_seg_id id);
size_t    seg_used        (scrip_seg_id id);
size_t    seg_stubs_add_ptr (void * fn);
void   ** seg_stubs_slot    (size_t off);
/*================================================================================================================================================================================*/
#endif
