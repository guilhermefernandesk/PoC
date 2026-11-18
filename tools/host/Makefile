# Testes host do transporte Modbus TCP e do driver UHX15A (sem STM32).
#
# Uso:
#   make -C tools/host
#   ./tools/host/test_uhx15a
#
# O teste do driver usa a porta definida em UHX_PORT (default 1503), que
# precisa corresponder ao simulador:
#   python3 tools/sew_sim.py --port 1503 --profile uhx15a --unit 0 \
#       --fieldbus-timeout 0
#
# test_faults nao precisa de simulador: ele abre servidores TCP minimos
# para testar timeout, MBAP invalido, excecao e reconexao.

CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -O2
UHX_PORT ?= 1503
CPPFLAGS += -I. -I../../lib/mbtcp -I../../lib/uhx15a -I../../lib/tcpprobe
CPPFLAGS += -DUHX15A_IP='"127.0.0.1"' -DUHX15A_PORT=$(UHX_PORT)

SANITIZER_FLAGS = -fsanitize=address,undefined -fno-omit-frame-pointer

COMMON_SRCS = freertos_mock.c tcpprobe_mock.c ../../lib/mbtcp/mbtcp.c
DRIVER_SRCS = $(COMMON_SRCS) ../../lib/uhx15a/uhx15a.c

all: test_uhx15a test_faults

test_uhx15a: test_uhx15a.c $(DRIVER_SRCS)
	$(CC) $(CFLAGS) $(CPPFLAGS) test_uhx15a.c $(DRIVER_SRCS) -lpthread -o $@

test_faults: test_faults.c $(COMMON_SRCS)
	$(CC) $(CFLAGS) $(CPPFLAGS) test_faults.c $(COMMON_SRCS) -lpthread -o $@

test_uhx15a_asan: test_uhx15a.c $(DRIVER_SRCS)
	$(CC) $(CFLAGS) $(SANITIZER_FLAGS) $(CPPFLAGS) \
	    test_uhx15a.c $(DRIVER_SRCS) -lpthread -o $@

test_faults_asan: test_faults.c $(COMMON_SRCS)
	$(CC) $(CFLAGS) $(SANITIZER_FLAGS) $(CPPFLAGS) \
	    test_faults.c $(COMMON_SRCS) -lpthread -o $@

clean:
	rm -f test_uhx15a test_faults test_uhx15a_asan test_faults_asan

.PHONY: all clean
