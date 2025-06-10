#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "stream.h"
#include "stream_intern.h"
#include "util.h"


/**
 * Helper for token scan methods
 *
 * Worth exposing publicly if we later want to make the stream interface available
 * to users to implement their own.
 *
 * NOTE: This is dog**** performance though; spend more time optimizing later, could use SIMD
 */
int asdf_scan_tokens(
    const uint8_t *buf, size_t len, const uint8_t **tokens, const size_t *token_lens,
    size_t n_tokens, size_t *match_offset, size_t *match_token_idx) {

    if (!buf || len == 0 || n_tokens == 0)
        return 1;

    size_t max_token_len = 0;

    for (size_t idx = 0; idx < n_tokens; idx++) {
        if (token_lens[idx] > max_token_len)
            max_token_len = token_lens[idx];
    }

    for (size_t idx = 0; idx < len; idx++) {
        for (size_t tdx = 0; tdx < n_tokens; tdx++) {
            size_t tok_len = token_lens[tdx];
            if (tok_len <= len - idx && memcmp(buf + idx, tokens[tdx], tok_len) == 0) {
                if (match_offset)
                    *match_offset = idx;

                if (match_token_idx)
                    *match_token_idx = tdx;

                return 0;
            }
        }
    }

    return 1;
}


static void stream_capture(asdf_stream_t *stream, const uint8_t *buf, size_t size) {
    if (LIKELY(!stream->capture_buf))
        return;

    uint8_t *capture_buf = *stream->capture_buf;

    if (!capture_buf)
        return;

    size_t needed = *stream->capture_size + size;

    if (needed > stream->capture_cap) {
        size_t new_cap = stream->capture_cap * 2;
        uint8_t *new_buf = realloc(capture_buf, new_cap);

        if (!new_buf) {
            stream->error = ASDF_STREAM_ERR_OOM;
            return;
        }

        capture_buf = new_buf;
        *stream->capture_buf = new_buf;
        stream->capture_cap = new_cap;
    }

    memcpy(capture_buf + *stream->capture_size, buf, size);
    *stream->capture_size += size;
}


void asdf_stream_set_capture(asdf_stream_t *stream, uint8_t **buf, size_t *size, size_t capacity) {
    if (!stream)
        return;

    stream->capture_buf = buf;
    stream->capture_cap = capacity;
    stream->capture_size = size;
}


/**
 * File-backed read handling
 */
static const uint8_t *file_next(asdf_stream_t *stream, size_t *avail) {
    assert(stream);
    assert(avail);
    file_userdata_t *data = stream->userdata;

    if (data->buf_pos >= data->buf_avail) {
        size_t n = fread(data->buf, 1, data->buf_size, data->file);
        if (n <= 0) {
            *avail = 0;
            return NULL;
        }
        data->buf_avail = n;
        data->buf_pos = 0;
        data->offset += n;
    }

    *avail = data->buf_avail - data->buf_pos;
    return data->buf + data->buf_pos;
}


static void file_consume(asdf_stream_t *stream, size_t count) {
    assert(stream);
    file_userdata_t *data = stream->userdata;

    stream_capture(stream, data->buf + data->buf_pos, count);

    data->buf_pos += count;

    if (data->buf_pos > data->buf_avail) {
        data->buf_pos = data->buf_avail;
    }
}


// The file-based readline only returns lines up to the file buffer size
// Anything longer than that is truncated, though it still advances the file
// to the end of the line (or EOF)
const char *file_readline(asdf_stream_t *stream, size_t *len) {
    file_userdata_t *data = stream->userdata;

    size_t avail = 0;
    const uint8_t *buf = file_next(stream, &avail);

    if (!buf || avail == 0) {
        *len = 0;
        return NULL;
    }

    for (size_t idx = 0; idx < avail; idx++) {
        if (buf[idx] == '\n') {
            *len = idx + 1;
            file_consume(stream, idx + 1);
            return (const char *)buf;
        }
    }

    // No newline found in current buffer
    // Truncate line at buffer end and discard remainder of line
    file_consume(stream, avail);

    int ch;
    while ((ch = fgetc(data->file)) != EOF) {
        if (ch == '\n')
            break;
    }

    *len = avail;
    return (const char *)buf;
}


static int file_scan(struct asdf_stream *stream, const uint8_t **tokens, const size_t *token_lens,
                     size_t n_tokens, size_t *match_offset, size_t *match_token_idx) {
    // File-based scan is a little trickier because tokens could straddle buffered pages of the
    // file, so we maintain a sliding window so that the last (max_token_len - 1) bytes are always
    // available at the beginning of the next read
    file_userdata_t *data = stream->userdata;
    size_t max_token_len = 0;
    size_t offset = 0;
    size_t token_idx = 0;

    for (size_t idx = 0; idx < n_tokens; idx++) {
        if (token_lens[idx] > max_token_len)
            max_token_len = token_lens[idx];
    }

    if (max_token_len == 0)
        return 1;

    while (true) {
        size_t avail = data->buf_avail - data->buf_pos;
        int res = 1;

        if (avail >= max_token_len)
            res = asdf_scan_tokens(data->buf + data->buf_pos, avail, tokens, token_lens, n_tokens,
                                   &offset, &token_idx);

        if (0 == res) {
            if (match_offset)
                *match_offset = data->offset - data->buf_avail + offset;

            if (match_token_idx)
                *match_token_idx = token_idx;

            file_consume(stream, offset);
            return res;
        }

        size_t preserve = (avail < max_token_len - 1) ? avail : max_token_len - 1;
        stream_capture(stream, data->buf, data->buf_avail - preserve);
        memmove(data->buf, data->buf + data->buf_avail - preserve, preserve);
        size_t n = fread(data->buf + preserve, 1, data->buf_size - preserve, data->file);
        size_t new_avail = preserve + n;
        data->buf_avail = new_avail;
        data->offset += n;

        if (new_avail < max_token_len) {
            data->buf_pos = new_avail;
            return 1;
        }

        data->buf_pos = 0;
    }
}


static int file_seek(asdf_stream_t *stream, off_t offset, int whence) {
    file_userdata_t *data = stream->userdata;
return fseeko(data->file, offset, whence);
}


static off_t file_tell(asdf_stream_t *stream) {
    file_userdata_t *data = stream->userdata;
    off_t base = data->offset;

    if (base < 0)
        return base;

    // Adjust for unconsumed buffer data
    return base - (off_t)(data->buf_avail - data->buf_pos);
}


static void file_close(asdf_stream_t *stream) {
    file_userdata_t *data = stream->userdata;

    if (data->should_close)
        fclose(data->file);

    free(data->buf);
    free(data);
    free(stream);
}


static bool file_is_seekable(FILE *file) {
    off_t pos = ftello(file);

    if (pos == -1)
        return false;

    // Try seeking back to the same position to catch edge cases where ftello works
    // but seeking is still not possible.
    if (fseeko(file, pos, SEEK_SET) != 0)
        return false;

    return true;
}


static int file_fy_parser_set_input(asdf_stream_t *stream, struct fy_parser *fyp) {
    file_userdata_t *data = stream->userdata;

    // This should only be used if the file is seekable
    assert(file_is_seekable(data->file));

    // If some data is already in the buffer we may need to seek backwards
    // so the data we already buffered locally will be available to fyaml
    fseeko(data->file, -data->buf_avail + data->buf_pos, SEEK_CUR);
    return fy_parser_set_input_fp(fyp, data->filename, data->file);
}


asdf_stream_t *asdf_stream_from_fp(FILE* file, const char *filename) {
    if (!file)
        return NULL;

    file_userdata_t *data = calloc(1, sizeof(file_userdata_t));

    if (!data) {
        return NULL;
    }

    data->file = file;
    data->filename = filename;
    data->buf_size = BUFSIZ;  // hard-coded for now, could make tuneable later
    data->buf = malloc(data->buf_size);

    if (!data->buf) {
        free(data);
        return NULL;
    }

    asdf_stream_t *stream = malloc(sizeof(asdf_stream_t));

    if (!stream) { 
        free(data->buf);
        free(data);
        return NULL;
    }

    stream->error = ASDF_STREAM_OK;
    stream->is_seekable = file_is_seekable(file);
    stream->userdata = data;
    stream->next = file_next;
    stream->consume = file_consume;
    stream->readline = file_readline;
    stream->scan = file_scan;
    stream->seek = file_seek;
    stream->tell = file_tell;
    stream->close = file_close;
    stream->fy_parser_set_input = file_fy_parser_set_input;
    asdf_stream_set_capture(stream, NULL, NULL, 0);

#if DEBUG
    stream->last_next_size = 0;
    stream->last_next_ptr = NULL;
    stream->unconsumed_next_count = 0;
#endif

    return stream;
}


asdf_stream_t *asdf_stream_from_file(const char *filename) {
    FILE *file = fopen(filename, "rb");

    if (!file)
        return NULL;

    asdf_stream_t *stream = asdf_stream_from_fp(file, filename);

    if (!stream) {
        fclose(file);
        return NULL;
    }

    file_userdata_t *data = stream->userdata;
    data->should_close = true;
    return stream;
}


/**
 * Memory-backed read handling
 */
static const uint8_t *mem_next(asdf_stream_t *stream, size_t *avail) {
    assert(stream);
    assert(avail);
    mem_userdata_t *data = stream->userdata;
    if (data->pos >= data->size) {
        *avail = 0;
        return NULL;
    }

    *avail = data->size - data->pos;
    return data->buf + data->pos;
}


static void mem_consume(asdf_stream_t *stream, size_t n) {
    mem_userdata_t *data = stream->userdata;
    stream_capture(stream, data->buf + data->pos, n);
    data->pos += n;
}


static const char *mem_readline(asdf_stream_t *stream, size_t *len) {
    mem_userdata_t *data = stream->userdata;
    const uint8_t *buf = data->buf + data->pos;
    size_t remaining = data->size - data->pos;

    for (size_t idx = 0; idx < remaining; idx++) {
        if (buf[idx] != '\n')
            continue;

        data->pos += idx + 1;
        *len = idx + 1;
        return (const char *)buf;
    }

    if (remaining > 0) {
        *len = remaining;
        data->pos = data->size;
        return (const char *)buf;
    }

    *len = 0;
    return NULL;
}


static int mem_scan(struct asdf_stream *stream, const uint8_t **tokens, const size_t *token_lens,
                    size_t n_tokens, size_t *match_offset, size_t *match_token_idx) {
    // The mem_scan case is simple; we only need to wrap asdf_scan_tokens
    mem_userdata_t *data = stream->userdata;
    size_t offset = 0;
    size_t token_idx = 0;
    size_t avail = data->size - data->pos;
    int res = asdf_scan_tokens(data->buf + data->pos, avail, tokens, token_lens, n_tokens,
                               &offset, &token_idx);

    if (0 == res) {
        if (match_offset)
            *match_offset = offset;

        if (match_token_idx)
            *match_token_idx = token_idx;

        mem_consume(stream, offset);
    } else {
        // Scan exhausted the buffer without finding anything; the full buffer is thus consumed
        mem_consume(stream, avail);
    }

    return res;
}


static int mem_seek(asdf_stream_t *stream, off_t offset, int whence) {
    mem_userdata_t *data = stream->userdata;
    size_t new_pos;

    switch (whence) {
    case SEEK_SET:
        new_pos = offset;
        break;
    case SEEK_CUR:
        new_pos = data->pos + offset;
        break;
    case SEEK_END:
        new_pos = data->size + offset;
        break;
    default:
        return -1;
    }

    if (new_pos > data->size)
        return -1;

    data->pos = new_pos;
    return 0;
}


static off_t mem_tell(asdf_stream_t *stream) {
    mem_userdata_t *data = stream->userdata;
    return (off_t)data->pos;
}


static void mem_close(asdf_stream_t *stream) {
    free(stream->userdata);
    free(stream);
}


static int mem_fy_parser_set_input(asdf_stream_t *stream, struct fy_parser *fyp) {
    mem_userdata_t *data = stream->userdata;
    return fy_parser_set_string(fyp, (const char *)data->buf + data->pos, data->size);
}


// TODO: mmap opener
asdf_stream_t *asdf_stream_from_memory(const void *buf, size_t size) {
    mem_userdata_t *data = malloc(sizeof(mem_userdata_t));

    if (!data) {
        return NULL;
    }

    data->buf = buf;
    data->size = size;
    data->pos = 0;

    asdf_stream_t *stream = malloc(sizeof(asdf_stream_t));

    if (!stream) {
        free(data);
        return NULL;
    }

    stream->error = ASDF_STREAM_OK;
    stream->is_seekable = true;
    stream->userdata = data;
    stream->next = mem_next;
    stream->consume = mem_consume;
    stream->readline = mem_readline;
    stream->scan = mem_scan;
    stream->seek = mem_seek;
    stream->tell = mem_tell;
    stream->close = mem_close;
    stream->fy_parser_set_input = mem_fy_parser_set_input;
    asdf_stream_set_capture(stream, NULL, NULL, 0);

#if DEBUG
    stream->last_next_size = 0;
    stream->last_next_ptr = NULL;
    stream->unconsumed_next_count = 0;
#endif

    return stream;
}
