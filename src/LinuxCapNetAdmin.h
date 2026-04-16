#pragma once

/// Raise effective CAP_NET_ADMIN when permitted (libcap). No Qt; safe for small helper binaries.
bool datagate_linux_try_raise_effective_cap_net_admin();
