//
//  webresponse.c
//  AirFloat
//
//  Copyright (c) 2013, Kristian Trenskow All rights reserved.
//
//  Redistribution and use in source and binary forms, with or
//  without modification, are permitted provided that the following
//  conditions are met:
//
//  Redistributions of source code must retain the above copyright
//  notice, this list of conditions and the following disclaimer.
//  Redistributions in binary form must reproduce the above
//  copyright notice, this list of conditions and the following
//  disclaimer in the documentation and/or other materials provided
//  with the distribution. THIS SOFTWARE IS PROVIDED BY THE
//  COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
//  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
//  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
//  OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
//  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
//  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
//  ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "debug.h"
#include "webtools.h"
#include "webheaders.h"

#include "obj.h"

#include "webresponse.h"

#define MIN(a,b) ((a)<(b)?(a):(b))

struct web_response_t {
    uint16_t status_code;
    char* status_message;
    web_headers_p headers;
    void* content;
    size_t content_length;
    bool keep_alive;
};

struct web_response_t* web_response_create() {
    
    struct web_response_t* wr = (struct web_response_t*)obj_create(sizeof(struct web_response_t));
    
    wr->headers = web_headers_create();
    web_response_set_status(wr, 500, "Internal Server Error");
    
    return wr;
    
}

void _web_response_destroy(void* obj) {
    
    struct web_response_t* wr = (struct web_response_t*)obj;
    
    if (wr->status_message != NULL)
        free(wr->status_message);
    
    if (wr->content != NULL)
        free(wr->content);
    
    web_headers_release(wr->headers);
    
}

struct web_response_t* web_response_retain(struct web_response_t* wr) {
    
    return obj_retain(wr);
    
}

struct web_response_t* web_response_release(struct web_response_t* wr) {
    
    return obj_release(wr, _web_response_destroy);
    
}

ssize_t web_response_parse(web_response_p wr, const void* data, size_t data_size) {
    
    size_t ret = 0;
    
    char* buffer = malloc(data_size);
    memcpy(buffer, data, data_size);
    
    const char* content_start = web_tools_get_content_start(buffer, data_size);;
    
    if (content_start != NULL) {
        
        size_t header_length = content_start - buffer;
        char header[header_length];
        memcpy(header, buffer, content_start - buffer);
        
        debug_data(data, content_start - buffer);
        
        char* header_start = header;
        header_length = web_tools_convert_new_lines(header_start, header_length);
        
        char* s_status_code = NULL;
        char* status_message = NULL;
        while (header_start[0] != '\n') {
            if (header_start[0] == ' ' && status_message == NULL) {
                if (s_status_code == NULL)
                    s_status_code = (char*)&header_start[1];
                else if (status_message == NULL)
                    status_message = (char*)&header_start[1];
                header_start[0] = '\0';
            }
            header_start++;
            header_length--;
        }
        
        header_start[0] = '\0';
        
        if (s_status_code == NULL || status_message == NULL) {
            free(buffer);
            return -1;
        }
        
        header_start++;
        header_length--;
        
        web_headers_p headers = web_headers_create();
        web_headers_parse(headers, header_start, header_length);
        
        size_t content_length = 0;
        const char* s_content_length = web_headers_value(headers, "Content-Length");
        if (s_content_length != NULL)
            content_length = atoi(s_content_length);
        
        size_t actual_content_length = data_size - (content_start - buffer);
        if (content_length <= actual_content_length) {
            
            if (wr->status_message != NULL)
                free(wr->status_message);
            
            wr->status_code = atoi(s_status_code);
            wr->status_message = (char*)malloc(strlen(status_message) + 1);
            strcpy(wr->status_message, status_message);
            
            web_headers_release(wr->headers);
            wr->headers = headers;
            
            debug(LOG_INFO, "(Complete) - %d bytes", content_length);
            
            web_response_set_content(wr, (void*)content_start, content_length);
            
            ret = content_start + content_length - buffer;
            
        } else {
            debug(LOG_INFO, "(Incomplete)");
            web_headers_release(headers);
        }
        
    }
    
    free(buffer);
    
    return ret;
    
}

web_headers_p web_response_get_headers(struct web_response_t* wr) {
    
    return wr->headers;
    
}

void web_response_set_status(struct web_response_t* wr, uint16_t code, const char* message) {
    
    assert(code > 0 && code < 1000 && message != NULL);
    
    if (wr->status_message != NULL)
        free(wr->status_message);
    
    wr->status_code = code;
    wr->status_message = (char*)malloc(strlen(message) + 1);
    strcpy(wr->status_message, message);
    
}

uint16_t web_response_get_status(struct web_response_t* wr) {
    
    return wr->status_code;
    
}

const char* web_response_get_status_message(struct web_response_t* wr) {
    
    return wr->status_message;
    
}

void web_response_set_content(struct web_response_t* wr, void* content, size_t size) {
    
    if (wr->content != NULL) {
        free(wr->content);
        wr->content = NULL;
    }
    
    if (content != NULL && size > 0) {
        
        wr->content = malloc(size);
        wr->content_length = size;
        memcpy(wr->content, content, size);
        
        web_headers_set_value(wr->headers, "Content-Length", "%d", size);
        
    } else {
        
        wr->content_length = 0;
        
        web_headers_set_value(wr->headers, "Content-Length", NULL);
        
    }
    
}

size_t web_response_get_content(struct web_response_t* wr, void* content, size_t size) {
    
    if (content != NULL && wr->content != NULL)
        memcpy(content, wr->content, MIN(size, wr->content_length));
    
    return wr->content_length;
    
}

void web_response_set_keep_alive(struct web_response_t* wr, bool keep_alive) {
    
    wr->keep_alive = keep_alive;
    
}

bool web_response_get_keep_alive(struct web_response_t* wr) {
    
    return wr->keep_alive;
    
}

size_t web_response_write(web_response_p wr, const char* protocol, void* data, size_t data_size) {
    
    assert(protocol != NULL);
    
    size_t write_pos = 0;

    size_t status_message_length = strlen(wr->status_message);
    size_t protocol_length = strlen(protocol);
    
    if (data != NULL && write_pos + status_message_length + protocol_length + 7 <= data_size)
        sprintf(data, "%s %d %s\r\n", protocol, wr->status_code, wr->status_message);
    
    write_pos += status_message_length + protocol_length + 7;
    
    size_t headers_length = web_headers_write(wr->headers, NULL, 0);
    
    if (data != NULL && write_pos + headers_length <= data_size)
        web_headers_write(wr->headers, data + write_pos, data_size - write_pos);
    
    return write_pos + headers_length;
    
}
