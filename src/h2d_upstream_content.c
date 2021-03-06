#include "h2d_main.h"

static struct h2d_upstream_conf *h2d_upstream_content_conf(struct h2d_request *r)
{
	/* `struct h2d_upstream_conf *` must be at the top of content module's conf */
	struct h2d_upstream_conf **p = r->conf_path->module_confs[r->conf_path->content->index];
	return *p;
}
static struct h2d_upstream_content_ctx *h2d_upstream_content_ctx(struct h2d_request *r)
{
	/* `struct h2d_upstream_content_ctx` must be at the top of content module's ctx */
	return r->module_ctxs[r->conf_path->content->index];
}

static bool h2d_upstream_content_status_code_retry(struct h2d_request *r)
{
	struct h2d_upstream_conf *upstream = h2d_upstream_content_conf(r);
	if (upstream->retry_status_codes == NULL) {
		return false;
	}
	for (int *p = upstream->retry_status_codes; *p != 0; p++) {
		if (r->resp.status_code == *p) {
			h2d_request_log_at(r, upstream->log, H2D_LOG_DEBUG,
					"retry for status_code=%d", r->resp.status_code);
			return true;
		}
	}
	return false;
}

static int h2d_upstream_content_fail(struct h2d_request *r);
int h2d_upstream_content_generate_response_headers(struct h2d_request *r)
{
	struct h2d_upstream_conf *upstream = h2d_upstream_content_conf(r);
	struct h2d_upstream_content_ctx *ctx = h2d_upstream_content_ctx(r);
	struct h2d_upstream_ops *ops = upstream->ops;

	if (ctx == NULL) {
		ctx = wuy_pool_alloc(r->pool, sizeof(struct h2d_upstream_content_ctx));
		r->module_ctxs[r->conf_path->content->index] = ctx;

		int ret = ops->build_request(r);
		if (ret != H2D_OK) {
			return ret;
		}
	}

	if (ctx->upc == NULL || ctx->upc == H2D_PTR_AGAIN) {
		ctx->upc = h2d_upstream_get_connection(upstream, r);
		if (!H2D_PTR_IS_OK(ctx->upc)) {
			return H2D_PTR2RET(ctx->upc);
		}
	}

	if (!ctx->has_sent_request) {
		if (h2d_upstream_connection_write_blocked(ctx->upc)) {// remove this check if h2d_upstream_connection_write() can return H2D_AGAIN
			return H2D_AGAIN;
		}
		int ret = h2d_upstream_connection_write(ctx->upc, ctx->req_buf, ctx->req_len);
		if (ret == H2D_AGAIN) {
			return H2D_AGAIN;
		}
		if (ret == H2D_ERROR) {
			return h2d_upstream_content_fail(r);
		}
		ctx->has_sent_request = true;
	}

	char buffer[4096];
	int read_len = h2d_upstream_connection_read(ctx->upc, buffer, sizeof(buffer));
	if (read_len == H2D_AGAIN) {
		return H2D_AGAIN;
	}
	if (read_len == H2D_ERROR) {
		return h2d_upstream_content_fail(r);
	}

	int proc_len = 0;
	if (ops->parse_response_headers == NULL) {
		r->resp.status_code = WUY_HTTP_200;
		goto no_headers;
	}

	bool is_done;
	proc_len = ops->parse_response_headers(r, buffer, read_len, &is_done);
	if (proc_len < 0) {
		return h2d_upstream_content_fail(r);
	}
	if (!is_done) {
		// TODO read again
		printf("too long response header\n");
		return H2D_ERROR;
	}

	if (h2d_upstream_content_status_code_retry(r)) {
		return h2d_upstream_content_fail(r);
	}
no_headers:

	h2d_upstream_connection_read_notfinish(ctx->upc, buffer + proc_len, read_len - proc_len);
	return H2D_OK;
}

static int h2d_upstream_content_fail(struct h2d_request *r)
{
	struct h2d_upstream_conf *upstream = h2d_upstream_content_conf(r);
	struct h2d_upstream_content_ctx *ctx = h2d_upstream_content_ctx(r);

	ctx->upc->error = true;

	/* retry */
	if (ctx->retries < 0 || ctx->retries++ >= upstream->max_retries) {
		h2d_request_log_at(r, upstream->log, H2D_LOG_INFO, "no retry %d", ctx->retries);
		return r->resp.status_code != 0 ? H2D_OK : WUY_HTTP_502;
	}

	h2d_request_reset_response(r);

	ctx->upc = h2d_upstream_retry_connection(ctx->upc);
	if (ctx->upc == H2D_PTR_ERROR) {
		return WUY_HTTP_500;
	}
	ctx->has_sent_request = false;

	return h2d_upstream_content_generate_response_headers(r);
}

int h2d_upstream_content_generate_response_body(struct h2d_request *r,
		uint8_t *buffer, int buf_len)
{
	struct h2d_upstream_conf *upstream = h2d_upstream_content_conf(r);
	struct h2d_upstream_content_ctx *ctx = h2d_upstream_content_ctx(r);
	struct h2d_upstream_ops *ops = upstream->ops;

	if (ops->is_response_body_done && ops->is_response_body_done(r)) {
		return 0;
	}

	int read_len = h2d_upstream_connection_read(ctx->upc, buffer, buf_len);
	if (read_len < 0) {
		return read_len;
	}

	if (ops->build_response_body != NULL) {
		read_len = ops->build_response_body(r, buffer, read_len, buf_len);
	}

	return read_len;
}

void h2d_upstream_content_ctx_free(struct h2d_request *r)
{
	struct h2d_upstream_content_ctx *ctx = h2d_upstream_content_ctx(r);
	if (H2D_PTR_IS_OK(ctx->upc)) {
		h2d_upstream_release_connection(ctx->upc);
	}
}

const char *h2d_upstream_content_set_ops(struct h2d_upstream_conf *conf,
		struct h2d_upstream_ops *ops)
{
	if (conf == NULL) {
		return WUY_CFLUA_OK;
	}
	if (conf->ops != NULL) {
		if (conf->ops != ops) {
			return "different ops for one upstream";
		}
		return WUY_CFLUA_OK;
	}
	conf->ops = ops;
	return WUY_CFLUA_OK;
}
