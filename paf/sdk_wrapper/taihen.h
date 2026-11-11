#ifndef _TAIHEN_H_
#define _TAIHEN_H_
/* Pull in VitaSDK's actual helpers.  Keeping a local no-op declaration here
 * silently disabled every taiHEN operation even though taihen_stub is linked. */
#include_next <taihen.h>
#endif
