#ifndef ULLVS_GLOBAL
#define ULLVS_GLOBAL
#include <gst/gst.h>
#include <libsoup/soup.h>
#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>
#include <gst/sdp/sdp.h>
#include <json-glib/json-glib.h>
#include <gobject/gvaluetypes.h>

typedef struct {
	const gchar * url;
	const gchar * u_id;
	
}Context;

extern GMainLoop *loop;
extern GstElement *pipe1, *webrtc1;

extern SoupWebsocketConnection *ws_conn;
extern enum AppState app_state;
extern gboolean disable_ssl;
extern gint values;

extern Context g_ctx;

gchar* get_string_from_json_object (JsonObject * object);
JsonObject* parse_json_object(const gchar* text);
// gchar* parse_json_child(JsonObject* object);

#endif