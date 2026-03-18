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
