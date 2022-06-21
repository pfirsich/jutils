#include <cassert>
#include <cstring>
#include <iostream>

#include <arpa/inet.h>
#include <asm/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/inet_diag.h> /* for IPv4 and IPv6 sockets */
#include <linux/netlink.h>
#include <linux/sock_diag.h>
#include <linux/unix_diag.h> /* for UNIX domain sockets */
#include <netinet/in.h>
#include <pwd.h>
#include <sys/socket.h>
#include <unistd.h>

#include <clipp/clipp.hpp>

#include "io.hpp"
#include "util.hpp"

/* Sources:
 * https://www.man7.org/linux/man-pages/man7/sock_diag.7.html
 * https://www.man7.org/linux/man-pages/man7/netlink.7.html
 * https://www.man7.org/linux/man-pages/man3/netlink.3.html
 *
 * http://kristrev.github.io/2013/07/26/passive-monitoring-of-sockets-on-linux
 * https://github.com/kristrev/inet-diag-example
 * https://www.linuxjournal.com/article/7356
 */

namespace {
struct NetstatArgs : clipp::ArgsBase {
    bool tcp = false;
    bool udp = false;
    bool ipv4 = false;
    bool ipv6 = false;
    bool process = false;

    void args()
    {
        flag(tcp, "tcp", 't');
        flag(udp, "udp", 'u');
        flag(ipv4, "ipv4", '4');
        flag(ipv6, "ipv6", '6');
        flag(process, "process", 'p');
    }
};

template <typename R>
struct Request {
    ::nlmsghdr header;
    R request;
};

using InetRequest = Request<::inet_diag_req_v2>;

enum class IpFamily {
    v4 = AF_INET,
    v6 = AF_INET6,
};

std::string toString(IpFamily family)
{
    switch (family) {
    case IpFamily::v4:
        return "IPv4";
    case IpFamily::v6:
        return "IPv6";
    default:
        return "invalid";
    }
}

enum class Protocol {
    Tcp = IPPROTO_TCP,
    Udp = IPPROTO_UDP,
    UdpLite = IPPROTO_UDPLITE,
};

std::string toString(Protocol protocol)
{
    switch (protocol) {
    case Protocol::Tcp:
        return "TCP";
    case Protocol::Udp:
        return "UDP";
    case Protocol::UdpLite:
        return "UDP-Lite";
    default:
        return "invalid";
    }
}

// https://github.com/torvalds/linux/blob/master/include/net/tcp_states.h
enum class TcpState {
    Established = 1,
    SynSent,
    SynRecv,
    FinWait1,
    FinWait2,
    TimeWait,
    Close,
    CloseWait,
    LastAck,
    Listen,
    Closing,
    NewSynRecv,
};

std::string toString(TcpState state)
{
    switch (state) {
    case TcpState::Established:
        return "ESTABLISHED";
    case TcpState::SynSent:
        return "SYN-SENT";
    case TcpState::SynRecv:
        return "SYN-RECV";
    case TcpState::FinWait1:
        return "FIN-WAIT-1";
    case TcpState::FinWait2:
        return "FIN-WAIT-2";
    case TcpState::TimeWait:
        return "TIME-WAIT";
    case TcpState::Close:
        return "CLOSE";
    case TcpState::CloseWait:
        return "CLOSE-WAIT";
    case TcpState::LastAck:
        return "LAST-ACK";
    case TcpState::Listen:
        return "LISTEN";
    case TcpState::Closing:
        return "CLOSING";
    case TcpState::NewSynRecv:
        return "NEW-SYN-RECV";
    default:
        return "invalid";
    }
}

InetRequest makeInetRequest(IpFamily ipFamily, Protocol protocol)
{
    InetRequest req;
    std::memset(&req.header, 0, sizeof(req.header));
    req.header.nlmsg_type = SOCK_DIAG_BY_FAMILY;
    req.header.nlmsg_len = sizeof(InetRequest);
    req.header.nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST;

    std::memset(&req.request, 0, sizeof(req.request));
    req.request.sdiag_family = static_cast<uint8_t>(ipFamily);
    req.request.sdiag_protocol = static_cast<uint8_t>(protocol);
    req.request.idiag_ext = 0; // INET_DIAG_MEMINFO;
    req.request.pad = 0;
    req.request.idiag_states = 0xffff;

    return req;
}

template <typename Request>
bool sendNetlinkRequest(int fd, Request& req)
{
    ::sockaddr_nl sa { AF_NETLINK, 0, 0, 0 };

    ::iovec iov { &req, sizeof(req) };

    ::msghdr msg;
    std::memset(&msg, 0, sizeof(msg));
    msg.msg_name = &sa;
    msg.msg_namelen = sizeof(sa);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    return ::sendmsg(fd, &msg, 0) > 0;
}

struct Socket {
    int fd;
    int pid;
    std::string comm;
};

std::unordered_map<uint32_t, std::vector<Socket>>& getSocketInodeMap()
{
    static std::unordered_map<uint32_t, std::vector<Socket>> map;
    return map;
}

// This is roughly taken from the ss source code and it doing it this way suggests, there is no
// nice(r) way to get the pid from a socket inode.
void initSocketInodeMap()
{
    auto procDir = ::opendir("/proc/");
    if (!procDir) {
        std::cerr << "Could not open /proc/" << std::endl;
        std::exit(7);
    }

    auto& socketInodeMap = getSocketInodeMap();

    ::dirent* procDirent;
    while ((procDirent = ::readdir(procDir))) {
        if (procDirent->d_type != DT_DIR) {
            continue;
        }

        int pid;
        if (::sscanf(procDirent->d_name, "%d", &pid) != 1) {
            continue;
        }

        const auto procPath = std::string("/proc/") + procDirent->d_name;

        const auto fdPath = procPath + "/fd/";
        auto fdDir = ::opendir(fdPath.c_str());
        if (!fdDir) {
            std::cerr << "DBG: can't open " << fdPath << std::endl;
            continue;
        }

        // This seems to be exactly the same as the second field of /stat (which is what ss uses),
        // but easier to retrieve
        const auto commPath = procPath + "/comm";
        const auto commFileData = readFile(commPath);
        if (!commFileData) {
            std::cerr << "Could not read " << commPath << std::endl;
            continue;
        }
        // Remove trailing newline
        const auto comm = commFileData->substr(0, commFileData->size() - 1);

        ::dirent* fdDirent;
        while ((fdDirent = ::readdir(fdDir))) {
            if (fdDirent->d_type != DT_LNK) {
                continue;
            }

            int fd;
            if (::sscanf(fdDirent->d_name, "%d", &fd) != 1) {
                continue;
            }

            const auto path = fdPath + fdDirent->d_name;
            char targetBuffer[256];
            const auto res = ::readlink(path.c_str(), targetBuffer, sizeof(targetBuffer));
            if (res < 0) {
                std::cerr << "Error reading symlink target of " << path << std::endl;
                continue;
            }

            uint32_t inode = 0;
            const auto scanRes = ::sscanf(targetBuffer, "socket:[%u]", &inode);
            if (scanRes < 1) {
                // Might just be something else
                continue;
            }

            socketInodeMap[inode].push_back(Socket { fd, pid, comm });
        }
        ::closedir(fdDir);
    }
    ::closedir(procDir);
}

std::string addrToString(int family, const void* addr)
{
    char buf[INET6_ADDRSTRLEN];
    if (!::inet_ntop(family, addr, buf, INET6_ADDRSTRLEN)) {
        return "<ERROR>";
    }
    return buf;
}

bool processNetlinkMessage(const inet_diag_msg& msg, Output& output, const NetstatArgs& args)
{
    if (msg.idiag_family != AF_INET && msg.idiag_family != AF_INET6) {
        std::cerr << "Unexpected family in netlink response: " << msg.idiag_family << std::endl;
        return false;
    }
    const auto user = ::getpwuid(msg.idiag_uid);
    std::vector<Value> values {
        toString(static_cast<IpFamily>(msg.idiag_family)),
        toString(static_cast<TcpState>(msg.idiag_state)),
        static_cast<int64_t>(msg.idiag_timer),
        static_cast<int64_t>(msg.idiag_retrans),
        static_cast<int64_t>(msg.idiag_expires),
        addrToString(msg.idiag_family, &msg.id.idiag_src),
        static_cast<int64_t>(::ntohs(msg.id.idiag_sport)),
        addrToString(msg.idiag_family, &msg.id.idiag_dst),
        static_cast<int64_t>(::ntohs(msg.id.idiag_dport)),
        // idiag_if could be good
        static_cast<int64_t>(msg.idiag_rqueue),
        static_cast<int64_t>(msg.idiag_wqueue),
        user ? std::string(user->pw_name) : std::string("<ERROR>"),
        static_cast<int64_t>(msg.idiag_inode),
    };
    if (args.process) {
        const auto& map = getSocketInodeMap();
        const auto it = map.find(msg.idiag_inode);
        if (it == map.end()) {
            // Might just be a race condition, which is impossible to prevent
            values.push_back(static_cast<int64_t>(-1));
            values.push_back(static_cast<int64_t>(-1));
            values.push_back(std::string(""));
        } else {
            assert(it->second.size() > 0);
            // TODO: Figure out what to do here!
            values.push_back(static_cast<int64_t>(it->second[0].fd));
            values.push_back(static_cast<int64_t>(it->second[0].pid));
            values.push_back(it->second[0].comm);
        }
    }
    output.row(values);
    return true;
}

bool processNetlinkResponse(int fd, Output& output, const NetstatArgs& args)
{
    char recvBuffer[4096];
    while (true) {
        auto num = ::recv(fd, recvBuffer, sizeof(recvBuffer), 0);
        auto header = reinterpret_cast<::nlmsghdr*>(recvBuffer);

        while (NLMSG_OK(header, num)) {
            if (header->nlmsg_type == NLMSG_DONE) {
                return true;
            }

            if (header->nlmsg_type == NLMSG_ERROR) {
                const auto err = reinterpret_cast<const ::nlmsgerr*>(NLMSG_DATA(header));
                if (header->nlmsg_len < NLMSG_LENGTH(sizeof(*err))) {
                    std::cerr << "Received netlink error" << std::endl;
                } else {
                    std::cerr << "Netlink error: " << -err->error << std::endl;
                }
                return false;
            }

            if (header->nlmsg_type != SOCK_DIAG_BY_FAMILY) {
                std::cerr << "Unexpected netlink message type: " << header->nlmsg_type << std::endl;
                return false;
            }

            const auto data = reinterpret_cast<const ::inet_diag_msg*>(NLMSG_DATA(header));
            if (!processNetlinkMessage(*data, output, args)) {
                return false;
            }
            header = NLMSG_NEXT(header, num);
        }

        if (num > 0) {
            std::cerr << num << " bytes left" << std::endl;
        }
    }
    return true;
}
}

int netstat(int argc, char** argv)
{
    auto parser = clipp::Parser(argv[0]);
    const auto args = parser.parse<NetstatArgs>(argc, argv).value();

    std::vector<Column> columns {
        { "family", Column::Type::String },
        { "state", Column::Type::String },
        { "timer", Column::Type::I64 },
        { "retrans", Column::Type::I64 },
        { "expires", Column::Type::I64 },
        { "srcaddr", Column::Type::String },
        { "srcport", Column::Type::I64 },
        { "dstaddr", Column::Type::String },
        { "dstport", Column::Type::I64 },
        { "rqueue", Column::Type::I64 },
        { "wqueue", Column::Type::I64 },
        { "user", Column::Type::String },
        { "inode", Column::Type::I64 },
    };

    if (args.process) {
        columns.push_back({ "fd", Column::Type::I64 });
        columns.push_back({ "pid", Column::Type::I64 });
        columns.push_back({ "comm", Column::Type::String });
        initSocketInodeMap();
    }

    const auto defaultProtocols = !args.tcp && !args.udp;
    std::vector<Protocol> protocols;
    if (defaultProtocols || args.tcp) {
        protocols.push_back(Protocol::Tcp);
    }
    if (defaultProtocols || args.udp) {
        protocols.push_back(Protocol::Udp);
    }

    const auto defaultFamilies = !args.ipv4 && !args.ipv6;
    std::vector<IpFamily> families;
    if (defaultFamilies || args.ipv4) {
        families.push_back(IpFamily::v4);
    }
    if (defaultFamilies || args.ipv6) {
        families.push_back(IpFamily::v6);
    }

    Output output(columns);

    const auto diagSocket = ::socket(AF_NETLINK, SOCK_RAW, NETLINK_SOCK_DIAG);
    if (diagSocket == -1) {
        std::cerr << "could not create netlink socket" << std::endl;
        return 2;
    }

    for (const auto protocol : protocols) {
        for (const auto family : families) {
            auto req = makeInetRequest(family, protocol);
            if (!sendNetlinkRequest(diagSocket, req)) {
                std::cerr << "Could not send netlink request "
                          << "(" << toString(family) << "/" << toString(protocol) << ")"
                          << std::endl;
                return 3;
            }
            if (!processNetlinkResponse(diagSocket, output, args)) {
                std::cerr << "Could not process netlink response "
                          << "(" << toString(family) << "/" << toString(protocol) << ")"
                          << std::endl;
                return 4;
            }
        }
    }

    return 0;
}
