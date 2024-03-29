#ifndef H2D_REQUEST_H
#define H2D_REQUEST_H

struct h2d_request;

#include "h2d_module.h"
#include "h2d_header.h"
#include "h2d_conf.h"
#include "h2d_connection.h"

#define H2D_CONTENT_LENGTH_INIT		SIZE_MAX

struct h2d_request {
	struct {
		enum wuy_http_method	method;
		int			version;
		size_t			content_length;

		struct {
			const char	*raw;
			const char	*path;
			bool		is_rewrited;

			const char	*path_pos;
			const char	*query_pos;
			int		path_len;
			int		query_len;
		} uri;

		const char		*host;
		wuy_slist_t		headers;

		wuy_http_chunked_t	chunked;

		uint8_t			*body_buf;
		int			body_len;
		bool			body_finished;
	} req;

	struct {
		enum wuy_http_status_code  status_code;
		int			version;
		wuy_slist_t		headers;

		size_t			content_length; /* set by content module, and may be changed by any filter module later */

		size_t			content_original_length; /* original length set by content module */
		size_t			content_generated_length; /* the length of body generated by content module */

		size_t			sent_length; /* only for log and stats */

		const char		*easy_string;
		int			easy_str_len;
		int			easy_fd;
	} resp;

	enum {
		H2D_REQUEST_STATE_RECEIVE_HEADERS = 0,
		H2D_REQUEST_STATE_LOCATE_CONF_HOST,
		H2D_REQUEST_STATE_LOCATE_CONF_PATH,
		H2D_REQUEST_STATE_RECEIVE_BODY_SYNC,
		H2D_REQUEST_STATE_PROCESS_HEADERS,
		H2D_REQUEST_STATE_PROCESS_BODY,
		H2D_REQUEST_STATE_RESPONSE_HEADERS_1,
		H2D_REQUEST_STATE_RESPONSE_HEADERS_2,
		H2D_REQUEST_STATE_RESPONSE_HEADERS_3,
		H2D_REQUEST_STATE_RESPONSE_BODY,
		H2D_REQUEST_STATE_DONE,
	} state;

	uint32_t		id;

	bool			closed;
	bool			is_broken; //TODO may put in h2d_request_run()?

	int			redirects;

	int			filter_indexs[3];
	const struct h2d_module	*filter_terminal;

	long			create_time;
	long			req_end_time;
	long			resp_begin_time;

	const char		*named_path;

	struct h2d_dynamic_ctx	*dynamic_ctx;

	lua_State		*L;
	wuy_cflua_function_t	current_entry;

	struct h2d_request	*father; /* of subrequest */
	wuy_list_t		subr_head;

	wuy_list_node_t		list_node;

	http2_stream_t		*h2s;

	struct h2d_connection	*c;

	struct h2d_conf_host	*conf_host;
	struct h2d_conf_path	*conf_path;

	wuy_pool_t		*pool;

	/* #module_ctxs should be $h2d_module_number.
	 * However it's not known in compiling because of dynamic
	 * modules, so set 0 by now. */
	void 			*module_ctxs[0];
};

struct h2d_request *h2d_request_new(struct h2d_connection *c);
void h2d_request_close(struct h2d_request *r);

bool h2d_request_set_uri(struct h2d_request *r, const char *uri_str, int uri_len);
bool h2d_request_set_host(struct h2d_request *r, const char *host_str, int host_len);
int h2d_request_append_body(struct h2d_request *r, const void *buf, int len);

void h2d_request_reset_response(struct h2d_request *r);

int h2d_request_redirect(struct h2d_request *r, const char *path);

void h2d_request_run(struct h2d_request *r, const char *from);

void h2d_request_init(void);

struct h2d_request *h2d_request_subr_new(struct h2d_request *father, const char *uri);
void h2d_request_subr_detach(struct h2d_request *subr);
void h2d_request_subr_close(struct h2d_request *subr);

int h2d_request_subr_flush_connection(struct h2d_connection *c);

#define h2d_request_do_log(r, log, level, fmt, ...) \
	do { \
		const char *_uri = (level >= H2D_LOG_ERROR) ? r->req.uri.raw : "-"; \
		h2d_log_level(log, level, "%lu:%u %s " fmt, r->c->id, r->id, _uri, ##__VA_ARGS__); \
	} while(0)

#define h2d_request_log(r, level, fmt, ...) \
	h2d_request_do_log(r, r->conf_path->error_log, level, fmt, ##__VA_ARGS__)

#define h2d_request_log_at(r, log, level, fmt, ...) \
	do { \
		struct h2d_log *_log = log ? log : r->conf_path->error_log; \
		h2d_request_do_log(r, _log, level, fmt, ##__VA_ARGS__); \
	} while(0)

#endif
