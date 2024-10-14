/**
 * @file 		libmem.h 
 *
 * @brief 		Header file for memory management library
 *
 * @copyright   Copyright (C) 2024 Jackrabbit Founders LLC. All rights reserved.
 *       
 * @date        Jul 2024
 * @author      Barrett Edwards <code@jrlabs.io>
 *
 * Macro / Enumeration Prefixes (LM)
 * PL - Memory Online Policies 
 * ST - State options 
 * ZN - Valid Zones bitfield enum
 * ZM - Valid Zones bitfield masks 
 */
#ifndef _LIBMEM_H
#define _LIBMEM_H

/* INCLUDES ==================================================================*/

/* MACROS ====================================================================*/

#define mem_blk_foreach(ctx, blk)  	for (blk = mem_blk_get_first(ctx); blk != NULL; blk = mem_blk_get_next(blk))

/* ENUMERATIONS ==============================================================*/

/**
 * Enumereation represeting log destinations
 */
enum LMLD
{
	LMLD_STDIO  = 0, 
	LMLD_SYSLOG = 1, 
	LMLD_NULL 	= 2,
	LMLD_FILE 	= 3, 
	LMLD_MAX
};

/* Auto Online Policy Options */
enum LMPL
{
	LMPL_OFFLINE	= 0,
	LMPL_ONLINE		= 1,
	LMPL_KERNEL 	= 2,
	LMPL_MOVABLE 	= 3,
	LMPL_MAX
};

/* State options */
enum LMST 
{
	LMST_OFFLINE 		= 0,
	LMST_ONLINE 		= 1,
	LMST_GOING_OFFLINE 	= 2,
	LMST_MAX
};

/* Valid Zone options  */
enum LMZN
{
	LMZN_DMA 			= 0, 
	LMZN_DMA32 			= 1, 
	LMZN_NORMAL 		= 2,
	LMZN_MOVABLE 		= 3, 
	LMZN_NONE 			= 4,
	LMZN_MAX
};

/* Bitfield masks for valid_zones */
#define LMZM_DMA    	(0x01)
#define LMZM_DMA32  	(0x02)
#define LMZM_NORMAL 	(0x04)
#define LMZM_MOVABLE 	(0x08)
#define LMZM_NONE   	(0x10)

/* STRUCTS ===================================================================*/

struct mem_ctx;
struct mem_blk;

/* 
 * Typedef for mem_set_log_fn()
 */
typedef void (*mem_log_fn)(struct mem_ctx *ctx, int priority, const char *fn, int ln, const char *format, va_list args);

/* GLOBAL VARIABLES ==========================================================*/

/* PROTOTYPES ================================================================*/

/* Library Instantiation */
int                  mem_new(struct mem_ctx **ctx);
struct mem_ctx *     mem_ref(struct mem_ctx *ctx);
int                  mem_unref(struct mem_ctx *ctx);

/* Library Log Configuration */
int	                 mem_log_get_priority(struct mem_ctx *ctx);
void                 mem_log_set_destination(struct mem_ctx *ctx, int dst, char *file);
void                 mem_log_set_fn(struct mem_ctx *ctx, 
                                    void (*mem_log_fn)(struct mem_ctx *ctx, 
                                                       int priority,
                                                       const char *fn,
                                                       int line, 
                                                       const char *format, 
                                                       va_list args));
void                 mem_log_set_priority(struct mem_ctx *ctx, int priority);

/* Library Collections API - Get */
struct cxl_region *  mem_get_region(struct mem_ctx *ctx, char *name);
struct cxl_memdev *  mem_get_memdev(struct mem_ctx *ctx, char *name);
struct cxl_memdev ** mem_get_memdevs(struct mem_ctx *ctx);
struct cxl_region ** mem_get_regions(struct mem_ctx *ctx);
struct cxl_decoder * mem_get_root_decoder(struct mem_ctx *ctx);
int                  mem_num_memdevs(struct mem_ctx* ctx);
int                  mem_num_regions(struct mem_ctx* ctx);

/* Memory System API - Get */
int *                mem_system_get_blocks(struct mem_ctx *ctx);
unsigned long long   mem_system_get_blocksize(struct mem_ctx *ctx);
unsigned long long   mem_system_get_capacity(struct mem_ctx *ctx);
unsigned long long   mem_system_get_capacity_offline(struct mem_ctx *ctx);
unsigned long long   mem_system_get_capacity_online(struct mem_ctx *ctx);
int                  mem_system_get_policy(struct mem_ctx *ctx);
int                  mem_system_num_blocks(struct mem_ctx *ctx);
int                  mem_system_num_blocks_online(struct mem_ctx *ctx);
int                  mem_system_num_blocks_offline(struct mem_ctx *ctx);

/* Memory System API - Actions */
int                  mem_system_set_policy(struct mem_ctx *ctx, int mode);

/* Memory Block API - Enumeration */
struct mem_blk *     mem_blk_get_first(struct mem_ctx *ctx);
struct mem_blk *     mem_blk_get_next(struct mem_blk *blk);

/* Memory Block API - Get */
int                  mem_blk_get_device(struct mem_blk *blk);
int                  mem_blk_get_id(struct mem_blk *blk);
int                  mem_blk_get_node(struct mem_blk *blk);
struct cxl_region *  mem_blk_get_region(struct mem_blk *blk);	
int                  mem_blk_get_state(struct mem_blk *blk);
unsigned long        mem_blk_get_zones(struct mem_blk *blk);
int                  mem_blk_is_online(struct mem_blk *blk);
int                  mem_blk_is_removable(struct mem_blk *blk);

/* Memory Block API - Actions  */
int                  mem_blk_offline(struct mem_blk *blk);
int                  mem_blk_online(struct mem_blk *blk);
int                  mem_blk_set_state(struct mem_blk *blk, int state);

/* Memory BlockID API - Get  */
struct mem_blk *     mem_blkid_get_blk(struct mem_ctx *ctx, int id);
int                  mem_blkid_get_device(struct mem_ctx *ctx, int id);
int                  mem_blkid_get_node(struct mem_ctx *ctx, int id);
int                  mem_blkid_get_state(struct mem_ctx *ctx, int id);
unsigned long        mem_blkid_get_zones(struct mem_ctx *ctx, int id);
int                  mem_blkid_is_online(struct mem_ctx *ctx, int id);
int                  mem_blkid_is_removable(struct mem_ctx *ctx, int id);

/* Memory BlockID API - Actions */
int                  mem_blkid_offline(struct mem_ctx *ctx, int index);
int                  mem_blkid_online(struct mem_ctx *ctx, int index);
int                  mem_blkid_set_state(struct mem_ctx *ctx, int index, int state);

/* Memory Memdev API - Get */
int                  mem_memdev_get_interleave_granularity(struct mem_ctx *ctx, struct cxl_memdev *memdev);
int                  mem_memdev_is_available(struct mem_ctx *ctx, struct cxl_memdev *memdev);

/* Memory Region API - Get */
int                  mem_region_get_blk_state(struct mem_ctx *ctx, struct cxl_region *region, int offset);
int *                mem_region_get_blocks(struct mem_ctx *ctx, struct cxl_region *region);
unsigned long long   mem_region_get_capacity(struct mem_ctx *ctx, struct cxl_region *region);
unsigned long long   mem_region_get_capacity_offline(struct mem_ctx *ctx, struct cxl_region *region);
unsigned long long   mem_region_get_capacity_online(struct mem_ctx *ctx, struct cxl_region *region);
int                  mem_region_is_daxmode(struct mem_ctx *ctx, struct cxl_region* region);
int                  mem_region_is_rammode(struct mem_ctx *ctx, struct cxl_region* region);
int                  mem_region_num_blocks(struct mem_ctx *ctx, struct cxl_region *region);
int                  mem_region_num_blocks_offline(struct mem_ctx *ctx, struct cxl_region *region);
int                  mem_region_num_blocks_online(struct mem_ctx *ctx, struct cxl_region *region);

/* Memory Region API - Actions */
int                  mem_region_create(struct mem_ctx *ctx, int granularity, int num, struct cxl_memdev **memdevs);
int                  mem_region_delete(struct mem_ctx *ctx, struct cxl_region *region);

int                  mem_region_offline_blocks(struct mem_ctx *ctx, struct cxl_region *region);
int                  mem_region_online_blocks(struct mem_ctx *ctx, struct cxl_region *region);
int                  mem_region_set_blk_state(struct mem_ctx *ctx, struct cxl_region *region, int offset, int mode);

int                  mem_region_daxmode(struct mem_ctx *ctx, struct cxl_region *region);
int                  mem_region_rammode(struct mem_ctx *ctx, struct cxl_region *region);

/* Compare functions for qsort */
int                  mem_compare_ints(const void* a, const void* b);
int                  mem_compare_cxl_memdevs(const void* a, const void* b);
int                  mem_compare_cxl_regions(const void* a, const void* b);

/* Print Functions */
void                 mem_blk_print(struct mem_blk *blk);

/* String representations of enumerations */
const char *         mem_lmpl(int policy);
const char *         mem_lmst(int state);
const char *         mem_lmzn(int zone);

/* Convert strings into enum values */
int                  mem_to_lmpl(char *policy);
int                  mem_to_lmst(char *state);
int                  mem_to_lmzn(char *zone);

#endif //ifndef _LIBMEM_H

