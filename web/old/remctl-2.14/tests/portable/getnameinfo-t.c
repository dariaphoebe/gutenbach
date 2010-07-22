/*
 * getnameinfo test suite.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2009 Board of Trustees, Leland Stanford Jr. University
 * Copyright (c) 2004, 2005, 2006, 2007
 *     by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
 *     2002, 2003 by The Internet Software Consortium and Rich Salz
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>
#include <portable/socket.h>

#include <tests/tap/basic.h>
#include <util/util.h>

int test_getnameinfo(const struct sockaddr *, socklen_t, char *, socklen_t,
                     char *, socklen_t, int);

/* Linux doesn't provide EAI_OVERFLOW, so make up our own for testing. */
#ifndef EAI_OVERFLOW
# define EAI_OVERFLOW 10
#endif

int
main(void)
{
    char node[256], service[256];
    struct sockaddr_in sin;
    struct sockaddr *sa = (struct sockaddr *) &sin;
    int status;
    struct hostent *hp;
    struct servent *serv;
    char *name;

    plan(26);

    /*
     * Test the easy stuff that requires no assumptions.  Hopefully everyone
     * has nntp, exec, and biff as services.  exec and biff are special since
     * they're one of the rare universal pairs of services that are both on
     * the same port, but with different protocols.
     */
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(119);
    inet_aton("10.20.30.40", &sin.sin_addr);
    status = test_getnameinfo(sa, sizeof(sin), NULL, 0, NULL, 0, 0);
    is_int(EAI_NONAME, status, "EAI_NONAME from NULL");
    status = test_getnameinfo(sa, sizeof(sin), node, 0, service, 0, 0);
    is_int(EAI_NONAME, status, "EAI_NONAME from length of 0");
    status = test_getnameinfo(sa, sizeof(sin), NULL, 0, service,
                              sizeof(service), 0);
    is_int(0, status, "lookup of port 119");
    serv = getservbyname("nntp", "tcp");
    if (serv == NULL)
        skip_block(5, "nntp service not found");
    else {
        is_string("nntp", service, "...found nntp");
        service[0] = '\0';
        status = test_getnameinfo(sa, sizeof(sin), NULL, 0, service, 1, 0);
        is_int(EAI_OVERFLOW, status, "EAI_OVERFLOW with one character");
        status = test_getnameinfo(sa, sizeof(sin), NULL, 0, service, 4, 0);
        is_int(EAI_OVERFLOW, status, "EAI_OVERFLOW with four characters");
        status = test_getnameinfo(sa, sizeof(sin), NULL, 0, service, 5, 0);
        is_int(0, status, "fits in five characters");
        is_string("nntp", service, "...and found nntp");
    }
    status = test_getnameinfo(sa, sizeof(sin), NULL, 0, service,
                              sizeof(service), NI_NUMERICSERV);
    is_int(0, status, "NI_NUMERICSERV");
    is_string("119", service, "...and returns number");
    sin.sin_port = htons(512);
    status = test_getnameinfo(sa, sizeof(sin), NULL, 0, service,
                              sizeof(service), 0);
    is_int(0, status, "lookup of 512 TCP");
    serv = getservbyname("exec", "tcp");
    if (serv == NULL)
        skip("exec service not found");
    else
        is_string("exec", service, "...and found exec");
    status = test_getnameinfo(sa, sizeof(sin), NULL, 0, service,
                              sizeof(service), NI_DGRAM);
    is_int(0, status, "lookup of 512 UDP");
    serv = getservbyname("biff", "udp");
    if (serv == NULL)
        skip("biff service not found");
    else
        is_string("biff", service, "...and found biff");
    status = test_getnameinfo(sa, sizeof(sin), node, sizeof(node), NULL, 0,
                              NI_NUMERICHOST);
    is_int(0, status, "lookup with NI_NUMERICHOST");
    is_string("10.20.30.40", node, "...and found correct IP address");
    node[0] = '\0';
    status = test_getnameinfo(sa, sizeof(sin), node, 1, NULL, 0,
                              NI_NUMERICHOST);
    is_int(EAI_OVERFLOW, status, "EAI_OVERFLOW with one character");
    status = test_getnameinfo(sa, sizeof(sin), node, 11, NULL, 0,
                              NI_NUMERICHOST);
    is_int(EAI_OVERFLOW, status, "EAI_OVERFLOW with 11 characters");
    status = test_getnameinfo(sa, sizeof(sin), node, 12, NULL, 0,
                              NI_NUMERICHOST);
    is_int(0, status, "fits into 12 characters");
    is_string("10.20.30.40", node, "...and found correct IP address");

    /*
     * Okay, now it gets annoying.  Do a forward and then reverse lookup of
     * some well-known host and make sure that getnameinfo returns the same
     * results.  This may need to be skipped.
     */
    hp = gethostbyname("www.isc.org");
    if (hp == NULL)
        skip_block(2, "cannot look up www.isc.org");
    else {
        memcpy(&sin.sin_addr, hp->h_addr, sizeof(sin.sin_addr));
        hp = gethostbyaddr((const void *) &sin.sin_addr, sizeof(sin.sin_addr),
                           AF_INET);
        if (hp == NULL || strchr(hp->h_name, '.') == NULL)
            skip_block(2, "cannot reverse-lookup www.isc.org");
        else {
            name = xstrdup(hp->h_name);
            status = test_getnameinfo(sa, sizeof(sin), node, sizeof(node),
                                      NULL, 0, 0);
            is_int(0, status, "lookup of www.isc.org IP address");
            is_string(name, node, "...matches gethostbyaddr");
            free(name);
        }
    }

    /* Hope that no one is weird enough to put 0.0.0.0 into DNS. */
    inet_aton("0.0.0.0", &sin.sin_addr);
    status = test_getnameinfo(sa, sizeof(sin), node, sizeof(node), NULL, 0, 0);
    is_int(0, status, "lookup of 0.0.0.0");
    is_string("0.0.0.0", node, "...returns the IP address");
    node[0] = '\0';
    status = test_getnameinfo(sa, sizeof(sin), node, sizeof(node), NULL, 0,
                              NI_NAMEREQD);
    is_int(EAI_NONAME, status, "...fails with NI_NAMEREQD");

    sin.sin_family = AF_UNIX;
    status = test_getnameinfo(sa, sizeof(sin), node, sizeof(node), NULL, 0, 0);
    is_int(EAI_FAMILY, status, "EAI_FAMILY");

    return 0;
}