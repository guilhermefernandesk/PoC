#include "app_regs.h"
#include "mb_server.h"
#include "mb_transport_posix.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    uint16_t port = 5020u;

    if (argc > 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        return 2;
    }
    if (argc == 2) {
        const long parsed = strtol(argv[1], NULL, 10);
        if ((parsed < 1) || (parsed > 65535)) {
            fprintf(stderr, "invalid port: %s\n", argv[1]);
            return 2;
        }
        port = (uint16_t)parsed;
    }

    (void)signal(SIGPIPE, SIG_IGN);
    (void)setvbuf(stdout, NULL, _IONBF, 0);

    app_regs_init();

    const mb_server_cfg_t cfg = {
        .transport = mb_transport_posix_get(),
        .map = app_regs_map(),
        .port = port,
        .accept_timeout_ms = 1000u,
        .recv_timeout_ms = 60000u,
    };

    printf("Modbus TCP server listening on 0.0.0.0:%u (unit %u)\n",
           (unsigned)port, (unsigned)MB_UNIT_ID_DEFAULT);

    const int result = mb_server_run(&cfg);
    fprintf(stderr, "mb_server_run failed: %d\n", result);
    return 1;
}
