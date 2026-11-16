# mbtcp.mk - fragmento de Makefile
#
# Uso no Makefile do projeto (depois de C_SOURCES / C_INCLUDES):
#     include lib/tcpprobe/tcpprobe.mk
#     include lib/mbtcp/mbtcp.mk
#
# Adiciona 3 fontes (transporte, codec e mapa) e os includes de mbtcp
# e tcpprobe. Nao compila mbtcp_demo.c (exemplo/autoteste).

ifeq ($(MBTCP_INCLUDED),)
MBTCP_INCLUDED := 1
MBTCP_DIR := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
TCPPROBE_DIR ?= $(abspath $(MBTCP_DIR)/../tcpprobe)
C_SOURCES  += $(MBTCP_DIR)/mbtcp.c
C_SOURCES  += $(MBTCP_DIR)/mbtcp_codec.c
C_SOURCES  += $(MBTCP_DIR)/mbtcp_map.c
C_INCLUDES += -I$(MBTCP_DIR) -I$(TCPPROBE_DIR)
endif
