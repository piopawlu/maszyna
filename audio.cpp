﻿/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "stdafx.h"

#include <sndfile.h>

#include "audio.h"
#include "Globals.h"
#include "Logs.h"
#include "ResourceManager.h"
#include "utilities.h"

namespace audio {

static sf_count_t sf_vioimp_get_filelen(void *user_data)
{
    std::istream *stream = (std::istream*)user_data;
    size_t cur = stream->tellg();
    stream->seekg(0, std::ios_base::end);
    size_t end = stream->tellg();
    stream->seekg(cur, std::ios_base::beg);
    return end;
}

static sf_count_t sf_vioimp_seek(sf_count_t offset, int whence, void *user_data)
{
    std::istream *stream = (std::istream*)user_data;
    if (whence == SF_SEEK_SET)
        stream->seekg(offset, std::ios_base::beg);
    else if (whence == SF_SEEK_CUR)
        stream->seekg(offset, std::ios_base::cur);
    else if (whence == SF_SEEK_END)
        stream->seekg(offset, std::ios_base::end);
    stream->clear();
    return stream->tellg();
}

static sf_count_t sf_vioimp_read(void *ptr, sf_count_t count, void *user_data)
{
    std::istream *stream = (std::istream*)user_data;
    stream->read((char*)ptr, count);
    size_t gcount = stream->gcount();
    stream->clear();
    return gcount;
}

static sf_count_t sf_vioimp_write(const void *, sf_count_t, void *)
{
    return 0;
}

static sf_count_t sf_vioimp_tell(void *user_data)
{
    std::istream *stream = (std::istream*)user_data;
    return stream->tellg();
}

openal_buffer::openal_buffer( std::string const &Filename ) :
    name( Filename ) {

	SF_INFO si;
	si.format = 0;

	std::string file = Filename;

	WriteLog("sound: loading file: " + file);

    ivfsstream stream(file, std::ios::binary);
    std::istream *stream_ptr = &stream;
    SF_VIRTUAL_IO vio { sf_vioimp_get_filelen, sf_vioimp_seek, sf_vioimp_read, sf_vioimp_write, sf_vioimp_tell };

    SNDFILE *sf = sf_open_virtual(&vio, SFM_READ, &si, stream_ptr);

	if (sf == nullptr)
		throw std::runtime_error("sound: sf_open failed");

	sf_command(sf, SFC_SET_NORM_FLOAT, NULL, SF_TRUE);

	float *fbuf = new float[si.frames * si.channels];
	if (sf_readf_float(sf, fbuf, si.frames) != si.frames)
		throw std::runtime_error("sound: incomplete file");

	sf_close(sf);

	rate = si.samplerate;

	if (si.channels != 1)
		WriteLog("sound: warning: mixing multichannel file to mono");

	int16_t *buf = new int16_t[si.frames];
	for (size_t i = 0; i < si.frames; i++)
	{
		float accum = 0;
		for (size_t j = 0; j < si.channels; j++)
			accum += fbuf[i * si.channels + j];

		long val = lrintf(accum / si.channels * 32767.0f);
		if (val > 32767)
			val = 32767;
		if (val < -32767)
			val = -32767;
		buf[i] = val;
	}

	alGenBuffers(1, &id);
	if (id != null_resource && alIsBuffer(id)) {
		alGetError();
		alBufferData(id, AL_FORMAT_MONO16, buf, si.frames * 2, rate);
	}
	else {
		id = null_resource;
		ErrorLog("sound: failed to create AL buffer");
	}

	delete[] buf;
	delete[] fbuf;

	fetch_caption();
}

// retrieves sound caption in currently set language
void
openal_buffer::fetch_caption() {

    std::string captionfilename { name };
    captionfilename.erase( captionfilename.rfind( '.' ) ); // obcięcie rozszerzenia
    captionfilename += "-" + Global.asLang + ".txt"; // już może być w różnych językach
    if( true == FileExists( captionfilename ) ) {
        // wczytanie
        ivfsstream inputfile( captionfilename );
        caption.assign( std::istreambuf_iterator<char>( inputfile ), std::istreambuf_iterator<char>() );
    }
}

buffer_manager::~buffer_manager() {

    for( auto &buffer : m_buffers ) {
        if( buffer.id != null_resource ) {
            ::alDeleteBuffers( 1, &( buffer.id ) );
        }
    }
}

// creates buffer object out of data stored in specified file. returns: handle to the buffer or null_handle if creation failed
audio::buffer_handle
buffer_manager::create( std::string const &Filename ) {

    auto filename { ToLower( Filename ) };

    erase_extension( filename );

    audio::buffer_handle lookup { null_handle };
    std::string filelookup;
    if( false == Global.asCurrentDynamicPath.empty() ) {
        // try dynamic-specific sounds first
        lookup = find_buffer( Global.asCurrentDynamicPath + filename );
        if( lookup != null_handle ) {
            return lookup;
        }
        filelookup = find_file( Global.asCurrentDynamicPath + filename );
        if( false == filelookup.empty() ) {
            return emplace( filelookup );
        }
    }
    if( filename.find( '/' ) != std::string::npos ) {
        // if the filename includes path, try to use it directly
        lookup = find_buffer( filename );
        if( lookup != null_handle ) {
            return lookup;
        }
        filelookup = find_file( filename );
        if( false == filelookup.empty() ) {
            return emplace( filelookup );
        }
    }
    // if dynamic-specific and/or direct lookups find nothing, try the default sound folder
    lookup = find_buffer( szSoundPath + filename );
    if( lookup != null_handle ) {
        return lookup;
    }
    filelookup = find_file( szSoundPath + filename );
    if( false == filelookup.empty() ) {
        return emplace( filelookup );
    }
    // if we still didn't find anything, give up
	ErrorLog( "Bad file: failed to locate audio file \"" + Filename + "\"", logtype::file );
    return null_handle;
}

// provides direct access to a specified buffer
audio::openal_buffer const &
buffer_manager::buffer( audio::buffer_handle const Buffer ) const {

    return m_buffers[ Buffer ];
}

// places in the bank a buffer containing data stored in specified file. returns: handle to the buffer
audio::buffer_handle
buffer_manager::emplace( std::string Filename ) {

    buffer_handle const handle { m_buffers.size() };
    m_buffers.emplace_back( Filename );

    // NOTE: we store mapping without file type extension, to simplify lookups
    erase_extension( Filename );
    m_buffermappings.emplace(
        Filename,
        handle );

    return handle;
}

audio::buffer_handle
buffer_manager::find_buffer( std::string const &Buffername ) const {

    auto const lookup = m_buffermappings.find( Buffername );
    if( lookup != std::end( m_buffermappings ) )
        return lookup->second;
    else
        return null_handle;
}

std::string
buffer_manager::find_file( std::string const &Filename ) const {

    auto const lookup {
        FileExists(
            { Filename },
            { ".ogg", ".flac", ".wav" } ) };

    return lookup.first + lookup.second;
}

} // audio

//---------------------------------------------------------------------------
