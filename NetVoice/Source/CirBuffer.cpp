#include "stdafx.h"
#include "stdlib.h"
#include "assert.h"
#include "CirBuffer.h"

#define CNT_DATA(write, read, size) (((write)-(read)) & ((size)-1))
#define CNT_SPACE(write, read, size) CNT_DATA((read), ((write)+1), (size))

static size_t DATA_TO_END(size_t write, size_t read, size_t size)
{
	size_t end = size - read;
	size_t n = (write + end) & (size - 1);
	return n < end ? n : end;
}

static size_t SPACE_TO_END(size_t write, size_t read, size_t size)
{
	size_t end = size - 1 - write;
	size_t n = (read + end) & (size - 1);
	return n <= end ? n : end+1;
}

struct Cirbuf
{
	char *mp_buf;
	size_t m_buf_size;
	size_t m_write, m_read;	// 读写指针
};
typedef struct Cirbuf Cirbuf;

struct tea_cirbuf_t *util_cbuf_create (size_t size)
{
	Cirbuf *buf = (Cirbuf*)malloc(sizeof(Cirbuf));
	buf->mp_buf = (char*)malloc(size);
	buf->m_buf_size = size;
	buf->m_read = 0;
	buf->m_write = 0;

	return (struct tea_cirbuf_t*)buf;
}

void util_cbuf_reset(struct tea_cirbuf_t *buf)
{
    Cirbuf *b = (Cirbuf*)buf;
    b->m_read = 0;
    b->m_write = 0;
}

void util_cbuf_release (struct tea_cirbuf_t *buf)
{
	Cirbuf *b = (Cirbuf*)buf;
	free(b->mp_buf);
	free(b);
}

size_t util_cbuf_space (struct tea_cirbuf_t *buf)
{
	Cirbuf *b = (Cirbuf*)buf;
	return CNT_SPACE(b->m_write, b->m_read, b->m_buf_size);
}

size_t util_cbuf_data (struct tea_cirbuf_t *buf)
{
	Cirbuf *b = (Cirbuf*)buf;
	return CNT_DATA(b->m_write, b->m_read, b->m_buf_size);
}

size_t util_cbuf_contiguous_data (struct tea_cirbuf_t *buf)
{
	Cirbuf *b = (Cirbuf *)buf;
	return DATA_TO_END(b->m_write, b->m_read, b->m_buf_size);
}

void *util_cbuf_get_contiguous_data (struct tea_cirbuf_t *buf)
{
	Cirbuf *b = (Cirbuf*)buf;
	return b->mp_buf + b->m_read;
}

size_t util_cbuf_get_cdata (struct tea_cirbuf_t *buf, void **data)
{
	Cirbuf *b = (Cirbuf*)buf;
	*data = b->mp_buf + b->m_read;
	return DATA_TO_END(b->m_write, b->m_read, b->m_buf_size);
}

void util_cbuf_save (struct tea_cirbuf_t *buf, const void *data, size_t len)
{
	Cirbuf *b = (Cirbuf*)buf;
	size_t c;
	assert(len <= util_cbuf_space(buf));
	c = SPACE_TO_END(b->m_write, b->m_read, b->m_buf_size);
	if (c >= len) {
		memcpy(b->mp_buf+b->m_write, data, len);
	}
	else {
		char *src = (char*)data+c;
		memcpy(b->mp_buf+b->m_write, data, c);
		memcpy(b->mp_buf, src, (len-c));
	}

	b->m_write += len;
	b->m_write &= (b->m_buf_size-1);
}

void util_cbuf_load(struct tea_cirbuf_t *buf, void *data, size_t len)
{
    Cirbuf *b = (Cirbuf*)buf;
    size_t c;
    assert(len <= util_cbuf_data(buf));
    c = DATA_TO_END(b->m_write, b->m_read, b->m_buf_size);
    if (c >= len) {
        memcpy(data, b->mp_buf + b->m_read, len);
    }
    else {
        char *src = (char*)data + c;
        memcpy(data, b->mp_buf + b->m_read, c);
        memcpy(src, b->mp_buf, (len - c));
    }

    b->m_read += len;
    b->m_read &= (b->m_buf_size - 1);
}

void util_cbuf_consume (struct tea_cirbuf_t *buf, size_t cnt)
{
	Cirbuf *b = (Cirbuf*)buf;
	assert(cnt <= util_cbuf_data(buf));
	b->m_read += cnt;
	b->m_read &= (b->m_buf_size-1);
}
