# tcpprobe.mk - fragmento de Makefile
#
# Uso no Makefile do projeto (depois de C_SOURCES / C_INCLUDES):
#     include lib/tcpprobe/tcpprobe.mk
#
# Adiciona 1 fonte e 1 include. Nao compila example.c.

ifeq ($(TCPPROBE_INCLUDED),)
TCPPROBE_INCLUDED := 1

TCPPROBE_DIR := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

C_SOURCES  += $(TCPPROBE_DIR)/tcpprobe.c
C_INCLUDES += -I$(TCPPROBE_DIR)

endif
