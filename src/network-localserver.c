#include <glib.h>
#include <gio/gio.h>
#include "network-localserver.h"

/* this include were for Spicyyy*/
/*#include "channel-port.h"*/
/*#include "spice-channel-priv.h"*/
#include <spice-client-gtk.h>

#define NETWORK_LOCAL_SERVER_BUFFER_SIZE    G_MAXUINT16
/* <GLOBAL> */
gchar *networkLocalServerClassName="NetworkLocalServer";
gchar *conduitStartPrefix="io.";
gchar *g_tempFolder=NULL;
/* </GLOBAL> */
#define CLIENT_READBUFFER_SIZE 64*1024



typedef struct _client {
    GSocketConnection   *socketConnection;
    GInputStream        *inputStream;
    GOutputStream       *outputStream;
    GIOChannel          *channel;
    gchar               *readBuffer;
    gsize               bufferSize;
} Client;

struct _NetworkLocalServerPrivate {
    /*PROPERTIES*/
    gchar               *name; // socket name to listen
    SpicePortChannel    *portChannel;
    gboolean            hasClient;
    /* private */
    GSocketService      *socketService;
    /* singleton client*/
    Client              client;

};

G_DEFINE_TYPE_WITH_PRIVATE(NetworkLocalServer, network_local_server, G_TYPE_OBJECT)

enum prop {
    PROP_0,
    PROP_NAME,
    PROP_HASCLIENT,
    PROP_CHANNEL,
};

/* INSTANCE */
static void network_local_server_init(NetworkLocalServer *obj) {
    PRINT_DEBUG("[+++] new NetworkLocalServer");
    obj->priv = network_local_server_get_instance_private(obj);
    NetworkLocalServerPrivate *p = obj->priv;
    p->name=NULL;
    p->portChannel=0;
    p->socketService=0;
    p->hasClient=FALSE;
    p->client.socketConnection = 0;
    p->client.inputStream = 0;
    p->client.outputStream = 0;
    p->client.channel = 0;
}


static void network_local_server_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    NetworkLocalServerPrivate *p = NETWORK_LOCAL_SERVER(object)->priv;
    switch (prop_id)  {
        case PROP_NAME:         g_value_set_string (value, p->name);            break;
        case PROP_HASCLIENT:    g_value_set_boolean(value, p->hasClient);     break;
        case PROP_CHANNEL:      g_value_set_pointer(value, p->portChannel);     break;
        default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);    break;
    }
}

static void network_local_server_finalize(GObject *object) {
    PRINT_DEBUG("[---] delete NetworkLocalServer()");
    NetworkLocalServerPrivate *p = NETWORK_LOCAL_SERVER(object)->priv;
    if (p->name)    g_free(p->name);
    /* parent::finalize() */
    if (G_OBJECT_CLASS(network_local_server_parent_class)->finalize)    G_OBJECT_CLASS(network_local_server_parent_class)->finalize(object);
}


static void network_local_server_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    NetworkLocalServerPrivate *p = NETWORK_LOCAL_SERVER(object)->priv;
    switch (prop_id)  {
        case PROP_NAME:
            if (p->name)    g_free(p->name);
            p->name = g_strdup(g_value_get_string(value));
            break;
        case PROP_CHANNEL:
            p->portChannel = g_value_get_pointer(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}


/* CLASS */
static void network_local_server_class_init(NetworkLocalServerClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    /*METHODS */
    gobject_class->finalize     = network_local_server_finalize;
    gobject_class->get_property = network_local_server_get_property;
    gobject_class->set_property = network_local_server_set_property;
    /* PROPERTIES */
    g_object_class_install_property(gobject_class, PROP_NAME,       g_param_spec_string("name","Name","Name",NULL,G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_HASCLIENT,  g_param_spec_boolean("hasclient","HasClient","HasClient",FALSE,G_PARAM_READABLE|G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_CHANNEL,    g_param_spec_pointer("channel","Channel","Channel",G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS));
}

/*
GObject *network_local_server_new(void) {
    return g_object_new(NETWORK_TYPE_LOCAL_SERVER,0);
}


void network_local_server_test(NetworkLocalServer *o) {
    NetworkLocalServerPrivate *priv;
    g_return_if_fail (o != NULL);
    priv = NETWORK_LOCAL_SERVER_GET_PRIVATE(o);
    printf ("Hello, %s!\n", priv->m_name);
}

*/

gboolean newClientConnection_cb(GSocketService *service, GSocketConnection *connection, GObject *src_object, gpointer user_data);
gboolean clientDataReady_cb(GIOChannel *source, GIOCondition cond, gpointer networkLocalServerObject);


gboolean network_local_server_start_listener(GObject *networkLocalServerObject) {
    NetworkLocalServerPrivate *p = NETWORK_LOCAL_SERVER(networkLocalServerObject)->priv;
    GSocketService *service = p->socketService = g_socket_service_new();
    gchar *sock_path = p->name;
    GError *error = NULL;
    if (g_file_test(sock_path,G_FILE_TEST_EXISTS)) {
        g_unlink(sock_path);
    }
    GSocketAddress *address = g_unix_socket_address_new(sock_path);
    if (!address) {
        PRINT_DEBUG("[LOCALSERVER] %s Failed to retrieve socket address : %s", sock_path, error->message);
        return FALSE;
    }
    if (!g_socket_listener_add_address(G_SOCKET_LISTENER(service), address, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT, NULL, NULL, &error)) {
        PRINT_DEBUG("[LOCALSERVER] %s Failed to bind to Unix socket : %s", sock_path, error->message);
        g_error_free(error);
        return FALSE;
    } else {
        PRINT_DEBUG("[LOCALSERVER] %s  Listening...",sock_path);
    }
    g_object_unref (address);
    g_signal_connect_object (service, "incoming" , G_CALLBACK(newClientConnection_cb), networkLocalServerObject, G_CONNECT_AFTER);
    g_socket_service_start(service);
    return TRUE;
}

void network_local_server_stop_listener(GObject *networkLocalServerObject)  {
    NetworkLocalServerPrivate *p = NETWORK_LOCAL_SERVER(networkLocalServerObject)->priv;
    g_socket_service_stop(p->socketService);
    unlink(p->name); /* delete socket file */
}

void clientDelete(GObject *networkLocalServerObject)   {
    NetworkLocalServerPrivate *p = NETWORK_LOCAL_SERVER(networkLocalServerObject)->priv;
    PRINT_DEBUG("[CLIENT] DELETION (%s)",p->name);
    /*g_object_unref(p->client.channel);*/
    p->client.channel = 0;
    g_object_unref (p->client.socketConnection);                  /* Drop last reference on connection */
    p->client.socketConnection = 0;
    p->hasClient=FALSE;
    p->client.inputStream = 0;
    p->client.outputStream = 0;
    g_free(p->client.readBuffer);
    p->client.readBuffer =NULL;
}
void conduitWriteDataFinished_cb(GObject *sender, GAsyncResult *res, gpointer networkLocalServerObject) {
    NetworkLocalServerPrivate *p = NETWORK_LOCAL_SERVER(networkLocalServerObject)->priv;
    GError *error = NULL;
    gssize size = spice_port_channel_write_finish(p->portChannel, res, &error);
    /*if ERROR*/
    PRINT_DEBUG("[SENT ] %ld bytes",size);
    if (error) {
        PRINT_DEBUG("[ERROR] Conduit write error: %s", error->message);
    }
}

void printLocalServerInfo(gpointer networkLocalServerObject) {
    NetworkLocalServerPrivate *p = NETWORK_LOCAL_SERVER(networkLocalServerObject)->priv;
    PRINT_DEBUG("PORT_CHANNEL_INFO [%p] buffer %p [%ld] ",(void*)p->portChannel, (void*)p->client.readBuffer, p->client.bufferSize);
    /*SpiceChannelPrivate *priv = SPICE_CHANNEL(p->portChannel)->priv;
    int fd = priv->fd;
    PRINT_DEBUG("CHANNEL_FD [%d] ",fd);*/
}

void conduitWriteData(gpointer networkLocalServerObject) {
    NetworkLocalServerPrivate *p = NETWORK_LOCAL_SERVER(networkLocalServerObject)->priv;
//    printLocalServerInfo(networkLocalServerObject);
    spice_port_channel_write_async(p->portChannel, p->client.readBuffer, p->client.bufferSize, 0, conduitWriteDataFinished_cb, networkLocalServerObject);
}

gboolean clientDataReady_cb(GIOChannel *clientChannel, GIOCondition cond, gpointer networkLocalServerObject)     {
    NetworkLocalServerPrivate *p = NETWORK_LOCAL_SERVER(networkLocalServerObject)->priv;
    GError *error = NULL;
    gsize bytesRead;
    gchar *currBuf;
    gsize currBufCapacity;

    if (cond & (G_IO_HUP)) {
        PRINT_DEBUG("[CLIENT] %s disconnected!",p->name);
        clientDelete(networkLocalServerObject);
        return FALSE;
    }
    if (cond & (G_IO_ERR)) {
        PRINT_DEBUG("[CLIENT] %s connection error!",p->name);
        clientDelete(networkLocalServerObject);
        return FALSE;
    }
    currBuf = p->client.readBuffer;
    currBufCapacity = CLIENT_READBUFFER_SIZE;
    p->client.bufferSize=0;
    do {
        GIOStatus result = g_io_channel_read_chars(clientChannel, currBuf, currBufCapacity, &bytesRead, &error);
        if (result == G_IO_STATUS_ERROR) {
            PRINT_DEBUG("[CLIENT] %s ERROR reading: %s\n",p->name, error->message);
            clientDelete(networkLocalServerObject);
            return FALSE;                                           /* Remove the event source */
        }
        if (result == G_IO_STATUS_EOF) {
            PRINT_DEBUG("[CLIENT] %s EOF",p->name);
            clientDelete(networkLocalServerObject);
            return FALSE;
        }
        p->client.bufferSize += bytesRead; /*size of data appended*/
        currBuf+=bytesRead;                /*update the pointer to append next reading*/
        currBufCapacity-= bytesRead;       /*obviously buffer is smaller now*/
        if (currBufCapacity<=0) break;
    } while (bytesRead);

    PRINT_DEBUG("[DATA-CLIENT] %s <== bytes %ld",p->name, p->client.bufferSize);
    PRINT_DEBUG("DATA[%ld] %s", p->client.bufferSize, dumpBufferHex(p->client.readBuffer ,p->client.bufferSize) );
    conduitWriteData(networkLocalServerObject);
    return TRUE;
}

void clientNewFromConnection(GObject *networkLocalServerObject, GSocketConnection *clientConnection ) {

    NetworkLocalServerPrivate *p = NETWORK_LOCAL_SERVER(networkLocalServerObject)->priv;
    p->client.socketConnection = g_object_ref (clientConnection); /* keep connection valid */
    p->client.inputStream = g_io_stream_get_input_stream(G_IO_STREAM(p->client.socketConnection));
    p->client.outputStream = g_io_stream_get_output_stream(G_IO_STREAM(p->client.socketConnection));
    GSocket *socket = g_socket_connection_get_socket(clientConnection);
    gint fd = g_socket_get_fd(socket);
    PRINT_DEBUG("[CLIENT] NEW (fd: %d): %s",fd, p->name);
    p->client.channel = g_io_channel_unix_new(fd);
    /* Pass connection as user_data to the watch callback */
    g_io_add_watch(p->client.channel, G_IO_IN | G_IO_HUP | G_IO_ERR, (GIOFunc) clientDataReady_cb, networkLocalServerObject);
    g_io_channel_set_encoding(p->client.channel, NULL, NULL); /* RAW DATA channel*/
    p->client.readBuffer = g_malloc(CLIENT_READBUFFER_SIZE);
    p->hasClient = TRUE;
}

gboolean newClientConnection_cb(GSocketService *service, GSocketConnection *clientConnection, GObject *src_object, gpointer networkLocalServerObject)   {
    NetworkLocalServerPrivate *p = NETWORK_LOCAL_SERVER(networkLocalServerObject)->priv;
    PRINT_DEBUG("[INCOMING] new connection request to %s",p->name);
    if (p->hasClient) { /*  onReturn connection will be unRef'd and so detroyed */
        PRINT_DEBUG("[LOCALSERVER] %s One Client Only!",p->name);
        return TRUE;
    }
    clientNewFromConnection(networkLocalServerObject, clientConnection);
    return TRUE;
}

void clientWriteDataFinished_cb(GObject *sender, GAsyncResult *res, gpointer user_data) {
    NetworkLocalServerPrivate *p = NETWORK_LOCAL_SERVER(user_data)->priv;
    GError *error = NULL;
    gsize bytesWritten;
    /*you can use G_OUTPUT_STREAM (sender)*/
    gboolean success = g_output_stream_write_all_finish(p->client.outputStream, res, &bytesWritten, &error);
    if (!success) {
        PRINT_DEBUG("[ERROR] %s | writing Client data (%s)",p->name, error->message);
        g_error_free(error);
    }
}

/* channel-port-data-Signal  ->  network_local_server_dispatch_data */
void        network_local_server_dispatch_data(GObject *networkLocalServerObject, gpointer buffer, gsize bufferSize) {
    NetworkLocalServerPrivate *p = NETWORK_LOCAL_SERVER(networkLocalServerObject)->priv;
    if (!p->hasClient) {
        PRINT_DEBUG("[LOCALSERVER] %s no client to dispatch data",p->name);
        return;
    }
    g_output_stream_write_all_async(p->client.outputStream, buffer, bufferSize, G_PRIORITY_DEFAULT, 0, clientWriteDataFinished_cb, networkLocalServerObject);
}

/*  */
gboolean    isConduitChannelName(const gchar *name) {
    return g_str_has_prefix(name, conduitStartPrefix);
}

gboolean    isConduitChannel(GObject* channel) {
    SpiceChannel *spiceChannel = SPICE_CHANNEL(channel);
    gchar *name = NULL;
    g_object_get(spiceChannel, "port-name", &name,NULL);
    gboolean result = isConduitChannelName(name);
    g_free(name);
    return result;
}


gchar* dumpBufferHex(const gchar * buffer, gsize bufferSize)   {
    static gchar out[512]; /*same buffer */
    gsize maxSize = 512;
    const gchar* pin = buffer;
    const gchar* hex = "0123456789ABCDEF";
    char * pout = out;
    if (bufferSize > 24) bufferSize = 24;
    for( ; pin < buffer+bufferSize; pout +=3, pin++){
        pout[0] = hex[(*pin>>4) & 0xF];
        pout[1] = hex[ *pin     & 0xF];
        pout[2] = ':';
        if (pout + 3 - out > maxSize)   break;
    }
    pout[-1] = 0;
    return out;
}

/*FOLDER CREATION #todo: extract Create/destroyTempFolder to its own unit */

gboolean CreateTempFolder(void) {
    g_tempFolder = g_strdup_printf("%s/%s-%d",g_get_tmp_dir(), "remote-viewer", getpid());
    if (g_mkdir_with_parents(g_tempFolder, S_IRWXG | S_IRWXU | S_IRWXO) == -1) {
        PRINT_DEBUG("TEMP directory creation failed");
        return FALSE;
    }
    PRINT_DEBUG("TEMP directory successfully created!");
    return TRUE;
}

gboolean DestroyTempFolder(void) {
    if (g_rmdir(g_tempFolder) == -1)     {
        PRINT_DEBUG("~TEMP directory deletion failed");
        g_free(g_tempFolder);
        return FALSE;
    }
    PRINT_DEBUG("~TEMP directory successfully removed!");
    g_free(g_tempFolder);
    return TRUE;
}
