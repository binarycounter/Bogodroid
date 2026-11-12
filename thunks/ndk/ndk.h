#ifndef __NDK_BRIDGE_H__
#define __NDK_BRIDGE_H__

#include "platform.h"
#include "baron/baron.h"
#include "android.h"
#include <vector>

typedef struct
{
   // Number of bytes in this structure.
    uint32_t size;
} AConfiguration;

typedef struct
{

} ARect;

typedef struct
{

} ANativeWindow;

typedef struct
{

} AInputQueue;


ABI_ATTR extern AConfiguration* AConfiguration_new();


#endif /* __NDK_BRIDGE_H__ */