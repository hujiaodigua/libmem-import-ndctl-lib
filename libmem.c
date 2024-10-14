/**
 * @file 		libmem.c
 *
 * @brief 		Code file for memory management library
 *
 * @copyright   Copyright (C) 2024 Jackrabbit Founders LLC. All rights reserved.
 *       
 * @date        Jul 2024
 * @author      Barrett Edwards <code@jrlabs.io>
 */

/* INCLUDES ==================================================================*/

/* printf()
 */
#include <stdio.h>

/* calloc() 
 * free()
 */
#include <stdlib.h>

/* memset()
 * memcpy()
 */
#include <string.h>

/* open()
 */
#include <fcntl.h>

/* close()
 * unlink()
 */
#include <unistd.h>

/* errno
 */
#include <errno.h>

/* opendir()
 */
#include <dirent.h>

/* LOG_* Macros 
 */
#include <syslog.h>

/* cxl objects 
 * cxl_new()
 */
#include <cxl/libcxl.h>

/* struct daxctl_region
 */
#include <daxctl/libdaxctl.h>

// #include <cxltoyaml.h>

/* log_init()
 * log_free()
 * log_dbg()
 * log_info()
 * log_err();
 */
#include "log.h"

#include "libmem.h"

/* MACROS ====================================================================*/

#define LMLN_SYSFS_ATTR_SIZE 			1024
#define LMLN_FILEPATH 					1024
#define LMFP_MEM_DIR    				"/sys/devices/system/memory"

/* ENUMERATIONS ==============================================================*/

/* STRUCTS ===================================================================*/

/**
 * Object representing a kernel memory block 
 */
struct mem_blk
{
	int id; 
	int node;
	int online;
	int device;
	int removable;
	int state;
	unsigned long int valid_zones;
	struct mem_ctx *ctx;
};

/**
 * Memory library context
 */
struct mem_ctx
{
	struct log_ctx *log;  // Must be first for mem_set_log_fn
	int refcount;
	int num;
	int num_regions;
	struct mem_blk *blocks;
	struct cxl_ctx *cxl;
	struct cxl_region **regions;
};

/* GLOBAL VARIABLES ==========================================================*/

/**
 * String representtaion of enum _LMPL
 */
const char *_LMPL[] = 
{
	"offline", 
	"online",
	"online_kernel",
	"online_movable"
};

/**
 * String representation of enum _LMST
 */
const char *_LMST[] = 
{
	"offline", 
	"online",
	"going-offline",
};

/**
 * String representation of enum _LMZN
 */
const char *_LMZN[] = 
{
	"DMA",
	"DMA32",
	"Normal",
	"Movable",
	"none",
};

/* PROTOTYPES ================================================================*/

// Static methods for sysfs read / write 
static int mem_sysfs_read(struct mem_ctx *ctx, const char *path, char *buf);
static int mem_sysfs_write(struct mem_ctx *ctx, const char *path, const char *buf);

// Compare functions for qsort
int mem_compare_cxl_memdevs(const void* a, const void* b);
int mem_compare_cxl_regions(const void* a, const void* b);
int mem_compare_ints(const void* a, const void* b);
int mem_compare_mem_blks(const void* a, const void* b);

static int mem_blk_init(struct mem_ctx *ctx);

/* FUNCTIONS =================================================================*/

int mem_blk_get_device(struct mem_blk *blk)
{
	return blk->device;
}

/**
 * Get the first memory block in the system 
 */
struct mem_blk *mem_blk_get_first(struct mem_ctx *ctx)
{
	int rv;
	struct mem_blk *blk;

	blk = NULL;

	if (ctx->blocks == NULL)
	{
		rv = mem_blk_init(ctx);
		if (rv != 0)
		{
			err(ctx, "mem_blk_init() failed: %d", rv);
			goto end;
		}
	}

	if (ctx->num > 0)
		blk = &ctx->blocks[0];

end:

	return blk; 
}

int mem_blk_get_id(struct mem_blk *blk)
{
	return blk->id;
}

/**
 * Get the next memory block in the system 
 */
struct mem_blk *mem_blk_get_next(struct mem_blk *blk)
{
	int i;
	struct mem_ctx *ctx;
	struct mem_blk *next; 

	ctx = blk->ctx;
	next = NULL; 

	for ( i = 0 ; i < ctx->num ; i++ )
		if (blk->id == ctx->blocks[i].id)
			break;
	i++; 

	if (i < ctx->num)
		next = &ctx->blocks[i];

	return next; 
}

int mem_blk_get_node(struct mem_blk *blk)
{
	return blk->node;
}

struct cxl_region *mem_blk_get_region(struct mem_blk *blk)
{
	int num;
	struct cxl_region *region, **regions;
	struct mem_ctx *ctx;
	unsigned long long block_size, base, size, end, addr;

	ctx = blk->ctx;
	region = NULL;
	num = mem_num_regions(ctx);

	// If there are no regions then return null
	if (num == 0)
		goto end;

	// Get memory block size in bytes 
	block_size = mem_system_get_blocksize(ctx);
	if (block_size == 0)
	{
		err(ctx, "Unable to read system memory block size");
		goto end;
	}
	addr = block_size * blk->id;			

	regions = mem_get_regions(ctx);	
	if (regions == NULL)
	{
		err(ctx, "Could not obtain regions");
		goto end;	
	}

	for ( int i = 0 ; i < num ; i++)
	{
		region = regions[i];

		// Get region base address 
		base = cxl_region_get_resource(region);
		if (base == 0 || base == 0xFFFFFFFFFFFFFFFF)
		{
			num = 0;
			err(ctx, "Unable to get cxl region %s resource address", cxl_region_get_devname(region));
			goto end;
		}

		// Get region size in bytes 
		size = cxl_region_get_size(region);
		if (size == 0)
		{
			num = 0;
			warn(ctx, "Region size was zero for region %s", cxl_region_get_devname(region));
			goto end;
		}
		end = base + size;

		if (base <= addr && addr < end )
			goto end;
	}

	region = NULL;

end:

	return region;
}

int mem_blk_get_state(struct mem_blk *blk)
{
	if (blk->state == LMST_OFFLINE)
		return LMPL_OFFLINE;

	else if (blk->valid_zones & LMZM_DMA)
		return LMPL_KERNEL;
	
	else if (blk->valid_zones & LMZM_DMA32)
		return LMPL_KERNEL;
	
	else if (blk->valid_zones & LMZM_NORMAL)
		return LMPL_ONLINE;
	
	else if (blk->valid_zones & LMZM_MOVABLE)
		return LMPL_MOVABLE;
	
	else 	
		return LMPL_ONLINE;
}

unsigned long mem_blk_get_zones(struct mem_blk *blk)
{
	return blk->valid_zones;
}

static int mem_blk_init(struct mem_ctx *ctx)
{
	int rv, i, num, index;
	DIR *d;
  	struct dirent *e; 
	struct mem_blk *mb;
	char path[LMLN_FILEPATH];
	char buf[LMLN_FILEPATH];

	// Initialize variables 
	rv = 1;
	num = 0;
	i = 0;

	// Validate inputs 
	// Skip if the blocks array has already been initialized 
	if (ctx->num > 0)
	{
		rv = 0;
		goto end;
	}

	// 1: Count the number of memory directories

	// Open the directory 
	d = opendir(LMFP_MEM_DIR);	
	if (d == NULL)
	{
		err(ctx, "Could not open memory directory for enumeration: %s", LMFP_MEM_DIR);
		goto end;
	}

	// Walk the directory tree and get the block index numbers 
	for (e = readdir(d) ; e != NULL ; e = readdir(d))
		if (e->d_type == DT_DIR && sscanf(e->d_name, "memory%d", &index) == 1)
			num++;

	// Allocate array for mem_blks 
	ctx->blocks = calloc(num, sizeof(struct mem_blk));
	ctx->num = num;

	info(ctx, "Found %d Memory Blocks", num);

	// Populate the mem_blk array 
	rewinddir(d);

	// Walk the directory tree and get the block index numbers 
	for (e = readdir(d) ; e != NULL ; e = readdir(d))
		if (e->d_type == DT_DIR && sscanf(e->d_name, "memory%d", &index) == 1)
		{
			mb = &ctx->blocks[i++];
			mb->ctx = ctx;	
			mb->id = index;

			{
				// Open memory directory to search for node link 
				DIR *d2;	
  				struct dirent *e2; 
				mb->node = -1;
				sprintf(path, "%s/%s", LMFP_MEM_DIR, e->d_name);							
				d2 = opendir(path);
				if (d2 != NULL)
					for (e2 = readdir(d2) ; e2 != NULL ; e2 = readdir(d2))
						if (e2->d_type == DT_LNK && !strncmp("node", e2->d_name, 4))
							sscanf(e2->d_name, "node%d", &mb->node);
			}

			sprintf(path, "%s/%s/online", LMFP_MEM_DIR, e->d_name);
			if (mem_sysfs_read(ctx, path, buf) > 0)
				mb->online = strtoul(buf, NULL, 0);
				
			sprintf(path, "%s/%s/phys_device", LMFP_MEM_DIR, e->d_name);
			if (mem_sysfs_read(ctx, path, buf) > 0)
				mb->device = strtoul(buf, NULL, 0);
				
			sprintf(path, "%s/%s/removable", LMFP_MEM_DIR, e->d_name);
			if (mem_sysfs_read(ctx, path, buf) > 0)
				mb->removable = strtoul(buf, NULL, 0);

			sprintf(path, "%s/%s/state", LMFP_MEM_DIR, e->d_name);
			if (mem_sysfs_read(ctx, path, buf) > 0)
				for ( int j = 0 ; j < LMZN_MAX ; j++)
					if (!strcmp(buf, mem_lmst(j)))
					{
						mb->state = j;
						break;
					}

			sprintf(path, "%s/%s/valid_zones", LMFP_MEM_DIR, e->d_name);
			if (mem_sysfs_read(ctx, path, buf) > 0)
    		{
				char *state = NULL;
				mb->valid_zones = 0;
				for (char *t = strtok_r(buf, " ", &state); t ; t = strtok_r(NULL, " ", &state))
   					for ( int j = 0 ; j < LMZN_MAX ; j++ )
   						if (!strcmp(t, mem_lmzn(j)))
   							mb->valid_zones |= (0x01 << j);
			}
		}

	closedir(d);

	// Sort the array
	qsort(ctx->blocks, ctx->num, sizeof(struct mem_blk), mem_compare_mem_blks);

	rv = 0;

end:

	return rv;
}

int mem_blk_is_online(struct mem_blk *blk)
{
	return blk->online;
}

int mem_blk_is_removable(struct mem_blk *blk)
{
	return blk->removable;
}

/**
 * Offline a memory block
 */
int mem_blk_offline(struct mem_blk *blk)
{
	int rv, ret, index, state;
	DIR *d;
	char path[LMLN_FILEPATH];
  	struct dirent *e; 
	struct mem_ctx *ctx;

	// Initialize variables 
	rv = 0;
	ctx = blk->ctx;

    // Open the directory 
	d = opendir(LMFP_MEM_DIR);	
	if (d == NULL)
	{
		err(ctx, "Could not open memory directory for enumeration: %s", LMFP_MEM_DIR);
		rv = 1;
		goto end;
	}

	// Loop through all the directory entries and online the memory block that matches
	for (e = readdir(d) ; e != NULL ; e = readdir(d))
		if (e->d_type == DT_DIR && sscanf(e->d_name, "memory%d", &index) == 1 && index == blk->id)
		{
			state = mem_blk_get_state(blk);
			info(ctx, "Found memory block %d. Current State: %d Desired State %d", index, state, LMPL_OFFLINE);

			if ( state == LMPL_OFFLINE )
			{
				info(ctx, "Memory block %d already offline. Skipping", index, mem_lmpl(state));
				rv = 0;
				goto end;
			}

			sprintf(path, "%s/%s/%s", LMFP_MEM_DIR, e->d_name, "online");							
			ret = mem_sysfs_write(ctx, path, "0");
			if (ret != 2)
			{
				err(ctx, "Failed to offline memory block %d", index);
				rv += 1;
			}
			else 
				info(ctx, "Offlined memory block %d", index);
		}
end:

	return rv; 
}

int mem_blk_online(struct mem_blk *blk)
{
	int rv, ret, index, state;
	DIR *d;
	char path[LMLN_FILEPATH];
  	struct dirent *e; 
	struct mem_ctx *ctx;

	// Initialize variables 
	rv = 0;
	ctx = blk->ctx;

	// Open the directory 
	d = opendir(LMFP_MEM_DIR);	
	if (d == NULL)
	{
		err(ctx, "Could not open memory directory for enumeration: %s", LMFP_MEM_DIR);
		rv = 1;
		goto end;
	}

	// Loop through all the directory entries and online the memory block that matches
	for (e = readdir(d) ; e != NULL ; e = readdir(d))
		if (e->d_type == DT_DIR && sscanf(e->d_name, "memory%d", &index) == 1 && index == blk->id)
		{
			state = mem_blk_get_state(blk);
			info(ctx, "Found memory block %d. Current State: %d Desired State %d", index, state, LMPL_MOVABLE);

			if (state == LMPL_MOVABLE)
			{
				info(ctx, "Memory block %d already in state %s. Skipping", index, mem_lmpl(state));
				rv = 0;
				goto end;
			}

			if (state != LMPL_OFFLINE)
			{
				err(ctx, "Failed to online Memory block %d becuase it is not offline: %s", index, mem_lmpl(mem_blk_get_state(blk)));
				rv = 1;
				goto end;
			}

			sprintf(path, "%s/%s/%s", LMFP_MEM_DIR, e->d_name, "state");							
			ret = mem_sysfs_write(ctx, path, "online_movable");
			if (ret != 15)
			{
				err(ctx, "Failed to online memory block %d", index);
				rv += 1;
			}
			else 
				info(ctx, "Onlined memory block %d", index);
		}
end:

	return rv; 
}

/**
 * Print out the values of a struct mem_blk
 */
void mem_blk_print(struct mem_blk *blk)
{
	printf("id        %d\n", blk->id); 
	printf("node      %d\n", blk->node);
	printf("online    %d\n", blk->online);
	printf("device    %d\n", blk->device);
	printf("removable %d\n", blk->removable);
	printf("state     %d - %s\n", blk->state, mem_lmst(blk->state));
	for ( int i = 0 ; i < LMZN_MAX ; i++ )
	{
		if ((0x01 << i) & blk->valid_zones)
			printf("%s ", mem_lmzn(i));
	}
	printf("\n");
}

int mem_blk_set_state(struct mem_blk *blk, int state)
{
	int rv, ret, index;
	DIR *d;
	char path[LMLN_FILEPATH];
  	struct dirent *e; 
	struct mem_ctx *ctx;

	// Initialize variables 
	rv = 0;
	ctx = blk->ctx;

	// Validate inputs 
	if (state < 0 || state >= LMPL_MAX)
	{
		err(ctx, "Attempted to set invalid state: %d", state);
		rv = 1;
		goto end;
	}

	// Open the directory 
	d = opendir(LMFP_MEM_DIR);	
	if (d == NULL)
	{
		err(ctx, "Could not open memory directory for enumeration: %s", LMFP_MEM_DIR);
		rv = 1;
		goto end;
	}

	// Loop through all the directory entries and online the memory block that matches
	for (e = readdir(d) ; e != NULL ; e = readdir(d))
		if (e->d_type == DT_DIR && sscanf(e->d_name, "memory%d", &index) == 1 && index == blk->id)
		{
			info(ctx, "Found memory block %d. Current State: %d Desired State %d", index, mem_blk_get_state(blk), state);
			if (mem_blk_get_state(blk) == state)
			{
				info(ctx, "Memory block %d already in state %s. Skipping", index, mem_lmpl(state));
				rv = 0;
				goto end;
			}

			if (state != LMPL_OFFLINE && mem_blk_get_state(blk) != LMPL_OFFLINE)
			{
				err(ctx, "Failed to set state of Memory block %d to %s becuase it is not offline: %s", index, mem_lmpl(state), mem_lmpl(mem_blk_get_state(blk)));
				rv = 1;
				goto end;
			}

			sprintf(path, "%s/%s/%s", LMFP_MEM_DIR, e->d_name, "state");							
			ret = mem_sysfs_write(ctx, path, mem_lmpl(state));
			if (ret < 0 || ret != (int) (strlen(mem_lmpl(state)) + 1))
			{
				err(ctx, "Failed to set state to %s on memory block %d. %d", mem_lmpl(state), index, ret);
				rv += 1;
			}
			else 
				info(ctx, "Set state to %s on memory block %d", mem_lmpl(state), index);
		}
end:

	return rv; 
}

/** 
 * Get a struct mem_blk* from a memory block ID 
 */ 
struct mem_blk *mem_blkid_get_blk(struct mem_ctx *ctx, int id)
{
	struct mem_blk *blk; 

	// Validate Inputs 
	if (ctx == NULL || id < 0)
		return NULL;

	mem_blk_foreach(ctx, blk)
		if (blk->id == id)
			return blk;

	return NULL;
}

/**
 * Get the physical device for the memory block 
 */
int mem_blkid_get_device(struct mem_ctx *ctx, int id)
{
  	struct mem_blk *blk;

	mem_blk_foreach(ctx, blk)
		if (id == blk->id)
			return blk->device;

	return -1;
}

/**
 * Get the numa node for the memory blocks in the system
 * @return The number of blocks. 0 if error
 */
int mem_blkid_get_node(struct mem_ctx *ctx, int index)
{
	struct mem_blk *blk;

	mem_blk_foreach(ctx, blk)
		if (blk->id == index)
			return blk->node;

	return -1;
}
 
/**
 * Get the onnline state of a memory block
 */
int mem_blkid_get_state(struct mem_ctx *ctx, int id)
{
  	struct mem_blk *blk;

	mem_blk_foreach(ctx, blk)
		if (id == blk->id)
			return blk->state;

	return -1;
}

/**
 * Get a bitfield representing the valid zones*
 */
unsigned long mem_blkid_get_zones(struct mem_ctx *ctx, int id)
{
  	struct mem_blk *blk;

	mem_blk_foreach(ctx, blk)
		if (id == blk->id)
			return blk->valid_zones;

	return 0;
}

/**
 * Determine if a memory block is online by phys_index
 */
int mem_blkid_is_online(struct mem_ctx *ctx, int id)
{
  	struct mem_blk *blk;

	mem_blk_foreach(ctx, blk)
		if (id == blk->id)
			return blk->online;

	return -1;
}

/**
 * Determine if a memory block is removable by phys_index
 */
int mem_blkid_is_removable(struct mem_ctx *ctx, int id)
{
  	struct mem_blk *blk;

	mem_blk_foreach(ctx, blk)
		if (id == blk->id)
			return blk->removable;

	return -1;
}

/**
 * Offline a memory block by phys_index
 */ 
int mem_blkid_offline(struct mem_ctx *ctx, int id)
{
  	struct mem_blk *blk;

	mem_blk_foreach(ctx, blk)
		if (id == blk->id)
			return mem_blk_offline(blk);

	return 1;
}

/**
 * Online a memory block by phys_index
 * @return 0 upon success, non-zero othersise
 */
int mem_blkid_online(struct mem_ctx *ctx, int id)
{
  	struct mem_blk *blk;

	mem_blk_foreach(ctx, blk)
		if (id == blk->id)
			return mem_blk_online(blk);

	return -1;
}

/**
 * Set the online state of the memory block 
 */
int mem_blkid_set_state(struct mem_ctx *ctx, int id, int state)
{
  	struct mem_blk *blk;

	mem_blk_foreach(ctx, blk)
		if (id == blk->id)
			return mem_blk_set_state(blk, state);

	return -1;
}

/**
 * Compare cxl_memdev function for qsort
 */ 
int mem_compare_cxl_memdevs(const void* a, const void* b)
{
    struct cxl_memdev *arg1 = *(struct cxl_memdev **)a;
    struct cxl_memdev *arg2 = *(struct cxl_memdev **)b;

 	int i1 = cxl_memdev_get_id(arg1);
 	int i2 = cxl_memdev_get_id(arg2);

 	return mem_compare_ints(&i1, &i2);
}

/**
 * Compare cxl_region function for qsort
 */ 
int mem_compare_cxl_regions(const void* a, const void* b)
{
    struct cxl_region *arg1 = *(struct cxl_region **)a;
    struct cxl_region *arg2 = *(struct cxl_region **)b;

 	int i1 = cxl_region_get_id(arg1);
 	int i2 = cxl_region_get_id(arg2);

 	return mem_compare_ints(&i1, &i2);
}

/**
 * Compare int function for qsort
 */ 
int mem_compare_ints(const void* a, const void* b)
{
    int arg1 = *(const int*)a;
    int arg2 = *(const int*)b;
 
    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}

/**
 * Compare mem_blk function for qsort
 */ 
int mem_compare_mem_blks(const void* a, const void* b)
{
    struct mem_blk *arg1 = (struct mem_blk *)a;
    struct mem_blk *arg2 = (struct mem_blk *)b;

 	int i1 = arg1->id;
 	int i2 = arg2->id;

 	return mem_compare_ints(&i1, &i2);
}

/**
 * Search for and return a cxl_memdev object matching name 
 * @return struct cxl_memdev *. NULL if error. 
 */
struct cxl_memdev *mem_get_memdev(struct mem_ctx *ctx, char *name)
{
	struct cxl_memdev *memdev; 

	cxl_memdev_foreach(ctx->cxl, memdev)  
		if (!strcmp(cxl_memdev_get_devname(memdev), name))
			return memdev;
	
	return NULL;
}

/**
 * Return an array of pointers to cxl_memdevs
 */
struct cxl_memdev **mem_get_memdevs(struct mem_ctx *ctx)
{
	int i, num;
	struct cxl_memdev *memdev;
	struct cxl_memdev **array;

	// Initialize variables 
	i = 0;
	num = mem_num_memdevs(ctx);
	if (num == 0)
		return NULL;
	
	// Allocate memory 
	array = calloc(num, sizeof(*array));
	
	// Populate the array with the memdev pointers 
	cxl_memdev_foreach(ctx->cxl, memdev)  
		array[i++] = memdev;

	// Sort the array
	qsort(array, num, sizeof(*array), mem_compare_cxl_memdevs);
	
	return array;
}

/**
 * Search for and return a cxl_region object matching name
 * @return struct cxl_region* or NULL if not found
 */
struct cxl_region *mem_get_region(struct mem_ctx *ctx, char * name)
{
	struct cxl_bus *bus;
	struct cxl_decoder *decoder;
	struct cxl_region *region;
	
	cxl_bus_foreach(ctx->cxl, bus)
		cxl_decoder_foreach(cxl_bus_get_port(bus), decoder)
			cxl_region_foreach(decoder, region)
				if (!strcmp(name, cxl_region_get_devname(region)))
					goto end;

	region = NULL;

end:

	return region;
}

/**
 * Return an array of pointers to cxl_regions 
 */
struct cxl_region **mem_get_regions(struct mem_ctx *ctx)
{
	struct cxl_bus *bus;
	struct cxl_decoder *decoder;
	struct cxl_region *region;
	struct cxl_region **array;
	int i, num;

	if (ctx->regions != NULL)
		return ctx->regions;

	// Initialize variables 
	i = 0;
	num = mem_num_regions(ctx);
	if (num == 0)
		return NULL;
	
	// Allocate memory 
	array = calloc(num, sizeof(*array));
	
	// Loop through CXL tree and get regions 
	cxl_bus_foreach(ctx->cxl, bus)
		cxl_decoder_foreach(cxl_bus_get_port(bus), decoder)
			cxl_region_foreach(decoder, region)
				array[i++] = region;
	
	// Sort the array
	qsort(array, num, sizeof(*array), mem_compare_cxl_regions);

	ctx->num_regions = num;
	ctx->regions = array;

	return array;	
}

/**
 * Search for and return root cxl_decoder object 
 * @return struct cxl_decoder* or NULL if error
 */
struct cxl_decoder *mem_get_root_decoder(struct mem_ctx *ctx)
{
	struct cxl_bus *bus;
	struct cxl_port *port;
	struct cxl_decoder *decoder;

	decoder = NULL;

	bus = cxl_bus_get_first(ctx->cxl);
	if (bus == NULL)
	{
		err(ctx, "Unable to obtain first cxl bus");
		goto end;
	}

	port = cxl_bus_get_port(bus);
	if (port == NULL)
	{
		err(ctx, "Unable to obtain first cxl bus port on bus %s", cxl_bus_get_devname(bus));
		goto end;
	}

	decoder = cxl_decoder_get_first(port);	

end:

	return decoder;
}

/**
 * Return a const char * (string) representation of enum LMPL
 */
const char *mem_lmpl(int policy)
{
	if (policy < 0 || policy >= LMPL_MAX)
		return NULL;
	return _LMPL[policy];
}

/**
 * Return a const char * (string) representation of enum LMST
 */
const char *mem_lmst(int state)
{
	if (state < 0 || state >= LMST_MAX)
		return NULL;
	return _LMST[state];
}

/**
 * Return a const char * (string) representation of enum LMZN
 */
const char *mem_lmzn(int zone)
{
	if (zone < 0 || zone >= LMZN_MAX)
		return NULL;
	return _LMZN[zone];
}

/**
 * Return current library log priority level 
 */
int	mem_log_get_priority(struct mem_ctx *ctx)
{
	return ctx->log->priority;
}

/**
 * Set the log library destination to use
 * @param dst 	int from enum LMLD 
 */
void mem_log_set_destination(struct mem_ctx *ctx, int dst, char *filepath)
{
	if (dst == LMLD_SYSLOG)
		ctx->log->log_fn = log_to_syslog;
	else if (dst == LDST_NULL)
		ctx->log->log_fn = log_to_null;
	else if (dst == LDST_FILE & filepath != NULL)
	{
		ctx->log->file = fopen(filepath, "a");
		ctx->log->log_fn = log_to_file;
	}
	else 
		ctx->log->log_fn = log_to_stdio;
	
	info(ctx, "Set log destination to %d %s", dst, log_dst_to_str(dst));
}

/**
 * Set logging function 
 */
void mem_log_set_fn(struct mem_ctx *ctx,
		void (*mem_log_fn)(	struct mem_ctx *ctx, 
							int priority,
							const char *fn,
							int line, 
							const char *format, 
							va_list args))
{
	ctx->log->log_fn = (log_fn) mem_log_fn;
	info(ctx, "custom logging function %p registered\n", mem_log_fn);
}

/**
 * Set library log priority level 
 */
void mem_log_set_priority(struct mem_ctx *ctx, int priority)
{
	if (priority > LOG_DEBUG)
		priority = LOG_DEBUG;
	if (priority < 0 )
		priority = 0;	

	ctx->log->priority = priority;
	info(ctx, "logging priority set to %d - %s\n", priority, log_priority_to_str(priority));
}

/**
 * Get the Interleave granulariy presented by first port of the bus 
 */
int mem_memdev_get_interleave_granularity(struct mem_ctx *ctx, struct cxl_memdev *memdev)
{
	unsigned int rv; 
	struct cxl_bus *bus;
	struct cxl_port *parent, *port; 
	struct cxl_decoder *decoder;

	rv = 0;

	bus = cxl_memdev_get_bus(memdev);
	if (bus == NULL)
	{
		rv = 0;
		err(ctx, "Unable to obtain cxl_bus for memdev %s: %d", cxl_memdev_get_devname(memdev), rv);
		goto end;
	}

	parent = cxl_bus_get_port(bus);
	if (parent == NULL)
	{
		rv = 0;
		err(ctx, "Unable to obtain cxl_port for bus %s: %d", cxl_bus_get_devname(bus), rv);
		goto end;
	}

	port = cxl_port_get_first(parent);
	if (port == NULL)
	{
		rv = 0;
		err(ctx, "Unable to obtain first cxl_port for parent %s: %d", cxl_port_get_devname(parent), rv);
		goto end;
	}

	decoder = cxl_decoder_get_first(port);
	if (decoder == NULL)
	{
		rv = 0;
		err(ctx, "Unable to obtain decoder for cxl_port %s: %d", cxl_port_get_devname(port), rv);
		goto end;
	}

	rv = cxl_decoder_get_interleave_granularity(decoder);

end:

	return rv;
}

/**
 * Is a memdev free to be added to a new region 
 * @return 1 for true, 0 for false
 */
int mem_memdev_is_available(struct mem_ctx *ctx, struct cxl_memdev *memdev)
{
	int rv;
	struct cxl_endpoint *endpoint;
	struct cxl_port *port;
	struct cxl_decoder *decoder;
	struct cxl_region *region;

	rv = 0;

	// Check if memdev is even enabled 
	rv = cxl_memdev_is_enabled(memdev);
	if (rv == 0)
		goto end;

	// Get endpoint 
	endpoint = cxl_memdev_get_endpoint(memdev);
	if (endpoint == NULL)
	{
		rv = 0;
		err(ctx, "Unable to get cxl_endpoint for memdev %s", cxl_memdev_get_devname(memdev));
		goto end;
	}
	
	// Check if endpoint is enabled 
	rv = cxl_endpoint_is_enabled(endpoint);
	if (rv == 0)
		goto end;

	// Get the port 
	port = cxl_endpoint_get_port(endpoint);
	if (port == NULL)
	{
		rv = 0;
		err(ctx, "Unable to get cxl_port for cxl_endpoint %s", cxl_endpoint_get_devname(endpoint));
		goto end;
	}

	// Check if port is enabled 
	rv = cxl_port_is_enabled(port);
	if (rv == 0)
		goto end;

	// Get Decoder 
	decoder = cxl_decoder_get_first(port);
	if (decoder == NULL)
	{
		rv = 0;
		err(ctx, "Unable to get cxl_decoder for cxl_port %s", cxl_port_get_devname(port));
		goto end;
	}

	region = cxl_decoder_get_region(decoder);
	if (region == NULL)
		rv = 1;
	else 
		rv = 0;

end:

	return rv;
}

/**
 * Create a new lib mem context 
 */
int mem_new(struct mem_ctx **ctx)
{
	int rv; 
	struct mem_ctx *c; 

	rv = 1; 

	// Allocate memory for the object 
	c = calloc(1, sizeof(struct mem_ctx));
	if (c == NULL) 
		return -ENOMEM;

	c->refcount = 1;

	// Get a cxl context 
	rv = cxl_new(&c->cxl);
	if (rv != 0)
		goto end;

	// Set up logger 
	c->log = log_init("libmem", LDST_SYSLOG, LOG_ERR, 1, NULL);
	
	log_info(c->log, "mem_ctx created at: %p\n", c);
	log_dbg(c->log, "log_priority=%d\n", c->log->priority);
	
	// Store logger object into mem_ctx 
	*ctx = c;

	rv = 0;

end:

	return rv; 
}

/**
 * Count and return the number of CXL rmemdevs
 */
int mem_num_memdevs(struct mem_ctx* ctx)
{
	int num; 
	struct cxl_memdev *memdev; 

	num = 0;

	cxl_memdev_foreach(ctx->cxl, memdev)  
		num++;	

	return num;
}

/**
 * Count and return the number of CXL regions 
 */
int mem_num_regions(struct mem_ctx *ctx)
{
	int num; 
	struct cxl_bus *bus;
	struct cxl_decoder *decoder;
	struct cxl_region *region;

	if (ctx->regions != NULL)
		return ctx->num_regions;

	num = 0;

	// Loop through the cxl bus to count the number of cxl_regions
	cxl_bus_foreach(ctx->cxl, bus)
		cxl_decoder_foreach(cxl_bus_get_port(bus), decoder)
			cxl_region_foreach(decoder, region)
				num++;

	return num;
}

/**
 * mem_ref - Create an additional reference on the mem context
 * @param ctx struct mem_ctx context created by cxl_new()
 */
struct mem_ctx *mem_ref(struct mem_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;
	ctx->refcount++;
	return ctx;
}

/**
 * Create a region from a list of memory devices 
 */
int mem_region_create(struct mem_ctx *ctx, int granularity, int num, struct cxl_memdev **memdevs)
{
	int rv; 
	unsigned long long size, total_size;
	char name[256];
	struct cxl_decoder *root, *decoder;
	struct cxl_region *region;
	struct cxl_memdev *memdev;

	// Initialize variables
	rv = 1;
	total_size = 0;

	root = mem_get_root_decoder(ctx);
	if (root == NULL)
	{
		err(ctx, "Could not obtain root decoder");
		goto end;
	}

	region = cxl_decoder_create_ram_region(root);
	if (region == NULL)
	{
		err(ctx, "Could not create ram region");
		goto end;;
	}
	else
		info(ctx, "Created ram region %s", cxl_region_get_devname(region));

	cxl_region_set_interleave_ways(region, num);
	cxl_region_set_interleave_granularity(region, granularity);

	info(ctx, "Set interleave ways to %d on region %s", num, cxl_region_get_devname(region));
	info(ctx, "Set interleave granularity to %d on region %s", granularity, cxl_region_get_devname(region));

	// Loop through requested memory devices 
	for ( int i = 0 ; i < num ; i++ )
	{
		memdev = memdevs[i];
		if (memdev == NULL)
		{
			err(ctx, "Memdev poiner was NULL");
			goto delete_region;
		}

		size = cxl_memdev_get_ram_size(memdev);
		decoder = cxl_decoder_get_first(cxl_endpoint_get_port(cxl_memdev_get_endpoint(memdev)));

		rv = cxl_decoder_set_mode(decoder, CXL_DECODER_MODE_RAM);
		if (rv != 0)
		{
			err(ctx, "Attempt to set decoder mode failed: %d", rv);
			goto delete_region;
		}
		else 
			info(ctx, "Set decoder mode to %s on decoder %s", cxl_decoder_mode_name(CXL_DECODER_MODE_RAM), cxl_decoder_get_devname(decoder));

		rv = cxl_decoder_set_dpa_size(decoder, size);
		if (rv != 0)
		{
			err(ctx, "Attempt to set decoder dpa size failed: %d", rv);
			goto delete_region;
		}
		else 
			info(ctx, "Set decoder DPA size to %llu on decoder %s", size, cxl_decoder_get_devname(decoder));
		
		total_size += size;
	}

	// Set region total size 
	rv = cxl_region_set_size(region, total_size);
	if (rv != 0)
	{
		err(ctx, "Attempt to set region size failed: %d", rv);
		goto delete_region;
	}
	else 
		info(ctx, "Set region size to %llu on region %s", total_size, cxl_region_get_devname(region));

	// Loop through requested memory devices 
	for ( int i = 0 ; i < num ; i++ )
	{
		memdev = memdevs[i];
		if (memdev == NULL)
		{
			err(ctx, "Memdev poiner was NULL");
			goto delete_region;
		}

		decoder = cxl_decoder_get_first(cxl_endpoint_get_port(cxl_memdev_get_endpoint(memdev)));

		// Set region targets to decoders
		rv = cxl_region_set_target(region, i, decoder);
		if (rv != 0)
		{
			err(ctx, "Unable to set region target i: %d rv: %d", i, rv);
			goto delete_region;
		}
		else 
			info(ctx, "Set region target %d to %s on region %s", i, cxl_decoder_get_devname(decoder), cxl_region_get_devname(region));
	}

	// commit the region
	rv = cxl_region_decode_commit(region);
	if (rv != 0)
	{
		err(ctx, "Decode commit failed: %d", rv);
		goto delete_region;
	}
	else
		info(ctx, "Decode commit on region %s", cxl_region_get_devname(region));

	// bind the region 
	rv = cxl_region_enable(region);
	if (rv != 0)
	{
		err(ctx, "Failed to enable region: %d", rv);
		goto delete_region;
	}
	else 
		info(ctx, "Enabled region %s", cxl_region_get_devname(region));

	rv = 0;

	goto end;

delete_region:

	strcpy(name, cxl_region_get_devname(region));
	rv = cxl_region_delete(region);
	if (rv != 0)
	{
		err(ctx, "Failed to delete region %s rv: %d", name, rv);
		goto end;
	}
	else 
		err(ctx, "Deleted region %s", name);

	rv = 1;

end:

	return rv;
}

/**
 * Set a cxl_region to devdax mode 
 */
int mem_region_daxmode(struct mem_ctx *ctx, struct cxl_region *region)
{
	int rv; 
	struct daxctl_region *dax_region;
	struct daxctl_dev *dax_dev;
	struct daxctl_memory *dax_mem;

	rv = 1;

	// Get the dax region for the cxl_region
	dax_region = cxl_region_get_daxctl_region(region);
	if (dax_region == NULL)
	{
		err(ctx, "Failed to obtain dax_region for cxl region %s\n", cxl_region_get_devname(region));
		goto end;
	}

	// Get the dax device 
	dax_dev = daxctl_dev_get_first(dax_region);
	if (dax_dev == NULL)
	{
		err(ctx, "Failed to obtain dax_dev for dax_region %s\n", daxctl_region_get_devname(dax_region));
		goto end;
	}
	
	// Check if region is already in devdax mode 
	dax_mem = daxctl_dev_get_memory(dax_dev);
	if (dax_mem == NULL)
	{
		rv = 0;
		info(ctx, "dax_dev %s was already in devdax mode\n", daxctl_dev_get_devname(dax_dev));
		goto end;
	}

	if (cxl_region_is_enabled(region))
	{
		// Offline all the blocks for the cxl_region
		rv = mem_region_offline_blocks(ctx, region);
		if (rv != 0)
		{
			err(ctx, "Failed to offline all region blocks: %d", rv);
			goto end;
		}
		else 
			info(ctx, "Offlined all memory blocks of region %s", cxl_region_get_devname(region));
 
	}

	// Disable the dax device if enabled
	if (daxctl_dev_is_enabled(dax_dev))
	{
		rv = daxctl_dev_disable(dax_dev);
		if (rv != 0)
		{
			err(ctx, "Failed to disable dax_dev %s %d\n", daxctl_dev_get_devname(dax_dev), rv);
			goto end;
		}
		else
			info(ctx, "Disabled dax device %s", daxctl_dev_get_devname(dax_dev));
	}

	// Set region to devdax mode 
	rv = daxctl_dev_enable_devdax(dax_dev);
	if (rv != 0)
	{
		err(ctx, "Failed to enable dax mode on %s %d\n", daxctl_dev_get_devname(dax_dev), rv);
		goto end;
	}
	else
		info(ctx, "Enabled devdax mode on dax device %s", daxctl_dev_get_devname(dax_dev));

	rv = 0;

end:

	return rv;
}

/**
 * Delete a cxl_region 
 *
 * This function will attempt to offline all the memory blocks
 * before deleting the retion. If a memory block fails to offline
 * this command will fail 
 */
int mem_region_delete(struct mem_ctx *ctx, struct cxl_region *region)
{
	int rv;
	char buf[LMLN_FILEPATH];

	// Initialize variables 
	rv = 1;

	if (mem_region_num_blocks_online(ctx, region) > 0)
	{
		// Offline all the region blocks 
		rv = mem_region_offline_blocks(ctx, region);
		if (rv != 0)
		{
			err(ctx, "Failed to offline all region blocks: %d", rv);
			goto end;
		}
		else 
			info(ctx, "Offlined all memory blocks of region %s", cxl_region_get_devname(region));
	}
	
	// Diable the region 
	rv = cxl_region_disable(region);
	if (rv != 0)
	{
		err(ctx, "Failed to disable region: %d", rv);
		goto end;
	}
	else
		info(ctx, "Disabled region %s", cxl_region_get_devname(region));

	// Copy name for final debug print
	strncpy(buf, cxl_region_get_devname(region), LMLN_FILEPATH);

	// Delete the region
	rv = cxl_region_delete(region);
	if (rv != 0)
	{
		err(ctx, "Failed to delete region %s rv: %d", cxl_region_get_devname(region), rv);
		goto end;
	}
	else 
		info(ctx, "Deleted region %s", buf);

	rv = 0;

end:

	return rv;
}

/**
 * Get the state of block offset within 
 */
int mem_region_get_blk_state(struct mem_ctx *ctx, struct cxl_region *region, int offset)
{
	int rv;
	unsigned long long block_size, base, size, end, addr;
  	struct mem_blk *blk;

	rv = -1;

	if (offset < 0)
	{
		err(ctx, "Requested offset is below zero: %d", offset);
		goto end;
	}

	// Get memory block size in bytes 
	block_size = mem_system_get_blocksize(ctx);
	if (block_size == 0)
	{
		rv = -1;
		err(ctx, "Unable to obtain system memory block size");
		goto end;
	}

	// Get region base address 
	base = cxl_region_get_resource(region);
	if (base == 0 || base == 0xFFFFFFFFFFFFFFFF)
	{
		rv = -1;
		err(ctx, "Unable to get cxl region %s resource address\n", cxl_region_get_devname(region));
		goto end;
	}

	// Get region size in bytes 
	size = cxl_region_get_size(region);
	if (size == 0)
	{
		rv = -1;
		warn(ctx, "Region size was zero for region %s", cxl_region_get_devname(region));
		goto end;
	}
	end = base + size;
	addr = base + block_size * offset;

	// Verify offset block is within region
	if (addr >= end)
	{
		err(ctx, "Could not get offset within region as it exceeds region range");
		rv = -1;
		goto end;
	}

	// Loop through the memory directories and check address
	mem_blk_foreach(ctx, blk)
	{
		if (addr == (block_size * blk->id))
		{
			rv = mem_blk_get_state(blk);
			goto end;	
		}
	}

end:

	return rv;
}

/**
 * Return an array of integers representing the block index number 
 * @return int* array of phys_index numebrs, NULL on error
 *
 * The length of the array is mem_get_num_blocks()
 */
int *mem_region_get_blocks(struct mem_ctx *ctx, struct cxl_region *region)
{
	unsigned long long block_size, base, size, end, addr;
  	struct mem_blk *blk;
	int i, num; 
	int *array;

	i = 0; 
	array = NULL;

	// Get memory block size in bytes 
	block_size = mem_system_get_blocksize(ctx);
	if (block_size == 0)
	{
		err(ctx, "Unable to read system memory block size");
		goto end;
	}

	// Get region base address 
	base = cxl_region_get_resource(region);
	if (base == 0)
	{
		err(ctx, "Unable to get cxl region %s resource address", cxl_region_get_devname(region));
		return NULL;
	}

	// Get region size in bytes 
	size = cxl_region_get_size(region);
	if (size == 0)
	{
		warn(ctx, "Region size was zero for region %s", cxl_region_get_devname(region));
		return NULL;
	}
	end = base + size;

	// Get the number of blocks 
	num = mem_region_num_blocks(ctx, region);

	// Allocate memory for the array 
	array = malloc(num * sizeof(int));
	if (array == NULL)
		return NULL;

	mem_blk_foreach(ctx, blk)
	{
		addr = block_size * blk->id;			
		if (addr >= base && addr < end)
			array[i++] = blk->id;
	}

	// Sort the array
	qsort(array, num, sizeof(int), mem_compare_ints);

end:

	return array;
}

/**
 * Get total memory capacity of a cxl_region in bytes 
 */
unsigned long long mem_region_get_capacity(struct mem_ctx *ctx, struct cxl_region *region)
{
	unsigned long long capacity; 

	capacity = mem_system_get_blocksize(ctx) * mem_region_num_blocks(ctx, region);

	return capacity;
}
 
/**
 * Get offline memory capacity of a cxl_region in bytes 
 */
unsigned long long mem_region_get_capacity_offline(struct mem_ctx *ctx, struct cxl_region *region)
{
	unsigned long long capacity; 

	capacity = mem_system_get_blocksize(ctx) * mem_region_num_blocks_offline(ctx, region);

	return capacity;
}

/**
 * Get online memory capacity of a cxl_region in bytes 
 */
unsigned long long mem_region_get_capacity_online(struct mem_ctx *ctx, struct cxl_region *region)
{
	unsigned long long capacity; 

	capacity = mem_system_get_blocksize(ctx) * mem_region_num_blocks_online(ctx, region);

	return capacity;
}

/**
 * Determine and return true if cxl_region is in system-ram mode
 */
int mem_region_is_rammode(struct mem_ctx *ctx, struct cxl_region* region)
{
	int rv; 
	struct daxctl_region *dax_region;
	struct daxctl_dev *dax_dev;
	struct daxctl_memory *dax_mem;

	rv = -1;

	// Get the dax region for the cxl_region
	dax_region = cxl_region_get_daxctl_region(region);
	if (dax_region == NULL)
	{
		rv = -1;
		err(ctx, "Failed to obtain dax_region for cxl region %s\n", cxl_region_get_devname(region));
		goto end;
	}

	// Get the dax device 
	dax_dev = daxctl_dev_get_first(dax_region);
	if (dax_dev == NULL)
	{
		err(ctx, "Failed to obtain dax_dev for dax_region %s\n", daxctl_region_get_devname(dax_region));
		goto end;
	}
	
	// Check if region is already in ram mode 
	dax_mem = daxctl_dev_get_memory(dax_dev);
	if (dax_mem == NULL)
	{
		rv = 0;
	}
	else 
		rv = 1;

end:

	return rv;
}

/**
 * Determine and return true if cxl_region is in daxmode 
 */
int mem_region_is_daxmode(struct mem_ctx *ctx, struct cxl_region* region)
{
	int rv; 
	struct daxctl_region *dax_region;
	struct daxctl_dev *dax_dev;
	struct daxctl_memory *dax_mem;

	rv = -1;

	// Get the dax region for the cxl_region
	dax_region = cxl_region_get_daxctl_region(region);
	if (dax_region == NULL)
	{
		err(ctx, "Failed to obtain dax_region for cxl region %s\n", cxl_region_get_devname(region));
		goto end;
	}

	// Get the dax device 
	dax_dev = daxctl_dev_get_first(dax_region);
	if (dax_dev == NULL)
	{
		err(ctx, "Failed to obtain dax_dev for dax_region %s\n", daxctl_region_get_devname(dax_region));
		goto end;
	}
	
	// Check if region is already in ram mode 
	dax_mem = daxctl_dev_get_memory(dax_dev);
	if (dax_mem == NULL)
	{
		rv = 1;
	}
	else 
		rv = 0;

end:

	return rv;
}

/**
 * Get the number of memory blocks within a cxl_region
 * @param num blocks. <0 if error
 */
int mem_region_num_blocks(struct mem_ctx *ctx, struct cxl_region *region)
{
	int num;
	unsigned long long block_size, base, size, end, addr;
  	struct mem_blk *blk;

	num = 0;

	// Get memory block size in bytes 
	block_size = mem_system_get_blocksize(ctx);
	if (block_size == 0)
	{
		num = -1;
		err(ctx, "Unable to obtain system memory block size");
		goto end;
	}

	// Get region base address 
	base = cxl_region_get_resource(region);
	if (base == 0 || base == 0xFFFFFFFFFFFFFFFF)
	{
		num = 0;
		err(ctx, "Unable to get cxl region %s resource address\n", cxl_region_get_devname(region));
		goto end;
	}

	// Get region size in bytes 
	size = cxl_region_get_size(region);
	if (size == 0)
	{
		num = 0;
		warn(ctx, "Region size was zero for region %s", cxl_region_get_devname(region));
		goto end;
	}
	end = base + size;

	// Loop through the memory directories and check address
	mem_blk_foreach(ctx, blk)
	{
		addr = block_size * blk->id;
		if (addr >= base && addr < end)
			num++;	
	}

end:

	return num;
}

/**
 * Get the number of memory blocks offline within a cxl_region
 * @param num blocks. <0 if error
 */
int mem_region_num_blocks_offline(struct mem_ctx *ctx, struct cxl_region *region)
{
	int num;
	unsigned long long block_size, base, size, end, addr;
  	struct mem_blk *blk;

	num = 0;

	// Get memory block size in bytes 
	block_size = mem_system_get_blocksize(ctx);
	if (block_size == 0)
	{
		err(ctx, "Unable to read system memory block size");
		goto end;
	}

	// Get region base address 
	base = cxl_region_get_resource(region);
	if (base == 0 || base == 0xFFFFFFFFFFFFFFFF)
	{
		num = 0;
		err(ctx, "Unable to get cxl region %s resource address", cxl_region_get_devname(region));
		goto end;
	}

	// Get region size in bytes 
	size = cxl_region_get_size(region);
	if (size == 0)
	{
		num = 0;
		warn(ctx, "Region size was zero for region %s", cxl_region_get_devname(region));
		goto end;
	}
	end = base + size;

	// Loop through the memory directories and check address
	mem_blk_foreach(ctx, blk)
	{
		addr = block_size * blk->id;			
		if (addr >= base && addr < end && !mem_blk_is_online(blk))
			num++;	
	}

end:

	return num;
}

/**
 * Get the number of memory blocks online within a cxl_region
 * @param num blocks. <0 if error
 */
int mem_region_num_blocks_online(struct mem_ctx *ctx, struct cxl_region *region)
{
	int num;
	unsigned long long block_size, base, size, end, addr;
  	struct mem_blk *blk;

	num = 0;

	// Get memory block size in bytes 
	block_size = mem_system_get_blocksize(ctx);
	if (block_size == 0)
	{
		err(ctx, "Unable to read system memory block size");
		goto end;
	}

	// Get region base address 
	base = cxl_region_get_resource(region);
	if (base == 0 || base == 0xFFFFFFFFFFFFFFFF)
	{
		num = 0;
		err(ctx, "Unable to get cxl region %s resource address", cxl_region_get_devname(region));
		goto end;
	}

	// Get region size in bytes 
	size = cxl_region_get_size(region);
	if (size == 0)
	{
		num = 0;
		warn(ctx, "Region size was zero for region %s", cxl_region_get_devname(region));
		goto end;
	}
	end = base + size;

	// Loop through the memory directories and check address
	mem_blk_foreach(ctx, blk)
	{
		addr = block_size * blk->id;			
		if (addr >= base && addr < end && mem_blk_is_online(blk))
			num++;	
	}

end:

	return num;
}

/**
 * Offline all blocks in a region
 */
int mem_region_offline_blocks(struct mem_ctx *ctx, struct cxl_region *region)
{
	int rv, ret;
	struct mem_blk *blk;
	unsigned long long block_size, base, size, end, addr;

	// Initialize variables 
	rv = 1;

	// Get memory block size in bytes 
	block_size = mem_system_get_blocksize(ctx);
	if (block_size == 0)
	{
		rv = 1;
		err(ctx, "Unable to obtain system memory block size");
		goto end;
	}

	// Get region base address 
	base = cxl_region_get_resource(region);
	if (base == 0 || base == 0xFFFFFFFFFFFFFFFF)
	{
		rv = 1;
		err(ctx, "Unable to get cxl region %s resource address\n", cxl_region_get_devname(region));
		goto end;
	}

	// Get region size in bytes 
	size = cxl_region_get_size(region);
	if (size == 0)
	{
		rv = 0;
		warn(ctx, "Region size was zero for region %s", cxl_region_get_devname(region));
		goto end;
	}
	end = base + size;

	// Loop through the memory directories and check address
	rv = 0;
	mem_blk_foreach(ctx, blk)
	{
		addr = block_size * blk->id;
		if (addr >= base && addr < end)
		{
			ret = mem_blk_offline(blk);	
			if (ret != 0)
			{
				err(ctx, "Could not offline memory block %d. %d", blk->id, ret);
				rv += 1;
			}
		}
	}

	if (rv == 0)
	{info(ctx, "Offlined all blocks of region %s", cxl_region_get_devname(region));}
	else 
		err(ctx, "Failed to offline all blocks of region %s", cxl_region_get_devname(region));

end:

	return rv;
}

/**
 * Online all blocks in a region
 */
int mem_region_online_blocks(struct mem_ctx *ctx, struct cxl_region *region)
{
	int rv, ret;
	struct mem_blk *blk;
	unsigned long long block_size, base, size, end, addr;

	// Initialize variables 
	rv = 1;

	// Get memory block size in bytes 
	block_size = mem_system_get_blocksize(ctx);
	if (block_size == 0)
	{
		rv = 1;
		err(ctx, "Unable to obtain system memory block size");
		goto end;
	}

	// Get region base address 
	base = cxl_region_get_resource(region);
	if (base == 0 || base == 0xFFFFFFFFFFFFFFFF)
	{
		rv = 1;
		err(ctx, "Unable to get cxl region %s resource address\n", cxl_region_get_devname(region));
		goto end;
	}

	// Get region size in bytes 
	size = cxl_region_get_size(region);
	if (size == 0)
	{
		rv = 0;
		warn(ctx, "Region size was zero for region %s", cxl_region_get_devname(region));
		goto end;
	}
	end = base + size;

	// Loop through the memory directories and check address
	rv = 0;
	mem_blk_foreach(ctx, blk)
	{
		addr = block_size * blk->id;
		if (addr >= base && addr < end)
		{
			ret = mem_blk_online(blk);	
			if (ret != 0)
			{
				err(ctx, "Could not online memory block %d. %d", blk->id, ret);
				rv += 1;
			}
		}
	}

	if (rv == 0)
	{info(ctx, "Onlined all blocks of region %s", cxl_region_get_devname(region));}
	else 
		err(ctx, "Failed to online all blocks of region %s", cxl_region_get_devname(region));

end:

	return rv;
}

/**
 * Set a cxl_region to system-ram mode 
 */
int mem_region_rammode(struct mem_ctx *ctx, struct cxl_region *region)
{
	int rv; 
	struct daxctl_region *dax_region;
	struct daxctl_dev *dax_dev;
	struct daxctl_memory *dax_mem;

	rv = 1;

	// Get the dax region for the cxl_region
	dax_region = cxl_region_get_daxctl_region(region);
	if (dax_region == NULL)
	{
		rv = 1;
		err(ctx, "Failed to obtain dax_region for cxl region %s\n", cxl_region_get_devname(region));
		goto end;
	}

	// Get the dax device 
	dax_dev = daxctl_dev_get_first(dax_region);
	if (dax_dev == NULL)
	{
		err(ctx, "Failed to obtain dax_dev for dax_region %s\n", daxctl_region_get_devname(dax_region));
		goto end;
	}

	// Check if region is already in ram mode 
	dax_mem = daxctl_dev_get_memory(dax_dev);
	if (dax_mem != NULL)
	{
		rv = 0;
		info(ctx, "dax_dev %s was already in system-ram mode\n", daxctl_dev_get_devname(dax_dev));
		goto end;
	}

	// Disable the dax device if enabled 
	if (daxctl_dev_is_enabled(dax_dev))
	{
		rv = daxctl_dev_disable(dax_dev);
		if (rv != 0)
		{
			err(ctx, "Failed to disable dax_dev %s %d\n", daxctl_dev_get_devname(dax_dev), rv);
			goto end;
		}
		else
			info(ctx, "Disabled dax device %s", daxctl_dev_get_devname(dax_dev));
	}

	// Set region to system-ram mode 
	rv = daxctl_dev_enable_ram(dax_dev);
	if (rv != 0)
	{
		err(ctx, "Failed to enable system ram mode on %s %d\n", daxctl_dev_get_devname(dax_dev), rv);
		goto end;
	}
	else
		info(ctx, "Enabled system-ram mode on dax device %s", daxctl_dev_get_devname(dax_dev));
	
	rv = 0;

end:

	return rv;
}

/**
 * Set the online state of the memory block within a specified region 
 */
int mem_region_set_blk_state(struct mem_ctx *ctx, struct cxl_region *region, int offset, int mode)
{
	int rv;
	unsigned long long block_size, base, size, end, addr;
  	struct mem_blk *blk;

	rv = -1;

	if (offset < 0)
	{
		err(ctx, "Requested offset is below zero: %d", offset);
		goto end;
	}

	// Get memory block size in bytes 
	block_size = mem_system_get_blocksize(ctx);
	if (block_size == 0)
	{
		rv = -1;
		err(ctx, "Unable to obtain system memory block size");
		goto end;
	}

	// Get region base address 
	base = cxl_region_get_resource(region);
	if (base == 0 || base == 0xFFFFFFFFFFFFFFFF)
	{
		rv = -1;
		err(ctx, "Unable to get cxl region %s resource address\n", cxl_region_get_devname(region));
		goto end;
	}

	// Get region size in bytes 
	size = cxl_region_get_size(region);
	if (size == 0)
	{
		rv = -1;
		warn(ctx, "Region size was zero for region %s", cxl_region_get_devname(region));
		goto end;
	}
	end = base + size;
	addr = base + block_size * offset;

	// Verify offset block is within region
	if (addr >= end)
	{
		err(ctx, "Could not get offset within region as it exceeds region range");
		rv = -1;
		goto end;
	}

	// Loop through the memory directories and check address
	mem_blk_foreach(ctx, blk)
	{
		if (addr == (block_size * blk->id))
		{
			rv = mem_blk_set_state(blk, mode);
			goto end;	
		}
	}

end:

	return rv;
}

/**
 * Read in a sysfs attribute 
 * @return the number of bytes read. negative errno if an error
 */
static int mem_sysfs_read(struct mem_ctx *ctx, const char *path, char *buf)
{
	int n, fd;

	fd = open(path, O_RDONLY|O_CLOEXEC);
	if (fd < 0) 
	{
		n = -errno;
		err(ctx, "Failed to open sysfs file: %s %d - %s", path, errno, strerror(errno) );
		goto end;
	}

	n = read(fd, buf, LMLN_SYSFS_ATTR_SIZE);
	close(fd);

	if (n < 0 || n >= LMLN_SYSFS_ATTR_SIZE) 
	{
		buf[0] = 0;
		n = -errno;
		err(ctx, "Failed to read sysfs file: %s %d - %s", path, errno, strerror(errno) );
		goto end;
	}

	buf[n] = 0;
	if (n > 0 && buf[n-1] == '\n')
		buf[n-1] = 0;

end:

	return n;
}

/**
 * Write a value to a sysfs atribute 
 * @return the number of bytes written, 0 or negative number of bytes written if an error
 */
static int mem_sysfs_write(struct mem_ctx *ctx, const char *path, const char *buf)
{
	int n, fd, len;

	fd = open(path, O_WRONLY|O_CLOEXEC);
	if (fd < 0) 
	{
		n = -errno;
		err(ctx, "Failed to open sysfs file: %s %d - %s", path, errno, strerror(errno) );
		goto end;
	}

	len = strlen(buf) + 1;

	n = write(fd, buf, len);
	close(fd);

	if (n < len) 
	{
		err(ctx, "Failed to write all bytes to sysfs file: %s %d/%d", path, n, len);
		n = -n;
		goto end;
	}

end:

	return n;
}
 
/**
 * Return an array of integers representing the block index number 
 * @return int* array of phys_index numebrs, NULL on error
 *
 * The length of the array is mem_get_num_blocks()
 */
int *mem_system_get_blocks(struct mem_ctx *ctx)
{
	int i, num; 
	int *array;
	struct mem_blk *blk;

	i = 0; 

	// Get the number of blocks 
	num = mem_system_num_blocks(ctx);

	// Allocate memory for the array 
	array = malloc(num * sizeof(int));
	if (array == NULL)
		return NULL;

	// Walk the list of blocks and add the IDs 
	mem_blk_foreach(ctx, blk)
		array[i++] = mem_blk_get_id(blk);

	// Sort the array
	qsort(array, num, sizeof(int), mem_compare_ints);

	return array;
}

/**
 * Get system memory block size 
 * @return unsigned long long size i Get system memory block size 
 * @return unsigned long long size in bytes. 0 if error
 */
unsigned long long mem_system_get_blocksize(struct mem_ctx *ctx)
{
	int rv;
	char path[LMLN_FILEPATH];
	char buf[LMLN_SYSFS_ATTR_SIZE];

	sprintf(path, "%s/%s", LMFP_MEM_DIR, "block_size_bytes");							
	rv = mem_sysfs_read(ctx, path, buf);
	if (rv <= 0)
	{
		err(ctx, "Unable to read system memory block size: %d\n", rv);
		return 0;
	}

	return strtoul(buf, NULL, 16);
}

/**
 * Get total system memory capacity in bytes 
 */
unsigned long long mem_system_get_capacity(struct mem_ctx *ctx)
{
	unsigned long long capacity; 

	capacity = mem_system_get_blocksize(ctx) * mem_system_num_blocks(ctx);

	return capacity;
}

/**
 * Get offline system memory capacity in bytes 
 */
unsigned long long mem_system_get_capacity_offline(struct mem_ctx *ctx)
{
	unsigned long long capacity; 

	capacity = mem_system_get_blocksize(ctx) * mem_system_num_blocks_offline(ctx);

	return capacity;
}

/**
 * Get online system memory capacity in bytes 
 */
unsigned long long mem_system_get_capacity_online(struct mem_ctx *ctx)
{
	unsigned long long capacity; 

	capacity = mem_system_get_blocksize(ctx) * mem_system_num_blocks_online(ctx);

	return capacity;
}
 
/**
 * Get the current auto_online_policy 
 * 
 * Returns an int LMPL represeting the online policy
 */
int mem_system_get_policy(struct mem_ctx *ctx)
{
	int rv;
	char path[LMLN_FILEPATH];
	char buf[LMLN_SYSFS_ATTR_SIZE];

	sprintf(path, "%s/%s", LMFP_MEM_DIR, "auto_online_blocks");							
	rv = mem_sysfs_read(ctx, path, buf);
	if (rv <= 0)
	{
		err(ctx, "Unable to read system auto memory online policy from sysfs: %d", rv);
		return -1;
	}

	return mem_to_lmpl(buf);
}	

/**
 * Get the number of memory blocks in the system
 * @return The number of blocks. 0 if error
 */
int mem_system_num_blocks(struct mem_ctx *ctx)
{
  	struct mem_blk *blk;
	int num;

	num = 0;

	mem_blk_foreach(ctx, blk)
		num++;	

	return num;
}

/**
 * Return the number of memory blocks that are offline
 */
int mem_system_num_blocks_offline(struct mem_ctx *ctx)
{
  	struct mem_blk *blk;
	int num;

	num = 0;

	mem_blk_foreach(ctx, blk)
		if (!mem_blk_is_online(blk))
			num++;	

	return num;
}

/**
 * Return the number of memory blocks that are online 
 */
int mem_system_num_blocks_online(struct mem_ctx *ctx)
{
  	struct mem_blk *blk; 
	int num;

	num = 0;

	mem_blk_foreach(ctx, blk)
		if (mem_blk_is_online(blk))
			num++;	

	return num;
}

/**
 * Set the auto online policy for a memory block
 * @param mode int representing policy [LMPL]
 */
int mem_system_set_policy(struct mem_ctx *ctx, int mode)
{
	int rv; 
	char path[LMLN_FILEPATH];

	rv = 0;

	if (mode < 0 || mode >= LMPL_MAX)
	{
		rv = -2;
		err(ctx, "User attempted to set an invalid memory auto online policy: %d", mode);
		goto end;
	}

	if (mode == mem_system_get_policy(ctx))
	{
		info(ctx, "Memory policy already in state %s. Skipping", mem_lmpl(mode));
		rv = 0;
		goto end;
	}

	sprintf(path, "%s/%s", LMFP_MEM_DIR, "auto_online_blocks");							
	rv = mem_sysfs_write(ctx, path, mem_lmpl(mode));
	if (rv <= 0 || rv != (int) (strlen(mem_lmpl(mode))+1))
	{
		err(ctx, "Failed to write memory auto online policy to sysfs: %d", rv);
		rv = -1;
		goto end;;
	}
	else 
		info(ctx, "Set online policy to %s", mem_lmpl(mode));

	rv = 0;

end:

	return rv;
}

/* Return the enum LMPL representing a string */
int mem_to_lmpl(char *policy)
{
	for ( int i = 0 ; i < LMPL_MAX ; i++ )
		if (!strcmp(policy, mem_lmpl(i)))
			return i;
	return -1;
}

/* Return the enum LMST representing a string */
int mem_to_lmst(char *state)
{
	for ( int i = 0 ; i < LMST_MAX ; i++ )
		if (!strcmp(state, mem_lmst(i)))
			return i;
	return -1;
}

/* Return the enum LMZN representing a string */
int mem_to_lmzn(char *zone)
{
	for ( int i = 0 ; i < LMZN_MAX ; i++ )
		if (!strcmp(zone, mem_lmzn(i)))
			return i;
	return -1;
}

/**
 * Free the memory context object
 */
int mem_unref(struct mem_ctx *ctx)
{
	if (ctx == NULL)
		return 1;

	// Decrement the ref counter and check if there are still references
	ctx->refcount--;
	if (ctx->refcount > 0)
		return 0;

	if (ctx->regions != NULL)
		free(ctx->regions);

	if (ctx->cxl)
		cxl_unref(ctx->cxl);
	
	if (ctx->log)
		log_free(ctx->log);
	
	free(ctx);

	return 0;
}

// Append point ////////////////////////////////////////////////////////////////////

