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



// res_gltex.c

#include <thread>
#include <condition_variable>
#include <deque>
#include <map>
#include <iostream>
#include <tuple>
#include "interfaces.h"
#include "g_shared.h"

#include "r_private.h"

#include "res_gltex.h"
#include "res_gltexdefs.h"
#include "shared/log.h"
#include "g_message_passing.h"
#include "message_passing.h"
//#define HAVE_BOOLEAN



#include <setjmp.h>

#define GLTEX_GEN_MIPMAP	 1
#define GLTEX_MIPMAP_HACK	 1

#pragma pack(push)
#pragma pack(1)



struct dds_header {
    typedef int32_t DWORD;
    struct pixel_format // DDPIXELFORMAT
    {
        int32_t dwSize;
        int32_t dwFlags;
        int32_t dwFourCC;
        int32_t dwRGBBitCount;
        int32_t dwRBitMask, dwGBitMask, dwBBitMask;
        int32_t dwRGBAlphaBitMask;
    };

//    struct caps2
//    {
//        int32_t dwCaps1;
//        int32_t dwCaps2;
//        int32_t Reserved[2];
//    };
      DWORD           dwSize;
      DWORD           dwFlags;
      DWORD           dwHeight;
      DWORD           dwWidth;
      DWORD           dwPitchOrLinearSize;
      DWORD           dwDepth;
      DWORD           dwMipMapCount;
      DWORD           dwReserved1[11];
      pixel_format ddspf;
      DWORD           dwCaps;
      DWORD           dwCaps2;
      DWORD           dwCaps3;
      DWORD           dwCaps4;
      DWORD           dwReserved2;
};
#pragma pack(pop)

class gltex_data_source {
public:
    virtual ~gltex_data_source() {}

    virtual std::pair<size_t,size_t> mipmap_size( size_t level ) = 0;
    virtual std::pair<const char *,const char *> mipmap_data( size_t level ) = 0;
    virtual size_t max_level() = 0;
};


class gltex_data_source_dds_mmap : public gltex_data_source {


public:
    gltex_data_source_dds_mmap( const char *name )
        : h_(name),
          map_(h_),
          header_((dds_header*)(map_.ptr() + sizeof(int32_t))),
          data_(map_.ptr() + sizeof(int32_t) + sizeof( dds_header ))
    {

        std::cout << "dds: " << header_->dwWidth << " " << header_->dwHeight << "\n";
        assert( header_->dwSize == sizeof(dds_header));
    }

    size_t max_level() {
        return header_->dwMipMapCount - 1;
    }

    std::pair<size_t,size_t> mipmap_size( size_t level ) {
        size_t w = header_->dwWidth;
        size_t h = header_->dwHeight;

        for( size_t i = 0; i < level; ++i ) {
            w /= 2;
            h /= 2;
        }

        return std::make_pair( w, h );
    }

    std::pair<const char *,const char *> mipmap_data( size_t level ) {
        size_t w = header_->dwWidth;
        size_t h = header_->dwHeight;

        const char *ptr = data_;
        const size_t pixel_width = (header_->ddspf.dwRGBBitCount / 8);
        for( size_t i = 0; i < level; ++i ) {
            ptr += (w * h * pixel_width);
            w /= 2;
            h /= 2;
        }
        return std::make_pair( ptr, ptr + w * h * (header_->ddspf.dwRGBBitCount / 8));
    }


private:
    ibase::file_handle h_;
    ibase::file_handle::mapping map_;
    const dds_header *header_;
    const char *data_;
};

class gltex_loader_impl {

public:
//    class job {
//    public:
//        typedef std::vector<uint8_t> data_type;

//        GLuint width_;
//        GLuint height_;
//        GLuint target_;
//        data_type data_;
//        size_t mipmap_level_;
//        size_t max_level_;

//    };
    class job {
    public:
        std::shared_ptr<gltex_data_source> src_;

        GLuint target_;
        size_t mipmap_level_;
    };

    gltex_loader_impl( mp::queue &tq )
        : tq_(tq),
          do_stop_(false)
    {
        thread_ = std::thread( [&] {
            this->run();
        });




    }
    ~gltex_loader_impl() {
        if( thread_.joinable() ) {
            stop_and_join();
        }
    }

    void run() {
        {
            std::unique_lock<std::mutex> lock(mtx_);
            sleep_cond_.wait_for(lock, std::chrono::seconds(3));
        }
        while( !do_stop_ ) {
            std::unique_lock<std::mutex> lock(mtx_);


            while( q_.empty() && !do_stop_) {
                cond_.wait(lock);
            }

            if( do_stop_ ) {
                break;
            }




            //auto & j = q_.front();
            auto it = q_.begin();
            auto &j = it->second;

            size_t w, h;
            std::tie( w, h) = j.src_->mipmap_size( j.mipmap_level_ );
            auto data = j.src_->mipmap_data( j.mipmap_level_ );
            tq_.emplace<msg::gl_upload_texture>( j.target_, w, h, j.mipmap_level_, j.src_->max_level(), std::vector<uint8_t>(data.first, data.second) );
            if( !false ) {
                auto x = std::accumulate( data.first, data.second, 0, std::plus<const char>() );
                std::cout << "x: " << int(x) << "\n";
            }
//            tq_.emplace<msg::test_tuple>( std::make_tuple( j.target_, j.width_, j.height_, j.mipmap_level_, j.max_level_, std::move( j.data_ )) );
            //q_.pop_front();
            q_.erase(it);

            sleep_cond_.wait_for(lock, std::chrono::milliseconds(10));
        }


    }

    void add( job && j ) {
        std::unique_lock<std::mutex> lock(mtx_);
        //q_.emplace_back( std::move(j) );
        //q_.emplace( j.mipmap_level_, std::move(j) );
        q_.insert( std::make_pair(j.mipmap_level_, std::move(j)) );
        lock.unlock();
        cond_.notify_one();
    }

    void stop_and_join() {
        DD_LOG << "joining gltex_loader...\n";

        std::unique_lock<std::mutex> lock(mtx_);
        do_stop_ = true;


        lock.unlock();
        cond_.notify_all();
        thread_.join();
    }

private:

    mp::queue &tq_;
    std::thread thread_;
    std::condition_variable cond_;
    std::condition_variable sleep_cond_;
    std::mutex mtx_;
    bool do_stop_;
    //std::deque<job> q_;
    std::multimap<int,job,std::greater<int>> q_;
};

static gltex_loader_impl *s_loader = 0;

typedef struct 
{
	const char	*param_text;
	GLenum	pname;
	GLint	param;
} res_gltex_param_map_t;

static res_gltex_param_map_t	tex_param_map[] = { { "mag_linear", GL_TEXTURE_MAG_FILTER, GL_LINEAR },
						    { "mag_nearest", GL_TEXTURE_MAG_FILTER, GL_NEAREST },
						    { "min_linear", GL_TEXTURE_MIN_FILTER, GL_LINEAR },
						    { "min_nearest", GL_TEXTURE_MIN_FILTER, GL_NEAREST },
						    { "min_nearest_mipmap_nearest", GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST },
						    { "min_linear_mipmap_nearest", GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST },
						    { "min_nearest_mipmap_linear", GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR },
						    { "min_linear_mipmap_linear", GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR },
						    { "wrap_s_clamp", GL_TEXTURE_WRAP_S, GL_CLAMP },
						    { "wrap_s_repeat", GL_TEXTURE_WRAP_S, GL_REPEAT },
						    { "wrap_t_clamp", GL_TEXTURE_WRAP_T, GL_CLAMP },
						    { "wrap_t_repeat", GL_TEXTURE_WRAP_T, GL_REPEAT },
						    { NULL, GL_NONE, GL_NONE } };

static bool_t	flag_mipmap_hint;

static bool_t SetTexParameterByName( char *name )
{
	res_gltex_param_map_t	*tp;

	for ( tp = tex_param_map; tp->param_text; tp++ )
	{
		if ( !strcmp( name, tp->param_text ) )
		{
			glTexParameteri( GL_TEXTURE_2D, tp->pname, tp->param );	
			break;
		}
		
		if ( strstr( name, "mipmap" ) )
		{
			flag_mipmap_hint = true;	
		}

	}

	if ( !tp->param_text )
	{
//		__error( "no texture parameter found for name '%s'\n", name );
		return false;
	}
	return true;
}

static GLuint CreateTexObject( hobj_t *resobj )
{
	GLuint		texobj;
	hpair_search_iterator_t		iter;
	hpair_t				*pair;

	flag_mipmap_hint = false;

	glGenTextures( 1, &texobj );
	glBindTexture( GL_TEXTURE_2D, texobj );
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

#if 1
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);           
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);           
                                                                                
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);       
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST); 
//#else

	InitHPairSearchIterator( &iter, resobj, "param" );
	for ( ; ( pair = SearchGetNextHPair( &iter ) ) ; )
	{
		if ( pair->value[0] == '$' )
		{
			sh_var_t	*var;

			var = SHP_GetVar( &pair->value[1] );
			if ( !var )
				__error( "can't get variable name '%s'\n", pair->value );
			if ( !SetTexParameterByName( var->string ) )
			{
				__error( "no texture parameter found for sh_var_t '%s'='%s'\n", var->key, var->string );
			}
		}
		else
		{
			if ( !SetTexParameterByName( pair->value ) )
			{
				__error( "no texture parameter found for name '%s'\n", pair->value );
			}
		}
	}
#endif

	return texobj;
}


//typedef std::vector<uint8_t> tex_data;

static GLuint s_texobj = -1;

void rgb_mipmaps( int mipmap, int width, int height, unsigned char *color_buf, std::vector<gltex_loader_impl::job> &out_jobs )
{
#if 0
    int		size;
    unsigned char	*half_buf;
    int		x, y;


    out_jobs.push_back( gltex_loader::job() );
    gltex_loader::job & j = out_jobs.back();
    j.target_ = s_texobj;
    j.width_ = width;
    j.height_ = height;
    j.mipmap_level_ = mipmap;
    j.data_.assign( color_buf, color_buf + (width * height *3));

    //glTexImage2D( GL_TEXTURE_2D, mipmap, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, color_buf );

    mipmap++;

        assert( width > 0 && height > 0 );

    //if ( width == 2 || height == 2 )
    //    return;

    if( width * height < 64 ) {
        return;
    }

    width /= 2;
    height /= 2;

    size = width * height * 3;
    half_buf = (unsigned char *)alloca( size );

    for ( x = 0; x < width; x++ )
    {
        for ( y = 0; y < height; y++ )
        {
            unsigned int	r, g, b;
            int	x2, y2, xx, yy;
            int		pb;

            r=g=b=0;

            for ( xx = 0; xx <= 1; xx++ )
            {
                for ( yy = 0; yy <= 1; yy++ )
                {
                    x2 = x*2 + xx;
                    y2 = y*2 + yy;

                    pb = (width*2*y2 + x2)*3;
                    r+=color_buf[pb];
                    g+=color_buf[pb+1];
                    b+=color_buf[pb+2];
                }
            }

            pb = (width*y + x)*3;
            half_buf[pb] = r / 4;
            half_buf[pb+1] = g / 4;
            half_buf[pb+2] = b / 4;
        }
    }

    rgb_mipmaps( mipmap, width, height, half_buf, out_jobs );
#endif
}

void Res_CreateGLTEX_rgb_mipmap( int mipmap, int width, int height, unsigned char *color_buf )
{
#if 1
#if 0
    std::vector<gltex_loader::job> jobs;
    rgb_mipmaps( 0, width, height, color_buf, jobs );

    if( !jobs.empty() ) {
        size_t max_level = jobs.size() - 1;

        for( auto it = jobs.rbegin(); it != jobs.rend(); ++it ) {
            it->max_level_ = max_level;
            s_loader->add(std::move(*it));
        }
    }
#else
    std::shared_ptr<gltex_data_source> ds{ std::make_shared<gltex_data_source_dds_mmap>( "textures/wall/con1_1.dds" )};
    size_t num_mipmap = ds->max_level() + 1;
    for( size_t i = 0; i < num_mipmap; ++i ) {
        gltex_loader_impl::job j;
        j.mipmap_level_ = i;
        j.src_ = ds;
        j.target_ = s_texobj;
        s_loader->add( std::move(j) );
    }
#endif
#else

	int		size;
	unsigned char	*half_buf;
	int		x, y;

	glTexImage2D( GL_TEXTURE_2D, mipmap, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, color_buf );

	mipmap++;

        assert( width > 0 && height > 0 );
        
	if ( width == 1 || height == 1 )
		return;

	width /= 2;
	height /= 2;

	size = width * height * 3;
	half_buf = (unsigned char *)alloca( size );

	for ( x = 0; x < width; x++ )
	{
		for ( y = 0; y < height; y++ )
		{
			unsigned int	r, g, b;
			int	x2, y2, xx, yy;
			int		pb;

			r=g=b=0;

			for ( xx = 0; xx <= 1; xx++ )
			{
				for ( yy = 0; yy <= 1; yy++ )
				{
					x2 = x*2 + xx;
					y2 = y*2 + yy;
					
					pb = (width*2*y2 + x2)*3;
					r+=color_buf[pb];					
					g+=color_buf[pb+1];
					b+=color_buf[pb+2];
				}
			}
			
			pb = (width*y + x)*3;
			half_buf[pb] = r / 4;
			half_buf[pb+1] = g / 4;
			half_buf[pb+2] = b / 4;
		}
	}

	Res_CreateGLTEX_rgb_mipmap( mipmap, width, height, half_buf );	
#endif
}


void Res_CreateGLTEX_rgba_mipmap( int mipmap, int width, int height, unsigned char *color_buf )
{
	int		size;
	unsigned char	*half_buf;
	int		x, y;
	GLenum		err;
	
	printf( "[%d:%d,%d] ", mipmap, width, height );
	glTexImage2D( GL_TEXTURE_2D, mipmap, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, color_buf );
	if ( ( err = glGetError() ) != GL_NO_ERROR )
		__error( "glTexImage2D failed: %d\n", err );
	
	mipmap++;

	if ( width == 1 || height == 1 )
		return;

	width /= 2;
	height /= 2;

	size = width * height * 4;
	half_buf = (unsigned char *)alloca( size );

	for ( x = 0; x < width; x++ )
	{
		for ( y = 0; y < height; y++ )
		{
			unsigned int	r, g, b, a;
			int	x2, y2, xx, yy;
			int		pb;

			r=g=b=a=0;

			for ( xx = 0; xx <= 1; xx++ )
			{
				for ( yy = 0; yy <= 1; yy++ )
				{
					x2 = x*2 + xx;
					y2 = y*2 + yy;
					
					pb = (width*2*y2 + x2)*4;
					r+=color_buf[pb];					
					g+=color_buf[pb+1];
					b+=color_buf[pb+2];
					a+=color_buf[pb+3];
				}
			}
			
			pb = (width*y + x)*4;
			half_buf[pb] = r / 4;
			half_buf[pb+1] = g / 4;
			half_buf[pb+2] = b / 4;
			half_buf[pb+3] = a / 4;
		}
	}

	Res_CreateGLTEX_rgba_mipmap( mipmap, width, height, half_buf );	
}

void Res_CreateGLTEX_565( int width, int height, unsigned char *color_buf )
{
	int		i, pixelnum;
	unsigned short	p16;
	unsigned char	*tmp, *ptr;

//	tmp = Image565ToImage888( color_buf, width*height );

	pixelnum = width*height; 

	tmp = (unsigned char *)alloca( pixelnum*3 );
	__chkptr( tmp );

	ptr = tmp;
	for ( i = 0; i < pixelnum; i++ )
	{
		p16 = *((unsigned short *)(color_buf));
		color_buf+=2;

		*ptr++ = ((p16>>11)&31)<<3;
		*ptr++ = ((p16>>5)&63)<<2;
		*ptr++ = ((p16&31))<<3;
	}



#if 1

	if ( flag_mipmap_hint )
	{
        Res_CreateGLTEX_rgb_mipmap( 0, width, height, color_buf );



	}
	else
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, color_buf );
	}

#endif
}


void Res_CreateGLTEX_rgb( int width, int height, unsigned char *color_buf )
{

#if 1

	if ( flag_mipmap_hint )
	{
		Res_CreateGLTEX_rgb_mipmap( 0, width, height, color_buf );	
	}
	else
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, color_buf );
	}
#endif
}

void Res_CreateGLTEX_rgba( int width, int height, unsigned char *color_buf )
{

#if 1

	if ( flag_mipmap_hint )
	{
		Res_CreateGLTEX_rgba_mipmap( 0, width, height, color_buf );	
	}
	else
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, color_buf );
	}
#endif

}



// ==================================================

res_gltex_register_t * Res_SetupRegisterGLTEX( char *path )
{
	res_gltex_register_t	*reg;
	size_t			size;

	size = strlen( path ) + 1;

	size = (size_t)&(((res_gltex_register_t *)0)->path[size]);
	reg = (res_gltex_register_t*)NEWBYTES( size );

	strcpy( reg->path, path );

	reg->resobj = NULL;

	return reg;
}


/*
  ==============================
  Res_CacheInGLTEX_tga

  ==============================
*/
res_gltex_cache_t * Res_CacheInGLTEX_tga( res_gltex_register_t *reg )
{
	ib_file_t	*h;
// 	int		filesize;
// 	char unsigned 		*buf;

	unsigned char   ident_len;
// 	unsigned char   cmap_type;
	unsigned char   image_type;
// 	unsigned short  cmap_origin;
	unsigned short  cmap_len;
	unsigned char   centry_size;
// 	unsigned short  image_xorg;
// 	unsigned short  image_yorg;
	unsigned short  image_width;
	unsigned short  image_height;
	unsigned char   pixel_size;
// 	unsigned char   image_discr;

	unsigned char	alpha, red, green, blue;
	unsigned char	*image, *ptr;

	bool_t		fliph;
	int		i, w;
	res_gltex_cache_t		*gltex;
	int		pixels;

	h = IB_Open( reg->path );
	if ( !h )
		__error( "load of '%s' failed\n", reg->path );

#if 0
	filesize = IB_GetSize( h );

	buf = alloca( filesize );
//	buf = MM_Malloc( filesize );


//	printf( "tga: alloca(%d) = %p\n", filesize, buf );
//	printf( "filesize %s: %d\n", reg->path, filesize );

	if ( !buf )
		__error( "alloca for '%s' failed\n" );


	

	IB_Read( buf, filesize, 1, h );
	IB_Close( h );
#if 0
	for ( i = 0 ; i < filesize; i++ )
	{
		printf( "%d ", (int)buf[i] );
	}
	printf( "pixels: %d\n", i );
#endif

	//
	// interpret buffer as tga
	//

	U_BeginUnpack( U_PACKED_BIN, buf, filesize );

	// extract tga header
	U_Unpacks8( &ident_len );
	U_Unpacks8( &cmap_type );
	U_Unpacks8( &image_type );
	U_Unpacks16( &cmap_origin );
	U_Unpacks16( &cmap_len );
	U_Unpacks8( &centry_size );
	U_Unpacks16( &image_xorg );
	U_Unpacks16( &image_yorg );
	U_Unpacks16( &image_width );
	U_Unpacks16( &image_height );
	U_Unpacks8( &pixel_size );
	U_Unpacks8( &image_discr );

	// skip ident
	U_UnpackSkip( ident_len );
	// skip cmap
	U_UnpackSkip( (cmap_len * centry_size) / 8 );

#endif

	ident_len = IB_GetChar( h );
	/*cmap_type = */IB_GetChar( h );
	image_type = IB_GetChar( h );
	/*cmap_origin = */IB_GetShort( h );
    cmap_len = IB_GetShort( h );
	centry_size = IB_GetChar( h );
	/*image_xorg = */IB_GetShort( h );
	/*image_yorg = */IB_GetShort( h );
	image_width = IB_GetShort( h );
	image_height = IB_GetShort( h );
	pixel_size = IB_GetChar( h );
	/*image_discr = */IB_GetChar( h );

	IB_Skip( h, ident_len );
	IB_Skip( h, (cmap_len * centry_size) / 8 );

	gltex = NEWTYPE( res_gltex_cache_t );
	gltex->width = image_width;
	gltex->height = image_height;

	fliph = true;

    
	//
	// mipmap hack
	//
	if ( gltex->width != gltex->height )
	{
        

        flag_mipmap_hint = false;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}

	if ( ( image_type == 2 ) && pixel_size == 24 /*&& image_discr == 32*/  )
	{
		// 24-bit-BGR-tga
		// read as RGB
		pixels = image_width * image_height;
		ptr = image = (unsigned char*)alloca( pixels * 3 );

		if ( fliph )
		{
			ptr = ptr + (image_width * image_height * 3 - image_width * 3);
		}

		for ( i = 0; i < image_height; i++ )
		{
			for ( w = 0; w < image_width; w++ )
			{
				// blue
//			U_Unpacks8( &blue );
				blue = IB_GetChar( h );
				// green
//			U_Unpacks8( &green );
				green = IB_GetChar( h );
				// red
//			U_Unpacks8( &red );
				red = IB_GetChar( h );
				
				*ptr = red;
				ptr++;
				*ptr = green;
				ptr++;
				*ptr = blue;
				ptr++;						
			}	      

			if ( fliph )
				ptr -= image_width * 3 * 2;
		}

		
		
		gltex->comp = resGltexComponents_rgb;
		Res_CreateGLTEX_rgb( image_width, image_height, image );

	}
	else if ( ( image_type == 2 )&& pixel_size == 32 /*&& image_discr == 40*/ )
	{
		// 32-bit-ARGB-tga
		// read as RGBA
		pixels = image_width * image_height;
		ptr = image = (unsigned char*)alloca( pixels * 4 );

		if ( fliph )
		{
			ptr = ptr + (image_width * image_height * 4 - image_width * 4);
		}

		
		for ( i = 0; i < image_height; i++ )
		{
			for ( w = 0; w < image_width; w++ )
			{
				// blue
//			U_Unpacks8( &blue );
				blue = IB_GetChar( h );
				// green
//			U_Unpacks8( &green );
				green = IB_GetChar( h );
				// red
//			U_Unpacks8( &red );
				red = IB_GetChar( h );
				// alpha
//			U_Unpacks8( &alpha );
				alpha = IB_GetChar( h );
				
				*ptr = red;
				ptr++;
				*ptr = green;
				ptr++;
				*ptr = blue;
				ptr++;	
				*ptr = alpha;
				ptr++;
			}
			

			if ( fliph )
				ptr -= image_width * 4 * 2;
		}			

		
		gltex->comp = resGltexComponents_rgba;		
		Res_CreateGLTEX_rgba( image_width, image_height, image );
		
	}
	else
	{
		if( image_type == 10 )
			__error( "RLE compression not supported. '%s'\n", reg->path );
		else
			__error( "format not supported. '%s'\n", reg->path );
	}

//	U_EndUnpack();
	IB_Close( h );
	
	return gltex;	
}

/*
  ==============================
  Res_CacheInGLTEX_arr

  ==============================
*/
res_gltex_cache_t * Res_CacheInGLTEX_arr( res_gltex_register_t *reg )
{
    __error( "out of order\n" );
    return 0;
    
#if 0
	ib_file_t	*h;
	int		filesize;
	unsigned char	*buf;
	int		pos;

	char		id[4];
	short		tmp;
// 	int		mipmap;
// 	int		stepnum;
	int		width;
	int		height;
	int		flags;

	res_gltex_cache_t	*gltex;

	h = IB_Open( reg->path );
	if ( !h )
		__error( "load of '%s' failed\n", reg->path );

	filesize = IB_GetSize( h );

	buf = (unsigned char*)alloca( filesize );
//	printf( "arr: alloca(%d) = %p\n", filesize, buf );

	if ( !buf )
		__error( "alloca for '%s' failed\n" );

	IB_Read( buf, filesize, 1, h );
	IB_Close( h );

	//
	// interpret buffer as arr
	//

	U_BeginUnpack( U_PACKED_BIN, buf, filesize );

	// extract arr header
	U_UnpackString( id, 4 );

	if ( memcmp( id, "ARRM", 4 ) )
	{
		__error( "arr header check failed\n" );
	}

	U_UnpackSkip( 32 );

	// mipmap num
	U_Unpacks16( &tmp );
// 	mipmap = (int) tmp;

	// width
	U_Unpacks16( &tmp );
	width = (int) tmp;

	// height
	U_Unpacks16( &tmp );
	height = (int) tmp;

	// stepnum
	U_Unpacks16( &tmp );
// 	stepnum = (int) tmp;

	// flags
	U_Unpacks32( &flags );

	
	gltex = NEWTYPE( res_gltex_cache_t );
	gltex->width = width;
	gltex->height = height;

	//
	// mipmap hack
	//
	if ( gltex->width != gltex->height )
	{
		flag_mipmap_hint = false;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}

	pos = U_EndUnpack();
	buf+=pos;
	Res_CreateGLTEX_565( width, height, buf );

	return gltex;
    
#endif
}

#if 0
/*
  ==============================
  Res_CacheInGltex_JPG

  ==============================
*/

// util funcs ...

#define INPUT_BUF_SIZE	4096 // should be good for us, too.

typedef struct {
	struct jpeg_source_mgr	pub;
	
	ib_file_t	*h;
	JOCTET	buffer[INPUT_BUF_SIZE];
} my_source_mgr;

typedef struct my_error_mgr {
	struct jpeg_error_mgr pub;    /* "public" fields */
	jmp_buf setjmp_buffer;        /* for return to caller */
} *my_error_ptr;

typedef my_source_mgr *my_src_ptr;


// I do this with the jpeglib style, although I don't really know, what I am doing.

METHODDEF( void )init_source( j_decompress_ptr cinfo )
{
	// what the hell are they doing in jdatasrc.c?
}

METHODDEF( boolean )fill_input_buf( j_decompress_ptr cinfo )
{
	my_src_ptr	src = (my_src_ptr) cinfo->src;
	int	num, i, c;

	__named_message( "\n" );

	if( !src->h )
	{
		__warning( "handle == NULL.\n" );
		// very faky.
		src->buffer[0] = (JOCTET) 0xFF;
		src->buffer[1] = (JOCTET) JPEG_EOI;
		num = 2;
	}

      
	for( i = 0; i < INPUT_BUF_SIZE; i++ )
	{
		c = IB_GetChar( src->h );


//		printf( "%d\n", c );
		if( c == EOF )
		{
			break;
		}


		src->buffer[i] =  c;

	}
       
	num = i;
	
	src->pub.next_input_byte = src->buffer;
	src->pub.bytes_in_buffer = num;
	
	return TRUE;
}	

METHODDEF( void ) skip_input_data( j_decompress_ptr cinfo, long num )
{
	int	i;

	my_src_ptr src = (my_src_ptr) cinfo->src;

//	__named_message( "%d\n", num );


	if (num > 0) {
		while (num > (long) src->pub.bytes_in_buffer) 
		{
			num -= (long) src->pub.bytes_in_buffer;
			fill_input_buf(cinfo);
			/* note we assume that fill_input_buffer will never return FALSE,         
       * so suspension need not be handled.                                     
       */                                                                       
		}
		src->pub.next_input_byte += (size_t) num;
		src->pub.bytes_in_buffer -= (size_t) num;
	}


}

METHODDEF( void ) term_source( j_decompress_ptr cinfo )
{
}

void jpeg_ibase_src( j_decompress_ptr cinfo, ib_file_t *h )
{
	my_src_ptr	src;

	if( cinfo->src != NULL )
	{
		__warning( "cinfo->src != NULL\n" );
	}
	cinfo->src = (struct jpeg_source_mgr *) NEWTYPE( my_source_mgr );
	src = (my_src_ptr) cinfo->src; 
	
	src->pub.init_source = init_source;
	src->pub.fill_input_buffer = fill_input_buf;
	src->pub.skip_input_data = skip_input_data;
	src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */ 
	src->pub.term_source = term_source;
	src->h = h;
	src->pub.bytes_in_buffer = 0; /* forces fill_input_buffer on first read */
	src->pub.next_input_byte = NULL; /* until buffer loaded */

}      

static void jpeg_error_exit( j_common_ptr cinfo)
{
	my_error_ptr myerr = (my_error_ptr) cinfo->err;
	(*cinfo->err->output_message) (cinfo);

	longjmp (myerr->setjmp_buffer, 1);
}


res_gltex_cache_t * Res_CacheInGLTEX_jpg( res_gltex_register_t *reg )
{
	struct jpeg_decompress_struct cinfo;

	ib_file_t	*h;
	unsigned char *buffer;
	int row_stride;
	struct my_error_mgr jerr;
	FILE	*b;

	int	pixels;
	unsigned char	*image, *ptr;
	res_gltex_cache_t		*gltex;
	

	h = IB_Open( reg->path );
//	b = fopen( "test.jpg", "rb" );

	if( !h )
	{
		__error( "load of '%s' failed\n", reg->path );
	}

//	memset( &cinfo, 0, sizeof( struct jpeg_decompress_struct ));

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = jpeg_error_exit;
	
	if (setjmp (jerr.setjmp_buffer))                                              
	{

		jpeg_destroy_decompress (&cinfo);
		if( h )
			IB_Close( h );
		__error( "XJT: JPEG load error\n");
	       
	}



	printf( "%p\n", cinfo.err );


	// step 1

	jpeg_create_decompress(&cinfo);	
//	cinfo.err = jpeg_std_error (&jerr.pub);                                       
//	printf( "%p\n", cinfo.err );

	// step 2
	jpeg_ibase_src(&cinfo, h);
//	jpeg_stdio_src( &cinfo, b );

//	printf( "back\n" );
	
	// step 3
	jpeg_read_header(&cinfo, TRUE);


/*
	if( cinfo.jpeg_color_space != JCS_RGB )
	{
		__error( "colorspace is not RGB\n" );
	}
*/


	jpeg_start_decompress( &cinfo );
//	printf( "jpeg: %d %d %d\n", cinfo.image_width, cinfo.image_height, cinfo.output_components );
	if( cinfo.output_components != 3 )
	{
		__error( "jpeg file '%s' is not RGB\n", reg->path );
	}

	pixels = cinfo.output_width * cinfo.output_height;
	image = MM_Malloc( pixels * 3 );
       


	row_stride = cinfo.output_width * 3;

	ptr = image + ( row_stride * (cinfo.output_height - 1) );

	buffer = alloca( row_stride );

//	printf( "row_stride: %d\n", row_stride );

       

	while( cinfo.output_scanline < cinfo.output_height )
	{
//		__named_message( "%d\n", cinfo.output_scanline );
		jpeg_read_scanlines( &cinfo, &buffer, 1 ); 
	
		memcpy( ptr, buffer, row_stride );
		
		ptr -= row_stride;

	}

//	memset( image, 0, pixels *3 );

	jpeg_finish_decompress( &cinfo );
	jpeg_destroy_decompress( &cinfo );
	IB_Close( h );

	gltex = NEWTYPE( res_gltex_cache_t );
	gltex->width = cinfo.output_width;
	gltex->height = cinfo.output_height;

	__message( "%s: %d %d\n", reg->path, gltex->width, gltex->height );

	gltex->comp = resGltexComponents_rgb;
	Res_CreateGLTEX_rgb( cinfo.output_width, cinfo.output_height, image );
//	__error( "good\n" );

	return gltex;
}
	

#endif	
	

/*
  ==============================
  Res_RegisterGLTEX

  ==============================
*/
g_resource_t * Res_RegisterGLTEX( hobj_t *resobj )
{
	g_resource_t	*r;
	hpair_t		*name;
	hpair_t		*path;

	name = FindHPair( resobj, "name" );
	if ( !name )
		return NULL;

	path = FindHPair( resobj, "path" );
	if ( !path )
		return NULL;

	r = G_NewResource( name->value, resobj->name );
	r->res_register = Res_SetupRegisterGLTEX( path->value );
	
	// hack:
	((res_gltex_register_t *)(r->res_register))->resobj = resobj;

	r->state = G_RESOURCE_STATE_REGISTERED;

	return r;
}

void Res_UnregisterGLTEX( g_resource_t *r )
{
	printf( "Res_UnregisterGLTEX: do nothing\n" );
}

res_gltex_cache_t * Res_CacheInGLTEX_jpg( res_gltex_register_t *reg ); // implementation in res_gltex_jpeg.c


void Res_CacheGLTEX( g_resource_t *r )
{
	res_gltex_register_t	*res_register;
	GLuint			texobj;
	
	if ( r->state != G_RESOURCE_STATE_REGISTERED )
		return;

	res_register = (res_gltex_register_t *) r->res_register;

	texobj = CreateTexObject( res_register->resobj );
    s_texobj = texobj;
#if 1
	if ( strstr( res_register->path, ".tga" ) )
	{		
		r->res_cache = Res_CacheInGLTEX_tga( (res_gltex_register_t *)r->res_register );
	}
	else if ( strstr( res_register->path, ".arr" ) )
	{
		r->res_cache = Res_CacheInGLTEX_arr( (res_gltex_register_t *)r->res_register );
	}	
	else if ( strstr( res_register->path, ".jpg" ) )
	{
		r->res_cache = Res_CacheInGLTEX_jpg( (res_gltex_register_t *)r->res_register );
	}	
	else
	{
		__error( "can't recognize image file format of '%s'\n", res_register->path );
	}
    ((res_gltex_cache_t *)(r->res_cache))->texobj = texobj;
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 3 );
    r->state = G_RESOURCE_STATE_CACHED;
    s_texobj = -1;
#else
    {
        int width = 64;
        int height = 64;
        auto gltex = NEWTYPE( res_gltex_cache_t );
        r->res_cache = gltex;

        ((res_gltex_cache_t *)(r->res_cache))->texobj = texobj;
        ((res_gltex_cache_t *)(r->res_cache))->width = 64;
        ((res_gltex_cache_t *)(r->res_cache))->height = 64;
        ((res_gltex_cache_t *)(r->res_cache))->comp = resGltexComponents_rgb;

        std::vector<unsigned char> tmp(width * height * 4);


//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        int mipmap = 0;
        while( width >= 1 && height >= 1 ) {
            std::generate( tmp.begin(), tmp.end(), []() {return rand() % 256;});
//            std::fill( tmp.begin(), tmp.end(), rand() % 256);
            if( mipmap >= 3 ) {
                glTexImage2D( GL_TEXTURE_2D, mipmap, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, tmp.data() );
            }
            GLenum err;
            if ( ( err = glGetError() ) != GL_NO_ERROR ) {
                __error( "glTexImage2D failed: %d\n", err );
            }
            width /= 2;
            height /= 2;
            ++mipmap;
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 3 );
        //glTexImage2D( GL_TEXTURE_2D, 1, GL_RGBA, width/2, height/2, 0, GL_RGBA, GL_UNSIGNED_BYTE, tmp.data() );
        r->state = G_RESOURCE_STATE_CACHED;

    }
#endif

	GC_GiveBackTime();
		
}

void Res_UncacheGLTEX( g_resource_t *res )
{
	printf( "Res_UncacheGLTEX: do nothing\n" );
}

#if 1
namespace g_res {
const char *traits<tag::gltex>::name = "gltex";
    
    


namespace loader
{
    
    
res* gltex::make ( hobj_t* resobj ) {
    res_impl<tag::gltex> *r = new res_impl<tag::gltex>();
    
    hpair_t     *name;
    hpair_t     *path;

    name = FindHPair( resobj, "name" );
    if ( !name )
        return NULL;

    path = FindHPair( resobj, "path" );
    if ( !path )
        return NULL;

    
    r->rs_ = Res_SetupRegisterGLTEX( path->value );
    
    // hack:
    r->rs_->resobj = resobj;

    return r;
}
    
void gltex::unmake ( res* r ) {
    res_impl< tag::gltex >* res_gltex = safe_cast<tag::gltex>(r);
    
    free( res_gltex->rs_ );
    res_gltex->rs_ = 0;
    delete r;
}
void gltex::cache ( res* r ) {
//     if( r->type_id() != traits<tag::gltex>::id ) {
//         __error( "type chaos: res->type_id() != traits<tag::gltex>::id\n" );
//     }
//     
//     res_impl<tag::gltex> *res_gltex = static_cast<res_impl<tag::gltex> *>(r);
    
    
    res_impl< tag::gltex >* res_gltex = safe_cast<tag::gltex>(r);
    
    
    GLuint          texobj;
    
    

    {
        
        
        
        texobj = CreateTexObject( res_gltex->rs_->resobj );
        
    }
    if ( strstr( res_gltex->rs_->path, ".tga" ) )
    {       
        res_gltex->cs_ = Res_CacheInGLTEX_tga( res_gltex->rs_ );
    }
    else if ( strstr( res_gltex->rs_->path, ".arr" ) )
    {
        res_gltex->cs_ = Res_CacheInGLTEX_arr( res_gltex->rs_ );
    }   
    else if ( strstr( res_gltex->rs_->path, ".jpg" ) )
    {
        res_gltex->cs_ = Res_CacheInGLTEX_jpg( res_gltex->rs_ );
    }   
    else
    {
        __error( "can't recognize image file format of '%s'\n", res_gltex->rs_->path );
    }

    res_gltex->cs_->texobj = texobj;
    
    
    
}
void gltex::uncache ( res *r ) {
    __named_message( "leaking gl resources!!!\n" );
    res_impl< tag::gltex >* res_gltex = safe_cast<tag::gltex>(r);
    
    free( res_gltex->cs_ );
    res_gltex->cs_ = 0;
}
} // namespace loader




    
} // namespace g_res

#if 0
void start_gltex_loader( mp::queue &q ) {
    s_loader = new gltex_loader_impl(q);
}

void stop_gltex_loader() {
    if( s_loader ) {
        // destructor automatically does a stop+join
        delete s_loader;
    }


    s_loader = nullptr;
}
#endif

gltex_background_loader::gltex_background_loader(mp::queue &q)
    : impl_( mp::make_unique<gltex_loader_impl>(q) )
{
    // HACK: make the loader kind of a singleton internally
    assert( s_loader == nullptr );
    s_loader = impl_.get();
}

gltex_background_loader::~gltex_background_loader()
{
    s_loader = nullptr;
}

#endif
