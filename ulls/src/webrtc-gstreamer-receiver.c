/*
 * Demo gstreamer app for negotiating and streaming a sendrecv webrtc stream
 * with a browser JS app.
 *
 * gcc webrtc-gstreamer-receiver.c $(pkg-config --cflags --libs gstreamer-webrtc-1.0 gstreamer-sdp-1.0 libsoup-2.4 json-glib-1.0) -o webrtc-gstreamer-receiver
 *
 * Author: Nirbheek Chauhan <nirbheek@centricular.com>
 */

#include <string.h>
#include <pthread.h> 

#include "global.h"
#include "ullvs-gst-webrtc.h"


gboolean cleanup_and_quit_loop (const gchar * msg, enum AppState state) {
  if (msg)
    g_printerr ("%s\n", msg);
  if (state > 0) {
    app_state1 = state;
    app_state2 = state;
    app_state3 = state;
  }

  if (ws_conn) {
    if (soup_websocket_connection_get_state (ws_conn) ==
        SOUP_WEBSOCKET_STATE_OPEN)
      /* This will call us again */
      soup_websocket_connection_close (ws_conn, 1000, "");
    else
      g_object_unref (ws_conn);
  }

  if (loop) {
    g_main_loop_quit (loop);
    loop = NULL;
  }

  /* To allow usage as a GSourceFunc */
  return G_SOURCE_REMOVE;
}

static void handle_media_stream (GstPad * pad, GstElement * pipe, const char * convert_name, const char * sink_name) {
  GstPad *qpad;
  GstElement *q, *conv, *resample, *sink;
  GstPadLinkReturn ret;

  g_print ("Trying to handle stream with %s ! %s", convert_name, sink_name);

  q = gst_element_factory_make ("queue", NULL);
  g_assert_nonnull (q);
  conv = gst_element_factory_make (convert_name, NULL);
  g_assert_nonnull (conv);
  sink = gst_element_factory_make (sink_name, NULL);
  g_assert_nonnull (sink);

  if (g_strcmp0 (convert_name, "audioconvert") == 0) {
    /* Might also need to resample, so add it just in case.
     * Will be a no-op if it's not required. */
    resample = gst_element_factory_make ("audioresample", NULL);
    g_assert_nonnull (resample);
    gst_bin_add_many (GST_BIN (pipe), q, conv, resample, sink, NULL);
    gst_element_sync_state_with_parent (q);
    gst_element_sync_state_with_parent (conv);
    gst_element_sync_state_with_parent (resample);
    gst_element_sync_state_with_parent (sink);
    gst_element_link_many (q, conv, resample, sink, NULL);
  } else {
    gst_bin_add_many (GST_BIN (pipe), q, conv, sink, NULL);
    gst_element_sync_state_with_parent (q);
    gst_element_sync_state_with_parent (conv);
    gst_element_sync_state_with_parent (sink);
    gst_element_link_many (q, conv, sink, NULL);
  }

  qpad = gst_element_get_static_pad (q, "sink");

  ret = gst_pad_link (pad, qpad);
  g_assert_cmphex (ret, ==, GST_PAD_LINK_OK);
}

static void on_incoming_decodebin_stream (GstElement * decodebin, GstPad * pad, GstElement * pipe)
{
  GstCaps *caps;
  const gchar *name;
  // add by liyujia
  const gchar *capsString;
  const gchar *structureString;
  const GValue *formatValue;
  //add end

  if (!gst_pad_has_current_caps (pad)) {
    g_printerr ("Pad '%s' has no caps, can't do anything, ignoring\n",
        GST_PAD_NAME (pad));
    return;
  }

  caps = gst_pad_get_current_caps (pad);
  name = gst_structure_get_name (gst_caps_get_structure (caps, 0));
  g_print ("Pad name: %s\n", name); 
  //add by liyujia
  capsString = gst_caps_to_string (caps);
  g_print ("caps string: %s\n", capsString);
  formatValue = gst_structure_get_value (gst_caps_get_structure (caps, 0), "width");
  values = g_value_get_int (formatValue);
  structureString = gst_structure_to_string (gst_caps_get_structure (caps, 0));
  g_print ("formate value: %d\n", values); 
  //add end

  if (g_str_has_prefix (name, "video")) {
    handle_media_stream (pad, pipe, "videoconvert", "fpsdisplaysink");
  } else if (g_str_has_prefix (name, "audio")) {
    handle_media_stream (pad, pipe, "audioconvert", "autoaudiosink");
  } else {
    g_printerr ("Unknown pad %s, ignoring", GST_PAD_NAME (pad));
  }
}

/*tatic void on_incoming_stream (GstElement * webrtc, GstPad * pad, GstElement * pipe)
{
  GstElement *decodebin;

  if (GST_PAD_DIRECTION (pad) != GST_PAD_SRC)
    return;

  decodebin = gst_element_factory_make ("decodebin", NULL);
  g_signal_connect (decodebin, "pad-added",
      G_CALLBACK (on_incoming_decodebin_stream), pipe);
  gst_bin_add (GST_BIN (pipe), decodebin);
  gst_element_sync_state_with_parent (decodebin);
  gst_element_link (webrtc, decodebin);
}*/

static void realize_cb (GtkWidget *widget, GstElement *sink) {
  GdkWindow *window = gtk_widget_get_window (widget);
  guintptr window_handle;
  if (!gdk_window_ensure_native (window))
     g_error ("Couldn't create native window needed for GstXOverlay!");

  #if defined (GDK_WINDOWING_WIN32)
    window_handle = (guintptr)GDK_WINDOW_HWND (window);
  #elif defined (GDK_WINDOWING_QUARTZ)
    window_handle = gdk_quartz_window_get_nsview (window);
  #elif defined (GDK_WINDOWING_X11)
    window_handle = GDK_WINDOW_XID (window);
  #endif
  
  gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(sink), window_handle);
  
}

static void on_incoming_stream1 (GstElement * webrtc, GstPad * pad, GstElement * pipe)
{
  GstElement *play;
  play = gst_parse_bin_from_description ("rtpvp8depay ! avdec_vp8 ! queue ! videoconvert ! autovideosink", TRUE, NULL);
  //play = gst_parse_bin_from_description ("decodebin ! queue ! videoconvert ! autovideosink", TRUE, NULL);
  gst_bin_add (GST_BIN (pipe), play);

  // Start displaying video 
  gst_element_sync_state_with_parent (play);
  gst_element_link (webrtc, play);
  GstElement *sink = gst_bin_get_by_name (GST_BIN (play), "sink");  
  realize_cb(video1, sink);
}

static void on_incoming_stream2 (GstElement * webrtc, GstPad * pad, GstElement * pipe)
{
  GstElement *play;
  play = gst_parse_bin_from_description ("rtpvp8depay ! avdec_vp8 ! queue ! videoconvert ! autovideosink", TRUE, NULL);
  gst_bin_add (GST_BIN (pipe), play);

  // Start displaying video 
  gst_element_sync_state_with_parent (play);
  gst_element_link (webrtc, play);
  GstElement *sink = gst_bin_get_by_name (GST_BIN (play), "sink");  
  realize_cb(video2, sink);
}

static void on_incoming_stream3 (GstElement * webrtc, GstPad * pad, GstElement * pipe)
{
  GstElement *play;
  play = gst_parse_bin_from_description ("rtpvp8depay ! avdec_vp8 ! queue ! videoconvert ! autovideosink", TRUE, NULL);
  gst_bin_add (GST_BIN (pipe), play);

  // Start displaying video 
  gst_element_sync_state_with_parent (play);
  gst_element_link (webrtc, play);
  GstElement *sink = gst_bin_get_by_name (GST_BIN (play), "sink");  
  realize_cb(video3, sink);
}

static void send_ice_candidate_message1 (GstElement * webrtc G_GNUC_UNUSED, guint mlineindex,
    gchar * candidate, gpointer user_data G_GNUC_UNUSED)
{
  gchar *text;
  JsonObject *ice, *msg;

  if (app_state1 < PEER_CALL_NEGOTIATING) {
    cleanup_and_quit_loop ("Can't send ICE1, not in call", APP_STATE_ERROR);
    return;
  }

  ice = json_object_new ();
  json_object_set_string_member (ice, "candidate", candidate);
  json_object_set_int_member (ice, "sdpMLineIndex", mlineindex);
  msg = json_object_new ();
  json_object_set_object_member (msg, "ice1", ice);
  text = get_string_from_json_object (msg);
  json_object_unref (msg);

  soup_websocket_connection_send_text (ws_conn, text);
  g_free (text);
}

static void send_ice_candidate_message2 (GstElement * webrtc G_GNUC_UNUSED, guint mlineindex,
    gchar * candidate, gpointer user_data G_GNUC_UNUSED)
{
  gchar *text;
  JsonObject *ice, *msg;

  if (app_state2 < PEER_CALL_NEGOTIATING) {
    cleanup_and_quit_loop ("Can't send ICE2, not in call", APP_STATE_ERROR);
    return;
  }

  ice = json_object_new ();
  json_object_set_string_member (ice, "candidate", candidate);
  json_object_set_int_member (ice, "sdpMLineIndex", mlineindex);
  msg = json_object_new ();
  json_object_set_object_member (msg, "ice2", ice);
  text = get_string_from_json_object (msg);
  json_object_unref (msg);

  soup_websocket_connection_send_text (ws_conn, text);
  g_free (text);
}

static void send_ice_candidate_message3 (GstElement * webrtc G_GNUC_UNUSED, guint mlineindex,
    gchar * candidate, gpointer user_data G_GNUC_UNUSED)
{
  gchar *text;
  JsonObject *ice, *msg;

  if (app_state3 < PEER_CALL_NEGOTIATING) {
    cleanup_and_quit_loop ("Can't send ICE3, not in call", APP_STATE_ERROR);
    return;
  }

  ice = json_object_new ();
  json_object_set_string_member (ice, "candidate", candidate);
  json_object_set_int_member (ice, "sdpMLineIndex", mlineindex);
  msg = json_object_new ();
  json_object_set_object_member (msg, "ice3", ice);
  text = get_string_from_json_object (msg);
  json_object_unref (msg);

  soup_websocket_connection_send_text (ws_conn, text);
  g_free (text);
}

//modified by liyujia

static void send_sdp_answer1 (GstWebRTCSessionDescription * answer)
{
  gchar *text;
  JsonObject *msg, *sdp;
  if (app_state1 < PEER_CALL_NEGOTIATING) {
    cleanup_and_quit_loop ("Can't send answer1, not in call", APP_STATE_ERROR);
    return;
  }

  text = gst_sdp_message_as_text (answer->sdp);
  g_print ("Sending answer1:\n%s\n", text);

  sdp = json_object_new ();
  json_object_set_string_member (sdp, "type", "answer1");
  json_object_set_string_member (sdp, "sdp", text);
  g_free (text);

  msg = json_object_new ();
  json_object_set_object_member (msg, "sdp", sdp);
  text = get_string_from_json_object (msg);
  json_object_unref (msg);

  soup_websocket_connection_send_text (ws_conn, text);
  g_free (text);
}

static void send_sdp_answer2 (GstWebRTCSessionDescription * answer)
{
  gchar *text;
  JsonObject *msg, *sdp;
  if (app_state2 < PEER_CALL_NEGOTIATING) {
    cleanup_and_quit_loop ("Can't send answer2, not in call", APP_STATE_ERROR);
    return;
  }

  text = gst_sdp_message_as_text (answer->sdp);
  g_print ("Sending answer2:\n%s\n", text);

  sdp = json_object_new ();
  json_object_set_string_member (sdp, "type", "answer2");
  json_object_set_string_member (sdp, "sdp", text);
  g_free (text);

  msg = json_object_new ();
  json_object_set_object_member (msg, "sdp", sdp);
  text = get_string_from_json_object (msg);
  json_object_unref (msg);

  soup_websocket_connection_send_text (ws_conn, text);
  g_free (text);
}

static void send_sdp_answer3 (GstWebRTCSessionDescription * answer)
{
  gchar *text;
  JsonObject *msg, *sdp;
  if (app_state3 < PEER_CALL_NEGOTIATING) {
    cleanup_and_quit_loop ("Can't send answer3, not in call", APP_STATE_ERROR);
    return;
  }

  text = gst_sdp_message_as_text (answer->sdp);
  g_print ("Sending answer3:\n%s\n", text);

  sdp = json_object_new ();
  json_object_set_string_member (sdp, "type", "answer3");
  json_object_set_string_member (sdp, "sdp", text);
  g_free (text);

  msg = json_object_new ();
  json_object_set_object_member (msg, "sdp", sdp);
  text = get_string_from_json_object (msg);
  json_object_unref (msg);

  soup_websocket_connection_send_text (ws_conn, text);
  g_free (text);
}

/* answer created by our pipeline, to be sent to the peer */
static void on_answer_created1 (GstPromise * promise, gpointer user_data)
{
  GstWebRTCSessionDescription *answer = NULL;
  const GstStructure *reply;

  g_assert_cmphex (app_state1, ==, PEER_CALL_NEGOTIATING);

  g_assert_cmphex (gst_promise_wait(promise), ==, GST_PROMISE_RESULT_REPLIED);
  reply = gst_promise_get_reply (promise);
  gst_structure_get (reply, "answer",
      GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &answer, NULL);
  gst_promise_unref (promise);

  promise = gst_promise_new ();
  g_assert_nonnull (webrtc1);
  g_signal_emit_by_name (webrtc1, "set-local-description", answer, promise);
  gst_promise_interrupt (promise);
  gst_promise_unref (promise);
  g_print ("local description setted!\n");

  /* Send answer to peer */
  send_sdp_answer1 (answer);
  gst_webrtc_session_description_free (answer);
}

static void on_answer_created2 (GstPromise * promise, gpointer user_data)
{
  GstWebRTCSessionDescription *answer = NULL;
  const GstStructure *reply;

  g_assert_cmphex (app_state2, ==, PEER_CALL_NEGOTIATING);

  g_assert_cmphex (gst_promise_wait(promise), ==, GST_PROMISE_RESULT_REPLIED);
  reply = gst_promise_get_reply (promise);
  gst_structure_get (reply, "answer",
      GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &answer, NULL);
  gst_promise_unref (promise);

  promise = gst_promise_new ();
  g_assert_nonnull (webrtc2);
  g_signal_emit_by_name (webrtc2, "set-local-description", answer, promise);
  gst_promise_interrupt (promise);
  gst_promise_unref (promise);
  g_print ("local description setted!\n");

  /* Send answer to peer */
  send_sdp_answer2 (answer);
  gst_webrtc_session_description_free (answer);
}

static void on_answer_created3 (GstPromise * promise, gpointer user_data)
{
  GstWebRTCSessionDescription *answer = NULL;
  const GstStructure *reply;

  g_assert_cmphex (app_state3, ==, PEER_CALL_NEGOTIATING);

  g_assert_cmphex (gst_promise_wait(promise), ==, GST_PROMISE_RESULT_REPLIED);
  reply = gst_promise_get_reply (promise);
  gst_structure_get (reply, "answer",
      GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &answer, NULL);
  gst_promise_unref (promise);

  promise = gst_promise_new ();
  g_assert_nonnull (webrtc3);
  g_signal_emit_by_name (webrtc3, "set-local-description", answer, promise);
  gst_promise_interrupt (promise);
  gst_promise_unref (promise);
  g_print ("local description setted!\n");

  /* Send answer to peer */
  send_sdp_answer3 (answer);
  gst_webrtc_session_description_free (answer);
}

static void on_negotiation_needed1 (GstElement * element, gpointer user_data)
{
  GstPromise *promise;

  app_state1 = PEER_CALL_NEGOTIATING;
  g_print ("app state1 == PEER_CALL_NEGOTIATING\n");
  promise = gst_promise_new_with_change_func (on_answer_created1, user_data, NULL);;
  g_signal_emit_by_name (webrtc1, "create-answer", NULL, promise);
}

static void on_negotiation_needed2 (GstElement * element, gpointer user_data)
{
  GstPromise *promise;

  app_state2 = PEER_CALL_NEGOTIATING;
  g_print ("app state2 == PEER_CALL_NEGOTIATING\n");
  promise = gst_promise_new_with_change_func (on_answer_created2, user_data, NULL);;
  g_signal_emit_by_name (webrtc2, "create-answer", NULL, promise);
}

static void on_negotiation_needed3 (GstElement * element, gpointer user_data)
{
  GstPromise *promise;

  app_state3 = PEER_CALL_NEGOTIATING;
  g_print ("app state3 == PEER_CALL_NEGOTIATING\n");
  promise = gst_promise_new_with_change_func (on_answer_created3, user_data, NULL);;
  g_signal_emit_by_name (webrtc3, "create-answer", NULL, promise);
}

static gboolean create_pipeAndwebrtc1 (void)
{
  GError *error = NULL;
  
  pipe1 = gst_parse_launch ("webrtcbin name=recv1 " STUN_SERVER "udpsrc", &error);
    
  if (error) {
    g_printerr ("Failed to parse launch: %s\n", error->message);
    g_error_free (error);
    goto err;
  }
  webrtc1 = gst_bin_get_by_name (GST_BIN (pipe1), "recv1");  
  g_assert_nonnull (webrtc1); 

  return TRUE;

err:
  if (pipe1)
    g_clear_object (&pipe1);
  if (webrtc1)
    webrtc1 = NULL;
  return FALSE;
}

static gboolean create_pipeAndwebrtc2 (void)
{
  GError *error = NULL;
  
  pipe2 = gst_parse_launch ("webrtcbin name=recv2 " STUN_SERVER "udpsrc", &error);
    
  if (error) {
    g_printerr ("Failed to parse launch: %s\n", error->message);
    g_error_free (error);
    goto err;
  }
  webrtc2 = gst_bin_get_by_name (GST_BIN (pipe2), "recv2");  
  g_assert_nonnull (webrtc2); 

  return TRUE;

err:
  if (pipe2)
    g_clear_object (&pipe2);
  if (webrtc2)
    webrtc2 = NULL;
  return FALSE;
}

static gboolean create_pipeAndwebrtc3 (void)
{
  GError *error = NULL;

  pipe3 = gst_parse_launch ("webrtcbin name=recv3 " STUN_SERVER "udpsrc", &error);
    
  if (error) {
    g_printerr ("Failed to parse launch: %s\n", error->message);
    g_error_free (error);
    goto err;
  }
  webrtc3 = gst_bin_get_by_name (GST_BIN (pipe3), "recv3");  
  g_assert_nonnull (webrtc3); 

  return TRUE;

err:
  if (pipe3)
    g_clear_object (&pipe3);
  if (webrtc3)
    webrtc3 = NULL;
  return FALSE;
}

static gboolean start_pipeline1 (void)
{
  GstStateChangeReturn ret;

  /* This is the gstwebrtc entry point where we create the answer and so on. It
   * will be called when the pipeline goes to PLAYING. */
  on_negotiation_needed1(NULL,NULL);                                           // 5: creste answer and send

  /* We need to transmit this ICE candidate to peer via the websockets
   * signalling server. Incoming ice candidates from the peer need to be
   * added by us too, see on_server_message() */
  g_signal_connect (webrtc1, "on-ice-candidate", G_CALLBACK (send_ice_candidate_message1), NULL);   // 6: send candidate

  /* Incoming streams will be exposed via this signal */
  g_signal_connect (webrtc1, "pad-added", G_CALLBACK (on_incoming_stream1), pipe1);

  /* Lifetime is the same as the pipeline itself */
  gst_object_unref (webrtc1);

  g_print ("Starting pipeline 1!\n");
  ret = gst_element_set_state (GST_ELEMENT (pipe1), GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE)
    goto err;

  return TRUE;

err:
  if (pipe1)
    g_clear_object (&pipe1);
  if (webrtc1)
    webrtc1 = NULL;
  return FALSE;
}

static gboolean start_pipeline2 (void)
{
  GstStateChangeReturn ret;
  on_negotiation_needed2(NULL,NULL);                                           // 5: creste answer and send
  g_signal_connect (webrtc2, "on-ice-candidate", G_CALLBACK (send_ice_candidate_message2), NULL);   // 6: send candidate
  g_signal_connect (webrtc2, "pad-added", G_CALLBACK (on_incoming_stream2), pipe2);
  gst_object_unref (webrtc2);
  g_print ("Starting pipeline2 !\n");
  ret = gst_element_set_state (GST_ELEMENT (pipe2), GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE)
    goto err;

  return TRUE;

err:
  if (pipe2)
    g_clear_object (&pipe2);
  if (webrtc2)
    webrtc2 = NULL;
  return FALSE;
}

static gboolean start_pipeline3 (void)
{
  GstStateChangeReturn ret;
  on_negotiation_needed3(NULL,NULL);                                           // 5: creste answer and send
  g_signal_connect (webrtc3, "on-ice-candidate", G_CALLBACK (send_ice_candidate_message3), NULL);   // 6: send candidate
  g_signal_connect (webrtc3, "pad-added", G_CALLBACK (on_incoming_stream3), pipe3);
  gst_object_unref (webrtc3);

  g_print ("Starting pipeline3 !\n");
  ret = gst_element_set_state (GST_ELEMENT (pipe3), GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE)
    goto err;

  return TRUE;

err:
  if (pipe3)
    g_clear_object (&pipe3);
  if (webrtc3)
    webrtc3 = NULL;
  return FALSE;
}

/* One mega message handler for our asynchronous calling mechanism */
static void on_server_message (SoupWebsocketConnection * conn, SoupWebsocketDataType type, GBytes * message, gpointer user_data) {
	gsize size;
	gchar *text, *data;

	switch (type) {
		case SOUP_WEBSOCKET_DATA_BINARY:
			g_printerr ("Received unknown binary message, ignoring\n");
			g_bytes_unref (message);
			return;
		case SOUP_WEBSOCKET_DATA_TEXT:
			data = g_bytes_unref_to_data (message, &size);
			/* Convert to NULL-terminated string */
			text = g_strndup (data, size);
			g_free (data);
			break;
		default:
			g_assert_not_reached ();
	}
	/* Server has accepted our registration, we are ready to send commands */
  if (g_strcmp0 (text, "hello") == 0) {
		g_printerr ("messages == %s\n", text);
		g_print ("received server's ack\n");
	} else if (g_strcmp0 (text, "Hello Controller_video") == 0) {
		g_printerr ("messages == %s\n", text);
		if (app_state1 != SERVER_REGISTERING) {
			cleanup_and_quit_loop ("ERROR: Received HELLO when not registering", APP_STATE_ERROR);
			goto out;
		}
		app_state1 = SERVER_REGISTERED;
    app_state2 = SERVER_REGISTERED;
    app_state3 = SERVER_REGISTERED;
		g_print ("Registered with server\n");
	} else if (g_str_has_prefix (text, "ERROR")) {   /* Handle errors */
		switch (app_state1) {
			case SERVER_CONNECTING:
				app_state1 = SERVER_CONNECTION_ERROR;
        app_state2 = SERVER_CONNECTION_ERROR;
        app_state3 = SERVER_CONNECTION_ERROR;
				break;
			case SERVER_REGISTERING:
				app_state1 = SERVER_REGISTRATION_ERROR;
        app_state2 = SERVER_REGISTRATION_ERROR;
        app_state3 = SERVER_REGISTRATION_ERROR;
				break;
			case PEER_CONNECTING:
			  app_state1 = PEER_CONNECTION_ERROR;
        app_state2 = PEER_CONNECTION_ERROR;
        app_state3 = PEER_CONNECTION_ERROR;
			  break;
			case PEER_CONNECTED:
			default:
			  app_state1 = APP_STATE_ERROR;
        app_state2 = APP_STATE_ERROR;
        app_state3 = APP_STATE_ERROR;
    }
		cleanup_and_quit_loop (text, 0);
	/* Look for JSON messages containing SDP and ICE candidates */
	} else {

		JsonNode *root;
    JsonObject *object, *child;
    JsonParser *parser = json_parser_new ();
    if (!json_parser_load_from_data (parser, text, -1, NULL)) {
      g_printerr ("Unknown message '%s', ignoring", text);
      g_object_unref (parser);
      goto out;
    }
    root = json_parser_get_root (parser);
    if (!JSON_NODE_HOLDS_OBJECT (root)) {
      g_printerr ("Unknown json message '%s', ignoring", text);
      g_object_unref (parser);
      goto out;
    }
    object = json_node_get_object (root);
    /* Check type of JSON message */
    if (json_object_has_member (object, "sdp")) {           
      int ret;
      GstSDPMessage *sdp;
      const gchar *text, *sdptype;
      GstWebRTCSessionDescription *offer;
      child = json_object_get_object_member (object, "sdp");
      if (!json_object_has_member (child, "type")) {
        cleanup_and_quit_loop ("ERROR: received SDP without 'type'", PEER_CALL_ERROR);
        goto out;
      }
      sdptype = json_object_get_string_member (child, "type");
      /* handle offers from peers and reply with answers using webrtcbin */
      // g_assert_cmpstr (sdptype, ==, "offer");                           // 1: received offer
      text = json_object_get_string_member (child, "sdp");
      ret = gst_sdp_message_new (&sdp);
      g_assert_cmphex (ret, ==, GST_SDP_OK);
      ret = gst_sdp_message_parse_buffer ((guint8 *) text, strlen (text), sdp);
      g_assert_cmphex (ret, ==, GST_SDP_OK);
      offer = gst_webrtc_session_description_new (GST_WEBRTC_SDP_TYPE_OFFER, sdp);
      g_assert_nonnull (offer);                                         // check offer is null or not
      if (strcmp(sdptype, "offer1") == 0) {
        g_print ("Received offer1 :\n%s\n", text);  
        if(app_state1 < PEER_CONNECTED){
          app_state1 = PEER_CONNECTED;
          if(!create_pipeAndwebrtc1())                                 // 2: create pipeline and webrtc
            cleanup_and_quit_loop ("ERROR: failed to create pipeline1 and webrtc1", PEER_CALL_ERROR);
          g_print ("app state1 == PEER_CONNECTED\n");
          {
            GstPromise *promise = gst_promise_new ();
            g_assert_nonnull (webrtc1);
            g_signal_emit_by_name (webrtc1, "set-remote-description", offer, promise);  // 3: Set remote description on our pipeline
            gst_promise_interrupt (promise);
            gst_promise_unref (promise);
            g_print ("remote description1 setted\n");
          }
          if (!start_pipeline1 ())                                         // 4: start pipeline
            cleanup_and_quit_loop ("ERROR: failed to start pipeline1", PEER_CALL_ERROR);
        }
        g_assert_nonnull (webrtc1);
      }else if (strcmp(sdptype, "offer2") == 0) {                                        
        if(app_state2 < PEER_CONNECTED){
          app_state2 = PEER_CONNECTED;
          if(!create_pipeAndwebrtc2())                                    
            cleanup_and_quit_loop ("ERROR: failed to create pipeline2 and webrtc2", PEER_CALL_ERROR);
          g_print ("app state2 == PEER_CONNECTED\n");
          {
            GstPromise *promise = gst_promise_new ();
            g_assert_nonnull (webrtc2);
            g_signal_emit_by_name (webrtc2, "set-remote-description", offer, promise); 
            gst_promise_interrupt (promise);
            gst_promise_unref (promise);
            g_print ("remote description2 setted\n");
          }
          if (!start_pipeline2 ())                                       
            cleanup_and_quit_loop ("ERROR: failed to start pipeline2", PEER_CALL_ERROR);
        }
        g_assert_nonnull (webrtc2);
      }else if (strcmp(sdptype, "offer3") == 0) {                              
        if(app_state3 < PEER_CONNECTED){
          app_state3 = PEER_CONNECTED;
          if(!create_pipeAndwebrtc3())                                   
            cleanup_and_quit_loop ("ERROR: failed to create pipeline3 and webrtc3", PEER_CALL_ERROR);
          g_print ("app state3 == PEER_CONNECTED\n");
          {
            GstPromise *promise = gst_promise_new ();
            g_assert_nonnull (webrtc3);
            g_signal_emit_by_name (webrtc3, "set-remote-description", offer, promise);  
            gst_promise_interrupt (promise);
            gst_promise_unref (promise);
            g_print ("remote description3 setted\n");
          }
          if (!start_pipeline3 ())                                         
            cleanup_and_quit_loop ("ERROR: failed to start pipeline3", PEER_CALL_ERROR);
        }
        g_assert_nonnull (webrtc3);
      }
    } else if (json_object_has_member (object, "ice1")) {
      /* ICE candidate received from peer, add it */
      const gchar *candidate;
      gint sdpmlineindex;

      child = json_object_get_object_member (object, "ice1");
      candidate = json_object_get_string_member (child, "candidate");
      sdpmlineindex = json_object_get_int_member (child, "sdpMLineIndex");

      g_signal_emit_by_name (webrtc1, "add-ice-candidate", sdpmlineindex, candidate);  // Add ice candidate sent by remote peer
      g_print ("remote ice1 candidate added!\n");
    } else if (json_object_has_member (object, "ice2")) {
      /* ICE candidate received from peer, add it */
      const gchar *candidate;
      gint sdpmlineindex;

      child = json_object_get_object_member (object, "ice2");
      candidate = json_object_get_string_member (child, "candidate");
      sdpmlineindex = json_object_get_int_member (child, "sdpMLineIndex");

      g_signal_emit_by_name (webrtc2, "add-ice-candidate", sdpmlineindex, candidate);  
      g_print ("remote ice2 candidate added!\n");
    }  else if (json_object_has_member (object, "ice3")) {
      /* ICE candidate received from peer, add it */
      const gchar *candidate;
      gint sdpmlineindex;

      child = json_object_get_object_member (object, "ice3");
      candidate = json_object_get_string_member (child, "candidate");
      sdpmlineindex = json_object_get_int_member (child, "sdpMLineIndex");

      g_signal_emit_by_name (webrtc3, "add-ice-candidate", sdpmlineindex, candidate); 
      g_print ("remote ice3 candidate added!\n");
    } else {
      g_printerr ("Ignoring unknown JSON message:\n%s\n", text);
    }
    g_object_unref (parser);
	}
out:
  g_free (text);
}

gboolean setup_call (void) {
	gchar *msg;
	if (soup_websocket_connection_get_state (ws_conn) != SOUP_WEBSOCKET_STATE_OPEN)
		return FALSE;
	g_print ("Setting up signalling server call\n");
	msg = g_strdup_printf ("Session_video");
	soup_websocket_connection_send_text (ws_conn, msg);
	g_free (msg);
	return TRUE;
}

static gboolean register_with_server () {
  gchar *hello;

  if (soup_websocket_connection_get_state (ws_conn) !=
      SOUP_WEBSOCKET_STATE_OPEN)
    return FALSE;

  g_print ("Registering id %s with server\n", g_ctx.u_id);
  app_state1 = SERVER_REGISTERING;
  app_state2 = SERVER_REGISTERING;
  app_state3 = SERVER_REGISTERING;

  /* Register with the server with a random integer id. Reply will be received
   * by on_server_message() */
  hello = g_strdup_printf ("Controller_video hello %s", g_ctx.u_id);
  soup_websocket_connection_send_text (ws_conn, hello);
  g_free (hello);

  return TRUE;
}

static void on_server_closed (SoupWebsocketConnection * conn G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED)
{
  app_state1 = SERVER_CLOSED;
  app_state2 = SERVER_CLOSED;
  app_state3 = SERVER_CLOSED;
  cleanup_and_quit_loop ("Server connection closed", 0);
}

static void on_server_connected(SoupSession * session, GAsyncResult * res, gpointer user_data)
{
  GError *error = NULL;

  ws_conn = soup_session_websocket_connect_finish (session, res, &error);
  if (error) {
    cleanup_and_quit_loop (error->message, SERVER_CONNECTION_ERROR);
    g_error_free (error);
    return;
  }

  g_assert_nonnull (ws_conn);

  app_state1 = SERVER_CONNECTED;
  app_state2 = SERVER_CONNECTED;
  app_state3 = SERVER_CONNECTED;
  g_print ("Connected to signalling server\n");

  g_signal_connect (ws_conn, "closed", G_CALLBACK (on_server_closed), NULL);
  g_signal_connect (ws_conn, "message", G_CALLBACK (on_server_message), NULL);
  g_print ("app state == SERVER_CONNECTED\n"); 

  /* Register with the server so it knows about us and can accept commands */
  register_with_server();
}

/*
 * Connect to the signalling server. This is the entrypoint for everything else.
 */
static void connect_to_websocket_server_async () {
  SoupLogger *logger;
  SoupMessage *message;
  SoupSession *session;
  const char *https_aliases[] = {"wss", NULL};

  session = soup_session_new_with_options (SOUP_SESSION_SSL_STRICT, !disable_ssl,
      SOUP_SESSION_SSL_USE_SYSTEM_CA_FILE, TRUE,
      //SOUP_SESSION_SSL_CA_FILE, "/etc/ssl/certs/ca-bundle.crt",
      SOUP_SESSION_HTTPS_ALIASES, https_aliases, NULL);

  logger = soup_logger_new (SOUP_LOGGER_LOG_BODY, -1);
  soup_session_add_feature (session, SOUP_SESSION_FEATURE (logger));
  g_object_unref (logger);

  message = soup_message_new (SOUP_METHOD_GET, g_ctx.url);

  g_print ("Connecting to server...\n");

  /* Once connected, we will register */
  soup_session_websocket_connect_async (session, message, NULL, NULL, NULL, (GAsyncReadyCallback) on_server_connected, message);
  app_state1 = SERVER_CONNECTING;
  app_state2 = SERVER_CONNECTING;
  app_state3 = SERVER_CONNECTING;
  g_printerr ("app state == SERVER_CONNECTING\n"); 
}

static gboolean check_plugins (void) {
  int i;
  gboolean ret;
  GstPlugin *plugin;
  GstRegistry *registry;
  const gchar *needed[] = { "opus", "vpx", "nice", "webrtc", "dtls", "srtp",
      "rtpmanager", "videotestsrc", "audiotestsrc", NULL};

  registry = gst_registry_get ();
  ret = TRUE;
  for (i = 0; i < g_strv_length ((gchar **) needed); i++) {
    plugin = gst_registry_find_plugin (registry, needed[i]);
    if (!plugin) {
      g_print ("Required gstreamer plugin '%s' not found\n", needed[i]);
      ret = FALSE;
      continue;
    }
    gst_object_unref (plugin);
  }
  return ret;
}

void checkAndConnect (const gchar *s_url, const gchar *u_id) {
  g_ctx.url = s_url;
  g_ctx.u_id = u_id;
  
	if (check_plugins ()) {
	    /* Disable ssl when running a localhost server, because
	    * it's probably a test server with a self-signed certificate */
	
		GstUri *uri = gst_uri_from_string (s_url);
		if ( g_strcmp0 ("localhost", gst_uri_get_host (uri)) == 0 || g_strcmp0 ("127.0.0.1", gst_uri_get_host (uri)) == 0 )
		  disable_ssl = TRUE;
		gst_uri_unref (uri);
		connect_to_websocket_server_async();
	}

	// connect_to_websocket_server_async();
}
