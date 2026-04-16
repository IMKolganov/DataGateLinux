#include "LinuxCapNetAdmin.h"

#if defined(__linux__) && defined(DATAGATE_HAVE_LIBCAP)

#include <sys/capability.h>

bool datagate_linux_try_raise_effective_cap_net_admin()
{
    cap_t c = cap_get_proc();
    if (!c) {
        return false;
    }
    cap_flag_value_t fv = CAP_CLEAR;
    if (cap_get_flag(c, CAP_NET_ADMIN, CAP_PERMITTED, &fv) != 0 || fv != CAP_SET) {
        cap_free(c);
        return false;
    }
    if (cap_get_flag(c, CAP_NET_ADMIN, CAP_EFFECTIVE, &fv) == 0 && fv == CAP_SET) {
        cap_free(c);
        return true;
    }
    cap_value_t one = CAP_NET_ADMIN;
    if (cap_set_flag(c, CAP_EFFECTIVE, 1, &one, CAP_SET) != 0) {
        cap_free(c);
        return false;
    }
    const int r = cap_set_proc(c);
    cap_free(c);
    return r == 0;
}

#else

bool datagate_linux_try_raise_effective_cap_net_admin()
{
    return false;
}

#endif
