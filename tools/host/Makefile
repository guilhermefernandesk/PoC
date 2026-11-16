# Teste host do cliente Modbus TCP (sem STM32, sem FreeRTOS real).
#
# Uso:
#   make -C tools/host
#   ./tools/host/test_mbtcp 1503 1504
#
# Os portos devem corresponder a dois simuladores:
#   python3 tools/sew_sim.py --port 1503 --profile uhx65a --unit 0
#   python3 tools/sew_sim.py --port 1504 --profile generic --unit 1

CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -O2
CPPFLAGS += -I. -I../../lib/mbtcp -I../../lib/tcpprobe

SRCS = test_mbtcp.c freertos_mock.c tcpprobe_mock.c \
       ../../lib/mbtcp/mbtcp.c \
       ../../lib/mbtcp/mbtcp_codec.c \
       ../../lib/mbtcp/mbtcp_map.c

all: test_mbtcp

test_mbtcp: $(SRCS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SRCS) -lpthread -o $@

clean:
	rm -f test_mbtcp

.PHONY: all clean
