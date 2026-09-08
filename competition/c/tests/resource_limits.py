"""Hard limits for an isolated solver child, not for the embedding application."""
import resource
import sys


def child_limits(address_space_bytes=0, file_bytes=0, cpu_seconds=0):
    for value in (address_space_bytes, file_bytes, cpu_seconds):
        if not isinstance(value, int) or value < 0:
            raise ValueError('resource limits must be nonnegative integers')
    if address_space_bytes and sys.platform != 'linux':
        raise ValueError('hard address-space limits require Linux; no RSS substitute')
    if not address_space_bytes and not file_bytes and not cpu_seconds:
        return None

    def apply():
        for key, value in ((resource.RLIMIT_AS, address_space_bytes),
                           (resource.RLIMIT_FSIZE, file_bytes)):
            if not value:
                continue
            _, inherited = resource.getrlimit(key)
            ceiling = value if inherited == resource.RLIM_INFINITY else min(value, inherited)
            resource.setrlimit(key, (ceiling, ceiling))
        if cpu_seconds:
            _, inherited=resource.getrlimit(resource.RLIMIT_CPU)
            hard=cpu_seconds+1 if inherited==resource.RLIM_INFINITY else min(cpu_seconds+1,inherited)
            resource.setrlimit(resource.RLIMIT_CPU,(min(cpu_seconds,hard),hard))
    return apply
