# mbtcp.mk - fragmento de Makefile
#
# Uso no Makefile do projeto (depois de C_SOURCES / C_INCLUDES):
#     include lib/tcpprobe/tcpprobe.mk
#     include lib/mbtcp/mbtcp.mk
#     include lib/uhx15a/uhx15a.mk
#
# Adiciona somente o transporte Modbus TCP. O driver UHX15A fica em
# uhx15a.mk; map/codec/demo antigos nao fazem mais parte do build.

ifeq ($(MBTCP_INCLUDED),)
MBTCP_INCLUDED := 1
MBTCP_DIR := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
TCPPROBE_DIR ?= $(abspath $(MBTCP_DIR)/../tcpprobe)
C_SOURCES  += $(MBTCP_DIR)/mbtcp.c
C_INCLUDES += -I$(MBTCP_DIR) -I$(TCPPROBE_DIR)
endif
