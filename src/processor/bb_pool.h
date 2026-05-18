#ifndef BB_POOL_H
#define BB_POOL_H
#include <stddef.h>
#include <stdint.h>
/*================================================================================================================================================================================*/
typedef uint8_t * bb_buf_t;
/* PST-RB-5i: raised from 4MB to 64MB. The 4MB pool was sized for SNOBOL4
   beauty.sno (~few hundred pattern allocations × 16K). The SCRIP-hosted
   parsers compose much larger compound patterns and many more sub-pattern
   BB allocations during pre_build_children. 64MB anon-mmap costs nothing
   if unused (lazy page commit) and gives multi-MB-pattern headroom. */
#define BB_POOL_SIZE   (4 * 1024 * 1024)
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void     bb_pool_init    (void);
int      bb_in_pool      (const void * p);
bb_buf_t bb_alloc        (size_t size);
void     bb_seal         (bb_buf_t buf, size_t size);
void     bb_free         (bb_buf_t buf, size_t size);
void     bb_pool_destroy (void);
void     bb_pool_reset   (void);
size_t   bb_pool_used    (void);
/*================================================================================================================================================================================*/
#endif
