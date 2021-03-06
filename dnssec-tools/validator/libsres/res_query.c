
/*
 * Portions Copyright (c) 1995, 1996, 1997 by Trusted Information Systems, Inc.
 *
 * Permission to use, copy modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND TRUSTED INFORMATION SYSTEMS
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
 * TRUSTED INFORMATION SYSTEMS BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THE SOFTWARE.
 */
/*
 * Copyright 2005-2013 SPARTA, Inc.  All rights reserved.
 * See the COPYING file distributed with this software for details.
 */
#include "validator-internal.h"

#include "res_mkquery.h"
#include "res_support.h"
#include "res_tsig.h"
#include "res_io_manager.h"

#ifndef NULL
#define NULL (void*)0
#endif

#define ENVELOPE   10
#define EMSG_MAX   2048

size_t
wire_name_length(const u_char * field)
{
    /*
     * Calculates the number of bytes in a DNS wire format name 
     */
    size_t         j;
    if (field == NULL)
        return 0;

    /* 
     * Check that the total length is less than max allowable, and that
     * label length does not exceed 63
     */
    for (j = 0; j < NS_MAXCDNAME && field[j] && !(0xc0 & field[j]);
         j += field[j] + 1);

    /* 
     * return 0 if we exceed max allowable label length or total length
     */
    if (j >= NS_MAXCDNAME || field[j])
        return 0;
    else
        return j+1;
}

void
dump_response(const u_char * ans, size_t resplen)
{
    /*
     * Prints the "raw" response from DNS 
     */
    size_t             i, j, k;

    printf("Message length is %d\n", (int)resplen);

    for (i = 0; i < 12; i++)
        printf("%02x ", (u_char) ans[i]);
    printf("\n");
    k = 12;
    while (ans[k])
        k += ans[k] + 1;
    for (i = 12; i < k + 1; i++)
        printf("%02x ", (u_char) ans[i]);
    printf(": ");
    for (i = k + 1; i < k + 5; i++)
        printf("%02x ", (u_char) ans[i]);
    printf("\n");
    k += 5;
    if (k < resplen)
        do {
            j = wire_name_length(&ans[k]) + 10; /* j = envelope length */
            j += ntohs(*(const u_short *) (&ans[k + j - 2]));   /* adds rdata length to j */
            for (i = k; i < k + j; i++)
                printf("%02x ", (u_char) ans[i]);
            printf("\n");
            k += j;
        } while (k < resplen);
}

int
res_quecmp(u_char * query, u_char * response)
{
    if (query == NULL || response == NULL)
        return 1;

    return namecmp(&query[12], &response[12]);
}

int
theres_something_wrong_with_header(u_char * response,
                                   size_t response_length)
{
    HEADER         *header = (HEADER *) response;

    /*
     * Check to see if this is supposed to be a real response 
     */
    if (header->qr == 1 && header->opcode != ns_o_query) {
        res_log(NULL, LOG_DEBUG, "libsres: ""header: not a query!");
        return SR_HEADER_ERROR;
    }

    /*
     * Check the RCODE value.
     * RCODE of no error is always welcome 
     */
    if (header->rcode == ns_r_noerror)
        return SR_UNSET;

    /*
     * RCODE of NXDOMAIN (no such domain) is welcome in some circumtances:
     * With no other records present
     * With records in the authority (ns) section
     */
    if (header->rcode == ns_r_nxdomain) {
        if ((header->ancount == 0 && header->nscount == 0
            && header->arcount == 0) || ntohs(header->nscount) > 0)
            return SR_UNSET;

        res_log(NULL, LOG_DEBUG, "libsres: ""header: nxdomain!");
        return SR_NXDOMAIN;
    }

    switch (header->rcode) {
    case ns_r_formerr:
        res_log(NULL, LOG_DEBUG, "libsres: ""header: formerr!");
        return SR_FORMERR;

    case ns_r_servfail:
        res_log(NULL, LOG_DEBUG, "libsres: ""header: servfail!");
        return SR_SERVFAIL;

    case ns_r_notimpl:
        res_log(NULL, LOG_DEBUG, "libsres: ""header: notimpl!");
        return SR_NOTIMPL;

    case ns_r_refused:
        res_log(NULL, LOG_DEBUG, "libsres: ""header: refused!");
        return SR_REFUSED;

    default:
        res_log(NULL, LOG_DEBUG, "libsres: ""header: genericerr!");
        return SR_DNS_GENERIC_ERROR;
    }

    return SR_UNSET;
}

int
clone_ns(struct name_server **cloned_ns, struct name_server *ns)
{
    int             i, j;
    int name_len;

    if (ns == NULL) {
        *cloned_ns = NULL;
        return SR_UNSET;
    }

    /*
     * Create the structure for the name server 
     */
    *cloned_ns = (struct name_server *)
        MALLOC(sizeof(struct name_server));
    if (*cloned_ns == NULL)
        return SR_MEMORY_ERROR;

    /*
     * Make room for the name and insert the name 
     */
    name_len = wire_name_length(ns->ns_name_n);
    memcpy((*cloned_ns)->ns_name_n, ns->ns_name_n, name_len);

    /*
     * Initialize the rest of the fields 
     */
    (*cloned_ns)->ns_tsig = clone_ns_tsig(ns->ns_tsig);
    (*cloned_ns)->ns_security_options = ns->ns_security_options;
    (*cloned_ns)->ns_status = ns->ns_status;

    (*cloned_ns)->ns_options = ns->ns_options;
    (*cloned_ns)->ns_edns0_size = ns->ns_edns0_size;
    (*cloned_ns)->ns_retrans = ns->ns_retrans;
    (*cloned_ns)->ns_retry = ns->ns_retry;

    (*cloned_ns)->ns_address = (struct sockaddr_storage **)
        MALLOC(ns->ns_number_of_addresses *
               sizeof(struct sockaddr_storage *));
    if ((*cloned_ns)->ns_address == NULL) {
        return SR_MEMORY_ERROR;
    }
    for (i = 0; i < ns->ns_number_of_addresses; i++) {
        (*cloned_ns)->ns_address[i] =
            (struct sockaddr_storage *)
            MALLOC(sizeof(struct sockaddr_storage));
        if ((*cloned_ns)->ns_address[i] == NULL) {
            for (j = 0; j < i; j++) {
                FREE((*cloned_ns)->ns_address[i]);
            }
            FREE((*cloned_ns)->ns_address);
            (*cloned_ns)->ns_address = NULL;
        } else {
            memset((*cloned_ns)->ns_address[i], 0, 
                    sizeof(struct sockaddr_storage));
        }
    }

    if ((ns->ns_number_of_addresses > 0)
        && (*cloned_ns)->ns_address == NULL) {
        FREE(*cloned_ns);
        *cloned_ns = NULL;
        return SR_MEMORY_ERROR;
    }
    (*cloned_ns)->ns_number_of_addresses = ns->ns_number_of_addresses;
    (*cloned_ns)->ns_next = NULL;
    for (i = 0; i < ns->ns_number_of_addresses; i++) {
        memcpy((*cloned_ns)->ns_address[i], (ns)->ns_address[i],
               sizeof(struct sockaddr_storage));
    }

    return SR_UNSET;
}

int
clone_ns_list(struct name_server **ns_list,
              struct name_server *orig_ns_list)
{
    struct name_server *ns, *tail_ns;
    int             ret_val;

    *ns_list = NULL;
    for (ns = orig_ns_list; ns != NULL; ns = ns->ns_next) {

        struct name_server *temp_ns;
        if ((ret_val = clone_ns(&temp_ns, ns)) != SR_UNSET) {
            free_name_servers(ns_list);
            return ret_val;
        }

        /*
         * Add the name server record to the list 
         */
        if (*ns_list == NULL)
            *ns_list = temp_ns;
        else {
            /*
             * Preserving order in case of round robin 
             */
            tail_ns = *ns_list;
            while (tail_ns->ns_next != NULL)
                tail_ns = tail_ns->ns_next;
            tail_ns->ns_next = temp_ns;
        }
    }
    return SR_UNSET;
}



int
query_send(const char *name,
           const u_int16_t type_h,
           const u_int16_t class_h,
           struct name_server *pref_ns, 
           int *trans_id)
{
    int             ret_val;
    struct timeval  dummy;
    struct timeval  now;

    ret_val = query_queue(name, type_h, class_h, pref_ns, trans_id);
    if (SR_UNSET != ret_val)
        return ret_val;

    timerclear(&dummy);
    gettimeofday(&now, NULL);
    res_io_check_one_tid(*trans_id, &dummy, NULL);
    //res_io_check_one_tid(*trans_id, &now, NULL);

    return SR_UNSET;
}

/*
 * create payloads and queue them in the transaction array, but do not
 * start sending them.
 *
 * this is usefull if you want to change something about the transaction
 * first (e.g. set it to default to tcp instead of udp). To start
 * transaction processing, call res_io_check_one_tid(), and then
 * response_recv() in a loop.
 */
int
query_queue(const char *name, const u_int16_t type_h, const u_int16_t class_h,
            struct name_server *pref_ns, int *trans_id)
{
    struct expected_arrival *ea;
    int             ret_val;

    if (pref_ns == NULL || name == NULL || trans_id == NULL)
        return SR_CALL_ERROR;

    *trans_id = -1;

    ea = res_async_query_create(name, type_h, class_h, pref_ns, 0);
    if (NULL == ea)
        return SR_MEMORY_ERROR;

    ret_val = res_io_queue_ea(trans_id, ea);
    if (ret_val != SR_IO_UNSET)
        return SR_INTERNAL_ERROR;

    return SR_UNSET;
}

int
res_response_checks(u_char ** answer, size_t * answer_length,
                    struct name_server **respondent)
{
    int retval;

    if (NULL == answer || NULL == answer_length)
        return SR_INTERNAL_ERROR;

    log_response(*answer, *answer_length);

    if ((*respondent != NULL) && 
        (res_tsig_verifies(*respondent, 
                           *answer, *answer_length) != SR_TS_OK))
        retval = SR_TSIG_ERROR;
    else
        retval = theres_something_wrong_with_header(*answer, *answer_length);

    if (SR_UNSET == retval)
        return retval;

    res_log(NULL,LOG_DEBUG,"libsres: ""error in response; dropping; rc %d",
            retval);
    FREE(*answer);
    *answer = NULL;
    *answer_length = 0;

    return SR_NO_ANSWER; 
}

/*
 * Returns:
 *    SR_INTERNAL_ERROR
 *    SR_NO_ANSWER_YET
 *    SR_NO_ANSWER
 *    SR_GOT_ANSWER
 *    SR_UNSET
 *
 * caller is responsible for releasing respondent, even when SR_NO_ANSWER
 * is returned.
 */
int
response_recv(int *trans_id,
              fd_set *pending_desc,
              struct timeval *closest_event,
              struct name_server **respondent,
              u_char ** answer, size_t * answer_length)
{
    int             ret_val;

    if ((NULL == trans_id) || (NULL == pending_desc) || (NULL == closest_event) ||
        (NULL == respondent) || (NULL == answer) || (NULL == answer_length))
        return SR_INTERNAL_ERROR;

    res_log(NULL, LOG_DEBUG, "libsres: ""response_recv tid %d", *trans_id);

    /*
     * Prepare the default response 
     */
    *answer = NULL;
    *answer_length = 0;
    *respondent = NULL;

    ret_val = res_io_accept(*trans_id, pending_desc, closest_event, 
                            answer, answer_length, respondent);

    ret_val = res_map_srio_to_sr(ret_val);

    return ret_val;
}

int
get(const char *name,
    const u_int16_t type_h,
    const u_int16_t class_h,
    struct name_server *nslist,
    struct name_server **server,
    u_char ** response, size_t * response_length)
{
    int             ret_val;
    int             trans_id;
    struct timeval closest_event;
    fd_set pending_desc;
    if (SR_UNSET == (ret_val = query_send(name, type_h, class_h, nslist, &trans_id))) {
        if (server)
            *server = NULL;
        res_log(NULL,LOG_DEBUG,"libsres: ""get %s", name);
        do {
            FD_ZERO(&pending_desc);
            timerclear(&closest_event);

            if (server && NULL != *server) {
                free_name_server(server);
                *server = NULL;
            }
            ret_val = response_recv(&trans_id, &pending_desc, &closest_event, server, response,
                                    response_length);

            if (ret_val == SR_NO_ANSWER_YET) {
                /* wait for some data to become available */
                wait_for_res_data(&pending_desc, &closest_event);
            }
        } while (ret_val == SR_NO_ANSWER_YET);
        res_cancel(&trans_id); /* cleanup transaction */
    }

    return ret_val;
}

int
get_tcp(const char *name, u_int16_t type_h, u_int16_t class_h,
        struct name_server *nslist,
        struct name_server **server,
        u_char ** response, size_t * response_length)
{
    int             ret_val;
    int             trans_id;
    struct timeval closest_event;
    fd_set pending_desc;

    ret_val = query_queue(name, type_h, class_h, nslist, &trans_id);
    if (SR_UNSET != ret_val)
        return ret_val;

    res_switch_all_to_tcp_tid(trans_id);
    if (server)
        *server = NULL;

    res_log(NULL,LOG_DEBUG,"libsres: ""get_tcp %s", name);
    do {
        FD_ZERO(&pending_desc);
        timerclear(&closest_event);

        if (server && NULL != *server) {
            free_name_server(server);
            *server = NULL;
        }
        ret_val = response_recv(&trans_id, &pending_desc, &closest_event,
                                server, response, response_length);

        if (ret_val == SR_NO_ANSWER_YET) {
            /* wait for some data to become available */
            wait_for_res_data(&pending_desc, &closest_event);
        }
    } while (ret_val == SR_NO_ANSWER_YET);

    res_cancel(&trans_id); /* cleanup transaction */

    return ret_val;
}
