#ifndef H2D_RESOLVER_H
#define H2D_RESOLVER_H

struct h2d_resolver_query {
	int	expire_after;
	char	hostname[4096];
};

void h2d_resolver_init(void);
void h2d_resolver_init_if_fork(void);

int h2d_resolver_connect(void);

uint8_t *h2d_resolver_hostname(const char *hostname, int *plen);

extern struct wuy_cflua_table h2d_conf_runtime_resolver_table;

#endif
