#ifndef LIGHT_GAME_CONTROL_H
#define LIGHT_GAME_CONTROL_H


#include "Shared/LinearMath.h"

struct g_map_t;


class light_game_control
{
public:
    light_game_control(const char *map_name);


    void draw_frame();

    void set_origin( const Vector3 &v ) {
        origin_ = v;
    }

    void set_sky_origin( const Vector3 &v ) {
        sky_origin_ = v;
    }

    void set_lat_lon( f4_t lat, f4_t lon ) {
        lon_ = lon;
        lat_ = lat;
    }

 private:
    g_map_t			*map_;


    Vector3 origin_;
    Vector3 sky_origin_;
    f4_t lon_;
    f4_t lat_;

};

#endif // LIGHT_GAME_CONTROL_H
