/*
 * main2.c
 *
 *  Created on: Sep 24, 2021
 *      Author: povalac
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "main.h"
#include "rf12.h"
#include "tables.h"
#include "mini-printf.h"


extern void SystemClock_Config(void);
extern volatile uint32_t uwTick;

#define FLASH_PEKEY1               (0x89ABCDEFU) /*!< Flash program erase key1 */
#define FLASH_PEKEY2               (0x02030405U) /*!< Flash program erase key: used with FLASH_PEKEY2
                                                     to unlock the write access to the FLASH_PECR register and
                                                     data EEPROM */

#define MDATA_MAX_ENTRIES   48   // max. number of entries (next are overwritten)
#define MDATA_MAX_ITPLUS    6    // max. number of IT+ transmitters (extra are ignored)

#define MCFG_GSM_APN        "internet.t-mobile.cz"
#define MCFG_GSM_URL        "www.some-server.com/up.php"

#define nUSART_DEBUG

#ifdef USART_DEBUG
#define debug_send(x)   uart_send(x)
#define MCFG_GSM_DELAY  5000
#else
#define debug_send(...)
#define MCFG_GSM_DELAY  40000
#endif

typedef struct {
	uint32_t magic;

	uint8_t mcfg_typ_entries;   // number of entries to 1st GSM try
	uint16_t mcfg_interval; 	// seconds of sleep between RX/ADC cycles
	uint16_t mcfg_itplus_time;	// seconds of IT+ reception
	uint16_t mcfg_volt_min;		// min. voltage of battery to proceed with IT+/GSM

	uint16_t out1_volt_min;		// min. voltage of battery to enable OUT1
	uint16_t out2_volt_min;		// min. voltage of battery to enable OUT2
	uint16_t out3_volt_min;		// min. voltage of battery to enable OUT3

	uint8_t out1_itp_in;		// indoor IT+ sensor ID
	uint8_t out1_itp_out;		// outdoor IT+ sensor ID
	uint8_t out1_dp_enable;		// dew point enabled for OUT1
	int16_t out1_dp_in_offset;	// indoor dew point offset
	int16_t out1_t_in_max;		// max. indoor temperature for OUT1
	int16_t out1_t_out_min;		// min. outdoor temperature for OUT1
	int16_t out1_h_out_max;		// max. outdoor humidity for OUT1
} config_t;

static config_t config __attribute__((used, section(".eeprom")));

static const config_t config_default = {
	.magic = 0xDEADBEEF,

	.mcfg_typ_entries = 20,
	.mcfg_interval = 3600,
	.mcfg_itplus_time = 20,
	.mcfg_volt_min = 1050,

	.out1_volt_min = 2000,
	.out2_volt_min = 2000,
	.out3_volt_min = 2000,

	.out1_itp_in = 0,
	.out1_itp_out = 0,
	.out1_dp_enable = 0,
	.out1_dp_in_offset = 0,
	.out1_t_in_max = 230,
	.out1_t_out_min = -50,
	.out1_h_out_max = 95,
};

// max. POST length is ~2kB (10+MDATA_MAX_ITPLUS*3)*MDATA_MAX_ENTRIES + ~70 + ~120
static char buffer[2048];

// temporary buffer for GSM sending
static char buffer2[128];

static char gsm_sq[4];

static const char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._";

static struct /*__attribute__((packed))*/ {
    struct __attribute__((packed)) {
        int16_t temp;
        int16_t dp;
        uint8_t lowbatt:1;
        uint8_t hygro:7;
    } itplus[MDATA_MAX_ITPLUS];
    uint16_t idx;
    uint8_t cnt_rx;
    uint8_t outputs;
    uint16_t voltage_batt;
    int16_t ntc_int;
    int16_t ntc_ext;
} mdata[MDATA_MAX_ENTRIES+2]; // +2 is extra-safe size for possible buffer overflow (although it should not happen)

static struct {
    enum { ST_ADC, ST_ITP, ST_GSM, ST_STOP } state;
    uint8_t cnt;
    uint8_t cnt_itplus;
    uint8_t itplus_id[MDATA_MAX_ITPLUS];
    uint8_t cnt_pwron;
    uint8_t cnt_lowbatt;
    uint16_t cnt_gsm;
    uint8_t force;
} mcfg;

#define streq(__s1, __s2) (strcasecmp(__s1, __s2) == 0)


static void uart_send(char *s)
{
	LL_mDelay(2);
	while (*s) {
        /* Wait for TXE flag to be raised */
        while (!LL_USART_IsActiveFlag_TXE(USART1)) {}

        /* If last char to be sent, clear TC flag */
        if (s[1] == '\0') {
            LL_USART_ClearFlag_TC(USART1);
        }

        /* Write character in Transmit Data register.
           TXE flag is cleared by writing data in TDR register */
        LL_USART_TransmitData8(USART1, *s++);
    }

    /* Wait for TC flag to be raised for last char */
    while (!LL_USART_IsActiveFlag_TC(USART1)) {}
}

static uint16_t uart_recv(char *s, uint16_t maxlen, uint8_t ignore, uint32_t timeout)
{
    uint32_t timeoutTick = uwTick + timeout;

    while (ignore) {
    	uint8_t rcvd = 0;
        do {
        	LL_IWDG_ReloadCounter(IWDG);
            if (LL_USART_IsActiveFlag_RXNE(USART1)) {
                rcvd = LL_USART_ReceiveData8(USART1);
            }
        } while (rcvd != '\n' && uwTick < timeoutTick);
        ignore--;
    }

    uint16_t idx = 0;
    uint8_t rcvd = 0;
    do {
    	LL_IWDG_ReloadCounter(IWDG);
        if (LL_USART_IsActiveFlag_RXNE(USART1)) {
            rcvd = LL_USART_ReceiveData8(USART1);
            s[idx++] = rcvd;
        }
        s[idx] = '\0';
    } while (rcvd != '\n' && uwTick < timeoutTick && idx < (maxlen - 1));

    return idx;
}

static void uart_flush(void)
{
	LL_USART_ClearFlag_ORE(USART1);
	LL_USART_ClearFlag_FE(USART1);
	LL_USART_ClearFlag_NE(USART1);
	while (LL_USART_IsActiveFlag_RXNE(USART1)) {
		LL_USART_ReceiveData8(USART1);
	}
}

static void lowpower_wait(uint32_t seconds)
{
    uint32_t timeoutTick = uwTick + (seconds * 1000) / 64;
	LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_64);
	LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE3);
    do {
    	LL_IWDG_ReloadCounter(IWDG);
    	__WFI();
        if (!LL_GPIO_IsInputPinSet(BTN1_GPIO_Port, BTN1_Pin)) {
            mcfg.force = true;
            break;
        }
    } while (uwTick < timeoutTick);
	LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE2);
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
}

static uint16_t adc_to_voltage(uint16_t adc)
{
    return (uint32_t)adc * (uint16_t)(300 * (470+100)/100) / 4096;
}

static int16_t adc_to_temperature(uint16_t adc)
{
    uint16_t adc_ntc = adc / 8;
    if (adc_ntc < 133) adc_ntc = 133;
    if (adc_ntc > 477) adc_ntc = 477;
    adc_ntc -= 133;
    return tab_ntc[adc_ntc];
}

static uint16_t ssat10(int16_t value)
{
    if (value > 511) value = 511;
    if (value < -512) value = -512;
    return (uint16_t)value;
}

static bool do_itp_receive(void)
{
    int8_t idx = -1;
    uint8_t SensorId;
    bool WeakBatt;
    int16_t TempCelsius;
    uint8_t Hygro;
    int16_t DewPoint = 0;

    if (rf12_ProcessITPlusFrame(&SensorId, &WeakBatt, &TempCelsius, &Hygro)) {
        for (uint8_t i = 0; i < mcfg.cnt_itplus; i++) {
            if (mcfg.itplus_id[i] == SensorId) {
                idx = i;
                break;
            }
        }
        if (idx == -1 && mcfg.cnt_itplus < MDATA_MAX_ITPLUS) {
            idx = mcfg.cnt_itplus++;
            mcfg.itplus_id[idx] = SensorId;
        }
        if (idx != -1) {
            mdata[mcfg.cnt].itplus[idx].temp = TempCelsius;
            mdata[mcfg.cnt].itplus[idx].lowbatt = WeakBatt ? 1 : 0;
            mdata[mcfg.cnt].itplus[idx].hygro = Hygro;

            if (Hygro >= 1 && Hygro <= 100) {
            	// https://cs.wikipedia.org/wiki/Rosn%C3%BD_bod
                float Gamma = ((17.67f * 0.1f * TempCelsius) / (243.5f + 0.1f * TempCelsius)) + tab_log_hygro[Hygro];
                DewPoint = (10.0f * 243.5f * Gamma) / (17.67f - Gamma);
            }
            mdata[mcfg.cnt].itplus[idx].dp = DewPoint;
        }

#ifdef USART_DEBUG
        snprintf(buffer, sizeof(buffer), "%d,%d,%d,%c,%d\n", SensorId, TempCelsius, Hygro, WeakBatt ? 'W' : '-', DewPoint);
        debug_send(buffer);
#endif

        return true;
    }
    return false;
}

static uint16_t get_post_data(char *s)
{
    uint16_t len = 0;

    // ss=20&si=3600&st=20&sv=1050&o1v=1200&o2v=1100&o3v=2000&o1si=31&o1so=57&o1de=1&o1do=0&o1ti=230&o1to=-50&o1ho=95

	len += snprintf(&s[len], UINT16_MAX, "i=%d&sq=%s&c=%d&cp=%d&cl=%d&ci=%d&cg=%d",
		(config.mcfg_interval) / 60, gsm_sq, mcfg.cnt, mcfg.cnt_pwron,
		mcfg.cnt_lowbatt, mcfg.cnt_itplus, mcfg.cnt_gsm);
	len += snprintf(&s[len], UINT16_MAX, "&ss=%d&si=%d&st=%d&sv=%d&o1v=%d&o2v=%d&o3v=%d&o1si=%d&o1so=%d&o1de=%d&o1do=%d&o1ti=%d&o1to=%d&o1ho=%d",
		config.mcfg_typ_entries, config.mcfg_interval, config.mcfg_itplus_time, config.mcfg_volt_min,
		config.out1_volt_min, config.out2_volt_min, config.out3_volt_min,
		config.out1_itp_in, config.out1_itp_out, config.out1_dp_enable, config.out1_dp_in_offset, config.out1_t_in_max, config.out1_t_out_min, config.out1_h_out_max);
    for (uint8_t i = 0; i < mcfg.cnt_itplus; i++) {
        len += snprintf(&s[len], UINT16_MAX, "&it%d=", mcfg.itplus_id[i]);
        for (uint8_t j = 0; j < mcfg.cnt; j++) {
            int16_t temp = ssat10(mdata[j].itplus[i].temp);
            uint8_t lowbatt = mdata[j].itplus[i].lowbatt;
            uint8_t hygro = mdata[j].itplus[i].hygro; if (hygro > 127) hygro = 127;
            // 987654-321065-432100
            // TTTTTT-TTTTHH-HHHHHB
            s[len++] = base64[(((uint16_t)temp >> 4) & 0x3F)];
            s[len++] = base64[(((uint16_t)temp << 2) & 0x3C) | ((hygro >> 5) & 0x03)];
            s[len++] = base64[((hygro << 1) & 0x3E) | (lowbatt & 0x01)];
        }
        s[len] = 0;
    }
    do {
        len += snprintf(&s[len], UINT16_MAX, "&tw=");
        for (uint8_t j = 0; j < mcfg.cnt; j++) {
            uint16_t idx = mdata[j].idx; if (idx > 255) idx = 255;
            uint8_t cnt_rx = mdata[j].cnt_rx; if (cnt_rx > 15) cnt_rx = 15;
            int16_t ti = ssat10(mdata[j].ntc_int);
            int16_t te = ssat10(mdata[j].ntc_ext);
            uint16_t vb = mdata[j].voltage_batt;
            uint8_t vu = mdata[j].outputs;
            // 765432-103210-987654-321098-765432-100987-654321-0210xx
            // iiiiii-iicccc-tttttt-tttttt-tttttt-ttvvvv-vvvvvv-vvvvxx
            // dddddd-ddrrrr-IIIIII-IIIIEE-EEEEEE-EEBBBB-BBBBBB-BUUUxx
            s[len++] = base64[((idx >> 2) & 0x3F)];
            s[len++] = base64[((idx << 4) & 0x30) | ((cnt_rx) & 0x0F)];
            s[len++] = base64[(((uint16_t)ti >> 4) & 0x3F)];
            s[len++] = base64[(((uint16_t)ti << 4) & 0x3C) | (((uint16_t)te >> 8) & 0x03)];
            s[len++] = base64[(((uint16_t)te >> 2) & 0x3F)];
            s[len++] = base64[(((uint16_t)te << 4) & 0x30) | ((vb >> 7) & 0x0F)];
            s[len++] = base64[((vb >> 1) & 0x3F)];
            s[len++] = base64[((vb << 5) & 0x20) | ((vu << 2) & 0x1C)];
        }
        s[len] = 0;
    } while (0);

    return len;
}

static bool process_gsm_response(char *buffer)
{
    // r=ack
    // r=update&ss=20&si=3600&st=20&sv=1050&o1v=1200&o2v=1100&o3v=2000&o1si=31&o1so=57&o1de=1&o1do=0&o1ti=230&o1to=-50&o1ho=95
    // r=defaults
    // r=reboot

	const char *separator = "&=\r\n";
	char *saveptr;
    char *token = strtok_r(buffer, separator, &saveptr);
    if (!streq(token, "r")) {
    	return false;
    }

    token = strtok_r(NULL, separator, &saveptr);
    if (streq(token, "ack")) {
        debug_send("POST acknowledged\n");
        return true;
    }
    else if (streq(token, "update")) {
        debug_send("Config update requested\n");

    	config_t config_new;
    	memcpy(&config_new, &config, sizeof(config_t));

        token = strtok_r(NULL, separator, &saveptr);
    	while (token != NULL) {
    		if (streq(token, "ss")) {
    	        int32_t value = atoi(strtok_r(NULL, separator, &saveptr));
    	        if (value >= 1 && value < MDATA_MAX_ENTRIES) {
    	        	config_new.mcfg_typ_entries = value;
    	        }
    		}
    		else if (streq(token, "si")) {
    	        int32_t value = atoi(strtok_r(NULL, separator, &saveptr));
    	        if (value >= 0 && value <= 43200) {
    	        	config_new.mcfg_interval = value;
    	        }
    		}
    		else if (streq(token, "st")) {
    	        int32_t value = atoi(strtok_r(NULL, separator, &saveptr));
    	        if (value >= 0 && value <= 600) {
    	        	config_new.mcfg_itplus_time = value;
    	        }
    		}
    		else if (streq(token, "sv")) {
    	        int32_t value = atoi(strtok_r(NULL, separator, &saveptr));
    	        if (value >= 0 && value <= 2000) {
    	        	config_new.mcfg_volt_min = value;
    	        }
    		}
    		else if (streq(token, "o1v")) {
    	        int32_t value = atoi(strtok_r(NULL, separator, &saveptr));
    	        if (value >= 0 && value <= 2000) {
    	        	config_new.out1_volt_min = value;
    	        }
    		}
    		else if (streq(token, "o2v")) {
    	        int32_t value = atoi(strtok_r(NULL, separator, &saveptr));
    	        if (value >= 0 && value <= 2000) {
    	        	config_new.out2_volt_min = value;
    	        }
    		}
    		else if (streq(token, "o3v")) {
    	        int32_t value = atoi(strtok_r(NULL, separator, &saveptr));
    	        if (value >= 0 && value <= 2000) {
    	        	config_new.out3_volt_min = value;
    	        }
    		}
    		else if (streq(token, "o1si")) {
    	        int32_t value = atoi(strtok_r(NULL, separator, &saveptr));
    	        if (value >= 0 && value <= 255) {
    	        	config_new.out1_itp_in = value;
    	        }
    		}
    		else if (streq(token, "o1so")) {
    	        int32_t value = atoi(strtok_r(NULL, separator, &saveptr));
    	        if (value >= 0 && value <= 255) {
    	        	config_new.out1_itp_out = value;
    	        }
    		}
    		else if (streq(token, "o1de")) {
    	        int32_t value = atoi(strtok_r(NULL, separator, &saveptr));
    	        if (value == 0 || value == 1) {
    	        	config_new.out1_dp_enable = value;
    	        }
    		}
    		else if (streq(token, "o1do")) {
    	        int32_t value = atoi(strtok_r(NULL, separator, &saveptr));
    	        if (value >= -500 && value <= 500) {
    	        	config_new.out1_dp_in_offset = value;
    	        }
    		}
    		else if (streq(token, "o1ti")) {
    	        int32_t value = atoi(strtok_r(NULL, separator, &saveptr));
    	        if (value >= -500 && value <= 1000) {
    	        	config_new.out1_t_in_max = value;
    	        }
    		}
    		else if (streq(token, "o1to")) {
    	        int32_t value = atoi(strtok_r(NULL, separator, &saveptr));
    	        if (value >= -500 && value <= 1000) {
    	        	config_new.out1_t_out_min = value;
    	        }
    		}
    		else if (streq(token, "o1ho")) {
    	        int32_t value = atoi(strtok_r(NULL, separator, &saveptr));
    	        if (value >= 0 && value <= 100) {
    	        	config_new.out1_h_out_max = value;
    	        }
    		}

    	    token = strtok_r(NULL, separator, &saveptr);
    	}

    	memcpy(&config, &config_new, sizeof(config_t));
    	return true;
    }
    else if (streq(token, "defaults")) {
        debug_send("Invalidating config, rebooting...\n");
        config.magic = 0;
        LL_mDelay(50);
        NVIC_SystemReset();
    }
    else if (streq(token, "reboot")) {
        debug_send("Rebooting...\n");
        LL_mDelay(50);
        NVIC_SystemReset();
    }

    debug_send("Response error!\n");
    return false;
}

static bool do_gsm_communication(void)
{
    bool ok = false;
    uint16_t len;

    LL_GPIO_SetOutputPin(GSM_EN_GPIO_Port, GSM_EN_Pin);

    lowpower_wait(20); // 20sec

    LL_GPIO_SetOutputPin(LED1_GPIO_Port, LED1_Pin);
    uart_flush(); // flush

#ifdef USART_DEBUG
#define goto
#define gsm_exit
#endif

    uart_send("AT\r\n");
    uart_recv(buffer, sizeof(buffer), 1, 1000);
    if (strcmp(buffer, "OK\r\n") != 0) goto gsm_exit;

    uart_send("ATE1\r\n"); // HTTPPARA does not work with echo disabled...
    uart_recv(buffer, sizeof(buffer), 1, 1000);
    if (strcmp(buffer, "OK\r\n") != 0) goto gsm_exit;

    uart_send("AT+IPR=19200\r\n");
    uart_recv(buffer, sizeof(buffer), 1, 1000);
    if (strcmp(buffer, "OK\r\n") != 0) goto gsm_exit;
/*
    uart_send("AT+CREG?\r\n");
    uart_recv(buffer, sizeof(buffer), 1, 1000);   // +CREG: 1\r\n
    uart_recv(buffer, sizeof(buffer), 1, 1000);   // \r\nOK\r\n
*/
    uart_send("AT+CSQ\r\n");
    uart_recv(buffer, sizeof(buffer), 1, 1000);   // +CSQ: 60,2\r\n
    char *idx1 = strstr(buffer, "+CSQ:");
    char *idx2 = strstr(idx1, ",");
    len = idx2-(idx1+6)-0;
    if (idx1 && idx2 && len < sizeof(gsm_sq)) {
        strncpy(gsm_sq, idx1+6, len);
        gsm_sq[len] = '\0';
    } else {
    	strcpy(gsm_sq, "-1");
    }
    uart_recv(buffer, sizeof(buffer), 1, 1000);   // \r\nOK\r\n
    if (strcmp(buffer, "OK\r\n") != 0) goto gsm_exit;

    uart_send("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\r\n");
    uart_recv(buffer, sizeof(buffer), 1, 1000);
    if (strcmp(buffer, "OK\r\n") != 0) goto gsm_exit;

    uart_send("AT+SAPBR=3,1,\"APN\",\"" MCFG_GSM_APN "\"\r\n");
    uart_recv(buffer, sizeof(buffer), 1, 1000);
    if (strcmp(buffer, "OK\r\n") != 0) goto gsm_exit;

    uart_send("AT+SAPBR=1,1\r\n");
    uart_recv(buffer, sizeof(buffer), 1, MCFG_GSM_DELAY); // 40sec
    if (strcmp(buffer, "OK\r\n") != 0) goto gsm_exit;

    uart_send("AT+HTTPINIT\r\n");
    uart_recv(buffer, sizeof(buffer), 1, 1000);
    if (strcmp(buffer, "OK\r\n") != 0) goto gsm_exit;

    uart_send("AT+HTTPPARA=\"URL\",\"" MCFG_GSM_URL "\"\r\n");
    uart_recv(buffer, sizeof(buffer), 1, 1000);
    if (strcmp(buffer, "OK\r\n") != 0) goto gsm_exit;

    uart_send("AT+HTTPPARA=\"CONTENT\",\"application/x-www-form-urlencoded\"\r\n");
    uart_recv(buffer, sizeof(buffer), 1, 1000);
    if (strcmp(buffer, "OK\r\n") != 0) goto gsm_exit;

    len = get_post_data(buffer);
    snprintf(buffer2, sizeof(buffer2), "AT+HTTPDATA=%u,1000\r\n", len);
    uart_send(buffer2);
    uart_recv(buffer2, sizeof(buffer2), 1, 1000);
    if (strcmp(buffer2, "DOWNLOAD\r\n") != 0) goto gsm_exit;
    uart_send(buffer);
    uart_recv(buffer2, sizeof(buffer2), 1, 1000);   // \r\nOK\r\n
    if (strcmp(buffer2, "OK\r\n") != 0) goto gsm_exit;

    uart_send("AT+HTTPACTION=1\r\n");
    uart_recv(buffer, sizeof(buffer), 1, 1000);   // OK\r\n
    if (strcmp(buffer, "OK\r\n") != 0) goto gsm_exit;
    uart_recv(buffer, sizeof(buffer), 1, MCFG_GSM_DELAY);  // \r\n+HTTPACTION: 1,200,n\r\n, 40sec
    if (strstr(buffer, "+HTTPACTION:") == NULL) goto gsm_exit;

    uart_send("AT+HTTPREAD\r\n");
    uart_recv(buffer, sizeof(buffer), 1, 1000);   // +HTTPREAD: n\r\n
    if (strstr(buffer, "+HTTPREAD:") == NULL) goto gsm_exit;
    uart_recv(buffer, sizeof(buffer), 0, 1000); // response processed later
    uart_recv(buffer2, sizeof(buffer2), 1, 1000);   // \r\nOK\r\n
    if (strcmp(buffer2, "OK\r\n") != 0) goto gsm_exit;

    uart_send("AT+HTTPTERM\r\n");
    uart_recv(buffer2, sizeof(buffer2), 1, 1000);
    if (strcmp(buffer2, "OK\r\n") != 0) goto gsm_exit;

    ok = process_gsm_response(buffer); // process PHP script response

#ifdef USART_DEBUG
#undef goto
#undef gsm_exit
goto gsm_exit;
#endif

gsm_exit:
    LL_GPIO_ResetOutputPin(LED1_GPIO_Port, LED1_Pin);
    LL_GPIO_ResetOutputPin(GSM_EN_GPIO_Port, GSM_EN_Pin);

    return ok;
}

static uint8_t update_outputs(void)
{
    uint16_t voltage_batt = mdata[mcfg.cnt].voltage_batt;
	bool dp_in_found = false, dp_out_found = false;
	int16_t dp_in, dp_out;
	bool out1 = (voltage_batt > config.out1_volt_min);

	for (uint8_t idx = 0; idx < mcfg.cnt_itplus; idx++) {
		if (mcfg.itplus_id[idx] == config.out1_itp_in && mdata[mcfg.cnt].itplus[idx].hygro != 0) {
			dp_in_found = true;
			dp_in = mdata[mcfg.cnt].itplus[idx].dp;
			if (mdata[mcfg.cnt].itplus[idx].temp > config.out1_t_in_max) {
				out1 = false;
			}
		}
		if (mcfg.itplus_id[idx] == config.out1_itp_out && mdata[mcfg.cnt].itplus[idx].hygro != 0) {
			dp_out_found = true;
			dp_out = mdata[mcfg.cnt].itplus[idx].dp;
			if (mdata[mcfg.cnt].itplus[idx].temp < config.out1_t_out_min || mdata[mcfg.cnt].itplus[idx].hygro > config.out1_h_out_max) {
				out1 = false;
			}
		}
	}
	if (config.out1_dp_enable && dp_out_found && dp_in_found && (dp_out > (dp_in + config.out1_dp_in_offset))) {
		out1 = false;
	}
    if (out1) {
    	LL_GPIO_SetOutputPin(OUT1_GPIO_Port, OUT1_Pin);
    } else {
    	LL_GPIO_ResetOutputPin(OUT1_GPIO_Port, OUT1_Pin);
    }

    if (voltage_batt > config.out2_volt_min) {
    	LL_GPIO_SetOutputPin(OUT2_GPIO_Port, OUT2_Pin);
    } else {
    	LL_GPIO_ResetOutputPin(OUT2_GPIO_Port, OUT2_Pin);
    }
    if (voltage_batt > config.out3_volt_min) {
    	LL_GPIO_SetOutputPin(OUT3_GPIO_Port, OUT3_Pin);
    } else {
    	LL_GPIO_ResetOutputPin(OUT3_GPIO_Port, OUT3_Pin);
    }

    return (LL_GPIO_IsOutputPinSet(OUT1_GPIO_Port, OUT1_Pin) ? 0x01 : 0x00) |
           (LL_GPIO_IsOutputPinSet(OUT2_GPIO_Port, OUT2_Pin) ? 0x02 : 0x00) |
           (LL_GPIO_IsOutputPinSet(OUT3_GPIO_Port, OUT3_Pin) ? 0x04 : 0x00);
}

void fault_handler(void)
{
    LL_GPIO_ResetOutputPin(GSM_EN_GPIO_Port, GSM_EN_Pin);
	for (uint16_t i = 0; i < 1000; i++) {
		LL_GPIO_SetOutputPin(LED1_GPIO_Port, LED1_Pin);
		LL_mDelay(50);
		LL_GPIO_ResetOutputPin(LED1_GPIO_Port, LED1_Pin);
		LL_mDelay(50);
	}
	NVIC_SystemReset();
}

void main2(void)
{
    /* Reconfigure SysTick to enable its interrupt */
    SysTick_Config(SystemCoreClock/1000);

    mcfg.state = ST_ADC;
    debug_send("BOOT\n");

    rf12_initialize();
    rf12_shutdown();

    // unlock EEPROM write and load defaults if the content is not valid
    while (FLASH->SR & FLASH_SR_BSY) {}
    if (FLASH->PECR & FLASH_PECR_PELOCK) {
    	FLASH->PEKEYR = FLASH_PEKEY1;
    	FLASH->PEKEYR = FLASH_PEKEY2;
    }
    if (config.magic != config_default.magic) {
        debug_send("Default config!\n");
    	memcpy(&config, &config_default, sizeof(config_t));
    }

    if (LL_RCC_IsActiveFlag_IWDGRST()) {
    	debug_send("Watchdog reset!\n");
    }
    LL_RCC_ClearResetFlags();

    while (1) {
        if (mcfg.state == ST_ADC)
        {
            debug_send("Starting ADC conversion...\n");
            LL_GPIO_SetOutputPin(LED1_GPIO_Port, LED1_Pin);
            LL_mDelay(500);
            LL_GPIO_ResetOutputPin(LED1_GPIO_Port, LED1_Pin);

            LL_GPIO_SetOutputPin(NTC_EN_GPIO_Port, NTC_EN_Pin);
            LL_ADC_StartCalibration(ADC1);
            while (LL_ADC_IsCalibrationOnGoing(ADC1)) {}
            LL_mDelay(2);
            LL_ADC_Enable(ADC1);
            while (!LL_ADC_IsActiveFlag_ADRDY(ADC1)) {}
            LL_ADC_REG_StartConversion(ADC1);

            if (mcfg.cnt > MDATA_MAX_ENTRIES - 1) {
            	// never write outside mdata array
                mcfg.cnt = MDATA_MAX_ENTRIES - 1;
            }

            while (!LL_ADC_IsActiveFlag_EOC(ADC1)) {}
            mdata[mcfg.cnt].voltage_batt = adc_to_voltage(LL_ADC_REG_ReadConversionData12(ADC1));

            while (!LL_ADC_IsActiveFlag_EOC(ADC1)) {}
            mdata[mcfg.cnt].ntc_int = adc_to_temperature(LL_ADC_REG_ReadConversionData12(ADC1));

            while (!LL_ADC_IsActiveFlag_EOC(ADC1)) {}
            mdata[mcfg.cnt].ntc_ext = adc_to_temperature(LL_ADC_REG_ReadConversionData12(ADC1));

            LL_GPIO_ResetOutputPin(NTC_EN_GPIO_Port, NTC_EN_Pin);
            LL_ADC_Disable(ADC1);

            if (mdata[mcfg.cnt].voltage_batt >= config.mcfg_volt_min) {
                mcfg.state = ST_ITP;
            } else {
                if (mcfg.cnt_lowbatt < 255) mcfg.cnt_lowbatt++;
            	LL_GPIO_ResetOutputPin(OUT1_GPIO_Port, OUT1_Pin);
            	LL_GPIO_ResetOutputPin(OUT2_GPIO_Port, OUT2_Pin);
            	LL_GPIO_ResetOutputPin(OUT3_GPIO_Port, OUT3_Pin);
                mcfg.state = ST_STOP;
            }
        }
        else if (mcfg.state == ST_ITP)
        {
            debug_send("Starting IT+ RX...\n");
            uint32_t timeoutTick = uwTick + config.mcfg_itplus_time * 1000UL;
            uint32_t tickLastRx = 0;
            rf12_initialize();
            do {
                if (do_itp_receive()) {
                    mdata[mcfg.cnt].cnt_rx++;
                    LL_GPIO_SetOutputPin(LED1_GPIO_Port, LED1_Pin);
                    tickLastRx = uwTick;
                }
                if (uwTick > tickLastRx + 100) {
                    LL_GPIO_ResetOutputPin(LED1_GPIO_Port, LED1_Pin);
                }
            	LL_IWDG_ReloadCounter(IWDG);
                __WFI();
            } while (uwTick < timeoutTick);
            rf12_shutdown();
            LL_GPIO_ResetOutputPin(LED1_GPIO_Port, LED1_Pin);
#ifdef USART_DEBUG
            snprintf(buffer, sizeof(buffer), "BATT=%d,Tint=%d,Text=%d\n", mdata[mcfg.cnt].voltage_batt, mdata[mcfg.cnt].ntc_int, mdata[mcfg.cnt].ntc_ext);
            debug_send(buffer);
#endif

            mdata[mcfg.cnt].outputs = update_outputs();
            mdata[mcfg.cnt].idx = (uint16_t)mcfg.cnt + mcfg.cnt_lowbatt;
			mcfg.cnt++;

            if (mcfg.force || mcfg.cnt >= config.mcfg_typ_entries) {
                mcfg.state = ST_GSM;
            } else {
                mcfg.state = ST_STOP;
            }
        }
        else if (mcfg.state == ST_GSM)
        {
            debug_send("GSM data...\n");
            if (do_gsm_communication()) {
            	uint16_t cnt_gsm = mcfg.cnt_gsm;
                memset(&mdata, 0, sizeof(mdata));
                memset(&mcfg, 0, sizeof(mcfg));
                mcfg.cnt_gsm = cnt_gsm + 1;
            }

			mcfg.state = ST_STOP;
        }
        else if (mcfg.state == ST_STOP)
        {
            debug_send("Going to sleep\n");
            lowpower_wait(config.mcfg_interval);
            debug_send("Back from sleep\n");

            uwTick = 0;
            mcfg.state = ST_ADC;
        }

    }
}

