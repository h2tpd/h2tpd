#include "h2d_main.h"

struct h2d_auth_request_conf {
	const char	*pathname;
};

struct h2d_module h2d_auth_request_module;

static int h2d_auth_request_process_headers(struct h2d_request *r)
{
	struct h2d_auth_request_conf *conf = r->conf_path->module_confs[h2d_auth_request_module.index];
	if (conf->pathname == NULL) {
		return H2D_OK;
	}

	struct h2d_request *subr = r->module_ctxs[h2d_auth_request_module.index];
	if (subr == NULL) { /* first time get in */
		subr = h2d_request_subr_new(r, conf->pathname);
		h2d_header_dup_list(&subr->req.headers, &r->req.headers, r->pool);
		// TODO req-body
		// subr->req.method = r->req.method;

		r->module_ctxs[h2d_auth_request_module.index] = subr;
		return H2D_AGAIN;
	}

	struct h2d_header *h;
	switch (subr->resp.status_code) {
	case 0:
		return H2D_AGAIN;
	case WUY_HTTP_200:
		h2d_request_subr_close(subr);
		r->module_ctxs[h2d_auth_request_module.index] = NULL;
		return H2D_OK;
	case WUY_HTTP_401:
		h2d_header_iter(&subr->resp.headers, h) {
			if (strcasecmp(h->str, "WWW-Authenticate") == 0) {
				h2d_header_add(&r->resp.headers, "WWW-Authenticate", 16,
						h2d_header_value(h), h->value_len, r->pool);
				break;
			}
		}
		return WUY_HTTP_401;
	case WUY_HTTP_403:
		return WUY_HTTP_403;
	default:
		return H2D_ERROR;
	}
}

/* configuration */

static const char *h2d_auth_request_conf_post(void *data)
{
	struct h2d_auth_request_conf *conf = data;
	if (conf->pathname == NULL) {
		return WUY_CFLUA_OK;
	}

	char first = conf->pathname[0];
	if (first != '/') {
		return "invalid pathname";
	}

	return WUY_CFLUA_OK;
}

static struct wuy_cflua_command h2d_auth_request_conf_commands[] = {
	{	.type = WUY_CFLUA_TYPE_STRING,
		.is_single_array = true,
		.description = "Path name",
		.offset = offsetof(struct h2d_auth_request_conf, pathname),
	},
	{ NULL }
};

struct h2d_module h2d_auth_request_module = {
	.name = "auth_request",
	.command_path = {
		.name = "auth_request",
		.description = "Subrequest authentication filter module.",
		.type = WUY_CFLUA_TYPE_TABLE,
		.u.table = &(struct wuy_cflua_table) {
			.commands = h2d_auth_request_conf_commands,
			.size = sizeof(struct h2d_auth_request_conf),
			.post = h2d_auth_request_conf_post,
		}
	},

	.filters = {
		.process_headers = h2d_auth_request_process_headers,
	},
};
