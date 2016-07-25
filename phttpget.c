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
#include <sys/queue.h>

/* maximum SCTP streams */
#define NUM_SCTP_STREAMS    10

static const char *     env_HTTP_USER_AGENT;
static char *           env_HTTP_TIMEOUT;
static char *           env_HTTP_TRANSPORT_PROTOCOL;
static char *           env_HTTP_SCTP_UDP_ENCAPS_PORT;

static struct           timeval timo = {15, 0};

static in_port_t        udp_encaps_port = 0;
static int              protocol = IPPROTO_TCP;
static uint8_t          streamstatus[NUM_SCTP_STREAMS];
static uint32_t         lastStream = 0;                     /* last SCTP stream read from */
static char             *servername;                        /* Name of server */
static uint8_t          streamsbusy = 0;                    /* all streams are busy ... */

enum stream_status {STREAM_FREE, STREAM_USED};

struct request {
    char *url;
    TAILQ_ENTRY(request) entries;
};

TAILQ_HEAD(request_queue, request);

struct request_queue requests_open;
struct request_queue requests_pending;

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


/* Lookup next free SCTP stream */
static int
getnextstream(struct sctp_sndinfo *sndinfo)
{
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


/* Read environment variables */
static void
readenv(void)
{
    char *p;
    long http_timeout;
    long port;

    env_HTTP_USER_AGENT = getenv("HTTP_USER_AGENT");
    if (env_HTTP_USER_AGENT == NULL) {
        env_HTTP_USER_AGENT = "phttpget/0.1";
    }

    env_HTTP_TIMEOUT = getenv("HTTP_TIMEOUT");
    if (env_HTTP_TIMEOUT != NULL) {
        http_timeout = strtol(env_HTTP_TIMEOUT, &p, 10);
        if ((*env_HTTP_TIMEOUT == '\0') || (*p != '\0') || (http_timeout < 0)) {
            warnx("HTTP_TIMEOUT (%s) is not a positive integer", env_HTTP_TIMEOUT);
        } else {
            timo.tv_sec = http_timeout;
        }
    }
    env_HTTP_TRANSPORT_PROTOCOL = getenv("HTTP_TRANSPORT_PROTOCOL");
    if (env_HTTP_TRANSPORT_PROTOCOL != NULL) {
        if (strncasecmp(env_HTTP_TRANSPORT_PROTOCOL, "TCP", 3) == 0) {
            protocol = IPPROTO_TCP;
        } else if (strncasecmp(env_HTTP_TRANSPORT_PROTOCOL, "SCTP", 4) == 0) {
            protocol = IPPROTO_SCTP;
        } else {
            warnx("HTTP_TRANSPORT_PROTOCOL (%s) not supported", env_HTTP_TRANSPORT_PROTOCOL);
        }
    }

    env_HTTP_SCTP_UDP_ENCAPS_PORT = getenv("HTTP_SCTP_UDP_ENCAPS_PORT");
    if (env_HTTP_SCTP_UDP_ENCAPS_PORT != NULL) {
        port = strtol(env_HTTP_SCTP_UDP_ENCAPS_PORT, &p, 10);
        if ((*env_HTTP_SCTP_UDP_ENCAPS_PORT == '\0') || (*p != '\0') || (port < 0) || (port > 65535)) {
            warnx("HTTP_SCTP_UDP_ENCAPS_PORT (%s) is not a valid port number", env_HTTP_SCTP_UDP_ENCAPS_PORT);
        } else {
            udp_encaps_port = (in_port_t)port;
        }
    }
}

static int
readln(int sd, char *resbuf, int *resbuflen, int *resbufpos)
{
    ssize_t len;
    struct msghdr msg;
    struct iovec iov;
    char cmsgbuf[CMSG_SPACE(sizeof(struct sctp_rcvinfo))];
    struct sctp_rcvinfo *rcvinfo;

    memset(cmsgbuf, 0, CMSG_SPACE(sizeof(struct sctp_rcvinfo)));

    while (strnstr(resbuf + *resbufpos, "\r\n", *resbuflen - *resbufpos) == NULL) {
        /* Move buffered data to the start of the buffer */
        if (*resbufpos != 0) {
            memmove(resbuf, resbuf + *resbufpos, *resbuflen - *resbufpos);
            *resbuflen -= *resbufpos;
            *resbufpos = 0;
        }

        /* If the buffer is full, complain */
        if (*resbuflen == BUFSIZ) {
            fprintf(stderr, "buffer is full\n");
            return -1;
        }

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

        if (protocol == IPPROTO_SCTP) {
            lastStream = rcvinfo->rcv_sid;
        }

        if ((len == 0) || ((len == -1) && (errno != EINTR))) {
            return -1;
        }

        if (len != -1) {
            *resbuflen += len;
        }
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

        if (protocol == IPPROTO_SCTP) {
            lastStream = rcvinfo->rcv_sid;
        }

        /* Read more data into buffer */
        //len = recv(sd, resbuf, BUFSIZ, 0);
        if (len == -1) {
            if (errno == EINTR) {
                continue;
            }
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

static int
send_request(int sd,  char *reqbuf, int *reqbuflen, int *reqbufpos) {
    char* cmsgbuf[CMSG_SPACE(sizeof(struct sctp_sndinfo))];
    struct iovec iov;
    struct msghdr msghdr;
    struct sctp_sndinfo *sndinfo;
    struct cmsghdr *cmsg;
    int len;


    /* initialize */
    memset(cmsgbuf, 0, CMSG_SPACE(sizeof(struct sctp_sndinfo)));
    memset(&msghdr, 0, sizeof(struct msghdr));
    memset(&iov, 0, sizeof(struct iovec));
    streamsbusy = 0;

    msghdr.msg_iov = &iov;
    msghdr.msg_iovlen = 1;

    /* if using SCTP, append control msg */
    if (protocol == IPPROTO_SCTP) {
        msghdr.msg_control = cmsgbuf;
        msghdr.msg_controllen = CMSG_SPACE(sizeof(struct sctp_sndinfo));

        cmsg = CMSG_FIRSTHDR(&msghdr);
        cmsg->cmsg_level = IPPROTO_SCTP;
        cmsg->cmsg_type = SCTP_SNDINFO;
        cmsg->cmsg_len = CMSG_LEN(sizeof(struct sctp_sndinfo));

        sndinfo = (struct sctp_sndinfo*) CMSG_DATA(cmsg);

        /* lookup next free stream - if all streams busy return with -2 */
        if (getnextstream(sndinfo) == -1) {
            streamsbusy = 1;
            fprintf(stderr, "problem : send_request failed - streams are busy...");
            return -2;
        }
    }

    /* send until we reached the end of the buffer... */
    while (*reqbufpos < *reqbuflen) {
        iov.iov_base = reqbuf + *reqbufpos;
        iov.iov_len = *reqbuflen - *reqbufpos;

        if ((len = sendmsg(sd, &msghdr, 0)) < 0) {
            break;
        }

        /* move reqbufpos by sent bytes */
        *reqbufpos += len;
    }

    return 0;
}

static int
handle_response(int *sd, int *fd, char *resbuf, int *resbuflen, int *resbufpos, int *nres, int *keepalive, int *pipelined)
{
    int error = 0;
    char * hln;                 /* Pointer within header line */
    char * eolp;                /* Pointer to "\r\n" within resbuf */
    int statuscode;             /* HTTP Status code */
    off_t contentlength;        /* Value from Content-Length header */
    int chunked;                /* != if transfer-encoding is chunked */
    off_t clen;                 /* Chunk length */
    char * fname = NULL;        /* Name of downloaded file */
    struct request *request;    /* request from queue */


    statuscode = 0;
    contentlength = -1;
    chunked = 0;
    *keepalive = 0;

    if ((request = TAILQ_FIRST(&requests_pending)) == NULL) {
        fprintf(stderr, "%s - this should not happen\n", __func__);
        exit(-1);
    }

    do {
        /* Get a header line */
        if (readln(*sd, resbuf, resbuflen, resbufpos)) {
            fprintf(stderr, "handle_response - readline error\n");
            return -1;
        }

        hln = resbuf + *resbufpos;
        eolp = strnstr(hln, "\r\n", *resbuflen - *resbufpos);
        *resbufpos = (eolp - resbuf) + 2;
        *eolp = '\0';

        /* Make sure it doesn't contain a NUL character */
        if (strchr(hln, '\0') != eolp) {
            return -1;
        }

        if (statuscode == 0) {
            /* The first line MUST be HTTP/1.x xxx ... */
            if ((strncmp(hln, "HTTP/1.", 7) != 0) || !isdigit(hln[7])) {
                return -1;
            }

            /*
             * If the minor version number isn't zero,
             * then we can assume that pipelining our
             * requests is OK -- as long as we don't
             * see a "Connection: close" line later
             * and we either have a Content-Length or
             * Transfer-Encoding: chunked header to
             * tell us the length.
             */
            if (hln[7] != '0') {
                *pipelined = 1;
            }

            /* Skip over the minor version number */
            hln = strchr(hln + 7, ' ');
            if (hln == NULL) {
                return -1;
            } else {
                hln++;
            }

            /* Read the status code */
            while (isdigit(*hln)) {
                statuscode = statuscode * 10 +
                    *hln - '0';
                hln++;
            }

            if (statuscode < 100 || statuscode > 599) {
                return -1;
            }

            /* Ignore the rest of the line */
            continue;
        }

        /*
         * Check for "Connection: close" or
         * "Connection: Keep-Alive" header
         */
        if (strncasecmp(hln, "Connection:", 11) == 0) {
            hln += 11;
            if (strcasestr(hln, "close") != NULL) {
                *pipelined = 0;
            }

            if (strcasestr(hln, "Keep-Alive") != NULL) {
                *keepalive = 1;
            }

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
                    return -1;
                }
                contentlength = contentlength * 10 + *hln - '0';
                hln++;
            }

            /* Next header... */
            continue;
        }

        /* Check for "Transfer-Encoding: chunked" header */
        if (strncasecmp(hln, "Transfer-Encoding:", 18) == 0) {
            hln += 18;
            if (strcasestr(hln, "chunked") != NULL) {
                chunked = 1;
            }

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
        (*nres)++;
        return 0;
    }

    /*
     * There should be a message body coming, but we only want
     * to send it to a file if the status code is 200
     */
    if (statuscode == 200) {
        /* Generate a file name for the download */
        fname = strrchr(request->url, '/');
        if (fname == NULL) {
            fname = request->url;
        } else {
            fname++;
        }

        if (strlen(fname) == 0) {
            errx(1, "Cannot obtain file name from %s\n", request->url);
        }

        *fd = open(fname, O_CREAT | O_TRUNC | O_WRONLY, 0644);
        if (*fd == -1) {
            errx(1, "open(%s)", fname);
        }
    };

    /* Read the message and send data to fd if appropriate */
    if (chunked) {
        /* Handle a chunked-encoded entity */

        /* Read chunks */
        do {
            error = readln(*sd, resbuf, resbuflen, resbufpos);
            if (error) {
                return -1;
            }
            hln = resbuf + *resbufpos;
            eolp = strstr(hln, "\r\n");
            *resbufpos = (eolp - resbuf) + 2;

            clen = 0;
            while (isxdigit(*hln)) {
                if (clen >= OFF_MAX / 16) {
                    /* Nasty people... */
                    return -1;
                }

                if (isdigit(*hln)) {
                    clen = clen * 16 + *hln - '0';
                } else {
                    clen = clen * 16 + 10 + tolower(*hln) - 'a';
                }
                hln++;
            }

            error = copybytes(*sd, *fd, clen, resbuf, resbuflen, resbufpos);
            if (error) {
                return -1;
            }
        } while (clen != 0);



        /* Read trailer and final CRLF */
        do {
            error = readln(*sd, resbuf, resbuflen, resbufpos);
            if (error)
                return -1;
            hln = resbuf + *resbufpos;
            eolp = strstr(hln, "\r\n");
            *resbufpos = (eolp - resbuf) + 2;
        } while (hln != eolp);
    } else if (contentlength != -1) {
        error = copybytes(*sd, *fd, contentlength, resbuf, resbuflen, resbufpos);
        if (error) {
            printf("copybytes problem\n");
            return -1;
        }
    } else {

        /*
         * Not chunked, and no content length header.
         * Read everything until the server closes the
         * socket.
         */
        error = copybytes(*sd, *fd, OFF_MAX, resbuf, resbuflen, resbufpos);
        if (error == -1) {
            printf("copybytes problem\n");
            return -1;
        }
        *pipelined = 0;
    }

    if (*fd != -1) {
        close(*fd);
        *fd = -1;
    }

    fprintf(stderr, "http://%s/%s - %d - %d - ", servername, request->url, statuscode, *nres);

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

    return 0;
}


int
main(int argc, char *argv[])
{
    struct addrinfo hints;          /* Hints to getaddrinfo */
    struct addrinfo *res;           /* Pointer to server address being used */
    struct addrinfo *res0;          /* Pointer to server addresses */
#ifdef SCTP_REMOTE_UDP_ENCAPS_PORT
    struct sctp_udpencaps encaps;   /* SCTP/UDP information */
#endif

    char * reqbuf = NULL;           /* Request buffer */
    int reqbufpos = 0;              /* Request buffer position */
    int reqbuflen = 0;              /* Request buffer length */
    int nreq = 0;                   /* Number of next request to send */
    int nres = 0;                   /* Number of next reply to receive */
    int pipelined = 0;              /* != 0 if connection in pipelined mode. */
    int sd = -1;                    /* Socket descriptor */
    int sdflags = 0;                /* Flags on the socket sd */
    int error;                      /* Error code */
    int i = 0;                      /* For loop iterator */
    int firstreq = 0;               /* # of first request for this connection */
    int val;                        /* Value used for setsockopt call */
    struct sctp_initmsg initmsg;
    int tempindex = 0;
    char *resbuf = NULL;            /* Response buffer */
    int resbuflen = 0;
    int keepalive = 0;
    int fd = 0;
    struct request *request;
    int resbufpos = 0;              /* Response buffer position */
    int              num_req_open = 0;
    int              num_req_pending = 0;

    TAILQ_INIT(&requests_open);
    TAILQ_INIT(&requests_pending);

    for (tempindex = 0; tempindex < NUM_SCTP_STREAMS; tempindex++) {
        streamstatus[tempindex] = STREAM_FREE;
    }

    /* Check that the arguments are sensible */
    if (argc < 2) {
        usage();
    }

    /* Read important environment variables */
    readenv();

    /* Get server name and adjust arg[cv] to point at file names */
    servername = argv[1];
    argv += 2;
    argc -= 2;

    /* Parse requests by cmdline arguments and queue them */
    for (i = 0; i < argc; i++) {
        if ((request = malloc(sizeof(struct request))) == NULL) {
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }

        request->url = argv[i];
        printf("queueing : %s\n", argv[i]);
        TAILQ_INSERT_TAIL(&requests_open, request, entries);
        num_req_open++;
    }

    printf("requests open : %d\n", num_req_open);

    /* Allocate response buffer */
    resbuf = malloc(BUFSIZ);
    if (resbuf == NULL) {
        err(1, "malloc");
    }

    /* Server lookup */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = protocol;
#ifdef __FreeBSD__
    error = getaddrinfo(servername, "http", &hints, &res0);
#else
    /* Not all OSes have an SCTP entry for HTTP in /etc/services yet. */
    error = getaddrinfo(servername, "80", &hints, &res0);
#endif
    if (error) {
        errx(1, "host = %s, port = %s: %s", servername, "http", gai_strerror(error));
    }
    if (res0 == NULL) {
        errx(1, "could not look up %s", servername);
    }
    res = res0;

    /* Do the fetching */
    while (num_req_open > 0 || num_req_pending > 0) {

        /* Make sure we have a connected socket */
        for (; sd == -1; res = res->ai_next) {
            /* No addresses left to try :-( */
            if (res == NULL) {
                errx(1, "Could not connect to %s", servername);
            }

            /* Create a socket... */
            sd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
            if (sd == -1) {
                continue;
            }

            /* ... set 15-second timeouts ... */
            setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO, (void *)&timo, (socklen_t)sizeof(timo));
            setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (void *)&timo, (socklen_t)sizeof(timo));

            /* set SCTP specific options */
            if (protocol == IPPROTO_SCTP) {
                /* Ensure an appropriate number of stream will be negotated. */
                initmsg.sinit_num_ostreams = NUM_SCTP_STREAMS;
                initmsg.sinit_max_instreams = NUM_SCTP_STREAMS;
                initmsg.sinit_max_attempts = 0;   /* Use default */
                initmsg.sinit_max_init_timeo = 0; /* Use default */
                if (setsockopt(sd, IPPROTO_SCTP, SCTP_INITMSG, (char*) &initmsg, sizeof(initmsg) ) < 0 ) {
                    fprintf(stderr, "problem : SCTP_INITMSG\n");
                    exit(-1);
                }

                /* Enable RCVINFO delivery */
                val = 1;
                if (setsockopt(sd, IPPROTO_SCTP, SCTP_RECVRCVINFO, (char*) &val, sizeof(val) ) < 0 ) {
                    fprintf(stderr, "problem : SCTP_RECVRCVINFO\n");
                    exit(-1);
                }

#ifdef SCTP_REMOTE_UDP_ENCAPS_PORT
                /* Use UDP encapsulation for SCTP */
                memset(&encaps, 0, sizeof(encaps));
                encaps.sue_address.ss_family = res->ai_family;
                encaps.sue_address.ss_len = res->ai_addrlen;
                encaps.sue_port = htons(udp_encaps_port);
                setsockopt(sd, IPPROTO_SCTP, SCTP_REMOTE_UDP_ENCAPS_PORT, (void *)&encaps, (socklen_t)sizeof(encaps));
#else
                if (udp_encaps_port > 0) {
                    errx(1, "UDP encapsulation not supported");
                }
#endif
            }

#ifdef SO_NOSIGPIPE
            /* ... disable SIGPIPE generation ... */
            val = 1;
            setsockopt(sd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&val, sizeof(int));
#endif

            /* ... and connect to the server. */
            if (connect(sd, res->ai_addr, res->ai_addrlen)) {
                close(sd);
                sd = -1;
                continue;
            }

            //felix - why?!
            firstreq = nres;
        }

        /*
         * If in pipelined HTTP mode, put socket into non-blocking
         * mode, since we're probably going to want to try to send
         * several HTTP requests.
         */
        if (pipelined) {
            sdflags = fcntl(sd, F_GETFL);
            if (fcntl(sd, F_SETFL, sdflags | O_NONBLOCK) == -1) {
                err(1, "fcntl");
            }
        }

        /* Construct requests and/or send them without blocking */
        while ((num_req_open > 0) && ((reqbuf == NULL) || pipelined)) {

            /* If not in the middle of a request, make one */
            if (reqbuf == NULL) {
                if ((request = TAILQ_FIRST(&requests_open)) == NULL) {
                    fprintf(stderr, "This should not happen ... make request\n");
                }

                if ((reqbuflen = asprintf(&reqbuf,
                    "GET /%s HTTP/1.1\r\n"
                    "Host: %s\r\n"
                    "User-Agent: %s\r\n"
                    "%s"
                    "\r\n",
                    request->url, servername, env_HTTP_USER_AGENT,
                    (num_req_open == 1) ? "Connection: Close\r\n" : "Connection: Keep-Alive\r\n"
                )) == -1) {
                    err(1, "asprintf");
                }

                reqbufpos = 0;
            }

            /* If in pipelined mode, try to send the request */
            if (pipelined && streamsbusy == 0) {
                if (send_request(sd, reqbuf, &reqbuflen, &reqbufpos) == -1) {
                    fprintf(stderr, "send_request failed\n");
                    goto conndied; //handle more precisely...
                }

                if (reqbufpos < reqbuflen) {
                    printf("pipelined - reqbufpos < reqbuflen\n");
                    if (errno != EAGAIN && streamsbusy == 0) {
                        goto conndied;
                    }
                    break;
                } else {
                    free(reqbuf);
                    reqbuf = NULL;


                    /* move queue element from open to pending */
                    if ((request = TAILQ_FIRST(&requests_open)) == NULL) {
                        fprintf(stderr, "%s - this should not happen...\n", __func__);
                        exit(-1);
                    }
                    TAILQ_REMOVE(&requests_open, request, entries);
                    TAILQ_INSERT_TAIL(&requests_pending, request, entries);

                    nreq++;
                    num_req_open--;
                    num_req_pending++;

                    printf("request : %s - open -> pending\n", request->url);
                }
            }
        }

        /* Put connection back into blocking mode */
        if (pipelined) {
            if (fcntl(sd, F_SETFL, sdflags) == -1)
                err(1, "fcntl");
        }

        /* sending last request ... do we need to blocking-send a request? */
        if (nreq == nres && streamsbusy == 0) {
            if (send_request(sd, reqbuf, &reqbuflen, &reqbufpos) == -1) {
                fprintf(stderr, "send_request failed\n");
                goto conndied; //handle more precisely...
            }

            if (reqbufpos < reqbuflen) {
                printf("non pipelined - reqbufpos < reqbuflen\n");
                if (errno != EAGAIN && streamsbusy == 0) {
                    goto conndied;
                }
                break;
            } else {
                free(reqbuf);
                reqbuf = NULL;

                /* move queue element from open to pending */
                if ((request = TAILQ_FIRST(&requests_open)) == NULL) {
                    fprintf(stderr, "%s - this should not happen...\n", __func__);
                    exit(-1);
                }
                TAILQ_REMOVE(&requests_open, request, entries);
                TAILQ_INSERT_TAIL(&requests_pending, request, entries);

                nreq++;
                num_req_open--;
                num_req_pending++;

                printf("request : %s - open -> pending\n", request->url);
            }
        }

        /* Get response and delete entry from pending list */
        if (handle_response(&sd, &fd, resbuf, &resbuflen, &resbufpos, &nres, &keepalive, &pipelined) == 0) {
            request = TAILQ_FIRST(&requests_pending);
            TAILQ_REMOVE(&requests_pending, request, entries);
            free(request);

            /* We've finished this file! */
            nres++;
            num_req_pending--;

            /* at least one stream is free for a new request */
            streamsbusy = 0;

            continue;
        }

        /*
         * If necessary, clean up this connection so that we
         * can start a new one.
         */
        if (pipelined == 0 && keepalive == 0) {
            goto cleanupconn;
        }
        continue;

conndied:
        /*
         * Something went wrong -- our connection died, the server
         * sent us garbage, etc.  If this happened on the first
         * request we sent over this connection, give up.  Otherwise,
         * close this connection, open a new one, and reissue the
         * request.
         */

        if (nres == firstreq) {
            errx(1, "Connection failure");
        }

cleanupconn:
        /*
         * Clean up our connection and keep on going
         */
        printf("cleanupconn?!?!?!?!?\n");
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
        nreq = nres; // we did'nt sent all nres..
        res = res0;
        pipelined = 0;
        resbuflen = 0;
        continue;
    }

    free(resbuf);
    freeaddrinfo(res0);

    return 0;
}
