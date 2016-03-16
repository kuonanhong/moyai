#include "common.h"
#include "client.h"
#include "Remote.h"

#include "ConvertUTF.h"



////////

void Moyai::globalInitNetwork() {
    static bool g_global_init_done = false;
    
    if( g_global_init_done ) return;
    
#ifdef WIN32
    WSADATA data;
    WSAStartup(MAKEWORD(2,0), &data);
#endif
#ifndef WIN32
    signal( SIGPIPE, SIG_IGN );
#endif        

}

void uv_run_times( int maxcount ) {
    for(int i=0;i<maxcount;i++) {
        uv_run( uv_default_loop(), UV_RUN_NOWAIT );
    }
}

///////////

void setupPacketColorReplacerShaderSnapshot( PacketColorReplacerShaderSnapshot *outpkt, ColorReplacerShader *crs ) {
    outpkt->shader_id = crs->id;
    outpkt->epsilon = crs->epsilon;
    copyColorToPacketColor( &outpkt->from_color, &crs->from_color );
    copyColorToPacketColor( &outpkt->to_color, &crs->to_color );
}

//////////////

void Tracker2D::scanProp2D( Prop2D *parentprop ) {
    PacketProp2DSnapshot *out = & pktbuf[cur_buffer_index];

    out->prop_id = target_prop2d->id;
    if( parentprop ) {
        out->layer_id = 0;
        out->parent_prop_id = parentprop->id;
    } else {
        out->layer_id = target_prop2d->getParentLayer()->id;
        out->parent_prop_id = 0;
    }
    out->loc.x = target_prop2d->loc.x;
    out->loc.y = target_prop2d->loc.y;
    out->scl.x = target_prop2d->scl.x;
    out->scl.y = target_prop2d->scl.y;
    out->index = target_prop2d->index;
    out->tiledeck_id = target_prop2d->deck ? target_prop2d->deck->id : 0;
    if( target_prop2d->grid_used_num == 0 ) {
        out->grid_id = 0;
    } else if( target_prop2d->grid_used_num == 1 ) {
        out->grid_id = target_prop2d->grids[0]->id;
    } else {
        assertmsg(false, "Tracker2D: multiple grids are not implemented yet" );
    }
    out->debug = target_prop2d->debug_id;
    out->rot = target_prop2d->rot;
    out->xflip = target_prop2d->xflip;
    out->yflip = target_prop2d->yflip;
    out->uvrot = target_prop2d->uvrot;
    out->color.r = target_prop2d->color.r;
    out->color.g = target_prop2d->color.g;
    out->color.b = target_prop2d->color.b;
    out->color.a = target_prop2d->color.a;
    out->shader_id = target_prop2d->fragment_shader ? target_prop2d->fragment_shader->id : 0;
    out->optbits = 0;
    if( target_prop2d->use_additive_blend ) out->optbits |= PROP2D_OPTBIT_ADDITIVE_BLEND;
}

void Tracker2D::flipCurrentBuffer() {
    cur_buffer_index = ( cur_buffer_index == 0 ? 1 : 0 );
}

static const int CHANGED_LOC = 0x1;
static const int CHANGED_INDEX = 0x2;
static const int CHANGED_SCL = 0x4;
static const int CHANGED_ROT = 0x8;
static const int CHANGED_XFLIP = 0x10;
static const int CHANGED_YFLIP = 0x20;
static const int CHANGED_COLOR = 0x40;
static const int CHANGED_SHADER = 0x80;
static const int CHANGED_ADDITIVE_BLEND = 0x100;


int getPacketProp2DSnapshotDiff( PacketProp2DSnapshot *s0, PacketProp2DSnapshot *s1 ) {
    int changes = 0;
    if(s0->loc.x != s1->loc.x) changes |= CHANGED_LOC;
    if(s0->loc.y != s1->loc.y) changes |= CHANGED_LOC;
    if(s0->index != s1->index ) changes |= CHANGED_INDEX;
    if(s0->scl.x != s1->scl.x) changes |= CHANGED_SCL;
    if(s0->scl.y != s1->scl.y) changes |= CHANGED_SCL;
    if(s0->rot != s1->rot ) changes |= CHANGED_ROT;
    if(s0->xflip != s1->xflip ) changes |= CHANGED_XFLIP;
    if(s0->yflip != s1->yflip ) changes |= CHANGED_YFLIP;
    if(s0->color.r != s1->color.r ) changes |= CHANGED_COLOR;
    if(s0->color.r != s1->color.r ) changes |= CHANGED_COLOR;    
    if(s0->color.r != s1->color.r ) changes |= CHANGED_COLOR;
    if(s0->color.r != s1->color.r ) changes |= CHANGED_COLOR;
    if(s0->shader_id != s1->shader_id ) changes |= CHANGED_SHADER;
    if( (s0->optbits & PROP2D_OPTBIT_ADDITIVE_BLEND) != (s1->optbits & PROP2D_OPTBIT_ADDITIVE_BLEND) ) changes |= CHANGED_ADDITIVE_BLEND;
    return changes;    
}

// send packet if necessary
bool Tracker2D::checkDiff() {
    PacketProp2DSnapshot *curpkt, *prevpkt;
    if(cur_buffer_index==0) {
        curpkt = & pktbuf[0];
        prevpkt = & pktbuf[1];
    } else {
        curpkt = & pktbuf[1];
        prevpkt = & pktbuf[0];        
    }
    return getPacketProp2DSnapshotDiff( curpkt, prevpkt );
}
void Tracker2D::broadcastDiff( bool force ) {
    if( checkDiff() || force ) {
        parent_rh->broadcastUS1Bytes( PACKETTYPE_S2C_PROP2D_SNAPSHOT, (const char*)&pktbuf[cur_buffer_index], sizeof(PacketProp2DSnapshot) );
    }
}

Tracker2D::~Tracker2D() {
    parent_rh->notifyProp2DDeleted(target_prop2d);
}
void RemoteHead::notifyProp2DDeleted( Prop2D *deleted ) {
    broadcastUS1UI1( PACKETTYPE_S2C_PROP2D_DELETE, deleted->id );
}
void RemoteHead::notifyChildCleared( Prop2D *owner_prop, Prop2D *child_prop ) {
    broadcastUS1UI2( PACKETTYPE_S2C_PROP2D_CLEAR_CHILD, owner_prop->id, child_prop->id );
}
void RemoteHead::notifyGridDeleted( Grid *deleted ) {
    broadcastUS1UI1( PACKETTYPE_S2C_GRID_DELETE, deleted->id );
}

void RemoteHead::addClient( Client *cl ) {
    Client *stored = cl_pool.get(cl->id);
    if(!stored) {
        cl->parent_rh = this;
        cl_pool.set(cl->id,cl);
    }
}
void RemoteHead::delClient( Client *cl ) {
    cl_pool.del(cl->id);
}

// Assume all props in all layers are Prop2Ds.
void RemoteHead::track2D() {
    for(int i=0;i<Moyai::MAXGROUPS;i++) {
        Layer *layer = (Layer*) target_moyai->getGroupByIndex(i);
        if(!layer)continue;
        if(layer->camera) layer->camera->onTrack(this);
        Prop *cur = layer->prop_top;
        while(cur) {
            Prop2D *p = (Prop2D*) cur;
            p->onTrack(this, NULL);
            cur = cur->next;
        }        
    }
}
// Send all IDs of tiledecks, layers, textures, fonts, viwports by scanning all props and grids.
// This occurs only when new player is comming in.
void RemoteHead::scanSendAllPrerequisites( uv_stream_t *outstream ) {
    std::unordered_map<int,Viewport*> vpmap;
    std::unordered_map<int,Camera*> cammap;
    
    // Viewport , Camera
    for(int i=0;i<Moyai::MAXGROUPS;i++) {
        Group *grp = target_moyai->getGroupByIndex(i);
        if(!grp)continue;
        Layer *l = (Layer*) grp; // assume all groups are layers
        if(l->viewport) {
            vpmap[l->viewport->id] = l->viewport;
        }
        if(l->camera) {
            cammap[l->camera->id] = l->camera;
        }        
    }
    for( std::unordered_map<int,Viewport*>::iterator it = vpmap.begin(); it != vpmap.end(); ++it ) {
        Viewport *vp = it->second;
        print("sending viewport_create id:%d sz:%d,%d scl:%f,%f", vp->id, vp->screen_width, vp->screen_height, vp->scl.x, vp->scl.y );
        sendUS1UI1( outstream, PACKETTYPE_S2C_VIEWPORT_CREATE, vp->id );
        sendUS1UI1F2( outstream, PACKETTYPE_S2C_VIEWPORT_SIZE, vp->id, vp->screen_width, vp->screen_height );
        sendUS1UI1F2( outstream, PACKETTYPE_S2C_VIEWPORT_SCALE, vp->id, vp->scl.x, vp->scl.y );
    }
    for( std::unordered_map<int,Camera*>::iterator it = cammap.begin(); it != cammap.end(); ++it ) {
        Camera *cam = it->second;
        print("sending camera_create id:%d", cam->id );
        sendUS1UI1( outstream, PACKETTYPE_S2C_CAMERA_CREATE, cam->id );
        sendUS1UI1F2( outstream, PACKETTYPE_S2C_CAMERA_LOC, cam->id, cam->loc.x, cam->loc.y );
    }
    
    // Layers(Groups) don't need scanning props
    for(int i=0;i<Moyai::MAXGROUPS;i++) {
        Layer *l = (Layer*) target_moyai->getGroupByIndex(i);
        if(!l)continue;
        print("sending layer_create id:%d",l->id);
        sendUS1UI1( outstream, PACKETTYPE_S2C_LAYER_CREATE, l->id );
        if( l->viewport ) sendUS1UI2( outstream, PACKETTYPE_S2C_LAYER_VIEWPORT, l->id, l->viewport->id);
        if( l->camera ) sendUS1UI2( outstream, PACKETTYPE_S2C_LAYER_CAMERA, l->id, l->camera->id );
    }
    
    // Image, Texture, tiledeck
    std::unordered_map<int,Image*> imgmap;
    std::unordered_map<int,Texture*> texmap;
    std::unordered_map<int,TileDeck*> tdmap;
    std::unordered_map<int,Font*> fontmap;
    std::unordered_map<int,ColorReplacerShader*> crsmap;
    
    for(int i=0;i<Moyai::MAXGROUPS;i++) {
        Group *grp = target_moyai->getGroupByIndex(i);
        if(!grp)continue;

        Prop *cur = grp->prop_top;
        while(cur) {
            Prop2D *p = (Prop2D*) cur;
            if(p->deck) {
                tdmap[p->deck->id] = p->deck;
                if( p->deck->tex) {
                    texmap[p->deck->tex->id] = p->deck->tex;
                    if( p->deck->tex->image ) {
                        imgmap[p->deck->tex->image->id] = p->deck->tex->image;
                    }
                }
            }
            if(p->fragment_shader) {
                ColorReplacerShader *crs = dynamic_cast<ColorReplacerShader*>(p->fragment_shader); // TODO: avoid using dyncast..
                if(crs) {
                    crsmap[crs->id] = crs;
                }                
            }
            for(int i=0;i<p->grid_used_num;i++) {
                Grid *g = p->grids[i];
                if(g->deck) {
                    tdmap[g->deck->id] = g->deck;
                    if( g->deck->tex) {
                        texmap[g->deck->tex->id] = g->deck->tex;
                        if( g->deck->tex->image ) {
                            imgmap[g->deck->tex->image->id] = g->deck->tex->image;
                        }
                    }
                }
            }
            TextBox *tb = dynamic_cast<TextBox*>(cur); // TODO: refactor this!
            if(tb) {
                if(tb->font) {
                    fontmap[tb->font->id] = tb->font;
                }
            }
            cur = cur->next;
        }
    }
    // Files
    for( std::unordered_map<int,Image*>::iterator it = imgmap.begin(); it != imgmap.end(); ++it ) {
        Image *img = it->second;
        if( img->last_load_file_path[0] ) {
            print("sending file path:'%s' in image %d", img->last_load_file_path, img->id );
            sendFile( outstream, img->last_load_file_path );
        }
    }
    for( std::unordered_map<int,Image*>::iterator it = imgmap.begin(); it != imgmap.end(); ++it ) {
        Image *img = it->second;
        print("sending image_create id:%d", img->id );
        sendUS1UI1( outstream, PACKETTYPE_S2C_IMAGE_CREATE, img->id );
        if( img->last_load_file_path[0] ) {
            print("sending image_load_png: '%s'", img->last_load_file_path );
            sendUS1UI1Str( outstream, PACKETTYPE_S2C_IMAGE_LOAD_PNG, img->id, img->last_load_file_path );                
        }
        if( img->width>0 && img->buffer) {
            // this image is not from file, maybe generated.
            sendUS1UI3( outstream, PACKETTYPE_S2C_IMAGE_ENSURE_SIZE, img->id, img->width, img->height );
        }
        if( img->modified_pixel_num > 0 ) {
            // modified image (includes loadPNG case)
            sendUS1UI3( outstream, PACKETTYPE_S2C_IMAGE_ENSURE_SIZE, img->id, img->width, img->height );
            broadcastUS1UI1Bytes( PACKETTYPE_S2C_IMAGE_RAW,
                                  img->id, (const char*) img->buffer, img->getBufferSize() );                
        }
    }
    for( std::unordered_map<int,Texture*>::iterator it = texmap.begin(); it != texmap.end(); ++it ) {
        Texture *tex = it->second;
        print("sending texture_create id:%d", tex->id );
        sendUS1UI1( outstream, PACKETTYPE_S2C_TEXTURE_CREATE, tex->id );
        sendUS1UI2( outstream, PACKETTYPE_S2C_TEXTURE_IMAGE, tex->id, tex->image->id );
    }
    for( std::unordered_map<int,TileDeck*>::iterator it = tdmap.begin(); it != tdmap.end(); ++it ) {
        TileDeck *td = it->second;
        print("sending tiledeck_create id:%d", td->id );
        sendUS1UI1( outstream, PACKETTYPE_S2C_TILEDECK_CREATE, td->id );
        sendUS1UI2( outstream, PACKETTYPE_S2C_TILEDECK_TEXTURE, td->id, td->tex->id );
        //        print("sendS2RTileDeckSize: id:%d %d,%d,%d,%d", td->id, sprw, sprh, cellw, cellh );        
        sendUS1UI5( outstream, PACKETTYPE_S2C_TILEDECK_SIZE, td->id, td->tile_width, td->tile_height, td->cell_width, td->cell_height );        
    }
    for( std::unordered_map<int,Font*>::iterator it = fontmap.begin(); it != fontmap.end(); ++it ) {
        Font *f = it->second;
        print("sending font id:%d path:%s", f->id, f->last_load_file_path );
        sendUS1UI1( outstream, PACKETTYPE_S2C_FONT_CREATE, f->id );
        // utf32toutf8
        sendUS1UI1Wstr( outstream, PACKETTYPE_S2C_FONT_CHARCODES, f->id, f->charcode_table, f->charcode_table_used_num );
        sendFile( outstream, f->last_load_file_path );
        sendUS1UI2Str( outstream, PACKETTYPE_S2C_FONT_LOADTTF, f->id, f->pixel_size, f->last_load_file_path );
    }
    for( std::unordered_map<int,ColorReplacerShader*>::iterator it = crsmap.begin(); it != crsmap.end(); ++it ) {
        ColorReplacerShader *crs = it->second;
        print("sending col repl shader id:%d", crs->id );
        PacketColorReplacerShaderSnapshot ss;
        setupPacketColorReplacerShaderSnapshot(&ss,crs);
        sendUS1Bytes( outstream, PACKETTYPE_S2C_COLOR_REPLACER_SHADER_SNAPSHOT, (const char*)&ss, sizeof(ss) );        
    }

    // sounds
    for(int i=0;i<elementof(target_soundsystem->sounds);i++){
        Sound *snd = target_soundsystem->sounds[i];
        if(!snd)continue;

        if( snd->last_load_file_path[0] ) {
            print("sending sound load file: %d, '%s'", snd->id, snd->last_load_file_path );
            sendUS1UI1Str( outstream, PACKETTYPE_S2C_SOUND_CREATE_FROM_FILE, snd->id, snd->last_load_file_path );
        } else if( snd->last_samples ){
            sendUS1UI1Bytes( outstream, PACKETTYPE_S2C_SOUND_CREATE_FROM_SAMPLES, snd->id,
                                    (const char*) snd->last_samples,
                                    snd->last_samples_num * sizeof(snd->last_samples[0]) );
        }
        sendUS1UI1F1( outstream, PACKETTYPE_S2C_SOUND_DEFAULT_VOLUME, snd->id, snd->default_volume );
        if(snd->isPlaying()) {
            sendUS1UI1F2( outstream, PACKETTYPE_S2C_SOUND_POSITION, snd->id, snd->getTimePositionSec(), snd->last_play_volume );
        }
    }

}

// Send snapshots of all props and grids
void RemoteHead::scanSendAllProp2DSnapshots( uv_stream_t *outstream ) {
    for(int i=0;i<Moyai::MAXGROUPS;i++) {
        Group *grp = target_moyai->getGroupByIndex(i);
        if(!grp)continue;

        Prop *cur = grp->prop_top;
        while(cur) {
            Prop2D *p = (Prop2D*) cur;

            TextBox *tb = dynamic_cast<TextBox*>(p);
            if(tb) {
                if(!tb->tracker) {
                    tb->tracker = new TrackerTextBox(this,tb);
                    tb->tracker->scanTextBox();
                }
                tb->tracker->broadcastDiff(true);
            } else {
                // prop body
                if(!p->tracker) {
                    p->tracker = new Tracker2D(this,p);
                    p->tracker->scanProp2D(NULL);
                }
                p->tracker->broadcastDiff(true);
                // grid
                for(int i=0;i<p->grid_used_num;i++) {
                    Grid *g = p->grids[i];
                    if(!g->tracker) {
                        g->tracker = new TrackerGrid(this,g);
                        g->tracker->scanGrid();                    
                    }
                    g->tracker->broadcastDiff(p, true );
                }
                // prims
                if(p->prim_drawer) {
                    if( !p->prim_drawer->tracker) p->prim_drawer->tracker = new TrackerPrimDrawer(this,p->prim_drawer);
                    p->prim_drawer->tracker->scanPrimDrawer();
                    p->prim_drawer->tracker->broadcastDiff(p, true );
                }
                
                // children
                for(int i=0;i<p->children_num;i++) {
                    Prop2D *chp = p->children[i];
                    if(!chp->tracker) {
                        chp->tracker = new Tracker2D(this,chp);
                        chp->tracker->scanProp2D(p);
                    }
                    chp->tracker->broadcastDiff(true);
                }
            }
            cur = cur->next;
        }
    }    
}

void RemoteHead::heartbeat() {
    track2D();
    uv_run_times(100);
}    
static void remotehead_on_close_callback( uv_handle_t *s ) {
    print("on_close_callback");
}
static void remotehead_on_packet_callback( uv_stream_t *s, uint16_t funcid, char *argdata, uint32_t argdatalen ) {
    Client *cli = (Client*)s->data;
    //    print("on_packet_callback. id:%d fid:%d len:%d", funcid, argdatalen );
    switch(funcid) {
    case PACKETTYPE_C2S_KEYBOARD:
        {
            uint32_t keycode = get_u32(argdata);
            uint32_t action = get_u32(argdata+4);
            uint32_t mod_shift = get_u32(argdata+8);
            uint32_t mod_ctrl = get_u32(argdata+12);
            uint32_t mod_alt = get_u32(argdata+16);
            print("kbd: %d %d %d %d %d", keycode, action, mod_shift, mod_ctrl, mod_alt );            
            Keyboard *kbd = cli->parent_rh->target_keyboard;
            if(kbd) {
                kbd->update(keycode, action, mod_shift, mod_ctrl, mod_alt );
            }
        }
        break;
    case PACKETTYPE_C2S_MOUSE_BUTTON:
        {
            uint32_t button = get_u32(argdata);
            uint32_t action = get_u32(argdata+4);
            uint32_t mod_shift = get_u32(argdata+8);
            uint32_t mod_ctrl = get_u32(argdata+12);
            uint32_t mod_alt = get_u32(argdata+16);
            print("mou: %d %d %d %d %d", button, action, mod_shift, mod_ctrl, mod_alt );
            Mouse *mou = cli->parent_rh->target_mouse;
            if(mou) {
                mou->updateButton( button, action, mod_shift, mod_ctrl, mod_alt );
            }
        }
        break;
    case PACKETTYPE_C2S_CURSOR_POS:
        {
            float x = get_f32(argdata);
            float y = get_f32(argdata+4);
            print("pos: %f %f", x,y);
            Mouse *mou = cli->parent_rh->target_mouse;
            if(mou) {
                mou->updateCursorPosition(x,y);
            }
            
        }
        break;
    default:
        print("unhandled funcid: %d",funcid);
        break;
    }
}

static void remotehead_on_read_callback( uv_stream_t *s, ssize_t nread, const uv_buf_t *inbuf ) {
    Client *cl = (Client*) s->data;
    if(nread>0) {
        bool res = parseRecord( s, &cl->recvbuf, inbuf->base, nread, remotehead_on_packet_callback );
        if(!res) {
            print("receiveData failed");
            uv_close( (uv_handle_t*)s, remotehead_on_close_callback );
            return;
        }        
    } else if( nread <= 0 ) {
        print("on_read_callback EOF. clid:%d", cl->id );
        uv_close( (uv_handle_t*)s, remotehead_on_close_callback );        
    }
}

void moyai_libuv_alloc_buffer( uv_handle_t *handle, size_t suggested_size, uv_buf_t *outbuf ) {
    *outbuf = uv_buf_init( (char*) MALLOC(suggested_size), suggested_size );
}

static void remotehead_on_accept_callback( uv_stream_t *listener, int status ) {
    if( status != 0 ) {
        print("on_accept_callback status:%d", status);
        return;
    }

    uv_tcp_t *newsock = (uv_tcp_t*) MALLOC( sizeof(uv_tcp_t) );
    uv_tcp_init( uv_default_loop(), newsock );
    if( uv_accept( listener, (uv_stream_t*) newsock ) == 0 ) {
        Client *cl = new Client(newsock, (RemoteHead*)listener->data );
        newsock->data = cl;
        cl->tcp = newsock;
        cl->parent_rh->addClient(cl);
        print("on_accept_callback. ok status:%d client-id:%d", status, cl->id );

        int r = uv_read_start( (uv_stream_t*) newsock, moyai_libuv_alloc_buffer, remotehead_on_read_callback );
        if(r) {
            print("uv_read_start: fail ret:%d",r);
        }

        cl->parent_rh->scanSendAllPrerequisites( (uv_stream_t*) newsock );
        cl->parent_rh->scanSendAllProp2DSnapshots( (uv_stream_t*) newsock);
    }
}

// return false if can't
// TODO: implement error handling
bool RemoteHead::startServer( int portnum ) {    
    tcp_port = portnum;
    int r = uv_tcp_init( uv_default_loop(), &listener );
    if(r) {
        print("uv_tcp_init failed");
        return false;
    }
    listener.data = (void*)this;
    struct sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", portnum, &addr );
    r = uv_tcp_bind( &listener, (const struct sockaddr*) &addr, SO_REUSEADDR );
    if(r) {
        print("uv_tcp_bind failed");
        return false;
    }
    r = uv_listen( (uv_stream_t*)&listener, 10, remotehead_on_accept_callback );
    if(r) {
        print("uv_listen failed");
        return false;
    }
    return true;
}

#if 0
void HMPConn::onPacket( uint16_t funcid, char *argdata, size_t argdatalen ) {
    print("HMPConn::onfunction. id:%d fid:%d len:%d",id, funcid, argdatalen );
    switch(funcid) {
    case PACKETTYPE_C2S_KEYBOARD:
        {
            uint32_t keycode = get_u32(argdata);
            uint32_t action = get_u32(argdata+4);
            uint32_t mod_shift = get_u32(argdata+8);
            uint32_t mod_ctrl = get_u32(argdata+12);
            uint32_t mod_alt = get_u32(argdata+16);
            assert(remote_head);
            print("kbd: %d %d %d %d %d", keycode, action, mod_shift, mod_ctrl, mod_alt );            
            Keyboard *kbd = remote_head->target_keyboard;
            if(kbd) {
                kbd->update(keycode, action, mod_shift, mod_ctrl, mod_alt );
            }
        }
        break;
    case PACKETTYPE_C2S_MOUSE_BUTTON:
        {
            uint32_t button = get_u32(argdata);
            uint32_t action = get_u32(argdata+4);
            uint32_t mod_shift = get_u32(argdata+8);
            uint32_t mod_ctrl = get_u32(argdata+12);
            uint32_t mod_alt = get_u32(argdata+16);
            assert(remote_head);
            print("mou: %d %d %d %d %d", button, action, mod_shift, mod_ctrl, mod_alt );
            Mouse *mou = remote_head->target_mouse;
            if(mou) {
                mou->updateButton( button, action, mod_shift, mod_ctrl, mod_alt );
            }
        }
        break;
    case PACKETTYPE_C2S_CURSOR_POS:
        {
            float x = get_f32(argdata);
            float y = get_f32(argdata+4);
            assert(remote_head);
            print("pos: %f %f", x,y);
            Mouse *mou = remote_head->target_mouse;
            if(mou) {
                mou->updateCursorPosition(x,y);
            }
            
        }
        break;
    default:
        print("unhandled funcid: %d",funcid);
        break;
    }
}

void HMPConn::sendFile( const char *filename ) {
    const size_t MAXBUFSIZE = 1024*1024*4;
    char *buf = (char*) MALLOC(MAXBUFSIZE);
    assert(buf);
    size_t sz = MAXBUFSIZE;
    bool res = readFile( filename, buf, &sz );
    assertmsg(res, "sendFile: file '%s' read error", filename );
    int r = sendUS1StrBytes( PACKETTYPE_S2C_FILE, filename, buf, sz );
    assert(r>0);
    print("sendFile: path:%s len:%d data:%x %x %x %x sendres:%d", filename, sz, buf[0], buf[1], buf[2], buf[3], r );
    FREE(buf);
}
#endif

////////////////

TrackerGrid::TrackerGrid( RemoteHead *rh, Grid *target ) : target_grid(target), cur_buffer_index(0), parent_rh(rh) {
    for(int i=0;i<2;i++) {
        index_table[i] = (int32_t*) MALLOC(target->getCellNum() * sizeof(int32_t) );
        flip_table[i] = (uint8_t*) MALLOC(target->getCellNum() * sizeof(uint8_t) );
        texofs_table[i] = (PacketVec2*) MALLOC(target->getCellNum() * sizeof(PacketVec2) );
        color_table[i] = (PacketColor*) MALLOC(target->getCellNum() * sizeof(PacketColor) );
    }
}
TrackerGrid::~TrackerGrid() {
    parent_rh->notifyGridDeleted(target_grid);
    for(int i=0;i<2;i++) {
        FREE( index_table[i] );
        FREE( flip_table[i] );
        FREE( texofs_table[i] );
        FREE( color_table[i] );
    }
}
void TrackerGrid::scanGrid() {
    for(int y=0;y<target_grid->height;y++){
        for(int x=0;x<target_grid->width;x++){
            int ind = target_grid->index(x,y);
            index_table[cur_buffer_index][ind] = target_grid->get(x,y);
            uint8_t bits = 0;
            if( target_grid->getXFlip(x,y) ) bits |= GTT_FLIP_BIT_X;
            if( target_grid->getYFlip(x,y) ) bits |= GTT_FLIP_BIT_Y;
            if( target_grid->getUVRot(x,y) ) bits |= GTT_FLIP_BIT_UVROT;
            flip_table[cur_buffer_index][ind] = bits;
            Vec2 texofs;
            target_grid->getTexOffset(x,y,&texofs);
            texofs_table[cur_buffer_index][ind].x = texofs.x;
            texofs_table[cur_buffer_index][ind].y = texofs.y;
            Color col = target_grid->getColor(x,y);
            color_table[cur_buffer_index][ind].r = col.r;
            color_table[cur_buffer_index][ind].g = col.g;
            color_table[cur_buffer_index][ind].b = col.b;
            color_table[cur_buffer_index][ind].a = col.a;            
        }
    }
}
void TrackerGrid::flipCurrentBuffer() {
    cur_buffer_index = ( cur_buffer_index == 0 ? 1 : 0 );    
}

// TODO: add a new packet type of sending changes in each cells.
bool TrackerGrid::checkDiff( GRIDTABLETYPE gtt ) { 
    char *curtbl, *prevtbl;
    int curind, prevind;
    
    if(cur_buffer_index==0) {
        curind = 0;
        prevind = 1;
    } else {
        curind = 1;
        prevind = 0;
    }
    switch(gtt) {
    case GTT_INDEX:
        curtbl = (char*) index_table[curind];
        prevtbl = (char*) index_table[prevind];
        break;
    case GTT_FLIP:
        curtbl = (char*) flip_table[curind];
        prevtbl = (char*) flip_table[prevind];
        break;
    case GTT_TEXOFS:
        curtbl = (char*) texofs_table[curind];
        prevtbl = (char*) texofs_table[prevind];
        break;
    case GTT_COLOR:
        curtbl = (char*) color_table[curind];
        prevtbl = (char*) color_table[prevind];            
        break;
    }

    size_t compsz;
    switch(gtt){
    case GTT_INDEX:
        compsz = target_grid->getCellNum() * sizeof(int32_t);
        break;
    case GTT_FLIP:
        compsz = target_grid->getCellNum() * sizeof(uint8_t);
        break;
    case GTT_TEXOFS:
        compsz = target_grid->getCellNum() * sizeof(Vec2);
        break;
    case GTT_COLOR:
        compsz = target_grid->getCellNum() * sizeof(PacketColor);
        break;   
    }
    int cmp = memcmp( curtbl, prevtbl, compsz );
    return cmp;
}

void TrackerGrid::broadcastDiff( Prop2D *owner, bool force ) {
    if( checkDiff( GTT_INDEX ) || force ) {
        broadcastGridConfs(owner); // TODO: not necessary to send every time
        parent_rh->broadcastUS1UI1Bytes( PACKETTYPE_S2C_GRID_TABLE_INDEX_SNAPSHOT, target_grid->id,
                                         (const char*) index_table[cur_buffer_index],
                                         target_grid->getCellNum() * sizeof(int32_t) );
    }
    if( checkDiff( GTT_FLIP) || force ) {
        broadcastGridConfs(owner); // TODO: not necessary to send every time
        parent_rh->broadcastUS1UI1Bytes( PACKETTYPE_S2C_GRID_TABLE_FLIP_SNAPSHOT, target_grid->id,
                                         (const char*) flip_table[cur_buffer_index],
                                         target_grid->getCellNum() * sizeof(uint8_t) );
    }
    if( checkDiff( GTT_TEXOFS ) || force ) {
        broadcastGridConfs(owner);
        parent_rh->broadcastUS1UI1Bytes( PACKETTYPE_S2C_GRID_TABLE_TEXOFS_SNAPSHOT, target_grid->id,
                                         (const char*) texofs_table[cur_buffer_index],
                                         target_grid->getCellNum() * sizeof(Vec2) );
    }
    if( checkDiff( GTT_COLOR ) || force ) {
        broadcastGridConfs(owner); // TODO: not necessary to send every time
        parent_rh->broadcastUS1UI1Bytes( PACKETTYPE_S2C_GRID_TABLE_COLOR_SNAPSHOT, target_grid->id,
                                         (const char*) color_table[cur_buffer_index],
                                         target_grid->getCellNum() * sizeof(PacketColor) );
    }
}

void TrackerGrid::broadcastGridConfs( Prop2D *owner ) {
    parent_rh->broadcastUS1UI3( PACKETTYPE_S2C_GRID_CREATE, target_grid->id, target_grid->width, target_grid->height );
    int dk_id = 0;
    if(target_grid->deck) dk_id = target_grid->deck->id; else if(owner->deck) dk_id = owner->deck->id;
    if(dk_id) parent_rh->broadcastUS1UI2( PACKETTYPE_S2C_GRID_DECK, target_grid->id, dk_id );
    parent_rh->broadcastUS1UI2( PACKETTYPE_S2C_GRID_PROP2D, target_grid->id, owner->id );    
}

/////////////

TrackerTextBox::TrackerTextBox(RemoteHead *rh, TextBox *target) : target_tb(target), cur_buffer_index(0), parent_rh(rh) {
    memset( pktbuf, 0, sizeof(pktbuf) );
    memset( strbuf, 0, sizeof(strbuf) );
}
TrackerTextBox::~TrackerTextBox() {
    parent_rh->notifyProp2DDeleted(target_tb);
}
void TrackerTextBox::scanTextBox() {
    PacketProp2DSnapshot *out = & pktbuf[cur_buffer_index];
    out->prop_id = target_tb->id;
    out->layer_id = target_tb->getParentLayer()->id;
    out->loc.x = target_tb->loc.x;
    out->loc.y = target_tb->loc.y;
    out->scl.x = target_tb->scl.x;
    out->scl.y = target_tb->scl.y;
    out->index = 0; // fixed
    out->tiledeck_id = 0; // fixed
    out->grid_id = 0; // fixed
    out->debug = target_tb->debug_id;
    out->rot = 0; // fixed
    out->xflip = 0; // fixed
    out->yflip = 0; // fixed
    out->color.r = target_tb->color.r;
    out->color.g = target_tb->color.g;
    out->color.b = target_tb->color.b;
    out->color.a = target_tb->color.a;

    size_t copy_sz = (target_tb->len_str + 1) * sizeof(wchar_t);
    assertmsg( copy_sz <= MAX_STR_LEN, "textbox string too long" );

    memcpy( strbuf[cur_buffer_index], target_tb->str, copy_sz );
    str_bytes[cur_buffer_index] = copy_sz;
    //    print("scantb: cpsz:%d id:%d s:%s l:%d cbi:%d",copy_sz, target_tb->id, target_tb->str, target_tb->len_str, cur_buffer_index );
}
bool TrackerTextBox::checkDiff() {
    PacketProp2DSnapshot *curpkt, *prevpkt;
    size_t cur_str_bytes, prev_str_bytes;
    uint8_t *cur_str, *prev_str;
    if(cur_buffer_index==0) {
        curpkt = & pktbuf[0];
        prevpkt = & pktbuf[1];
        cur_str_bytes = str_bytes[0];
        prev_str_bytes = str_bytes[1];
        cur_str = strbuf[0];
        prev_str = strbuf[1];        
    } else {
        curpkt = & pktbuf[1];
        prevpkt = & pktbuf[0];
        cur_str_bytes = str_bytes[1];
        prev_str_bytes = str_bytes[0];        
        cur_str = strbuf[1];
        prev_str = strbuf[0];                
    }
    int pktchanges = getPacketProp2DSnapshotDiff( curpkt, prevpkt );
    bool str_changed = false;
    if( cur_str_bytes != prev_str_bytes ) {
        str_changed = true;
    } else if( memcmp( cur_str, prev_str, cur_str_bytes ) ){
        str_changed = true;
    }

    if( str_changed ) {
        //        print("string changed! id:%d l:%d", target_tb->id, cur_str_bytes );
    }
    
    return pktchanges || str_changed;    
}
void TrackerTextBox::flipCurrentBuffer() {
    cur_buffer_index = ( cur_buffer_index == 0 ? 1 : 0 );        
}
void TrackerTextBox::broadcastDiff( bool force ) {
    if( checkDiff() || force ) {
        parent_rh->broadcastUS1UI1( PACKETTYPE_S2C_TEXTBOX_CREATE, target_tb->id );
        parent_rh->broadcastUS1UI2( PACKETTYPE_S2C_TEXTBOX_LAYER, target_tb->id, target_tb->getParentLayer()->id );        
        parent_rh->broadcastUS1UI2( PACKETTYPE_S2C_TEXTBOX_FONT, target_tb->id, target_tb->font->id );
        parent_rh->broadcastUS1UI1Wstr( PACKETTYPE_S2C_TEXTBOX_STRING, target_tb->id, target_tb->str, target_tb->len_str );
        parent_rh->broadcastUS1UI1F2( PACKETTYPE_S2C_TEXTBOX_LOC, target_tb->id, target_tb->loc.x, target_tb->loc.y );
        parent_rh->broadcastUS1UI1F2( PACKETTYPE_S2C_TEXTBOX_SCL, target_tb->id, target_tb->scl.x, target_tb->scl.y );        
        PacketColor pc;
        pc.r = target_tb->color.r;
        pc.g = target_tb->color.g;
        pc.b = target_tb->color.b;
        pc.a = target_tb->color.a;        
        parent_rh->broadcastUS1UI1Bytes( PACKETTYPE_S2C_TEXTBOX_COLOR, target_tb->id, (const char*)&pc, sizeof(pc) );            
    }
}

//////////////

TrackerColorReplacerShader::~TrackerColorReplacerShader() {
}
void TrackerColorReplacerShader::scanShader() {
    PacketColorReplacerShaderSnapshot *out = &pktbuf[cur_buffer_index];
    out->epsilon = target_shader->epsilon;
    copyColorToPacketColor( &out->from_color, &target_shader->from_color );
    copyColorToPacketColor( &out->to_color, &target_shader->to_color );
}
void TrackerColorReplacerShader::flipCurrentBuffer() {
    cur_buffer_index = ( cur_buffer_index == 0 ? 1 : 0 );            
}
bool TrackerColorReplacerShader::checkDiff() {
    PacketColorReplacerShaderSnapshot *curpkt, *prevpkt;
    if(cur_buffer_index==0) {
        curpkt = &pktbuf[0];
        prevpkt = &pktbuf[1];
    } else {
        curpkt = &pktbuf[1];
        prevpkt = &pktbuf[0];        
    }
    if( ( curpkt->epsilon != prevpkt->epsilon ) ||
        ( curpkt->from_color.r != prevpkt->from_color.r ) ||
        ( curpkt->from_color.g != prevpkt->from_color.g ) ||
        ( curpkt->from_color.b != prevpkt->from_color.b ) ||
        ( curpkt->from_color.a != prevpkt->from_color.a ) ||        
        ( curpkt->to_color.r != prevpkt->to_color.r ) ||
        ( curpkt->to_color.g != prevpkt->to_color.g ) ||
        ( curpkt->to_color.b != prevpkt->to_color.b ) ||
        ( curpkt->to_color.a != prevpkt->to_color.a ) ) {        
        return true;
    } else {
        return false;
    }
}
void TrackerColorReplacerShader::broadcastDiff( bool force ) {
    if( checkDiff() || force ) {
        PacketColorReplacerShaderSnapshot pkt;
        setupPacketColorReplacerShaderSnapshot( &pkt, target_shader );
        parent_rh->broadcastUS1Bytes( PACKETTYPE_S2C_COLOR_REPLACER_SHADER_SNAPSHOT, (const char*)&pkt, sizeof(pkt) );
    }
}
//////////////////
TrackerPrimDrawer::~TrackerPrimDrawer() {
    if( pktbuf[0] ) FREE(pktbuf[0]);
    if( pktbuf[1] ) FREE(pktbuf[1]);
}
void TrackerPrimDrawer::flipCurrentBuffer() {
    cur_buffer_index = ( cur_buffer_index == 0 ? 1 : 0 );                
}
void TrackerPrimDrawer::scanPrimDrawer() {
    // ensure buffer
    if( pktmax[cur_buffer_index] < target_pd->prim_num ) {
        if( pktbuf[cur_buffer_index] ) {
            print("free buf:%d", cur_buffer_index );
            FREE( pktbuf[cur_buffer_index] );
        }
        
        size_t sz = target_pd->prim_num * sizeof(PacketPrim);
        pktbuf[cur_buffer_index] = (PacketPrim*) MALLOC(sz);
        pktmax[cur_buffer_index] = target_pd->prim_num;
        print( "scanPrimDrawer: malloc buf:%d pktmax:%d sz:%d", cur_buffer_index, pktmax[cur_buffer_index], sz);
    }
    // 
    // scan
    pktnum[cur_buffer_index] = target_pd->prim_num;
    for(int i=0;i<pktnum[cur_buffer_index];i++){
        PacketPrim *outpkt = &pktbuf[cur_buffer_index][i];
        Prim *srcprim = target_pd->prims[i];
        outpkt->prim_id = srcprim->id;
        outpkt->prim_type = srcprim->type;
        outpkt->a.x = srcprim->a.x;
        outpkt->a.y = srcprim->a.y;
        outpkt->b.x = srcprim->b.x;
        outpkt->b.y = srcprim->b.y;
        copyColorToPacketColor( &outpkt->color, &srcprim->color );
        outpkt->line_width = srcprim->line_width;
    }
}
bool TrackerPrimDrawer::checkDiff() {
    PacketPrim *curary, *prevary;
    int curnum, prevnum;
    if(cur_buffer_index==0) {
        curary = pktbuf[0];
        curnum = pktnum[0];
        prevary = pktbuf[1];
        prevnum = pktnum[1];
    } else {
        curary = pktbuf[1];
        curnum = pktnum[1];        
        prevary = pktbuf[0];
        prevnum = pktnum[0];        
    }

    if( prevnum != curnum ) return true;

    for(int i=0;i<curnum;i++ ) {
        PacketPrim *curpkt = &curary[i];
        PacketPrim *prevpkt = &prevary[i];
        if( ( curpkt->prim_id != prevpkt->prim_id ) ||
            ( curpkt->prim_type != prevpkt->prim_type ) ||
            ( curpkt->a.x != prevpkt->a.x ) ||
            ( curpkt->a.y != prevpkt->a.y ) ||
            ( curpkt->b.x != prevpkt->b.x ) ||
            ( curpkt->b.y != prevpkt->b.y ) ||            
            ( curpkt->color.r != prevpkt->color.r ) ||
            ( curpkt->color.g != prevpkt->color.g ) ||
            ( curpkt->color.b != prevpkt->color.b ) ||
            ( curpkt->color.a != prevpkt->color.a ) ||
            ( curpkt->line_width != prevpkt->line_width ) ) {
            return true;
        }
    }
    return false;
}
void TrackerPrimDrawer::broadcastDiff( Prop2D *owner, bool force ) {
    if( checkDiff() || force ) {
        if( pktnum[cur_buffer_index] > 0 ) {
            //            print("sending %d prims for prop %d", pktnum[cur_buffer_index], owner->id );
            //            for(int i=0;i<pktnum[cur_buffer_index];i++) print("#### primid:%d", pktbuf[cur_buffer_index][i].prim_id );
            parent_rh->broadcastUS1UI1Bytes( PACKETTYPE_S2C_PRIM_BULK_SNAPSHOT,
                                         owner->id,
                                         (const char*) pktbuf[cur_buffer_index],
                                         pktnum[cur_buffer_index] * sizeof(PacketPrim) );
        }
    }
}
//////////////////

TrackerImage::TrackerImage( RemoteHead *rh, Image *target ) : target_image(target), cur_buffer_index(0), parent_rh(rh) {
    size_t sz = target->getBufferSize();
    for(int i=0;i<2;i++) {
        imgbuf[i] = (uint8_t*) MALLOC(sz);
        assert(imgbuf[i]);
    }
}
TrackerImage::~TrackerImage() {
    for(int i=0;i<2;i++) if( imgbuf[i] ) FREE(imgbuf[i]);
}
void TrackerImage::scanImage() {
    uint8_t *dest = imgbuf[cur_buffer_index];
    memcpy( dest, target_image->buffer, target_image->getBufferSize() );
}
void TrackerImage::flipCurrentBuffer() {
    cur_buffer_index = ( cur_buffer_index == 0 ? 1 : 0 );
}
bool TrackerImage::checkDiff() {
    uint8_t *curimg, *previmg;
    if( cur_buffer_index==0) {
        curimg = imgbuf[0];
        previmg = imgbuf[1];
    } else {
        curimg = imgbuf[1];
        previmg = imgbuf[0];
    }
    if( memcmp( curimg, previmg, target_image->getBufferSize() ) != 0 ) {
        return true;
    } else {
        return false;
    }
}
void TrackerImage::broadcastDiff( TileDeck *owner_dk, bool force ) {
    if( checkDiff() || force ) {
        //        print("TrackerImage::broadcastDiff bufsz:%d", target_image->getBufferSize() );
        parent_rh->broadcastUS1UI3( PACKETTYPE_S2C_IMAGE_ENSURE_SIZE,
                                   target_image->id, target_image->width, target_image->height );
        parent_rh->broadcastUS1UI1Bytes( PACKETTYPE_S2C_IMAGE_RAW,
                                        target_image->id, (const char*) imgbuf[cur_buffer_index], target_image->getBufferSize() );
        parent_rh->broadcastUS1UI2( PACKETTYPE_S2C_TEXTURE_IMAGE, owner_dk->tex->id, target_image->id );
        parent_rh->broadcastUS1UI2( PACKETTYPE_S2C_TILEDECK_TEXTURE, owner_dk->id, owner_dk->tex->id ); // to update tileeck's image_width/height
    }
}
////////////////////
TrackerCamera::TrackerCamera( RemoteHead *rh, Camera *target ) : target_camera(target), cur_buffer_index(0), parent_rh(rh) {
}
TrackerCamera::~TrackerCamera() {
}
void TrackerCamera::scanCamera() {
    locbuf[cur_buffer_index] = Vec2( target_camera->loc.x, target_camera->loc.y );
}
void TrackerCamera::flipCurrentBuffer() {
    cur_buffer_index = ( cur_buffer_index == 0 ? 1 : 0 );    
}
bool TrackerCamera::checkDiff() {
    Vec2 curloc, prevloc;
    if( cur_buffer_index == 0 ) {
        curloc = locbuf[0];
        prevloc = locbuf[1];
    } else {
        curloc = locbuf[1];
        prevloc = locbuf[0];
    }
    return curloc != prevloc;
}
void TrackerCamera::broadcastDiff( bool force ) {
    if( checkDiff() || force ) {
        parent_rh->broadcastUS1UI1F2( PACKETTYPE_S2C_CAMERA_LOC, target_camera->id, locbuf[cur_buffer_index].x, locbuf[cur_buffer_index].y );
    }
}
/////////////////////

void RemoteHead::broadcastUS1Bytes( uint16_t usval, const char *data, size_t datalen ) {
    for( ClientIteratorType it = cl_pool.idmap.begin(); it != cl_pool.idmap.end(); ++it ) {
        Client *cl = it->second;
        sendUS1Bytes( (uv_stream_t*)cl->tcp, usval, data, datalen );
    }
}
void RemoteHead::broadcastUS1UI1Bytes( uint16_t usval, uint32_t uival, const char *data, size_t datalen ) {
    for( ClientIteratorType it = cl_pool.idmap.begin(); it != cl_pool.idmap.end(); ++it ) {
        Client *cl = it->second;
        sendUS1UI1Bytes( (uv_stream_t*)cl->tcp, usval, uival, data, datalen );
    }
}
void RemoteHead::broadcastUS1UI1( uint16_t usval, uint32_t uival ) {
    for( ClientIteratorType it = cl_pool.idmap.begin(); it != cl_pool.idmap.end(); ++it ) {
        Client *cl = it->second;
        sendUS1UI1(  (uv_stream_t*)cl->tcp, usval, uival );
    }
}
void RemoteHead::broadcastUS1UI2( uint16_t usval, uint32_t ui0, uint32_t ui1 ) {
    for( ClientIteratorType it = cl_pool.idmap.begin(); it != cl_pool.idmap.end(); ++it ) {
        Client *cl = it->second;
        sendUS1UI2(  (uv_stream_t*)cl->tcp, usval, ui0, ui1 );
    }
}
void RemoteHead::broadcastUS1UI3( uint16_t usval, uint32_t ui0, uint32_t ui1, uint32_t ui2 ) {
    for( ClientIteratorType it = cl_pool.idmap.begin(); it != cl_pool.idmap.end(); ++it ) {
        Client *cl = it->second;
        sendUS1UI3( (uv_stream_t*)cl->tcp, usval, ui0, ui1, ui2 );
    }
}
void RemoteHead::broadcastUS1UI1Wstr( uint16_t usval, uint32_t uival, wchar_t *wstr, int wstr_num_letters ) {
    for( ClientIteratorType it = cl_pool.idmap.begin(); it != cl_pool.idmap.end(); ++it ) {
        Client *cl = it->second;
        sendUS1UI1Wstr( (uv_stream_t*)cl->tcp, usval, uival, wstr, wstr_num_letters );
    }
}
void RemoteHead::broadcastUS1UI1F2( uint16_t usval, uint32_t uival, float f0, float f1 ) {
    for( ClientIteratorType it = cl_pool.idmap.begin(); it != cl_pool.idmap.end(); ++it ) {
        Client *cl = it->second;
        sendUS1UI1F2( (uv_stream_t*)cl->tcp, usval, uival, f0, f1 );
    }
}
void RemoteHead::broadcastUS1UI1F1( uint16_t usval, uint32_t uival, float f0 ) {
    for( ClientIteratorType it = cl_pool.idmap.begin(); it != cl_pool.idmap.end(); ++it ) {
        Client *cl = it->second;
        sendUS1UI1F1( (uv_stream_t*)cl->tcp, usval, uival, f0 );
    }
}

///////////////////
char sendbuf_work[1024*1024*8];

void on_write_end( uv_write_t *req, int status ) {
    if(status==-1) {
        print("on_write_end: status:%d",status);
    }
    free(req->data);
    free(req);
}

#define SET_RECORD_LEN_AND_US1 \
    assert(totalsize<=sizeof(sendbuf_work));\
    set_u32( sendbuf_work+0, totalsize - 4 ); \
    set_u16( sendbuf_work+4, usval );

#define SETUP_UV_WRITE \
    uv_write_t *write_req = (uv_write_t*)MALLOC(sizeof(uv_write_t));\
    char *_data = (char*) MALLOC(totalsize);\
    memcpy( _data, sendbuf_work, totalsize );\
    write_req->data = _data;        \
    uv_buf_t buf = uv_buf_init(_data,totalsize);\
    int r = uv_write( write_req, s, &buf, 1, on_write_end );\
    if(r) { print("uv_write fail. %d",r); uv_close( (uv_handle_t*)s, NULL); return false; }
    
int sendUS1( uv_stream_t *s, uint16_t usval ) {
    size_t totalsize = 4 + 2;
    SET_RECORD_LEN_AND_US1;
    SETUP_UV_WRITE;
    return totalsize;    
}
int sendUS1Bytes( uv_stream_t *s, uint16_t usval, const char *bytes, uint16_t byteslen ) {
    size_t totalsize = 4 + 2 + (4+byteslen);
    SET_RECORD_LEN_AND_US1;
    set_u32( sendbuf_work+4+2, byteslen );
    memcpy( sendbuf_work+4+2+4, bytes, byteslen );
    SETUP_UV_WRITE;
    return totalsize;
}
int sendUS1UI1Bytes( uv_stream_t *s, uint16_t usval, uint32_t uival, const char *bytes, uint32_t byteslen ) {
    size_t totalsize = 4 + 2 + 4 + (4+byteslen);
    SET_RECORD_LEN_AND_US1;
    set_u32( sendbuf_work+4+2, uival );
    set_u32( sendbuf_work+4+2+4, byteslen );
    memcpy( sendbuf_work+4+2+4+4, bytes, byteslen );
    SETUP_UV_WRITE;
    return totalsize;
}
int sendUS1UI1( uv_stream_t *s, uint16_t usval, uint32_t uival ) {
    size_t totalsize = 4 + 2 + 4;
    SET_RECORD_LEN_AND_US1;
    set_u32( sendbuf_work+4+2, uival );
    SETUP_UV_WRITE;
    return totalsize;
}
int sendUS1UI2( uv_stream_t *s, uint16_t usval, uint32_t ui0, uint32_t ui1 ) {
    size_t totalsize = 4 + 2 + 4+4;
    SET_RECORD_LEN_AND_US1;
    set_u32( sendbuf_work+4+2, ui0 );
    set_u32( sendbuf_work+4+2+4, ui1 );
    SETUP_UV_WRITE;
    return totalsize;
}
int sendUS1UI3( uv_stream_t *s, uint16_t usval, uint32_t ui0, uint32_t ui1, uint32_t ui2 ) {
    size_t totalsize = 4 + 2 + 4+4+4;
    SET_RECORD_LEN_AND_US1;
    set_u32( sendbuf_work+4+2, ui0 );
    set_u32( sendbuf_work+4+2+4, ui1 );
    set_u32( sendbuf_work+4+2+4+4, ui2 );    
    SETUP_UV_WRITE;
    return totalsize;
}
int sendUS1UI5( uv_stream_t *s, uint16_t usval, uint32_t ui0, uint32_t ui1, uint32_t ui2, uint32_t ui3, uint32_t ui4 ) {
    size_t totalsize = 4 + 2 + 4+4+4+4+4;
    SET_RECORD_LEN_AND_US1;
    set_u32( sendbuf_work+4+2, ui0 );
    set_u32( sendbuf_work+4+2+4, ui1 );
    set_u32( sendbuf_work+4+2+4+4, ui2 );    
    set_u32( sendbuf_work+4+2+4+4+4, ui3 );
    set_u32( sendbuf_work+4+2+4+4+4+4, ui4 );        
    SETUP_UV_WRITE;
    return totalsize;
}
int sendUS1UI1F1( uv_stream_t *s, uint16_t usval, uint32_t uival, float f0 ) {
    size_t totalsize = 4 + 2 + 4+4;
    SET_RECORD_LEN_AND_US1;
    set_u32( sendbuf_work+4+2, uival );
    memcpy( sendbuf_work+4+2+4, &f0, 4 );
    SETUP_UV_WRITE;
    return totalsize;    
}
int sendUS1UI1F2( uv_stream_t *s, uint16_t usval, uint32_t uival, float f0, float f1 ) {
    size_t totalsize = 4 + 2 + 4+4+4;
    SET_RECORD_LEN_AND_US1;
    set_u32( sendbuf_work+4+2, uival );
    memcpy( sendbuf_work+4+2+4, &f0, 4 );
    memcpy( sendbuf_work+4+2+4+4, &f1, 4 );
    SETUP_UV_WRITE;
    return totalsize;    
}
int sendUS1F2( uv_stream_t *s, uint16_t usval, float f0, float f1 ) {
    size_t totalsize = 4 + 2 + 4+4;
    SET_RECORD_LEN_AND_US1;
    memcpy( sendbuf_work+4+2, &f0, 4 );
    memcpy( sendbuf_work+4+2+4, &f1, 4 );
    SETUP_UV_WRITE;
    return totalsize;
}
int sendUS1UI1Str( uv_stream_t *s, uint16_t usval, uint32_t uival, const char *cstr ) {
    int cstrlen = strlen(cstr);
    assert( cstrlen <= 255 );
    size_t totalsize = 4 + 2 + 4 + (1+cstrlen);
    SET_RECORD_LEN_AND_US1;
    set_u32( sendbuf_work+4+2, uival );
    set_u8( sendbuf_work+4+2+4, (unsigned char) cstrlen );
    memcpy( sendbuf_work+4+2+4+1, cstr, cstrlen );
    SETUP_UV_WRITE;
    return totalsize;
}
int sendUS1UI2Str( uv_stream_t *s, uint16_t usval, uint32_t ui0, uint32_t ui1, const char *cstr ) {
    int cstrlen = strlen(cstr);
    assert( cstrlen <= 255 );
    size_t totalsize = 4 + 2 + 4+4 + (1+cstrlen);
    SET_RECORD_LEN_AND_US1;
    set_u32( sendbuf_work+4+2, ui0 );
    set_u32( sendbuf_work+4+2+4, ui1 );    
    set_u8( sendbuf_work+4+2+4+4, (unsigned char) cstrlen );
    memcpy( sendbuf_work+4+2+4+4+1, cstr, cstrlen );
    SETUP_UV_WRITE;
    return totalsize;
}
// [record-len:16][usval:16][cstr-len:8][cstr-body][data-len:32][data-body]
int sendUS1StrBytes( uv_stream_t *s, uint16_t usval, const char *cstr, const char *data, uint32_t datalen ) {
    int cstrlen = strlen(cstr);
    assert( cstrlen <= 255 );
    size_t totalsize = 4 + 2 + (1+cstrlen) + (4+datalen);
    SET_RECORD_LEN_AND_US1;
    set_u8( sendbuf_work+4+2, (unsigned char) cstrlen );
    memcpy( sendbuf_work+4+2+1, cstr, cstrlen );
    set_u32( sendbuf_work+4+2+1+cstrlen, datalen );
    memcpy( sendbuf_work+4+2+1+cstrlen+4, data, datalen );
    SETUP_UV_WRITE;
    //    print("send_packet_str_bytes: cstrlen:%d datalen:%d totallen:%d", cstrlen, datalen, totalsize );
    return totalsize;
}
void parsePacketStrBytes( char *inptr, char *outcstr, char **outptr, size_t *outsize ) {
    uint8_t slen = get_u8(inptr);
    char *s = inptr + 1;
    uint32_t datalen = get_u32(inptr+1+slen);
    *outptr = inptr + 1 + slen + 4;
    memcpy( outcstr, s, slen );
    outcstr[slen]='\0';
    *outsize = (size_t) datalen;
}

// convert wchar_t to 
int sendUS1UI1Wstr( uv_stream_t *s, uint16_t usval, uint32_t uival, wchar_t *wstr, int wstr_num_letters ) {
#if defined(__APPLE__) || defined(__linux__)
    assert( sizeof(wchar_t) == sizeof(int32_t) );
    size_t bufsz = wstr_num_letters * sizeof(int32_t);
    UTF8 *outbuf = (UTF8*) MALLOC( bufsz + 1);
    assert(outbuf);
    UTF8 *orig_outbuf = outbuf;    
    const UTF32 *inbuf = (UTF32*) wstr;
    ConversionResult r = ConvertUTF32toUTF8( &inbuf, inbuf+wstr_num_letters, &outbuf, outbuf+bufsz, strictConversion );
    assertmsg(r==conversionOK, "ConvertUTF32toUTF8 failed:%d bufsz:%d", r, bufsz );    
#else
    assert( sizeof(wchar_t) == sizeof(int16_t) );
    size_t bufsz = wstr_num_letters * sizeof(int16_t) * 2; // utf8 gets bigger than utf16
    UTF8 *outbuf = (UTF8*) MALLOC( bufsz + 1);
    assert(outbuf);
    UTF8 *orig_outbuf = outbuf;        
    const UTF16 *inbuf = (UTF16*) wstr;
    ConversionResult r = ConvertUTF16toUTF8( &inbuf, inbuf+wstr_num_letters, &outbuf, outbuf+bufsz, strictConversion );
    assertmsg(r==conversionOK, "ConvertUTF16toUTF8 failed:%d bufsz:%d", r, bufsz );    
#endif    
    size_t outlen = outbuf - orig_outbuf;
    //    print("ConvertUTF32toUTF8 result utf8 len:%d out:'%s'", outlen, orig_outbuf );
    int ret = sendUS1UI1Bytes( s, usval, uival, (const char*) orig_outbuf, outlen );
    free(orig_outbuf);
    return ret;    
}

void sendFile( uv_stream_t *s, const char *filename ) {
    const size_t MAXBUFSIZE = 1024*1024*4;
    char *buf = (char*) MALLOC(MAXBUFSIZE);
    assert(buf);
    size_t sz = MAXBUFSIZE;
    bool res = readFile( filename, buf, &sz );
    assertmsg(res, "sendFile: file '%s' read error", filename );
    int r = sendUS1StrBytes( s, PACKETTYPE_S2C_FILE, filename, buf, sz );
    assert(r>0);
    print("sendFile: path:%s len:%d data:%x %x %x %x sendres:%d", filename, sz, buf[0], buf[1], buf[2], buf[3], r );
    FREE(buf);
}

////////////////////
Buffer::Buffer() : buf(0), size(0), used(0) {
}
void Buffer::ensureMemory( size_t sz ) {
    buf = (char*) MALLOC(sz);
    assert(buf);
    size = sz;
    used = 0;    
}

Buffer::~Buffer() {
    assert(buf);
    free(buf);
    size = used = 0;
}

bool Buffer::push( const char *data, size_t datasz ) {
    size_t left = size - used;
    if( left < datasz ) return false;
    memcpy( buf + used, data, datasz );
    used += datasz;
    //    fprintf(stderr, "buffer_push: pushed %d bytes, used: %d\n", (int)datasz, (int)b->used );
    return true;
}
bool Buffer::pushWithNum32( const char *data, size_t datasz ) {
    size_t left = size - used;
    if( left < 4 + datasz ) return false;
    set_u32( buf + used, datasz );
    used += 4;
    push( data, datasz );
    return true;
}
bool Buffer::pushU32( unsigned int val ) {
    size_t left = size - used;
    if( left < 4 ) return false;
    set_u32( buf + used, val );
    used += 4;
    //    fprintf(stderr, "buffer_push_u32: pushed 4 bytes. val:%u\n",val );
    return true;
}
bool Buffer::pushU16( unsigned short val ) {
    size_t left = size - used;
    if( left < 2 ) return false;
    set_u16( buf + used, val );
    used += 2;
    return true;
}
bool Buffer::pushU8( unsigned char val ) {
    size_t left = size - used;
    if( left < 1 ) return false;
    set_u8( buf + used, val );
    used += 1;
    return true;
}

// ALL or NOTHING. true when success
bool Buffer::shift( size_t toshift ) {
    if( used < toshift ) return false;
    if( toshift == used ) { // most cases
        used = 0;
        return true;
    }
    // 0000000000 size=10
    // uuuuu      used=5
    // ss         shift=2
    //   mmm      move=3
    memmove( buf, buf + toshift, used - toshift );
    used -= toshift;
    return true;
}


//////////////////
int Client::idgen = 1;
Client::Client( uv_tcp_t *sk, RemoteHead *rh ) : id(idgen++), tcp(sk), parent_rh(rh) {
    recvbuf.ensureMemory(1024*1024*1);
    
}
Client::~Client() {
    
}
// return false when error(to close)
bool parseRecord( uv_stream_t *s, Buffer *recvbuf, const char *data, size_t datalen, void (*funcCallback)( uv_stream_t *s, uint16_t funcid, char *data, uint32_t datalen ) ) {
    bool pushed = recvbuf->push( data, datalen );
    //    print("parseRecord: datalen:%d bufd:%d pushed:%d", datalen, recvbuf->used, pushed );

    if(!pushed) {
        print("recv buf full? close.");
        return false;
    }

    // Parse RPC
    //        fprintf(stderr, "recvbuf used:%zu\n", c->recvbuf->used );
    //        moynet_t *h = c->parent_moynet;
    while(true) { // process everything in one poll
        //            print("recvbuf:%d", c->recvbuf->used );
        if( recvbuf->used < (4+2) ) return true; // need more data from network
        //              <---RECORDLEN------>
        // [RECORDLEN32][FUNCID32][..DATA..]            
        size_t record_len = get_u32( recvbuf->buf );
        unsigned int func_id = get_u16( recvbuf->buf + 4 );

        if( recvbuf->used < (4+record_len) ) {
            //   print("need. used:%d reclen:%d", recvbuf->used, record_len );
            return true; // need more data from network
        }
        if( record_len < 2 ) {
            fprintf(stderr, "invalid packet format" );
            return false;
        }
        //        fprintf(stderr, "dispatching func_id:%d record_len:%lu\n", func_id, record_len );
        //        dump( recvbuf->buf, record_len-4);
        funcCallback( s, func_id, (char*) recvbuf->buf +4+2, record_len - 2 );
        recvbuf->shift( 4 + record_len );
        //            fprintf(stderr, "after dispatch recv func: buffer used: %zu\n", c->recvbuf->used );
        //            if( c->recvbuf->used > 0 ) dump( c->recvbuf->buf, c->recvbuf->used );
    }
}

