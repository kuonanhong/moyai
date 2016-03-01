#ifndef _REMOTE_H_
#define _REMOTE_H_

// packet structures
typedef struct {
    float x,y;
} PacketVec2;
typedef struct {
    float x,y,z;
} PacketVec3;
typedef struct {
    float r,g,b,a;
} PacketColor;
typedef struct  {
    uint32_t prop_id; // non-zero
    uint32_t layer_id; // non-zero
    PacketVec2 loc;
    PacketVec2 scl;
    int32_t index;
    uint32_t tiledeck_id; // non-zero
    uint32_t grid_id; // 0 for nothing
    int32_t debug;
    float rot;
    uint32_t xflip; // TODO:smaller size
    unsigned int yflip;
    PacketColor color;
} PacketProp2DSnapshot;

#define MAX_PACKET_SIZE 1024

///////
// HMP: Headless Moyai Protocol
class RemoteHead;
class HMPListener : public Listener {
public:
    RemoteHead *remote_head;
    HMPListener(Network *nw, RemoteHead *rh) : Listener(nw), remote_head(rh) {};
    virtual ~HMPListener(){};
    virtual void onAccept( int newfd );    
};
class HMPConn : public Conn {
public:
    HMPConn( Network *nw, int newfd ) : Conn(nw,newfd) {};
    virtual ~HMPConn() {};
    virtual void onError( NET_ERROR e, int eno );
    virtual void onClose();
    virtual void onConnect();
    virtual void onPacket( uint16_t funcid, char *argdata, size_t argdatalen );

    // send
    void sendFile( const char *filename );
    
};
class Prop2D;
class RemoteHead {
public:
    int tcp_port;
    Network *nw;
    HMPListener *listener;
    MoyaiClient *target_moyai;
    static const int DEFAULT_PORT = 22222;
    RemoteHead( MoyaiClient *m ) : tcp_port(0), nw(0), listener(0), target_moyai(m) {
    }
    void track2D();
    bool startServer( int portnum, bool to_log_syscall = false );
    void heartbeat();
    void scanSendAllGraphicsPrerequisites( HMPConn *outco );
    void scanSendAllProp2DSnapshots( HMPConn *c );
    void notifyDeleted( Prop2D *deleted );
};


typedef enum {
    // generic
    PACKETTYPE_PING = 1,    
    // client to server 
    PACKETTYPE_C2S_GET_ALL_PREREQUISITES = 100,    
    PACKETTYPE_C2S_KEYBOARD_DOWN = 200,
    PACKETTYPE_C2S_KEYBOARD_UP = 201,    
    PACKETTYPE_C2S_MOUSE_DOWN = 202,
    PACKETTYPE_C2S_MOUSE_UP = 203,    
    PACKETTYPE_C2S_TOUCH_BEGIN = 204,
    PACKETTYPE_C2S_TOUCH_MOVE = 205,
    PACKETTYPE_C2S_TOUCH_END = 206,
    PACKETTYPE_C2S_TOUCH_CANCEL = 207,
    
    // server to client
    PACKETTYPE_S2C_PROP2D_SNAPSHOT = 400, 
    PACKETTYPE_S2C_PROP2D_LOC = 401,
    PACKETTYPE_S2C_PROP2D_GRID = 402,
    PACKETTYPE_S2C_PROP2D_INDEX = 403,
    PACKETTYPE_S2C_PROP2D_SCALE = 404,
    PACKETTYPE_S2C_PROP2D_ROT = 405,
    PACKETTYPE_S2C_PROP2D_XFLIP = 406,
    PACKETTYPE_S2C_PROP2D_YFLIP = 407,
    PACKETTYPE_S2C_PROP2D_COLOR = 408,
    PACKETTYPE_S2C_PROP2D_DELETE = 410,
    
    PACKETTYPE_S2C_LAYER_CREATE = 420,
    PACKETTYPE_S2C_LAYER_VIEWPORT = 421,
    PACKETTYPE_S2C_LAYER_CAMERA = 422,
    PACKETTYPE_S2C_VIEWPORT_CREATE = 430,
    PACKETTYPE_S2C_VIEWPORT_SIZE = 431,
    PACKETTYPE_S2C_VIEWPORT_SCALE = 432,    
    PACKETTYPE_S2C_CAMERA_CREATE = 440,
    PACKETTYPE_S2C_CAMERA_LOC = 441,
    
    PACKETTYPE_S2C_TEXTURE_CREATE = 450,
    PACKETTYPE_S2C_TEXTURE_IMAGE = 451,
    PACKETTYPE_S2C_IMAGE_CREATE = 460,
    PACKETTYPE_S2C_IMAGE_LOAD_PNG = 461,
    
    PACKETTYPE_S2C_TILEDECK_CREATE = 470,
    PACKETTYPE_S2C_TILEDECK_TEXTURE = 471,
    PACKETTYPE_S2C_TILEDECK_SIZE = 472,
    PACKETTYPE_S2C_GRID_CREATE_SNAPSHOT = 480, // Gridの情報を一度に1種類送る
    PACKETTYPE_S2C_GRID_TABLE_SNAPSHOT = 481, // Gridの水平移動各種テーブル
    PACKETTYPE_S2C_GRID_INDEX = 482, // indexが変化した。
    PACKETTYPE_S2C_FILE = 490, // ファイルを直接送信する step 1: ファイルを作成してIDを割りつける。

    PACKETTYPE_ERROR = 2000, // 何らかのエラー。エラー番号を返す
} PACKETTYPE;


class Prop2D;
class Tracker2D {
public:
    PacketProp2DSnapshot pktbuf[2]; // flip-flop    
    int cur_buffer_index;
    RemoteHead *parent_rh;
    Tracker2D(RemoteHead *rh ) : cur_buffer_index(0), parent_rh(rh) {
        memset( pktbuf, 0, sizeof(pktbuf) );
    }
    void scanProp2D( Prop2D *);
    void flipCurrentBuffer();
    size_t getDiffPacket( char *outpktbuf, size_t maxoutsize, PACKETTYPE *pkttype );
    size_t getCurrentPacket( char *outpktbuf, size_t maxoutsize );
    void notifyDeleted( Prop2D *deleted );
};
class TrackerGrid {
    
};






#endif