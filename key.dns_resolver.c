/*
 * DNS Resolver Module User-space Helper for AFSDB records
 *
 * Copyright (C) Wang Lei (wang840925@gmail.com) 2010
 * Authors: Wang Lei (wang840925@gmail.com)
 *          David Howells (dhowells@redhat.com)
 *
 * This is a userspace tool for querying AFSDB RR records in the DNS on behalf
 * of the kernel, and converting the VL server addresses to IPv4 format so that
 * they can be used by the kAFS filesystem.
 *
 * Compile with:
 *
 * 	cc -o key.dns_resolver key.dns_resolver.c -lresolv -lkeyutils
 *
 * As some function like res_init() should use the static liberary, which is a
 * bug of libresolv, that is the reason for cifs.upcall to reimplement.
 *
 * To use this program, you must tell /sbin/request-key how to invoke it.  You
 * need to have the keyutils package installed and something like the following
 * lines added to your /etc/request-key.conf file:
 *
 * 	#OP    TYPE         DESCRIPTION CALLOUT INFO PROGRAM ARG1 ARG2 ARG3 ...
 * 	====== ============ =========== ============ ==========================
 * 	create dns_resolver afsdb:*     *            /sbin/key.dns_resolver %k
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#define _GNU_SOURCE
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <keyutils.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

static const char *DNS_PARSE_VERSION = "1.0";
static const char prog[] = "key.dns_resolver";
static const char key_type[] = "dns_resolver";
static const char afsdb_query_type[] = "afsdb";
static key_serial_t key;
static int verbose;
static int debug_mode;


#define	MAX_VLS			15	/* Max Volume Location Servers Per-Cell */
#define DNS_EXPIRY_PREFIX	"expiry_time="
#define DNS_EXPIRY_TIME_LEN	10 /* 2^32 - 1 = 4294967295 */
#define AFSDB_MAX_DATA_LEN						\
	((MAX_VLS * (INET6_ADDRSTRLEN + 1)) + sizeof(DNS_EXPIRY_PREFIX) + \
	 DNS_EXPIRY_TIME_LEN + 1 /* '#'*/ + 1 /* end 0 */)

#define	INET_IP4_ONLY	0x1
#define	INET_IP6_ONLY	0x2
#define	INET_ALL	0xFF

#define DNS_ERR_PREFIX	"#dnserror="

/*
 * Print an error to stderr or the syslog, negate the key being created and
 * exit
 */
static __attribute__((format(printf, 1, 2), noreturn))
void error(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	if (isatty(2)) {
		vfprintf(stderr, fmt, va);
		fputc('\n', stderr);
	} else {
		vsyslog(LOG_ERR, fmt, va);
	}
	va_end(va);

	/*
	 * on error, negatively instantiate the key ourselves so that we can
	 * make sure the kernel doesn't hang it off of a searchable keyring
	 * and interfere with the next attempt to instantiate the key.
	 */
	if (!debug_mode)
		keyctl_negate(key, 1, KEY_REQKEY_DEFL_DEFAULT);

	exit(1);
}

#define error(FMT, ...) error("Error: " FMT, ##__VA_ARGS__);

/*
 * Just print an error to stderr or the syslog
 */
static __attribute__((format(printf, 1, 2)))
void _error(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	if (isatty(2)) {
		vfprintf(stderr, fmt, va);
		fputc('\n', stderr);
	} else {
		vsyslog(LOG_ERR, fmt, va);
	}
	va_end(va);
}

/*
 * Print status information
 */
static __attribute__((format(printf, 1, 2)))
void info(const char *fmt, ...)
{
	va_list va;

	if (verbose < 1)
		return;

	va_start(va, fmt);
	if (isatty(1)) {
		fputs("I: ", stdout);
		vfprintf(stdout, fmt, va);
		fputc('\n', stdout);
	} else {
		vsyslog(LOG_INFO, fmt, va);
	}
	va_end(va);
}

/*
 * Print a nameserver error and exit
 */
static const int ns_errno_map[] = {
	[0]			= ECONNREFUSED,
	[HOST_NOT_FOUND]	= ENODATA,
	[TRY_AGAIN]		= EAGAIN,
	[NO_RECOVERY]		= ECONNREFUSED,
	[NO_DATA]		= ENODATA,
};

static __attribute__((noreturn))
void nsError(int err, const char *domain)
{
	char buf[AFSDB_MAX_DATA_LEN];
	int ret;

	if (isatty(2))
		fprintf(stderr, "%s: %s.\n", domain, hstrerror(err));
	else
		syslog(LOG_INFO, "%s: %s", domain, hstrerror(err));

	if (err >= sizeof(ns_errno_map) / sizeof(ns_errno_map[0]))
		err = -ECONNREFUSED;
	else
		err = ns_errno_map[err];

	sprintf(buf, "%s%d", DNS_ERR_PREFIX, err);

	info("The key instantiation ERROR data is '%s'", buf);

	if (!debug_mode) {
		ret = keyctl_instantiate(key, buf, strlen(buf) + 1, 0);
		if (ret == -1)
			error("%s: keyctl_instantiate: %m", __func__);
	}
	exit(0);
}

/*
 * Print debugging information
 */
static __attribute__((format(printf, 1, 2)))
void debug(const char *fmt, ...)
{
	va_list va;

	if (verbose < 2)
		return;

	va_start(va, fmt);
	if (isatty(1)) {
		fputs("D: ", stdout);
		vfprintf(stdout, fmt, va);
		fputc('\n', stdout);
	} else {
		vsyslog(LOG_DEBUG, fmt, va);
	}
	va_end(va);
}

/*
 * Perform address resolution on a hostname
 */
static int
dns_resolver(char *server_name, char *ip, short mask)
{
	struct addrinfo hints, *addr;
	int ret, len;
	void *p;

	memset(&hints, 0, sizeof(hints));
	switch (mask) {
	case INET_IP4_ONLY:	hints.ai_family = AF_INET;	break;
	case INET_IP6_ONLY:	hints.ai_family = AF_INET6;	break;
	default: break;
	}

	/* resolve name to ip */
	ret = getaddrinfo(server_name, NULL, &hints, &addr);
	if (ret) {
		info("unable to resolve hostname: %s [%s]",
		     server_name, gai_strerror(ret));
		return -1;
	}

	/* convert ip to string form */
	if (addr->ai_family == AF_INET && (mask & INET_IP4_ONLY)) {
		p = &(((struct sockaddr_in *)addr->ai_addr)->sin_addr);
		len = INET_ADDRSTRLEN;
	} else if (addr->ai_family == AF_INET6 && (mask & INET_IP6_ONLY)) {
		p = &(((struct sockaddr_in6 *)addr->ai_addr)->sin6_addr);
		len = INET6_ADDRSTRLEN;
	} else {
		freeaddrinfo(addr);
		return -1;
	}

	if (!inet_ntop(addr->ai_family, p, ip, len))
		error("%s: inet_ntop: %m", __func__);

	freeaddrinfo(addr);
	return 0;
}

/*
 *
 */
static void
addVLServers(char *VLlist[],
	     int *vlsnum,
             ns_msg handle,
             ns_sect section,
	     char *result,
	     short mask,
	     unsigned long *_ttl)
{
	int rrnum;  /* resource record number */
	ns_rr rr;   /* expanded resource record */
	char ip[INET6_ADDRSTRLEN];
	char *p = result;
	int subtype, i, ret, alen;
	unsigned int ttl = UINT_MAX, rr_ttl;

	debug("AFSDB RR count is %d", ns_msg_count(handle, section));

	/*
	 * Look at all the resource records in this section.
	 */
	for (rrnum = 0; rrnum < ns_msg_count(handle, section); rrnum++) {
		/*
		 * Expand the resource record number rrnum into rr.
		 */
		if (ns_parserr(&handle, section, rrnum, &rr)) {
			_error("ns_parserr failed : %m");
			continue;
		}

		/*
		 * We're only interested in AFSDB records
		 */
		if (ns_rr_type(rr) == ns_t_afsdb) {
			VLlist[*vlsnum] = malloc(MAXDNAME);
			if (!VLlist[*vlsnum])
				error("Out of memory");

			subtype = ns_get16(ns_rr_rdata(rr));

			/* Expand the name server's domain name */
			if (ns_name_uncompress(
				    ns_msg_base(handle),/* Start of the message	*/
				    ns_msg_end(handle), /* End of the message	*/
				    ns_rr_rdata(rr) + 2,    /* Position in the message*/
				    VLlist[*vlsnum],		/* Result	*/
				    MAXDNAME		/* Size of VLlist buffer*/
					       ) < 0)	/* Negative: error	*/
				error("ns_name_uncompress failed");

			rr_ttl = ns_rr_ttl(rr);
			if (ttl > rr_ttl)
				ttl = rr_ttl;

			/* Check the domain name we've just unpacked and add it to
			 * the list of name servers if it is not a duplicate.
			 * If it is a duplicate, just ignore it.
			 */
			for (i = 0; i < *vlsnum; i++)
				if (strcasecmp(VLlist[i], VLlist[*vlsnum]) == 0)
					goto next_one;

			/* Turn the hostname into IP addresses */
			ret = dns_resolver(VLlist[*vlsnum], ip, mask);
			if (ret) {
				debug("AFSDB RR can't resolve."
				      "subtype:%d, server name:%s, netmask:%d",
				      subtype, VLlist[*vlsnum], mask);
				goto next_one;
			}

			info("AFSDB RR subtype:%d, server name:%s, ip:%s, ttl:%u",
			     subtype, VLlist[*vlsnum], ip, ttl);

			/* colons are used in IPv6 addresses, so we use commas
			 * to separate IP addresses
			 */
			if (p > result)
				*p++ = ',';
			alen = strlen(ip);
			memcpy(p, ip, alen);
			p += alen;
			p[0] = '\0';

			/* prepare for the next record */
			(*vlsnum)++;
			continue;

		next_one:
			free(VLlist[*vlsnum]);
		}
	}

	*_ttl = ttl;
	info("ttl: %u", ttl);
}

/*
 * Look up the AFSDB record to get the VL server addresses.
 *
 * The callout_info is parsed for request options.  For instance, "ipv4" to
 * request only IPv4 addresses and "ipv6" to request only IPv6 addresses.
 */
static __attribute__((noreturn))
int dns_get_vlserver(key_serial_t key, const char *cell, char *options)
{
	int	ret;
	char	*VLlist[MAX_VLS];	/* list of name servers	*/
	char	ip[AFSDB_MAX_DATA_LEN];
	int	vlsnum = 0;		/* number of name servers in list */
	short	mask = INET_ALL;
	int	responseLen, len;	/* buffer length */
	ns_msg	handle;			/* handle for response message */
	unsigned long ttl = ULONG_MAX;
	union {
		HEADER hdr;
		u_char buf[NS_PACKETSZ];
	} response;		/* response buffers */

	debug("Get AFSDB RR for cell name:'%s', options:'%s'", cell, options);

	/* query the dns for an AFSDB RR */
	responseLen = res_query(cell,		/* the query to make */
				ns_c_in,	/* record class */
				ns_t_afsdb,	/* record type */
				response.buf,
				sizeof(response));
	if (responseLen < 0) {
		/* negative result; set an arbitrary timeout on the cache of 1
		 * minute */
		if (!debug_mode) {
			ret = keyctl_set_timeout(key, 1 * 60);
			if (ret == -1)
				error("%s: keyctl_set_timeout: %m", __func__);
		}
		nsError(h_errno, cell);
	}

	if (ns_initparse(response.buf, responseLen, &handle) < 0)
		error("ns_initparse: %m");

	/* Is the IP address family limited? */
	if (strcmp(options, "ipv4") == 0)
		mask = INET_IP4_ONLY;
	else if (strcmp(options, "ipv6") == 0)
		mask = INET_IP6_ONLY;

	/* look up a list of VL servers */
	addVLServers(VLlist, &vlsnum, handle, ns_s_an, ip, mask, &ttl);

	info("DNS query AFSDB RR results:'%s' ttl:%lu", ip, ttl);

	len = strlen(ip);

	/* set the key's expiry time from the minimum TTL encountered */
	if (!debug_mode) {
		ret = keyctl_set_timeout(key, ttl);
		if (ret == -1)
			error("%s: keyctl_set_timeout: %m", __func__);
	}

	/* handle a lack of results */
	if (len == 0)
		nsError(NO_DATA, cell);

	info("The key instantiation data is '%s'", ip);

	/* load the key with data key */
	if (!debug_mode) {
		ret = keyctl_instantiate(key, ip, strlen(ip) + 1, 0);
		if (ret == -1)
			error("%s: keyctl_instantiate: %m", __func__);
	}
	exit(0);
}

/*
 * Print usage details,
 */
static __attribute__((noreturn))
void usage(void)
{
	if (isatty(2)) {
		fprintf(stderr,
			"Usage: %s [-vv] key_serial\n",
			prog);
		fprintf(stderr,
			"Usage: %s -D [-vv] <desc> <calloutinfo>\n",
			prog);
	} else {
		info("Usage: %s [-vv] key_serial", prog);
	}
	if (!debug_mode)
		keyctl_negate(key, 1, KEY_REQKEY_DEFL_DEFAULT);
	exit(2);
}

const struct option long_options[] = {
	{ "debug",	0, NULL, 'D' },
	{ "verbose",	0, NULL, 'v' },
	{ "version",	0, NULL, 'V' },
	{ NULL,		0, NULL, 0 }
};


int main(int argc, char *argv[])
{
	int ktlen, qtlen, ret;
	char *keyend, *p;
	char *callout_info = NULL;
	char *buf = NULL, *name;
	char hostbuf[NI_MAXHOST];

	hostbuf[0] = '\0';

	openlog(prog, 0, LOG_DAEMON);

	while ((ret = getopt_long(argc, argv, "vD", long_options, NULL)) != -1) {
		switch (ret) {
		case 'D':
			debug_mode = 1;
			continue;
		case 'V':
			printf("version: %s\n", DNS_PARSE_VERSION);
			exit(0);
		case 'v':
			verbose++;
			continue;
		default:
			if (!isatty(2))
				syslog(LOG_ERR, "unknown option: %c", ret);
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (!debug_mode) {
		if (argc != 1)
			usage();

		/* get the key ID */
		errno = 0;
		key = strtol(*argv, NULL, 10);
		if (errno != 0)
			error("Invalid key ID format: %m");

		/* get the key description (of the form "x;x;x;x;<query_type>:<name>") */
		if (!buf) {
			ret = keyctl_describe_alloc(key, &buf);
			if (ret == -1)
				error("keyctl_describe_alloc failed: %m");
		}

		/* get the callout_info (which can supply options) */
		if (!callout_info) {
			ret = keyctl_read_alloc(KEY_SPEC_REQKEY_AUTH_KEY,
						(void **)&callout_info);
			if (ret == -1)
				error("Invalid key callout_info read: %m");
		}
	} else {
		if (argc != 2)
			usage();

		ret = asprintf(&buf, "%s;-1;-1;0;%s", key_type, argv[0]);
		if (ret < 0)
			error("Error %m");
		callout_info = argv[1];
	}

	ret = 1;
	info("Key description: '%s'", buf);
	info("Callout info: '%s'", callout_info);

	p = strchr(buf, ';');
	if (!p)
		error("Badly formatted key description '%s'", buf);
	ktlen = p - buf;

	if (ktlen != sizeof(key_type) - 1 ||
	    memcmp(buf, key_type, ktlen) != 0)
		error("Key type is not supported: '%*.*s'", ktlen, ktlen, buf);

	keyend = buf + ktlen + 1;

	/* the actual key description is after the last semicolon */
	keyend = rindex(keyend, ';');
	if (!keyend)
		error("Invalid key description: %s", buf);
	keyend++;

	name = index(keyend, ':');
	if (!name)
		error("Missing query type: '%s'", keyend);
	qtlen = name - keyend;
	name++;

	if (qtlen == sizeof(afsdb_query_type) - 1 &&
	    memcmp(keyend, afsdb_query_type, sizeof(afsdb_query_type) - 1) == 0
	    ) {
		info("Do DNS query of AFSDB type for:'%s' mask:'%s'",
		     name, callout_info);
		dns_get_vlserver(key, name, callout_info);
	}

	error("Query type: \"%*.*s\" is not supported", qtlen, qtlen, keyend);
}
