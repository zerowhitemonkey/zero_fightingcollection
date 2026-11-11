#include "glfn.h"
#define X(ret, name, args) PFN_gl##name gl##name;
GLFN_LIST
#undef X
int glfn_load(void *(*getproc)(const char*)) {
    int ok = 1;
#define X(ret, name, args) gl##name = (PFN_gl##name)getproc("gl" #name); if (!gl##name) ok = 0;
    GLFN_LIST
#undef X
    return ok;
}
