# Testes host do cliente Modbus TCP (sem STM32, sem FreeRTOS real).
#
# Uso:
#   make -C tools/host
#   ./tools/host/test_mbtcp 1503
#
# O porto deve corresponder ao simulador:
#   python3 tools/sew_sim.py --port 1503 --profile uhx65a --unit 0
#
# test_faults nao precisa de simulador: ele abre servidores TCP minimos
# para testar timeout, MBAP invalido, excecao e reconexao.

CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -O2
CPPFLAGS += -I. -I../../lib/mbtcp -I../../lib/tcpprobe

SANITIZER_FLAGS = -fsanitize=address,undefined -fno-omit-frame-pointer

LIB_SRCS = freertos_mock.c tcpprobe_mock.c \
           ../../lib/mbtcp/mbtcp.c \
           ../../lib/mbtcp/mbtcp_codec.c \
           ../../lib/mbtcp/mbtcp_map.c

all: test_mbtcp test_faults

test_mbtcp: test_mbtcp.c $(LIB_SRCS)
	$(CC) $(CFLAGS) $(CPPFLAGS) test_mbtcp.c $(LIB_SRCS) -lpthread -o $@

test_faults: test_faults.c $(LIB_SRCS)
	$(CC) $(CFLAGS) $(CPPFLAGS) test_faults.c $(LIB_SRCS) -lpthread -o $@

test_mbtcp_asan: test_mbtcp.c $(LIB_SRCS)
	$(CC) $(CFLAGS) $(SANITIZER_FLAGS) $(CPPFLAGS) \
	    test_mbtcp.c $(LIB_SRCS) -lpthread -o $@

test_faults_asan: test_faults.c $(LIB_SRCS)
	$(CC) $(CFLAGS) $(SANITIZER_FLAGS) $(CPPFLAGS) \
	    test_faults.c $(LIB_SRCS) -lpthread -o $@

clean:
	rm -f test_mbtcp test_faults test_mbtcp_asan test_faults_asan

.PHONY: all clean
