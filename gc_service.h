/* 
 * 3dyne Legacy Engine GPL Source Code
 * 
 * Copyright (C) 1996-2012 Matthias C. Berger & Simon Berger.
 * 
 * This file is part of the 3dyne Legacy Engine GPL Source Code ("3dyne Legacy
 * Engine Source Code").
 *   
 * 3dyne Legacy Engine Source Code is free software: you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 * 
 * 3dyne Legacy Engine Source Code is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * 3dyne Legacy Engine Source Code.  If not, see
 * <http://www.gnu.org/licenses/>.
 * 
 * In addition, the 3dyne Legacy Engine Source Code is also subject to certain
 * additional terms. You should have received a copy of these additional terms
 * immediately following the terms and conditions of the GNU General Public
 * License which accompanied the 3dyne Legacy Engine Source Code.
 * 
 * Contributors:
 *     Matthias C. Berger (mcb77@gmx.de) - initial API and implementation
 *     Simon Berger (simberger@gmail.com) - initial API and implementation
 */ 



// gc_service.h

#ifndef gc_service_h
#define gc_service_h

#include "defs.h"

extern fp_t	gc_fpfps;
extern int	gc_tps;
extern int	gc_tpf;

extern int	gc_ismenu;
extern int	gc_isshell;
//extern int	gc_ismap;

extern int	gc_renderframe;

extern int	gc_textcursor;
extern char	*gc_graphcursor;

extern int	gc_wftimebase;

//extern int	gci_exit;
extern int	gci_domuch;
//extern int	gci_spawnmap;
//extern int	gci_dropmap;dont
//extern int	gci_restartgl;

//extern int	gci_startlocalgame;
//extern int	gci_startremotegame;

extern sh_var_t	*gc_mousescale, *gc_invmouse;

extern int	gc_fps;
extern playermod_t	pmod;

extern unsigned int	ms_wfbegin;
extern unsigned int	ms_rfbegin;
#include "gc_defs.h"
extern gc_state_t	*gc_state;

namespace mp {
class queue;
}
namespace gs {
class interpreter;
}
namespace pan {
class gl_context;
}


//namespace legacy {
//class game_control {
//private:
//    //
//    // machine state
//    //
//    gcState			state;
//    msec_t			time;
//    msec_t			timeout;
//    void (*timeout_func)(struct gc_state_s *);

//    unique_t		id_first;
//    unique_t		id_last;
//    unique_t		id_next;

//    // async state changes
//    bool_t		u_start_demo;	// init game for demo
//    bool_t		u_start_single;	// init game for singel player
//    bool_t		u_connect_public_server;	// init game as network client
//    bool_t		u_connect_failed;
//    bool_t		u_start_public_server;		// init game as network server

//    bool_t		u_start_game;	// start game
//    bool_t		u_drop_game;	// stop game
//    bool_t		u_quit;

//    // current state
//    bool_t		gc_is_demo;
//    bool_t		gc_is_single;
//    bool_t		gc_has_server_connection;
//    bool_t		gc_is_public_server;
//    bool_t		gc_is_loading;
//    bool_t		gc_in_game;

//    // local port
//    gcUdpState		udp_state;
//    byte_iter_t		udp_send_bi;
//    byte_iter_t		udp_recv_bi;
//    char			udp_send_buf[GC_MAX_NET_UDP_SIZE];
//    char			udp_recv_buf[GC_MAX_NET_UDP_SIZE];

//    int			local_sock;
//    struct sockaddr_in	local_port;

//    struct sockaddr_in	addr_last_recv;
//    gc_host_addr_t		host_last_recv;

//    // connected hosts

//    gc_connected_client_t	connected_cl_tbl[GC_STATE_MAX_CONNECTED_CLIENTS];

//    gc_connected_server_t	connected_sv;

//    // the game map
//    g_map_t			map;

//    // the game/physic server
//    g_server_t		sv;

//    // the local game/visualization client
//    g_client_t		cl;

//    //
//    unique_t		id_bind_input;
//    unique_t		id_bind_view;

//    // event queues and their buffers
//    byte_iter_t		bi_server;		// queue for input => server
//    byte_iter_t		bi_client;
//    byte_iter_t		bi_render;		// queue for client=> render

//    char			buf_server[GC_BUFFER_SIZE_SERVER];
//    char			buf_client[GC_BUFFER_SIZE_CLIENT];
//    char			buf_render[GC_BUFFER_SIZE_RENDER];

//    // from input
//    fp_t		i_lat, i_lon;
//    bool_t		i_use;
//    bool_t		i_jump;
//    bool_t		i_shoot;
//    fp_t		i_speed;
//    fp_t		i_strafespeed;
//};


//}

void GC_MainLoop();
void GC_MakeMainLoop();
void GC_CheckInit();
void GC_ClearInitFlags();

void GC_FillTime();

void GC_GiveBackTime();

void GC_DoRenderFrame();

void GC_LoadGraphCursor( void );
void GC_DoRFInputUpdate( void );  // do renderframe input
void GC_DoRFDraw( void );

//
// gc_service.c
//
bool_t GC_NetOpenLocalPort( gc_state_t *state, bool_t auto_port );
void GC_NetCloseLocalPort( gc_state_t *state );
byte_iter_t * GC_NetRecvFromLocalPort( gc_state_t *state );
byte_iter_t * GC_NetSendInit( gc_state_t *state );
void GC_NetSendFromLocalPort( gc_state_t *state, gc_host_addr_t *host );
void GC_NetHandleLocalPort( gc_state_t *gc );

unique_t GC_GetUniqueID( gc_state_t *state );
void GC_AddPlayer( unique_t id );
void GC_RemovePlayer( unique_t id );

void GC_BindControl( unique_t id );

//
// gc_util.c
//

void GC_InitHostAddrFromInetAddr( gc_host_addr_t *haddr, struct sockaddr_in *inet_addr );
bool_t GC_HostAddrCompare( gc_host_addr_t *haddr, gc_host_addr_t *other );
struct sockaddr_in * GC_HostAddrGetInetAddr( gc_host_addr_t *haddr );

int GC_ConnectedClientTableGetEntryByHostAddr( gc_state_t *state, gc_host_addr_t *host );
int GC_ConnectedClientTableGetFreeEntry( gc_state_t *state );
void GC_ConnectedClientTableInitEntry( gc_state_t *state, int entry, gc_host_addr_t *host_last_recv );

void GC_ConnectedClientTableEntryUpdateTimeout( gc_state_t *state, int entry );
bool_t GC_ConnectedClientTableCheckTimeouts( gc_state_t *state );

bool_t GC_ConnectedServerVerify( gc_state_t *state );
int GC_ConnectedClientVerify( gc_state_t *state );

void GC_StateTimeoutKill( gc_state_t *state, void (*timeout_func)(gc_state_t *state ) );
void GC_StateTimeoutStart( gc_state_t *state, msec_t timeout_delta, void (*timeout_func)(gc_state_t *state ) );

//
// gc_ccl.c
//

void GC_ConnectedClientInit( gc_state_t *state, gc_connected_client_t *ccl, gc_host_addr_t *host );
void GC_ConnectedClientCleanUp( gc_state_t *state, gc_connected_client_t *ccl );
void GC_ConnectedClientRun( gc_state_t *state, gc_connected_client_t *ccl );
void GC_ConnectedClientNetEv( gc_state_t *state, gc_connected_client_t *ccl, netEv type, byte_iter_t *bi );



//
// gc_csv.c
//

void GC_ConnectedServerInit( gc_state_t *state, gc_connected_server_t *csv, gc_host_addr_t *host );
void GC_ConnectedServerCleanUp( gc_state_t *state, gc_connected_server_t *cs );
void GC_ConnectedServerRun( gc_state_t *state, gc_connected_server_t *csv );
void GC_ConnectedServerNetEv( gc_state_t *state, gc_connected_server_t *csv, netEv type, byte_iter_t *bi );

#endif
