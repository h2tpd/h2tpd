#include "h2d_main.h"

#include <arpa/inet.h>

// TODO support ipv6

struct h2d_acl_rule {
	uint32_t	start;
	uint32_t	end;
	bool		is_deny;
};
struct h2d_acl_conf {
	char			**strs;
	int			num;
	struct h2d_acl_rule	*rules;
};

struct h2d_module h2d_acl_module;

static int h2d_acl_process_headers(struct h2d_request *r)
{
	struct h2d_acl_conf *conf = r->conf_path->module_confs[h2d_acl_module.index];
	if (conf->strs == NULL) {
		return H2D_OK;
	}

	struct sockaddr_in *sin = (struct sockaddr_in *)&r->c->client_addr;
	uint32_t addr = ntohl(sin->sin_addr.s_addr);
	for (int i = 0; i < conf->num; i++) {
		struct h2d_acl_rule *rule = &conf->rules[i];
		if (addr >= rule->start && addr <= rule->end) {
			if (rule->is_deny) {
				printf("[info] denied by ACL rule #%d\n", i+1);
				return WUY_HTTP_403;
			}
			return H2D_OK;
		}
	}
	if (!conf->rules[conf->num-1].is_deny) {
		printf("[info] denied by ACL default rule\n");
		return WUY_HTTP_403;
	}
	return H2D_OK;
}

/* configuration */

static const char *h2d_acl_conf_post(void *data)
{
	struct h2d_acl_conf *conf = data;

	if (conf->strs == NULL) {
		return WUY_CFLUA_OK;
	}

	conf->rules = wuy_pool_alloc(wuy_cflua_pool, conf->num * sizeof(struct h2d_acl_rule));

	for (int i = 0; i < conf->num; i++) {
		char *str = conf->strs[i];
		struct h2d_acl_rule *rule = &conf->rules[i];

		if (str[0] == '!') {
			rule->is_deny = true;
			str++;
		} else {
			rule->is_deny = false;
		}

		int mask = -1;
		char *p = strchr(str, '/');
		if (p != NULL) {
			*p++ = '\0';
			errno = 0;
			char *endp;
			mask = strtol(p, &endp, 10);
			if (errno != 0 || *endp != '\0') {
				wuy_cflua_post_arg = str;
				return "invalid mask";
			}
			if (mask < 0 || mask > 32) {
				wuy_cflua_post_arg = str;
				return "invalid mask";
			}
		}

		struct in_addr ip;
		if (!inet_pton(AF_INET, str, &ip)) {
			wuy_cflua_post_arg = str;
			return "invalid IP";
		}

		rule->start = ntohl(ip.s_addr);

		if (mask == -1) {
			rule->end = rule->start;
		} else {
			rule->end = rule->start & (UINT32_MAX << (32-mask));
		}
	}

	return WUY_CFLUA_OK;
}

static struct wuy_cflua_command h2d_acl_conf_commands[] = {
	{	.type = WUY_CFLUA_TYPE_STRING,
		.description = "Rule list. " \
				"Deny-rules begin with '!', e.g \"!123.234.0.0/24\". " \
				"The default policy is the negative of the last rule.",
		.offset = offsetof(struct h2d_acl_conf, strs),
		.array_number_offset = offsetof(struct h2d_acl_conf, num),
	},
	{ NULL }
};

struct h2d_module h2d_acl_module = {
	.name = "acl",
	.command_path = {
		.name = "acl",
		.description = "Access control list (ACL) filter module.",
		.type = WUY_CFLUA_TYPE_TABLE,
		.u.table = &(struct wuy_cflua_table) {
			.commands = h2d_acl_conf_commands,
			.size = sizeof(struct h2d_acl_conf),
			.post = h2d_acl_conf_post,
		}
	},

	.filters = {
		.process_headers = h2d_acl_process_headers,
	},
};
