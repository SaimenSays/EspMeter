#ifndef BUFFER_H_
#define BUFFER_H_

#include <stdio.h>
#include <string.h>
#include <stdarg.h>



//*****************************************************************************
// Data structures
//*****************************************************************************

typedef struct
{
	uint8_t* data;
	uint16_t head;
	uint16_t tail;
	uint16_t len;
	uint16_t token;
} buffer_t;



//*****************************************************************************
// Function code
// The inline functions need to be in header.
// Otherwise they won't be inline because of seperateobject file.
//*****************************************************************************

inline void buffer_init( void* data, uint16_t len, buffer_t* buf )
{
	buf->data = data;
	buf->len = len;
	buf->head = 0;
	buf->tail = 0;
}

inline uint16_t buffer_used( buffer_t* buf )
{
	if (buf->head > buf->tail)
		return (buf->head - buf->tail);
	else
		return 0;
}

inline uint16_t buffer_free( buffer_t* buf )
{
	if( buf->head < buf->len )
		return (buf->len - buf->head);						
	else
		return 0;
}

inline bool buffer_put_data( void* data, uint16_t len, buffer_t* buf ) 
{
	if( (buf->data == NULL) || (data == NULL) || (buffer_free(buf) < len) ) return false;

	memcpy(&buf->data[buf->head], data, len);
	buf->head += len;
	return true;
}

inline bool buffer_put_string( const char* text, buffer_t* buf ) 
{
	size_t len;
	
	if( (buf->data == NULL) || (text == NULL) ) return false;
	
	len = strlen(text);
	if( buffer_free(buf) < len ) return false;

	memcpy(&buf->data[buf->head], text, len);
	buf->head += len;
	return true;
}

inline bool buffer_put_byte( char byte, buffer_t* buf )
{
	if( (buf->data == NULL) || (buffer_free(buf) == 0) ) return false;

	buf->data[buf->head++] = byte;
	return true;
}

inline uint8_t buffer_get_byte( buffer_t* buf )
{
	if( (buf->data != NULL) && (buffer_used(buf) >= 1) )
	{
		return buf->data[buf->tail++];
	}
	return 0x00;
}

inline uint8_t* buffer_ptr( buffer_t* buf )
{
	if( buf->data == NULL ) return NULL;

	return &buf->data[buf->tail];
}

inline void buffer_clear( buffer_t* buf )
{
	buf->head = 0;
	buf->tail = 0;
}


inline char* buffer_token( const char chr, buffer_t* buf ) 
{
	char* token;
	
	if( (buf->data == NULL) || (buffer_used(buf) == 0) ) return NULL;
	token = (char*)&buf->data[buf->tail];
	for (; buf->tail <= buf->head; buf->tail++) 
	{
		if( buf->data[buf->tail] == chr)  
		{
			buf->data[buf->tail] = '\0';
			buf->tail++;
			return token;
		}
	}
	return token; // token not found, return last position (be carefull, possibliy not \0 terminated)
}



inline bool buffer_put_format( buffer_t* buf, const char* format, ... )
{
	va_list arglist;
	uint16_t len;
	
	if( buf->data == NULL ) return false;

	va_start( arglist, format );
	len = vsnprintf( (char*)&buf->data[buf->head], buffer_free(buf), format, arglist );
	va_end( arglist );	
	if( len < 0 ) 
	{
		return false;
	}
	buf->head += len;	
	if( buffer_free(buf) <= 1 ) 
	{
		return false; 
	}
	return true;
}



// Free up space when some tail bytes are already used, which are all bytes before tail
inline bool buffer_crop( buffer_t* buf ) 
{
	uint16_t used = buffer_used(buf);
	uint16_t n;
	
	if( buf->tail == 0 ) return true;
	for( n=0; n<used; n++ )
	{
		buf->data[n] = buf->data[n+buf->tail];
	}
	buf->tail = 0;
	buf->head = used;
	return true;
}


#endif // BUFFER_H
