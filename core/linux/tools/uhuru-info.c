#include "libarmadito-config.h"

#include "utils/getopt.h"
#include "daemon/ipc.h"
#include "net/tcpsockclient.h"
#include "net/unixsockclient.h"
#include "net/netdefaults.h"

#include <assert.h>
#include <errno.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlsave.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PROGRAM_NAME "armadito-info"
#define PROGRAM_VERSION PACKAGE_VERSION

struct info_options {
	enum {
		TCP_SOCKET,
		UNIX_SOCKET,
	} socket_type;
	const char *unix_path;
	unsigned short port_number;
	int output_xml;
};

struct base_info {
	const char *name;
	/* UTC and ISO 8601 date */
	const char *date;
	const char *version;
	unsigned int signature_count;
	const char *full_path;
};

struct module_info {
	const char *name;
	const char *mod_status;
	/* UTC and ISO 8601 date time */
	const char *update_date;
	/* NULL terminated array of pointers to struct base_info */
	struct base_info **base_infos;
};

struct info {
	const char *global_status;
	/* NULL terminated array of pointers to struct a6o_module_info */
	struct module_info **module_infos;
	int n_modules;
	int alloc_modules;
};

static struct opt info_opt_defs[] = {
	{ .long_form = "help", .short_form = 'h', .need_arg = 0, .is_set = 0, .value = NULL},
	{ .long_form = "version", .short_form = 'V', .need_arg = 0, .is_set = 0, .value = NULL},
	{ .long_form = "xml", .short_form = 'x', .need_arg = 0, .is_set = 0, .value = NULL},
	{ .long_form = "tcp", .short_form = 't', .need_arg = 0, .is_set = 0, .value = NULL},
	{ .long_form = "port", .short_form = 'p', .need_arg = 1, .is_set = 0, .value = NULL},
	{ .long_form = "unix", .short_form = 'u', .need_arg = 0, .is_set = 0, .value = NULL},
	{ .long_form = "path", .short_form = 'a', .need_arg = 1, .is_set = 0, .value = NULL},
	{ .long_form = NULL, .short_form = 0, .need_arg = 0, .is_set = 0, .value = NULL},
};

static void version(void)
{
	printf("%s %s\n", PROGRAM_NAME, PROGRAM_VERSION);
	exit(EXIT_SUCCESS);
}

static void usage(void)
{
	fprintf(stderr, "usage: armadito-info [options]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Armadito antivirus information\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  --help  -h                    print help and quit\n");
	fprintf(stderr, "  --version -V                  print program version\n");
	fprintf(stderr, "  --tcp -t | --unix -u          use TCP (--tcp) or unix (--unix) socket (default is " DEFAULT_SOCKET_TYPE ")\n");
	fprintf(stderr, "  --port=PORT | -p PORT         TCP port number (default is " DEFAULT_SOCKET_PORT ")\n");
	fprintf(stderr, "  --path=PATH | -a PATH         unix socket path (default is " DEFAULT_SOCKET_PATH ")\n");
	fprintf(stderr, "  --xml -x                      output information as XML\n");
	fprintf(stderr, "\n");

	exit(1);
}

static void parse_options(int argc, const char *argv[], struct info_options *opts)
{
	int r = opt_parse(info_opt_defs, argc, argv);
	const char *s_port, *s_socket_type;

	if (r < 0|| r > argc)
		usage();

	if (opt_is_set(info_opt_defs, "help"))
		usage();

	if (opt_is_set(info_opt_defs, "version"))
		version();

	if (opt_is_set(info_opt_defs, "tcp") && opt_is_set(info_opt_defs, "unix"))
		usage();

	s_socket_type = DEFAULT_SOCKET_TYPE;
	if (opt_is_set(info_opt_defs, "tcp"))
		s_socket_type = "tcp";
	else if (opt_is_set(info_opt_defs, "unix"))
		s_socket_type = "unix";
	opts->socket_type = (!strcmp(s_socket_type, "unix")) ? UNIX_SOCKET : TCP_SOCKET;

	s_port = opt_value(info_opt_defs, "port", DEFAULT_SOCKET_PORT);
	opts->port_number = (unsigned short)atoi(s_port);

	opts->unix_path = opt_value(info_opt_defs, "path", DEFAULT_SOCKET_PATH);

	opts->output_xml = opt_is_set(info_opt_defs, "xml");
}

static struct info *info_new(void)
{
	struct info *info = malloc(sizeof(struct info));

	info->global_status = NULL;

	info->n_modules = 0;
	info->alloc_modules = 1;
	info->module_infos = malloc(sizeof(struct module_info) * info->alloc_modules);
	info->module_infos[info->n_modules] = NULL;

	return info;
}

static void info_append_module(struct info *info, struct module_info *mod_inf)
{
	if (info->n_modules >= info->alloc_modules - 1) {
		info->alloc_modules *= 2;
		info->module_infos = realloc(info->module_infos, sizeof(struct module_info) * info->alloc_modules);
	}

	info->module_infos[info->n_modules] = mod_inf;
	info->n_modules++;
	info->module_infos[info->n_modules] = NULL;
}

static void info_free(struct info *info)
{
	/* FIXME: free all the fields */
	free(info);
}

static xmlDocPtr info_doc_new(void)
{
	xmlDocPtr doc;
	xmlNodePtr root_node;

	LIBXML_TEST_VERSION;

	doc = xmlNewDoc("1.0");
	root_node = xmlNewNode(NULL, "armadito-info");
#if 0
	xmlNewProp(root_node, "xmlns", "http://www.armadito-am.com/UpdateInfoSchema");
	xmlNewProp(root_node, "xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
	xmlNewProp(root_node, "xsi:schemaLocation", "http://www.armadito-am.com/UpdateInfoSchema UpdateInfoSchema.xsd ");
#endif
	xmlDocSetRootElement(doc, root_node);

	return doc;
}

static const char *update_status_str(const char *status)
{
	if (!strcmp(status, "ARMADITO_UPDATE_OK"))
		return "ok";
	if (!strcmp(status, "ARMADITO_UPDATE_LATE"))
		return "late";
	if (!strcmp(status, "ARMADITO_UPDATE_CRITICAL"))
		return "critical";
	if (!strcmp(status, "ARMADITO_UPDATE_NON_AVAILABLE"))
		return "non available";

	return "non available";
}

static void info_doc_add_module(xmlDocPtr doc, struct module_info *info)
{
	xmlNodePtr root_node, module_node, base_node, date_node;
	struct base_info **pinfo;
	char buffer[64];

	root_node = xmlDocGetRootElement(doc);

	module_node = xmlNewChild(root_node, NULL, "module", NULL);
	xmlNewProp(module_node, "name", info->name);

	xmlNewChild(module_node, NULL, "update-status", info->mod_status);

	date_node = xmlNewChild(module_node, NULL, "update-date", info->update_date);
	xmlNewProp(date_node, "type", "xs:dateTime");

	for(pinfo = info->base_infos; *pinfo != NULL; pinfo++) {
		base_node = xmlNewChild(module_node, NULL, "base", NULL);
		xmlNewProp(base_node, "name", (*pinfo)->name);

		date_node = xmlNewChild(base_node, NULL, "date", (*pinfo)->date);
		xmlNewProp(date_node, "type", "xs:dateTime");

		xmlNewChild(base_node, NULL, "version", (*pinfo)->version);
		sprintf(buffer, "%d", (*pinfo)->signature_count);
		xmlNewChild(base_node, NULL, "signature-count", buffer);
		xmlNewChild(base_node, NULL, "full-path", (*pinfo)->full_path);
	}
}

static void info_doc_add_global(xmlDocPtr doc, const char *global_update_status)
{
	xmlNodePtr root_node = xmlDocGetRootElement(doc);

	xmlNewChild(root_node, NULL, "update-status", global_update_status);
}

static void info_doc_save_to_fd(xmlDocPtr doc, int fd)
{
	xmlSaveCtxtPtr xmlCtxt = xmlSaveToFd(fd, "UTF-8", XML_SAVE_FORMAT);

	if (xmlCtxt != NULL) {
		xmlSaveDoc(xmlCtxt, doc);
		xmlSaveClose(xmlCtxt);
	}
}

static void info_doc_free(xmlDocPtr doc)
{
	xmlFreeDoc(doc);
}

static void info_save_to_xml(struct info *info)
{
	xmlDocPtr doc = info_doc_new();

	info_doc_add_global(doc, info->global_status);

	if (info->module_infos != NULL) {
		struct module_info **m;

		for(m = info->module_infos; *m != NULL; m++)
			info_doc_add_module(doc, *m);
	}

	info_doc_save_to_fd(doc, STDOUT_FILENO);
	info_doc_free(doc);
}

static void info_save_to_stdout(struct info *info)
{
	struct module_info **m;
	struct base_info **b;

	printf( "--- Armadito info --- \n");
	printf( "Update global status : %s\n", info->global_status);
	if (info->module_infos != NULL) {
		for(m = info->module_infos; *m != NULL; m++){
			printf( "Module %s \n", (*m)->name );
			printf( "- Update status : %s\n", (*m)->mod_status);
			printf( "- Update date : %s \n", (*m)->update_date );

			if ((*m)->base_infos != NULL) {
				for(b = (*m)->base_infos; *b != NULL; b++){
					printf( "-- Base %s \n", (*b)->name );
					printf( "--- Update date : %s \n", (*b)->date );
					printf( "--- Version : %s \n", (*b)->version );
					printf( "--- Signature count : %d \n", (*b)->signature_count );
					printf( "--- Full path : %s \n", (*b)->full_path );
				}
			}
		}
	}
}

static void ipc_handler_info_module(struct ipc_manager *m, void *data)
{
	struct info *info = (struct info *)data;
	struct module_info *mod_info = malloc(sizeof(struct module_info));
	int n, n_bases, argc;
	char *mod_name, *mod_status, *update_date;

	ipc_manager_get_arg_at(m, 0, IPC_STRING_T, &mod_name);
	mod_info->name = strdup(mod_name);
	ipc_manager_get_arg_at(m, 1, IPC_STRING_T, &mod_status);
	mod_info->mod_status = update_status_str(mod_status);
	ipc_manager_get_arg_at(m, 2, IPC_STRING_T, &update_date);
	mod_info->update_date = strdup(update_date);

	n_bases = (ipc_manager_get_argc(m) - 3) / 5;

	mod_info->base_infos = malloc((n_bases + 1) * sizeof(struct a6o_base_info *));
	mod_info->base_infos[n_bases] = NULL;

	for (argc = 3, n = 0; argc < ipc_manager_get_argc(m); argc += 5, n++) {
		struct base_info *base_info = malloc(sizeof(struct base_info));
		char *name, *date, *version, *full_path;

		ipc_manager_get_arg_at(m, argc+0, IPC_STRING_T, &name);
		base_info->name = strdup(name);
		ipc_manager_get_arg_at(m, argc+1, IPC_STRING_T, &date);
		base_info->date = strdup(date);
		ipc_manager_get_arg_at(m, argc+2, IPC_STRING_T, &version);
		base_info->version = strdup(version);
		ipc_manager_get_arg_at(m, argc+3, IPC_INT32_T, &base_info->signature_count);
		ipc_manager_get_arg_at(m, argc+4, IPC_STRING_T, &full_path);
		base_info->full_path = strdup(full_path);

		mod_info->base_infos[n] = base_info;
	}

	info_append_module(info, mod_info);
}

static void ipc_handler_info_end(struct ipc_manager *m, void *data)
{
	struct info *info = (struct info *)data;
	char *global_status;

	ipc_manager_get_arg_at(m, 0, IPC_STRING_T, &global_status);
	info->global_status = update_status_str(global_status);
}

static void do_info(struct info_options *opts, int client_sock)
{
	struct info *info = info_new();
	struct ipc_manager *manager;

	manager = ipc_manager_new(client_sock);

	ipc_manager_add_handler(manager, IPC_MSG_ID_INFO_MODULE, ipc_handler_info_module, info);
	ipc_manager_add_handler(manager, IPC_MSG_ID_INFO_END, ipc_handler_info_end, info);

	ipc_manager_msg_send(manager, IPC_MSG_ID_INFO, IPC_NONE_T);

	while (ipc_manager_receive(manager) > 0)
		;

	ipc_manager_free(manager);

	if (opts->output_xml)
		info_save_to_xml(info);
	else
		info_save_to_stdout(info);

	info_free(info);
}

int main(int argc, const char **argv)
{
	struct info_options *opts = (struct info_options *)malloc(sizeof(struct info_options));
	int client_sock;

	parse_options(argc, argv, opts);

	if (opts->socket_type == TCP_SOCKET)
		client_sock = tcp_client_connect("127.0.0.1", opts->port_number, 10);
	else
		client_sock = unix_client_connect(opts->unix_path, 10);

	if (client_sock < 0) {
		fprintf(stderr, "cannot open client socket (errno %d)\n", errno);
		return 1;
	}

	do_info(opts, client_sock);

	return 0;
}
