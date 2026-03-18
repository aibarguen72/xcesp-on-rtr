/**
 * @file    IpCmd.cpp
 * @brief   IpCmd — thin wrapper for iproute2 shell commands
 * @project xcesp-on-rtr
 * @date    2026-03-18
 */

#include "IpCmd.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <sstream>

std::string IpCmd::ns(const std::string& nsName)
{
    if (nsName.empty() || nsName == "default")
        return "ip";
    return "ip -n " + nsName;
}

std::string IpCmd::run(const std::string& cmd)
{
    std::string full = cmd + " 2>/dev/null";
    FILE* fp = popen(full.c_str(), "r");
    if (!fp)
        return "";

    std::string result;
    std::array<char, 512> buf{};
    while (fgets(buf.data(), static_cast<int>(buf.size()), fp))
        result += buf.data();

    pclose(fp);
    return result;
}

bool IpCmd::exec(const std::string& cmd)
{
    int rc = system((cmd + " >/dev/null 2>&1").c_str());
    return (rc == 0);
}

std::string IpCmd::parseOperstate(const std::string& json)
{
    // Search for "operstate":"VALUE" — simple substring scan
    static const std::string key = "\"operstate\":\"";
    size_t pos = json.find(key);
    if (pos == std::string::npos)
        return "";
    pos += key.size();
    size_t end = json.find('"', pos);
    if (end == std::string::npos)
        return "";
    return json.substr(pos, end - pos);
}

std::vector<std::string> IpCmd::parseAddrList(const std::string& addrShow)
{
    // Parse lines of `ip addr show dev <dev>` text output.
    // Lines containing "    inet " (not "inet6 ") carry IPv4 addresses.
    // The address token immediately follows "inet " and ends at the next space.
    std::vector<std::string> result;
    std::istringstream ss(addrShow);
    std::string line;
    while (std::getline(ss, line)) {
        auto pos = line.find("inet ");
        if (pos == std::string::npos)
            continue;
        // Skip IPv6 lines: "inet6 " has '6' at pos+4, not ' '
        if (pos + 4 < line.size() && line[pos + 4] == '6')
            continue;
        pos += 5; // skip "inet "
        auto end = line.find(' ', pos);
        if (end == std::string::npos)
            end = line.size();
        std::string addr = line.substr(pos, end - pos);
        if (!addr.empty())
            result.push_back(addr);
    }
    return result;
}
