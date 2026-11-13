#include "client.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {

std::string socketPath() {
    const char *runtime = std::getenv("XDG_RUNTIME_DIR");
    std::string dir = runtime && *runtime ? runtime : "/run/user/" + std::to_string(getuid());
    return dir + "/soz/soz.sock";
}

int connectSocket() {
    const std::string path = socketPath();
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    if (path.size() >= sizeof(address.sun_path))
        return -1;
    std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return -1;
    if (::connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
        ::close(fd);
        return -1;
    }
    timeval timeout{5, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    return fd;
}

void sleepMs(int ms) {
    timespec t{ms / 1000, (ms % 1000) * 1000000L};
    nanosleep(&t, nullptr);
}

}

namespace Client {

bool reachable() {
    const int fd = connectSocket();
    if (fd < 0)
        return false;
    ::close(fd);
    return true;
}

int send(const std::string &command, bool startService) {
    int fd = connectSocket();
    if (fd < 0 && startService) {
        // The service is started by `soz on`; this covers a crash or a
        // shortcut pressed in the moment before it came up.
        std::system("systemctl --user start --no-block soz.service >/dev/null 2>&1");
        for (int i = 0; i < 150 && fd < 0; ++i) {
            sleepMs(20);
            fd = connectSocket();
        }
    }
    if (fd < 0) {
        std::fprintf(stderr, "soz is not running. Turn it on with `soz on`.\n");
        return 1;
    }

    const std::string line = command + "\n";
    if (::write(fd, line.data(), line.size()) != ssize_t(line.size())) {
        std::fprintf(stderr, "Could not talk to soz: %s\n", std::strerror(errno));
        ::close(fd);
        return 1;
    }
    std::string response;
    char buffer[4096];
    ssize_t n;
    while ((n = ::read(fd, buffer, sizeof(buffer))) > 0)
        response.append(buffer, size_t(n));
    ::close(fd);

    std::fputs(response.c_str(), stdout);
    return response.find("\"ok\":true") != std::string::npos ? 0 : 1;
}

}
