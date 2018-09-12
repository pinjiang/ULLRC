/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2003, Ximian, Inc.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <libsoup/soup.h>
#include <json-glib/json-glib.h>
#include <glib/gstdio.h>

#include "ullvs-gst-webrtc.h"
#include "global.h"

static int compare_strings (gconstpointer a, gconstpointer b)
{
	const char **sa = (const char **)a;
	const char **sb = (const char **)b;

	return strcmp (*sa, *sb);
}

static GString * get_directory_listing (const char *path)
{
	GPtrArray *entries;
	GString *listing;
	char *escaped;
	GDir *dir;
	const gchar *d_name;
	int i;

	entries = g_ptr_array_new ();
	dir = g_dir_open (path, 0, NULL);
	if (dir) {
		while ((d_name = g_dir_read_name (dir))) {
			if (!strcmp (d_name, ".") ||
			    (!strcmp (d_name, "..") &&
			     !strcmp (path, "./")))
				continue;
			escaped = g_markup_escape_text (d_name, -1);
			g_ptr_array_add (entries, escaped);
		}
		g_dir_close (dir);
	}

	g_ptr_array_sort (entries, (GCompareFunc)compare_strings);

	listing = g_string_new ("<html>\r\n");
	escaped = g_markup_escape_text (strchr (path, '/'), -1);
	g_string_append_printf (listing, "<head><title>Index of %s</title></head>\r\n", escaped);
	g_string_append_printf (listing, "<body><h1>Index of %s</h1>\r\n<p>\r\n", escaped);
	g_free (escaped);
	for (i = 0; i < entries->len; i++) {
		g_string_append_printf (listing, "<a href=\"%s\">%s</a><br>\r\n",
					(char *)entries->pdata[i], 
					(char *)entries->pdata[i]);
		g_free (entries->pdata[i]);
	}
	g_string_append (listing, "</body>\r\n</html>\r\n");

	g_ptr_array_free (entries, TRUE);
	return listing;
}

static void do_get (SoupServer *server, SoupMessage *msg, const char *path)
{
	char *slash;
	GStatBuf st;

	if (msg->method == SOUP_METHOD_GET) {
		SoupBuffer *buffer;
		SoupMessageHeadersIter iter;
		const char *name, *value;

		/* mapping = g_mapped_file_new (path, FALSE, NULL);
		if (!mapping) {
			soup_message_set_status (msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);
			return;
		} */

		soup_message_headers_append (msg->response_headers,"Access-Control-Allow-Origin", "*");
		
		/* buffer = soup_buffer_new_with_owner (g_mapped_file_get_contents (mapping),
											 g_mapped_file_get_length (mapping),
											 mapping, (GDestroyNotify)g_mapped_file_unref);
		
		soup_message_body_append_buffer (msg->response_body, buffer); */
				
		soup_buffer_free (buffer);
	} else /* msg->method == SOUP_METHOD_HEAD */ {
		char *length;

		/* We could just use the same code for both GET and
		 * HEAD (soup-message-server-io.c will fix things up).
		 * But we'll optimize and avoid the extra I/O.
		 */
		length = g_strdup_printf ("%lu", (gulong)st.st_size);
		soup_message_headers_append (msg->response_headers,
					     "Content-Length", length);
		g_free (length);
	}

	soup_message_set_status (msg, SOUP_STATUS_OK);
}

static void do_post (SoupServer *server, SoupMessage *msg)
{
	SoupBuffer *buffer;
	gchar *text;
	JsonObject *json_obj;
	
	json_obj = json_object_new ();
	json_object_set_string_member (json_obj, "code", "0");
	text = get_string_from_json_object (json_obj);
	json_object_unref (json_obj);
	
	buffer = soup_buffer_new_with_owner (text, strlen(text), text, (GDestroyNotify)g_free);
	
	soup_message_headers_append (msg->response_headers,"Access-Control-Allow-Origin", "*");
	soup_message_body_append_buffer (msg->response_body, buffer);
	soup_buffer_free (buffer);
	
	soup_message_set_status (msg, SOUP_STATUS_OK);
}

static void do_put (SoupServer *server, SoupMessage *msg, const char *path)
{
	GStatBuf st;
	FILE *f;
	gboolean created = TRUE;

	if (g_stat (path, &st) != -1) {
		const char *match = soup_message_headers_get_one (msg->request_headers, "If-None-Match");
		if (match && !strcmp (match, "*")) {
			soup_message_set_status (msg, SOUP_STATUS_CONFLICT);
			return;
		}

		if (!g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
			soup_message_set_status (msg, SOUP_STATUS_FORBIDDEN);
			return;
		}

		created = FALSE;
	}

	f = fopen (path, "w");
	if (!f) {
		soup_message_set_status (msg, SOUP_STATUS_INTERNAL_SERVER_ERROR);
		return;
	}

	fwrite (msg->request_body->data, 1, msg->request_body->length, f);
	fclose (f);

	soup_message_set_status (msg, created ? SOUP_STATUS_CREATED : SOUP_STATUS_OK);
}

void server_callback (SoupServer *server, SoupMessage *msg,
		 const char *path, GHashTable *query,
		 SoupClientContext *context, gpointer data) 
{
	char *file_path;
	SoupMessageHeadersIter iter;
	const char *name, *value;

	g_print ("%s %s HTTP/1.%d\n", msg->method, path,
		 soup_message_get_http_version (msg));
	soup_message_headers_iter_init (&iter, msg->request_headers);
	while (soup_message_headers_iter_next (&iter, &name, &value))
		g_print ("%s: %s\n", name, value);
	if (msg->request_body->length) {
		g_print ("%s\n", msg->request_body->data);
		
		//===============add by liyujia, 20180905=================
		//now ,parse json data
		JsonObject *object = parse_json_object(msg->request_body->data);
		if (json_object_has_member (object, "ConnectToServer")) {
			JsonObject *child;
			const gchar *server_url_, *terminal_id_;
			child = json_object_get_object_member (object, "ConnectToServer");

			if (json_object_has_member (child, "server_url") && json_object_has_member (child, "terminal_id")) {
				server_url_ = json_object_get_string_member (child, "server_url");                         
				terminal_id_ = json_object_get_string_member (child, "terminal_id");
			}
			g_print ("server_url = %s,  terminal_id = %s\n", server_url_, terminal_id_);
			checkAndConnect(server_url_, terminal_id_);
			
		} else if (json_object_has_member (object, "Monitoring")) {
			JsonObject *child;
			const gchar *_event, *_data;
			child = json_object_get_object_member (object, "Monitoring");

			if (json_object_has_member (child, "_event") && json_object_has_member (child, "_data")) {
				_event = json_object_get_string_member (child, "_event");                         
				_data = json_object_get_string_member (child, "_data");
			}
			g_print ("event = %s,  data = %s\n", _event, _data);
			if (!setup_call ()) {
				g_print ("Failed to setup call!\n");
				cleanup_and_quit_loop ("ERROR: Failed to setup call", PEER_CALL_ERROR);
			}
			
		} else {
			g_print ("do nothing!\n");
		}
		//add end
	}

	file_path = g_strdup_printf (".%s", path);

	if (msg->method == SOUP_METHOD_GET || msg->method == SOUP_METHOD_HEAD)
		do_get (server, msg, file_path);
	else if (msg->method == SOUP_METHOD_PUT)
		do_put (server, msg, file_path);
	else if (msg->method == SOUP_METHOD_POST)
		do_post(server, msg);
	else
		soup_message_set_status (msg, SOUP_STATUS_NOT_IMPLEMENTED);

	g_free (file_path);
	g_print ("  -> %d %s\n\n", msg->status_code, msg->reason_phrase);
}
#if 0
int main (int argc, char **argv)
{
	GOptionContext *opts;
	GMainLoop *loop;
	SoupServer *server;
	GSList *uris, *u;
	char *str;
	GTlsCertificate *cert;
	GError *error = NULL;

	opts = g_option_context_new (NULL);
	g_option_context_add_main_entries (opts, entries, NULL);
	if (!g_option_context_parse (opts, &argc, &argv, &error)) {
		g_printerr ("Could not parse arguments: %s\n",
			    error->message);
		g_printerr ("%s",
			    g_option_context_get_help (opts, TRUE, NULL));
		exit (1);
	}
	if (argc != 1) {
		g_printerr ("%s",
			    g_option_context_get_help (opts, TRUE, NULL));
		exit (1);
	}
	g_option_context_free (opts);

	signal (SIGINT, quit);

	if (tls_cert_file && tls_key_file) {
		cert = g_tls_certificate_new_from_files (tls_cert_file, tls_key_file, &error);
		if (error) {
			g_printerr ("Unable to create server: %s\n", error->message);
			exit (1);
		}
		server = soup_server_new (SOUP_SERVER_SERVER_HEADER, "simple-httpd ",
					  SOUP_SERVER_TLS_CERTIFICATE, cert,
					  NULL);
		g_object_unref (cert);

		soup_server_listen_all (server, port, SOUP_SERVER_LISTEN_HTTPS, &error);
	} else {
		server = soup_server_new (SOUP_SERVER_SERVER_HEADER, "simple-httpd ",
					  NULL);
		soup_server_listen_all (server, port, 0, &error);
	}

	soup_server_add_handler (server, NULL,
				 server_callback, NULL, NULL);

	uris = soup_server_get_uris (server);
	for (u = uris; u; u = u->next) {
		str = soup_uri_to_string (u->data, FALSE);
		g_print ("Listening on %s\n", str);
		g_free (str);
		soup_uri_free (u->data);
	}
	g_slist_free (uris);

	g_print ("\nWaiting for requests...\n");

	loop = g_main_loop_new (NULL, TRUE);
	g_main_loop_run (loop);

	return 0;
}
#endif
