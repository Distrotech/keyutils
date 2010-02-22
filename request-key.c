/* request-key.c: hand a key request off to the appropriate process
 *
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * /sbin/request-key <op> <key> <uid> <gid> <threadring> <processring> <sessionring> <info>
 *
 * Searches the specified session ring for a key indicating the command to run:
 *	type:	"user"
 *	desc:	"request-key:<op>"
 *	data:	command name, eg: "/home/dhowells/request-key-create.sh"
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include "keyutil.h"


static int xdebug;
static int xnolog;
static char *xkey;
static char *xuid;
static char *xgid;
static char *xthread_keyring;
static char *xprocess_keyring;
static char *xsession_keyring;
static int confline;

static void lookup_action(char *op,
			  key_serial_t key,
			  char *ktype,
			  char *kdesc,
			  char *callout_info)
	__attribute__((noreturn));

static void execute_program(char *op,
			    char *ktype,
			    char *kdesc,
			    char *callout_info,
			    char *cmdline)
	__attribute__((noreturn));

static int match(const char *pattern, int plen, const char *datum, int dlen);

static void debug(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void debug(const char *fmt, ...)
{
	va_list va;

	if (xdebug) {
		va_start(va, fmt);
		vfprintf(stderr, fmt, va);
		va_end(va);

		if (!xnolog) {
			openlog("request-key", 0, LOG_AUTHPRIV);

			va_start(va, fmt);
			vsyslog(LOG_DEBUG, fmt, va);
			va_end(va);

			closelog();
		}
	}
}

static void error(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
static void error(const char *fmt, ...)
{
	va_list va;

	if (xdebug) {
		va_start(va, fmt);
		vfprintf(stderr, fmt, va);
		va_end(va);
	}

	if (!xnolog) {
		openlog("request-key", 0, LOG_AUTHPRIV);

		va_start(va, fmt);
		vsyslog(LOG_ERR, fmt, va);
		va_end(va);

		closelog();
	}

	exit(1);
}

/*****************************************************************************/
/*
 *
 */
int main(int argc, char *argv[])
{
	key_serial_t key;
	char *ktype, *kdesc, *buf;
	int ret, ntype, dpos, dlen;

	for (;;) {
		if (argc > 1 && strcmp(argv[1], "-d") == 0) {
			xdebug++;
			argv++;
			argc--;
		}
		else if (argc > 1 && strcmp(argv[1], "-n") == 0) {
			xnolog = 1;
			argv++;
			argc--;
		}
		else
			break;
	}

	if (argc != 9)
		error("Unexpected argument count: %d\n", argc);

	xkey = argv[2];
	xuid = argv[3];
	xgid = argv[4];
	xthread_keyring = argv[5];
	xprocess_keyring = argv[6];
	xsession_keyring = argv[7];

	key = atoi(xkey);

	/* ask the kernel to describe the key to us */
	if (xdebug <= 0) {
		ret = keyctl_describe_alloc(key, &buf);
		if (ret < 0)
			goto inaccessible;
	}
	else {
		buf = strdup("user;0;0;1f0000;debug:1234");
	}

	/* extract the type and description from the key */
	debug("Key descriptor: \"%s\"\n", buf);
	ntype = -1;
	dpos = -1;
	dlen = -1;

	sscanf(buf, "%*[^;]%n;%*d;%*d;%*x;%n%*[^;]%n", &ntype, &dpos, &dlen);
	if (dlen == -1)
		error("Failed to parse key description\n");

	ktype = buf;
	ktype[ntype] = 0;
	kdesc = buf + dpos;

	debug("Key type: %s\n", ktype);
	debug("Key desc: %s\n", kdesc);

	/* determine the action to perform */
	lookup_action(argv[1],		/* op */
		      key,		/* ID of key under construction */
		      ktype,		/* key type */
		      kdesc,		/* key description */
		      argv[8]		/* call out info */
		      );

inaccessible:
	error("Key %d is inaccessible (%m)\n", key);

} /* end main() */

/*****************************************************************************/
/*
 * determine the action to perform
 */
static void lookup_action(char *op,
			  key_serial_t key,
			  char *ktype,
			  char *kdesc,
			  char *callout_info)
{
	char buf[4096 + 2], *p, *q;
	FILE *conf;
	int len, oplen, ktlen, kdlen, cilen;

	oplen = strlen(op);
	ktlen = strlen(ktype);
	kdlen = strlen(kdesc);
	cilen = strlen(callout_info);

	/* search the config file for a command to run */
	conf = fopen(xdebug < 2 ? "/etc/request-key.conf" : "request-key.conf", "r");
	if (!conf)
		error("Cannot open /etc/request-key.conf: %m\n");

	for (confline = 1;; confline++) {
		/* read the file line-by-line */
		if (!fgets(buf, sizeof(buf), conf)) {
			if (feof(conf))
				error("Cannot find command to construct key %d\n", key);
			error("Error reading /etc/request-key.conf\n");
		}

		len = strlen(buf);
		if (len >= sizeof(buf) - 2)
			error("/etc/request-key.conf:%d: Line too long\n", confline);

		/* ignore blank lines and comments */
		if (len == 1 || buf[0] == '#' || isspace(buf[0]))
			continue;

		buf[--len] = 0;
		p = buf;

		/* attempt to match the op */
		q = p;
		while (*p && !isspace(*p)) p++;
		if (!*p)
			goto syntax_error;
		*p = 0;

		if (!match(q, p - q, op, oplen))
			continue;

		p++;

		/* attempt to match the type */
		while (isspace(*p)) p++;
		if (!*p)
			goto syntax_error;

		q = p;
		while (*p && !isspace(*p)) p++;
		if (!*p)
			goto syntax_error;
		*p = 0;

		if (!match(q, p - q, ktype, ktlen))
			continue;

		p++;

		/* attempt to match the description */
		while (isspace(*p)) p++;
		if (!*p)
			goto syntax_error;

		q = p;
		while (*p && !isspace(*p)) p++;
		if (!*p)
			goto syntax_error;
		*p = 0;

		if (!match(q, p - q, kdesc, kdlen))
			continue;

		p++;

		/* attempt to match the callout info */
		while (isspace(*p)) p++;
		if (!*p)
			goto syntax_error;

		q = p;
		while (*p && !isspace(*p)) p++;
		if (!*p)
			goto syntax_error;
		*p = 0;

		if (!match(q, p - q, callout_info, cilen))
			continue;

		p++;

		debug("Line %d matches\n", confline);

		/* we've got an action */
		while (isspace(*p)) p++;
		if (!*p)
			goto syntax_error;

		execute_program(op, ktype, kdesc, callout_info, p);
	}

	error("/etc/request-key.conf: No matching action\n");

syntax_error:
	error("/etc/request-key.conf:%d: Syntax error\n", confline);

} /* end lookup_action() */

/*****************************************************************************/
/*
 * attempt to match a datum to a pattern
 * - one asterisk is allowed anywhere in the pattern to indicate a wildcard
 * - returns true if matched, false if not
 */
static int match(const char *pattern, int plen, const char *datum, int dlen)
{
	const char *asterisk;
	int n;

	debug("match(%*.*s,%*.*s)\n", plen, plen, pattern, dlen, dlen, datum);

	asterisk = memchr(pattern, '*', plen);
	if (!asterisk) {
		/* exact match only if no wildcard */
		if (plen == dlen && memcmp(pattern, datum, dlen) == 0)
			goto yes;
		goto no;
	}

	/* the datum mustn't be shorter than the pattern without the asterisk */
	if (dlen < plen - 1)
		goto no;

	n = asterisk - pattern;
	if (n == 0) {
		/* wildcard at beginning of pattern */
		pattern++;
		if (!*pattern)
			goto yes; /* "*" matches everything */

		/* match the end of the datum */
		plen--;
		if (memcmp(pattern, datum + (dlen - plen), plen) == 0)
			goto yes;
		goto no;
	}

	/* need to match beginning of datum for "abc*" and "abc*def" */
	if (memcmp(pattern, datum, n) != 0)
		goto no;

	if (!asterisk[1])
		goto yes; /* "abc*" matches */

	/* match the end of the datum */
	asterisk++;
	n = plen - n - 1;
	if (memcmp(pattern, datum + (dlen - n), n) == 0)
		goto yes;

no:
	debug(" = no\n");
	return 0;

yes:
	debug(" = yes\n");
	return 1;

} /* end match() */

/*****************************************************************************/
/*
 * execute a program to deal with a key
 */
static void execute_program(char *op,
			    char *ktype,
			    char *kdesc,
			    char *callout_info,
			    char *cmdline)
{
	char *argv[256];
	char *prog, *p, *q;
	int argc;

	debug("execute_program('%s')\n", cmdline);

	/* extract the path to the program to run */
	prog = p = cmdline;
	while (*p && !isspace(*p)) p++;
	if (!*p)
		error("/etc/request-key.conf:%d: No command path\n", confline);
	*p++ = 0;

	argv[0] = strrchr(prog, '/') + 1;

	/* extract the arguments */
	for (argc = 1; p; argc++) {
		while (isspace(*p)) p++;
		if (!*p)
			break;

		if (argc >= 254)
			error("/etc/request-key.conf:%d: Too many arguments\n", confline);
		argv[argc] = q = p;

		while (*p && !isspace(*p)) p++;

		if (*p)
			*p++ = 0;
		else
			p = NULL;

		debug("argv[%d]: '%s'\n", argc, argv[argc]);

		if (*q != '%')
			continue;

		/* it's a macro */
		q++;
		if (!*q)
			error("/etc/request-key.conf:%d: Missing macro name\n", confline);

		if (*q == '%') {
			/* it's actually an anti-macro escape "%%..." -> "%..." */
			argv[argc]++;
			continue;
		}

		/* single character macros */
		if (!q[1]) {
			switch (*q) {
			case 'o': argv[argc] = op;			continue;
			case 'k': argv[argc] = xkey;			continue;
			case 't': argv[argc] = ktype;			continue;
			case 'd': argv[argc] = kdesc;			continue;
			case 'c': argv[argc] = callout_info;		continue;
			case 'u': argv[argc] = xuid;			continue;
			case 'g': argv[argc] = xgid;			continue;
			case 'T': argv[argc] = xthread_keyring;		continue;
			case 'P': argv[argc] = xprocess_keyring;	continue;
			case 'S': argv[argc] = xsession_keyring;	continue;
			default:
				error("/etc/request-key.conf:%d: Unsupported macro\n", confline);
			}
		}

		/* keysub macro */
		if (*q == '{') {
			key_serial_t rqsession, keysub;
			void *tmp;
			char *ksdesc, *end, *subdata;
			int ret, loop;

			/* extract type and description */
			q++;
			ksdesc = strchr(q, ':');
			if (!ksdesc)
				error("/etc/request-key.conf:%d: Keysub macro lacks ':'\n",
				      confline);
			*ksdesc++ = 0;
			end = strchr(ksdesc, '}');
			if (!end)
				error("/etc/request-key.conf:%d: Unterminated keysub macro\n",
				      confline);

			*end++ = 0;
			if (*end)
				error("/etc/request-key.conf:%d:"
				      " Keysub macro has trailing rubbish\n",
				      confline);

			debug("Keysub: %s key \"%s\"\n", q, ksdesc);

			if (!q[0])
				error("/etc/request-key.conf:%d: Keysub type empty\n", confline);

			if (!ksdesc[0])
				error("/etc/request-key.conf:%d: Keysub description empty\n",
				      confline);

			/* look up the key in the requestor's session keyring */
			rqsession = atoi(xsession_keyring);

			keysub = keyctl_search(rqsession, q, ksdesc, 0);
			if (keysub < 0)
				error("/etc/request-key.conf:%d:"
				      " Keysub key not found: %m\n",
				      confline);

			ret = keyctl_read_alloc(keysub, &tmp);
			if (ret < 0)
				error("/etc/request-key.conf:%d:"
				      " Can't read keysub %d data: %m\n",
				      confline, keysub);
			subdata = tmp;

			for (loop = 0; loop < ret; loop++)
				if (!isprint(subdata[loop]))
					error("/etc/request-key.conf:%d:"
					      " keysub %d data not printable ('%02hhx')\n",
					      confline, keysub, subdata[loop]);

			argv[argc] = subdata;
			continue;
		}
	}

	if (argc == 0)
		error("/etc/request-key.conf:%d: No arguments\n", confline);

	argv[argc] = NULL;

	/* become the same UID/GID as the key requesting process */
	//setgid(atoi(xuid));
	//setuid(atoi(xgid));

	/* attempt to execute the command */
	if (xdebug) {
		char **ap;

		debug("Run %s\n", prog);
		for (ap = argv; *ap; ap++)
			debug("- argv[%zd] = \"%s\"\n", ap - argv, *ap);
	}

	execv(prog, argv);

	error("/etc/request-key.conf:%d: Failed to execute '%s': %m\n", confline, prog);

} /* end execute_program() */
