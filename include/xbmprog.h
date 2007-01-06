
#ifndef _XBMPROG_H
#define _XBMPROG_H

#define XBMPROG_METHOD_RAW 0
#define XBMPROG_METHOD_RLE 1
#define XBMPROG_METHOD_RLE_DELTA 2
#define XBMPROG_METHOD_END 3

// TODO : cmpb #EE isn't being compiled correctly?!
#ifndef XBMPROG_RLE_SKIP
#define XBMPROG_RLE_SKIP 0xEEu
#endif
#ifndef XBMPROG_RLE_REPEAT
#define XBMPROG_RLE_REPEAT 0xEDu
#endif

#endif /* _XBMPROG_H */
