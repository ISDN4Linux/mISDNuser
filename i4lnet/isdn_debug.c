#include <stdarg.h>
#include "isdn_debug.h"


static unsigned int	debug_mask = 0;
static FILE		*debug_file = NULL;
static FILE		*warn_file = NULL;
static FILE		*error_file = NULL;

int
debug_init(unsigned int mask, char *dfile, char *wfile, char *efile)
{
	if (dfile) {
		if (debug_file && (debug_file != stdout))
			debug_file = freopen(dfile, "a", debug_file);
		else
			debug_file = fopen(dfile, "a");
		if (!debug_file) {
			debug_file = stdout;
			fprintf(debug_file,
				"%s: cannot open %s for debug log, using stdout\n",
				__FUNCTION__, dfile);
		}
	} else {
		if (!debug_file) {
			debug_file = stdout;
//			fprintf(debug_file,
//				"%s: using stdout for debug log\n", __FUNCTION__);
		}
	}
	if (wfile) {
		if (warn_file && (warn_file != stderr))
			warn_file = freopen(wfile, "a", warn_file);
		else
			warn_file = fopen(wfile, "a");
		if (!warn_file) {
			warn_file = stderr;
			fprintf(warn_file,
				"%s: cannot open %s for warning log, using stderr\n",
				__FUNCTION__, wfile);
		}
	} else {
		if (!warn_file) {
			warn_file = stderr;
//			fprintf(warn_file,
//				"%s: using stderr for warning log\n", __FUNCTION__);
		}
	}
	if (efile) {
		if (error_file && (error_file != stderr))
			error_file = freopen(efile, "a", error_file);
		else
			error_file = fopen(efile, "a");
		if (!error_file) {
			error_file = stderr;
			fprintf(error_file,
				"%s: cannot open %s for error log, using stderr\n",
				__FUNCTION__, efile);
		}
	} else {
		if (!error_file) {
			error_file = stderr;
//			fprintf(error_file,
//				"%s: using stderr for error log\n", __FUNCTION__);
		}
	}
	debug_mask = mask;
//	fprintf(debug_file, "%s: debug_mask = %x\n", __FUNCTION__, debug_mask);
	return(0);
}

void
debug_close(void)
{
//	fprintf(debug_file, "%s: debug channel now closed\n", __FUNCTION__);
	if (debug_file && (debug_file != stdout))
		fclose(debug_file);
//	fprintf(warn_file, "%s:  warn channel now closed\n", __FUNCTION__);
	if (warn_file && (warn_file != stderr))
		fclose(warn_file);
//	fprintf(error_file, "%s: error channel now closed\n", __FUNCTION__);
	if (error_file && (error_file != stderr))
		fclose(error_file);
}

int
dprint(unsigned int mask, const char *fmt, ...)
{
	int	ret = 0;
	va_list	args;

	va_start(args, fmt);
	if (debug_mask & mask) {
		ret = vfprintf(debug_file, fmt, args);
		if (debug_file != stdout)
			fflush(debug_file);
	}
	va_end(args);
	return(ret);
}

int
wprint(const char *fmt, ...)
{
	int	ret = 0;
	va_list	args;

	va_start(args, fmt);
	ret = vfprintf(warn_file, fmt, args);
	fflush(warn_file);
	va_end(args);
	return(ret);
}


int
eprint(const char *fmt, ...)
{
	int	ret = 0;
	va_list	args;

	va_start(args, fmt);
	ret = vfprintf(error_file, fmt, args);
	fflush(error_file);
	va_end(args);
	return(ret);
}

int
dhexprint(unsigned int mask, char *head, unsigned char *buf, int len)
{
	int	ret = 0;
	char	*p,*obuf;

	if (debug_mask & mask) {
		obuf = malloc(3*(len+1));
		if (!obuf)
			return(-ENOMEM);
		p = obuf;
		while (len) {
			p += sprintf(p,"%02x ", *buf);
			buf++;
			len--;
		}
		p--;
		*p=0;
		ret = fprintf(debug_file, "%s %s\n", head, obuf);
		free(obuf);
	}
	return(ret);
}
