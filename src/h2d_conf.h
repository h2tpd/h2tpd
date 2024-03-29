#ifndef H2D_CONF_H
#define H2D_CONF_H

#include <stdbool.h>
#include <lua5.1/lua.h>

#include "h2d_module.h"
#include "h2d_dynamic.h"

struct h2d_conf_path_stats {
	atomic_long	total;
	atomic_long	done;

	atomic_long	lua_new;
	atomic_long	lua_again;
	atomic_long	lua_error;
	atomic_long	lua_done;
	atomic_long	lua_free;

	atomic_long	req_acc_ms;
	atomic_long	react_acc_ms;
	atomic_long	resp_acc_ms;
	atomic_long	total_acc_ms;

#define X(s, _) atomic_long status_##s;
	WUY_HTTP_STATUS_CODE_TABLE
#undef X
	atomic_long	status_others;
};

struct h2d_conf_host_stats {
	atomic_long	fail_no_path;
};

struct h2d_conf_listen_stats {
	atomic_long	fail_no_host;
	atomic_long	connections;
	atomic_long	total;
};

struct h2d_conf_access_log {
	const char		*filename;
	double			sampling_rate;
	bool			replace_format;
	bool			enable_subrequest;
	wuy_cflua_function_t	format;
	wuy_cflua_function_t	filter;
	int			buf_size;
	int			max_line;

	struct h2d_log_file	*file;
};

struct h2d_conf_path {
	const char		*name;

	char			**pathnames;

	struct h2d_dynamic_conf	dynamic;

	int			req_body_max;
	bool			req_body_sync;

	bool			(*req_hook)(void);

	struct h2d_log		*error_log;

	struct h2d_conf_access_log	*access_log;

	struct h2d_module		*content;
	struct h2d_module_filters	*filters;

	void				*module_confs[H2D_MODULE_MAX];
	int				content_inherit_counts[H2D_MODULE_MAX];

	struct h2d_conf_path_stats	*stats;
};

struct h2d_conf_host {
	const char		*name;

	char			**hostnames;

	struct h2d_conf_path	**paths;
	struct h2d_conf_path	*default_path;

	struct h2d_ssl_conf	*ssl;

	void			*module_confs[H2D_MODULE_MAX];

	struct h2d_conf_host_stats	*stats;
};

struct h2d_conf_listen {
	const char		*name;

	char			**addresses;
	int			address_num;
	int			*fds;
	int			*reuse_magics;

	struct h2d_conf_host	**hosts;
	struct h2d_conf_host	*default_host;

	wuy_dict_t		*host_dict;
	struct h2d_conf_host	*host_wildcard;
	bool			any_prefix_hostname;
	bool			any_subfix_hostname;

	struct {
		int		idle_timeout;
		int		idle_min_timeout;
		int		ping_interval;

		struct h2d_log	*log;

		struct http2_settings	settings;

		loop_group_timer_head_t	*idle_timer_group;
	} http2;

	struct {
		int		keepalive_timeout;
		int		keepalive_min_timeout;

		struct h2d_log	*log;

		loop_group_timer_head_t	*keepalive_timer_group;
	} http1;

	struct {
		int		connections;
		int		send_timeout;
		int		recv_timeout;
		int		recv_buffer_size;
		int		send_buffer_size;
		int		defer_accept;
		int		backlog;
		bool		reuse_port;

		loop_group_timer_head_t	*send_timer_group;
		loop_group_timer_head_t	*recv_timer_group;
	} network;

	void			*module_confs[H2D_MODULE_MAX];

	struct h2d_conf_listen_stats	*stats;
};

struct h2d_conf_runtime {
	const char		*pid;

	struct h2d_conf_runtime_worker {
		int	num;
	} worker;

	struct h2d_conf_runtime_resolver {
		const char *	ai_family_str;
		int		ai_family;
	} resolver;

	struct h2d_log		*error_log;

	struct h2d_module_dynamic *dynamic_modules;
	struct h2d_module_dynamic *dynamic_upstream_modules;
};

extern lua_State *h2d_L;

extern struct h2d_conf_runtime *h2d_conf_runtime;
extern struct h2d_conf_listen **h2d_conf_listens;

extern int h2d_conf_reload_count;

bool h2d_conf_parse(const char *conf_file);

struct h2d_conf_host *h2d_conf_host_locate(struct h2d_conf_listen *conf_listen,
		const char *name);

struct h2d_conf_path *h2d_conf_path_locate(struct h2d_conf_host *conf_host,
		const char *name);

void h2d_conf_listen_init_worker(void);

void h2d_conf_path_stats(struct h2d_conf_path *conf_path, wuy_json_t *json);
void h2d_conf_host_stats(struct h2d_conf_host *conf_host, wuy_json_t *json);
void h2d_conf_listen_stats(struct h2d_conf_listen *conf_listen, wuy_json_t *json);

void h2d_conf_doc(void);

#define h2d_conf_log(level, fmt, ...) \
	if (h2d_conf_runtime == NULL) \
		fprintf(level < H2D_LOG_ERROR ? stdout : stderr, fmt"\n", ##__VA_ARGS__); \
	else \
		h2d_log_level(h2d_conf_runtime->error_log, level, fmt, ##__VA_ARGS__)

#define h2d_conf_log_at(_log, level, fmt, ...) \
	if (_log != NULL) { \
		h2d_log_level(_log, level, fmt, ##__VA_ARGS__); \
	} else \
		h2d_log_level(h2d_conf_runtime->error_log, level, fmt, ##__VA_ARGS__)

/* internal */
extern struct wuy_cflua_table h2d_conf_listen_table;
extern struct wuy_cflua_table h2d_conf_host_table;
extern struct wuy_cflua_table h2d_conf_path_table;
extern struct wuy_cflua_table h2d_conf_runtime_table;

#endif
