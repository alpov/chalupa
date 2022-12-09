/*
 * main2.c
 *
 *  Created on: Sep 24, 2021
 *      Author: povalac
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "main.h"
#include "rf12.h"
#include "ntc.h"
#include "mini-printf.h"

extern void SystemClock_Config(void);
extern volatile uint32_t uwTick;

#define MDATA_TYP_ENTRIES   20   // number of entries to 1st GSM try
#define MDATA_MAX_ENTRIES   48   // max. number of entries (next are overwritten)
#define MDATA_MAX_ITPLUS    6    // max. number of IT+ transmitters (extra are ignored)

#define MCFG_INTERVAL       3600 // seconds of sleep between RX/ADC cycles
#define MCFG_ITPLUS_TIME    20   // seconds of IT+ reception
#define MCFG_BATT_MIN_VOLT  105  // min. voltage of battery to proceed with IT+/GSM

#define MCFG_GSM_APN        "internet.t-mobile.cz"
#define MCFG_GSM_URL        "www.some-server.com/up.php"

#define nUSART_DEBUG

#ifdef USART_DEBUG
#define debug_send(x) uart_send(x)
#else
#define debug_send(...)
#endif

// max. URL length is 2kB (8+MDATA_MAX_ITPLUS*3)*MDATA_MAX_ENTRIES + ~70
static char buffer[1536]; // min. 6*MDATA_MAX_ENTRIES+1

// temporary buffer for GSM sending
static char buffer2[128];

static char gsm_sq[4];

static const char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._";

static struct /*__attribute__((packed))*/ {
    struct /*__attribute__((packed))*/ {
        int16_t temp;
        uint8_t lowbatt:1;
        uint8_t hygro:7;
    } itplus[MDATA_MAX_ITPLUS];
    uint16_t idx;
    uint8_t cnt_rx;
    uint8_t voltage_pv;
    uint8_t voltage_batt;
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
    uint8_t cnt_gsm;
} mcfg;


static void uart_send(char *s)
{
	LL_mDelay(2);
	while (*s) {
        /* Wait for TXE flag to be raised */
        while (!LL_USART_IsActiveFlag_TXE(USART2)) {}

        /* If last char to be sent, clear TC flag */
        if (s[1] == '\0') {
            LL_USART_ClearFlag_TC(USART2);
        }

        /* Write character in Transmit Data register.
           TXE flag is cleared by writing data in TDR register */
        LL_USART_TransmitData8(USART2, *s++);
    }

    /* Wait for TC flag to be raised for last char */
    while (!LL_USART_IsActiveFlag_TC(USART2)) {}
}

static uint16_t uart_recv(char *s, uint8_t ignore, uint32_t timeout)
{
    uint32_t timeoutTick = uwTick + timeout;

    while (ignore) {
    	uint8_t rcvd = 0;
        do {
        	LL_IWDG_ReloadCounter(IWDG);
            if (LL_USART_IsActiveFlag_RXNE(USART2)) {
                rcvd = LL_USART_ReceiveData8(USART2);
            }
        } while (rcvd != '\n' && uwTick < timeoutTick);
        ignore--;
    }

    uint16_t idx = 0;
    uint8_t rcvd = 0;
    do {
    	LL_IWDG_ReloadCounter(IWDG);
        if (LL_USART_IsActiveFlag_RXNE(USART2)) {
            rcvd = LL_USART_ReceiveData8(USART2);
            s[idx++] = rcvd;
        }
        s[idx] = '\0';
    } while (rcvd != '\n' && uwTick < timeoutTick);

    return idx;
}

static void lowpower_wait(uint32_t seconds)
{
    uint32_t timeoutTick = uwTick + (seconds * 1000) / 64;
	LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_64);
	LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE3);
    do {
    	LL_IWDG_ReloadCounter(IWDG);
    	__WFI();
    } while (uwTick < timeoutTick);
	LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE2);
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
}

static uint16_t adc_to_voltage(uint16_t adc)
{
    return (uint32_t)adc * (uint16_t)(30 * (470+100)/100) / 4096;
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
        }

#ifdef USART_DEBUG
        snprintf(buffer, sizeof(buffer), "%d,%d,%d,%c\n", SensorId, TempCelsius, Hygro, WeakBatt ? 'W' : '-');
        debug_send(buffer);
#endif

        return true;
    }
    return false;
}

static uint16_t get_post_data(char *s)
{
    uint16_t len = 0;

	len += snprintf(&s[len], UINT16_MAX, "i=%d&sq=%s&c=%d&cp=%d&cl=%d&ci=%d&cg=%d", MCFG_INTERVAL/60, gsm_sq, mcfg.cnt, mcfg.cnt_pwron, mcfg.cnt_lowbatt, mcfg.cnt_itplus, mcfg.cnt_gsm);
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
        len += snprintf(&s[len], UINT16_MAX, "&tv=");
        for (uint8_t j = 0; j < mcfg.cnt; j++) {
            uint16_t idx = mdata[j].idx; if (idx > 255) idx = 255;
            uint8_t cnt_rx = mdata[j].cnt_rx; if (cnt_rx > 15) cnt_rx = 15;
            int16_t ti = ssat10(mdata[j].ntc_int);
            int16_t te = ssat10(mdata[j].ntc_ext);
            uint8_t vb = mdata[j].voltage_batt;
            uint8_t vp = mdata[j].voltage_pv;
            // 765432-103210-987654-321098-765432-107654-321076-543210
            // iiiiii-iicccc-tttttt-tttttt-tttttt-ttvvvv-vvvvvv-vvvvvv
            // dddddd-ddrrrr-IIIIII-IIIIEE-EEEEEE-EEBBBB-BBBBPP-PPPPPP
            s[len++] = base64[((idx >> 2) & 0x3F)];
            s[len++] = base64[((idx << 4) & 0x30) | ((cnt_rx) & 0x0F)];
            s[len++] = base64[(((uint16_t)ti >> 4) & 0x3F)];
            s[len++] = base64[(((uint16_t)ti << 4) & 0x3C) | (((uint16_t)te >> 8) & 0x03)];
            s[len++] = base64[(((uint16_t)te >> 2) & 0x3F)];
            s[len++] = base64[(((uint16_t)te << 4) & 0x30) | ((vb >> 4) & 0x0F)];
            s[len++] = base64[((vb << 2) & 0x3C) | ((vp >> 6) & 0x03)];
            s[len++] = base64[((vp) & 0x3F)];
        }
        s[len] = 0;
    } while (0);

    return len;
}

static bool do_gsm_communication(void)
{
    bool ok = false;
    uint16_t len;

    LL_GPIO_SetOutputPin(GSM_EN_GPIO_Port, GSM_EN_Pin);

    lowpower_wait(20); // 20sec

    LL_GPIO_SetOutputPin(LED1_GPIO_Port, LED1_Pin);
    uart_recv(buffer, 5, 100); // flush

#ifdef USART_DEBUG
#define goto
#define gsm_exit
#endif

    uart_send("AT\r\n");
    uart_recv(buffer, 1, 1000);
    if (strcmp(buffer, "OK\r\n") != 0) goto gsm_exit;

    uart_send("ATE1\r\n"); // HTTPPARA does not work with echo disabled...
    uart_recv(buffer, 1, 1000);
    if (strcmp(buffer, "OK\r\n") != 0) goto gsm_exit;

    uart_send("AT+IPR=19200\r\n");
    uart_recv(buffer, 1, 1000);
    if (strcmp(buffer, "OK\r\n") != 0) goto gsm_exit;
/*
    uart_send("AT+CREG?\r\n");
    uart_recv(buffer, 1, 1000);   // +CREG: 1\r\n
    uart_recv(buffer, 1, 1000);   // \r\nOK\r\n
*/
    uart_send("AT+CSQ\r\n");
    uart_recv(buffer, 1, 1000);   // +CSQ: 60,2\r\n
    char *idx1 = strstr(buffer, "+CSQ:");
    char *idx2 = strstr(idx1, ",");
    len = idx2-(idx1+6)-0;
    if (idx1 && idx2 && len < sizeof(gsm_sq)) {
        strncpy(gsm_sq, idx1+6, len);
        gsm_sq[len] = '\0';
    } else {
    	strcpy(gsm_sq, "-1");
    }
    uart_recv(buffer, 1, 1000);   // \r\nOK\r\n
    if (strcmp(buffer, "OK\r\n") != 0) goto gsm_exit;

    uart_send("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\r\n");
    uart_recv(buffer, 1, 1000);
    if (strcmp(buffer, "OK\r\n") != 0) goto gsm_exit;

    uart_send("AT+SAPBR=3,1,\"APN\",\"" MCFG_GSM_APN "\"\r\n");
    uart_recv(buffer, 1, 1000);
    if (strcmp(buffer, "OK\r\n") != 0) goto gsm_exit;

    uart_send("AT+SAPBR=1,1\r\n");
    uart_recv(buffer, 1, 40000); // 40sec
    if (strcmp(buffer, "OK\r\n") != 0) goto gsm_exit;

    uart_send("AT+HTTPINIT\r\n");
    uart_recv(buffer, 1, 1000);
    if (strcmp(buffer, "OK\r\n") != 0) goto gsm_exit;

    uart_send("AT+HTTPPARA=\"URL\",\"" MCFG_GSM_URL "\"\r\n");
    uart_recv(buffer, 1, 1000);
    if (strcmp(buffer, "OK\r\n") != 0) goto gsm_exit;

    uart_send("AT+HTTPPARA=\"CONTENT\",\"application/x-www-form-urlencoded\"\r\n");
    uart_recv(buffer, 1, 1000);
    if (strcmp(buffer, "OK\r\n") != 0) goto gsm_exit;

    len = get_post_data(buffer);
    snprintf(buffer2, sizeof(buffer2), "AT+HTTPDATA=%u,1000\r\n", len);
    uart_send(buffer2);
    uart_recv(buffer2, 1, 1000);
    if (strcmp(buffer2, "DOWNLOAD\r\n") != 0) goto gsm_exit;
    uart_send(buffer);
    uart_recv(buffer2, 1, 1000);   // \r\nOK\r\n
    if (strcmp(buffer2, "OK\r\n") != 0) goto gsm_exit;

    uart_send("AT+HTTPACTION=1\r\n");
    uart_recv(buffer, 1, 1000);   // OK\r\n
    if (strcmp(buffer, "OK\r\n") != 0) goto gsm_exit;
    uart_recv(buffer, 1, 40000);  // \r\n+HTTPACTION: 1,200,n\r\n, 40sec
    if (strstr(buffer, "+HTTPACTION:") == NULL) goto gsm_exit;

    uart_send("AT+HTTPREAD\r\n");
    uart_recv(buffer, 1, 1000);   // +HTTPREAD: n\r\n
    if (strstr(buffer, "+HTTPREAD:") == NULL) goto gsm_exit;
    uart_recv(buffer, 0, 1000);
    if (strstr(buffer, "ACK") != NULL) ok = true; // ACK\n, done, ACKnowledged by PHP script
    uart_recv(buffer, 1, 1000);   // \r\nOK\r\n
    if (strcmp(buffer, "OK\r\n") != 0) goto gsm_exit;

    uart_send("AT+HTTPTERM\r\n");
    uart_recv(buffer, 1, 1000);
    if (strcmp(buffer, "OK\r\n") != 0) goto gsm_exit;

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

    LL_GPIO_SetOutputPin(LED1_GPIO_Port, LED1_Pin);
    LL_mDelay(50);
    LL_GPIO_ResetOutputPin(LED1_GPIO_Port, LED1_Pin);

    if (!LL_GPIO_IsInputPinSet(BTN1_GPIO_Port, BTN1_Pin)) {
        // force send (blank data + one new sample)
    	debug_send("Force send!\n");
        LL_mDelay(50);
        LL_GPIO_SetOutputPin(LED1_GPIO_Port, LED1_Pin);
        LL_mDelay(50);
        LL_GPIO_ResetOutputPin(LED1_GPIO_Port, LED1_Pin);
        LL_mDelay(50);
        LL_GPIO_SetOutputPin(LED1_GPIO_Port, LED1_Pin);
        LL_mDelay(50);
        LL_GPIO_ResetOutputPin(LED1_GPIO_Port, LED1_Pin);
        mcfg.cnt = MDATA_TYP_ENTRIES-1;
    }

    if (LL_RCC_IsActiveFlag_IWDGRST()) {
    	debug_send("Watchdog reset!\n");
    }
    LL_RCC_ClearResetFlags();

    while (1) {
        if (mcfg.state == ST_ADC)
        {
            debug_send("Starting ADC conversion...\n");
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
            mdata[mcfg.cnt].voltage_pv = adc_to_voltage(LL_ADC_REG_ReadConversionData12(ADC1));

            while (!LL_ADC_IsActiveFlag_EOC(ADC1)) {}
            if (adc_to_voltage(LL_ADC_REG_ReadConversionData12(ADC1)) >= MCFG_BATT_MIN_VOLT) mcfg.cnt_pwron++;

            while (!LL_ADC_IsActiveFlag_EOC(ADC1)) {}
            mdata[mcfg.cnt].ntc_int = adc_to_temperature(LL_ADC_REG_ReadConversionData12(ADC1));

            while (!LL_ADC_IsActiveFlag_EOC(ADC1)) {}
            mdata[mcfg.cnt].ntc_ext = adc_to_temperature(LL_ADC_REG_ReadConversionData12(ADC1));

            LL_GPIO_ResetOutputPin(NTC_EN_GPIO_Port, NTC_EN_Pin);
            LL_ADC_Disable(ADC1);

            if (mdata[mcfg.cnt].voltage_batt >= MCFG_BATT_MIN_VOLT) {
                mcfg.state = ST_ITP;
            } else {
                if (mcfg.cnt_lowbatt < 255) mcfg.cnt_lowbatt++;
                mcfg.state = ST_STOP;
            }
        }
        else if (mcfg.state == ST_ITP)
        {
            debug_send("Starting IT+ RX...\n");
            uint32_t timeoutTick = uwTick + MCFG_ITPLUS_TIME*1000;
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
            snprintf(buffer, sizeof(buffer), "PV=%d,BATT=%d,Tint=%d,Text=%d\n", mdata[mcfg.cnt].voltage_pv, mdata[mcfg.cnt].voltage_batt, mdata[mcfg.cnt].ntc_int, mdata[mcfg.cnt].ntc_ext);
            debug_send(buffer);
#endif

            mdata[mcfg.cnt].idx = (uint16_t)mcfg.cnt + mcfg.cnt_lowbatt;
			mcfg.cnt++;

            if (mcfg.cnt >= MDATA_TYP_ENTRIES) {
                mcfg.state = ST_GSM;
            } else {
                mcfg.state = ST_STOP;
            }
        }
        else if (mcfg.state == ST_GSM)
        {
            debug_send("GSM data...\n");
            if (do_gsm_communication()) {
            	uint8_t cnt_gsm = mcfg.cnt_gsm;
                memset(&mdata, 0, sizeof(mdata));
                memset(&mcfg, 0, sizeof(mcfg));
                mcfg.cnt_gsm = cnt_gsm + 1;
            }

            mcfg.state = ST_STOP;
        }
        else if (mcfg.state == ST_STOP)
        {
            debug_send("Going to sleep\n");
            lowpower_wait(MCFG_INTERVAL);
            debug_send("Back from sleep\n");

            uwTick = 0;
            mcfg.state = ST_ADC;
        }

    }
}
