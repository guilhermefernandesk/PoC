# uhx15a.mk - fragmento de Makefile do driver UHX15A
#
# Uso no Makefile do projeto:
#     include lib/mbtcp/mbtcp.mk
#     include lib/uhx15a/uhx15a.mk

ifeq ($(UHX15A_INCLUDED),)
UHX15A_INCLUDED := 1
UHX15A_DIR := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
MBTCP_DIR ?= $(abspath $(UHX15A_DIR)/../mbtcp)
C_SOURCES  += $(UHX15A_DIR)/uhx15a.c
C_INCLUDES += -I$(UHX15A_DIR) -I$(MBTCP_DIR)
endif
