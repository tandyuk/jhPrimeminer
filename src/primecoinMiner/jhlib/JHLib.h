#ifndef __JHSYSTEMLIB
#define __JHSYSTEMLIB

#include <signal.h>
#include <stdint.h>
#include <cstring> // for memcpy/memset
#include<math.h>
#include <algorithm>


template< class T >
T* tmp_addressof(T& arg) {
    return reinterpret_cast<T*>(
               &const_cast<char&>(
                  reinterpret_cast<const volatile char&>(arg)));
}
/*
typedef unsigned long long 	uint64;
typedef signed long long	sint64;

typedef unsigned int 	uint32;
typedef signed int 		sint32;

typedef unsigned short 	uint16;
typedef signed short 	sint16;

typedef unsigned char 	uint8;
typedef signed char 	sint8;*/

typedef int64_t sint64;
typedef uint64_t uint64;
typedef int32_t sint32;
typedef uint32_t uint32;
typedef int16_t sint16;
typedef uint16_t uint16;
typedef int8_t sint8t;
typedef uint8_t uint8;


#define JHCALLBACK	__fastcall


void* _ex1_malloc(int size);
void* _ex1_realloc(void* old, int size);
void _ex1_free(void* p);

void _ex2_initialize();

void* _ex2_malloc(int size, char* file, sint32 line);
void* _ex2_realloc(void* old, int size, char* file, sint32 line);
void _ex2_free(void* p, char* file, sint32 line);
void _ex2_analyzeMemoryLog();

// memory validator
//#define malloc(x) _ex1_malloc(x)
//#define realloc(x,y) _ex1_realloc(x,y)
//#define free(x) _ex1_free(x)

// memory logger
//#define MEMORY_LOGGER_ACTIVE			1
//#define MEMORY_LOGGER_ANALYZE_ACTIVE	1

#ifdef MEMORY_LOGGER_ACTIVE
#define malloc(x) _ex2_malloc(x,__FILE__,__LINE__)
#define realloc(x,y) _ex2_realloc(x,y,__FILE__,__LINE__)
#define free(x) _ex2_free(x,__FILE__,__LINE__)
#endif

#include"fastString.h"
#include"hashTable.h"
#include"packetBuffer.h"
#include"simpleList.h"
#include"customBuffer.h"
#include"streamWrapper.h"


/* error */
#define assertFatal(condition, message, errorCode) if( condition ) { OutputDebugString(message);  ExitProcess(errorCode); } 


#endif