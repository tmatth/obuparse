/*
 * Copyright (c) 2020, Derek Buitenhuis
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Currently just an ad-hoc IVF parser to feed packets into obuparse to
 * help spot-check APIs.
 */

#ifdef _WIN32
#define fseeko _fseeki64
#define ftello _fseeki64
#define off_t __int64
#else
#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE
#endif

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "obuparse.h"

int main(int argc, char *argv[])
{
    FILE *ivf;
    int ret = 0;

    if (argc < 2) {
        printf("Usage: %s file.ivf\n", argv[0]);
        return 1;
    }

    ivf = fopen(argv[1], "rb");
    if (ivf == NULL) {
        printf("Couldn't open '%s'.\n", argv[1]);
        ret = 1;
        goto end;
    }

    ret = fseeko(ivf, 32, SEEK_SET);
    if (ret != 0) {
        printf("Failed to seek past IVF header.\n");
        ret = 1;
        goto end;
    }

    while (!feof(ivf))
    {
        uint8_t frame_header[12];
        uint8_t *packet_buf;
        size_t packet_size;
        size_t packet_pos = 0;

        size_t read_in = fread(&frame_header[0], 1, 12, ivf);
        if (read_in != 12) {
            if (feof(ivf))
                break;
            printf("Failed to read in IVF frame header (read %zu)\n", read_in);
            ret = 1;
            goto end;
        }

        assert(sizeof(packet_size) >= 4);

        packet_size =  frame_header[0]        +
                      (frame_header[1] << 8)  +
                      (frame_header[2] << 16) +
                      (frame_header[3] << 24);
        printf("Packet Size = %zu\n", packet_size);

        packet_buf = malloc(packet_size);
        if (packet_buf == NULL) {
            printf("Could not allocate packet buffer.\n");
            ret = 1;
            goto end;
        }

        read_in = fread(packet_buf, 1, packet_size, ivf);
        if (read_in != packet_size) {
            free(packet_buf);
            printf("Could not read in packet (read %zu)\n", read_in);
            ret = 1;
            goto end;
        }

        while (packet_pos < packet_size)
        {
            char err_buf[1024];
            ptrdiff_t offset;
            size_t obu_size;
            int temporal_id, spatial_id;
            OBPOBUType obu_type;
            OBPError err = { &err_buf[0], 1024 };

            ret = obp_get_next_obu(packet_buf + packet_pos, packet_size - packet_pos, 
                                   &obu_type, &offset, &obu_size, &temporal_id, &spatial_id, &err);
            if (ret < 0) {
                free(packet_buf);
                printf("Failed to parse OBU header: %s\n", err.error);
                ret = 1;
                goto end;
            }

            printf("OBU info | obu_type = %d | offset = %td | obu_size = %zu | temporal_id = %d | spatial_id = %d\n",
                   obu_type, offset, obu_size, temporal_id, spatial_id);

            packet_pos += obu_size + (size_t) offset;
        }

        free(packet_buf);

        if (packet_pos != packet_size) {
            printf("Didn't consume whole packet (%zu vs %zu).\n", packet_size, packet_pos);
            ret = 1;
            goto end;
        }
    }

end:
    fclose(ivf);
    return ret;
}