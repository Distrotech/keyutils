/* keyctl.c: key control program
 *
 * Copyright (C) 2005 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <asm/unistd.h>
#include "keyutils.h"

struct command {
	int (*action)(int argc, char *argv[]);
	const char	*name;
	const char	*format;
};

static int act_keyctl_show(int argc, char *argv[]);
static int act_keyctl_add(int argc, char *argv[]);
static int act_keyctl_padd(int argc, char *argv[]);
static int act_keyctl_request(int argc, char *argv[]);
static int act_keyctl_request2(int argc, char *argv[]);
static int act_keyctl_prequest2(int argc, char *argv[]);
static int act_keyctl_update(int argc, char *argv[]);
static int act_keyctl_pupdate(int argc, char *argv[]);
static int act_keyctl_newring(int argc, char *argv[]);
static int act_keyctl_revoke(int argc, char *argv[]);
static int act_keyctl_clear(int argc, char *argv[]);
static int act_keyctl_link(int argc, char *argv[]);
static int act_keyctl_unlink(int argc, char *argv[]);
static int act_keyctl_search(int argc, char *argv[]);
static int act_keyctl_read(int argc, char *argv[]);
static int act_keyctl_pipe(int argc, char *argv[]);
static int act_keyctl_print(int argc, char *argv[]);
static int act_keyctl_list(int argc, char *argv[]);
static int act_keyctl_rlist(int argc, char *argv[]);
static int act_keyctl_describe(int argc, char *argv[]);
static int act_keyctl_rdescribe(int argc, char *argv[]);
static int act_keyctl_chown(int argc, char *argv[]);
static int act_keyctl_chgrp(int argc, char *argv[]);
static int act_keyctl_setperm(int argc, char *argv[]);
static int act_keyctl_session(int argc, char *argv[]);
static int act_keyctl_instantiate(int argc, char *argv[]);
static int act_keyctl_pinstantiate(int argc, char *argv[]);
static int act_keyctl_negate(int argc, char *argv[]);
static int act_keyctl_timeout(int argc, char *argv[]);

const struct command commands[] = {
	{ act_keyctl_show,	"show",		"" },
	{ act_keyctl_add,	"add",		"<type> <desc> <data> <keyring>" },
	{ act_keyctl_padd,	"padd",		"<type> <desc> <keyring>" },
	{ act_keyctl_request,	"request",	"<type> <desc> [<dest_keyring>]" },
	{ act_keyctl_request2,	"request2",	"<type> <desc> <info> [<dest_keyring>]" },
	{ act_keyctl_prequest2,	"prequest2",	"<type> <desc> [<dest_keyring>]" },
	{ act_keyctl_update,	"update",	"<key> <data>" },
	{ act_keyctl_pupdate,	"pupdate",	"<key>" },
	{ act_keyctl_newring,	"newring",	"<name> <keyring>" },
	{ act_keyctl_revoke,	"revoke",	"<key>" },
	{ act_keyctl_clear,	"clear",	"<keyring>" },
	{ act_keyctl_link,	"link",		"<key> <keyring>" },
	{ act_keyctl_unlink,	"unlink",	"<key> <keyring>" },
	{ act_keyctl_search,	"search",	"<keyring> <type> <desc> [<dest_keyring>]" },
	{ act_keyctl_read,	"read",		"<key>" },
	{ act_keyctl_pipe,	"pipe",		"<key>" },
	{ act_keyctl_print,	"print",	"<key>" },
	{ act_keyctl_list,	"list",		"<keyring>" },
	{ act_keyctl_rlist,	"rlist",	"<keyring>" },
	{ act_keyctl_describe,	"describe",	"<keyring>" },
	{ act_keyctl_rdescribe,	"rdescribe",	"<keyring> [sep]" },
	{ act_keyctl_chown,	"chown",	"<key> <uid>" },
	{ act_keyctl_chgrp,	"chgrp",	"<key> <gid>" },
	{ act_keyctl_setperm,	"setperm",	"<key> <mask>" },
	{ act_keyctl_session,	"session",	"" },
	{ act_keyctl_session,	"session",	"- [<prog> <arg1> <arg2> ...]" },
	{ act_keyctl_session,	"session",	"<name> [<prog> <arg1> <arg2> ...]" },
	{ act_keyctl_instantiate, "instantiate","<key> <data> <keyring>" },
	{ act_keyctl_pinstantiate, "pinstantiate","<key> <keyring>" },
	{ act_keyctl_negate,	"negate",	"<key> <timeout> <keyring>" },
	{ act_keyctl_timeout,	"timeout",	"<key> <timeout>" },
	{ NULL, NULL, NULL }
};

static int dump_key_tree(key_serial_t keyring, const char *name);
static void format(void) __attribute__((noreturn));
static void error(const char *msg) __attribute__((noreturn));
static key_serial_t get_key_id(const char *arg);

static uid_t myuid;
static gid_t mygid, *mygroups;
static int myngroups;

/*****************************************************************************/
/*
 * handle an error
 */
static inline void error(const char *msg)
{
	perror(msg);
	exit(1);

} /* end error() */

/*****************************************************************************/
/*
 * execute the appropriate subcommand
 */
int main(int argc, char *argv[])
{
	const struct command *cmd, *best;
	int n;

	argv++;
	argc--;

	if (argc == 0)
		format();

	/* find the best fit command */
	best = NULL;
	n = strlen(*argv);

	for (cmd = commands; cmd->action; cmd++) {
		if (memcmp(cmd->name, *argv, n) != 0)
			continue;

		if (cmd->name[n] == 0) {
			/* exact match */
			best = cmd;
			break;
		}

		/* partial match */
		if (best) {
			fprintf(stderr, "Ambiguous command\n");
			exit(2);
		}

		best = cmd;
	}

	if (!best) {
		fprintf(stderr, "Unknown command\n");
		exit(2);
	}

	/* grab my UID, GID and groups */
	myuid = geteuid();
	mygid = getegid();
	myngroups = getgroups(0, NULL);

	if (myuid == -1 || mygid == -1 || myngroups == -1)
		error("Unable to get UID/GID/#Groups\n");

	mygroups = calloc(myngroups, sizeof(gid_t));
	if (!mygroups)
		error("calloc");

	myngroups = getgroups(myngroups, mygroups);
	if (myngroups < 0)
		error("Unable to get Groups\n");

	return best->action(argc, argv);

} /* end main() */

/*****************************************************************************/
/*
 * display command format information
 */
static void format(void)
{
	const struct command *cmd;

	fprintf(stderr, "Format:\n");

	for (cmd = commands; cmd->action; cmd++)
		fprintf(stderr, "  keyctl %s %s\n", cmd->name, cmd->format);

	fprintf(stderr, "\n");
	fprintf(stderr, "Key/keyring ID:\n");
	fprintf(stderr, "  <nnn>   numeric keyring ID\n");
	fprintf(stderr, "  @t      thread keyring\n");
	fprintf(stderr, "  @p      process keyring\n");
	fprintf(stderr, "  @s      session keyring\n");
	fprintf(stderr, "  @u      user keyring\n");
	fprintf(stderr, "  @us     user default session keyring\n");
	fprintf(stderr, "  @g      group keyring\n");
	fprintf(stderr, "  @a      assumed request_key authorisation key\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "<type> can be \"user\" for a user-defined keyring\n");
	fprintf(stderr, "If you do this, prefix the description with \"<subtype>:\"\n");

	exit(2);

} /* end format() */

/*****************************************************************************/
/*
 * grab data from stdin
 */
static char *grab_stdin(void)
{
	static char input[65536 + 1];
	int n, tmp;

	n = 0;
	do {
		tmp = read(0, input + n, sizeof(input) - 1 - n);
		if (tmp < 0)
			error("stdin");

		if (tmp == 0)
			break;

		n += tmp;

	} while (n < sizeof(input));

	if (n >= sizeof(input)) {
		fprintf(stderr, "Too much data read on stdin\n");
		exit(1);
	}

	input[n] = '\0';

	return input;

} /* end grab_stdin() */

/*****************************************************************************/
/*
 * convert the permissions mask to a string representing the permissions we
 * have actually been granted
 */
static void calc_perms(char *pretty, key_perm_t perm, uid_t uid, gid_t gid)
{
	unsigned perms;
	gid_t *pg;
	int loop;

	perms = (perm & KEY_POS_ALL) >> 24;

	if (uid == myuid) {
		perms |= (perm & KEY_USR_ALL) >> 16;
		goto write_mask;
	}

	if (gid != -1) {
		if (gid == mygid) {
			perms |= (perm & KEY_GRP_ALL) >> 8;
			goto write_mask;
		}

		pg = mygroups;
		for (loop = myngroups; loop > 0; loop--, pg++) {
			if (gid == *pg) {
				perms |= (perm & KEY_GRP_ALL) >> 8;
				goto write_mask;
			}
		}
	}

	perms |= (perm & KEY_OTH_ALL);

write_mask:
	sprintf(pretty, "--%c%c%c%c%c%c",
		perms & KEY_OTH_SETATTR	? 'a' : '-',
		perms & KEY_OTH_LINK	? 'l' : '-',
		perms & KEY_OTH_SEARCH	? 's' : '-',
		perms & KEY_OTH_WRITE	? 'w' : '-',
		perms & KEY_OTH_READ	? 'r' : '-',
		perms & KEY_OTH_VIEW	? 'v' : '-');

} /* end calc_perms() */

/*****************************************************************************/
/*
 * show the parent process's session keyring
 */
static int act_keyctl_show(int argc, char *argv[])
{
	if (argc != 1)
		format();

	dump_key_tree(KEY_SPEC_SESSION_KEYRING, "Session Keyring");
	return 0;

} /* end act_keyctl_show() */

/*****************************************************************************/
/*
 * add a key
 */
static int act_keyctl_add(int argc, char *argv[])
{
	key_serial_t dest;
	int ret;

	if (argc != 5)
		format();

	dest = get_key_id(argv[4]);

	ret = add_key(argv[1], argv[2], argv[3], strlen(argv[3]), dest);
	if (ret < 0)
		error("add_key");

	/* print the resulting key ID */
	printf("%d\n", ret);
	return 0;

} /* end act_keyctl_add() */

/*****************************************************************************/
/*
 * add a key, reading from a pipe
 */
static int act_keyctl_padd(int argc, char *argv[])
{
	char *args[6];

	if (argc != 4)
		format();

	args[0] = argv[0];
	args[1] = argv[1];
	args[2] = argv[2];
	args[3] = grab_stdin();
	args[4] = argv[3];
	args[5] = NULL;

	return act_keyctl_add(5, args);

} /* end act_keyctl_padd() */

/*****************************************************************************/
/*
 * request a key
 */
static int act_keyctl_request(int argc, char *argv[])
{
	key_serial_t dest;
	int ret;

	if (argc != 3 && argc != 4)
		format();

	dest = 0;
	if (argc == 4)
		dest = get_key_id(argv[3]);

	ret = request_key(argv[1], argv[2], NULL, dest);
	if (ret < 0)
		error("request_key");

	/* print the resulting key ID */
	printf("%d\n", ret);
	return 0;

} /* end act_keyctl_request() */

/*****************************************************************************/
/*
 * request a key, with recourse to /sbin/request-key
 */
static int act_keyctl_request2(int argc, char *argv[])
{
	key_serial_t dest;
	int ret;

	if (argc != 4 && argc != 5)
		format();

	dest = 0;
	if (argc == 5)
		dest = get_key_id(argv[4]);

	ret = request_key(argv[1], argv[2], argv[3], dest);
	if (ret < 0)
		error("request_key");

	/* print the resulting key ID */
	printf("%d\n", ret);
	return 0;

} /* end act_keyctl_request2() */

/*****************************************************************************/
/*
 * request a key, with recourse to /sbin/request-key, reading the callout info
 * from a pipe
 */
static int act_keyctl_prequest2(int argc, char *argv[])
{
	char *args[6];

	if (argc != 3 && argc != 4)
		format();

	args[0] = argv[0];
	args[1] = argv[1];
	args[2] = argv[2];
	args[3] = grab_stdin();
	args[4] = argv[3];
	args[5] = NULL;

	return act_keyctl_request2(argc + 1, args);

} /* end act_keyctl_prequest2() */

/*****************************************************************************/
/*
 * update a key
 */
static int act_keyctl_update(int argc, char *argv[])
{
	key_serial_t key;

	if (argc != 3)
		format();

	key = get_key_id(argv[1]);

	if (keyctl_update(key, argv[2], strlen(argv[2])) < 0)
		error("keyctl_update");

	return 0;

} /* end act_keyctl_update() */

/*****************************************************************************/
/*
 * update a key, reading from a pipe
 */
static int act_keyctl_pupdate(int argc, char *argv[])
{
	char *args[4];

	if (argc != 2)
		format();

	args[0] = argv[0];
	args[1] = argv[1];
	args[2] = grab_stdin();
	args[3] = NULL;

	return act_keyctl_update(3, args);

} /* end act_keyctl_pupdate() */

/*****************************************************************************/
/*
 * create a new keyring
 */
static int act_keyctl_newring(int argc, char *argv[])
{
	key_serial_t dest;
	int ret;

	if (argc != 3)
		format();

	dest = get_key_id(argv[2]);

	ret = add_key("keyring", argv[1], NULL, 0, dest);
	if (ret < 0)
		error("add_key");

	printf("%d\n", ret);
	return 0;

} /* end act_keyctl_newring() */

/*****************************************************************************/
/*
 * revoke a key
 */
static int act_keyctl_revoke(int argc, char *argv[])
{
	key_serial_t key;

	if (argc != 2)
		format();

	key = get_key_id(argv[1]);

	if (keyctl_revoke(key) < 0)
		error("keyctl_revoke");

	return 0;

} /* end act_keyctl_revoke() */

/*****************************************************************************/
/*
 * clear a keyring
 */
static int act_keyctl_clear(int argc, char *argv[])
{
	key_serial_t keyring;

	if (argc != 2)
		format();

	keyring = get_key_id(argv[1]);

	if (keyctl_clear(keyring) < 0)
		error("keyctl_clear");

	return 0;

} /* end act_keyctl_clear() */

/*****************************************************************************/
/*
 * link a key to a keyring
 */
static int act_keyctl_link(int argc, char *argv[])
{
	key_serial_t keyring, key;

	if (argc != 3)
		format();

	key = get_key_id(argv[1]);
	keyring = get_key_id(argv[2]);

	if (keyctl_link(key, keyring) < 0)
		error("keyctl_link");

	return 0;

} /* end act_keyctl_link() */

/*****************************************************************************/
/*
 * unlink a key from a keyrign
 */
static int act_keyctl_unlink(int argc, char *argv[])
{
	key_serial_t keyring, key;

	if (argc != 3)
		format();

	key = get_key_id(argv[1]);
	keyring = get_key_id(argv[2]);

	if (keyctl_unlink(key, keyring) < 0)
		error("keyctl_unlink");

	return 0;

} /* end act_keyctl_unlink() */

/*****************************************************************************/
/*
 * search a keyring for a key
 */
static int act_keyctl_search(int argc, char *argv[])
{
	key_serial_t keyring, dest;
	int ret;

	if (argc != 4 && argc != 5)
		format();

	keyring = get_key_id(argv[1]);

	dest = 0;
	if (argc == 5)
		dest = get_key_id(argv[4]);

	ret = keyctl_search(keyring, argv[2], argv[3], dest);
	if (ret < 0)
		error("keyctl_search");

	/* print the ID of the key we found */
	printf("%d\n", ret);
	return 0;

} /* end act_keyctl_search() */

/*****************************************************************************/
/*
 * read a key
 */
static int act_keyctl_read(int argc, char *argv[])
{
	key_serial_t key;
	void *buffer;
	char *p;
	int ret, sep, col;

	if (argc != 2)
		format();

	key = get_key_id(argv[1]);

	/* read the key payload data */
	ret = keyctl_read_alloc(key, &buffer);
	if (ret < 0)
		error("keyctl_read_alloc");

	if (ret == 0) {
		printf("No data in key\n");
		return 0;
	}

	/* hexdump the contents */
	printf("%u bytes of data in key:\n", ret);

	sep = 0;
	col = 0;
	p = buffer;

	do {
		if (sep) {
			putchar(sep);
			sep = 0;
		}

		printf("%02hhx", *p);
		p++;

		col++;
		if (col % 32 == 0)
			sep = '\n';
		else if (col % 4 == 0)
			sep = ' ';

	} while (--ret > 0);

	printf("\n");
	return 0;

} /* end act_keyctl_read() */

/*****************************************************************************/
/*
 * read a key and dump raw to stdout
 */
static int act_keyctl_pipe(int argc, char *argv[])
{
	key_serial_t key;
	void *buffer;
	int ret;

	if (argc != 2)
		format();

	key = get_key_id(argv[1]);

	/* read the key payload data */
	ret = keyctl_read_alloc(key, &buffer);
	if (ret < 0)
		error("keyctl_read_alloc");

	if (ret > 0 && write(1, buffer, ret) < 0)
		error("write");
	return 0;

} /* end act_keyctl_pipe() */

/*****************************************************************************/
/*
 * read a key and dump to stdout in printable form
 */
static int act_keyctl_print(int argc, char *argv[])
{
	key_serial_t key;
	void *buffer;
	char *p;
	int loop, ret;

	if (argc != 2)
		format();

	key = get_key_id(argv[1]);

	/* read the key payload data */
	ret = keyctl_read_alloc(key, &buffer);
	if (ret < 0)
		error("keyctl_read_alloc");

	/* see if it's printable */
	p = buffer;
	for (loop = ret; loop > 0; loop--, p++)
		if (!isprint(*p))
			goto not_printable;

	/* it is */
	printf("%s\n", (char *) buffer);
	return 0;

not_printable:
	/* it isn't */
	printf(":hex:");
	p = buffer;
	for (loop = ret; loop > 0; loop--, p++)
		printf("%02hhx", *p);
	printf("\n");
	return 0;

} /* end act_keyctl_print() */

/*****************************************************************************/
/*
 * list a keyring
 */
static int act_keyctl_list(int argc, char *argv[])
{
	key_serial_t keyring, key, *pk;
	key_perm_t perm;
	void *keylist;
	char *buffer, pretty_mask[9];
	uid_t uid;
	gid_t gid;
	int count, tlen, dpos, n, ret;

	if (argc != 2)
		format();

	keyring = get_key_id(argv[1]);

	/* read the key payload data */
	count = keyctl_read_alloc(keyring, &keylist);
	if (count < 0)
		error("keyctl_read_alloc");

	count /= sizeof(key_serial_t);

	if (count == 0) {
		printf("keyring is empty\n");
		return 0;
	}

	/* list the keys in the keyring */
	if (count == 1)
		printf("1 key in keyring:\n");
	else
		printf("%u keys in keyring:\n", count);

	pk = keylist;
	do {
		key = *pk++;

		ret = keyctl_describe_alloc(key, &buffer);
		if (ret < 0) {
			printf("%9d: key inaccessible (%m)\n", key);
			continue;
		}

		uid = 0;
		gid = 0;
		perm = 0;

		tlen = -1;
		dpos = -1;

		n = sscanf((char *) buffer, "%*[^;]%n;%d;%d;%x;%n",
			   &tlen, &uid, &gid, &perm, &dpos);
		if (n != 3) {
			fprintf(stderr, "Unparseable description obtained for key %d\n", key);
			exit(3);
		}

		calc_perms(pretty_mask, perm, uid, gid);

		printf("%9d: %s %5d %5d %*.*s: %s\n",
		       key,
		       pretty_mask,
		       uid, gid,
		       tlen, tlen, buffer,
		       buffer + dpos);

		free(buffer);

	} while (--count);

	return 0;

} /* end act_keyctl_list() */

/*****************************************************************************/
/*
 * produce a raw list of a keyring
 */
static int act_keyctl_rlist(int argc, char *argv[])
{
	key_serial_t keyring, key, *pk;
	void *keylist;
	int count;

	if (argc != 2)
		format();

	keyring = get_key_id(argv[1]);

	/* read the key payload data */
	count = keyctl_read_alloc(keyring, &keylist);
	if (count < 0)
		error("keyctl_read_alloc");

	count /= sizeof(key_serial_t);

	/* list the keys in the keyring */
	if (count <= 0) {
		printf("\n");
	}
	else {
		pk = keylist;
		for (; count > 0; count--) {
			key = *pk++;
			printf("%d%c", key, count == 1 ? '\n' : ' ');
		}
	}

	return 0;

} /* end act_keyctl_rlist() */

/*****************************************************************************/
/*
 * describe a key
 */
static int act_keyctl_describe(int argc, char *argv[])
{
	key_serial_t key;
	key_perm_t perm;
	char *buffer;
	uid_t uid;
	gid_t gid;
	int tlen, dpos, n, ret;

	if (argc != 2)
		format();

	key = get_key_id(argv[1]);

	/* get key description */
	ret = keyctl_describe_alloc(key, &buffer);
	if (ret < 0)
		error("keyctl_describe");

	/* parse it */
	uid = 0;
	gid = 0;
	perm = 0;

	tlen = -1;
	dpos = -1;

	n = sscanf(buffer, "%*[^;]%n;%d;%d;%x;%n",
		   &tlen, &uid, &gid, &perm, &dpos);
	if (n != 3) {
		fprintf(stderr, "Unparseable description obtained for key %d\n", key);
		exit(3);
	}

	/* display it */
	printf("%9d:"
	       " %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c"
	       " %5d %5d %*.*s: %s\n",
	       key,
	       perm & KEY_POS_SETATTR	? 'a' : '-',
	       perm & KEY_POS_LINK	? 'l' : '-',
	       perm & KEY_POS_SEARCH	? 's' : '-',
	       perm & KEY_POS_WRITE	? 'w' : '-',
	       perm & KEY_POS_READ	? 'r' : '-',
	       perm & KEY_POS_VIEW	? 'v' : '-',

	       perm & KEY_USR_SETATTR	? 'a' : '-',
	       perm & KEY_USR_LINK	? 'l' : '-',
	       perm & KEY_USR_SEARCH	? 's' : '-',
	       perm & KEY_USR_WRITE	? 'w' : '-',
	       perm & KEY_USR_READ	? 'r' : '-',
	       perm & KEY_USR_VIEW	? 'v' : '-',

	       perm & KEY_GRP_SETATTR	? 'a' : '-',
	       perm & KEY_GRP_LINK	? 'l' : '-',
	       perm & KEY_GRP_SEARCH	? 's' : '-',
	       perm & KEY_GRP_WRITE	? 'w' : '-',
	       perm & KEY_GRP_READ	? 'r' : '-',
	       perm & KEY_GRP_VIEW	? 'v' : '-',

	       perm & KEY_OTH_SETATTR	? 'a' : '-',
	       perm & KEY_OTH_LINK	? 'l' : '-',
	       perm & KEY_OTH_SEARCH	? 's' : '-',
	       perm & KEY_OTH_WRITE	? 'w' : '-',
	       perm & KEY_OTH_READ	? 'r' : '-',
	       perm & KEY_OTH_VIEW	? 'v' : '-',
	       uid, gid,
	       tlen, tlen, buffer,
	       buffer + dpos);

	return 0;

} /* end act_keyctl_describe() */

/*****************************************************************************/
/*
 * get raw key description
 */
static int act_keyctl_rdescribe(int argc, char *argv[])
{
	key_serial_t key;
	char *buffer, *q;
	int ret;

	if (argc != 2 && argc != 3)
		format();
	if (argc == 3 && !argv[2][0])
		format();

	key = get_key_id(argv[1]);

	/* get key description */
	ret = keyctl_describe_alloc(key, &buffer);
	if (ret < 0)
		error("keyctl_describe");

	/* replace semicolon separators with requested alternative */
	if (argc == 3) {
		for (q = buffer; *q; q++)
			if (*q == ';')
				*q = argv[2][0];
	}

	/* display raw description */
	printf("%s\n", buffer);
	return 0;

} /* end act_keyctl_rdescribe() */

/*****************************************************************************/
/*
 * change a key's ownership
 */
static int act_keyctl_chown(int argc, char *argv[])
{
	key_serial_t key;
	uid_t uid;
	char *q;

	if (argc != 3)
		format();

	key = get_key_id(argv[1]);

	uid = strtoul(argv[2], &q, 0);
	if (*q) {
		fprintf(stderr, "Unparsable uid: '%s'\n", argv[2]);
		exit(2);
	}

	if (keyctl_chown(key, uid, -1) < 0)
		error("keyctl_chown");

	return 0;

} /* end act_keyctl_chown() */

/*****************************************************************************/
/*
 * change a key's group ownership
 */
static int act_keyctl_chgrp(int argc, char *argv[])
{
	key_serial_t key;
	gid_t gid;
	char *q;

	if (argc != 3)
		format();

	key = get_key_id(argv[1]);

	gid = strtoul(argv[2], &q, 0);
	if (*q) {
		fprintf(stderr, "Unparsable gid: '%s'\n", argv[2]);
		exit(2);
	}

	if (keyctl_chown(key, -1, gid) < 0)
		error("keyctl_chown");

	return 0;

} /* end act_keyctl_chgrp() */

/*****************************************************************************/
/*
 * set the permissions on a key
 */
static int act_keyctl_setperm(int argc, char *argv[])
{
	key_serial_t key;
	key_perm_t perm;
	char *q;

	if (argc != 3)
		format();

	key = get_key_id(argv[1]);
	perm = strtoul(argv[2], &q, 0);
	if (*q) {
		fprintf(stderr, "Unparsable permissions: '%s'\n", argv[2]);
		exit(2);
	}

	if (keyctl_setperm(key, perm) < 0)
		error("keyctl_setperm");

	return 0;

} /* end act_keyctl_setperm() */

/*****************************************************************************/
/*
 * start a process in a new session
 */
static int act_keyctl_session(int argc, char *argv[])
{
	char *p, *q;
	int ret;

	argv++;
	argc--;

	/* no extra arguments signifies a standard shell in an anonymous
	 * session */
	p = NULL;
	if (argc != 0) {
		/* a dash signifies an anonymous session */
		p = *argv;
		if (strcmp(p, "-") == 0)
			p = NULL;

		argv++;
		argc--;
	}

	/* create a new session keyring */
	ret = keyctl_join_session_keyring(p);
	if (ret < 0)
		error("keyctl_join_session_keyring");

	fprintf(stderr, "Joined session keyring: %d\n", ret);

	/* run the standard shell if no arguments */
	if (argc == 0) {
		q = getenv("SHELL");
		if (!q)
			q = "/bin/sh";
		execl(q, q, NULL);
		error(q);
	}

	/* run the command specified */
	execvp(argv[0], argv);
	error(argv[0]);

} /* end act_keyctl_session() */

/*****************************************************************************/
/*
 * instantiate a key that's under construction
 */
static int act_keyctl_instantiate(int argc, char *argv[])
{
	key_serial_t key, dest;

	if (argc != 4)
		format();

	key = get_key_id(argv[1]);
	dest = get_key_id(argv[3]);

	if (keyctl_instantiate(key, argv[2], strlen(argv[2]), dest) < 0)
		error("keyctl_instantiate");

	return 0;

} /* end act_keyctl_instantiate() */

/*****************************************************************************/
/*
 * instantiate a key, reading from a pipe
 */
static int act_keyctl_pinstantiate(int argc, char *argv[])
{
	char *args[5];

	if (argc != 3)
		format();

	args[0] = argv[0];
	args[1] = argv[1];
	args[2] = grab_stdin();
	args[3] = argv[2];
	args[4] = NULL;

	return act_keyctl_instantiate(4, args);

} /* end act_keyctl_pinstantiate() */

/*****************************************************************************/
/*
 * negate a key that's under construction
 */
static int act_keyctl_negate(int argc, char *argv[])
{
	unsigned long timeout;
	key_serial_t key, dest;
	char *q;

	if (argc != 4)
		format();

	key = get_key_id(argv[1]);

	timeout = strtoul(argv[2], &q, 10);
	if (*q) {
		fprintf(stderr, "Unparsable timeout: '%s'\n", argv[2]);
		exit(2);
	}

	dest = get_key_id(argv[3]);

	if (keyctl_negate(key, timeout, dest) < 0)
		error("keyctl_negate");

	return 0;

} /* end act_keyctl_negate() */

/*****************************************************************************/
/*
 * set a key's timeout
 */
static int act_keyctl_timeout(int argc, char *argv[])
{
	unsigned long timeout;
	key_serial_t key;
	char *q;

	if (argc != 3)
		format();

	key = get_key_id(argv[1]);

	timeout = strtoul(argv[2], &q, 10);
	if (*q) {
		fprintf(stderr, "Unparsable timeout: '%s'\n", argv[2]);
		exit(2);
	}

	if (keyctl_set_timeout(key, timeout) < 0)
		error("keyctl_set_timeout");

	return 0;

} /* end act_keyctl_timeout() */

/*****************************************************************************/
/*
 * parse a key identifier
 */
static key_serial_t get_key_id(const char *arg)
{
	key_serial_t id;
	char *end;

	/* handle a special keyring name */
	if (arg[0] == '@') {
		if (strcmp(arg, "@t" ) == 0) return KEY_SPEC_THREAD_KEYRING;
		if (strcmp(arg, "@p" ) == 0) return KEY_SPEC_PROCESS_KEYRING;
		if (strcmp(arg, "@s" ) == 0) return KEY_SPEC_SESSION_KEYRING;
		if (strcmp(arg, "@u" ) == 0) return KEY_SPEC_USER_KEYRING;
		if (strcmp(arg, "@us") == 0) return KEY_SPEC_USER_SESSION_KEYRING;
		if (strcmp(arg, "@g" ) == 0) return KEY_SPEC_GROUP_KEYRING;
		if (strcmp(arg, "@a" ) == 0) return KEY_SPEC_REQKEY_AUTH_KEY;

		fprintf(stderr, "Unknown special key: '%s'\n", arg);
		exit(2);
	}

	/* handle a numeric key ID */
	id = strtoul(arg, &end, 0);
	if (*end) {
		fprintf(stderr, "Unparsable key: '%s'\n", arg);
		exit(2);
	}

	return id;

} /* end get_key_id() */

/*****************************************************************************/
/*
 * recursively display a key/keyring tree
 */
static int dump_key_tree_aux(key_serial_t key, int depth, int more)
{
	static char dumpindent[64];
	key_serial_t *pk;
	key_perm_t perm;
	size_t ringlen, desclen;
	void *payload;
	char *desc, type[255], pretty_mask[9];
	int uid, gid, ret, n, dpos, rdepth, kcount = 0;

	if (depth > 8)
		return 0;

	/* find out how big this key's description is */
	ret = keyctl_describe(key, NULL, 0);
	if (ret < 0) {
		printf("%d: key inaccessible (%m)\n", key);
		return 0;
	}
	desclen = ret + 1;

	desc = malloc(desclen);
	if (!desc)
		error("malloc");

	/* read the description */
	ret = keyctl_describe(key, desc, desclen);
	if (ret < 0) {
		printf("%d: key inaccessible (%m)\n", key);
		free(desc);
		return 0;
	}

	desclen = ret < desclen ? ret : desclen;

	desc[desclen] = 0;

	/* parse */
	type[0] = 0;
	uid = 0;
	gid = 0;
	perm = 0;

	n = sscanf(desc, "%[^;];%d;%d;%x;%n",
		   type, &uid, &gid, &perm, &dpos);

	if (n != 4) {
		fprintf(stderr, "Unparseable description obtained for key %d\n", key);
		exit(3);
	}

	/* and print */
	calc_perms(pretty_mask, perm, uid, gid);

	printf("%9d %s  %5d %5d  %s%s%s: %s\n",
	       key,
	       pretty_mask,
	       uid, gid,
	       dumpindent,
	       depth > 0 ? "\\_ " : "",
	       type, desc + dpos);

	/* if it's a keyring then we're going to want to recursively
	 * display it if we can */
	if (strcmp(type, "keyring") == 0) {
		/* find out how big the keyring is */
		ret = keyctl_read(key, NULL, 0);
		if (ret < 0)
			error("keyctl_read");
		if (ret == 0)
			return 0;
		ringlen = ret;

		/* read its contents */
		payload = malloc(ringlen);
		if (!payload)
			error("malloc");

		ret = keyctl_read(key, payload, ringlen);
		if (ret < 0)
			error("keyctl_read");

		ringlen = ret < ringlen ? ret : ringlen;
		kcount = ringlen / sizeof(key_serial_t);

		/* walk the keyring */
		pk = payload;
		do {
			key = *pk++;

			/* recurse into nexted keyrings */
			if (strcmp(type, "keyring") == 0) {
				if (depth == 0) {
					rdepth = depth;
					dumpindent[rdepth++] = ' ';
					dumpindent[rdepth] = 0;
				}
				else {
					rdepth = depth;
					dumpindent[rdepth++] = ' ';
					dumpindent[rdepth++] = ' ';
					dumpindent[rdepth++] = ' ';
					dumpindent[rdepth++] = ' ';
					dumpindent[rdepth] = 0;
				}

				if (more)
					dumpindent[depth + 0] = '|';

				kcount += dump_key_tree_aux(key,
							    rdepth,
							    ringlen - 4 >= sizeof(key_serial_t));
			}

		} while (ringlen -= 4, ringlen >= sizeof(key_serial_t));

		free(payload);
	}

	free(desc);
	return kcount;

} /* end dump_key_tree_aux() */

/*****************************************************************************/
/*
 * recursively list a keyring's contents
 */
static int dump_key_tree(key_serial_t keyring, const char *name)
{
	printf("%s\n", name);
	return dump_key_tree_aux(keyring, 0, 0);

} /* end dump_key_tree() */
