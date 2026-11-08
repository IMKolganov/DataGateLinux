#include "LinuxCapNetAdmin.h"

#if defined(__linux__) && defined(DATAGATE_HAVE_LIBCAP)

#include <sys/capability.h>
#include <sys/prctl.h>

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

    cap_value_t one = CAP_NET_ADMIN;
    // Effective: TUN ioctl / SO_BINDTODEVICE in this process.
    // Inheritable + ambient: child `ip route` (socket_protect host-route) must keep NET_ADMIN.
    if (cap_set_flag(c, CAP_EFFECTIVE, 1, &one, CAP_SET) != 0
        || cap_set_flag(c, CAP_INHERITABLE, 1, &one, CAP_SET) != 0) {
        cap_free(c);
        return false;
    }
    if (cap_set_proc(c) != 0) {
        cap_free(c);
        return false;
    }
    cap_free(c);

#if defined(PR_CAP_AMBIENT) && defined(PR_CAP_AMBIENT_RAISE)
    (void)prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, CAP_NET_ADMIN, 0, 0);
#endif
    return true;
}

#else

bool datagate_linux_try_raise_effective_cap_net_admin()
{
    return false;
}

#endif
