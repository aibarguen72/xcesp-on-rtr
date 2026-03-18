/**
 * @file    IpCmd.h
 * @brief   IpCmd — thin wrapper for iproute2 shell commands
 * @project xcesp-on-rtr
 * @date    2026-03-18
 *
 * All methods are static.  Commands are executed via popen() / system().
 * Requires iproute2 (`ip`) to be installed and xcespproc to run with
 * CAP_SYS_ADMIN (or root) for namespace and interface operations.
 */

#ifndef IPCMD_H
#define IPCMD_H

#include <string>

class IpCmd {
public:
    /**
     * @brief  Build the "ip [-n <ns>]" prefix for a command.
     *
     * Returns "ip" for nsName == "default" or empty string.
     * Returns "ip -n <nsName>" for any other namespace name.
     */
    static std::string ns(const std::string& nsName);

    /**
     * @brief  Execute a command and return its stdout output.
     *
     * Stderr is suppressed.  Returns empty string if the command fails
     * (non-zero exit) or cannot be started.
     */
    static std::string run(const std::string& cmd);

    /**
     * @brief  Execute a command; return true if exit code is 0.
     *
     * Stdout and stderr are both suppressed.
     */
    static bool exec(const std::string& cmd);

    /**
     * @brief  Parse the operstate value from `ip -j link show` JSON output.
     *
     * Searches for the first `"operstate":"<VALUE>"` pair and returns VALUE.
     * Returns empty string if not found.
     */
    static std::string parseOperstate(const std::string& json);
};

#endif // IPCMD_H
