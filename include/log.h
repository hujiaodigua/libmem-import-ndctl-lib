/**
 * @file 		log.h 
 *
 * @brief 		Header file for logging library
 *
 * @copyright   Copyright (C) 2024 Jackrabbit Founders LLC. All rights reserved.
 *       
 * @date        Jul 2024
 * @author      Barrett Edwards <code@jrlabs.io>
 *
 * Log levels for reference:
 * LOG_EMERG	0	 system is unusable 
 * LOG_ALERT	1	 action must be taken immediately
 * LOG_CRIT	    2	 critical conditions
 * LOG_ERR		3	 error conditions
 * LOG_WARNING	4	 warning conditions
 * LOG_NOTICE	5	 normal but significant condition
 * LOG_INFO	    6	 informational
 * LOG_DEBUG	7	 debug-level messages
 */

#ifndef _LOG_H
#define _LOG_H

/* INCLUDES ==================================================================*/

/* MACROS ====================================================================*/

#define dbg(x, arg...) 				log_dbg(x->log, ## arg)
#define info(x, arg...) 			log_info(x->log, ## arg)
#define notice(x, arg...) 			log_notice(x->log, ## arg)
#define warn(x, arg...) 			log_warn(x->log, ## arg)
#define err(x, arg...) 				log_err(x->log, ## arg)

#ifdef ENABLE_LOGGING
#  define log_dbg(ctx, arg...)  	do { if ((ctx)->priority >= LOG_DEBUG) 		log_submit(ctx, LOG_DEBUG, 		__FUNCTION__, __LINE__, ## arg); } while(0)
#  define log_info(ctx, arg...) 	do { if ((ctx)->priority >= LOG_INFO)  		log_submit(ctx, LOG_INFO, 		__FUNCTION__, __LINE__, ## arg); } while(0)
#  define log_notice(ctx, arg...)	do { if ((ctx)->priority >= LOG_NOTICE) 	log_submit(ctx, LOG_NOTICE, 	__FUNCTION__, __LINE__, ## arg); } while(0) 
#  define log_warn(ctx, arg...) 	do { if ((ctx)->priority >= LOG_WARNING)  	log_submit(ctx, LOG_WARNING, 	__FUNCTION__, __LINE__, ## arg); } while(0)
#  define log_err(ctx, arg...)  	do { if ((ctx)->priority >= LOG_ERR)   		log_submit(ctx, LOG_ERR, 		__FUNCTION__, __LINE__, ## arg); } while(0)
#else
#  define log_dbg(ctx, arg...)   	{}
#  define log_info(ctx, arg...)  	{}
#  define log_notice(ctx, arg...)   {}
#  define log_warn(ctx, arg...)  	{}
#  define log_err(ctx, arg...)   	{}
#endif

/* ENUMERATIONS ==============================================================*/

/**
 * Enumereation represeting log destinations to use 
 */
enum LOG_DST
{
	LDST_STDIO  = 0, 
	LDST_SYSLOG = 1, 
	LDST_NULL 	= 2,
	LDST_FILE 	= 3, 
	LDST_MAX
};

/* STRUCTS ===================================================================*/

struct log_ctx;
typedef void (*log_fn)(struct log_ctx *ctx, int priority, const char *fn, int ln, const char *format, va_list args);

struct log_ctx
{
	log_fn log_fn;
	int timestamp; 
	int priority;
	const char *owner;
	FILE *file;
};

/* GLOBAL VARIABLES ==========================================================*/

/* PROTOTYPES ================================================================*/

struct log_ctx *log_init(const char *owner, int dst, int level, int timestamp, char *filepath);
void log_submit(struct log_ctx *ctx, int priority, const char *fn, int ln, const char *format, ...);
int log_free(struct log_ctx *ctx);

/* Log functions */
void log_to_file(struct log_ctx *ctx, int priority, const char *fn, int ln, const char *format, va_list args);
void log_to_stdio(struct log_ctx *ctx, int priority, const char *fn, int ln, const char *format, va_list args);
void log_to_syslog(struct log_ctx *ctx, int priority, const char *fn, int ln, const char *format, va_list args);
static inline void log_to_null(struct log_ctx *ctx, int priority, const char *fn, int ln, const char *format, va_list args) {}

const char *log_priority_to_str(int priority);
const char *log_dst_to_str(int dst);

#endif //ifndef _LOG_H

