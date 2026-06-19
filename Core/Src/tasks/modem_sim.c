/*
 * modem_sim.c
 *
 *  Created on: May 6, 2026
 *      Author: LindUser
 */

#include "tasks.h"
#include "quectel_drive.h"
#include <stdio.h>
#include <string.h>

// Simulation mode flag - set to 1 to enable simulation
#define MODEM_SIMULATION_ENABLED 0

// Simulated UART handle for modem (points to huart2 which we can use for CLI)
static UART_HandleTypeDef* sim_modem_uart = NULL;

// Simulated AT command responses
typedef struct {
    const char* command;
    const char* response;
    uint32_t delay_ms;
} SimAtResponse;

// Basic AT command responses for EC25 simulation
static const SimAtResponse sim_responses[] = {
    {"AT", "OK", 10},
    {"ATE0", "OK", 10},
    {"AT+CPIN?", "+CPIN: READY\r\nOK", 100},
    {"AT+CSQ", "+CSQ: 20,0\r\nOK", 100},
    {"AT+CREG?", "+CREG: 0,1\r\nOK", 100},
    {"AT+CGREG?", "+CGREG: 0,1\r\nOK", 100},
    {"AT+QIACT?", "+QIACT: 1\r\nOK", 200},
    {"AT+QHTTPCFG=\"contextid\",1", "OK", 100},
    {"AT+QHTTPCFG=\"responseheader\",1", "OK", 100},
    {"AT+QHTTPURL=80,80", "CONNECT", 500},
    {"AT+QHTTPPOST=80,80,10000", "CONNECT", 500},
    {"AT+QHTTPREAD=10000", "+QHTTPREAD: 200\r\n{\"status\":\"success\"}\r\nOK", 1000},
    {"AT+QHTTPPOSTFILE=80,80,\"test.txt\",10000", "CONNECT", 500},
    {"AT+QFUPL=\"test.txt\",0,10000", "+QFUPL: 100,100\r\nOK", 2000},
};

void ModemSim_Init(UART_HandleTypeDef* huart) {
    sim_modem_uart = huart;
    printf("[MODEM-SIM] Simulator initialized\r\n");
}

HAL_StatusTypeDef ModemSim_SendAT(char* command, char* expected_reply, uint32_t timeout) {
    if (!MODEM_SIMULATION_ENABLED || !sim_modem_uart) {
        return HAL_ERROR;
    }

    printf("[MODEM-SIM] TX: %s\r\n", command);
    
    // Simulate transmission delay
    HAL_Delay(10);
    
    // Look for matching response
    for (size_t i = 0; i < sizeof(sim_responses)/sizeof(sim_responses[0]); i++) {
        if (strcmp(command, sim_responses[i].command) == 0) {
            printf("[MODEM-SIM] RX: %s\r\n", sim_responses[i].response);
            
            // Simulate processing delay
            HAL_Delay(sim_responses[i].delay_ms);
            
            // Check if expected reply is in response
            if (strstr(sim_responses[i].response, expected_reply) != NULL) {
                return HAL_OK;
            }
            return HAL_ERROR;
        }
    }
    
    // Default response for unknown commands
    printf("[MODEM-SIM] RX: OK (default)\r\n");
    
    if (strstr("OK", expected_reply) != NULL) {
        return HAL_OK;
    }
    
    return HAL_ERROR;
}

// Simulate receiving data (for testing upload progress)
void ModemSim_InjectData(const char* data, uint16_t length) {
    if (!MODEM_SIMULATION_ENABLED || !sim_modem_uart) {
        return;
    }
    
    printf("[MODEM-SIM] Injecting %u bytes of data\r\n", length);
    // In a real implementation, we would feed this to the UART RX buffer
    // For now, just print it
}

HAL_StatusTypeDef ModemSim_BringUpNetwork(void) {
    if (!MODEM_SIMULATION_ENABLED) {
        return HAL_ERROR;
    }
    
    printf("[MODEM-SIM] Bringing up network (simulated)\r\n");
    
    // Simulate network registration process
    const char* cmds[] = {
        "AT+CPIN?",
        "AT+CSQ",
        "AT+CREG?",
        "AT+CGREG?",
        "AT+QIACT?"
    };
    
    for (size_t i = 0; i < sizeof(cmds)/sizeof(cmds[0]); i++) {
        char expected[16] = {0};
        if (strstr(cmds[i], "CPIN") != NULL) {
            strcpy(expected, "READY");
        } else if (strstr(cmds[i], "CSQ") != NULL) {
            strcpy(expected, "+CSQ:");
        } else if (strstr(cmds[i], "CREG") != NULL) {
            strcpy(expected, "+CREG: 0,1");
        } else if (strstr(cmds[i], "CGREG") != NULL) {
            strcpy(expected, "+CGREG: 0,1");
        } else if (strstr(cmds[i], "QIACT") != NULL) {
            strcpy(expected, "+QIACT:");
        }
        
        if (ModemSim_SendAT((char*)cmds[i], expected, 1000) != HAL_OK) {
            printf("[MODEM-SIM] Network bring up failed at step %zu\r\n", i);
            return HAL_ERROR;
        }
        HAL_Delay(500);
    }
    
    printf("[MODEM-SIM] Network brought up successfully\r\n");
    return HAL_OK;
}

HAL_StatusTypeDef ModemSim_PowerOn(void) {
    if (!MODEM_SIMULATION_ENABLED) {
        return HAL_ERROR;
    }
    
    printf("[MODEM-SIM] Powering on modem (simulated)\r\n");
    // Simulate power on delay
    HAL_Delay(1000);
    printf("[MODEM-SIM] Modem powered on\r\n");
    return HAL_OK;
}

HAL_StatusTypeDef ModemSim_PowerOff(void) {
    if (!MODEM_SIMULATION_ENABLED) {
        return HAL_ERROR;
    }
    
    printf("[MODEM-SIM] Powering off modem (simulated)\r\n");
    // Simulate power off delay
    HAL_Delay(1000);
    printf("[MODEM-SIM] Modem powered off\r\n");
    return HAL_OK;
}