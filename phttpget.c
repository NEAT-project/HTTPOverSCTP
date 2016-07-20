/*-
 * Copyright 2005, 2016 Colin Percival, Felix Weinrank
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef __FreeBSD__
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/usr.sbin/portsnap/phttpget/phttpget.c 190679 2009-04-03 21:13:18Z cperciva $");
#endif

#ifdef linux
#define _GNU_SOURCE
#define OFF_MAX LONG_MAX
#endif
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/sctp.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define NUM_SCTP_STREAMS    10

static const char *     env_HTTP_PROXY;
static char *           env_HTTP_PROXY_AUTH;
static const char *     env_HTTP_USER_AGENT;
static char *           env_HTTP_TIMEOUT;
static char *           env_HTTP_TRANSPORT_PROTOCOL;
static char *           env_HTTP_SCTP_UDP_ENCAPS_PORT;
static const char *     proxyport;
static char *           proxyauth;

static struct           timeval    timo = { 15, 0};

static in_port_t        udp_encaps_port = 0;
static int              protocol = IPPROTO_TCP;
static uint8_t          streamstatus[NUM_SCTP_STREAMS];
static uint32_t         lastStream = 0;
static uint32_t         reqOpen = 0;


enum stream_status {
    STREAM_FREE,
    STREAM_USED
};

#ifndef __FreeBSD__
char *
strnstr(const char *s, const char *find, size_t slen)
{
    char c, sc;
    size_t len;

    if ((c = *find++) != '\0') {
        len = strlen(find);
        do {
            do {
                if (slen-- < 1 || (sc = *s++) == '\0')
                    return (NULL);
            } while (sc != c);
            if (len > slen)
                return (NULL);
        } while (strncmp(s, find, len) != 0);
        s--;
    }
    return ((char *)s);
}
#endif

static void
usage(void)
{
    fprintf(stderr, "usage: phttpget server [file ...]\n");
    exit(EX_USAGE);
}

static int
getnextstream(struct sctp_sndinfo *sndinfo) {
    uint32_t tempindex;

    for (tempindex = 0; tempindex < NUM_SCTP_STREAMS; tempindex++) {
        if (streamstatus[tempindex] == STREAM_FREE) {
            streamstatus[tempindex] = STREAM_USED;
            sndinfo->snd_sid = tempindex;
            return 0;
        }
    }
    return -1;
}

/*
 * Base64 encode a string; the string returned, if non-NULL, is
 * allocated using malloc() and must be freed by the caller.
 */
static char *
b64enc(const char *ptext)
{
    static const char base64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    const char *pt;
    char *ctext, *pc;
    size_t ptlen, ctlen;
    uint32_t t;
    unsigned int j;

    /*
     * Encoded length is 4 characters per 3-byte block or partial
     * block of plaintext, plus one byte for the terminating NUL
     */
    ptlen = strlen(ptext);
    if (ptlen > ((SIZE_MAX - 1) / 4) * 3 - 2)
        return NULL;    /* Possible integer overflow */
    ctlen = 4 * ((ptlen + 2) / 3) + 1;
    if ((ctext = malloc(ctlen)) == NULL)
        return NULL;
    ctext[ctlen - 1] = 0;

    /*
     * Scan through ptext, reading up to 3 bytes from ptext and
     * writing 4 bytes to ctext, until we run out of input.
     */
    for (pt = ptext, pc = ctext; ptlen; ptlen -= 3, pc += 4) {
        /* Read 3 bytes */
        for (t = j = 0; j < 3; j++) {
            t <<= 8;
            if (j < ptlen)
                t += *pt++;
        }

        /* Write 4 bytes */
        for (j = 0; j < 4; j++) {
            if (j <= ptlen + 1)
                pc[j] = base64[(t >> 18) & 0x3f];
            else
                pc[j] = '=';
            t <<= 6;
        }

        /* If we're done, exit the loop */
        if (ptlen <= 3)
            break;
    }

    return (ctext);
}

static void
readenv(void)
{
    char *proxy_auth_userpass, *proxy_auth_userpass64, *p;
    char *proxy_auth_user = NULL;
    char *proxy_auth_pass = NULL;
    long http_timeout;
    long port;
    int buflen;

    env_HTTP_PROXY = getenv("HTTP_PROXY");
    if (env_HTTP_PROXY == NULL)
        env_HTTP_PROXY = getenv("http_proxy");
    if (env_HTTP_PROXY != NULL) {
        if (strncmp(env_HTTP_PROXY, "http://", 7) == 0)
            env_HTTP_PROXY += 7;
        p = strchr(env_HTTP_PROXY, '/');
        if (p != NULL)
            *p = 0;
        p = strchr(env_HTTP_PROXY, ':');
        if (p != NULL) {
            *p = 0;
            proxyport = p + 1;
        } else
            proxyport = "3128";
    }

    env_HTTP_PROXY_AUTH = getenv("HTTP_PROXY_AUTH");
    if ((env_HTTP_PROXY != NULL) &&
        (env_HTTP_PROXY_AUTH != NULL) &&
        (strncasecmp(env_HTTP_PROXY_AUTH, "basic:" , 6) == 0)) {
        /* Ignore authentication scheme */
        (void) strsep(&env_HTTP_PROXY_AUTH, ":");

        /* Ignore realm */
        (void) strsep(&env_HTTP_PROXY_AUTH, ":");

        /* Obtain username and password */
        proxy_auth_user = strsep(&env_HTTP_PROXY_AUTH, ":");
        proxy_auth_pass = env_HTTP_PROXY_AUTH;
    }

    if ((proxy_auth_user != NULL) && (proxy_auth_pass != NULL)) {
        buflen = asprintf(&proxy_auth_userpass, "%s:%s",
            proxy_auth_user, proxy_auth_pass);
        if (buflen == -1)
            err(1, "asprintf");

        proxy_auth_userpass64 = b64enc(proxy_auth_userpass);
        if (proxy_auth_userpass64 == NULL)
            err(1, "malloc");

        buflen = asprintf(&proxyauth, "Proxy-Authorization: Basic %s\r\n",
            proxy_auth_userpass64);
        if (buflen == -1)
            err(1, "asprintf");

        free(proxy_auth_userpass);
        free(proxy_auth_userpass64);
    } else
        proxyauth = NULL;

    env_HTTP_USER_AGENT = getenv("HTTP_USER_AGENT");
    if (env_HTTP_USER_AGENT == NULL)
        env_HTTP_USER_AGENT = "phttpget/0.1";

    env_HTTP_TIMEOUT = getenv("HTTP_TIMEOUT");
    if (env_HTTP_TIMEOUT != NULL) {
        http_timeout = strtol(env_HTTP_TIMEOUT, &p, 10);
        if ((*env_HTTP_TIMEOUT == '\0') || (*p != '\0') ||
            (http_timeout < 0))
            warnx("HTTP_TIMEOUT (%s) is not a positive integer",
                env_HTTP_TIMEOUT);
        else
            timo.tv_sec = http_timeout;
    }
    env_HTTP_TRANSPORT_PROTOCOL = getenv("HTTP_TRANSPORT_PROTOCOL");
    if (env_HTTP_TRANSPORT_PROTOCOL != NULL) {
        if (strncasecmp(env_HTTP_TRANSPORT_PROTOCOL, "TCP", 3) == 0)
            protocol = IPPROTO_TCP;
        else if (strncasecmp(env_HTTP_TRANSPORT_PROTOCOL, "SCTP", 4) == 0)
            protocol = IPPROTO_SCTP;
        else
            warnx("HTTP_TRANSPORT_PROTOCOL (%s) not supported",
                env_HTTP_TRANSPORT_PROTOCOL);
    }

    env_HTTP_SCTP_UDP_ENCAPS_PORT = getenv("HTTP_SCTP_UDP_ENCAPS_PORT");
    if (env_HTTP_SCTP_UDP_ENCAPS_PORT != NULL) {
        port = strtol(env_HTTP_SCTP_UDP_ENCAPS_PORT, &p, 10);
        if ((*env_HTTP_SCTP_UDP_ENCAPS_PORT == '\0') || (*p != '\0') ||
            (port < 0) || (port > 65535))
            warnx("HTTP_SCTP_UDP_ENCAPS_PORT (%s) is not a valid port number",
                env_HTTP_SCTP_UDP_ENCAPS_PORT);
        else
            udp_encaps_port = (in_port_t)port;
    }
}

static int
makerequest(char ** buf, char * path, char * server, int connclose)
{
    int buflen;

    buflen = asprintf(buf,
        "GET %s%s/%s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: %s\r\n"
        "%s"
        "%s"
        "\r\n",
        env_HTTP_PROXY ? "http://" : "",
        env_HTTP_PROXY ? server : "",
        path, server, env_HTTP_USER_AGENT,
        proxyauth ? proxyauth : "",
        connclose ? "Connection: Close\r\n" : "Connection: Keep-Alive\r\n");
    if (buflen == -1)
        err(1, "asprintf");
    return(buflen);
}

static int
readln(int sd, char * resbuf, int * resbuflen, int * resbufpos)
{
    ssize_t len;
    struct msghdr msg;
    struct iovec iov;
    char cmsgbuf[CMSG_SPACE(sizeof(struct sctp_rcvinfo))];
    struct sctp_rcvinfo *rcvinfo;

    memset(cmsgbuf, 0, CMSG_SPACE(sizeof(struct sctp_rcvinfo)));

    while (strnstr(resbuf + *resbufpos, "\r\n",
        *resbuflen - *resbufpos) == NULL) {
        /* Move buffered data to the start of the buffer */
        if (*resbufpos != 0) {
            memmove(resbuf, resbuf + *resbufpos,
                *resbuflen - *resbufpos);
            *resbuflen -= *resbufpos;
            *resbufpos = 0;
        }

        /* If the buffer is full, complain */
        if (*resbuflen == BUFSIZ)
            return -1;

        /* Read more data into the buffer */
        iov.iov_base = resbuf + *resbuflen;
        iov.iov_len = BUFSIZ - *resbuflen;
        msg.msg_name = NULL;
        msg.msg_namelen = 0;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = cmsgbuf;
        msg.msg_controllen = sizeof(cmsgbuf);
        rcvinfo = (struct sctp_rcvinfo *)CMSG_DATA(cmsgbuf);
        len = recvmsg(sd, &msg, 0);
        reqOpen--;
        printf("recvmsg\n");
        if (protocol == IPPROTO_SCTP) {
            lastStream = rcvinfo->rcv_sid;
        }

        //len = recv(sd, resbuf + *resbuflen, BUFSIZ - *resbuflen, 0);

        if ((len == 0) ||
            ((len == -1) && (errno != EINTR)))
            return -1;

        if (len != -1)
            *resbuflen += len;
    }

    return 0;
}

static int
copybytes(int sd, int fd, off_t copylen, char * resbuf, int * resbuflen,
    int * resbufpos)
{
    ssize_t len;
    struct msghdr msg;
    struct iovec iov;
    char cmsgbuf[CMSG_SPACE(sizeof(struct sctp_rcvinfo))];
    struct sctp_rcvinfo *rcvinfo;

    memset(cmsgbuf, 0, CMSG_SPACE(sizeof(struct sctp_rcvinfo)));

    while (copylen) {
        /* Write data from resbuf to fd */
        len = *resbuflen - *resbufpos;
        if (copylen < len)
            len = copylen;
        if (len > 0) {
            if (fd != -1)
                len = write(fd, resbuf + *resbufpos, len);
            if (len == -1)
                err(1, "write");
            *resbufpos += len;
            copylen -= len;
            continue;
        }

        iov.iov_base = resbuf;
        iov.iov_len = BUFSIZ;
        msg.msg_name = NULL;
        msg.msg_namelen = 0;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = cmsgbuf;
        msg.msg_controllen = sizeof(cmsgbuf);
        rcvinfo = (struct sctp_rcvinfo *)CMSG_DATA(cmsgbuf);
        len = recvmsg(sd, &msg, 0);
        reqOpen--;

        if (protocol == IPPROTO_SCTP) {
            lastStream = rcvinfo->rcv_sid;
        }

        /* Read more data into buffer */
        //len = recv(sd, resbuf, BUFSIZ, 0);
        if (len == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        } else if (len == 0) {
            return -2;
        } else {
            *resbuflen = len;
            *resbufpos = 0;
        }
    }

    return 0;
}

int
main(int argc, char *argv[])
{
    struct addrinfo hints;        /* Hints to getaddrinfo */
    struct addrinfo *res;        /* Pointer to server address being used */
    struct addrinfo *res0;        /* Pointer to server addresses */
#ifdef SCTP_REMOTE_UDP_ENCAPS_PORT
    struct sctp_udpencaps encaps;    /* SCTP/UDP information */
#endif
    char * resbuf = NULL;        /* Response buffer */
    int resbufpos = 0;        /* Response buffer position */
    int resbuflen = 0;        /* Response buffer length */
    char * eolp;            /* Pointer to "\r\n" within resbuf */
    char * hln;            /* Pointer within header line */
    char * servername;        /* Name of server */
    char * fname = NULL;        /* Name of downloaded file */
    char * reqbuf = NULL;        /* Request buffer */
    int reqbufpos = 0;        /* Request buffer position */
    int reqbuflen = 0;        /* Request buffer length */
    ssize_t len;            /* Length sent or received */
    int nreq = 0;            /* Number of next request to send */
    int nres = 0;            /* Number of next reply to receive */
    int pipelined = 0;        /* != 0 if connection in pipelined mode. */
    int keepalive;            /* != 0 if HTTP/1.0 keep-alive rcvd. */
    int sd = -1;            /* Socket descriptor */
    int sdflags = 0;        /* Flags on the socket sd */
    int fd = -1;            /* Descriptor for download target file */
    int error;            /* Error code */
    int statuscode;            /* HTTP Status code */
    off_t contentlength;        /* Value from Content-Length header */
    int chunked;            /* != if transfer-encoding is chunked */
    off_t clen;            /* Chunk length */
    int firstreq = 0;        /* # of first request for this connection */
    int val;            /* Value used for setsockopt call */
    struct iovec iov;
    struct msghdr msghdr;
    struct sctp_sndinfo *sndinfo;
    struct cmsghdr *cmsg;
    char* cmsgbuf[CMSG_SPACE(sizeof(struct sctp_sndinfo))];
    struct sctp_initmsg initmsg;
    int tempindex = 0;

    /* initialize */
    memset(cmsgbuf, 0, CMSG_SPACE(sizeof(struct sctp_sndinfo)));
    for (tempindex = 0; tempindex < NUM_SCTP_STREAMS; tempindex++) {
        streamstatus[tempindex] = STREAM_FREE;
    }

    /* Check that the arguments are sensible */
    if (argc < 2)
        usage();

    /* Read important environment variables */
    readenv();

    /* Get server name and adjust arg[cv] to point at file names */
    servername = argv[1];
    argv += 2;
    argc -= 2;

    /* Allocate response buffer */
    resbuf = malloc(BUFSIZ);
    if (resbuf == NULL)
        err(1, "malloc");

    /* Look up server */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = protocol;
#ifdef __FreeBSD__
    error = getaddrinfo(env_HTTP_PROXY ? env_HTTP_PROXY : servername,
        env_HTTP_PROXY ? proxyport : "http", &hints, &res0);
#else
    /* Not all OSes have an SCTP entry for HTTP in /etc/services yet. */
    error = getaddrinfo(env_HTTP_PROXY ? env_HTTP_PROXY : servername,
        env_HTTP_PROXY ? proxyport : "80", &hints, &res0);
#endif
    if (error)
        errx(1, "host = %s, port = %s: %s",
            env_HTTP_PROXY ? env_HTTP_PROXY : servername,
            env_HTTP_PROXY ? proxyport : "http",
            gai_strerror(error));
    if (res0 == NULL)
        errx(1, "could not look up %s", servername);
    res = res0;

    /* Do the fetching */
    while (nres < argc) {
        /* Make sure we have a connected socket */
        for (; sd == -1; res = res->ai_next) {
            /* No addresses left to try :-( */
            if (res == NULL)
                errx(1, "Could not connect to %s", servername);

            /* Create a socket... */
            sd = socket(res->ai_family, res->ai_socktype,
                res->ai_protocol);
            if (sd == -1)
                continue;

            /* ... set 15-second timeouts ... */
            setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO,
                (void *)&timo, (socklen_t)sizeof(timo));
            setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO,
                (void *)&timo, (socklen_t)sizeof(timo));

            if (protocol == IPPROTO_SCTP) {
                /* Ensure an appropriate number of stream will be negotated. */
                initmsg.sinit_num_ostreams = NUM_SCTP_STREAMS;
                initmsg.sinit_max_instreams = NUM_SCTP_STREAMS;
                initmsg.sinit_max_attempts = 0;   /* Use default */
                initmsg.sinit_max_init_timeo = 0; /* Use default */
                if ( setsockopt(
                     sd, IPPROTO_SCTP, SCTP_INITMSG, (char*) &initmsg,
                     sizeof(initmsg) ) < 0 )
                {
                    fprintf(stderr, "problem : SCTP_INITMSG\n");
                    exit(-1);
                }

                /* Enable RCVINFO delivery */
                val = 1;
                if ( setsockopt(
                     sd, IPPROTO_SCTP, SCTP_RECVRCVINFO, (char*) &val,
                     sizeof(val) ) < 0 )
                {
                    fprintf(stderr, "problem : SCTP_RECVRCVINFO\n");
                    exit(-1);
                }
#ifdef SCTP_REMOTE_UDP_ENCAPS_PORT
                /* Use UDP encapsulation for SCTP */
                memset(&encaps, 0, sizeof(encaps));
                encaps.sue_address.ss_family = res->ai_family;
                encaps.sue_address.ss_len = res->ai_addrlen;
                encaps.sue_port = htons(udp_encaps_port);
                setsockopt(sd,
                    IPPROTO_SCTP, SCTP_REMOTE_UDP_ENCAPS_PORT,
                    (void *)&encaps, (socklen_t)sizeof(encaps));
#else
                if (udp_encaps_port > 0)
                    errx(1, "UDP encapsulation not supported");
#endif
            }

#ifdef SO_NOSIGPIPE
            /* ... disable SIGPIPE generation ... */
            val = 1;
            setsockopt(sd, SOL_SOCKET, SO_NOSIGPIPE,
                (void *)&val, sizeof(int));
#endif

            /* ... and connect to the server. */
            if (connect(sd, res->ai_addr, res->ai_addrlen)) {
                close(sd);
                sd = -1;
                continue;
            }

            firstreq = nres;
        }

        /*
         * If in pipelined HTTP mode, put socket into non-blocking
         * mode, since we're probably going to want to try to send
         * several HTTP requests.
         */
        if (pipelined) {
            sdflags = fcntl(sd, F_GETFL);
            if (fcntl(sd, F_SETFL, sdflags | O_NONBLOCK) == -1)
                err(1, "fcntl");
        }

        /* Construct requests and/or send them without blocking */
        while ((nreq < argc) && ((reqbuf == NULL) || pipelined)) {
            /* If not in the middle of a request, make one */
            if (reqbuf == NULL) {
                reqbuflen = makerequest(&reqbuf, argv[nreq],
                    servername, (nreq == argc - 1));
                reqbufpos = 0;
            }

            /* If in pipelined mode, try to send the request */
            if (pipelined) {
                while (reqbufpos < reqbuflen) {
                    memset(&msghdr, 0, sizeof(struct msghdr));
                    memset(&iov, 0, sizeof(struct iovec));

                    msghdr.msg_namelen = res->ai_addrlen;
                    msghdr.msg_iov = &iov;
                    msghdr.msg_iovlen = 1;

                    iov.iov_base = reqbuf + reqbufpos;
                    iov.iov_len = reqbuflen - reqbufpos;

                    if (protocol == IPPROTO_SCTP) {
                        msghdr.msg_control = cmsgbuf;
                        msghdr.msg_controllen = CMSG_SPACE(sizeof(struct sctp_sndinfo));

                        cmsg = CMSG_FIRSTHDR(&msghdr);
                        cmsg->cmsg_level = IPPROTO_SCTP;
                        cmsg->cmsg_type = SCTP_SNDINFO;
                        cmsg->cmsg_len = CMSG_LEN(sizeof(struct sctp_sndinfo));

                        sndinfo = (struct sctp_sndinfo*) CMSG_DATA(cmsg);
                        if (getnextstream(sndinfo) == -1) {
                            fprintf(stderr, "problem : getnextstream a\n");
                            break;
                        }
                    }

                    len = sendmsg(sd, &msghdr, 0);
                    reqOpen++;
                    //len = send(sd, reqbuf + reqbufpos, reqbuflen - reqbufpos, 0);
                    if (len == -1) {
                        break;
                        fprintf(stderr, "problem : len == -1\n");
                    }
                    reqbufpos += len;
                }
                if (reqbufpos < reqbuflen) {
                    if (errno != EAGAIN)
                        goto conndied;
                    break;
                } else {
                    free(reqbuf);
                    reqbuf = NULL;
                    nreq++;
                }
            }
        }

        /* Put connection back into blocking mode */
        if (pipelined) {
            if (fcntl(sd, F_SETFL, sdflags) == -1)
                err(1, "fcntl");
        }

        /* Do we need to blocking-send a request? */
        if (nres == nreq) {
            while (reqbufpos < reqbuflen) {
                memset(&msghdr, 0, sizeof(struct msghdr));
                memset(&iov, 0, sizeof(struct iovec));

                msghdr.msg_namelen = res->ai_addrlen;
                msghdr.msg_iov = &iov;
                msghdr.msg_iovlen = 1;

                iov.iov_base = reqbuf + reqbufpos;
                iov.iov_len = reqbuflen - reqbufpos;

                if (protocol == IPPROTO_SCTP) {
                    msghdr.msg_control = cmsgbuf;
                    msghdr.msg_controllen = CMSG_SPACE(sizeof(struct sctp_sndinfo));

                    cmsg = CMSG_FIRSTHDR(&msghdr);
                    cmsg->cmsg_level = IPPROTO_SCTP;
                    cmsg->cmsg_type = SCTP_SNDINFO;
                    cmsg->cmsg_len = CMSG_LEN(sizeof(struct sctp_sndinfo));

                    sndinfo = (struct sctp_sndinfo*) CMSG_DATA(cmsg);
                    if (getnextstream(sndinfo) == -1) {
                        fprintf(stderr, "problem : getnextstream b\n");
                        break;
                    }
                }

                len = sendmsg(sd, &msghdr, 0);
                reqOpen++;
                //len = send(sd, reqbuf + reqbufpos, reqbuflen - reqbufpos, 0);
                if (len == -1)
                    goto conndied;
                reqbufpos += len;
            }
            free(reqbuf);
            reqbuf = NULL;
            nreq++;
        }

        /* Scan through the response processing headers. */
        statuscode = 0;
        contentlength = -1;
        chunked = 0;
        keepalive = 0;


        do {
            /* Get a header line */
            if (reqOpen < 1) {
                printf("%d - nix offen, abbruch!\n", reqOpen);
                break;
            }

            error = readln(sd, resbuf, &resbuflen, &resbufpos);
            if (error)
                goto conndied;
            hln = resbuf + resbufpos;
            eolp = strnstr(hln, "\r\n", resbuflen - resbufpos);
            resbufpos = (eolp - resbuf) + 2;
            *eolp = '\0';

            /* Make sure it doesn't contain a NUL character */
            if (strchr(hln, '\0') != eolp)
                goto conndied;

            if (statuscode == 0) {
                /* The first line MUST be HTTP/1.x xxx ... */
                if ((strncmp(hln, "HTTP/1.", 7) != 0) ||
                    ! isdigit(hln[7]))
                    goto conndied;

                /*
                 * If the minor version number isn't zero,
                 * then we can assume that pipelining our
                 * requests is OK -- as long as we don't
                 * see a "Connection: close" line later
                 * and we either have a Content-Length or
                 * Transfer-Encoding: chunked header to
                 * tell us the length.
                 */
                if (hln[7] != '0')
                    pipelined = 1;

                /* Skip over the minor version number */
                hln = strchr(hln + 7, ' ');
                if (hln == NULL)
                    goto conndied;
                else
                    hln++;

                /* Read the status code */
                while (isdigit(*hln)) {
                    statuscode = statuscode * 10 +
                        *hln - '0';
                    hln++;
                }

                if (statuscode < 100 || statuscode > 599)
                    goto conndied;

                /* Ignore the rest of the line */
                continue;
            }

            /*
             * Check for "Connection: close" or
             * "Connection: Keep-Alive" header
             */
            if (strncasecmp(hln, "Connection:", 11) == 0) {
                hln += 11;
                if (strcasestr(hln, "close") != NULL)
                    pipelined = 0;
                if (strcasestr(hln, "Keep-Alive") != NULL)
                    keepalive = 1;

                /* Next header... */
                continue;
            }

            /* Check for "Content-Length:" header */
            if (strncasecmp(hln, "Content-Length:", 15) == 0) {
                hln += 15;
                contentlength = 0;

                /* Find the start of the length */
                while (!isdigit(*hln) && (*hln != '\0'))
                    hln++;

                /* Compute the length */
                while (isdigit(*hln)) {
                    if (contentlength >= OFF_MAX / 10) {
                        /* Nasty people... */
                        goto conndied;
                    }
                    contentlength = contentlength * 10 +
                        *hln - '0';
                    hln++;
                }

                /* Next header... */
                continue;
            }

            /* Check for "Transfer-Encoding: chunked" header */
            if (strncasecmp(hln, "Transfer-Encoding:", 18) == 0) {
                hln += 18;
                if (strcasestr(hln, "chunked") != NULL)
                    chunked = 1;

                /* Next header... */
                continue;
            }

            /* We blithely ignore any other header lines */

            /* No more header lines */
            if (strlen(hln) == 0) {
                /*
                 * If the status code was 1xx, then there will
                 * be a real header later.  Servers may emit
                 * 1xx header blocks at will, but since we
                 * don't expect one, we should just ignore it.
                 */
                if (100 <= statuscode && statuscode <= 199) {
                    statuscode = 0;
                    continue;
                }

                /* End of header; message body follows */
                break;
            }
        } while (1);

        /* No message body for 204 or 304 */
        if (statuscode == 204 || statuscode == 304) {
            nres++;
            continue;
        }

        /*
         * There should be a message body coming, but we only want
         * to send it to a file if the status code is 200
         */
        if (statuscode == 200) {
            /* Generate a file name for the download */
            fname = strrchr(argv[nres], '/');
            if (fname == NULL)
                fname = argv[nres];
            else
                fname++;
            if (strlen(fname) == 0)
                errx(1, "Cannot obtain file name from %s\n",
                    argv[nres]);

            fd = open(fname, O_CREAT | O_TRUNC | O_WRONLY, 0644);
            if (fd == -1)
                errx(1, "open(%s)", fname);
        };

        /* Read the message and send data to fd if appropriate */
        if (chunked) {
            /* Handle a chunked-encoded entity */

            /* Read chunks */
            do {
                error = readln(sd, resbuf, &resbuflen,
                    &resbufpos);
                if (error)
                    goto conndied;
                hln = resbuf + resbufpos;
                eolp = strstr(hln, "\r\n");
                resbufpos = (eolp - resbuf) + 2;

                clen = 0;
                while (isxdigit(*hln)) {
                    if (clen >= OFF_MAX / 16) {
                        /* Nasty people... */
                        goto conndied;
                    }
                    if (isdigit(*hln))
                        clen = clen * 16 + *hln - '0';
                    else
                        clen = clen * 16 + 10 +
                            tolower(*hln) - 'a';
                    hln++;
                }

                error = copybytes(sd, fd, clen, resbuf,
                    &resbuflen, &resbufpos);
                if (error) {
                    goto conndied;
                }
            } while (clen != 0);

            /* Read trailer and final CRLF */
            do {
                error = readln(sd, resbuf, &resbuflen,
                    &resbufpos);
                if (error)
                    goto conndied;
                hln = resbuf + resbufpos;
                eolp = strstr(hln, "\r\n");
                resbufpos = (eolp - resbuf) + 2;
            } while (hln != eolp);
        } else if (contentlength != -1) {
            error = copybytes(sd, fd, contentlength, resbuf,
                &resbuflen, &resbufpos);
            if (error)
                goto conndied;
        } else {
            /*
             * Not chunked, and no content length header.
             * Read everything until the server closes the
             * socket.
             */
            error = copybytes(sd, fd, OFF_MAX, resbuf,
                &resbuflen, &resbufpos);
            if (error == -1)
                goto conndied;
            pipelined = 0;
        }

        if (fd != -1) {
            close(fd);
            fd = -1;
        }

        fprintf(stderr, "http://%s/%s - %d - %d - ", servername, argv[nres], statuscode, nres);

        if (protocol == IPPROTO_SCTP) {
            fprintf(stderr, "SCTP Stream %d - ", lastStream);
            streamstatus[lastStream] = STREAM_FREE;
        }

        if (statuscode == 200)
            fprintf(stderr, "OK\n");
        else if (statuscode < 300)
            fprintf(stderr, "Successful (ignored)\n");
        else if (statuscode < 400)
            fprintf(stderr, "Redirection (ignored)\n");
        else
            fprintf(stderr, "Error (ignored)\n");

        /* We've finished this file! */
        nres++;

        /*
         * If necessary, clean up this connection so that we
         * can start a new one.
         */
        if (pipelined == 0 && keepalive == 0)
            goto cleanupconn;
        continue;

conndied:
        /*
         * Something went wrong -- our connection died, the server
         * sent us garbage, etc.  If this happened on the first
         * request we sent over this connection, give up.  Otherwise,
         * close this connection, open a new one, and reissue the
         * request.
         */
        if (nres == firstreq)
            errx(1, "Connection failure");

cleanupconn:
        /*
         * Clean up our connection and keep on going
         */
        shutdown(sd, SHUT_RDWR);
        close(sd);
        sd = -1;
        if (fd != -1) {
            close(fd);
            fd = -1;
        }
        if (reqbuf != NULL) {
            free(reqbuf);
            reqbuf = NULL;
        }
        nreq = nres;
        res = res0;
        pipelined = 0;
        resbufpos = resbuflen = 0;
        continue;
    }

    free(resbuf);
    freeaddrinfo(res0);

    return 0;
}
