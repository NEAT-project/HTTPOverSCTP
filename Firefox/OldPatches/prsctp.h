/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version 	
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan T. Leighton <leighton@cis.udel.edu> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * File:        prsctp.h
 *
 * Description: NSPR versions of SCTP data structures.  Structure definitions were
 *              chosen to match the -19 version of the SCTP Socket API ID.  Hopefully
 *              once the ID is finalized it will bring better alignment between the
 *              various platforms.
 *
 */

#ifndef prsctp_h__
#define prsctp_h__

#if defined(ENABLE_SCTP)
/* Need this to define the platform dependent opaque sctp_assoc_t structure.  */
#include <netinet/sctp.h>

typedef struct PRSctp_SndRcvInfo {
        PRUint16      sinfo_stream;
        PRUint16      sinfo_ssn;
        PRUint16      sinfo_flags;
        PRUint32      sinfo_ppid;
        PRUint32      sinfo_context;
        PRUint32      sinfo_pr_value;             /* sinfo_timetolive on all platforms */
        PRUint32      sinfo_tsn;
        PRUint32      sinfo_cumtsn;
        sctp_assoc_t  sinfo_assoc_id;
} PRSctp_SndRcvInfo;

typedef struct PRSctp_ExtRcvInfo {                /* Doesn't exist on Linux or Solaris */
        PRSctp_SndRcvInfo  serinfo_sinfo;         /* yet...                            */
        PRUint16           serinfo_next_flags;
        PRUint16           serinfo_next_stream;
        PRUint32           serinfo_next_aid;
        PRUint32           serinfo_next_length;
        PRUint32           serinfo_next_ppid;
} PRSctp_ExtRcvInfo;

typedef struct PRSctp_Event_Subscribe {
        PRUint8  sctp_data_io_event;
        PRUint8  sctp_association_event;
        PRUint8  sctp_address_event;
        PRUint8  sctp_send_failure_event;
        PRUint8  sctp_peer_error_event;
        PRUint8  sctp_shutdown_event;
        PRUint8  sctp_partial_delivery_event;
        PRUint8  sctp_adaptation_layer_event;     /* Spelled "adaption" on Solaris */
        PRUint8  sctp_authentication_event;       /* Member doesn't exist on Solaris */
        PRUint8  sctp_sender_dry_event;           /* Member exists only on FreeBSD 7.2. */
                                                  /* Darwin and other FreeBSDs > 7.0    */
                                                  /* have sctp_stream_reset_events      */
                                                  /* instead.  So... sender_dry_event   */
                                                  /* is only supported on FreeBSD 7.2   */
                                                  /* and stream_reset_events is not     */
                                                  /* supported on any platform, yet...  */
} PRSctp_Event_Subscribe;

typedef struct PRSctp_InitMsg {
        PRUint16  sinit_num_ostreams;             /* uint32_t on Darwin and FreeBSD */
        PRUint16  sinit_max_instreams;            /* uint32_t on Darwin and FreeBSD */
        PRUint16  sinit_max_attempts;
        PRUint16  sinit_max_init_timeo;
} PRSctp_InitMsg;

typedef struct PRSctp_PAddrInfo {
        sctp_assoc_t             spinfo_assoc_id; /* assoc_id and address in reverse */
        PRNetAddr                spinfo_address;  /* order on FreeBSD and Darwin */
        PRInt32                  spinfo_state;
        PRUint32                 spinfo_cwnd;
        PRUint32                 spinfo_srtt;
        PRUint32                 spinfo_rto;
        PRUint32                 spinfo_mtu;
} PRSctp_PAddrInfo;

typedef struct PRSctp_Status {
        sctp_assoc_t             sstat_assoc_id;
        PRInt32                  sstat_state;
        PRUint32                 sstat_rwnd;
        PRUint16                 sstat_unackdata;
        PRUint16                 sstat_penddata;
        PRUint16                 sstat_instrms;
        PRUint16                 sstat_outstrms;
        PRUint32                 sstat_fragmentation_point;
        struct PRSctp_PAddrInfo  sstat_primary;
} PRSctp_Status;

typedef struct PRSctp_AssocParams {               /* Member order is very different on */
        sctp_assoc_t  sasoc_assoc_id;             /* FreeBSD and Darwin                */
        PRUint16      sasoc_asocmaxrxt;
        PRUint16      sasoc_number_peer_destinations;
        PRUint32      sasoc_peer_rwnd;
        PRUint32      sasoc_local_rwnd;
        PRUint32      sasoc_cookie_life;
} PRSctp_AssocParams;

typedef struct PRSctp_SetPrim {                   /* Member order reversed on FreeBSD */
        sctp_assoc_t             ssp_assoc_id;    /* and Darwin.                      */
        PRNetAddr                ssp_addr;
} PRSctp_SetPrim;

typedef struct PRSctp_SetPeerPrim {               /* Member order reversed on FreeBSD */
        sctp_assoc_t             sspp_assoc_id;   /* and Darwin.                      */
        PRNetAddr                sspp_addr;
} PRSctp_SetPeerPrim;

typedef struct PRSctp_Assoc_Value {               /* Doesn't exist on Solaris */
        sctp_assoc_t  assoc_id;
        PRUint32      assoc_value;
} PRSctp_Assoc_Value;

#else /* ! ENABLE_SCTP */

/* Dummy definition */
typedef struct PRSctp_SndRcvInfo {
        PRUint32     nosctp;
} PRSctp_SndRcvInfo;

#endif /* ! ENABLE_SCTP */

#endif /* prsctp_h__ */
