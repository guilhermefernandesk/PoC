
#include "operation.h"
#include "command_cb_manager.h"

#include "mbtcp.h"
#include "uhx15a.h"

static bool stop_thread = false;
static osThreadId failure_thread_id = NULL;
static void _failure_thread(const void *arg);

typedef struct {
    uhx15a_axis_t axis;
    float degrees;
} move_command_t;

static QueueHandle_t s_move_commands = NULL;

/* callback MQTT: sem I/O */
static void on_move_command(const move_command_t *command)
{
    if (s_move_commands != NULL) {
        if (xQueueSend(s_move_commands, command, 0) != pdPASS) {
           
        }
    }
}

static void _failure_thread(const void *arg)
{
    if (uhx15a_init() != UHX15A_OK) {
        const char *text = uhx15a_last_error();
      
        failure_thread_id = NULL;
        osThreadTerminate(osThreadGetId());
        return;
    }

    move_command_t command = {
        .axis = UHX15A_AXIS_AZIMUTH,
        .degrees = 55.0f
    };
    on_move_command(&command);

    for (;;)
    {
        if (xQueueReceive(s_move_commands, &command, 0) == pdPASS) {
           
            (void)uhx15a_set_target(command.axis, command.degrees);
        }

        uhx15a_result_t rc = uhx15a_poll();
        if (rc != UHX15A_OK) {
           
        }
        if (stop_thread) {
            stop_thread = false;
            failure_thread_id = NULL;
            osThreadTerminate(osThreadGetId()); /* Encerra a própria thread com segurança */
        }
        // else if (uhx15a_enabled(UHX15A_AXIS_AZIMUTH)) {
        //     float az = uhx15a_position_deg(UHX15A_AXIS_AZIMUTH);
        //     float el = uhx15a_position_deg(UHX15A_AXIS_ELEVATION);
        //     (void)el;
        // }

        // uint16_t status;
        // if (mbtcp_fc(0x03, 2u, 1u, &status) == MBTCP_OK) {
        //     /* status do azimute (mapa provisorio) */
        // }

        // uint16_t control = 5u;
        // if (mbtcp_fc(0x06, 1u, 1u, &control) == MBTCP_OK) {
        //     /* habilita o control word do azimute (mapa provisorio) */
        // }

        if (stop_thread)
        {
            failure_thread_id = NULL;
            stop_thread = false; 
            osThreadTerminate(failure_thread_id);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

bool operation_entry(void *arg)
{
    command_cb_manager.enable_disable_all_commands(CMD_ENABLE);

    /* Garante que a fila exista antes de iniciar a thread */
    if (s_move_commands == NULL) {
        s_move_commands = xQueueCreate(5, sizeof(move_command_t));
    }

    if (failure_thread_id == NULL)
    {
        stop_thread = false;
        static osThreadDef(FAILURE, _failure_thread, osPriorityNormal, 0, configMINIMAL_STACK_SIZE * 3);
        failure_thread_id = osThreadCreate(osThread(FAILURE), NULL);
    }
    return true;
}

bool operation_exit(void)
{
    command_cb_manager.enable_disable_all_commands(CMD_DISABLE);
    if (failure_thread_id != NULL) {
        stop_thread = true;
    }
    return true;
}
