/*
 * tcpprobe.h - cliente TCP minimo + diagnostico para FreeRTOS-Plus-TCP
 *
 * Objetivo: provar que o caminho TCP *cliente* funciona, sem depender de
 * Modbus, HAL, netif global ou CMSIS-OS. Nao cria task permanente.
 *
 * Dependencias: FreeRTOS-Plus-TCP (FreeRTOS_Sockets.h / FreeRTOS_IP.h).
 * Nao inclui lwIP, HAL nem headers do projeto.
 */
#ifndef TCPPROBE_H
#define TCPPROBE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TCPPROBE_VERSION "1.0.0"

/* ------------------------------------------------------------------ */
/* Configuracao (sobrescreva por -D ou antes do include)               */
/* ------------------------------------------------------------------ */

/* 0 = remove todo o logging (compila vazio) */
#ifndef TCPPROBE_LOG_ENABLE
#define TCPPROBE_LOG_ENABLE 1
#endif

/* 1 = usa FreeRTOS_IsNetworkUp() no env_dump.
 * Se sua versao nao tiver a funcao, compile com -DTCPPROBE_HAS_ISNETWORKUP=0 */
#ifndef TCPPROBE_HAS_ISNETWORKUP
#define TCPPROBE_HAS_ISNETWORKUP 1
#endif

/* 1 = usa FreeRTOS_GetEndPointConfiguration() para imprimir IP/mask/gw.
 * Disponivel em FreeRTOS-Plus-TCP v3/v4. Deixe 0 se nao existir na sua. */
#ifndef TCPPROBE_HAS_ENDPOINT_CFG
#define TCPPROBE_HAS_ENDPOINT_CFG 0
#endif

/* 1 = usa xPortGetMinimumEverFreeHeapSize() (heap_4/heap_5) */
#ifndef TCPPROBE_HAS_MIN_HEAP
#define TCPPROBE_HAS_MIN_HEAP 1
#endif

/* Tamanho do buffer de uma linha de log */
#ifndef TCPPROBE_LOG_BUF
#define TCPPROBE_LOG_BUF 192
#endif

/* ------------------------------------------------------------------ */
/* Erros                                                               */
/* ------------------------------------------------------------------ */
#define TCPPROBE_OK         0
#define TCPPROBE_EINVAL    -1
#define TCPPROBE_ECONN     -2
#define TCPPROBE_ESEND     -3
#define TCPPROBE_ETIMEOUT  -4
#define TCPPROBE_ERECV     -5

/* ------------------------------------------------------------------ */
/* Logging ("link dinamico": o projeto pode injetar o proprio logger)  */
/* ------------------------------------------------------------------ */

/* Se fn == NULL, volta ao default (printf com prefixo [tcpprobe]). */
void tcpprobe_set_logger(void (*fn)(const char *line));

/* Log formatado. Thread-unsafe por design (diagnostico, nao producao). */
void tcpprobe_log(const char *fmt, ...);

/* Dump hex de um buffer, 16 bytes por linha. */
void tcpprobe_log_hex(const char *tag, const void *buf, unsigned len);

/* Retrato do ambiente: versao, heap, tasks, rede e configs do ipconfig. */
void tcpprobe_env_dump(void);

/* ------------------------------------------------------------------ */
/* Cliente TCP one-shot                                                */
/* ------------------------------------------------------------------ */

/*
 * Conecta em ip:port, envia tx (se txlen > 0), faz UM recv com timeout,
 * loga TX/RX em hex e fecha.
 *
 * Retorno:
 *   > 0  bytes recebidos (gravados em rx)
 *   0    conexao fechada pelo peer sem dados
 *   < 0  TCPPROBE_E*
 *
 * Para enquadramento de protocolo (ler exatamente N bytes), use mbtcp.
 */
int tcpprobe_tcp_client(const char *ip, uint16_t port,
                        const void *tx, uint16_t txlen,
                        void *rx, uint16_t rxcap,
                        uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
#endif /* TCPPROBE_H */
