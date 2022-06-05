#include <cassert>
#include <cstring>
#include <iostream>

#include <arpa/inet.h>
#include <asm/types.h>
#include <linux/inet_diag.h> /* for IPv4 and IPv6 sockets */
#include <linux/netlink.h>
#include <linux/sock_diag.h>
#include <linux/unix_diag.h> /* for UNIX domain sockets */
#include <netinet/in.h>
#include <pwd.h>
#include <sys/socket.h>

#include <clipp/clipp.hpp>

#include "io.hpp"

namespace {
struct NetlinkArgs : clipp::ArgsBase {
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

std::string addrToString(int family, const void* addr)
{
    char buf[INET6_ADDRSTRLEN];
    if (!::inet_ntop(family, addr, buf, INET6_ADDRSTRLEN)) {
        return "<ERROR>";
    }
    return buf;
}

bool processNetlinkMessage(const inet_diag_msg& msg, Output& output)
{
    if (msg.idiag_family != AF_INET && msg.idiag_family != AF_INET6) {
        std::cerr << "Unexpected family in netlink response: " << msg.idiag_family << std::endl;
        return false;
    }
    const auto user = ::getpwuid(msg.idiag_uid);
    output.row({
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
    });
    return true;
}

bool receiveNetlinkResponse(int fd, Output& output)
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
            if (!processNetlinkMessage(*data, output)) {
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
    const auto args = parser.parse<NetlinkArgs>(argc, argv).value();

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

    Output output(columns);

    const auto diagSocket = ::socket(AF_NETLINK, SOCK_RAW, NETLINK_SOCK_DIAG);
    if (diagSocket == -1) {
        std::cerr << "could not create netlink socket" << std::endl;
        return 2;
    }

    auto req4 = makeInetRequest(IpFamily::v4, Protocol::Tcp);
    if (!sendNetlinkRequest(diagSocket, req4)) {
        std::cerr << "Could not send netlink request" << std::endl;
        return 3;
    }
    if (!receiveNetlinkResponse(diagSocket, output)) {
        std::cerr << "Could not process netlink response (IPv4, TCP)" << std::endl;
        return 4;
    }
    auto req6 = makeInetRequest(IpFamily::v6, Protocol::Tcp);
    if (!sendNetlinkRequest(diagSocket, req6)) {
        std::cerr << "Could not send netlink request" << std::endl;
        return 3;
    }
    if (!receiveNetlinkResponse(diagSocket, output)) {
        std::cerr << "Could not process netlink response (IPv6, TCP)" << std::endl;
        return 4;
    }

    return 0;
}
