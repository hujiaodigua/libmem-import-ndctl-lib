/**       
 * @file        cli.c
 *
 * @brief       cli code file for memory management library
 *   
 * @copyright   Copyright (C) 2024 Jackrabbit Founders LLC. All rights reserved.
 *       
 * @date        Jul 2024
 * @author      Barrett Edwards <code@jrlabs.io>
 *          
 */

/* INCLUDES ==================================================================*/

/* printf()
 * fprintf()
 * stderr
 */
#include <stdio.h>

/* atoi()
 * calloc()
 * free()
 */
#include <stdlib.h>

/* strcmp()
 */
#include <string.h>

 /* errno MACROS
  */
 #include <errno.h>

/* CLI_LOG_LEVEL
 */
#include <sys/syslog.h>

/* cxl_new()
 * cxl_region
 * cxl_memdev
 * cxl_decoder
 */
#include <cxl/libcxl.h>

#include "options.h"

#include "libmem.h"

/* MACROS ====================================================================*/

#define CLI_LOG_LEVEL 	LOG_DEBUG
#define CLI_LOG_DST  	LMLD_SYSLOG
#define CLI_IG 			4096

/* ENUMERATIONS ==============================================================*/

/* STRUCTS ===================================================================*/

struct cli_arg
{
	char *name;
	char *args;
	char *help;
	int (*fn)(int argc, char **argv);
};

/* PROTOTYPES ================================================================*/

int cmd_blk_offline(int num, int start);
int cmd_blk_online(int num, int start);

int cmd_info();
int cmd_list(int online, int offline, char *region_name);

int cmd_region_create(int granularity, int num, char **names);
int cmd_region_delete(char *name);
int cmd_region_disable(char *name);
int cmd_region_enable(char *name);
int cmd_region_daxmode(char *name);
int cmd_region_rammode(char *name);
int cmd_region_set_blk_state(char *name, int offset, int state);

int cmd_set_blk_state(int index, int state);
int cmd_set_system_policy(int policy);

int cmd_show_blk_device(int id);
int cmd_show_blk_isonline(int id);
int cmd_show_blk_isremovable(int id);
int cmd_show_blk_node(int id);
int cmd_show_blk_state(int id);
int cmd_show_blk_zones(int id);

int cmd_show_blocks(int online, int offline, char *region_name);

int cmd_show_capacity(int online, int offline, char *region_name, int human);

int cmd_show_memdev_interleave_granulariy(char *name); //todo 
int cmd_show_memdev_isavailable(char *name);
int cmd_show_memdevs(char *memdev_name, char *region_name, int human);

int cmd_show_num_blocks(int online, int offline, char *region_name);
int cmd_show_num_devices();
int cmd_show_num_regions();

int cmd_show_region_blk_state(char *name, int offset);

int cmd_show_region_isenabled(char *name);

int cmd_show_regions(char *name, int human);

int cmd_show_system_blocksize(int human);
int cmd_show_system_policy();

/* GLOBAL VARIABLES ==========================================================*/

/* FUNCTIONS =================================================================*/

int cmd_blk_offline(int num, int start)
{
	int rv; 
	struct mem_ctx *ctx;

	rv = 1;

	// Validate Privileges
	if ( getuid() != 0 )
	{
		fprintf(stderr, "Error: Command must be run as root\n");
		rv = -EACCES;
		goto end;
	}

	// Get mem contex
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	for ( int i = start ; i < (start + num) ; i++)
	{
		if (mem_blkid_is_online(ctx, i) == 1)
		{
			// Online a memory block
			rv = mem_blkid_offline(ctx, i);
			if (rv != 0)
			{
				fprintf(stderr, "Error: Could not offline memory block %d. %d\n", i, rv);
				goto err;
			}
		}
	}

	rv = 0;

err:

	mem_unref(ctx);

end:

	return rv;
}

int cmd_blk_online(int num, int start)
{
	int rv; 
	struct mem_ctx *ctx; 
	struct mem_blk *blk;

	rv = 1;

	// Validate Privileges
	if ( getuid() != 0 )
	{
		fprintf(stderr, "Error: Command must be run as root\n");
		rv = -EACCES;
		goto end;
	}

	// Get mem contex
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	// Online all blocks 
	if (num < 0)
	{
		mem_blk_foreach(ctx, blk)
			if (!mem_blk_is_online(blk))
				mem_blk_online(blk);
	}
	else 
	{
		for ( int i = start ; i < (start + num) ; i++)
		{
			if (!mem_blkid_is_online(ctx, i))
			{
				// Online a memory block
				rv = mem_blkid_online(ctx, i);
				if (rv != 0)
				{
					fprintf(stderr, "Error: Could not online memory block %d. %d\n", i, rv);
					goto err;
				}
			}
		}
	}

	rv = 0;

err:
	
	mem_unref(ctx);

end:

	return rv;
}

int cmd_info()
{
	int rv; 
	struct mem_ctx *ctx; 

	// Create mem context 
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	printf("Memory Blocksize:              %llu\n", 	mem_system_get_blocksize(ctx));
	printf("Auto Online Memory Policy:     %s\n", 		mem_lmpl(mem_system_get_policy(ctx)));
	printf("Number of Blocks:              %d\n", 		mem_system_num_blocks(ctx));
	printf("  Number of Blocks online:     %d\n", 		mem_system_num_blocks_online(ctx));
	printf("  Number of Blocks offline:    %d\n", 		mem_system_num_blocks_offline(ctx));
	printf("Memory Capacity:               %llu\n", 	mem_system_get_capacity(ctx));
	printf("  Memory Capacity online:      %llu\n", 	mem_system_get_capacity_online(ctx));
	printf("  Memory Capacity offline:     %llu\n", 	mem_system_get_capacity_offline(ctx));
	printf("Number of CXL regions:         %d\n", 		mem_num_regions(ctx));
	printf("Number of CXL memdevs:         %d\n", 		mem_num_memdevs(ctx));
	
	mem_unref(ctx);

end:

	return rv;
}

int cmd_list(int online, int offline, char *region_name)
{
	int rv;
	struct mem_blk *blk;
	struct mem_ctx *ctx;
	struct cxl_region *region;
	unsigned long u;

	// Initialize variables
	rv = 1;

	// Get mem context 
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	printf("Index  node  online  cxl_region  zones\n");
	printf("-----  ----  ------  ----------  -------------------\n");

	mem_blk_foreach(ctx, blk)
	{
		if (online && !mem_blk_is_online(blk))
			continue;

		if (offline && mem_blk_is_online(blk))
			continue;

		region = mem_blk_get_region(blk);	

		if (region == NULL && region_name != NULL)
			continue;

		if (region != NULL && region_name != NULL && strcmp(region_name, cxl_region_get_devname(region)) )
			continue;

		printf("%-5d  %-4d  %-6d  ", 
			mem_blk_get_id(blk), 
			mem_blk_get_node(blk), 
			mem_blk_is_online(blk));

		if (region != NULL)
			printf("%-10s  ", cxl_region_get_devname(region));
		else 
			printf("%-10s  ", "-");

		// Print zones 
		u = mem_blk_get_zones(blk); 
		for ( int k = 0 ; k < LMZN_MAX ; k++)
		{
			if (u & (0x01 << k))
				printf("%s ", mem_lmzn(k));
		}
		printf("\n");
	}

end:
	return rv;
}

int cmd_region_create(int granularity, int num, char **names)
{
	int rv;
	struct cxl_memdev **memdevs;
	struct mem_ctx *ctx;

	// Initialize variables
	rv = 1;

	// Validate Privileges
	if ( getuid() != 0 )
	{
		fprintf(stderr, "Error: Command must be run as root\n");
		rv = -EACCES;
		goto end;
	}

	// Validate Inputs 
	if (granularity == 0)
		granularity = CLI_IG;

	if (!(granularity == 256 
		|| granularity == 512
		|| granularity == 1024
		|| granularity == 2048
		|| granularity == 4096
		|| granularity == 8192))
	{
		fprintf(stderr, "Error: Invalid Interleave Granularity: %d\n", granularity);
		rv = -EINVAL;
		goto end;
	}

	if (num < 0)
	{
		fprintf(stderr, "Error: Missing memdev[s]\n");
		rv = -EINVAL;
		goto end;
	}

	// Get mem context 
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	// Use all memdevs 
	if (num == 0)
	{
		num = mem_num_memdevs(ctx);
		memdevs = mem_get_memdevs(ctx);
	}
	else 
	{
		// Allocate memory for the array of memdevs 
		memdevs = calloc(num, sizeof(*memdevs));
		if (memdevs == NULL)
		{
			fprintf(stderr, "Error: Could not allocate memory. %d - %s\n", errno, strerror(errno));
			rv = -ENOMEM;
			goto err;
		}

		// Loop through requested memory devices 
		for ( int i = 0 ; i < num ; i++ )
		{
			memdevs[i] = mem_get_memdev(ctx, names[i]);
			if (memdevs[i] == NULL)
			{
				fprintf(stderr, "Error: Could not obtain memdev: %s\n", names[i]);
				rv = 1;
				goto err;
			}
		}
	}

	// Create the region 
	rv = mem_region_create(ctx, granularity, num, memdevs);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Could not create region: %d\n", rv);
		rv = 1;
		goto err;
	}

	rv = 0;

err:
	
	free(memdevs);
	mem_unref(ctx);

end:

	return rv;
}

int cmd_region_daxmode(char *name)
{
	int rv;
	struct mem_ctx *ctx;
	struct cxl_region *region;

	// Initialize variables 
	rv = 1;

	// Validate Privileges
	if ( getuid() != 0 )
	{
		fprintf(stderr, "Error: Command must be run as root\n");
		rv = -EACCES;
		goto end;
	}

	// Validate Inputs 
	if (name == NULL)
	{
		fprintf(stderr, "Error: Missing region\n");
		rv = -EINVAL;
		goto end;
	}

	// Get mem context 
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	// Get region 
	region = mem_get_region(ctx, name);
	if (region == NULL)
	{
		fprintf(stderr, "Error: Could not obtain region: %s\n", name);
		rv = 1;
		goto err;
	}	

	// Enable devdax mode
	rv = mem_region_daxmode(ctx, region);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Enable of devdax mode failed: %d\n", rv);
		rv = 1;
		goto err;
	}

	rv = 0;

err:

	mem_unref(ctx);

end:

	return rv;
}

int cmd_region_delete(char *name)
{
	int rv, num;
	struct mem_ctx *ctx;
	struct cxl_region *region, **regions;

	// Initialize variables 
	rv = 1;

	// Validate Privileges
	if ( getuid() != 0 )
	{
		fprintf(stderr, "Error: Command must be run as root\n");
		rv = -EACCES;
		goto end;
	}

	// Get mem context 
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	// Delete all regions 
	if (name == NULL)
	{
		num = mem_num_regions(ctx);
		regions = mem_get_regions(ctx);
		char name[256];

		for ( int i = 0 ; i < num ; i++)
		{
			// Delete region 
			strcpy(name, cxl_region_get_devname(regions[i]));
			rv = mem_region_delete(ctx, regions[i]);
			if (rv != 0)
			{
				fprintf(stderr, "Error: Could not delete region: %s\n", name);
				rv = 1;
				goto err;
			}
		}
	}
	else 
	{
		// Get region 
		region = mem_get_region(ctx, name);	
		if (region == NULL)
		{
			fprintf(stderr, "Error: Could not obtain region: %s\n", name);
			rv = 1;
			goto err;
		}

		// Delete region 
		rv = mem_region_delete(ctx, region);
		if (rv != 0)
		{
			fprintf(stderr, "Error: Could not delete region: %s\n", name);
			rv = 1;
			goto err;
		}
	}

	rv = 0;

err: 

	mem_unref(ctx);

end:

	return rv;
}

int cmd_region_disable(char *name)
{
	int rv; 
	struct mem_ctx *ctx;
	struct cxl_region *region;

	// Initialize variables 
	rv = 1;

	// Validate Privileges
	if ( getuid() != 0 )
	{
		fprintf(stderr, "Error: Command must be run as root\n");
		rv = -EACCES;
		goto end;
	}

	// Validate Inputs 
	if (name == NULL)
	{
		fprintf(stderr, "Error: Missing region\n");
		rv = -EINVAL;
		goto end;
	}

	// Get mem context 
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	// Get region 
	region = mem_get_region(ctx, name);
	if (region == NULL)
	{
		fprintf(stderr, "Error: Could not obtain region: %s\n", name);
		rv = 1;
		goto err;
	}	

	// Check if Region was enabled 
	rv = cxl_region_is_enabled(region);
	if (rv == 0)
	{
		fprintf(stderr, "Region was already disabled\n");
		rv = 1;
		goto err;
	}

	// Disable Region 
	rv = cxl_region_disable(region);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Could not disable region: %d\n", rv);
		rv = 1;
		goto err;
	}

	rv = 0;

err: 

	mem_unref(ctx);

end:

	return rv;
}

int cmd_region_enable(char *name)
{
	int rv; 
	struct mem_ctx *ctx;
	struct cxl_region *region;

	// Initialize variables 
	rv = 1;

	// Validate Privileges
	if ( getuid() != 0 )
	{
		fprintf(stderr, "Error: Command must be run as root\n");
		rv = -EACCES;
		goto end;
	}

	// Validate Inputs 
	if (name == NULL)
	{
		fprintf(stderr, "Error: Missing region\n");
		rv = -EINVAL;
		goto end;
	}

	// Get mem context 
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	// Get the region 
	region = mem_get_region(ctx, name);
	if (region == NULL)
	{
		fprintf(stderr, "Error: Could not obtain region: %s\n", name);
		rv = 1;
		goto err;
	}	

	// Check if region is already enabled 
	rv = cxl_region_is_enabled(region);
	if (rv == 1)
	{
		fprintf(stderr, "Region was already enabled\n");
		rv = 1;
		goto err;
	}

	// Enable the region 
	rv = cxl_region_enable(region);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Could not enable region: %d\n", rv);
		rv = 1;
		goto err;
	}

	rv = 0;

err:

	mem_unref(ctx);

end:

	return rv;
}

int cmd_region_rammode(char *name)
{
	int rv;
	struct mem_ctx *ctx;
	struct cxl_region *region;

	// Initialize variables 
	rv = 1;

	// Validate Privileges
	if ( getuid() != 0 )
	{
		fprintf(stderr, "Error: Command must be run as root\n");
		rv = -EACCES;
		goto end;
	}

	// Validate Inputs 
	if (name == NULL)
	{
		fprintf(stderr, "Error: Missing region\n");
		rv = -EINVAL;
		goto end;
	}

	// Get mem context 
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	// Get Region 
	region = mem_get_region(ctx, name);
	if (region == NULL)
	{
		fprintf(stderr, "Error: Could not obtain region: %s\n", name);
		rv = 1;
		goto err;
	}	

	// Enable Ram mode
	rv = mem_region_rammode(ctx, region);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Enable of systemram mode failed: %d\n", rv);
		rv = 1;
		goto err;
	}

	rv = 0;

err: 

	mem_unref(ctx);	

end:

	return rv;
}

int cmd_region_set_blk_state(char *name, int offset, int state)
{
	int rv, num;
	struct mem_ctx *ctx;
	struct cxl_region *region;

	rv = 1;

	// Validate Privileges
	if ( getuid() != 0 )
	{
		fprintf(stderr, "Error: Command must be run as root\n");
		rv = -EACCES;
		goto end;
	}

	// Validate Inputs 
	if (name == NULL)
	{
		fprintf(stderr, "Error: Missing region\n");
		rv = -EINVAL;
		goto end;
	}

	if (offset < -1)
	{
		fprintf(stderr, "Error: Invalid index\n");
		rv = -EINVAL;
		goto end;
	}

	if (state <0 || state >= LMPL_MAX)
	{
		fprintf(stderr, "Error: Invalid state\n");
		rv = -EINVAL;
		goto end;
	}

	// Get mem contex
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	region = mem_get_region(ctx, name);
	if (region == NULL)
	{
		fprintf(stderr, "Error: Could not obtain region\n");
		rv = 1;
		goto err;
	}

	if (offset == -1)
	{
		num = mem_region_num_blocks(ctx, region);
		for ( int i = 0 ; i < num ; i++)
		{
			// Set the state 
			rv = mem_region_set_blk_state(ctx, region, i, state);
			if (rv < 0)
			{
				fprintf(stderr, "Error: Could not set state of memory block %d in region %s: %d\n", i, cxl_region_get_devname(region), rv);	
				rv = 1;
				goto err;
			}
		}
	}
	else 
	{
		// Set the state 
		rv = mem_region_set_blk_state(ctx, region, offset, state);
		if (rv < 0)
		{
			fprintf(stderr, "Error: Could not set state of memory block %d in region %s: %d\n", offset, cxl_region_get_devname(region), rv);	
			rv = 1;
			goto err;
		}
	}

	rv = 0;

err:

	mem_unref(ctx);

end:

	return rv;
}

int cmd_set_blk_state(int index, int state)
{
	int rv;
	struct mem_ctx *ctx;

	rv = 1;

	// Validate Privileges
	if ( getuid() != 0 )
	{
		fprintf(stderr, "Error: Command must be run as root\n");
		rv = -EACCES;
		goto end;
	}

	// Validate Inputs 
	if (index < 0)
	{
		fprintf(stderr, "Error: Invalid index\n");
		rv = -EINVAL;
		goto end;
	}

	if (state < 0 || state >= LMPL_MAX)
	{
		fprintf(stderr, "Error: Invalid state\n");
		rv = -EINVAL;
		goto end;
	}

	// Get mem contex
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	// Set the state 
	rv = mem_blkid_set_state(ctx, index, state);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Could not set state of memory block. %d\n", rv);	
		rv = 1;
		goto err;
	}

	rv = 0;

err:

	mem_unref(ctx);

end:

	return rv;
}

int cmd_set_system_policy(int policy)
{
	int rv;
	struct mem_ctx *ctx;

	rv = 1;

	// Validate Privileges
	if ( getuid() != 0 )
	{
		fprintf(stderr, "Error: Command must be run as root\n");
		rv = -EACCES;
		goto end;
	}

	// Validate Inputs 
	if (policy < 0 || policy >= LMPL_MAX)
	{
		fprintf(stderr, "Error: Inavlid policy\n");
		rv = 1;
		goto end;
	}

	// Get mem contex
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	// Set the policy
	rv = mem_system_set_policy(ctx, policy);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Could not set policy. %d\n", rv);
		rv = 1;
		goto err;
	}

	rv = 0;

err:

	mem_unref(ctx);

end:

	return rv;
}

int cmd_show_blk_device(int id)
{
	int rv;
	struct mem_ctx *ctx;

	rv = 1;

	// Validate Inputs 
	if (id < 1)
	{
		fprintf(stderr, "Error: Missing block index\n");
		rv = -EINVAL;
		goto end;
	}

	// Get mem contex
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	// Get phys device 
	rv = mem_blkid_get_device(ctx, id);
	if (rv < 0)
	{
		fprintf(stderr, "Error: Could not obtain phys_device. %d\n", rv);	
		rv = 1;
		goto err;
	}
	
	printf("%d\n", rv);
	rv = 0;

err:

	mem_unref(ctx);

end:

	return rv;
}

int cmd_show_blk_isonline(int id)
{
	int rv;
	struct mem_ctx *ctx;
	struct mem_blk *blk;

	rv = 1;

	// Validate Inputs 
	if (id < 1)
	{
		fprintf(stderr, "Error: Missing block index\n");
		rv = -EINVAL;
		goto end;
	}

	// Get mem contex
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	// Check if memory block is online 
	rv = -1;
	mem_blk_foreach(ctx, blk)
		if (id == mem_blk_get_id(blk))
		{
			rv = mem_blk_is_online(blk);
			break;
		}
	printf("%d\n", rv);

	rv = 0;

	mem_unref(ctx);

end:

	return rv;
}

int cmd_show_blk_isremovable(int id)
{
	int rv;
	struct mem_ctx *ctx;

	rv = 1;

	// Validate Inputs 
	if (id < 1)
	{
		fprintf(stderr, "Error: Missing block index\n");
		rv = -EINVAL;
		goto end;
	}

	// Get mem contex
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	rv = mem_blkid_is_removable(ctx, id);
	printf("%d\n", rv);

	rv = 0;

	mem_unref(ctx);

end:

	return rv;
}

int cmd_show_blk_node(int id)
{
	int rv; 
	struct mem_ctx *ctx;

	rv = 1;

	// Validate Inputs 
	if (id < 1)
	{
		fprintf(stderr, "Error: Missing block index\n");
		rv = -EINVAL;
		goto end;
	}

	// Get mem contex
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	rv = mem_blkid_get_node(ctx, id);
	printf("%d\n", rv);

	rv = 0;
	
	mem_unref(ctx);

end:

	return rv;
}

int cmd_show_blk_state(int id)
{
	int rv;
	struct mem_ctx *ctx;

	rv = 1;

	// Validate Inputs 
	if (id < 1)
	{
		fprintf(stderr, "Error: Missing block index\n");
		rv = -EINVAL;
		goto end;
	}
	
	// Get mem contex
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	rv = mem_blkid_get_state(ctx, id);
	if (rv < 0)
	{
		fprintf(stderr, "Error: Could not obtain block state: %d\n", rv);	
		rv = 1;
		goto err;
	}

	printf("%s\n", mem_lmpl(rv));
	rv = 0;

err:

	mem_unref(ctx);

end:

	return rv;
}

int cmd_show_blk_zones(int id)
{
	int i, rv;
	unsigned long u;
	struct mem_ctx *ctx;

	rv = 1;

	// Validate Inputs 
	if (id < 1)
	{
		fprintf(stderr, "Error: Missing block index\n");
		rv = -EINVAL;
		goto end;
	}

	// Get mem contex
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	u = mem_blkid_get_zones(ctx, id);

	for ( i = 0 ; i < LMZN_MAX ; i++ )
	{
		if ((0x01 << i) & u)
			printf("%s ", mem_lmzn(i));
	}
	printf("\n");

	rv = 0;

	mem_unref(ctx);

end:

	return rv;
}

int cmd_show_blocks(int online, int offline, char *region_name)
{
	int rv;
	struct mem_ctx *ctx;
	struct mem_blk *blk;
	struct cxl_region *region;

	rv = 1;

	// Get mem contex
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	mem_blk_foreach(ctx, blk)
	{
		if (online && !mem_blk_is_online(blk))
			continue;
		
		if (offline && mem_blk_is_online(blk))
			continue;

		region = mem_blk_get_region(blk);

		if (region_name != NULL && region == NULL)
			continue;

		if (region_name != NULL && region != NULL && strcmp(region_name, cxl_region_get_devname(region)))
			continue;

		printf("%d\n", mem_blk_get_id(blk));
	}

	rv = 0;

	mem_unref(ctx);

end:

	return rv;
}

int cmd_show_capacity(int online, int offline, char *region_name, int human)
{
	unsigned long long size;
	double d;
	int i, rv;
	struct mem_ctx *ctx;
	struct cxl_region *region;

	i = 0; 

	// Get mem contex
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	if (region_name == NULL)
	{
		if (online)
			size = mem_system_get_capacity_online(ctx);
		else if (offline)
			size = mem_system_get_capacity_offline(ctx);
		else 
			size = mem_system_get_capacity(ctx);
	}
	else 
	{
		region = mem_get_region(ctx, region_name);
		if (region == NULL)
		{
			fprintf(stderr, "Error: Could not obtain region: %s\n", region_name);
			goto end;
		}

		if (online)
			size = mem_region_get_capacity_online(ctx, region);
		else if (offline)
			size = mem_region_get_capacity_offline(ctx, region);
		else 
			size = mem_region_get_capacity(ctx, region);
	}

	if (human)
	{
		char units[] = {' ', 'K', 'M', 'G', 'T'};
		d = (double) size;
		while ((d > 1024) && i++ < 5)
			d /= 1024;
		printf("%0.2f %c\n", d, units[i]);
	}
	else 
		printf("%llu\n", size);

	rv = 0;

	mem_unref(ctx);

end: 

	return rv;
}

int cmd_show_memdev_interleave_granulariy(char *name)
{
	int rv;
	struct mem_ctx *ctx;
	struct cxl_memdev *memdev;

	// Initialize variables 
	rv = 1;
	ctx = NULL;

	// Validate Inputs 
	if (name == NULL)
	{
		fprintf(stderr, "Error: Missing memdev\n");
		rv = -EINVAL;
		goto end;
	}

	// Get mem context 
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	// Get memdev
	memdev = mem_get_memdev(ctx, name);
	if (memdev == NULL)
	{
		fprintf(stderr, "Error: Could not obtain memdev: %s\n", name);
		rv = 1;
		goto err;
	}

	rv = mem_memdev_get_interleave_granularity(ctx, memdev);
	printf("%d\n", rv);

	rv = 0;

err:

	mem_unref(ctx);

end:

	return rv;
}

int cmd_show_memdev_isavailable(char *name)
{
	int rv;
	struct mem_ctx *ctx;
	struct cxl_memdev *memdev;

	// Initialize variables 
	rv = 1;

	// Validate Inputs 
	if (name == NULL)
	{
		fprintf(stderr, "Error: Missing memdev\n");
		rv = -EINVAL;
		goto end;
	}

	// Get mem context 
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	// Get memdev
	memdev = mem_get_memdev(ctx, name);
	if (memdev == NULL)
	{
		fprintf(stderr, "Error: Could not obtain memdev: %s\n", name);
		rv = 1;
		goto err;
	}

	rv = mem_memdev_is_available(ctx, memdev);
	printf("%d\n", rv);

	rv = 0;

err: 

	mem_unref(ctx);

end:

	return rv;
}

int cmd_show_memdevs(char *memdev_name, char *region_name, int human)
{
	int rv;
	struct mem_ctx *ctx;
	struct cxl_memdev *memdev; 
	struct cxl_endpoint *endpoint;
	struct cxl_port *port; 
	struct cxl_decoder *decoder; 
	struct cxl_region *region;
	int i, num;
	struct cxl_memdev **array;

	rv = 1;
	i = 0;
	array = NULL;

	// Get mem context 
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	// Get the number of memdevs 
	num = mem_num_memdevs(ctx);

	if (opts[CLOP_NUM].set)
	{
		printf("%d\n", num);
		goto end;
	}

	if (num <= 0)
	{
		rv = 0;
		goto end;
	}

	// Get a sorted list of memdevs 
	array = mem_get_memdevs(ctx);
	if (array == NULL)
	{
		fprintf(stderr, "Error: Could not obtain list of memdevs\n");
		rv = 1;
		goto err;
	}

	printf("Name    Enabled    Mode            Size          Host     Endpoint      Decoder        Region  FW Version\n");
	printf("------  -------  ------  --------------  ------------  -----------  -----------  ------------  -------------------\n");
	for ( i = 0 ; i < num ; i++)
	{
		memdev = array[i];

		if (memdev_name != NULL && strcmp(memdev_name, cxl_memdev_get_devname(memdev)))
			continue;

		endpoint = cxl_memdev_get_endpoint(memdev);
		port = cxl_endpoint_get_port(endpoint);
		decoder = cxl_decoder_get_first(port);
		region = cxl_decoder_get_region(decoder);

		int mode = cxl_decoder_get_mode(decoder);
		unsigned long long size	= cxl_memdev_get_ram_size(memdev);
		const char *host = cxl_memdev_get_host(memdev);
		const char *reg_name = NULL;

		if (region != NULL)
		{
			reg_name = cxl_region_get_devname(region);
		}
		else 
			reg_name ="-";

		if (region_name != NULL && strcmp(reg_name, region_name))
			continue;

		printf("%-6s  %7d  %6s  ", 
			cxl_memdev_get_devname(memdev), 
			cxl_memdev_is_enabled(memdev),
			cxl_decoder_mode_name(mode));

		if (human)
		{
			char units[] = {' ', 'K', 'M', 'G', 'T'};
			double d = (double) size;
			int i = 0;
			while ((d > 1024) && i++ < 5)
				d /= 1024;
			printf("%12.2f %c  ", d, units[i]);
		}
		else 
			printf("%14llu  ", size);

		printf("%12s  %11s  %11s  %12s  %-20s\n", 
			host,
			cxl_endpoint_get_devname(endpoint),
			cxl_decoder_get_devname(decoder),
			reg_name,
			cxl_memdev_get_firmware_verison(memdev)
			);
	}

	// Free the sorted array of memdevs 
	if (array != NULL)
		free(array);

	rv = 0;

err:
	
	mem_unref(ctx);

end:

	return rv;
}

int cmd_show_num_blocks(int online, int offline, char *region_name)
{
	int rv, num;
	struct mem_ctx *ctx;
	struct cxl_region *region;

	// Initialize variables
	num = 0;

	// Get mem contex
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	if (region_name != NULL) 
	{
		region = mem_get_region(ctx, region_name);
		if (region == NULL) 
		{
			fprintf(stderr, "Error: Could not obtain region: %s\n", region_name);
			rv = 1;
			goto end;
		}

		if (online)
			num = mem_region_num_blocks_online(ctx, region);
		else if (offline)
			num = mem_region_num_blocks_offline(ctx, region);
		else 
			num = mem_region_num_blocks(ctx, region);
	}
	else 
	{
		if (online)
			num = mem_system_num_blocks_online(ctx);
		else if (offline)
			num = mem_system_num_blocks_offline(ctx);
		else 
			num = mem_system_num_blocks(ctx);
	}

	printf("%d\n", num);

	rv = 0;

	mem_unref(ctx);

end:

	return rv;
}

int cmd_show_num_blocks_offline()
{
	struct mem_ctx *ctx;
	int rv; 

	// Get mem contex
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	printf("%d\n", mem_system_num_blocks_offline(ctx));

	rv = 0;

	mem_unref(ctx);

end:

	return rv;
}

int cmd_show_num_blocks_online()
{
	struct mem_ctx *ctx;
	int rv; 

	// Get mem contex
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	printf("%d\n", mem_system_num_blocks_online(ctx));

	rv = 0;

	mem_unref(ctx);

end:

	return rv;
}

int cmd_show_num_devices()
{
	int rv;
	struct mem_ctx *ctx;

	rv = 1;

	// Get mem context 
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	printf("%d\n", mem_num_memdevs(ctx));

	rv = 0;

	mem_unref(ctx);

end:

	return rv;
}

int cmd_show_num_regions()
{
	int rv;
	struct mem_ctx *ctx;

	rv = 1;

	// Get mem cntext 
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	printf("%d\n", mem_num_regions(ctx));

	rv = 0; 

	mem_unref(ctx);
	
end:

	return rv;
}

int cmd_show_region_blk_state(char *name, int offset)
{
	int rv;
	struct mem_ctx *ctx;
	struct cxl_region *region;

	rv = 1;

	// Validate Inputs 
	if (name == NULL)
	{
		fprintf(stderr, "Error: Missing region\n");
		rv = -EINVAL;
		goto end;
	}
	
	// Validate Inputs 
	if (offset < 0)
	{
		fprintf(stderr, "Error: Missing block index\n");
		rv = -EINVAL;
		goto end;
	}
	
	// Get mem contex
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	region = mem_get_region(ctx, name);
	if (region == NULL)
	{
		fprintf(stderr, "Error: Could not obtain region\n");
		rv = 1;
		goto err;
	}

	rv = mem_region_get_blk_state(ctx, region, offset);
	if (rv < 0)
	{
		fprintf(stderr, "Error: Could not obtain state of block %d in region %s: %d\n", offset, cxl_region_get_devname(region), rv);	
		rv = 1;
		goto err;
	}

	printf("%s\n", mem_lmpl(rv));
	rv = 0;

err:

	mem_unref(ctx);

end:

	return rv;
}

int cmd_show_region_isenabled(char *name)
{
	int rv; 
	struct mem_ctx *ctx;
	struct cxl_region *region;

	rv = 1; 

	// Validate Inputs 
	if (name == NULL)
	{
		fprintf(stderr, "Error: Missing region\n");
		rv = -EINVAL;
		goto end;
	}
	
	// Get mem context 
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	region = mem_get_region(ctx, name);
	if (region == NULL)
	{
		fprintf(stderr, "Error: Could not obtain region: %s\n", name);
		rv = 1;
		goto err;
	}	

	rv = cxl_region_is_enabled(region);
	printf("%d\n", rv);

	rv = 0;

err:

	mem_unref(ctx);

end:

	return rv;
}

int cmd_show_regions(char *name, int human)
{
	int rv;
	struct mem_ctx *ctx;
	struct cxl_decoder *d;
	struct cxl_region **regions, *region;
	struct cxl_memdev *memdev;
	int i, j, num, num_regions;

	rv = 1;

	// Get mem context 
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	num_regions = mem_num_regions(ctx);
	regions = mem_get_regions(ctx);
	
	if (num_regions <= 0 || regions == NULL)
	{
		rv = 0;
		goto err;
	}

	printf("Name       Enabled  Dax    Mode            Size  Ways  Granularity  Num Blocks  Blocks Online  Devices\n");
	printf("---------  -------  ---  ------  --------------  ----  -----------  ----------  -------------  -------\n");

	for ( i = 0 ; i < num_regions ; i++)
	{
		region = regions[i];

		// Skip if a region name was provided and doesn't match this region name 
		if (name != NULL && strcmp(name, cxl_region_get_devname(region)))
			continue;

		printf("%-9s  %7d  %3d  %6s  ", 
			cxl_region_get_devname(region),
			cxl_region_is_enabled(region),
			mem_region_is_daxmode(ctx, region),
			cxl_decoder_mode_name(cxl_region_get_mode(region)));

		if (human)
		{
			char units[] = {' ', 'K', 'M', 'G', 'T'};
			double d = (double) cxl_region_get_size(region);
			int i = 0;
			while ((d > 1024) && i++ < 5)
				d /= 1024;
			printf("%12.2f %c  ", d, units[i]);
		}
		else 
			printf("%14llu  ", cxl_region_get_size(region));

		printf("%4d  %11d  %10d  %13d  ", 
			cxl_region_get_interleave_ways(region),
			cxl_region_get_interleave_granularity(region),
			mem_region_num_blocks(ctx, region),
			mem_region_num_blocks_online(ctx, region)
			);
	
		num = cxl_region_get_interleave_ways(region);
		if (cxl_region_decode_is_committed(region))
		{
			for ( j = 0 ; j < num ; j++)
			{
				d = cxl_region_get_target_decoder(region, j);
				if (d == NULL)
				{
					printf("%d:%s ", j, "-");
					continue;
				}

				memdev = cxl_decoder_get_memdev(d);
				if (memdev != NULL)
					printf("%d:%s ", j, cxl_memdev_get_devname(memdev));
				else 
					printf("%d:%s ", j, "-");
			}
		}
		else 
			printf("      -");
		printf("\n");
	}

	rv = 0;

err:

	mem_unref(ctx);

end:

	return rv;
}

int cmd_show_system_blocksize(int human)
{
	int rv; 
	unsigned long long size;
	struct mem_ctx *ctx;

	// Get mem contex
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	size = mem_system_get_blocksize(ctx);

	if (human)
	{
		char units[] = {' ', 'K', 'M', 'G', 'T'};
		double d = (double) size;
		int i = 0;
		while ((d > 1024) && i++ < 5)
			d /= 1024;
		printf("%0.2f %c\n", d, units[i]);
	}
	else 
		printf("%llu\n", size);

	rv = 0;
	
	mem_unref(ctx);

end:

	return rv;
}

int cmd_show_system_policy()
{
	struct mem_ctx *ctx;
	int rv; 

	// Get mem contex
	rv = mem_new(&ctx);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to obtain mem context: %d\n", rv);
		rv = 1;
		goto end;
	}
	mem_log_set_destination(ctx, CLI_LOG_DST, NULL);
	mem_log_set_priority(ctx, CLI_LOG_LEVEL);

	rv = mem_system_get_policy(ctx);
	printf("%s\n", mem_lmpl(rv));

	rv = 0;

	mem_unref(ctx);

end:

	return rv;
}

int run()
{
	int rv;

	rv = 1;

	switch(opts[CLOP_CMD].val)
	{
		case CLCM_INFO:
			rv = cmd_info();
			break;

		case CLCM_LIST:
			rv = cmd_list(opts[CLOP_ONLINE].set, opts[CLOP_OFFLINE].set, opts[CLOP_REGION].str);
			break;

		case CLCM_BLOCK_ONLINE:
			if (opts[CLOP_ALL].set)
				rv = cmd_blk_online(-1, 0);
			else if (opts[CLOP_BLOCK].num == 0)
				rv = cmd_blk_online(1, opts[CLOP_BLOCK].val);
			else if (opts[CLOP_BLOCK].num >= 0)
				rv = cmd_blk_online(opts[CLOP_BLOCK].num, *((int*)opts[CLOP_BLOCK].buf));
			break;

		case CLCM_BLOCK_OFFLINE:
			if (opts[CLOP_BLOCK].num == 0)
				rv = cmd_blk_offline(1, opts[CLOP_BLOCK].val);
			else if (opts[CLOP_BLOCK].num > 0)
				rv = cmd_blk_offline(opts[CLOP_BLOCK].num, *((int*)opts[CLOP_BLOCK].buf));
			break;

		case CLCM_REGION_CREATE:
			if (opts[CLOP_ALL].set)
				rv = cmd_region_create(opts[CLOP_GRANULARITY].u32, 0, NULL);
			else 
				rv = cmd_region_create(opts[CLOP_GRANULARITY].u32, opts[CLOP_DEVICE].num, (char**)opts[CLOP_DEVICE].buf);
			break;

		case CLCM_REGION_DAXMODE:
			rv = cmd_region_daxmode(opts[CLOP_REGION].str);
			break;

		case CLCM_REGION_RAMMODE:
			rv = cmd_region_rammode(opts[CLOP_REGION].str);
			break;

		case CLCM_REGION_DELETE:
			if (opts[CLOP_ALL].set)
				rv = cmd_region_delete(NULL);
			else 
				rv = cmd_region_delete(opts[CLOP_REGION].str);
			break;

		case CLCM_REGION_DISABLE:
			rv = cmd_region_disable(opts[CLOP_REGION].str);
			break;

		case CLCM_REGION_ENABLE:
			rv = cmd_region_enable(opts[CLOP_REGION].str);
			break;

		case CLCM_SET_BLOCK_STATE:
			if (opts[CLOP_ALL].set && (opts[CLOP_ONLINE].set || opts[CLOP_MOVABLE].set))
				rv = cmd_blk_online(-1, 0);
			else if (opts[CLOP_ONLINE].set || opts[CLOP_MOVABLE].set)
				rv = cmd_set_blk_state(opts[CLOP_BLOCK].val, LMPL_MOVABLE);
			else if (opts[CLOP_KERNEL].set)
				rv = cmd_set_blk_state(opts[CLOP_BLOCK].val, LMPL_KERNEL);
			else
				rv = cmd_set_blk_state(opts[CLOP_BLOCK].val, LMPL_OFFLINE);
			break;

		case CLCM_SET_REGION_BLOCK_STATE:
		{
			int mode;

			if (opts[CLOP_ONLINE].set || opts[CLOP_MOVABLE].set)
				mode = LMPL_MOVABLE;
			else if (opts[CLOP_KERNEL].set)
				mode = LMPL_KERNEL;
			else
				mode = LMPL_OFFLINE;

			if (opts[CLOP_BLOCK].num > 0)
			{
				rv = 0;
				int start = *((int*)opts[CLOP_BLOCK].buf);
				for (int i = 0 ; i < (int) opts[CLOP_BLOCK].num ; i++)
					rv += cmd_region_set_blk_state(opts[CLOP_REGION].str, start+i, mode);
			}
			else if (opts[CLOP_ALL].set)
			{
				rv = cmd_region_set_blk_state(opts[CLOP_REGION].str, -1, mode);
			}
			else
				rv = cmd_region_set_blk_state(opts[CLOP_REGION].str, opts[CLOP_BLOCK].val, mode);
		}
			break;

		case CLCM_SET_SYSTEM_POLICY:
			if (opts[CLOP_ONLINE].set)
				rv = cmd_set_system_policy(LMPL_ONLINE);
			else if (opts[CLOP_MOVABLE].set)
				rv = cmd_set_system_policy(LMPL_MOVABLE);
			else if (opts[CLOP_KERNEL].set)
				rv = cmd_set_system_policy(LMPL_KERNEL);
			else 
				rv = cmd_set_system_policy(LMPL_OFFLINE);
			break;

		case CLCM_SHOW_BLK_ISONLINE:
			rv = cmd_show_blk_isonline(opts[CLOP_BLOCK].val);
			break;

		case CLCM_SHOW_BLK_ISREMOVABLE:
			rv = cmd_show_blk_isremovable(opts[CLOP_BLOCK].val);
			break;

		case CLCM_SHOW_BLK_NODE:
			rv = cmd_show_blk_node(opts[CLOP_BLOCK].val);
			break;

		case CLCM_SHOW_BLK_PHYSDEVICE:
			rv = cmd_show_blk_node(opts[CLOP_BLOCK].val);
			break;

		case CLCM_SHOW_BLK_STATE:
			if (opts[CLOP_REGION].set)
				rv = cmd_show_region_blk_state(opts[CLOP_REGION].str, opts[CLOP_BLOCK].val);
			else 
				rv = cmd_show_blk_state(opts[CLOP_BLOCK].val);
			break;

		case CLCM_SHOW_BLK_ZONES:
			rv = cmd_show_blk_zones(opts[CLOP_BLOCK].val);
			break;

		case CLCM_SHOW_BLOCKS:
			rv = cmd_show_blocks(opts[CLOP_ONLINE].set, opts[CLOP_OFFLINE].set, opts[CLOP_REGION].str);
			break;

		case CLCM_SHOW_CAPACITY:
			rv = cmd_show_capacity(opts[CLOP_ONLINE].set, opts[CLOP_OFFLINE].set, opts[CLOP_REGION].str, opts[CLOP_HUMAN].set);
			break;

		case CLCM_SHOW_DEVICES:
			rv = cmd_show_memdevs(opts[CLOP_DEVICE].str, opts[CLOP_REGION].str, opts[CLOP_HUMAN].set);
			break;

		case CLCM_SHOW_DEVICE_ISAVAILABLE:
			rv = cmd_show_memdev_isavailable(opts[CLOP_DEVICE].str);
			break;

		case CLCM_SHOW_DEVICE_INTERLEAVE_GRANULARITY:
			rv = cmd_show_memdev_interleave_granulariy(opts[CLOP_DEVICE].str);
			break;

		case CLCM_SHOW_REGIONS:
			rv = cmd_show_regions(opts[CLOP_REGION].str, opts[CLOP_HUMAN].set);
			break;

		case CLCM_SHOW_NUM_BLOCKS:
			rv = cmd_show_num_blocks(opts[CLOP_ONLINE].set, opts[CLOP_OFFLINE].set, opts[CLOP_REGION].str);
			break;

		case CLCM_SHOW_NUM_DEVICES:
			rv = cmd_show_num_devices();
			break;

		case CLCM_SHOW_NUM_REGIONS:
			rv = cmd_show_num_regions();
			break;

		case CLCM_SHOW_REGION_ISENABLED:
			rv = cmd_show_region_isenabled(opts[CLOP_REGION].str);
			break;

		case CLCM_SHOW_SYSTEM_BLOCKSIZE:
			rv = cmd_show_system_blocksize(opts[CLOP_HUMAN].set);;
			break;

		case CLCM_SHOW_SYSTEM_POLICY:
			rv = cmd_show_system_policy();
			break;

		default: 
			rv = 1;
			break;		
	}

	return rv;
}

int main(int argc, char *argv[])
{
	int rv;

	// Initialize variables 
	rv = 1;

	// Parse options 
	rv = options_parse(argc, argv);
	if (rv != 0)
	{
		fprintf(stderr, "Error: Failed to parse command line parameters: %d\n", rv);
		goto end;
	}

	// Verify command was specified
	if (opts[CLOP_CMD].set == 0)
	{
		fprintf(stderr, "Error: No command specified\n");
		rv = 1;
		goto err;	
	}

	// Execute command
	rv = run();
	if (rv != 0)
		fprintf(stderr, "Error: Command failed: %d\n", rv);

err:

	options_free();

end:

	return rv;
}

