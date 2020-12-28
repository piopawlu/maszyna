﻿/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/
/*
MaSzyna EU07 - SPKS
Brakes.
Copyright (C) 2007-2014 Maciej Cierniak
*/
#include "stdafx.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
#include <unistd.h>
#endif

#ifdef WIN32
#define stat _stat
#endif

#include "utilities.h"
#include "Globals.h"
#include "parser.h"

#include "Logs.h"

bool DebugModeFlag = false;
bool FreeFlyModeFlag = false;
bool EditorModeFlag = false;
bool DebugCameraFlag = false;
bool DebugTractionFlag = false;

double Max0R(double x1, double x2)
{
    if (x1 > x2)
        return x1;
    else
        return x2;
}

double Min0R(double x1, double x2)
{
    if (x1 < x2)
        return x1;
    else
        return x2;
}

// shitty replacement for Borland timestamp function
// TODO: replace with something sensible
std::string Now() {

    std::time_t timenow = std::time( nullptr );
    std::tm tm = *std::localtime( &timenow );
    std::stringstream  converter;
    converter << std::put_time( &tm, "%c" );
    return converter.str();
}

// zwraca różnicę czasu
// jeśli pierwsza jest aktualna, a druga rozkładowa, to ujemna oznacza opóżnienie
// na dłuższą metę trzeba uwzględnić datę, jakby opóżnienia miały przekraczać 12h (towarowych)
double CompareTime(double t1h, double t1m, double t2h, double t2m) {

    if ((t2h < 0))
        return 0;
    else
    {
        auto t = (t2h - t1h) * 60 + t2m - t1m; // jeśli t2=00:05, a t1=23:50, to różnica wyjdzie ujemna
        if ((t < -720)) // jeśli różnica przekracza 12h na minus
            t = t + 1440; // to dodanie doby minut;else
        if ((t > 720)) // jeśli przekracza 12h na plus
            t = t - 1440; // to odjęcie doby minut
        return t;
    }
}

bool SetFlag( int &Flag, int const Value ) {

    if( Value > 0 ) {
        if( false == TestFlag( Flag, Value ) ) {
            Flag |= Value;
            return true; // true, gdy było wcześniej 0 i zostało ustawione
        }
    }
    else if( Value < 0 ) {
        // Value jest ujemne, czyli zerowanie flagi
        return ClearFlag( Flag, -Value );
    }
    return false;
}

bool ClearFlag( int &Flag, int const Value ) {

    if( true == TestFlag( Flag, Value ) ) {
        Flag &= ~Value;
        return true;
    }
    else {
        return false;
    }
}

double Random(double a, double b)
{
	uint32_t val = Global.random_engine();
	return interpolate(a, b, (double)val / Global.random_engine.max());
}

double LocalRandom(double a, double b)
{
	uint32_t val = Global.local_random_engine();
	return interpolate(a, b, (double)val / Global.random_engine.max());
}

bool FuzzyLogic(double Test, double Threshold, double Probability)
{
    if ((Test > Threshold) && (!DebugModeFlag))
        return
            (Random() < Probability * Threshold * 1.0 / Test) /*im wiekszy Test tym wieksza szansa*/;
    else
        return false;
}

bool FuzzyLogicAI(double Test, double Threshold, double Probability)
{
    if ((Test > Threshold))
        return
            (Random() < Probability * Threshold * 1.0 / Test) /*im wiekszy Test tym wieksza szansa*/;
    else
        return false;
}

std::string DUE(std::string s) /*Delete Before Equal sign*/
{
    //DUE = Copy(s, Pos("=", s) + 1, length(s));
	return s.substr(s.find("=") + 1, s.length());
}

std::string DWE(std::string s) /*Delete After Equal sign*/
{
    size_t ep = s.find("=");
	if (ep != std::string::npos)
		//DWE = Copy(s, 1, ep - 1);
		return s.substr(0, ep);
    else
        return s;
}

std::string ExchangeCharInString( std::string const &Source, char const From, char const To )
{
	std::string replacement; replacement.reserve( Source.size() );
	std::for_each(
        std::begin( Source ), std::end( Source ),
        [&](char const idx) {
		    if( idx != From ) { replacement += idx; }
		    else              { replacement += To; } } );

	return replacement;
}

std::vector<std::string> &Split(const std::string &s, char delim, std::vector<std::string> &elems)
{ // dzieli tekst na wektor tekstow

    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim))
    {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> Split(const std::string &s, char delim)
{ // dzieli tekst na wektor tekstow
    std::vector<std::string> elems;
    Split(s, delim, elems);
    return elems;
}

std::vector<std::string> Split(const std::string &s)
{ // dzieli tekst na wektor tekstow po białych znakach
	std::vector<std::string> elems;
	std::stringstream ss(s);
	std::string item;
	while (ss >> item)
	{
		elems.push_back(item);
	}
	return elems;
}

std::pair<std::string, int>
split_string_and_number( std::string const &Key ) {

    auto const indexstart{ Key.find_first_of( "-1234567890" ) };
    auto const indexend{ Key.find_first_not_of( "-1234567890", indexstart ) };
    if( indexstart != std::string::npos ) {
        return {
            Key.substr( 0, indexstart ),
            std::stoi( Key.substr( indexstart, indexend - indexstart ) ) };
    }
    return { Key, 0 };
}

std::string to_string(int Value)
{
	std::ostringstream o;
	o << Value;
	return o.str();
};

std::string to_string(unsigned int Value)
{
	std::ostringstream o;
	o << Value;
	return o.str();
};

std::string to_string(double Value)
{
	std::ostringstream o;
	o << Value;
	return o.str();
};

std::string to_string(int Value, int width)
{
	std::ostringstream o;
	o.width(width);
	o << Value;
	return o.str();
};

std::string to_string(double Value, int precision)
{
	std::ostringstream o;
	o << std::fixed << std::setprecision(precision);
	o << Value;
	return o.str();
};

std::string to_string(double const Value, int const Precision, int const Width)
{
	std::ostringstream converter;
	converter << std::setw( Width ) << std::fixed << std::setprecision(Precision) << Value;
	return converter.str();
};

std::string to_hex_str( int const Value, int const Width )
{
	std::ostringstream converter;
	converter << "0x" << std::uppercase << std::setfill( '0' ) << std::setw( Width ) << std::hex << Value;
	return converter.str();
};

bool string_ends_with(const std::string &string, const std::string &ending)
{
	if (string.length() < ending.length())
		return false;

	return string.compare(string.length() - ending.length(), ending.length(), ending) == 0;
}

bool string_starts_with(const std::string &string, const std::string &begin)
{
	if (string.length() < begin.length())
		return false;

	return string.compare(0, begin.length(), begin) == 0;
}

std::string const fractionlabels[] = { " ", u8"¹", u8"²", u8"³", u8"⁴", u8"⁵", u8"⁶", u8"⁷", u8"⁸", u8"⁹" };

std::string to_minutes_str( float const Minutes, bool const Leadingzero, int const Width ) {

    float minutesintegral;
    auto const minutesfractional { std::modf( Minutes, &minutesintegral ) };
    auto const width { Width - 1 };
    auto minutes = (
        std::string( width - 1, ' ' )
        + ( Leadingzero ?
            to_string( 100 + minutesintegral ).substr( 1, 2 ) :
            to_string( minutesintegral, 0 ) ) );
    return (
        minutes.substr( minutes.size() - width, width )
        + fractionlabels[ static_cast<int>( std::floor( minutesfractional * 10 + 0.1 ) ) ] );
}


int stol_def(const std::string &str, const int &DefaultValue) {

    int result { DefaultValue };
    std::stringstream converter;
    converter << str;
    converter >> result;
    return result;
}

std::string ToLower(std::string const &text) {

    auto lowercase { text };
	std::transform(
        std::begin( text ), std::end( text ),
        std::begin( lowercase ),
        []( unsigned char c ) { return std::tolower( c ); } );
	return lowercase;
}

std::string ToUpper(std::string const &text) {

    auto uppercase { text };
    std::transform(
        std::begin( text ), std::end( text ),
        std::begin( uppercase ),
        []( unsigned char c ) { return std::toupper( c ); } );
    return uppercase;
}

// replaces polish letters with basic ascii
void
win1250_to_ascii( std::string &Input ) {

	std::unordered_map<char, char> const charmap {
		{ 165, 'A' }, { 198, 'C' }, { 202, 'E' }, { 163, 'L' }, { 209, 'N' }, { 211, 'O' }, { 140, 'S' }, { 143, 'Z' }, { 175, 'Z' },
		{ 185, 'a' }, { 230, 'c' }, { 234, 'e' }, { 179, 'l' }, { 241, 'n' }, { 243, 'o' }, { 156, 's' }, { 159, 'z' }, { 191, 'z' }
    };
    std::unordered_map<char, char>::const_iterator lookup;
    for( auto &input : Input ) {
        if( ( lookup = charmap.find( input ) ) != charmap.end() )
            input = lookup->second;
    }
}

std::string win1250_to_utf8(const std::string &Input) {
	std::unordered_map<char, std::string> const charmap {
		{ 165, u8"Ą" }, { 198, u8"Ć" }, { 202, u8"Ę" }, { 163, u8"Ł" }, { 209, u8"Ń" }, { 211, u8"Ó" }, { 140, u8"Ś" }, { 143, u8"Ź" }, { 175, u8"Ż" },
		{ 185, u8"ą" }, { 230, u8"ć" }, { 234, u8"ę" }, { 179, u8"ł" }, { 241, u8"ń" }, { 243, u8"ó" }, { 156, u8"ś" }, { 159, u8"ź" }, { 191, u8"ż" }
	};
	std::string output;
	std::unordered_map<char, std::string>::const_iterator lookup;
	for( auto &input : Input ) {
		if( ( lookup = charmap.find( input ) ) != charmap.end() )
			output += lookup->second;
		else
			output += input;
	}
	return output;
}

// Ra: tymczasowe rozwiązanie kwestii zagranicznych (czeskich) napisów
char charsetconversiontable[] =
    "E?,?\"_++?%S<STZZ?`'\"\".--??s>stzz"
    " ^^L$A|S^CS<--RZo±,l'uP.,as>L\"lz"
    "RAAAALCCCEEEEIIDDNNOOOOxRUUUUYTB"
    "raaaalccceeeeiiddnnoooo-ruuuuyt?";

// wycięcie liter z ogonkami
std::string Bezogonkow(std::string Input, bool const Underscorestospaces) {

    char const extendedcharsetbit { static_cast<char>( 0x80 ) };
    char const space { ' ' };
    char const underscore { '_' };

    for( auto &input : Input ) {
        if( input & extendedcharsetbit ) {
            input = charsetconversiontable[ input ^ extendedcharsetbit ];
        }
        else if( input < space ) {
            input = space;
        }
        else if( Underscorestospaces && ( input == underscore ) ) {
            input = space;
        }
    }

    return Input;
}

template <>
bool
extract_value( bool &Variable, std::string const &Key, std::string const &Input, std::string const &Default ) {

    auto value = extract_value( Key, Input );
    if( false == value.empty() ) {
        // set the specified variable to retrieved value
        Variable = ( ToLower( value ) == "yes" );
        return true; // located the variable
    }
    else {
        // set the variable to provided default value
        if( false == Default.empty() ) {
            Variable = ( ToLower( Default ) == "yes" );
        }
        return false; // couldn't locate the variable in provided input
    }
}

bool FileExistsDirect( std::string const &Filename ) {
	std::ifstream file( Filename );
    return( true == file.is_open() );
}

bool FileExists(std::string const &Filename) {
    eu07vfs_file_handle handle = eu07vfs_lookup_file(Global.vfs, Filename.data(), Filename.size());
    return handle != EU07VFS_NULL_HANDLE;
}

std::pair<std::string, std::string>
FileExists( std::vector<std::string> const &Names, std::vector<std::string> const &Extensions ) {

    for( auto const &name : Names ) {
        for( auto const &extension : Extensions ) {
            if( FileExists( name + extension ) ) {
                return { name, extension };
            }
        }
    }
    // nothing found
    return { {}, {} };
}

// potentially erases file extension from provided file name. returns: true if extension was removed, false otherwise
bool
erase_extension( std::string &Filename ) {

    auto const extensionpos { Filename.rfind( '.' ) };

    if( extensionpos == std::string::npos ) { return false; }

    if( extensionpos != Filename.rfind( ".." ) + 1 ) {
        // we can get extension for .mat or, in legacy files, some image format. just trim it and set it to material file extension
        Filename.erase( extensionpos );
        return true;
    }
    return false;
}

void
erase_leading_slashes( std::string &Filename ) {

    while( Filename[ 0 ] == '/' ) {
        Filename.erase( 0, 1 );
    }
}

// potentially replaces backward slashes in provided file path with unix-compatible forward slashes
void
replace_slashes( std::string &Filename ) {

	std::replace(
	    std::begin( Filename ), std::end( Filename ),
	    '\\', '/' );
}

// returns potential path part from provided file name
std::string
substr_path( std::string const &Filename ) {

    return (
        Filename.rfind( '/' ) != std::string::npos ?
            Filename.substr( 0, Filename.rfind( '/' ) + 1 ) :
            "" );
}

// returns length of common prefix between two provided strings
std::ptrdiff_t
len_common_prefix( std::string const &Left, std::string const &Right ) {

    auto const *left  { Left.data() };
    auto const *right { Right.data() };
    // compare up to the length of the shorter string
    return ( Right.size() <= Left.size() ?
        std::distance( right, std::mismatch( right, right + Right.size(), left ).first ) :
        std::distance( left,  std::mismatch( left,  left + Left.size(),  right ).first ) );
}

// returns true if provided string ends with another provided string
bool
ends_with( std::string const &String, std::string const &Suffix ) {

    return ( String.size() >= Suffix.size() )
        && ( 0 == String.compare( String.size() - Suffix.size(), Suffix.size(), Suffix ) );
}

// helper, restores content of a 3d vector from provided input stream
// TODO: review and clean up the helper routines, there's likely some redundant ones

glm::dvec3 LoadPoint( cParser &Input ) {
    // pobranie współrzędnych punktu
    glm::dvec3 point;
    Input.getTokens( 3 );
    Input
        >> point.x
        >> point.y
        >> point.z;
    return point;
}

// extracts a group of tokens from provided data stream, returns one of them picked randomly
std::string
deserialize_random_set( cParser &Input, char const *Break ) {

    auto token { Input.getToken<std::string>( true, Break ) };
    std::replace(token.begin(), token.end(), '\\', '/');
    if( token != "[" ) {
        // simple case, single token
        return token;
    }
    // if instead of a single token we've encountered '[' this marks a beginning of a random set
    // we retrieve all entries, then return a random one
    std::vector<std::string> tokens;
    while( ( ( token = deserialize_random_set( Input, Break ) ) != "" )
        && ( token != "]" ) ) {
        tokens.emplace_back( token );
    }
    if( false == tokens.empty() ) {
        std::shuffle( std::begin( tokens ), std::end( tokens ), Global.random_engine );
        return tokens.front();
    }
    else {
        // shouldn't ever get here but, eh
        return "";
    }
}

int count_trailing_zeros( uint32_t val )
{
    int r = 0;

    for( uint32_t shift = 1; !( val & shift ); shift <<= 1 )
        r++;

    return r;
}
