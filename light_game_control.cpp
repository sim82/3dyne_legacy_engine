#include "light_game_control.h"
#include "Shared/dep_inject.h"
#include "r_interface.h"
#include "r_glbackend.h"
#include "sh_parser.h"
#include "g_map.h"
#include <cstring>

light_game_control::light_game_control( const char *map_name )
    : origin_(Vector3::ZERO)
    , lon_(0)
    , lat_(0)
{
    //SHP_StartUp();
    SHP_SetVar( "gc_map", map_name, 0 );

    map_ = new g_map_t;
    memset( map_, 0, sizeof(g_map_t));

    G_InitMap( map_ );
//    dep_inject::get_registry().add<ri_view_t>("legacy_ri_vplayer", ri_vplayer_);
//    dep_inject::get_registry().add<ri_view_t>("legacy_ri_vspectator", ri_vspectator_);
//    dep_inject::get_registry().add<ri_view_t>("legacy_ri_vsky", ri_vsky_);
}

void light_game_control::draw_frame() {
    int		i, j;
    TFUNC_ENTER;

    //
    // reset gl backend counters
    //


    matrix3_t		render_matrix;



//		ri_vplayer = gc_state->cl.ri_view_player;
//		ri_vspectator = gc_state->cl.ri_view_spectator;
//		ri_vsky = gc_state->cl.ri_view_sky;

            //
    // if we only draw lines, the color buffer have to be cleared
    //

    bool draw_tris = false;

    if ( draw_tris )
    {
        glClearColor( 0.3, 0.3, 0.3, 1.0 );
        glClear(GL_COLOR_BUFFER_BIT);
    }
#if 0
    //
    // setup render models
    //
    for ( i = 0; i < gc_state->cl.ri_model_num; i++ )
    {
        G_SetupModel( gc_state->cl.ri_models[i]->md->cs_root, gc_state->cl.ri_models[i]->ref_origin, gc_state->cl.ri_models[i]->ref_axis );
    }
#endif

#if 0
    //
    // setup render psys
    //
    for ( i = 0; i < gc_state->cl.ri_psys_num; i++ )
    {
        G_FieldParticleSystemRun( &gc_state->cl.ri_psyss[i]->fpsys, ms_rfdelta );
    }
#endif
    for ( j = 0; j < 2; j++ )
    {
#if 1
        if ( true && j == 0 )
        {
          //		__named_message( "view_sky\n" );
            vec3d_t origin;
            sky_origin_.get(origin);
            R_SetView( origin, lat_, lon_ );
            Matrix3SetupRotate( render_matrix, 0*D2R, lat_*D2R, lon_*D2R );
            R_BE_SetVertexMatrix( &render_matrix );
            R_BE_SetVertexOrigin( origin );

            goto render;
        }
#endif
        if ( true && j == 1 )
        {
          //	__named_message( "view_player\n" );
            vec3d_t origin;
            origin_.get(origin);

            R_SetView( origin, lat_, lon_ );
            Matrix3SetupRotate( render_matrix, 0*D2R, lat_*D2R, lon_*D2R );
            R_BE_SetVertexMatrix( &render_matrix );
            R_BE_SetVertexOrigin( origin );

            goto render;
        }

        continue;

    render:

        glClearDepth( 0.0 );
        glClear(GL_DEPTH_BUFFER_BIT);

        {
            // render the view
            R_Prepare();
#if 0
            if ( j != 0 )
            {

                R_SetLocalLightInfos( gc_state->cl.ri_local_light_num, gc_state->cl.ri_local_lights );

                R_SetSpriteInfos( gc_state->cl.ri_sprite_num, gc_state->cl.ri_sprites );
                R_SetPsysInfos( gc_state->cl.ri_psys_num, gc_state->cl.ri_psyss );
                R_SetHaloInfos( gc_state->cl.ri_halo_num, gc_state->cl.ri_halos );
            }

            //
            // draw render models
            //
            if ( j != 0 )
            {
                for ( i = 0; i < gc_state->cl.ri_model_num; i++ )
                {
                    G_DrawModel( gc_state->cl.ri_models[i]->md->cs_root );
                }
            }
#endif
            R_RenderView();
#if 0
            { // do sound gerecht origin and view
                fp_t	theta;
                Vec3dCopy( snd_origin, g_st->render_origin );
                theta = g_st->view_lon * D2R;
                snd_view[0] = cos( theta );
                snd_view[1] = 0.0;
                snd_view[2] = sin( theta );
            }
#endif
        }
    }

    //
    // do draw infos
    //

    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho (0.0, 1.0, 0.0, 1.0, -1.0, 1.0 );
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();
    glDisable( GL_DEPTH_TEST );
    glEnable( GL_TEXTURE_2D );
    glEnable( GL_BLEND );
#if 0
    for ( i = 0; i < gc_state->cl.di_rect_num; i++ )
    {
        di_rect_t	*rect;
        res_gltex_cache_t	*cache;
        int		texobj;

        rect = gc_state->cl.di_rects[i];
        __chkptr( rect );

        cache = ( res_gltex_cache_t * ) rect->gltex->res_cache;
        __chkptr( cache );
        texobj = cache->texobj;
        glBindTexture( GL_TEXTURE_2D, texobj );

        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

        if ( cache->comp == resGltexComponents_rgb )
        {
            glBlendFunc( GL_ONE, GL_ZERO );
        }
        else
        {
            glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
//				glBlendFunc( GL_ONE, GL_ONE );
        }

        glColor4ubv( rect->flat_color );

        glBegin( GL_TRIANGLE_FAN );
        glTexCoord2f( rect->tx, rect->ty );
        glVertex2f( rect->x, rect->y );

        glTexCoord2f( rect->tx+rect->tw, rect->ty );
        glVertex2f( rect->x+rect->w, rect->y );

        glTexCoord2f( rect->tx+rect->tw, rect->ty+rect->th );
        glVertex2f( rect->x+rect->w, rect->y+rect->h );

        glTexCoord2f( rect->tx, rect->ty+rect->th );
        glVertex2f( rect->x, rect->y+rect->h );
        glEnd();
    }
#endif
}


