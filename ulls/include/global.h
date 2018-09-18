#ifndef ULLVS_GLOBAL
#define ULLVS_GLOBAL
#include <gst/gst.h>
#include <libsoup/soup.h>
#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>
#include <gst/sdp/sdp.h>
#include <json-glib/json-glib.h>
#include <gobject/gvaluetypes.h>


#include <gst/video/videooverlay.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#if defined (GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#elif defined (GDK_WINDOWING_WIN32)
#include <gdk/gdkwin32.h>
#elif defined (GDK_WINDOWING_QUARTZ)
#include <gdk/gdkquartz.h>
#endif

typedef struct {
	const gchar * url;
	const gchar * u_id;
	
}Context;

extern GMainLoop *loop;
extern GstElement *pipe1, *pipe2, *pipe3, *webrtc1, *webrtc2, *webrtc3;
extern GtkWidget *video1, *video2, *video3;

extern enum AppState app_state1;
extern enum AppState app_state2;
extern enum AppState app_state3;

extern SoupWebsocketConnection *ws_conn;
extern gboolean disable_ssl;
extern gint values;

extern Context g_ctx;

gchar* get_string_from_json_object (JsonObject * object);
JsonObject* parse_json_object(const gchar* text);

#endif
