/***************************************************************************
 * Copyright (c) 2009-2010, Open Information Security Foundation
 * Copyright (c) 2009-2012, Qualys, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the Qualys, Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ***************************************************************************/

/**
 * @file
 * @author Ivan Ristic <ivanr@webkreator.com>
 */

#include "htp_private.h"

void htp_connp_clear_error(htp_connp_t *connp) {
    connp->last_error = NULL;
}

void htp_connp_close(htp_connp_t *connp, htp_time_t *timestamp) {
    // Close the underlying connection.
    htp_conn_close(connp->conn, timestamp);

    // Update internal flags
    connp->in_status = STREAM_STATE_CLOSED;
    connp->out_status = STREAM_STATE_CLOSED;

    // Call the parsers one last time, which will allow them
    // to process the events that depend on stream closure
    htp_connp_req_data(connp, timestamp, NULL, 0);
    htp_connp_res_data(connp, timestamp, NULL, 0);   
}

htp_connp_t *htp_connp_create(htp_cfg_t *cfg) {
    htp_connp_t *connp = calloc(1, sizeof (htp_connp_t));
    if (connp == NULL) return NULL;

    // Use the supplied configuration structure
    connp->cfg = cfg;

    // Create a new connection.
    connp->conn = htp_conn_create();
    if (connp->conn == NULL) {
        free(connp);
        return NULL;
    }

    connp->in_status = HTP_OK;

    // Request parsing

    connp->in_line_size = cfg->field_limit_hard;
    connp->in_line_len = 0;
    connp->in_line = malloc(connp->in_line_size);
    if (connp->in_line == NULL) {
        htp_conn_destroy(connp->conn);
        free(connp);
        return NULL;
    }

    connp->in_header_line_index = -1;
    connp->in_state = htp_connp_REQ_IDLE;

    // Response parsing

    connp->out_line_size = cfg->field_limit_hard;
    connp->out_line_len = 0;
    connp->out_line = malloc(connp->out_line_size);
    if (connp->out_line == NULL) {
        free(connp->in_line);
        htp_conn_destroy(connp->conn);
        free(connp);
        return NULL;
    }

    connp->out_header_line_index = -1;
    connp->out_state = htp_connp_RES_IDLE;

    connp->in_status = STREAM_STATE_NEW;
    connp->out_status = STREAM_STATE_NEW;

    return connp;
}

void htp_connp_destroy(htp_connp_t *connp) {
    if (connp == NULL) return;
        
    if (connp->out_decompressor != NULL) {
        connp->out_decompressor->destroy(connp->out_decompressor);
        connp->out_decompressor = NULL;
    }

    if (connp->in_header_line != NULL) {
        if (connp->in_header_line->line != NULL) {
            free(connp->in_header_line->line);
        }

        free(connp->in_header_line);
    }

    if (connp->in_line != NULL) {
        free(connp->in_line);
    }

    if (connp->out_header_line != NULL) {
        if (connp->out_header_line->line != NULL) {
            free(connp->out_header_line->line);
        }

        free(connp->out_header_line);
    }

    if (connp->out_line != NULL) {
        free(connp->out_line);
    }

    free(connp);
}

void htp_connp_destroy_all(htp_connp_t *connp) {
    if (connp == NULL) return;

    // Destroy connection
    htp_conn_destroy(connp->conn);
    connp->conn = NULL;

    // Destroy everything else
    htp_connp_destroy(connp);
}

htp_log_t *htp_connp_get_last_error(const htp_connp_t *connp) {
    return connp->last_error;
}

void *htp_connp_get_user_data(const htp_connp_t *connp) {
    return connp->user_data;
}

void htp_connp_in_reset(htp_connp_t *connp) {
    connp->in_content_length = -1;
    connp->in_body_data_left = -1;
    connp->in_header_line_index = -1;
    connp->in_header_line_counter = 0;
    connp->in_chunk_request_index = connp->in_chunk_count;
}

void htp_connp_open(htp_connp_t *connp, const char *remote_addr, int remote_port, const char *local_addr,
        int local_port, htp_time_t *timestamp)
{
    // Check connection parser state first.
    if ((connp->in_status != STREAM_STATE_NEW) || (connp->out_status != STREAM_STATE_NEW)) {
        htp_log(connp, HTP_LOG_MARK, HTP_LOG_ERROR, 0, "Connection is already open");
        return;
    }

    if (htp_conn_open(connp->conn, remote_addr, remote_port, local_addr, local_port, timestamp) != HTP_OK) {
        return;
    }
    
    connp->in_status = STREAM_STATE_OPEN;
    connp->out_status = STREAM_STATE_OPEN;
}

void htp_connp_set_user_data(htp_connp_t *connp, void *user_data) {
    connp->user_data = user_data;
}
