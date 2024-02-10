#ifndef __NDK_BRIDGE_H__
#define __NDK_BRIDGE_H__

#include "platform.h"

typedef struct
{
   // Number of bytes in this structure.
    uint32_t size;
} AConfiguration;

ABI_ATTR extern AConfiguration* AConfiguration_new();


#endif /* __NDK_BRIDGE_H__ */