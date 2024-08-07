/**
 * @file 		log.c
 *
 * @brief 		Code file for logging library
 *
 * @copyright   Copyright (C) 2024 Jackrabbit Founders LLC. All rights reserved.
 *       
 * @date        Jul 2024
 * @author      Barrett Edwards <code@jrlabs.io>
 */

/* INCLUDES ==================================================================*/

/* printf()
 * fprintf()
 * vfprintf()
 * FILE
 * stdout
 * stderr
 */
#include <stdio.h>

/* calloc() 
 * free()
 */
#include <stdlib.h>

/* getpid()
 */
#include <unistd.h>

/* clock_gettime()
 */
#include <time.h>

/* va_start 
 * va_end
 */
#include <stdarg.h>

/* LOG_* Macros 
 */
#include <syslog.h> 

#include "log.h"

/* MACROS ====================================================================*/

/* ENUMERATIONS ==============================================================*/

/* STRUCTS ===================================================================*/

/* GLOBAL VARIABLES ==========================================================*/

/**
 * String representation of logging levels 
 */
static const char *levels[] = 
{
	"EMERG",
	"ALERT",
	"CRIT",
	"ERR",
	"WARNING",
	"NOTICE",
	"INFO",
	"DEBUG",
};

static const char *log_destinations[] =
{
	"STDIO", 
	"SYSLOG", 
	"NULL",
	"FILE", 
};

/* PROTOTYPES ================================================================*/

/* FUNCTIONS =================================================================*/

void log_to_syslog(struct log_ctx *ctx, int priority, const char *fn, int ln, const char *format, va_list args)
{
	vsyslog(priority, format, args);
}

void log_to_stdio(struct log_ctx *ctx, int priority, const char *fn, int ln, const char *format, va_list args)
{
	struct timespec ts;
	FILE *fp;

	if (priority >= LOG_INFO)
		fp = stdout;
	else
		fp = stderr;

	if (ctx->timestamp)
	{
		// Get time for entry 
		clock_gettime(CLOCK_REALTIME, &ts);

		// print the time first 
		fprintf(fp, "[%10ld.%09ld] [%d] %s - ", ts.tv_sec, ts.tv_nsec, getpid(), levels[priority]);
		fprintf(fp, "%s: %s:%d ", ctx->owner, fn, ln);
	}

	vfprintf(fp, format, args);
}

void log_to_file(struct log_ctx *ctx, int priority, const char *fn, int ln, const char *format, va_list args)
{
	struct timespec ts;
	FILE *f = ctx->file;

	if (ctx->timestamp)
	{
		// Get time for entry 
		clock_gettime(CLOCK_REALTIME, &ts);

		// print the time first 
		fprintf(f, "[%10ld.%09ld] [%d] %s - ", ts.tv_sec, ts.tv_nsec, getpid(), levels[priority]);
		fprintf(f, "%s: %s:%d ", ctx->owner, fn, ln);
	}

	// print the message 
	vfprintf(f, format, args);

	// flush 
	fflush(f);
}

void log_submit(struct log_ctx *ctx, int priority, const char *fn, int ln, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	ctx->log_fn(ctx, priority, fn, ln, format, args);
	va_end(args);
}

struct log_ctx *log_init(const char *owner, int dst, int priority, int timestamp, char *filepath)
{
	struct log_ctx *l;

	// Allocate memory for the log struct 
	l = calloc(1, sizeof(*l));
	if (l == NULL)
		goto end;
	
	// Set values 
	l->owner = owner;
	l->priority = priority;
	l->timestamp = timestamp;

	if (dst == LDST_SYSLOG)
		l->log_fn = log_to_syslog;
	else if (dst == LDST_NULL)
		l->log_fn = log_to_null;
	else if (dst == LDST_FILE & filepath != NULL)
	{
		l->file = fopen(filepath, "a");
		l->log_fn = log_to_file;
	}
	else 
		l->log_fn = log_to_stdio;

end:
	return l;
}

/**
 * Free the memory used by struct log_ctx
 */
int log_free(struct log_ctx *ctx)
{
	if (ctx->file != NULL)
		fclose(ctx->file);

	if (ctx != NULL)
		free(ctx);

	return 0;
}

const char *log_priority_to_str(int priority)
{
	if (priority < 0 || priority > LOG_DEBUG)
		return NULL;
	return levels[priority];	
}

const char *log_dst_to_str(int dst)
{
	if (dst < 0 || dst > LDST_MAX)
		return NULL;
	return log_destinations[dst];
}
