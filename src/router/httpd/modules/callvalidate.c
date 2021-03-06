/*
 * DD-WRT servicemanager.c
 *
 * Copyright (C) 2005 - 2006 Sebastian Gottschall <sebastian.gottschall@newmedia-net.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Id:
 */
#ifdef WEBS
#include <uemf.h>
#include <ej.h>
#else				/* !WEBS */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <httpd.h>
#include <errno.h>
#endif				/* WEBS */

#include <proto/ethernet.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/klog.h>
#include <sys/wait.h>
#include <dd_defs.h>
#include <cy_conf.h>
// #ifdef EZC_SUPPORT
#include <ezc.h>
// #endif
#include <broadcom.h>
#include <wlutils.h>
#include <netdb.h>
#include <utils.h>

#include <dlfcn.h>
#include <stdio.h>
// #include <shutils.h>

#if defined(HAVE_ADM5120) && !defined(HAVE_WP54G)
#define SERVICE_MODULE "/lib/validate.so"
#define VISSERVICE_MODULE "/lib/visuals.so"
#else
#define SERVICE_MODULE "/usr/lib/validate.so"
#define VISSERVICE_MODULE "/usr/lib/visuals.so"
#endif

#define SERVICEALT_MODULE "/jffs/usr/lib/validate.so"
#define VISSERVICEALT_MODULE "/jffs/usr/lib/visuals.so"
//#define SERVICEALT_MODULE "/tmp/validate.so"
//#define VISSERVICEALT_MODULE "/tmp/visuals.so"

//#define SERVICE_MODULE "/tmp/validate.so"
//#define VISSERVICE_MODULE "/tmp/visuals.so"

#define cprintf(fmt, args...)

#ifndef cprintf
#define cprintf(fmt, args...) do { \
	FILE *fp = fopen("/dev/console", "w"); \
	if (fp) { \
		fprintf(fp,"%s (%d):%s ",__FILE__,__LINE__,__func__); \
		fprintf(fp, fmt, ## args); \
		fclose(fp); \
	} \
} while (0)
#endif
static size_t websWrite(webs_t wp, char *fmt, ...);
static size_t vwebsWrite(webs_t wp, char *fmt, va_list args);
static char *websGetVar(webs_t wp, char *var, char *d)
{
	return get_cgi(wp, var) ? : d;
}

static int websGetVari(webs_t wp, char *var, int d)
{
	char *res = get_cgi(wp, var);
	return res ? atoi(res) : d;
}

static char *_GOZILA_GET(webs_t wp, char *name)
{
	if (!name)
		return NULL;
	if (!wp)
		return nvram_safe_get(name);
	return wp->gozila_action ? websGetVar(wp, name, NULL) : nvram_safe_get(name);
}

static void *load_visual_service(char *name)
{
	cprintf("load service %s\n", name);
	void *handle = dlopen(VISSERVICEALT_MODULE, RTLD_LAZY | RTLD_GLOBAL);
	if (!handle) {
		dd_debug(DEBUG_HTTPD, "cannot load %s\n", name);
		handle = dlopen(VISSERVICE_MODULE, RTLD_LAZY | RTLD_GLOBAL);
	}
	cprintf("done()\n");
	if (handle == NULL && name != NULL) {
		cprintf("not found, try to load alternate\n");
		char dl[64];

		sprintf(dl, "/lib/%s_visual.so", name);
		cprintf("try to load %s\n", dl);
		handle = dlopen(name, RTLD_LAZY | RTLD_GLOBAL);
		if (handle == NULL) {
			fprintf(stderr, "cannot load %s\n", dl);
			return NULL;
		}
	}
	cprintf("found it, returning handle\n");
	return handle;
}

static void *load_service(char *name)
{
	cprintf("load service %s\n", name);
	void *handle = dlopen(SERVICEALT_MODULE, RTLD_LAZY | RTLD_GLOBAL);
	if (!handle)
		handle = dlopen(SERVICE_MODULE, RTLD_LAZY | RTLD_GLOBAL);
	cprintf("done()\n");
	if (handle == NULL && name != NULL) {
		cprintf("not found, try to load alternate\n");
		char dl[64];

		sprintf(dl, "/lib/%s_validate.so", name);
		cprintf("try to load %s\n", dl);
		handle = dlopen(dl, RTLD_LAZY | RTLD_GLOBAL);
		if (handle == NULL) {
			fprintf(stderr, "cannot load %s\n", dl);
			return NULL;
		}
	}
	cprintf("found it, returning handle\n");
	return handle;
}

extern const websRomPageIndexType websRomPageIndex[];

static char *_live_translate(webs_t wp, const char *tran);
static void _validate_cgi(webs_t wp);

static void *s_service = NULL;

static void start_gozila(char *name, webs_t wp)
{
	// lcdmessaged("Starting Service",name);
	cprintf("start_gozila %s\n", name);
	char service[64];
	if (!s_service) {
		s_service = load_service(name);
	}
	if (s_service == NULL) {
		return;
	}

	int (*fptr) (webs_t wp);

	sprintf(service, "%s", name);
	cprintf("resolving %s\n", service);
	fptr = (int (*)(webs_t wp))dlsym(s_service, service);
	if (fptr)
		(*fptr) (wp);
	else
		dd_debug(DEBUG_HTTPD, "function %s not found \n", service);
#ifndef MEMLEAK_OVERRIDE
	dlclose(s_service);
	s_service = NULL;
#endif
	cprintf("start_sevice done()\n");
}

static int start_validator(char *name, webs_t wp, char *value, struct variable *v)
{
	// lcdmessaged("Starting Service",name);
	cprintf("start_validator %s\n", name);
	char service[64];
	if (!s_service) {
		s_service = load_service(name);
	}
	if (s_service == NULL) {
		return FALSE;
	}
	int ret = FALSE;

	int (*fptr) (webs_t wp, char *value, struct variable * v);

	sprintf(service, "%s", name);
	cprintf("resolving %s\n", service);
	fptr = (int (*)(webs_t wp, char *value, struct variable * v))
	    dlsym(s_service, service);
	if (fptr)
		ret = (*fptr) (wp, value, v);
	else
		dd_debug(DEBUG_HTTPD, "function %s not found \n", service);
#ifndef MEMLEAK_OVERRIDE
	dlclose(s_service);
	s_service = NULL;
#endif

	cprintf("start_sevice done()\n");
	return ret;
}

static void *start_validator_nofree(char *name, void *handle, webs_t wp, char *value, struct variable *v)
{
	// lcdmessaged("Starting Service",name);
	cprintf("start_service_nofree %s\n", name);
	char service[64];

	if (!handle) {
		handle = load_service(name);
	}
	if (handle == NULL) {
		return NULL;
	}
	void (*fptr) (webs_t wp, char *value, struct variable * v);

	sprintf(service, "%s", name);
	cprintf("resolving %s\n", service);
	fptr = (void (*)(webs_t wp, char *value, struct variable * v))
	    dlsym(handle, service);
	cprintf("found. pointer is %p\n", fptr);
	if (fptr)
		(*fptr) (wp, value, v);
	else
		dd_debug(DEBUG_HTTPD, "function %s not found \n", service);
	cprintf("start_sevice_nofree done()\n");
	return handle;
}

static void *call_ej(char *name, void *handle, webs_t wp, int argc, char_t ** argv)
{
	struct timeval before, after, r;

	if (nvram_matchi("httpd_debug", 1)) {
		dd_syslog(LOG_INFO, "%s:%s", __func__, name);
		fprintf(stderr, "call_ej %s", name);
		int i = 0;
		for (i = 0; i < argc; i++)
			fprintf(stderr, " %s", argv[i]);
	}
	char service[64];

	{
		memdebug_enter();
		if (!handle) {
			cprintf("load visual_service\n");
			handle = load_visual_service(name);
		}
		memdebug_leave_info("loadviz");
	}
	if (handle == NULL) {
		cprintf("handle null\n");
		return NULL;
	}
	cprintf("pointer init\n");
	void (*fptr) (webs_t wp, int argc, char_t ** argv);

	sprintf(service, "ej_%s", name);
	cprintf("resolving %s\n", service);
	fptr = (void (*)(webs_t wp, int argc, char_t ** argv))dlsym(handle, service);

	cprintf("found. pointer is %p\n", fptr);
	{
		memdebug_enter();
		if (fptr) {
			gettimeofday(&before, NULL);
			(*fptr) (wp, argc, argv);
			gettimeofday(&after, NULL);
			timersub(&after, &before, &r);
			dd_debug(DEBUG_HTTPD, " %s duration %ld.%06ld\n", service, (long int)r.tv_sec, (long int)r.tv_usec);

		} else
			dd_debug(DEBUG_HTTPD, " function %s not found \n", service);
		memdebug_leave_info(service);
	}
	cprintf("start_sevice_nofree done()\n");
	return handle;

}
