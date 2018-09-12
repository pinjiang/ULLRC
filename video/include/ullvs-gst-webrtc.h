#ifndef ULLVS_GST_WEBRTC_H
#define ULLVS_GST_WEBRTC_H

#define GST_USE_UNSTABLE_API
#define STUN_SERVER " stun-server=stun://stun.l.google.com:19302 "
#define RTP_CAPS_OPUS "application/x-rtp,media=audio,encoding-name=OPUS,payload="
#define RTP_CAPS_VP8 "application/x-rtp,media=video,encoding-name=VP8,payload="
#define RTP_CAPS_VP9 "application/x-rtp,media=video,encoding-name=VP9,payload="

enum AppState {
  APP_STATE_UNKNOWN = 0,
  APP_STATE_ERROR = 1, /* generic error */
  SERVER_CONNECTING = 1000,
  SERVER_CONNECTION_ERROR,
  SERVER_CONNECTED, /* Ready to register */
  SERVER_REGISTERING = 2000,
  SERVER_REGISTRATION_ERROR,
  SERVER_REGISTERED, /* Ready to call a peer */
  SERVER_CLOSED, /* server connection closed by us or the server */
  PEER_CONNECTING = 3000,
  PEER_CONNECTION_ERROR,
  PEER_CONNECTED,
  PEER_CALL_NEGOTIATING = 4000,
  PEER_CALL_ERROR,
};

gboolean cleanup_and_quit_loop (const gchar * msg, enum AppState state);
void checkAndConnect (const gchar *s_url, const gchar *u_id);
gboolean setup_call (void);


#endif