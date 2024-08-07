/**
 * @file 		options.c
 *
 * @brief 		Code file for CLI options
 *
 * @copyright 	Copyright (C) 2024 Jackrabbit Founders LLC. All rights reserved.
 *
 * @date 		Jul 2024
 * @author 		Barrett Edwards
 *
 */

/* INCLUDES ==================================================================*/

/* memset()
 * memcpy()
 * strdup()
 * strlen()
 * strnlen()
 * strstr()
 * strcmp()
 * strncmp()
 */
#include <string.h> 

/* free()
 * calloc()
 * malloc()
 * realloc()
 * strtoul()
 * exit()
 * getenv()
 */
#include <stdlib.h>

/* struct argp
 * struct argp_state
 * argp_parse()
 */
#include <argp.h>

#include "options.h"

/* MACROS ====================================================================*/

#define DEFAULT_WIDTH 	16	//!< Default width for printing __u8 buffers 
#define MIN_WIDTH 		4	//!< Minium width for printing buffers

/* ENUMERATIONS ==============================================================*/

/* STRUCTS ===================================================================*/

/**
 * Local struct used to define shell environment variables and the key to parse them
 */
struct envopt
{
	char key;
	char *name;
};

/* PROTOTYPES ================================================================*/

static void print_help(int option);
static void print_options(struct argp_option *o);
static void print_usage(int option, struct argp_option *o);

static int pr_main 			(int key, char *arg, struct argp_state *state);

static int pr_block			(int key, char *arg, struct argp_state *state);
static int pr_list 			(int key, char *arg, struct argp_state *state);
static int pr_region		(int key, char *arg, struct argp_state *state);
static int pr_set			(int key, char *arg, struct argp_state *state);
static int pr_show			(int key, char *arg, struct argp_state *state);

static int pr_set_block		(int key, char *arg, struct argp_state *state);
static int pr_set_region	(int key, char *arg, struct argp_state *state);
static int pr_set_system	(int key, char *arg, struct argp_state *state);

static int pr_show_block	(int key, char *arg, struct argp_state *state);
static int pr_show_capacity (int key, char *arg, struct argp_state *state);
static int pr_show_device   (int key, char *arg, struct argp_state *state);
static int pr_show_num      (int key, char *arg, struct argp_state *state);
static int pr_show_region   (int key, char *arg, struct argp_state *state);
static int pr_show_system   (int key, char *arg, struct argp_state *state);

/* GLOBAL VARIABLES ==========================================================*/

/**
 * Global varible to store parsed CLI options
 */
struct opt *opts;

const char *argp_program_version = "version 0.1";

/**
 * String representation of CLOP Enumeration 
 */ 
char *CLOP[] = {
	"VERBOSITY",			
	"PRNT_OPTS",			
	"INFILE",			
	"OUTFILE",
	"NUM",
	"ALL",				
	"LEN",				
	"LIMIT",
	"CMD",				
	"DATA",
	"HUMAN",
	"DEVICE",
	"REGION",
	"BLOCK",
	"GRANULARITY",
	"ZONE",
	"ONLINE",
	"OFFLINE",
	"KERNEL",
	"MOVABLE",
	"BLOCKS",
	"DEVICES",
	"REGIONS"
};


/* Help Output strings */ 
const char *ho_main = "\n\
Usage: mem <options> [[subcommand] <subcommand options>. . .] \n\n\
Subcommands: \n\
  block                       Perform actions on a memory block(s) \n\
  info                        Display information about memory system \n\
  list                        List memory blocks \n\
  region                      Perform actions on a memory region \n\
  set                         Configure a component or sytem setting \n\
  show                        Display information \n\
";

const char *ho_block = "\n\
Usage: mem block <id> [<subcommand> <options>] \n\n\
Subcommands: \n\
  online                      Online a memory block \n\
  offline                     Offline a memory block \n\
  kernel                      Online a memory block to zone normal \n\
  movable                     Online a memory block to zone movable \n\
";

const char *ho_list = "\n\
Usage: mem list [<subcommand> <options>] \n\n\
Filters. These filter the data to include only the desired qualifier: \n\
  offline                     Show offline blocks \n\
  online                      Show online blocks \n\
  <region>                    Show blocks of a region \n\
";

const char *ho_region = "\n\
Usage: mem region [<subcommand> <region name> <options>] \n\n\
Subcommands: \n\
  create <devices>            Create a region from memory devices (mem0 mem1 ... ) \n\
  delete <region>             Delete a region \n\
  disable <region>            Disable a region \n\
  enable <region>             Enable a region \n\
  daxmode <region>            Enable DAX mode of a region \n\
  rammode <region>            Enable RAM mode of a region (default)\n\
";

const char *ho_set = "\n\
Usage: mem set [subcommand <options>] \n\n\
Subcommands: \n\
  policy <policy>             Set system memory online policy \n\
";

const char *ho_show = "\n\
Usage: mem show [subcommand <options>] \n\n\
Subcommands: \n\
  block                       State of memory blocks \n\
  capacity                    Show memory capacity \n\
  device                      List of memory devices \n\
  num                         Count of items \n\
  region                      List of memory regions \n\
  system                      Memory System values \n\
";

const char *ho_show_block = "\n\
Usage: mem show block <id> [subcommand <options>] \n\n\
Single memory block Subcommands. Requires <id> to be specified: \n\
  device                      Show physical device of a specific block \n\
  isonline                    Show if a specific block is online \n\
  isremovable                 Show if a specific block is removable \n\
  node                        Show the CPU node of a specific block  \n\
  state                       Show the state of a specific memory block \n\
  zones                       Show the valid zones of a specific memory block \n\
 \n\
Filters. These filter the data to include only the desired qualifier:\n\
  offline                     Show offline memory blocks \n\
  online                      Show online memory blocks \n\
  <region>                    Show memory blocks from region \n\
";

const char *ho_show_capacity = "\n\
Usage: mem show capacity [subcommand <options>] \n\n\
Filters. These filter the data to include only the desired qualifier: \n\
  offline                     Show capacity of offline blocks \n\
  online                      Show capacity of online blocks \n\
  <region>                    Show capacity of a region \n\
";

const char *ho_show_device = "\n\
Usage: mem show device [subcommand <options>] \n\n\
Subcommands: \n\
  ig                          Show device interleave granularity \n\
  isavailable                 Show if device is not currently part of a region \n\n\
Filters. These filter the data to include only the desired qualifier: \n\
  <region>                    Show devices that are part of a region \n\
  <mem>                       Show device that matches this name \n\
";

const char *ho_show_num = "\n\
Usage: mem show num <object to count> <options> \n\n\
Objects to count: \n\
  blocks                      Show number of blocks \n\
  devices                     Show number of devices \n\
  regions                     Show number of regions \n\n\
Filters. These filter the data to include only the desired qualifier: \n\
  offline                     Show number of offline blocks \n\
  online                      Show number of online blocks \n\
  <region>                    Show number of blocks that are part of a region \n\
";

const char *ho_show_region = "\n\
Usage: mem show region [subcommand <options>] \n\n\
Filters. These filter the data to include only the desired qualifier: \n\
  <region>                    Show this region \n\
";

const char *ho_show_system = "\n\
Usage: mem show system [subcommand <options>] \n\n\
Subcommands: \n\
  blocksize                   Show system block size \n\
  policy                      Show system auto online policy \n\
";


/**
 * Global array of CLI options to pull from the shell environment if present
 */
struct envopt envopts[] = 
{
	{0,0}
};

/**
 * Global char pointer to dynamically store the name of the application 
 *
 * This is allocated and stored when parse_options() is called
 */
static char *app_name;

/**
 *  CLAP_MAIN - mem
 */
struct argp_option ao_main[] =						
{
	{0,                              0, 	0, 		OPTION_HIDDEN, 	"Command options", 						1}, 

	{0,                              0, 	0, 		OPTION_HIDDEN, 	"Object options", 						3},	
  	{"block",                      'b', 	"INT", 	OPTION_HIDDEN, 	"Memory Block ID (e.g. 306)", 			0},	
  	{"device",                     'd', 	"STR", 	OPTION_HIDDEN, 	"Device name (e.g. mem0)", 				0},	
  	{"region",                     'r', 	"STR", 	OPTION_HIDDEN, 	"Region name (e.g. region0)", 			0},	
  	{"interleave",                 'g', 	"INT", 	OPTION_HIDDEN, 	"Interleave Granularity (e.g. 4096)", 	0},	

	{0,                              0, 	0,		OPTION_HIDDEN, 	"State options", 						5},
  	{"offline",                    '0', 	NULL,  	OPTION_HIDDEN, 	"Offline object", 						0},	
  	{"online",                     '1', 	NULL,  	OPTION_HIDDEN, 	"Online object", 						0},	
  	{"kernel",                     'k', 	NULL,  	OPTION_HIDDEN, 	"Zone normal/kernel", 					0},	
  	{"movable",                    'm', 	NULL,  	OPTION_HIDDEN, 	"Zone movable", 						0},	

	{0,                              0,        0,  	OPTION_HIDDEN,	"Output options",						7},
  	{"human",                      'H', 	NULL, 	OPTION_HIDDEN, 	"Human readable output (K, M, G, T)", 	0},	
  	{"num",                        'n', 	NULL, 	OPTION_HIDDEN, 	"Dsipaly the number of items", 			0},	
  	{"all",                        'a', 	NULL, 	OPTION_HIDDEN, 	"Perform action on all items", 			0},	

	{0,                              0, 	0,		0, 				"Help options", 						9},
  	{"help",                       'h',  	NULL, 	0, 				"Display Help", 						0},
  	{"usage",                      701,  	NULL, 	0, 				"Display Usage", 						0},	
  	{"version",                    702,  	NULL, 	0, 				"Display Version", 						0},
  	{"print-options",              706,  	NULL, 	OPTION_HIDDEN,	"Print options array", 					0},

	{0,0,0,0,0,0} // Final option should be all null
};

/**
 *  CLAP_BLOCK - mem block
 */
struct argp_option ao_block[] =						
{
	{0,                              0, 	0, 		OPTION_HIDDEN, 	"Command options", 						1}, 

	{0,                              0, 	0, 		0,             	"Object options", 						3},	
  	{"block",                      'b', 	"INT", 	0, 				"Memory Block ID (e.g. 306)", 			0},	
  	{"device",                     'd', 	"STR", 	OPTION_HIDDEN, 	"Device name (e.g. mem0)", 				0},	
  	{"region",                     'r', 	"STR", 	0,             	"Region name (e.g. region0)", 			0},	
  	{"interleave",                 'g', 	"INT", 	OPTION_HIDDEN, 	"Interleave Granularity (e.g. 4096)", 	0},	
  	{"all",                        'a', 	NULL, 	0, 				"Perform action on all items", 			0},	

	{0,                              0, 	0,		0, 				"State options", 						5},
  	{"offline",                    '0', 	NULL,  	0, 				"Offline object", 						0},	
  	{"online",                     '1', 	NULL,  	0, 				"Online object", 						0},	
  	{"kernel",                     'k', 	NULL,  	0, 				"Zone normal/kernel", 					0},	
  	{"movable",                    'm', 	NULL,  	0, 				"Zone movable", 						0},	

	{0,                              0,        0,  	OPTION_HIDDEN,	"Output options",						7},
  	{"human",                      'H', 	NULL, 	OPTION_HIDDEN, 	"Human readable output (K, M, G, T)", 	0},	
  	{"num",                        'n', 	NULL, 	OPTION_HIDDEN, 	"Dsipaly the number of items", 			0},	

	{0,                              0, 	0,		0, 				"Help options", 						9},
  	{"help",                       'h',  	NULL, 	0, 				"Display Help", 						0},
  	{"usage",                      701,  	NULL, 	0, 				"Display Usage", 						0},	
  	{"version",                    702,  	NULL, 	0, 				"Display Version", 						0},
  	{"print-options",              706,  	NULL, 	OPTION_HIDDEN,	"Print options array", 					0},

	{0,0,0,0,0,0} // Final option should be all null
};

/**
 *  CLAP_LIST - mem list
 */
struct argp_option ao_list[] =						
{
	{0,                              0, 	0, 		OPTION_HIDDEN, 	"Command options", 						1}, 

	{0,                              0, 	0, 		OPTION_HIDDEN, 	"Filter options", 						3},	
  	{"block",                      'b', 	"INT", 	OPTION_HIDDEN, 	"Memory Block ID (e.g. 306)", 			0},	
  	{"device",                     'd', 	"STR", 	OPTION_HIDDEN, 	"Device name (e.g. mem0)", 				0},	
  	{"region",                     'r', 	"STR", 	0,             	"Region name (e.g. region0)", 			0},	
  	{"interleave",                 'g', 	"INT", 	OPTION_HIDDEN, 	"Interleave Granularity (e.g. 4096)", 	0},	
  	{"offline",                    '0', 	NULL,  	0,             	"List Offline blocks",					0},	
  	{"online",                     '1', 	NULL,  	0,             	"List Online blocks", 					0},	

	{0,                              0, 	0,		OPTION_HIDDEN, 	"State options", 						5},
  	{"kernel",                     'k', 	NULL,  	OPTION_HIDDEN, 	"Zone normal/kernel", 					0},	
  	{"movable",                    'm', 	NULL,  	OPTION_HIDDEN, 	"Zone movable", 						0},	

	{0,                              0,        0,  	0,              "Output options",						7},
  	{"human",                      'H', 	NULL, 	0,              "Human readable output (K, M, G, T)", 	0},	
  	{"num",                        'n', 	NULL, 	OPTION_HIDDEN, 	"Dsipaly the number of items", 			0},	
  	{"all",                        'a', 	NULL, 	OPTION_HIDDEN, 	"Perform action on all items", 			0},	

	{0,                              0, 	0,		0, 				"Help options", 						9},
  	{"help",                       'h',  	NULL, 	0, 				"Display Help", 						0},
  	{"usage",                      701,  	NULL, 	0, 				"Display Usage", 						0},	
  	{"version",                    702,  	NULL, 	0, 				"Display Version", 						0},
  	{"print-options",              706,  	NULL, 	OPTION_HIDDEN,	"Print options array", 					0},

	{0,0,0,0,0,0} // Final option should be all null
};


/**
 *  CLAP_REGION - mem region
 */
struct argp_option ao_region[] =						
{
	{0,                              0, 	0, 		OPTION_HIDDEN, 	"Command options", 						1}, 

	{0,                              0, 	0, 		0,             	"Create options", 						3},	
  	{"block",                      'b', 	"INT", 	OPTION_HIDDEN, 	"Memory Block ID (e.g. 306)", 			0},	
  	{"device",                     'd', 	"STR", 	OPTION_HIDDEN, 	"Device name (e.g. mem0)", 				0},	
  	{"region",                     'r', 	"STR", 	OPTION_HIDDEN, 	"Region name (e.g. region0)", 			0},	
  	{"interleave",                 'g', 	"INT", 	0,             	"Interleave Granularity (Default 4096)",0},	
  	{"all",                        'a', 	NULL, 	0,             	"Use all memory devices", 				0},	

	{0,                              0, 	0,		OPTION_HIDDEN, 	"State options", 						5},
  	{"offline",                    '0', 	NULL,  	OPTION_HIDDEN, 	"Offline object", 						0},	
  	{"online",                     '1', 	NULL,  	OPTION_HIDDEN, 	"Online object", 						0},	
  	{"kernel",                     'k', 	NULL,  	OPTION_HIDDEN, 	"Zone normal/kernel", 					0},	
  	{"movable",                    'm', 	NULL,  	OPTION_HIDDEN, 	"Zone movable", 						0},	

	{0,                              0,        0,  	OPTION_HIDDEN,	"Output options",						7},
  	{"human",                      'H', 	NULL, 	OPTION_HIDDEN, 	"Human readable output (K, M, G, T)", 	0},	
  	{"num",                        'n', 	NULL, 	OPTION_HIDDEN, 	"Dsipaly the number of items", 			0},	

	{0,                              0, 	0,		0, 				"Help options", 						9},
  	{"help",                       'h',  	NULL, 	0, 				"Display Help", 						0},
  	{"usage",                      701,  	NULL, 	0, 				"Display Usage", 						0},	
  	{"version",                    702,  	NULL, 	0, 				"Display Version", 						0},
  	{"print-options",              706,  	NULL, 	OPTION_HIDDEN,	"Print options array", 					0},

	{0,0,0,0,0,0} // Final option should be all null
};

/**
 *  CLAP_SET - mem set
 */
struct argp_option ao_set[] =						
{
	{0,                              0, 	0, 		OPTION_HIDDEN, 	"Command options", 						1}, 

	{0,                              0, 	0, 		OPTION_HIDDEN, 	"Object options", 						3},	
  	{"block",                      'b', 	"INT", 	OPTION_HIDDEN, 	"Memory Block ID (e.g. 306)", 			0},	
  	{"device",                     'd', 	"STR", 	OPTION_HIDDEN, 	"Device name (e.g. mem0)", 				0},	
  	{"region",                     'r', 	"STR", 	OPTION_HIDDEN, 	"Region name (e.g. region0)", 			0},	
  	{"interleave",                 'g', 	"INT", 	OPTION_HIDDEN, 	"Interleave Granularity (e.g. 4096)", 	0},	

	{0,                              0, 	0,		0, 				"Policy options", 						5},
  	{"offline",                    '0', 	NULL,  	0, 				"Default new memory blocks to offline", 0},	
  	{"online",                     '1', 	NULL,  	0, 				"Default new memory blocks to online",	0},	
  	{"kernel",                     'k', 	NULL,  	0, 				"Default new memory blocks to normal",	0},	
  	{"movable",                    'm', 	NULL,  	0, 				"Default new memory blocks to movable", 0},	

	{0,                              0,        0,  	OPTION_HIDDEN,	"Output options",						7},
  	{"human",                      'H', 	NULL, 	OPTION_HIDDEN, 	"Human readable output (K, M, G, T)", 	0},	
  	{"num",                        'n', 	NULL, 	OPTION_HIDDEN, 	"Dsipaly the number of items", 			0},	
  	{"all",                        'a', 	NULL, 	OPTION_HIDDEN, 	"Perform action on all items", 			0},	

	{0,                              0, 	0,		0, 				"Help options", 						9},
  	{"help",                       'h',  	NULL, 	0, 				"Display Help", 						0},
  	{"usage",                      701,  	NULL, 	0, 				"Display Usage", 						0},	
  	{"version",                    702,  	NULL, 	0, 				"Display Version", 						0},
  	{"print-options",              706,  	NULL, 	OPTION_HIDDEN,	"Print options array", 					0},

	{0,0,0,0,0,0} // Final option should be all null
};

/**
 *  CLAP_SHOW - mem show
 */
struct argp_option ao_show[] =						
{
	{0,                              0, 	0, 		OPTION_HIDDEN, 	"Command options", 						1}, 

	{0,                              0, 	0, 		OPTION_HIDDEN, 	"Object options", 						3},	
  	{"block",                      'b', 	"INT", 	OPTION_HIDDEN, 	"Memory Block ID (e.g. 306)", 			0},	
  	{"device",                     'd', 	"STR", 	OPTION_HIDDEN, 	"Device name (e.g. mem0)", 				0},	
  	{"region",                     'r', 	"STR", 	OPTION_HIDDEN, 	"Region name (e.g. region0)", 			0},	
  	{"interleave",                 'g', 	"INT", 	OPTION_HIDDEN, 	"Interleave Granularity (e.g. 4096)", 	0},	

	{0,                              0, 	0,		OPTION_HIDDEN, 	"State options", 						5},
  	{"offline",                    '0', 	NULL,  	OPTION_HIDDEN, 	"Offline object", 						0},	
  	{"online",                     '1', 	NULL,  	OPTION_HIDDEN, 	"Online object", 						0},	
  	{"kernel",                     'k', 	NULL,  	OPTION_HIDDEN, 	"Zone normal/kernel", 					0},	
  	{"movable",                    'm', 	NULL,  	OPTION_HIDDEN, 	"Zone movable", 						0},	

	{0,                              0,        0,  	OPTION_HIDDEN,	"Output options",						7},
  	{"human",                      'H', 	NULL, 	OPTION_HIDDEN, 	"Human readable output (K, M, G, T)", 	0},	
  	{"num",                        'n', 	NULL, 	OPTION_HIDDEN, 	"Dsipaly the number of items", 			0},	
  	{"all",                        'a', 	NULL, 	OPTION_HIDDEN, 	"Perform action on all items", 			0},	

	{0,                              0, 	0,		0, 				"Help options", 						9},
  	{"help",                       'h',  	NULL, 	0, 				"Display Help", 						0},
  	{"usage",                      701,  	NULL, 	0, 				"Display Usage", 						0},	
  	{"version",                    702,  	NULL, 	0, 				"Display Version", 						0},
  	{"print-options",              706,  	NULL, 	OPTION_HIDDEN,	"Print options array", 					0},

	{0,0,0,0,0,0} // Final option should be all null
};

/**
 *  CLAP_SHOW_BLOCK - mem show block 
 */
struct argp_option ao_show_block[] =
{
	{0,                              0, 	0, 		OPTION_HIDDEN, 	"Command options", 						1}, 

	{0,                              0, 	0, 		OPTION_HIDDEN, 	"Object options", 						3},	
  	{"device",                     'd', 	"STR", 	OPTION_HIDDEN, 	"Device name (e.g. mem0)", 				0},	
  	{"interleave",                 'g', 	"INT", 	OPTION_HIDDEN, 	"Interleave Granularity (e.g. 4096)", 	0},	

	{0,                              0, 	0,		OPTION_HIDDEN, 	"Result Filters", 						5},
  	{"block",                      'b', 	"INT", 	0,             	"Memory Block ID (e.g. 306)", 			0},	
  	{"offline",                    '0', 	NULL,  	0,             	"Offline object", 						0},	
  	{"online",                     '1', 	NULL,  	0,             	"Online object", 						0},	
  	{"region",                     'r', 	"STR", 	0,             	"Region name (e.g. region0)", 			0},	
  	{"kernel",                     'k', 	NULL,  	OPTION_HIDDEN, 	"Zone normal/kernel", 					0},	
  	{"movable",                    'm', 	NULL,  	OPTION_HIDDEN, 	"Zone movable", 						0},	

	{0,                              0,        0,  	OPTION_HIDDEN,	"Output options",						7},
  	{"human",                      'H', 	NULL, 	OPTION_HIDDEN, 	"Human readable output (K, M, G, T)", 	0},	
  	{"num",                        'n', 	NULL, 	OPTION_HIDDEN, 	"Dsipaly the number of items", 			0},	
  	{"all",                        'a', 	NULL, 	OPTION_HIDDEN, 	"Perform action on all items", 			0},	

	{0,                              0, 	0,		0, 				"Help options", 						9},
  	{"help",                       'h',  	NULL, 	0, 				"Display Help", 						0},
  	{"usage",                      701,  	NULL, 	0, 				"Display Usage", 						0},	
  	{"version",                    702,  	NULL, 	0, 				"Display Version", 						0},
  	{"print-options",              706,  	NULL, 	OPTION_HIDDEN,	"Print options array", 					0},

	{0,0,0,0,0,0} // Final option should be all null
};

/**
 *  CLAP_SHOW_CAPACITY - mem show capacity 
 */
struct argp_option ao_show_capacity[] =						
{
	{0,                              0, 	0, 		OPTION_HIDDEN, 	"Command options", 						1}, 

	{0,                              0, 	0, 		OPTION_HIDDEN, 	"Filter options", 						3},	
  	{"block",                      'b', 	"INT", 	OPTION_HIDDEN, 	"Memory Block ID (e.g. 306)", 			0},	
  	{"device",                     'd', 	"STR", 	OPTION_HIDDEN, 	"Device name (e.g. mem0)", 				0},	
  	{"region",                     'r', 	"STR", 	0,             	"Region name (e.g. region0)", 			0},	
  	{"interleave",                 'g', 	"INT", 	OPTION_HIDDEN, 	"Interleave Granularity (e.g. 4096)", 	0},	
  	{"offline",                    '0', 	NULL,  	0,             	"Offline capacity",						0},	
  	{"online",                     '1', 	NULL,  	0,             	"Online capacity", 						0},	

	{0,                              0, 	0,		OPTION_HIDDEN, 	"State options", 						5},
  	{"kernel",                     'k', 	NULL,  	OPTION_HIDDEN, 	"Zone normal/kernel", 					0},	
  	{"movable",                    'm', 	NULL,  	OPTION_HIDDEN, 	"Zone movable", 						0},	

	{0,                              0,        0,  	0,              "Output options",						7},
  	{"human",                      'H', 	NULL, 	0,              "Human readable output (K, M, G, T)", 	0},	
  	{"num",                        'n', 	NULL, 	OPTION_HIDDEN, 	"Dsipaly the number of items", 			0},	
  	{"all",                        'a', 	NULL, 	OPTION_HIDDEN, 	"Perform action on all items", 			0},	

	{0,                              0, 	0,		0, 				"Help options", 						9},
  	{"help",                       'h',  	NULL, 	0, 				"Display Help", 						0},
  	{"usage",                      701,  	NULL, 	0, 				"Display Usage", 						0},	
  	{"version",                    702,  	NULL, 	0, 				"Display Version", 						0},
  	{"print-options",              706,  	NULL, 	OPTION_HIDDEN,	"Print options array", 					0},

	{0,0,0,0,0,0} // Final option should be all null
};

/**
 *  CLAP_SHOW_DEVICE - mem show device
 */
struct argp_option ao_show_device[] =						
{
	{0,                              0, 	0, 		OPTION_HIDDEN, 	"Command options", 						1}, 

	{0,                              0, 	0, 		OPTION_HIDDEN, 	"Object options", 						3},	
  	{"block",                      'b', 	"INT", 	OPTION_HIDDEN, 	"Memory Block ID (e.g. 306)", 			0},	
  	{"device",                     'd', 	"STR", 	0,             	"Device name (e.g. mem0)", 				0},	
  	{"region",                     'r', 	"STR", 	0,             	"Region name (e.g. region0)", 			0},	
  	{"interleave",                 'g', 	"INT", 	OPTION_HIDDEN, 	"Interleave Granularity (e.g. 4096)", 	0},	

	{0,                              0, 	0,		OPTION_HIDDEN, 	"State options", 						5},
  	{"offline",                    '0', 	NULL,  	OPTION_HIDDEN, 	"Offline object", 						0},	
  	{"online",                     '1', 	NULL,  	OPTION_HIDDEN, 	"Online object", 						0},	
  	{"kernel",                     'k', 	NULL,  	OPTION_HIDDEN, 	"Zone normal/kernel", 					0},	
  	{"movable",                    'm', 	NULL,  	OPTION_HIDDEN, 	"Zone movable", 						0},	

	{0,                              0,        0,  	0,              "Output options",						7},
  	{"human",                      'H', 	NULL, 	0,             	"Human readable output (K, M, G, T)", 	0},	
  	{"num",                        'n', 	NULL, 	OPTION_HIDDEN, 	"Dsipaly the number of items", 			0},	
  	{"all",                        'a', 	NULL, 	OPTION_HIDDEN, 	"Perform action on all items", 			0},	

	{0,                              0, 	0,		0, 				"Help options", 						9},
  	{"help",                       'h',  	NULL, 	0, 				"Display Help", 						0},
  	{"usage",                      701,  	NULL, 	0, 				"Display Usage", 						0},	
  	{"version",                    702,  	NULL, 	0, 				"Display Version", 						0},
  	{"print-options",              706,  	NULL, 	OPTION_HIDDEN,	"Print options array", 					0},

	{0,0,0,0,0,0} // Final option should be all null
};

/**
 *  CLAP_SHOW_NUM - mem show num
 */
struct argp_option ao_show_num[] =						
{
	{0,                              0, 	0, 		OPTION_HIDDEN, 	"Command options", 						1}, 

	{0,                              0, 	0, 		OPTION_HIDDEN, 	"Filters", 								3},	
  	{"block",                      'b', 	"INT", 	OPTION_HIDDEN, 	"Memory Block ID (e.g. 306)", 			0},	
  	{"device",                     'd', 	"STR", 	OPTION_HIDDEN, 	"Device name (e.g. mem0)", 				0},	
  	{"offline",                    '0', 	NULL,  	0,             	"Num offline blocks", 		   			0},	
  	{"online",                     '1', 	NULL,  	0,             	"Num online blocks", 					0},	
  	{"region",                     'r', 	"STR", 	0,             	"Num blocks in region (e.g. region0)", 	0},	
  	{"interleave",                 'g', 	"INT", 	OPTION_HIDDEN, 	"Interleave Granularity (e.g. 4096)", 	0},	

	{0,                              0, 	0,		OPTION_HIDDEN, 	"State options", 						5},
  	{"kernel",                     'k', 	NULL,  	OPTION_HIDDEN, 	"Zone normal/kernel", 					0},	
  	{"movable",                    'm', 	NULL,  	OPTION_HIDDEN, 	"Zone movable", 						0},	

	{0,                              0,        0,  	OPTION_HIDDEN,	"Output options",						7},
  	{"human",                      'H', 	NULL, 	OPTION_HIDDEN, 	"Human readable output (K, M, G, T)", 	0},	
  	{"num",                        'n', 	NULL, 	OPTION_HIDDEN, 	"Dsipaly the number of items", 			0},	
  	{"all",                        'a', 	NULL, 	OPTION_HIDDEN, 	"Perform action on all items", 			0},	

	{0,                              0, 	0,		0, 				"Help options", 						9},
  	{"help",                       'h',  	NULL, 	0, 				"Display Help", 						0},
  	{"usage",                      701,  	NULL, 	0, 				"Display Usage", 						0},	
  	{"version",                    702,  	NULL, 	0, 				"Display Version", 						0},
  	{"print-options",              706,  	NULL, 	OPTION_HIDDEN,	"Print options array", 					0},

	{0,0,0,0,0,0} // Final option should be all null
};

/**
 *  CLAP_SHOW_REGION - Options for mem show region 
 */
struct argp_option ao_show_region[] =						
{
	{0,                              0, 	0, 		OPTION_HIDDEN, 	"Command options", 						1}, 

	{0,                              0, 	0, 		OPTION_HIDDEN, 	"Object options", 						3},	
  	{"block",                      'b', 	"INT", 	OPTION_HIDDEN, 	"Memory Block ID (e.g. 306)", 			0},	
  	{"device",                     'd', 	"STR", 	OPTION_HIDDEN, 	"Device name (e.g. mem0)", 				0},	
  	{"region",                     'r', 	"STR", 	0,             	"Region name (e.g. region0)", 			0},	
  	{"interleave",                 'g', 	"INT", 	OPTION_HIDDEN, 	"Interleave Granularity (e.g. 4096)", 	0},	

	{0,                              0, 	0,		OPTION_HIDDEN, 	"State options", 						5},
  	{"offline",                    '0', 	NULL,  	OPTION_HIDDEN, 	"Offline object", 						0},	
  	{"online",                     '1', 	NULL,  	OPTION_HIDDEN, 	"Online object", 						0},	
  	{"kernel",                     'k', 	NULL,  	OPTION_HIDDEN, 	"Zone normal/kernel", 					0},	
  	{"movable",                    'm', 	NULL,  	OPTION_HIDDEN, 	"Zone movable", 						0},	

	{0,                              0,        0,  	0,              "Output options",						7},
  	{"human",                      'H', 	NULL, 	0,             	"Human readable output (K, M, G, T)", 	0},	
  	{"num",                        'n', 	NULL, 	OPTION_HIDDEN, 	"Dsipaly the number of items", 			0},	
  	{"all",                        'a', 	NULL, 	OPTION_HIDDEN, 	"Perform action on all items", 			0},	

	{0,                              0, 	0,		0, 				"Help options", 						9},
  	{"help",                       'h',  	NULL, 	0, 				"Display Help", 						0},
  	{"usage",                      701,  	NULL, 	0, 				"Display Usage", 						0},	
  	{"version",                    702,  	NULL, 	0, 				"Display Version", 						0},
  	{"print-options",              706,  	NULL, 	OPTION_HIDDEN,	"Print options array", 					0},

	{0,0,0,0,0,0} // Final option should be all null
};

/**
 *  CLAP_SHOW_SYSTEM - mem show system
 */
struct argp_option ao_show_system[] =						
{
	{0,                              0, 	0, 		OPTION_HIDDEN, 	"Command options", 						1}, 

	{0,                              0, 	0, 		OPTION_HIDDEN, 	"Object options", 						3},	
  	{"block",                      'b', 	"INT", 	OPTION_HIDDEN, 	"Memory Block ID (e.g. 306)", 			0},	
  	{"device",                     'd', 	"STR", 	OPTION_HIDDEN, 	"Device name (e.g. mem0)", 				0},	
  	{"region",                     'r', 	"STR", 	OPTION_HIDDEN, 	"Region name (e.g. region0)", 			0},	
  	{"interleave",                 'g', 	"INT", 	OPTION_HIDDEN, 	"Interleave Granularity (e.g. 4096)", 	0},	

	{0,                              0, 	0,		OPTION_HIDDEN, 	"State options", 						5},
  	{"offline",                    '0', 	NULL,  	OPTION_HIDDEN, 	"Offline object", 						0},	
  	{"online",                     '1', 	NULL,  	OPTION_HIDDEN, 	"Online object", 						0},	
  	{"kernel",                     'k', 	NULL,  	OPTION_HIDDEN, 	"Zone normal/kernel", 					0},	
  	{"movable",                    'm', 	NULL,  	OPTION_HIDDEN, 	"Zone movable", 						0},	

	{0,                              0,        0,  	0,              "Output options",						7},
  	{"human",                      'H', 	NULL, 	0,             	"Human readable output (K, M, G, T)", 	0},	
  	{"num",                        'n', 	NULL, 	OPTION_HIDDEN, 	"Dsipaly the number of items", 			0},	
  	{"all",                        'a', 	NULL, 	OPTION_HIDDEN, 	"Perform action on all items", 			0},	

	{0,                              0, 	0,		0, 				"Help options", 						9},
  	{"help",                       'h',  	NULL, 	0, 				"Display Help", 						0},
  	{"usage",                      701,  	NULL, 	0, 				"Display Usage", 						0},	
  	{"version",                    702,  	NULL, 	0, 				"Display Version", 						0},
  	{"print-options",              706,  	NULL, 	OPTION_HIDDEN,	"Print options array", 					0},

	{0,0,0,0,0,0} // Final option should be all null
};

/**
 * struct argp objects
 *
 * Use [CLAP] enum to index into this array
 */
struct argp ap_main 			= {ao_main 				, pr_main				, 0, 0, 0, 0, 0};

struct argp ap_block  			= {ao_block 			, pr_block 				, 0, 0, 0, 0, 0};
struct argp ap_list  			= {ao_list 				, pr_list 				, 0, 0, 0, 0, 0};
struct argp ap_region 			= {ao_region			, pr_region				, 0, 0, 0, 0, 0};
struct argp ap_set  			= {ao_set  				, pr_set 				, 0, 0, 0, 0, 0};
struct argp ap_show 			= {ao_show 				, pr_show				, 0, 0, 0, 0, 0};

struct argp ap_show_block 		= {ao_show_block        , pr_show_block			, 0, 0, 0, 0, 0};
struct argp ap_show_capacity  	= {ao_show_capacity     , pr_show_capacity 		, 0, 0, 0, 0, 0};
struct argp ap_show_device		= {ao_show_device       , pr_show_device		, 0, 0, 0, 0, 0};
struct argp ap_show_num   		= {ao_show_num          , pr_show_num   		, 0, 0, 0, 0, 0};
struct argp ap_show_region		= {ao_show_region       , pr_show_region		, 0, 0, 0, 0, 0};
struct argp ap_show_system		= {ao_show_system       , pr_show_system		, 0, 0, 0, 0, 0};

/* FUNCTIONS =================================================================*/

/*
 * Print a unsigned char buffer 
 */
static void prnt_buf(void *buf, unsigned long len, unsigned long width, int print_header)
{
	unsigned long i, j, k, rows;
	__u8 *ptr;

	/* STEP 1: Verify Inputs */
	if ( buf == NULL) 
		return;

	if ( len == 0 )
		return;
	
	if ( width == 0 )
		width = DEFAULT_WIDTH;
	
	if ( width < MIN_WIDTH )
		width = MIN_WIDTH;

	ptr = (__u8*) buf;

	/* Compute the number of rows to print */
	rows = len / width;
	if ( (len % width) > 0)
		rows++;

	/* Print index '0x0000: ' */
	if (print_header) {
		printf("            ");
		for ( i = 0 ; i < width ; i++ )
			printf("%02lu ", i);
		printf("\n");
	}

	k = 0;
	for ( i = 0 ; i < rows ; i++ ) {
		printf("0x%08lx: ", i * width);
		for ( j = 0 ; j < width ; j++ ) {
			if (k >= len)
				break;

			printf("%02x ", ptr[i*width + j]);

			k++;
		}
		printf("\n");
	}

	return;
}

/**
 * Return a string representation of CLI Option Names [CLOP]
 */
char *clop(int u)
{
	if (u >= CLOP_MAX) 
		return NULL;
	else 
		return CLOP[u];
}

/**
 * Free allocated memory by option parsing proceedure
 *
 * @return 0 upon success. Non zero otherwise
 */
int options_free()
{
	int rv;
	struct opt *o;
	int i;

	rv = 1;

	if (opts == NULL)
		goto end;

	for ( i = 0 ; i < CLOP_MAX ; i++ )
	{
		o = &opts[i];	

		// Free buf field
		if (o->buf)
			free(o->buf);
		o->buf = NULL;

		// Free str field
		if (o->str)
			free(o->str);
		o->str = NULL;
	}

	// Free options array
	free(opts);

	// Free app Name 
	if (app_name)
		free(app_name);

	rv = 0;

end: 

	return rv;
}

/**
 * Print the command line flag options to the screen as part of help output
 *
 * @param o the menu level [CLAP] enum
 */
static void print_options(struct argp_option *o)
{
	int len;

	while (o->doc != NULL) 
	{
		// Break if this is the ending  NULL entry
		if ( !o->name && !o->key && !o->arg && !o->flags && !o->doc && !o->group )
			break;

		// Skip Hidden Options
		if (o->flags & OPTION_HIDDEN) {
			o++;
			continue;
		}

		// Determine if this is a section heading
		else if ( !o->name && !o->key && !o->arg && !o->flags && o->doc)
			printf("\n%s:\n", o->doc);

		// Print normal option entry 
		else { 

			// IF this option has a single character key, print the key, else print spaces
			if (isalnum(o->key)) 
				printf("  -%c, ", o->key);
			else 
				printf("      ");
			len = 6;

			// If this option has a long name, print the long name
			if (o->name) {
				printf("--%s", o->name);
				len += strlen(o->name) + 2;
			}

			// If this option has an arg type, print the type 
			if (o->arg) {
				printf("=%s", o->arg);
				len += strlen(o->arg) + 1;
			}

			// Print remaining spaces up to description column
			for ( int i = 0 ; i < CLMR_HELP_COLUMN - len ; i++ )
				printf(" ");
			
			// Print description of this option
			printf("%s\n", o->doc);
		}
		o++;
	}
}

/**
 * Debug function to print out the options array at the end of parsing
 */ 
static void print_options_array(struct opt *o)
{
	int i, len, maxlen;

	maxlen = 0;

	// Find max length of CLOP String
	for (i = 0 ; i < CLOP_MAX ; i++) {
		len = strlen(clop(i));
		if (len > maxlen)
			maxlen = len;
	}

	// Print Header 
	printf("##");						// index 
	printf(" Name");						// OP Name 
	for (int k = 5 ; k <= maxlen ; k++)	// Spaces 
		printf(" ");
	printf(" S"); 						// Set 	
	printf("   u8");  					// u8
	printf("    u16"); 					// u16
	printf("        u32"); 				// u32
	printf("                u64"); 		// u64
	printf("    val"); 					// val
	printf("                num"); 		// num
	printf("                len"); 		// len
	printf(" str");						// str
	printf("\n");

	// Print each entry
	for (i = 0 ; i < CLOP_MAX ; i++) {
		// index 
		printf("%02d", i);

		// OP Name 
		printf(" %s", clop(i));

		// Spaces 
		for (int k = strlen(clop(i)) ; k < maxlen ; k++)
			printf(" ");

		printf(" %d", 			o[i].set); // Set 	
		printf(" 0x%02x", 		o[i].u8);  // u8
		printf(" 0x%04x", 		o[i].u16); // u16
		printf(" 0x%08x", 		o[i].u32); // u32
		printf(" 0x%016llx", 	o[i].u64); // u64
		printf(" 0x%04x", 		o[i].val); // val
		printf(" 0x%016llx", 	o[i].num); // num
		printf(" 0x%016llx", 	o[i].len); // len

		if (o[i].str)
			printf(" %s", o[i].str);

		printf("\n");

		if (o[i].len > 0)
			prnt_buf(o[i].buf, o[i].len, 4, 0);
	}
}

/**
 * Print the usage information for a option level
 *
 * @param option 	Menu item from enum [CLAP]
 * @param o 		struct argp_option* to the string data to pull from	
 * STEPS:
 * 1: Initialize variables
 * 2: Generate header text 
 * 3: Count the number of short options with no argument
 * 4: If there is at least one short option with no arg, append short options with no argument here
 * 5: Append short options with arguments 
 * 6: Append long options
 * 7: Find index of last space before character 80
 * 8: Loop through usage buffer and break it up into smaller chunks 
*/
static void print_usage(int option, struct argp_option *o)
{
	int hdr_len, buf_len, num, i, index;
	char buf[4096];
	char str[4096];
	char *ptr;
	struct argp_option *original;
	
	// STEP 1: Initialize variables
	num = 0;
	index = 0;
	hdr_len = 0; 
	buf_len = 1;
	memset(buf, 0, 4096);
	memset(str, 0, 4096);
	original = o;
	ptr = buf;

	// STEP 2: Generate header text 
	switch(option)
	{
		case CLAP_MAIN: 				sprintf(str, "Usage: %s ",      				app_name); break;
		case CLAP_SET:					sprintf(str, "Usage: %s set ",  				app_name); break;
		case CLAP_SHOW: 				sprintf(str, "Usage: %s show ", 				app_name); break;
		case CLAP_BLOCK: 				sprintf(str, "Usage: %s block ", 				app_name); break;
		default: 																				   break;
	}
	hdr_len = strlen(str);

	// STEP 3: Count the number of short options with no argument
	while ( !( !o->name && !o->key && !o->arg && !o->flags && !o->doc && !o->group ) )
	{
		if (isalnum(o->key) && !o->arg) 
			num++;
		o++;
	}
	
	// Reset pointer
	o = original;

	// STEP 4: If there is at least one short option with no arg, append short options with no argument here
	if ( num > 0 ) 
	{
		// Add Leader [-
		sprintf(&buf[buf_len], "[-");
		buf_len += 2;

		// Add each key character
		while ( !( !o->name && !o->key && !o->arg && !o->flags && !o->doc && !o->group ) )
		{
			// If this option has a single character key, print the key, else print spaces
			if (isalnum(o->key) && !o->arg) {
				sprintf(&buf[buf_len], "%c", o->key);
				buf_len += 1;
			}
			o++;
		}

		// Add trailing ]
		sprintf(&buf[buf_len], "] ");
		buf_len += 2;
	}
	
	// Reset pointer
	o = original;

	// STEP 5: Append short options with arguments 
	while ( !( !o->name && !o->key && !o->arg && !o->flags && !o->doc && !o->group ) )
	{
		// If this option has a single character key and an arg 
		if (isalnum(o->key) && o->arg) {
			sprintf(&buf[buf_len], "[-%c=%s] ", o->key, o->arg);
			buf_len += 6;
			buf_len += strlen(o->arg);
		}
		o++;
	}
	
	// Reset pointer
	o = original;

	// STEP 6: Append long options
	while ( !( !o->name && !o->key && !o->arg && !o->flags && !o->doc && !o->group ) )
	{
		// If this option has a long name, print the long name
		if (o->name) 
		{
			sprintf(&buf[buf_len], "[--%s", o->name);
			buf_len += strlen(o->name) + 3;

			// If this option has an arg type, print the type 
			if (o->arg) 
			{
				sprintf(&buf[buf_len], "=%s", o->arg);
				buf_len += strlen(o->arg) + 1;
			}

			// Add trailing ]
			sprintf(&buf[buf_len], "] ");
			buf_len += 2;
		}
		o++;
	}

	// STEP 7: Find index of last space before character 80
	index = 0;
	for ( i = 1 ; i < (CLMR_MAX_HELP_WIDTH-hdr_len) ; i++ ) {
		if (ptr[i] == ' ') 
			index = i;
		if (ptr[i]==0)
			break;
	}

	// STEP 8: Loop through usage buffer and break it up into smaller chunks 
	while (index != 0)
	{
		// Copy the line of the buffer into str 
		memcpy(&str[hdr_len], &ptr[1], index-1); 

		// Set the next char after the string to 0
		str[hdr_len+index-1] = 0;

		// Print the merged string 
		printf("%s\n", str);

		// Clear the header portion of the print str
		memset(str, ' ', hdr_len);

		// Advance buffer
		ptr = &ptr[index];

		// Find index of last space before character 80
		index = 0;
		for ( i = 1 ; i < (CLMR_MAX_HELP_WIDTH-hdr_len) ; i++ ) {
			if (ptr[i] == ' ') 
				index = i;
			if (ptr[i]==0)
				break;
		}
	}
}

/**
 * Print the Help output
 *
 * @param option the level to print [CLAP]
 *
 */
static void print_help(int option)
{
	printf("Memory Expression Management CLI Tool\n");
	
	switch (option)
	{
		case CLAP_MAIN:
			printf("%s", ho_main);
			print_options(ao_main);
			printf("\n");
			break;

		case CLAP_BLOCK:
			printf("%s", ho_block);
			print_options(ao_block);
			printf("\n");
			break;

		case CLAP_LIST:
			printf("%s", ho_list);
			print_options(ao_list);
			printf("\n");
			break;

		case CLAP_REGION:
			printf("%s", ho_region);
			print_options(ao_region);
			printf("\n");
			break;

		case CLAP_SET:
			printf("%s", ho_set);
			print_options(ao_set);
			printf("\n");
			break;

		case CLAP_SHOW:
			printf("%s", ho_show);
			print_options(ao_show);
			printf("\n");
			break;

		case CLAP_SHOW_BLOCK:
			printf("%s", ho_show_block);
			print_options(ao_show_block);
			printf("\n");
			break;

		case CLAP_SHOW_CAPACITY:
			printf("%s", ho_show_capacity);
			print_options(ao_show_capacity);
			printf("\n");
			break;

		case CLAP_SHOW_DEVICE:
			printf("%s", ho_show_device);
			print_options(ao_show_device);
			printf("\n");
			break;

		case CLAP_SHOW_NUM:
			printf("%s", ho_show_num);
			print_options(ao_show_num);
			printf("\n");
			break;

		case CLAP_SHOW_REGION:
			printf("%s", ho_show_region);
			print_options(ao_show_region);
			printf("\n");
			break;

		case CLAP_SHOW_SYSTEM:
			printf("%s", ho_show_system);
			print_options(ao_show_system);
			printf("\n");
			break;

		default: 
			break;
	} 
}

/**
 * Common parse function 
 *
 * This function implements the common flags to most parsers 
 *
 * @return 0 success, non-zero to indicate a problem 
 *
 * Global keys
 * -T --tcp-address 	Server TCP Address
 * -P --tcp-port 		Server TCP Port
 * -V --verbosity 		Set Verbosity Flag
 * -X --verbosity-hex	Set all Verbosity Flags with hex value
 * -A --all				All of collection 
 * -h --help 			Display Help
 *
 * Standard key mapping 
 * -n --length 			Length 
 * -o --offset 			Memory Offset
 * -w --write 			Perform a Write transaction
 *    --data 			Write Data (up to 4 bytes)
 *    --infile 			Filename for input data
 *    --outfile 		Filename for output data
 * 
 * Non char key mapping
 * 701 - usage
 * 702 - version
 * 703 - data
 * 704 - infile
 * 705 - outfile
 * 706 - print-options
 *
 * Special Keys:
 * ARGP_KEY_INIT  		Called first. Initialize any data structures here
 * ARGP_KEY_ARG   		Called for non option parameter (no flag)
 * ARGP_KEY_NO_ARGS  	Only called if there are no args at all
 * ARGP_KEY_SUCCESS 	Called after all args completed successfuly
 * ARGP_KEY_END 		Last call. Verify parameters. Fill in missing parameters
 * ARGP_KEY_ERROR 		Called after an error
 * ARGP_KEY_FINI  		Absolutely last call to this parse functon
 */
static int pr_common(int key, char *arg, struct argp_state *state, int type, struct argp_option *ao)
{
	struct opt *o, *opts = (struct opt*) state->input;
	int rv = 0;

	switch (key)
	{
		// all
		case 'a': 
			o = &opts[CLOP_ALL];
			o->set = 1;
			break;

		// block 
		case 'b': 
			o = &opts[CLOP_BLOCK];
			o->set = 1;
			o->val = strtoul(arg, NULL, 0);
			break;

		// device
		case 'd': 
			o = &opts[CLOP_DEVICE];
			o->set = 1;
			o->str = strdup(arg);
			rv = sscanf(arg, "mem%d", &o->u32);
			if (rv != 1)
			{
				fprintf(stderr, "Error: Could not parse mem device: %s\n", arg);
				exit(1);
			}
			rv = 0;
			break;

		// granularity
		case 'g': 
			o = &opts[CLOP_GRANULARITY];
			o->set = 1;
			o->u32 = strtoul(arg, NULL, 0);
			break;

		// help
		case 'h': 
			print_help(type);
			exit(0);
			break;

		// human
		case 'H': 
			o = &opts[CLOP_HUMAN];
			o->set = 1;
			break;

		// kernel
		case 'k': 
			o = &opts[CLOP_KERNEL];
			o->set = 1;
			break;

		// movable
		case 'm': 
			o = &opts[CLOP_MOVABLE];
			o->set = 1;
			break;

		// num
		case 'n': 
			o = &opts[CLOP_NUM];
			o->set = 1;
			break;

		// offline
		case 'o': 
			o = &opts[CLOP_OFFLINE];
			o->set = 1;
			break;

		// online
		case 'O': 
			o = &opts[CLOP_ONLINE];
			o->set = 1;
			break;

		// region
		case 'r': 
			o = &opts[CLOP_REGION];
			o->set = 1;
			o->str = strdup(arg);
			rv = sscanf(arg, "region%d", &o->u32);
			if (rv != 1)
			{
				fprintf(stderr, "Error: Could not parse region: %s\n", arg);
				rv = 1;
				exit(1);
			}
			rv = 0;
			break;

		// verbosity
		case 'v': 
			o = &opts[CLOP_VERBOSITY];
			o->set = 1;
			o->u32 += 1;
			break;

		// usage 
		case 701: 
			print_usage(type, ao);
			exit(0);
			break;

		// version
		case 702: 
			printf("%s\n", argp_program_version);
			exit(0);
			break;

		// print-options
		case 706: 
			o = &opts[CLOP_PRNT_OPTS];
			o->set = 1;
			break;

		// Last call. Verify parameters. Fill in missing values
		case ARGP_KEY_END:				
			break;
	} 
	return rv;	
}

/**
 * Parse function for: mem
 *
 * @return 0 success, non-zero to indicate a problem 
 */
static int pr_main(int key, char *arg, struct argp_state *state)
{
	struct opt *o, *opts = (struct opt*) state->input;
	int rv = pr_common(key, arg, state, CLAP_MAIN, ao_main);

	switch (key)
	{
		case ARGP_KEY_ARG: 				
			if (!strcmp(arg, "block") || !strcmp(arg, "blk") )
				rv = argp_parse(&ap_block, state->argc-state->next+1, &state->argv[state->next-1], ARGP_IN_ORDER | ARGP_NO_HELP, 0, opts);

			else if (!strcmp(arg, "info")) 
			{
				o = &opts[CLOP_CMD];
				o->set = 1;
				o->val = CLCM_INFO;
			}

			else if (!strcmp(arg, "list")) 
				rv = argp_parse(&ap_list, state->argc-state->next+1, &state->argv[state->next-1], ARGP_IN_ORDER | ARGP_NO_HELP, 0, opts);

			else if (!strcmp(arg, "region") || !strcmp(arg, "reg") ) 
				rv = argp_parse(&ap_region, state->argc-state->next+1, &state->argv[state->next-1], ARGP_IN_ORDER | ARGP_NO_HELP, 0, opts);

			else if (!strcmp(arg, "set")) 
				rv = argp_parse(&ap_set, state->argc-state->next+1, &state->argv[state->next-1], ARGP_IN_ORDER | ARGP_NO_HELP, 0, opts);

			else if (!strcmp(arg, "show")) 
				rv = argp_parse(&ap_show, state->argc-state->next+1, &state->argv[state->next-1], ARGP_IN_ORDER | ARGP_NO_HELP, 0, opts);

			else 
				argp_error (state, "Invalid subcommand"); 

			// Stop current parser
			state->next = state->argc; 	
			break;

		case ARGP_KEY_END:				

			// Print options array if requested 
			if (opts[CLOP_PRNT_OPTS].set)
				print_options_array(opts);

			// Print help if a command has not been set
			if (!opts[CLOP_CMD].set) 
			{
				print_help(CLAP_MAIN);
				exit(0);
			}
			break;
	} 
	return rv;	
}

/**
 * Parse function for: mem block
 *
 * @return 0 success, non-zero to indicate a problem 
 */
static int pr_block(int key, char *arg, struct argp_state *state)
{
	struct opt *o, *opts = (struct opt*) state->input;
	int index, index1, index2, rv = pr_common(key, arg, state, CLAP_BLOCK, ao_block);

	switch (key)
	{
		case ARGP_KEY_ARG: 				
			if (!strcmp(arg, "online") || !strcmp(arg, "on") )
				opts[CLOP_ONLINE].set = 1;

			else if (!strcmp(arg, "offline") || !strcmp(arg, "off" ) )
				opts[CLOP_OFFLINE].set = 1;

			else if (!strcmp(arg, "kernel") || !strcmp(arg, "normal") ) 
				opts[CLOP_KERNEL].set = 1;

			else if (!strcmp(arg, "movable") || !strcmp(arg, "move") ) 
				opts[CLOP_KERNEL].set = 1;

			else if (!strcmp(arg, "all") ) 
				opts[CLOP_ALL].set = 1;

			else if (sscanf(arg, "region%d", &index) == 1) 
			{
				opts[CLOP_REGION].set = 1;
				opts[CLOP_REGION].str = strdup(arg);
			}
			else 
			{
				char *dash = strstr(arg, "-");
				
				if (dash == NULL && sscanf(arg, "%d ", &index1) == 1)
				{
					o = &opts[CLOP_BLOCK];
					o->set = 1;
					o->val = index1;
				}
				else if (dash != NULL && sscanf(arg, "%d-%d", &index1, &index2) == 2)
				{
					o = &opts[CLOP_BLOCK];
					o->set = 1;
					o->num = (index2-index1)+1;
					o->buf = calloc(o->num, sizeof(int));
					for (int i = 0 ; i < (int) o->num ; i++)
						((int*)o->buf)[i] = index1+i;
				}
				else 
					argp_error (state, "Invalid subcommand"); 
			}

			break;

		case ARGP_KEY_END:				

			if (opts[CLOP_REGION].set)
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val =  CLCM_SET_REGION_BLOCK_STATE;
			}
			else if (opts[CLOP_OFFLINE].set)
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_BLOCK_OFFLINE;
			}
			else if (opts[CLOP_MOVABLE].set)
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_SET_BLOCK_STATE;
				opts[CLOP_ZONE].set = 1; 
				opts[CLOP_ZONE].val = CLZN_MOVABLE;
			}
			else if (opts[CLOP_KERNEL].set)
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_SET_BLOCK_STATE;
				opts[CLOP_ZONE].set = 1; 
				opts[CLOP_ZONE].val = CLZN_NORMAL;
			}
			else if (opts[CLOP_ONLINE].set)
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_BLOCK_ONLINE;
			}

			if ( (opts[CLOP_ONLINE].set  
				+ opts[CLOP_OFFLINE].set 
				+ opts[CLOP_KERNEL].set 
				+ opts[CLOP_MOVABLE].set) > 1)
			{
				fprintf(stderr, "Error: Multiple states specified. Specify only one desired block state\n");
				exit(0);
			}

			// Print options array if requested 
			if (opts[CLOP_PRNT_OPTS].set)
			{
				print_options_array(opts);
				opts[CLOP_PRNT_OPTS].set = 0;
			}

			// If the block identifier is not set, print help and exit
			if (opts[CLOP_BLOCK].set == 0 && opts[CLOP_ALL].set == 0)
			{
				fprintf(stderr, "Error: Missing block id(s)\n");
				print_help(CLAP_BLOCK);
				exit(0);
			}

			// Print help if a command has not been set
			if (!opts[CLOP_CMD].set) 
			{
				fprintf(stderr, "Error: Missing command\n");
				print_help(CLAP_BLOCK);
				exit(0);
			}

			break;
	} 
	return rv;	
}

/**
 * Parse function for: mem list
 *
 * @return 0 success, non-zero to indicate a problem 
 */
static int pr_list(int key, char *arg, struct argp_state *state)
{
	struct opt *o, *opts = (struct opt*) state->input;
	int index, index1, index2, rv = pr_common(key, arg, state, CLAP_LIST, ao_list);

	opts[CLOP_CMD].set = 1;
	opts[CLOP_CMD].val =  CLCM_LIST;

	switch (key)
	{
		case ARGP_KEY_ARG: 				
			if (!strcmp(arg, "online") || !strcmp(arg, "on") )
				opts[CLOP_ONLINE].set = 1;

			else if (!strcmp(arg, "offline") || !strcmp(arg, "off" ) )
				opts[CLOP_OFFLINE].set = 1;

			else if (!strcmp(arg, "kernel") || !strcmp(arg, "normal") ) 
				opts[CLOP_KERNEL].set = 1;

			else if (!strcmp(arg, "movable") || !strcmp(arg, "move") ) 
				opts[CLOP_KERNEL].set = 1;

			else if (sscanf(arg, "region%d", &index) == 1) 
			{
				opts[CLOP_REGION].set = 1;
				opts[CLOP_REGION].str = strdup(arg);
			}
			else 
			{
				char *dash = strstr(arg, "-");
				
				if (dash == NULL && sscanf(arg, "%d ", &index1) == 1)
				{
					o = &opts[CLOP_BLOCK];
					o->set = 1;
					o->val = index1;
				}
				else if (dash != NULL && sscanf(arg, "%d-%d", &index1, &index2) == 2)
				{
					o = &opts[CLOP_BLOCK];
					o->set = 1;
					o->num = (index2-index1)+1;
					o->buf = calloc(o->num, sizeof(int));
					for (int i = 0 ; i < (int) o->num ; i++)
						((int*)o->buf)[i] = index1+i;
				}
				else 
					argp_error (state, "Invalid subcommand"); 
			}

			break;

		case ARGP_KEY_END:				

			if ( (opts[CLOP_ONLINE].set  
				+ opts[CLOP_OFFLINE].set 
				+ opts[CLOP_KERNEL].set 
				+ opts[CLOP_MOVABLE].set) > 1)
			{
				fprintf(stderr, "Error: Multiple online states specified. Specify only one desired block state\n");
				exit(0);
			}

			// Print options array if requested 
			if (opts[CLOP_PRNT_OPTS].set)
			{
				print_options_array(opts);
				opts[CLOP_PRNT_OPTS].set = 0;
			}

			break;
	} 
	return rv;	
}

/**
 * Parse function for: mem region
 *
 * @return 0 success, non-zero to indicate a problem 
 */
static int pr_region(int key, char *arg, struct argp_state *state)
{
	struct opt *opts = (struct opt*) state->input;
	int index, rv = pr_common(key, arg, state, CLAP_REGION, ao_region);

	switch (key)
	{
		case ARGP_KEY_ARG: 				
			if (!strcmp(arg, "create")) 
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_REGION_CREATE;
			}
			else if (!strcmp(arg, "daxmode") || !strcmp(arg, "dax") )
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_REGION_DAXMODE;
			}
			else if (!strcmp(arg, "delete") || !strcmp(arg, "del") )
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_REGION_DELETE;
			}
			else if (!strcmp(arg, "disable") )
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_REGION_DISABLE;
			}
			else if (!strcmp(arg, "enable") )
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_REGION_ENABLE;
			}
			else if (!strcmp(arg, "rammode") || !strcmp(arg, "ram") )
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_REGION_RAMMODE;
			}
			else if (!strcmp(arg, "all") ) 
				opts[CLOP_ALL].set = 1;

			else if (sscanf(arg, "region%d", &index) == 1) 
			{
				opts[CLOP_REGION].set = 1;
				opts[CLOP_REGION].str = strdup(arg);
			}
			else if (sscanf(arg, "mem%d", &index) == 1)
			{
				if (opts[CLOP_CMD].val != CLCM_REGION_CREATE)
					argp_error (state, "Invalid subcommand"); 

				opts[CLOP_DEVICE].set = 1;
				opts[CLOP_DEVICE].num++;
				opts[CLOP_DEVICE].buf = realloc(opts[CLOP_DEVICE].buf, sizeof(char *) * opts[CLOP_DEVICE].num);
				((char**) opts[CLOP_DEVICE].buf)[opts[CLOP_DEVICE].num-1] = strdup(arg);
			}
			else 
				argp_error (state, "Invalid subcommand"); 

			break;

		case ARGP_KEY_END:				

			if (opts[CLOP_CMD].val == CLCM_REGION_CREATE 
				&& !opts[CLOP_ALL].set 
				&& !opts[CLOP_DEVICE].set)
			{
				opts[CLOP_ALL].set = 1;
			}

			if (opts[CLOP_CMD].val == CLCM_REGION_DELETE
				&& !opts[CLOP_ALL].set 
				&& !opts[CLOP_REGION].set)
			{
				fprintf(stderr, "Error: Missing region name or all\n");
				print_help(CLAP_REGION);
				exit(1);
			}

			if (opts[CLOP_CMD].val == CLCM_REGION_DISABLE
				&& !opts[CLOP_ALL].set 
				&& !opts[CLOP_REGION].set)
			{
				fprintf(stderr, "Error: Missing region name or all\n");
				print_help(CLAP_REGION);
				exit(1);
			}

			if (opts[CLOP_CMD].val == CLCM_REGION_ENABLE
				&& !opts[CLOP_ALL].set 
				&& !opts[CLOP_REGION].set)
			{
				opts[CLOP_ALL].set = 1;
			}

			if (opts[CLOP_CMD].val == CLCM_REGION_DAXMODE
				&& !opts[CLOP_ALL].set 
				&& !opts[CLOP_REGION].set)
			{
				fprintf(stderr, "Error: Missing region name or all\n");
				print_help(CLAP_REGION);
				exit(1);
			}

			if (opts[CLOP_CMD].val == CLCM_REGION_RAMMODE
				&& !opts[CLOP_ALL].set 
				&& !opts[CLOP_REGION].set)
			{
				fprintf(stderr, "Error: Missing region name or all\n");
				print_help(CLAP_REGION);
				exit(1);
			}

			// Print options array if requested 
			if (opts[CLOP_PRNT_OPTS].set)
			{
				print_options_array(opts);
				opts[CLOP_PRNT_OPTS].set = 0;
			}

			// Print help if a command has not been set
			if (!opts[CLOP_CMD].set) 
			{
				print_help(CLAP_REGION);
				exit(1);
			}

			break;
	} 
	return rv;	
}

/**
 * Parse function for: mem set
 *
 * @return 0 success, non-zero to indicate a problem 
 */
static int pr_set(int key, char *arg, struct argp_state *state)
{
	struct opt *opts = (struct opt*) state->input;
	int rv = pr_common(key, arg, state, CLAP_SET, ao_set);

	switch (key)
	{
		case ARGP_KEY_ARG: 				
			if (!strcmp(arg, "policy") ) 
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_SET_SYSTEM_POLICY;
			}
			else if (!strcmp(arg, "online") || !strcmp(arg, "on") ) 
			{
				opts[CLOP_ONLINE].set = 1;
			}
			else if (!strcmp(arg, "offline") || !strcmp(arg, "off") ) 
			{
				opts[CLOP_OFFLINE].set = 1;
			}
			else if (!strcmp(arg, "kernel") ) 
			{
				opts[CLOP_KERNEL].set = 1;
			}
			else if (!strcmp(arg, "movable") || !strcmp(arg, "move") ) 
			{
				opts[CLOP_MOVABLE].set = 1;
			}
			else 
				 argp_error (state, "Invalid subcommand"); 

			break;

		case ARGP_KEY_END:				

			if (opts[CLOP_CMD].val == CLCM_SET_SYSTEM_POLICY 
				&& !opts[CLOP_ONLINE].set 
				&& !opts[CLOP_OFFLINE].set 
				&& !opts[CLOP_KERNEL].set 
				&& !opts[CLOP_MOVABLE].set)
			{
				fprintf(stderr, "Error: Missing policy\n");
				exit(0);
			}

			if ( (opts[CLOP_ONLINE].set  
				+ opts[CLOP_OFFLINE].set 
				+ opts[CLOP_KERNEL].set 
				+ opts[CLOP_MOVABLE].set) > 1)
			{
				fprintf(stderr, "Error: Multiple policies specified. Specify only one policy\n");
				exit(0);
			}

			if (opts[CLOP_PRNT_OPTS].set)
			{
				print_options_array(opts);
				opts[CLOP_PRNT_OPTS].set = 0;
			}

			if (!opts[CLOP_CMD].set) 
			{
				fprintf(stderr, "Error: Missing subcommand\n");
				print_help(CLAP_SET);
				exit(0);
			}
			break;
	} 
	return rv;	
}

/**
 * Parse function for: mem show
 *
 * @return 0 success, non-zero to indicate a problem 
 */
static int pr_show(int key, char *arg, struct argp_state *state)
{
	struct opt *opts = (struct opt*) state->input;
	int rv = pr_common(key, arg, state, CLAP_SHOW, ao_show);

	switch (key)
	{
		case ARGP_KEY_ARG: 				

			if (!strcmp(arg, "block") || !strcmp(arg, "blk") || !strcmp(arg, "blocks") ) 
				rv = argp_parse(&ap_show_block, state->argc-state->next+1, &state->argv[state->next-1], ARGP_IN_ORDER | ARGP_NO_HELP, 0, opts);

			else if (!strcmp(arg, "capacity") || !strcmp(arg, "cap") || !strcmp(arg, "size") ) 
				rv = argp_parse(&ap_show_capacity, state->argc-state->next+1, &state->argv[state->next-1], ARGP_IN_ORDER | ARGP_NO_HELP, 0, opts);

			else if (!strcmp(arg, "device") || !strcmp(arg, "dev") || !strcmp(arg, "devices") ) 
				rv = argp_parse(&ap_show_device, state->argc-state->next+1, &state->argv[state->next-1], ARGP_IN_ORDER | ARGP_NO_HELP, 0, opts);

			else if (!strcmp(arg, "num") ) 
				rv = argp_parse(&ap_show_num, state->argc-state->next+1, &state->argv[state->next-1], ARGP_IN_ORDER | ARGP_NO_HELP, 0, opts);

			else if (!strcmp(arg, "region") || !strcmp(arg, "rgn") || !strcmp(arg, "regions") ) 
				rv = argp_parse(&ap_show_region, state->argc-state->next+1, &state->argv[state->next-1], ARGP_IN_ORDER | ARGP_NO_HELP, 0, opts);

			else if (!strcmp(arg, "system") || !strcmp(arg, "sys") ) 
				rv = argp_parse(&ap_show_system, state->argc-state->next+1, &state->argv[state->next-1], ARGP_IN_ORDER | ARGP_NO_HELP, 0, opts);

			else 
				argp_error (state, "Invalid subcommand"); 

			// Stop current parser
			state->next = state->argc; 	
			break;

		case ARGP_KEY_END:				

			if (opts[CLOP_PRNT_OPTS].set)
			{
				print_options_array(opts);
				opts[CLOP_PRNT_OPTS].set = 0;
			}

			// Fail if no command is set 
			if (!opts[CLOP_CMD].set) 
			{
				fprintf(stderr, "Error: Missing command\n");
				print_help(CLAP_SHOW);
				exit(0);
			}

			break;
	} 
	return rv;	
}

/**
 * Parse function for: mem show block 
 *
 * @return 0 success, non-zero to indicate a problem 
 */
static int pr_show_block(int key, char *arg, struct argp_state *state)
{
	struct opt *opts = (struct opt*) state->input;
	int index, val, rv = pr_common(key, arg, state, CLAP_SHOW_BLOCK, ao_show_block);

	switch (key)
	{
		case ARGP_KEY_ARG: 				

			if (!strcmp(arg, "isonline") ) 
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_SHOW_BLK_ISONLINE;
			}
			else if (!strcmp(arg, "isremovable") || !strcmp(arg, "removable") ) 
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_SHOW_BLK_ISREMOVABLE;
			}
			else if (!strcmp(arg, "node") ) 
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_SHOW_BLK_NODE;
			}
			else if (!strcmp(arg, "device") || !strcmp(arg, "physdevice") ) 
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_SHOW_BLK_PHYSDEVICE;
			}
			else if (!strcmp(arg, "state") ) 
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_SHOW_BLK_STATE;
			}
			else if (!strcmp(arg, "zones") || !strcmp(arg,"validzones") ) 
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_SHOW_BLK_ZONES;
			}
			else if (!strcmp(arg, "online") || !strcmp(arg, "on") ) 
			{
				opts[CLOP_ONLINE].set = 1;
			}
			else if (!strcmp(arg, "offline") || !strcmp(arg, "off") ) 
			{
				opts[CLOP_OFFLINE].set = 1;
			}
			else if (sscanf(arg, "region%d", &index) == 1) 
			{
				opts[CLOP_REGION].set = 1;
				opts[CLOP_REGION].str = strdup(arg);
			}
			else if (sscanf(arg, "%d", &index) == 1)
			{
				opts[CLOP_BLOCK].set = 1;
				opts[CLOP_BLOCK].val = index;
			}
			else
				argp_error (state, "Invalid subcommand"); 

			break;

		case ARGP_KEY_END:				

			val = opts[CLOP_CMD].val;
			if ( ( val == CLCM_SHOW_BLK_ISONLINE
				|| val == CLCM_SHOW_BLK_ISREMOVABLE
				|| val == CLCM_SHOW_BLK_NODE
				|| val == CLCM_SHOW_BLK_PHYSDEVICE
				|| val == CLCM_SHOW_BLK_STATE
				|| val == CLCM_SHOW_BLK_ZONES ) 
				&& !opts[CLOP_BLOCK].set)
			{
				fprintf(stderr, "Error: You must supply a block id for individual block state commands\n");
				exit(1);
			}

			if (!opts[CLOP_CMD].set)
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_SHOW_BLOCKS;
			}

			if (opts[CLOP_PRNT_OPTS].set)
			{
				print_options_array(opts);
				opts[CLOP_PRNT_OPTS].set = 0;
			}

			// Fail if no command is set 
			if (!opts[CLOP_CMD].set) 
			{
				print_help(CLAP_SHOW_BLOCK);
				exit(0);
			}

			break;
	} 
	return rv;	
}

/**
 * Parse function for: mem show capacity
 *
 * @return 0 success, non-zero to indicate a problem 
 */
static int pr_show_capacity(int key, char *arg, struct argp_state *state)
{
	struct opt *opts = (struct opt*) state->input;
	int index, rv = pr_common(key, arg, state, CLAP_SHOW_CAPACITY, ao_show_capacity);

	opts[CLOP_CMD].set = 1;
	opts[CLOP_CMD].val = CLCM_SHOW_CAPACITY;

	switch (key)
	{
		case ARGP_KEY_ARG: 				

			if (sscanf(arg, "region%d", &index) ) 
			{
				opts[CLOP_REGION].set = 1;
				opts[CLOP_REGION].str = strdup(arg);
			}
			else if (!strcmp(arg, "online") || !strcmp(arg, "on") ) 
			{
				opts[CLOP_ONLINE].set = 1;
			}
			else if (!strcmp(arg, "offline") || !strcmp(arg, "off") ) 
			{
				opts[CLOP_OFFLINE].set = 1;
			}
			else 
				argp_error (state, "Invalid subcommand"); 

			break;

		case ARGP_KEY_END:				

			if ( (  opts[CLOP_ONLINE].set 
				  + opts[CLOP_OFFLINE].set) > 1)
			{
				fprintf(stderr, "Error: Contradictory filter states specified. Please only specify online or offline\n");
				exit(1);
			}

			if (opts[CLOP_PRNT_OPTS].set)
			{
				print_options_array(opts);
				opts[CLOP_PRNT_OPTS].set = 0;
			}

			// Fail if no command is set 
			if (!opts[CLOP_CMD].set) 
			{
				print_help(CLAP_SHOW_CAPACITY);
				exit(0);
			}

			break;
	} 
	return rv;	
}

/**
 * Parse function for: mem show device
 *
 * @return 0 success, non-zero to indicate a problem 
 */
static int pr_show_device(int key, char *arg, struct argp_state *state)
{
	struct opt *opts = (struct opt*) state->input;
	int index, rv = pr_common(key, arg, state, CLAP_SHOW_DEVICE, ao_show_device);

	switch (key)
	{
		case ARGP_KEY_ARG: 				

			if (sscanf(arg, "mem%d", &index) == 1) 
			{
				opts[CLOP_DEVICE].set = 1;
				opts[CLOP_DEVICE].str = strdup(arg);
			}
			else if (sscanf(arg, "region%d", &index) == 1) 
			{
				opts[CLOP_REGION].set = 1;
				opts[CLOP_REGION].str = strdup(arg);
			}
			else if (!strcmp(arg, "isavailable") || !strcmp(arg, "available") || !strcmp(arg, "avail") )
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_SHOW_DEVICE_ISAVAILABLE;
			}
			else if (!strcmp(arg, "ig") || !strcmp(arg, "granularity") || !strcmp(arg, "interleave") )
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_SHOW_DEVICE_INTERLEAVE_GRANULARITY;
			}
			else 
				argp_error (state, "Invalid subcommand"); 

			break;


		case ARGP_KEY_END:				

			if (opts[CLOP_CMD].val == CLCM_SHOW_DEVICE_ISAVAILABLE
				&& !opts[CLOP_DEVICE].set)
			{
				fprintf(stderr, "Error: You must specify a device when using subcommand: isavailable\n");;
				exit(1);
			}

			if (opts[CLOP_CMD].val == CLCM_SHOW_DEVICE_INTERLEAVE_GRANULARITY
				&& !opts[CLOP_DEVICE].set)
			{
				fprintf(stderr, "Error: You must specify a device when using subcommand: ig\n");;
				exit(1);
			}

			if (opts[CLOP_DEVICE].set 
				&& !opts[CLOP_CMD].set)
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_SHOW_DEVICES;
			}

			if (!opts[CLOP_CMD].set)
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_SHOW_DEVICES;
			}

			if (opts[CLOP_PRNT_OPTS].set)
			{
				print_options_array(opts);
				opts[CLOP_PRNT_OPTS].set = 0;
			}

			// Fail if no command is set 
			if (!opts[CLOP_CMD].set) 
			{
				print_help(CLAP_SHOW_DEVICE);
				exit(0);
			}

			break;
	} 
	return rv;	
}

/**
 * Parse function for: mem show num
 *
 * @return 0 success, non-zero to indicate a problem 
 */
static int pr_show_num(int key, char *arg, struct argp_state *state)
{
	struct opt *opts = (struct opt*) state->input;
	int index, rv = pr_common(key, arg, state, CLAP_SHOW_NUM, ao_show_num);

	opts[CLOP_NUM].set = 1;

	switch (key)
	{
		case ARGP_KEY_ARG: 				

			if (!strcmp(arg, "blocks") || !strcmp(arg, "blk") || !strcmp(arg, "block") ) 
				opts[CLOP_BLOCKS].set = 1;

			else if (!strcmp(arg, "devices") || !strcmp(arg, "dev") || !strcmp(arg, "device") ) 
				opts[CLOP_DEVICES].set = 1;

			else if (!strcmp(arg, "regions") || !strcmp(arg, "reg") ) 
				opts[CLOP_REGIONS].set = 1;

			else if (!strcmp(arg, "online") || !strcmp(arg, "on") ) 
				opts[CLOP_ONLINE].set = 1;

			else if (!strcmp(arg, "offline") || !strcmp(arg, "off") ) 
				opts[CLOP_OFFLINE].set = 1;

			else if (sscanf(arg, "region%d", &index) == 1) 
			{
				opts[CLOP_REGION].set = 1;
				opts[CLOP_REGION].str = strdup(arg);
			}
			else 
				argp_error (state, "Invalid subcommand"); 

			break;

		case ARGP_KEY_END:				

			if ( ( opts[CLOP_BLOCKS].set 
				 + opts[CLOP_DEVICES].set 
				 + opts[CLOP_REGIONS].set) > 1)
			{
				fprintf(stderr, "Error: Specify only one type of object\n");
				exit(1);
			}

			// Default to show num blocks if no object is specified 
			if ( ( opts[CLOP_BLOCKS].set 
				 + opts[CLOP_DEVICES].set 
				 + opts[CLOP_REGIONS].set) == 0)
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_SHOW_NUM_BLOCKS;
			}
			else if (opts[CLOP_BLOCKS].set)
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_SHOW_NUM_BLOCKS;
			}
			else if (opts[CLOP_DEVICES].set)
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_SHOW_NUM_DEVICES;
			}
			else if (opts[CLOP_REGIONS].set)
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_SHOW_NUM_REGIONS;
			}

			if (opts[CLOP_PRNT_OPTS].set)
			{
				print_options_array(opts);
				opts[CLOP_PRNT_OPTS].set = 0;
			}

			// Fail if no command is set 
			if (!opts[CLOP_CMD].set) 
			{
				print_help(CLAP_SHOW_NUM);
				exit(0);
			}

			break;
	} 
	return rv;	
}

/**
 * Parse function for: mem show region
 *
 * @return 0 success, non-zero to indicate a problem 
 */
static int pr_show_region(int key, char *arg, struct argp_state *state)
{
	struct opt *opts = (struct opt*) state->input;
	int index, rv = pr_common(key, arg, state, CLAP_SHOW_REGION, ao_show_region);

	opts[CLOP_CMD].set = 1;
	opts[CLOP_CMD].val = CLCM_SHOW_REGIONS;

	switch (key)
	{
		case ARGP_KEY_ARG: 				

			if (sscanf(arg, "region%d", &index) == 1) 
			{
				opts[CLOP_REGION].set = 1;
				opts[CLOP_REGION].str = strdup(arg);
			}
			else 
				argp_error (state, "Invalid subcommand"); 

			break;

		case ARGP_KEY_END:				

			if (opts[CLOP_PRNT_OPTS].set)
			{
				print_options_array(opts);
				opts[CLOP_PRNT_OPTS].set = 0;
			}

			break;
	} 
	return rv;	
}

/**
 * Parse function for: mem show system
 *
 * @return 0 success, non-zero to indicate a problem 
 */
static int pr_show_system(int key, char *arg, struct argp_state *state)
{
	struct opt *opts = (struct opt*) state->input;
	int rv = pr_common(key, arg, state, CLAP_SHOW_SYSTEM, ao_show_system);

	switch (key)
	{
		case ARGP_KEY_ARG: 				

			if (!strcmp(arg, "blocksize") || !strcmp(arg, "size") || !strcmp(arg, "bs") ) 
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_SHOW_SYSTEM_BLOCKSIZE;
			}
			else if (!strcmp(arg, "policy") ) 
			{
				opts[CLOP_CMD].set = 1;
				opts[CLOP_CMD].val = CLCM_SHOW_SYSTEM_POLICY;
			}
			else 
				argp_error (state, "Invalid subcommand"); 

			break;

		case ARGP_KEY_END:				

			if (opts[CLOP_PRNT_OPTS].set)
			{
				print_options_array(opts);
				opts[CLOP_PRNT_OPTS].set = 0;
			}

			// Fail if no command is set 
			if (!opts[CLOP_CMD].set) 
			{
				print_help(CLAP_SHOW_SYSTEM);
				exit(0);
			}

			break;
	} 
	return rv;	
}

/**
 * Obtain option defaults from environment if present 
 *
 * @return 	0 on Success, non zero otherwise
 */
int options_getenv()
{
	struct argp_state state;
	int rv;
	char *o;
	struct envopt *e;

	// Initialize variables
	rv = 1;
	state.input = opts;
	e = envopts;

	if (e == NULL)
		goto end;

	// Loop through global envopts array 
	while (e->key != 0)
	{
		// Get value from shell environment 
		o = getenv(e->name);

		// If the returned value is not NULL parse using pr_common()
		if (o != NULL)
			pr_common(e->key, o, &state, 0, NULL);

		// Advance to the next entry in the global envopts array 
		e++;
	}

	rv = 0;

end:
	
	return rv;
}

/**
 * Parse CLI options 
 *
 * @param argc Number of CLI parameters 
 * @param argv Array of string pointers to CLI parameters 
 * 
 * STEPS
 * 1: Store app name in global variable
 * 2: Allocate and clear memory for options array
 * 3: Obtain Option defaults from shell environment 
 * 4: Parse options 
 */ 
int options_parse(int argc, char *argv[])
{
	int rv, len;
	char default_name[] = "app";

	rv = 1;

	// STEP 1: Store app name in global variable
	app_name = NULL;
	if (argc > 0) 
	{
		len = strnlen(argv[0], CLMR_MAX_NAME_LEN);
		if (len > 0) 
		{
			app_name = malloc(len+1);
			if (app_name == NULL)
				goto end;
			memset(app_name, 0, len+1);

			if (!strncmp("./", argv[0], 2))
				memcpy(app_name, &argv[0][2], len);
			else
				memcpy(app_name, argv[0], len);
		}
	}
	else 
		app_name = default_name;

	// STEP 2: Allocate and clear memory for options array
	opts = calloc(CLOP_MAX, sizeof(struct opt));
	if (opts == NULL) 
		goto end_name;

	// STEP 3: Obtain Option defaults from shell environment 
	options_getenv();

	// STEP 4: Parse options 
  	rv = argp_parse(&ap_main, argc, argv, ARGP_IN_ORDER | ARGP_NO_HELP, 0, opts);
	if (rv != 0) 
		goto end_opt;

	return rv;

end_opt:

	free(opts);

end_name:

	if (app_name && app_name != default_name)
		free(app_name);

end:

	return rv;
}

