// Copyright(c) 2021, Intel Corporation
//
// Redistribution  and  use  in source  and  binary  forms,  with  or  without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of  source code  must retain the  above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name  of Intel Corporation  nor the names of its contributors
//   may be used to  endorse or promote  products derived  from this  software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,  BUT NOT LIMITED TO,  THE
// IMPLIED WARRANTIES OF  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT  SHALL THE COPYRIGHT OWNER  OR CONTRIBUTORS BE
// LIABLE  FOR  ANY  DIRECT,  INDIRECT,  INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR
// CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT LIMITED  TO,  PROCUREMENT  OF
// SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,  DATA, OR PROFITS;  OR BUSINESS
// INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY  OF LIABILITY,  WHETHER IN
// CONTRACT,  STRICT LIABILITY,  OR TORT  (INCLUDING NEGLIGENCE  OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,  EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>

#include "intel_fpga_platform_api.h"
#include "intel_st_debug_if_remote_dbg.h"
#include "intel_st_debug_if_stream_dbg.h"
#include "app_version.h"
#include "intel_fpga_api.h"

// etherlink Command line input help
static void show_help(const char* program)
{
    printf(
        "Usage:\n"
        " %s [--uio-driver-path=<path>] [--start-address=<address>] [--h2t-t2h-mem-size=<size>] "
        "[--port=<port>] [--ip=<ip address>]\n"
        " %s --version\n"
        " %s --help\n\n"
        "Optional arguments:\n"
        " --uio-driver-path=<path>, -u <path>       UIO driver path (default: /dev/uio0)\n"
        " --start-address=<address>, -s <address>   IP CSR interface starting address "
        "within this UIO driver (default: 0)\n"
        " --h2t-t2h-mem-size=<size>, -m <size>      H2T/T2H memory size in "
        "bytes (default: 4096)\n"
        " --port=<port>, -p <port>                  Listening port (default: 0)\n"
        " --version, -v                             Print version and exit\n"
        " --help, -h                                Print this usage description\n"
        "\n"
        "Note:\n"
        " In the device tree, the address span of the whole CSR interface should be "
        "bound to the specified UIO driver.\n"
        " Typically, the base address starts at 0x0.\n\n"
        " The option --h2t-t2h-mem-size is not used for HS ST Debug Interface IP because the "
        "size information is available on\n"
        "the CSR interface.\n\n",
        program,
        program,
        program);
}

static void show_version()
{
    printf("%s-%s\n", APP_VERSION_BASE, GIT_VERSION);
}

static IRemoteDebug* s_etherlink_server = nullptr;

// Streaming debug command line struct
enum
{
    IP_MAX_STR_LEN = 15
};

struct EtherlinkCommandLine
{
    size_t h2t_t2h_mem_size;
    int port;
    char ip[IP_MAX_STR_LEN + 1];
};

static int parse_cmd_args(EtherlinkCommandLine* etherlink_cmdline, int argc, char* argv[]);
static long parse_integer_arg(const char* name);
static int run_etherlink(const struct EtherlinkCommandLine* etherlink_cmdline);
static void install_sigint_handler();

class StreamingDebug : public IRemoteDebug
{
public:
    StreamingDebug() {}
    virtual ~StreamingDebug() { terminate(); }
    int run(size_t h2t_t2h_mem_size, const char* /*unused*/, int port) override
    {
        const int fpga_index = 0;  // Only 1 IP instance is supported.
        FPGA_MMIO_INTERFACE_HANDLE handle = fpga_open(fpga_index);
        init_st_dbg_transport_server_over_tcpip(&m_server_context, handle, h2t_t2h_mem_size, port);
        return start_st_dbg_transport_server_over_tcpip(&m_server_context);
    }

    void terminate() override
    {
        fpga_close(m_server_context.driver_cxt.mmio_handle);
        terminate_st_dbg_transport_server_over_tcpip();
    }

private:
    intel_remote_debug_server_context m_server_context;
};

int main(int argc, char** argv)
{
    EtherlinkCommandLine etherlink_cmdline = {4096,
                                              0,
                                              {
                                                  0,
                                              }};
    int rc = parse_cmd_args(&etherlink_cmdline, argc, argv);
    if (rc)
    {
        switch (rc)
        {
            case -2:
                show_help(argv[0]);
                break;

            case -4:
                show_version();
                break;

            default:
                printf("ERROR: Error scanning command line; exiting\n\n");
        }
        /*
                if ( rc != -2 ){
                    printf("ERROR: Error scanning command line; exiting\n\n");
                }

                show_help(argv[0]);
        */
        goto out_exit;
    }

    printf("INFO: Etherlink Server Configuration:\n");
    printf("INFO:    H2T/T2H Memory Size  : %ld\n", etherlink_cmdline.h2t_t2h_mem_size);
    printf("INFO:    Listening Port       : %d\n", etherlink_cmdline.port);
    printf("INFO:    IP Address           : %s\n", etherlink_cmdline.ip);

    if (fpga_platform_init(argc, (const char**) argv) == false)
    {
        printf("ERROR: Platform failed to initialize; exiting\n\n");
        show_help(argv[0]);
        rc = -1;
        goto out_exit;
    }

    // Install SIGINT handler
    install_sigint_handler();

    if (run_etherlink(&etherlink_cmdline) != 0)
    {
        printf("ERROR: Etherlink server failed to start successfully; exiting.\n");
        rc = 3;
    }

out_exit:
    fpga_platform_cleanup();

    return rc;
}

int run_etherlink(const struct EtherlinkCommandLine* etherlink_cmdline)
{
    int res = 0;

    s_etherlink_server = new StreamingDebug();
    if (s_etherlink_server)
    {
        res = s_etherlink_server->run(
            etherlink_cmdline->h2t_t2h_mem_size, etherlink_cmdline->ip, etherlink_cmdline->port);
        delete s_etherlink_server;
        s_etherlink_server = nullptr;
    }

    return res;
}

// parse Input command line
int parse_cmd_args(EtherlinkCommandLine* etherlink_cmdline, int argc, char* argv[])
{
    int option_index = 0;
    int c;

    const char* GETOPT_STRING = "hp:i:v";

    struct option longopts[] = {{"help", no_argument, NULL, 'h'},
                                {"version", no_argument, NULL, 'v'},
                                {"h2t-t2h-mem-size", required_argument, NULL, 'm'},
                                {"port", required_argument, NULL, 'p'},
                                {"ip", required_argument, NULL, 'i'},
                                {0, 0, 0, 0}};

    opterr = 0;  // Suppress stderr output from getopt_long upon unrecognized options
    optind = 0;  // Reset getopt_long position.

    while (1)
    {
        c = getopt_long(argc, (char* const*) argv, GETOPT_STRING, longopts, &option_index);

        if (c == -1)
        {
            break;
        }

        switch (c)
        {
            case 'v':
                return -4;
                break;

            case 'h':
                // Command line help
                return -2;
                break;

            case 'm':
                // H2T/T2H Mem Size
                etherlink_cmdline->h2t_t2h_mem_size = parse_integer_arg("h2t-t2h-mem-size");
                if (etherlink_cmdline->h2t_t2h_mem_size == 0)
                {
                    return -3;
                }
                break;

            case 'p':
                // TCP Port
                etherlink_cmdline->port = parse_integer_arg("port");
                break;

            case 'i':
                // Ip address
                strncpy(etherlink_cmdline->ip, optarg, 15);
                etherlink_cmdline->ip[15] = '\0';
                break;
        }
    }

    if (etherlink_cmdline->ip[0] == '\0')
    {
        strncpy(etherlink_cmdline->ip, "0.0.0.0", sizeof(etherlink_cmdline->ip));
    }

    return 0;
}

long parse_integer_arg(const char* name)
{
    long ret = 0;

    bool is_all_digit = true;
    char* p;
    typedef int (*DIGIT_TEST_FN)(int c);
    DIGIT_TEST_FN is_acceptabl_digit;
    if (optarg[0] == '0' && (optarg[1] == 'x' || optarg[1] == 'X'))
    {
        is_acceptabl_digit = isxdigit;
        optarg += 2;  // trim the "0x" portion
    }
    else
    {
        is_acceptabl_digit = isdigit;
    }

    for (p = optarg; (*p) != '\0'; ++p)
    {
        if (!is_acceptabl_digit(*p))
        {
            is_all_digit = false;
            break;
        }
    }

    if (is_acceptabl_digit == isxdigit)
    {
        optarg -= 2;  // restore the "0x" portion
    }

    if (is_all_digit)
    {
        if (sizeof(size_t) <= sizeof(long))
        {
            ret = (size_t) strtol(optarg, NULL, 0);
            if (errno == ERANGE)
            {
                ret = 0;
                printf("ERROR: %s value is too big. %s is provided; maximum accepted is %ld\n",
                       name,
                       optarg,
                       LONG_MAX);
            }
        }
        else
        {
            long span, span_c;
            span = strtol(optarg, NULL, 0);
            if (errno == ERANGE)
            {
                ret = 0;
                printf("ERROR: %s value is too big. %s is provided; maximum accepted is %ld\n",
                       name,
                       optarg,
                       LONG_MAX);
            }
            else
            {
                ret = (size_t) span;
                span_c = ret;
                if (span != span_c)
                {
                    printf("ERROR: %s value is too big. %s is provided; maximum accepted is %ld\n",
                           name,
                           optarg,
                           (size_t) -1);
                    ret = 0;
                }
            }
        }
    }
    else
    {
        printf(
            "ERROR: Invalid argument value type is provided. A integer value is expected. %s is "
            "provided.\n",
            optarg);
    }

    return ret;
}

void etherlink_sig_handler(int signo)
{
    if (signo == SIGINT)
    {
        printf("\nINFO: Signal, SIGINT, was triggered; the program is terminating.\n");
        fpga_platform_cleanup();
        if (s_etherlink_server != nullptr)
        {
            delete s_etherlink_server;
            s_etherlink_server = nullptr;
        }
        exit(0);
    }
    else
    {
        printf("WARNING: Unexpected signal, %d, triggered; it is ignored\n", signo);
    }
}

void install_sigint_handler()
{
    struct sigaction sig_action;
    memset(&sig_action, 0, sizeof(sig_action));
    sig_action.sa_handler = &etherlink_sig_handler;
    sig_action.sa_flags = 0;

    if (sigaction(SIGINT, &sig_action, NULL) != 0)
    {
        printf(
            "WARNING: SIGINT handler installment failed; this program will not terminate "
            "gracefully.\n");
    }
}
