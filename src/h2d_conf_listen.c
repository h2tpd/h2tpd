#include "h2d_main.h"

void h2d_conf_listen_stats(struct h2d_conf_listen *conf_listen, wuy_json_ctx_t *json)
{
	struct h2d_conf_listen_stats *stats = conf_listen->stats;
	wuy_json_object_int(json, "fail_no_host", atomic_load(&stats->fail_no_host));
}

static int h2d_conf_listen_name(void *data, char *buf, int size)
{
	struct h2d_conf_listen *conf_listen = data;
	return snprintf(buf, size, "Listen(%s)>", conf_listen->addresses[0]);
}

bool h2d_conf_host_register(struct h2d_conf_listen *conf_listen);

static bool h2d_conf_listen_post(void *data)
{
	struct h2d_conf_listen *conf_listen = data;

	if (conf_listen->hosts == NULL) {
		if (conf_listen->default_host->default_path->content == NULL) {
			printf("error: No host defined in listen\n");
			return false;
		}
		return true;
	}

	if (!h2d_conf_host_register(conf_listen)) {
		return false;
	}

	bool is_ssl = false, is_plain = false;
	struct h2d_conf_host *conf_host;
	for (int i = 0; (conf_host = conf_listen->hosts[i]) != NULL; i++) {
		if (conf_host->ssl->ctx != NULL) {
			is_ssl = true;
		} else {
			is_plain = true;
		}
	}

	/* ssl */
	if (is_ssl) {
		if (is_plain) {
			printf("plain vs ssl\n");
			return false;
		}

		if (conf_listen->default_host->ssl->ctx != NULL) {
			conf_listen->ssl_ctx = conf_listen->default_host->ssl->ctx;
		} else if (conf_listen->host_wildcard != NULL) {
			conf_listen->ssl_ctx = conf_listen->host_wildcard->ssl->ctx;
		} else {
			conf_listen->ssl_ctx = h2d_ssl_ctx_empty_server();
		}
	}

	if (conf_listen->name == NULL) {
		conf_listen->name = conf_listen->addresses[0];
	}

	conf_listen->stats = wuy_shmem_alloc(sizeof(struct h2d_conf_listen_stats));

	return true;
}

static struct wuy_cflua_command h2d_conf_listen_http2_commands[] = {
	{	.name = "idle_timeout",
		.type = WUY_CFLUA_TYPE_INTEGER,
		.offset = offsetof(struct h2d_conf_listen, http2.idle_timeout),
		.default_value.n = 5 * 60,
		.limits.n = WUY_CFLUA_LIMITS_POSITIVE,
	},
	{	.name = "ping_interval",
		.type = WUY_CFLUA_TYPE_INTEGER,
		.offset = offsetof(struct h2d_conf_listen, http2.ping_interval),
		.default_value.n = 60,
		.limits.n = WUY_CFLUA_LIMITS_POSITIVE,
	},
	{ NULL }
};

static struct wuy_cflua_command h2d_conf_listen_http1_commands[] = {
	{	.name = "keepalive_timeout",
		.type = WUY_CFLUA_TYPE_INTEGER,
		.offset = offsetof(struct h2d_conf_listen, http1.keepalive_timeout),
		.default_value.n = 60,
		.limits.n = WUY_CFLUA_LIMITS_POSITIVE,
	},
	{ NULL }
};

static struct wuy_cflua_command h2d_conf_listen_network_commands[] = {
	{	.name = "connections",
		.type = WUY_CFLUA_TYPE_INTEGER,
		.offset = offsetof(struct h2d_conf_listen, network.connections),
		.limits.n = WUY_CFLUA_LIMITS_NON_NEGATIVE,
	},
	{	.name = "recv_timeout",
		.type = WUY_CFLUA_TYPE_INTEGER,
		.offset = offsetof(struct h2d_conf_listen, network.recv_timeout),
		.default_value.n = 10,
		.limits.n = WUY_CFLUA_LIMITS_POSITIVE,
	},
	{	.name = "send_timeout",
		.type = WUY_CFLUA_TYPE_INTEGER,
		.offset = offsetof(struct h2d_conf_listen, network.send_timeout),
		.default_value.n = 10,
		.limits.n = WUY_CFLUA_LIMITS_POSITIVE,
	},
	{	.name = "send_buffer_size",
		.type = WUY_CFLUA_TYPE_INTEGER,
		.offset = offsetof(struct h2d_conf_listen, network.send_buffer_size),
		.default_value.n = 16 * 1024,
		.limits.n = WUY_CFLUA_LIMITS_LOWER(4 * 1024),
	},
	{ NULL }
};

static struct wuy_cflua_command h2d_conf_listen_commands[] = {
	{	.name = "_addresses",
		.type = WUY_CFLUA_TYPE_TABLE,
		.offset = offsetof(struct h2d_conf_listen, addresses),
		.u.table = WUY_CFLUA_ARRAY_STRING_TABLE,
	},
	{	.type = WUY_CFLUA_TYPE_TABLE,
		.offset = offsetof(struct h2d_conf_listen, hosts),
		.u.table = &h2d_conf_host_table,
		.is_extra_commands = true,
	},
	{	.name = "_default_next",
		.type = WUY_CFLUA_TYPE_TABLE,
		.offset = offsetof(struct h2d_conf_listen, default_host),
		.u.table = &h2d_conf_host_table,
	},
	{	.name = "name",
		.type = WUY_CFLUA_TYPE_STRING,
		.offset = offsetof(struct h2d_conf_listen, name),
	},
	{	.name = "http1",
		.type = WUY_CFLUA_TYPE_TABLE,
		.u.table = &(struct wuy_cflua_table) { h2d_conf_listen_http1_commands },
	},
	{	.name = "http2",
		.type = WUY_CFLUA_TYPE_TABLE,
		.u.table = &(struct wuy_cflua_table) { h2d_conf_listen_http2_commands },
	},
	{	.name = "network",
		.type = WUY_CFLUA_TYPE_TABLE,
		.u.table = &(struct wuy_cflua_table) { h2d_conf_listen_network_commands },
	},
	{	.type = WUY_CFLUA_TYPE_END,
		.u.next = h2d_module_next_listen_command,
	},
};

struct wuy_cflua_table h2d_conf_listen_table = {
	.commands = h2d_conf_listen_commands,
	.size = sizeof(struct h2d_conf_listen),
	.post = h2d_conf_listen_post,
	.name = h2d_conf_listen_name,
};
