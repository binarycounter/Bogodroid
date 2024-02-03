#ifndef __FCNTL_BRIDGE_H__
#define __FCNTL_BRIDGE_H__

#include "fcntl.h"


#include "platform.h"


ABI_ATTR extern int   fcntl_open(const char* filename, int flags);
ABI_ATTR extern ssize_t fcntl_read(int fd, void *buf, size_t count);
ABI_ATTR extern int   fcntl_close(int fd);


#endif /* __FCNTL_BRIDGE_H__ */