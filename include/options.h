/**
 * @file 		options.h 
 *
 * @brief 		Header file for CLI options
 *
 * @copyright 	Copyright (C) 2024 Jackrabbit Founders LLC. All rights reserved.
 *
 * @date 		Jul 2024
 * @author 		Barrett Edwards
 *
 * Macro / Enumeration Prefixes (CL)
 * CLAP - CLI Options Parsers Enumeration (AP)
 * CLCM - CLI Command Opcod (CM)
 * CLMR - CLI Macros (MR)
 * CLOP	- CLI Option (CL)
 * CLPC - Physical Port Control Opcodes (PC)
 * CLPU - Port Unbind Mode Options (PU)
 * 
 * Key mapping 
 * -o --offline			Zone: offline 
 * -O --online 			online 
 * -a --all 			Perform operatiion on all objects 
 * -b --block 			Block id 
 * -d --device 			Memdev name 
 * -g --granularity 	Interleave Granularity 
 * -h --help 			Display Help
 * -H --human			Display numbers using K, M, G, T units 
 * -k --kernel 			Zone: kernel 
 * -m --movable 		Zone: Online movable
 * -n --num 			Display the number of objects 
 * -r --region 			Region name 
 * -v --verbose 		Increase verbosity 
 * 
 * Non char key mapping
 * 701 - usage
 * 702 - version
 * 703 - data
 * 704 - infile
 * 705 - outfile
 * 706 - print-options
 * 
 */

#ifndef _OPTIONS_H
#define _OPTIONS_H

/* INCLUDES ==================================================================*/

/* __u8
 */
#include <linux/types.h> 

/* MACROS ====================================================================*/

/**
 * CLI Macros (MR)
 *
 * These are values that don't belong in an enumeration
 */
#define CLMR_HELP_COLUMN 		30
#define CLMR_MAX_HELP_WIDTH 	100
#define CLMR_MAX_NAME_LEN 		64

/* ENUMERATIONS ==============================================================*/

/** 
 * Verbosity Options (VO)
 */
enum _CLVO 
{
	CLVO_GENERAL	= 0,
	CLVO_MAX
};

/** 
 * Verbosity Bitfield Index (VB)
 */
enum _CLVB
{
	CLVB_GENERAL	= (0x01 << 0),
};

/**
 * CLI Options Parsers Enumeration (AP)
 *
 * This enumeration identifies each argp parser. It is used to print options 
 */
enum _CLAP 
{
	CLAP_MAIN 					= 0,

	CLAP_BLOCK					,
	CLAP_LIST					,
	CLAP_REGION					,
	CLAP_SET  					,
	CLAP_SHOW 					,

	CLAP_SHOW_BLOCK				,
	CLAP_SHOW_CAPACITY			,
	CLAP_SHOW_DEVICE			,
	CLAP_SHOW_NUM    			,
	CLAP_SHOW_REGION			,
	CLAP_SHOW_SYSTEM			,

	CLAP_MAX
};

/**
 * CLI Command Opcode (CM)
 */
enum _CLCM 
{
	CLCM_NULL 									= 0,

	CLCM_INFO 									,
	CLCM_LIST 									,

	CLCM_BLOCK_ONLINE 							, 
	CLCM_BLOCK_OFFLINE 							, 

	CLCM_SET_BLOCK_STATE 						, 
	CLCM_SET_REGION_BLOCK_STATE 				, 
	CLCM_SET_SYSTEM_POLICY 						, 

	CLCM_SHOW_REGIONS 							,
	CLCM_SHOW_BLOCKS 							,
	CLCM_SHOW_DEVICES 							,

	CLCM_SHOW_CAPACITY 							,

	CLCM_SHOW_NUM_BLOCKS						,
	CLCM_SHOW_NUM_DEVICES						,
	CLCM_SHOW_NUM_REGIONS						,

	CLCM_SHOW_SYSTEM_BLOCKSIZE 					, 
	CLCM_SHOW_SYSTEM_POLICY 					, 

	CLCM_SHOW_BLK_ISONLINE 		    			,
	CLCM_SHOW_BLK_ISREMOVABLE 	    			,
	CLCM_SHOW_BLK_NODE 							,
	CLCM_SHOW_BLK_PHYSDEVICE 					,
	CLCM_SHOW_BLK_STATE 						,
	CLCM_SHOW_BLK_ZONES 						,

	CLCM_SHOW_REGION_ISENABLED      			,
	CLCM_SHOW_DEVICE_ISAVAILABLE    			,
	CLCM_SHOW_DEVICE_INTERLEAVE_GRANULARITY 	,

	CLCM_REGION_CREATE 							,
	CLCM_REGION_DAXMODE							,
	CLCM_REGION_DELETE 							,
	CLCM_REGION_DISABLE							,
	CLCM_REGION_ENABLE 							,
	CLCM_REGION_RAMMODE							,

	CLCM_MAX
};

/**
 * CLI Option (OP)
 */
enum _CLOP
{	
	/* General CLI Options */
	CLOP_VERBOSITY 			= 0,	//!< Count <u32>
	CLOP_PRNT_OPTS 			= 1, 	//!< Print Options Array when completed parsing <set>
	CLOP_INFILE 			= 2, 	//!< Filename for input <str,buf,len>
	CLOP_OUTFILE			= 3,	//!< Filename for output <str>
	CLOP_NUM 				= 4, 	//!< Number of items <u8>
	CLOP_ALL 				= 5,	//!< Perform aciton on all of collection <set>
	CLOP_LEN				= 6,	//!< Length of data parameter <len>
	CLOP_LIMIT				= 7, 	//!< Message Response Limit <u8>
	CLOP_CMD 				= 8,	//!< Command to RUN <val>
	CLOP_DATA 				= 9,	//!< Immediate Data for Write transaction <u32>
	CLOP_HUMAN 				= 10,	//!< Human readable values (K, M, G, T) <set> 

	CLOP_DEVICE         	= 11,	//!< Device name <str>, <u32>
	CLOP_REGION         	= 12,	//!< Region name <str>, <u32>
	CLOP_BLOCK         		= 13,	//!< Block id <val>
	CLOP_GRANULARITY   		= 14,	//!< Memory Interleave Granularity <u32>
	CLOP_ZONE          		= 15,	//!< Memory Zone <u32> [CLZN]
	CLOP_ONLINE        		= 16,	//!< Online action <set>
	CLOP_OFFLINE       		= 17,	//!< Offline action <set>
	CLOP_KERNEL       		= 18,	//!< Online to kernel normal zone 
	CLOP_MOVABLE      		= 19,	//!< Online to zone movable 

	CLOP_BLOCKS       		= 20,	//!< Show Blocks <set>
	CLOP_DEVICES      		= 21,	//!< Show Devices <set>
	CLOP_REGIONS      		= 22,	//!< Show Regions <set>

	CLOP_MAX
};

/* Valid Zone options  */
enum _CLZN 
{
	CLZN_DMA 			= 0, 
	CLZN_DMA32 			= 1, 
	CLZN_NORMAL 		= 2,
	CLZN_MOVABLE 		= 3, 
	CLZN_NONE 			= 4,
	CLZN_MAX
};

/* STRUCTS ===================================================================*/

/**
 * CLI Option Struct
 *
 * Each command line parameter is stored in one of these objects
 */
struct opt
{
	int 			set;	//!< Not set (0), set (1) 
	__u8 			u8; 	//!< Unsigned char value
	__u16 			u16; 	//!< Unsigned long value
	__u32 			u32;	//!< Unsigned long value 
	__u64 			u64;	//!< Unsigned long long value 
	__s32 	 		val;	//!< Generic signed value
	__u64 			num;	//!< Number of items 
	__u64 			len;	//!< Data Buffer Length 
	char 			*str;	//!< String value 
	__u8 			*buf;	//!< Data buffer 
};

/* GLOBAL VARIABLES ==========================================================*/

/**
 * Global varible to store parsed CLI options
 */
extern struct opt *opts;

/* PROTOTYPES ================================================================*/

/**
 * Free allocated memory by option parsing proceedure
 *
 * @return 0 upon success. Non zero otherwise
 */
int options_free();

/**
 * Parse command line options 
 */
int options_parse(int argc, char *argv[]);

#endif //ifndef _OPTIONS_H
