/*
 * stu_websocket_filter.c
 *
 *  Created on: 2017年11月29日
 *      Author: Tony Lau
 */

#include "stu_websocket.h"

static stu_int32_t         stu_websocket_filter_static_handler(stu_websocket_request_t *r);

stu_hash_t                 stu_websocket_filter_hash;

static stu_websocket_filter_t  websocket_filter[] = {
	{ stu_string("/"), stu_websocket_filter_static_handler },
	{ stu_null_string, NULL }
};


stu_int32_t
stu_websocket_filter_init_hash() {
	stu_websocket_filter_t *f;

	if (stu_hash_init(&stu_websocket_filter_hash, STU_WEBSOCKET_FILTER_MAX_RECORDS, NULL, STU_HASH_FLAGS_LOWCASE|STU_HASH_FLAGS_REPLACE) == STU_ERROR) {
		return STU_ERROR;
	}

	for (f = websocket_filter; f->handler; f++) {
		if (stu_websocket_filter_add(&f->pattern, f->handler) == STU_ERROR) {
			return STU_ERROR;
		}
	}

	return STU_OK;
}

stu_int32_t
stu_websocket_filter_add(stu_str_t *pattern, stu_websocket_filter_handler_pt handler) {
	stu_list_t             *list;
	stu_list_elt_t         *e;
	stu_websocket_filter_t *f;
	stu_uint32_t            hk;
	stu_int32_t             rc;

	rc = STU_ERROR;

	stu_mutex_lock(&stu_websocket_filter_hash.lock);

	hk = stu_hash_key(pattern->data, pattern->len, stu_websocket_filter_hash.flags);

	list = stu_hash_find_locked(&stu_websocket_filter_hash, hk, pattern->data, pattern->len);
	if (list == NULL) {
		list = stu_calloc(sizeof(stu_list_t));
		if (list == NULL) {
			goto failed;
		}

		stu_list_init(list, (stu_list_hooks_t *) &stu_websocket_filter_hash.hooks);

		if (stu_hash_insert_locked(&stu_websocket_filter_hash, pattern, list) == STU_ERROR) {
			goto failed;
		}
	}

	f = stu_calloc(sizeof(stu_websocket_filter_t));
	if (f == NULL) {
		goto failed;
	}

	f->pattern = *pattern;
	f->handler = handler;

	stu_mutex_lock(&list->lock);
	e = stu_list_insert_tail(list, f);
	stu_mutex_unlock(&list->lock);

	if (e) {
		rc = STU_OK;
	}

failed:

	stu_mutex_unlock(&stu_websocket_filter_hash.lock);

	return rc;
}

stu_int32_t
stu_websocket_filter_del(stu_str_t *pattern, stu_websocket_filter_handler_pt handler) {
	stu_list_t     *list;
	stu_list_elt_t *elts, *e;
	stu_queue_t    *q;
	stu_uint32_t    hk;

	stu_mutex_lock(&stu_websocket_filter_hash.lock);

	hk = stu_hash_key(pattern->data, pattern->len, stu_websocket_filter_hash.flags);

	if (handler == NULL) {
		stu_hash_remove(&stu_websocket_filter_hash, hk, pattern->data, pattern->len);
		goto done;
	}

	list = stu_hash_find(&stu_websocket_filter_hash, hk, pattern->data, pattern->len);
	if (list == NULL) {
		goto done;
	}

	stu_mutex_lock(&list->lock);

	elts = &list->elts;

	for (q = stu_queue_head(&elts->queue); q != stu_queue_sentinel(&elts->queue); q = stu_queue_next(q)) {
		e = stu_queue_data(q, stu_list_elt_t, queue);
		if (e->value == handler) {
			stu_list_remove(list, e);
			break;
		}
	}

	stu_mutex_unlock(&list->lock);

done:

	stu_mutex_unlock(&stu_websocket_filter_hash.lock);

	return STU_OK;
}


stu_int32_t
stu_websocket_filter_upgrade_handler(stu_http_request_t *r) {
	if (stu_websocket_is_upgrade_request(r) == TRUE) {
		r->headers_out.status = STU_DECLINED;
		return STU_OK;
	}

	return STU_ERROR;
}

static stu_int32_t
stu_websocket_filter_static_handler(stu_websocket_request_t *r) {
	r->status = STU_ERROR;
	return STU_DECLINED;
}
