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
#include <memory>
#include <openjpeg.h>
#include <turbojpeg.h>
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


class mipmap_cache_wrap : public mipmap_cache {
public:
    mipmap_cache_wrap( size_t width, size_t height, const uint8_t *first, const uint8_t *last )
        : size_(width, height),
          data_(first, last )
    {}

    virtual std::pair<size_t,size_t> size() {
        return size_;
    }

    virtual std::pair<const uint8_t *,const uint8_t *> data() {
        return data_;
    }

    std::pair<size_t,size_t> size_;
    std::pair<const uint8_t *,const uint8_t *> data_;
};

class mipmap_cache_copy : public mipmap_cache {
public:
    mipmap_cache_copy( size_t width, size_t height, std::vector<uint8_t> && data )
        : width_(width),
          height_(height),
          data_( std::move(data) )
    {}

//    mipmap_cache_copy( size_t width, size_t height, size_t size )
//        : width_(width),
//          height_(height),
//          data_(size)
//    {}

    virtual std::pair<size_t,size_t> size() {
        return std::make_pair( width_, height_ );
    }

    virtual std::pair<const uint8_t *,const uint8_t *> data() {
        return data_nonconst();
    }

    virtual std::pair<uint8_t *, uint8_t *> data_nonconst() {
        return std::make_pair(data_.data(), data_.data() + data_.size());
    }
private:
    size_t width_;
    size_t height_;
    std::vector<uint8_t> data_;
};



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


    virtual ~gltex_data_source() {
      //  DD_LOG << "~gltex_data_source()\n";
    }

    virtual std::pair<size_t,size_t> mipmap_size( size_t level ) = 0;
    virtual std::pair<const uint8_t *,const uint8_t *> mipmap_data( size_t level ) = 0;
    virtual size_t max_level() = 0;
    virtual GLenum pixel_format() = 0;

    virtual std::shared_ptr<mipmap_cache> get_cache( size_t level ) = 0;
};


static size_t pixel_size( GLint pf ) {
    return (pf == GL_BGR) ? 3 : 4;
}

class gltex_data_source_dds_mmap : public gltex_data_source {
public:
    gltex_data_source_dds_mmap( const char *name )
        : h_(name),
          map_(h_),
          header_((dds_header*)(map_.ptr() + sizeof(int32_t))),
          data_((const uint8_t*) map_.ptr() + sizeof(int32_t) + sizeof( dds_header ))
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

    std::pair<const uint8_t *,const uint8_t *> mipmap_data( size_t level ) {
        size_t w = header_->dwWidth;
        size_t h = header_->dwHeight;

        const uint8_t *ptr = data_;
        const size_t pixel_width = (header_->ddspf.dwRGBBitCount / 8);
        for( size_t i = 0; i < level; ++i ) {
            ptr += (w * h * pixel_width);
            w /= 2;
            h /= 2;
        }
        return std::make_pair( ptr, ptr + w * h * (header_->ddspf.dwRGBBitCount / 8));
    }
    GLenum pixel_format() {
        return GL_BGR;
    }
    std::shared_ptr<mipmap_cache> get_cache( size_t level ) {
        __error( "not implemented\n");
    }


private:
    ibase::file_handle h_;
    ibase::file_handle::mapping map_;
    const dds_header *header_;
    const uint8_t *data_;


};


std::vector<uint8_t> next_mipmap( size_t w, size_t h, const uint8_t *data, GLint pf ) {
    size_t ps = pixel_size(pf);
    size_t line_size = w * ps;



    std::vector<uint8_t> out_buf;


    size_t out_w = w / 2;
    size_t out_h = h / 2;
    out_buf.reserve( out_w * out_h * ps );

    const uint8_t *ldata1_1 = data;
    const uint8_t *ldata1_2 = data + ps;

    const uint8_t *ldata2_1 = data + line_size;
    const uint8_t *ldata2_2 = data + ps + line_size;

    for( size_t i = 0; i < out_h; ++i ) {
        for( size_t j = 0; j < out_w; ++j ) {
            // loop over color/alpha components
            for( size_t k = 0; k < ps; ++k ) {
                // average over 4 adjacent pixels from the higher-res texture
                int cp = *(ldata1_1++) + *(ldata1_2++) + *(ldata2_1++) + *(ldata2_2++);
                out_buf.push_back( cp / 4 );
            }
            // jump over next pixel
            ldata1_1 += ps;
            ldata1_2 += ps;
            ldata2_1 += ps;
            ldata2_2 += ps;

        }
        // jump over next line
        ldata1_1 += line_size;
        ldata1_2 += line_size;
        ldata2_1 += line_size;
        ldata2_2 += line_size;
    }

    return out_buf;
}

class gltex_data_source_tga_mmap : public gltex_data_source {
#pragma pack(push)
#pragma pack(1)
    typedef struct
    {
        int8_t  identsize;          // size of ID field that follows 18 byte header (0 usually)
        int8_t  colourmaptype;      // type of colour map 0=none, 1=has palette
        int8_t  imagetype;          // type of image 0=none,1=indexed,2=rgb,3=grey,+8=rle packed

        int16_t colourmapstart;     // first colour map entry in palette
        int16_t colourmaplength;    // number of colours in palette
        int8_t  colourmapbits;      // number of bits per palette entry 15,16,24,32

        int16_t xstart;             // image x origin
        int16_t ystart;             // image y origin
        int16_t width;              // image width in pixels
        int16_t height;             // image height in pixels
        int8_t  bits;               // image bits per pixel 8,16,24,32
        int8_t  descriptor;         // image descriptor bits (vh flip bits)

        // pixel data follows header

    } tga_header;
#pragma pack(pop)

public:
    gltex_data_source_tga_mmap( const char *name )
        : h_(name),
          map_(h_),
          header_((tga_header*)(map_.ptr())),
          data_((const uint8_t *)map_.ptr() + sizeof( tga_header ))
    {

        //std::cerr << "tga: " << header_->width << " " << header_->height << std::endl;
        if( !need_flip_ )
        {
            size_t w = header_->width;
            size_t h = header_->height;
            const uint8_t *ptr = data_ + header_->identsize;
            cache_.emplace_back( std::make_shared<mipmap_cache_wrap>( w, h, ptr, ptr + w * h * (header_->bits / 8)));
        } else {
            size_t w = header_->width;
            size_t h = header_->height;
            const uint8_t *ptr = data_ + header_->identsize;

            auto cache_ptr = std::make_shared<mipmap_cache_copy>( w, h, std::vector<uint8_t>( ptr, ptr + w * h * (header_->bits / 8)));
            cache_.emplace_back( cache_ptr );


            // flip scanlines to match opengl texcoord-system
            size_t ps = pixel_size(pixel_format());
            size_t line_size = w * ps;
            std::vector<uint8_t> tmp;
            auto data = cache_ptr->data_nonconst();

            // pointer to the start of the first line
            uint8_t *insert_pos = data.first;

            // pointer to the start of the last line
            uint8_t *get_pos = data.second - line_size;

            for( size_t i = 0; i < h / 2; ++i ) {

                tmp.assign(insert_pos, insert_pos + line_size );

                std::copy( get_pos, get_pos + line_size, insert_pos );
                std::copy( tmp.begin(), tmp.end(), get_pos );

                // increase/decrease line pointers
                insert_pos += line_size;
                get_pos -= line_size;
            }


        }

        const uint8_t *md = (const uint8_t *)cache_.back()->data().first;
        auto ms = mipmap_size(0);

        // generate mipmaps
        while( ms.first > 4 && ms.second > 4 ) {
            cache_.emplace_back( std::make_shared<mipmap_cache_copy>(ms.first / 2, ms.second / 2, next_mipmap(ms.first, ms.second, md, pixel_format() )));
            //gen_mipmaps_.emplace_back( next_mipmap(ms.first, ms.second, md, pixel_format() ));
            md = cache_.back()->data().first;
            ms.first /= 2;
            ms.second /= 2;
        }

    }

    std::pair<size_t,size_t> mipmap_size( size_t level ) {

        assert( level < cache_.size() );
        return cache_[level]->size();
    }
    size_t max_level() {
        //return gen_mipmaps_.size(); // gen_mipmaps_[0] is mipmap level 1!
        return cache_.size() - 1;
    }

    std::pair<const uint8_t *,const uint8_t *> mipmap_data( size_t level ) {
        assert( level < cache_.size() );
        return cache_[level]->data();

    }

    GLenum pixel_format() {
        switch( header_->bits ) {
        case 32:
            return GL_BGRA;
        case 24:
            return GL_BGR;

        default:
            __error( "unknown pixel format");
        }
    }

    std::shared_ptr<mipmap_cache> get_cache( size_t level ) {
        assert( level < cache_.size() );
        return cache_[level];
    }

private:
    ibase::file_handle h_;
    ibase::file_handle::mapping map_;
    const tga_header *header_;
    const uint8_t *data_;

    std::vector<std::vector<uint8_t>> gen_mipmaps_;

    std::vector<std::shared_ptr<mipmap_cache> > cache_;
    const static bool need_flip_ = true;

    //std::vector<uint8_t> ms[2];
};
#if 0
class gltex_data_source_jpeg2000_mmap : public gltex_data_source {
    static void error_callback (const char *msg, void *client_data) {
        DD_LOG << "jp2k error: " << msg << "\n";
    }
    static void warning_callback (const char *msg, void *client_data) {
        DD_LOG << "jp2k warning: " << msg << "\n";
    }
    static void info_callback (const char *msg, void *client_data) {
        DD_LOG << "jp2k info: " << msg << "\n";
    }
public:
    gltex_data_source_jpeg2000_mmap( const char *name )
    : h_(name), map_(h_)
    {
        event_mgr_.error_handler = error_callback;
        event_mgr_.info_handler = info_callback;
        event_mgr_.warning_handler = warning_callback;

        codec_ = opj_create_decompress(CODEC_JP2);
        opj_set_event_mgr( (opj_common_ptr)codec_, &event_mgr_, stderr );
        opj_setup_decoder( codec_, &parameters_ );
        opj_set_default_decoder_parameters( &parameters_ );
        opj_setup_decoder( codec_, &parameters_ );

        cio_ = opj_cio_open( (opj_common_ptr)codec_, const_cast<unsigned char *>((const unsigned char *)map_.ptr()), h_.get_size() ); // aahhrg, stupid c progrmmers
    }

    virtual ~gltex_data_source_jpeg2000_mmap() {
        opj_cio_close( cio_ );
        opj_destroy_decompress( codec_ );
    }

    size_t max_level() {
        return 0;
    }

    std::pair<size_t,size_t> mipmap_size( size_t level ) {
        opj_image_t *image = opj_decode( codec_, cio_ );
        image->

    }

    std::pair<const uint8_t *,const uint8_t *> mipmap_data( size_t level ) {
        if( level == 0 ) {
            assert( level == 0 );

            size_t w = header_->width;
            size_t h = header_->height;

            const uint8_t *ptr = data_ + header_->identsize;

            return std::make_pair( ptr, ptr + w * h * (header_->bits / 8));
        } else {
            assert( level <= gen_mipmaps_.size() );

            const size_t idx = level - 1;
            const uint8_t *first = &(gen_mipmaps_[idx].front());
            const uint8_t *last = (&(gen_mipmaps_[idx].back())) + 1;
            return std::make_pair( first, last );
        }
    }

private:
    ibase::file_handle h_;
    ibase::file_handle::mapping map_;
    opj_event_mgr_t event_mgr_;
    opj_dparameters_t parameters_;

    opj_dinfo_t *codec_;
    opj_cio_t *cio_;

};
#endif
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
            sleep_cond_.wait_for(lock, std::chrono::seconds(1));
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

            tq_.emplace<msg::gl_upload_texture>( j.target_, j.src_->get_cache(j.mipmap_level_), j.src_->pixel_format(), j.mipmap_level_, j.src_->max_level());
            q_.erase(it);
            sleep_cond_.wait_for(lock, std::chrono::milliseconds(1));
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

static std::deque<gltex_loader_impl::job> s_deferred_jobs;

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


    {
        res_gltex_cache_t		*gltex;

        gltex = NEWTYPE( res_gltex_cache_t );


        std::shared_ptr<gltex_data_source> ds;



        if( strstr( res_register->path, ".tga")) {
            ds = std::make_shared<gltex_data_source_tga_mmap>( res_register->path );
        } else {
            ds = std::make_shared<gltex_data_source_dds_mmap>( "textures/wall/con1_1.dds" );
        }
        DD_LOG << "cache: " << res_register->path << "\n";

        std::tie( gltex->width, gltex->height) = ds->mipmap_size(0);
        switch( ds->pixel_format() ) {
        case GL_RGB:
        case GL_BGR:
            gltex->comp = resGltexComponents_rgb;
            break;
        case GL_RGBA:
        case GL_BGRA:
            gltex->comp = resGltexComponents_rgba;
            break;

        default:
            __error( "unknown pixel format\n");
        }



        size_t num_mipmap = ds->max_level() + 1;
        for( size_t i = 0; i < num_mipmap; ++i ) {
            gltex_loader_impl::job j;
            j.mipmap_level_ = i;
            j.src_ = ds;
            j.target_ = texobj;


            if( s_loader != nullptr ) {
                s_loader->add( std::move(j) );
            } else {
                // HACK: if the loader has not been started, store the jobs in the deferred list
                s_deferred_jobs.push_back(std::move(j));
            }


        }
        r->res_cache = gltex;
    }




    ((res_gltex_cache_t *)(r->res_cache))->texobj = texobj;
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 3 );
    r->state = G_RESOURCE_STATE_CACHED;


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
#if 0
    if ( strstr( res_gltex->rs_->path, ".tga" ) )
    {
        res_gltex->cs_ = Res_CacheInGLTEX_tga( res_gltex->rs_ );
    }
    else if ( strstr( res_gltex->rs_->path, ".jpg" ) )
    {
        res_gltex->cs_ = Res_CacheInGLTEX_jpg( res_gltex->rs_ );
    }
    else
    {
        __error( "can't recognize image file format of '%s'\n", res_gltex->rs_->path );
    }
#endif
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

    // HACK2: add 'deferred jobs' (jobs that have been collected before the background loader was started)
    for( auto & j : s_deferred_jobs ) {
        s_loader->add( std::move(j) );
    }

}

gltex_background_loader::~gltex_background_loader()
{
    s_loader = nullptr;
}

#endif
