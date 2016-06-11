#ifndef _util_cirbuf__hh
#define _util_cirbuf__hh

struct tea_cirbuf_t;

#ifdef __cplusplus
extern "C" {
#endif // c++

struct tea_cirbuf_t *util_cbuf_create (size_t size);

void util_cbuf_release (struct tea_cirbuf_t *buf);

void util_cbuf_reset(struct tea_cirbuf_t *buf);

size_t util_cbuf_space (struct tea_cirbuf_t *buf);

size_t util_cbuf_data (struct tea_cirbuf_t *buf);
size_t util_cbuf_contiguous_data (struct tea_cirbuf_t *buf);

void *util_cbuf_get_contiguous_data (struct tea_cirbuf_t *buf);
size_t util_cbuf_get_cdata (struct tea_cirbuf_t *buf, void **data);

void util_cbuf_save (struct tea_cirbuf_t *buf, const void *data, size_t len);

void util_cbuf_load(struct tea_cirbuf_t *cbuf, void *buf, size_t len);

void util_cbuf_consume (struct tea_cirbuf_t *buf, size_t cnt);

#ifdef __cplusplus
}
#endif // c++

#endif // util_cirbuf.h
