#ifndef LOCALSOCKET_H
#define LOCALSOCKET_H

#include <glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>

G_BEGIN_DECLS

/* NETOWRK_LOCAL_SERVER */

#define NETWORK_TYPE_LOCAL_SERVER       (network_local_server_get_type())

#define NETWORK_LOCAL_SERVER(o)         (G_TYPE_CHECK_INSTANCE_CAST ((o),   NETWORK_TYPE_LOCAL_SERVER, NetworkLocalServer))
#define NETWORK_LOCAL_SERVER_CLASS(c)   (G_TYPE_CHECK_CLASS_CAST ((c),      NETWORK_TYPE_LOCAL_SERVER, NetworkLocalServerClass))
#define NETWORK_IS_LOCAL_SERVER(o)      (G_TYPE_CHECK_INSTANCE_TYPE ((o),   NETWORK_TYPE_LOCAL_SERVER))
#define NETWORK_IS_LOCAL_SERVER_CLASS(c) (G_TYPE_CHECK_CLASS_TYPE((c),      NETWORK_TYPE_LOCAL_SERVER))
#define NETOWRK_LOCAL_SERVER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o),   NETWORK_TYPE_LOCAL_SERVER, NetworkLocalServerClass))

typedef struct _NetworkLocalServer          NetworkLocalServer;
typedef struct _NetworkLocalServerClass     NetworkLocalServerClass;
typedef struct _NetworkLocalServerPrivate   NetworkLocalServerPrivate;

struct _NetworkLocalServer {
    GObject                     parent;
    NetworkLocalServerPrivate   *priv;
};

struct _NetworkLocalServerClass {
    GObjectClass                parent_class;
};

GType       network_local_server_get_type(void);

/*
GObject*    network_local_server_new(void);
void        network_local_server_test(NetworkLocalServer *o);
*/

G_END_DECLS

#define DEBUGTAG "M4RC0"
#define PRINT_DEBUG(fmt, ...)                                   \
            g_debug(G_STRLOC " " DEBUGTAG " " fmt, ## __VA_ARGS__)

extern const gchar *networkLocalServerClassName;
extern const gchar *conduitStartPrefix;
extern gchar *g_tempFolder;

void        network_local_server_stop_listener(GObject *networkLocalServerObject);
gboolean    network_local_server_start_listener(GObject *networkLocalServerObject);
void        network_local_server_dispatch_data(GObject *networkLocalServerObject, gpointer buffer, gsize bufferSize);

gboolean    isConduitChannelName(const gchar *name);
gboolean    isConduitChannel(GObject* channel);
gchar*      dumpBufferHex(const gchar* buffer, gsize length);
gboolean    CreateTempFolder(void);
gboolean    DestroyTempFolder(void);

void clientDelete(GObject *networkLocalServerObject);
void conduitWriteDataFinished_cb(GObject *sender, GAsyncResult *res, gpointer networkLocalServerObject);
void printLocalServerInfo(gpointer networkLocalServerObject);
void conduitWriteData(gpointer networkLocalServerObject);
void clientNewFromConnection(GObject *networkLocalServerObject, GSocketConnection *clientConnection );
void clientWriteDataFinished_cb(GObject *sender, GAsyncResult *res, gpointer user_data);

#define UNUSED(x) (void)(x)

#endif // LOCALSOCKET_H
