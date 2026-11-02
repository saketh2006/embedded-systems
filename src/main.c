#include "stm32f4xx.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* ==============================================================================
   EMBEDDED SYSTEMS CASE STUDY: STM32-BASED KNOCK PATTERN SMART DOOR LOCK
   Keyless Access Using Accelerometer-Based Knock Rhythm Analysis
   ------------------------------------------------------------------------------
   Course      : 23CSE304 – Embedded Systems
   Department  : Department of Computer Science and Engineering
   Institution : School of Computing, Amrita Vishwa Vidyapeetham, Coimbatore, India
   Mentor      : Dr. A. Baskar

   Student Name(s) and Registration Number(s):
   - V. Saketh       (CB.SC.U4CSE24558)
   - Y. Jothi Basu   (CB.SC.U4CSE24559)
   - T. Rohit Reddy  (CB.SC.U4CSE24551)
   - T. Kumar        (CB.SC.U4CSE24552)
   - Vageesh Raj V S (CB.SC.U4CSE24554)

   Hardware Interconnects (STM32F401CCU6 Black Pill):
   - ADXL345 Accelerometer : PB8 (I2C1_SCL), PB9 (I2C1_SDA) [Address 0x53 / 0xA6]
   - 16x2 Character LCD    : PB8 (I2C1_SCL), PB9 (I2C1_SDA) [Address 0x27 / 0x3F]
   - Analog Potentiometer  : PA0 (ADC1 Channel 0, 12-bit SAR ADC)
   - Push Button Override  : PA1 (Internal Pull-Up to VDD, active Low)
   - HC-05 Bluetooth Module: PA2 (USART2_TX), PA3 (USART2_RX) [9600 Baud, 8N1]
   - SG90 Actuator Servo   : PA6 (TIM3_CH1 50Hz PWM, 1000us - 2000us)
   - Clock Frequency       : 16 MHz HSI Internal RC Oscillator
   ============================================================================== */

#define PASSWORD               "9999"
#define KNOCK_MIN_MS            150U
#define KNOCK_MAX_MS           1000U
#define KNOCK_TIMEOUT          1500U
#define KNOCK_DEBOUNCE          180U
#define PATTERN_TOLERANCE        35U
#define AUTO_LOCK_MS           5000U
#define TELEMETRY_INTERVAL_MS   600U /* Transmit ADC sample to Bluetooth every 600ms */

/* Actuator Calibration */
#define SERVO_POS_LOCK         2000U /* 2000us Pulse Width (~180 deg Locked) */
#define SERVO_POS_UNLOCK       1000U /* 1000us Pulse Width (~0 deg Unlocked) */
#define SERVO_MOVE_TIME         500U /* Disable PWM output after 500ms to eliminate jitter */

/* LCD Display Parameters */
#define LCD_ADDR               0x27U 
#define LCD_BACKLIGHT          0x08U
#define LCD_ENABLE             0x04U
#define LCD_RS_DATA            0x01U
#define LCD_RS_CMD             0x00U

/* System State Variables */
volatile uint32_t milliseconds = 0U;
volatile uint8_t  adxl_id = 0U;
volatile uint16_t pot_value = 0U;
volatile uint8_t  knock_count = 0U;
volatile uint8_t  door_unlocked = 0U;
volatile uint8_t  knock_system_enabled = 1U;

static uint8_t  adxl_ok = 0U;
static uint8_t  first_sample = 1U;
static int16_t  old_x, old_y, old_z;
static uint32_t knock_time[3] = {0U, 0U, 0U};
static uint32_t last_knock = 0U;
static uint32_t unlock_time = 0U;

static uint32_t servo_power_start = 0U;
static uint8_t  servo_active = 0U;

/* Function Declarations */
static void SysTick_Init(void);
static void delay_ms(uint32_t ms);
static void delay_us(uint32_t us);

static void I2C1_Init(void);
static void I2C1_Reset(void);
static uint8_t ADXL_ReadReg(uint8_t reg, uint8_t *value);
static uint8_t ADXL_WriteReg(uint8_t reg, uint8_t value);
static uint8_t ADXL_ReadBurst(uint8_t start_reg, uint8_t *buf, uint8_t len);
static uint8_t ADXL_Init(void);
static uint8_t ADXL_ReadAxes(int16_t *x, int16_t *y, int16_t *z);

static uint8_t LCD_WriteByte(uint8_t data);
static void LCD_PulseEnable(uint8_t data);
static void LCD_Write4Bits(uint8_t nibble, uint8_t mode);
static void LCD_Send(uint8_t value, uint8_t mode);
static void LCD_Command(uint8_t cmd);
static void LCD_Char(char c);
static void LCD_Init(void);
static void LCD_SetCursor(uint8_t row, uint8_t col);
static void LCD_Print(const char *text);
static void LCD_ShowStatus(const char *line1, const char *line2);

static void ADC1_Init(void);
static uint16_t ADC1_Read(void);
static void USART2_Init(void);
static void UART_SendString(const char *s);
static void UART_SendTelemetry(void);
static void Button_Init(void);
static void Button_Process(void);
static void Bluetooth_Process(void);

static void Servo_Init(void);
static void Servo_SetPosition(uint16_t pulse_us);
static void Servo_Process(void);
static void Door_Lock(void);
static void Door_Unlock(void);
static void Knock_Detection(void);

/* ---------- Timebase Configuration (SysTick) ---------- */

void SysTick_Handler(void)
{
    milliseconds++;
}

static void SysTick_Init(void)
{
    SysTick->LOAD = 16000U - 1U;
    SysTick->VAL = 0U;
    SysTick->CTRL = 7U;
}

static void delay_ms(uint32_t ms)
{
    uint32_t start = milliseconds;
    while ((uint32_t)(milliseconds - start) < ms) {}
}

static void delay_us(uint32_t us)
{
    volatile uint32_t count = us * 3U;
    while (count--) {}
}

/* ---------- I2C1 Hardware Driver ---------- */

static void I2C1_Reset(void)
{
    I2C1->CR1 |= I2C_CR1_SWRST;
    for (volatile int i = 0; i < 300; i++) {}
    I2C1->CR1 &= ~I2C_CR1_SWRST;

    I2C1->CR2 = 16U;
    I2C1->CCR = 80U;
    I2C1->TRISE = 17U;
    I2C1->CR1 = I2C_CR1_PE;
}

static void I2C1_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    GPIOB->MODER &= ~((3U << 16) | (3U << 18));
    GPIOB->MODER |=  ((2U << 16) | (2U << 18));
    GPIOB->OTYPER |= (1U << 8) | (1U << 9);

    GPIOB->PUPDR &= ~((3U << 16) | (3U << 18));
    GPIOB->PUPDR |=  ((1U << 16) | (1U << 18));

    GPIOB->AFR[1] &= ~((0xFU << 0) | (0xFU << 4));
    GPIOB->AFR[1] |=  ((4U << 0) | (4U << 4));

    I2C1_Reset();
}

/* ---------- HD44780 LCD via PCF8574 ---------- */

static uint8_t LCD_WriteByte(uint8_t data)
{
    uint32_t t = 10000U;

    while ((I2C1->SR2 & I2C_SR2_BUSY) && t--) {}
    if (t == 0U) { I2C1_Reset(); return 0U; }

    I2C1->CR1 |= I2C_CR1_START;
    t = 10000U;
    while (!(I2C1->SR1 & I2C_SR1_SB) && t--) {}
    if (t == 0U) goto fail;

    I2C1->DR = (LCD_ADDR << 1);
    t = 10000U;
    while (!(I2C1->SR1 & I2C_SR1_ADDR) && t--) {
        if (I2C1->SR1 & I2C_SR1_AF) goto fail;
    }
    if (t == 0U) goto fail;

    (void)I2C1->SR1;
    (void)I2C1->SR2;

    t = 10000U;
    while (!(I2C1->SR1 & I2C_SR1_TXE) && t--) {}
    if (t == 0U) goto fail;

    I2C1->DR = data;

    t = 10000U;
    while (!(I2C1->SR1 & I2C_SR1_BTF) && t--) {}
    if (t == 0U) goto fail;

    I2C1->CR1 |= I2C_CR1_STOP;
    return 1U;

fail:
    I2C1->CR1 |= I2C_CR1_STOP;
    I2C1_Reset();
    return 0U;
}

static void LCD_PulseEnable(uint8_t data)
{
    LCD_WriteByte(data | LCD_ENABLE);
    delay_us(50U);
    LCD_WriteByte(data & ~LCD_ENABLE);
    delay_us(50U);
}

static void LCD_Write4Bits(uint8_t nibble, uint8_t mode)
{
    uint8_t byte = (nibble & 0xF0U) | mode | LCD_BACKLIGHT;
    LCD_PulseEnable(byte);
}

static void LCD_Send(uint8_t value, uint8_t mode)
{
    LCD_Write4Bits(value & 0xF0U, mode);
    LCD_Write4Bits((value << 4) & 0xF0U, mode);
}

static void LCD_Command(uint8_t cmd)
{
    LCD_Send(cmd, LCD_RS_CMD);
    if (cmd == 0x01U || cmd == 0x02U)
        delay_ms(2U);
    else
        delay_us(50U);
}

static void LCD_Char(char c)
{
    LCD_Send((uint8_t)c, LCD_RS_DATA);
}

static void LCD_Init(void)
{
    delay_ms(50U);

    LCD_Write4Bits(0x30U, LCD_RS_CMD);
    delay_ms(5U);
    LCD_Write4Bits(0x30U, LCD_RS_CMD);
    delay_us(150U);
    LCD_Write4Bits(0x30U, LCD_RS_CMD);
    delay_us(150U);
    LCD_Write4Bits(0x20U, LCD_RS_CMD);
    delay_ms(2U);

    LCD_Command(0x28U);
    LCD_Command(0x08U);
    LCD_Command(0x01U);
    delay_ms(2U);
    LCD_Command(0x06U);
    LCD_Command(0x0CU);
}

static void LCD_SetCursor(uint8_t row, uint8_t col)
{
    LCD_Command((row == 0U ? 0x80U : 0xC0U) + col);
}

static void LCD_Print(const char *text)
{
    while (*text)
    {
        LCD_Char(*text++);
    }
}

static void LCD_ShowStatus(const char *line1, const char *line2)
{
    LCD_Command(0x01U);
    LCD_SetCursor(0U, 0U);
    LCD_Print(line1);
    if (line2)
    {
        LCD_SetCursor(1U, 0U);
        LCD_Print(line2);
    }
}

/* ---------- ADXL345 Accelerometer Driver ---------- */

static uint8_t ADXL_WriteReg(uint8_t reg, uint8_t value)
{
    uint32_t t = 10000U;

    while ((I2C1->SR2 & I2C_SR2_BUSY) && t--) {}
    if (t == 0U) { I2C1_Reset(); return 0U; }

    I2C1->CR1 |= I2C_CR1_START;
    t = 10000U;
    while (!(I2C1->SR1 & I2C_SR1_SB) && t--) {}
    if (t == 0U) goto fail;

    I2C1->DR = 0xA6U;
    t = 10000U;
    while (!(I2C1->SR1 & I2C_SR1_ADDR) && t--) {
        if (I2C1->SR1 & I2C_SR1_AF) goto fail;
    }
    if (t == 0U) goto fail;

    (void)I2C1->SR1;
    (void)I2C1->SR2;

    t = 10000U;
    while (!(I2C1->SR1 & I2C_SR1_TXE) && t--) {}
    if (t == 0U) goto fail;
    I2C1->DR = reg;

    t = 10000U;
    while (!(I2C1->SR1 & I2C_SR1_TXE) && t--) {}
    if (t == 0U) goto fail;
    I2C1->DR = value;

    t = 10000U;
    while (!(I2C1->SR1 & I2C_SR1_BTF) && t--) {}
    if (t == 0U) goto fail;

    I2C1->CR1 |= I2C_CR1_STOP;
    return 1U;

fail:
    I2C1->CR1 |= I2C_CR1_STOP;
    I2C1_Reset();
    return 0U;
}

static uint8_t ADXL_ReadBurst(uint8_t start_reg, uint8_t *buf, uint8_t len)
{
    uint32_t t = 10000U;

    while ((I2C1->SR2 & I2C_SR2_BUSY) && t--) {}
    if (t == 0U) { I2C1_Reset(); return 0U; }

    I2C1->CR1 |= I2C_CR1_START;
    t = 10000U;
    while (!(I2C1->SR1 & I2C_SR1_SB) && t--) {}
    if (t == 0U) goto fail;

    I2C1->DR = 0xA6U;
    t = 10000U;
    while (!(I2C1->SR1 & I2C_SR1_ADDR) && t--) {}
    if (t == 0U) goto fail;

    (void)I2C1->SR1;
    (void)I2C1->SR2;

    t = 10000U;
    while (!(I2C1->SR1 & I2C_SR1_TXE) && t--) {}
    if (t == 0U) goto fail;
    I2C1->DR = start_reg;

    t = 10000U;
    while (!(I2C1->SR1 & I2C_SR1_BTF) && t--) {}
    if (t == 0U) goto fail;

    I2C1->CR1 |= I2C_CR1_START;
    t = 10000U;
    while (!(I2C1->SR1 & I2C_SR1_SB) && t--) {}
    if (t == 0U) goto fail;

    I2C1->DR = 0xA7U;
    t = 10000U;
    while (!(I2C1->SR1 & I2C_SR1_ADDR) && t--) {}
    if (t == 0U) goto fail;

    if (len == 1U)
    {
        I2C1->CR1 &= ~I2C_CR1_ACK;
        (void)I2C1->SR1;
        (void)I2C1->SR2;
        I2C1->CR1 |= I2C_CR1_STOP;

        t = 10000U;
        while (!(I2C1->SR1 & I2C_SR1_RXNE) && t--) {}
        if (t == 0U) goto fail;
        buf[0] = (uint8_t)I2C1->DR;
    }
    else
    {
        I2C1->CR1 |= I2C_CR1_ACK;
        (void)I2C1->SR1;
        (void)I2C1->SR2;

        for (uint8_t i = 0; i < len; i++)
        {
            if (i == len - 1U)
            {
                I2C1->CR1 &= ~I2C_CR1_ACK;
                I2C1->CR1 |= I2C_CR1_STOP;
            }
            t = 10000U;
            while (!(I2C1->SR1 & I2C_SR1_RXNE) && t--) {}
            if (t == 0U) goto fail;
            buf[i] = (uint8_t)I2C1->DR;
        }
    }

    I2C1->CR1 |= I2C_CR1_ACK;
    return 1U;

fail:
    I2C1->CR1 |= I2C_CR1_STOP | I2C_CR1_ACK;
    I2C1_Reset();
    return 0U;
}

static uint8_t ADXL_ReadReg(uint8_t reg, uint8_t *value)
{
    return ADXL_ReadBurst(reg, value, 1U);
}

static uint8_t ADXL_Init(void)
{
    uint8_t id = 0U;

    delay_ms(10U);
    if (!ADXL_ReadReg(0x00U, &id)) return 0U;

    adxl_id = id;
    if (id != 0xE5U) return 0U;

    if (!ADXL_WriteReg(0x2DU, 0x08U)) return 0U;
    if (!ADXL_WriteReg(0x31U, 0x08U)) return 0U;
    if (!ADXL_WriteReg(0x2CU, 0x0AU)) return 0U;

    delay_ms(50U);
    return 1U;
}

static uint8_t ADXL_ReadAxes(int16_t *x, int16_t *y, int16_t *z)
{
    uint8_t raw[6];
    if (!ADXL_ReadBurst(0x32U, raw, 6U)) return 0U;

    *x = (int16_t)((uint16_t)raw[0] | ((uint16_t)raw[1] << 8));
    *y = (int16_t)((uint16_t)raw[2] | ((uint16_t)raw[3] << 8));
    *z = (int16_t)((uint16_t)raw[4] | ((uint16_t)raw[5] << 8));
    return 1U;
}

/* ---------- 12-Bit ADC1 Subsystem (PA0) ---------- */

static void ADC1_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    GPIOA->MODER &= ~(3U << 0);
    GPIOA->MODER |=  (3U << 0);
    GPIOA->PUPDR &= ~(3U << 0);

    ADC1->CR1 = 0U;
    ADC1->CR2 = 0U;
    ADC1->SQR1 = 0U;
    ADC1->SQR2 = 0U;
    ADC1->SQR3 = 0U;
    ADC1->SMPR2 = (ADC1->SMPR2 & ~7U) | 7U;

    ADC->CCR &= ~(3U << 16);
    ADC->CCR |= (1U << 16);
    ADC1->CR2 |= ADC_CR2_ADON;
}

static uint16_t ADC1_Read(void)
{
    ADC1->SR = 0U;
    ADC1->CR2 |= ADC_CR2_SWSTART;
    while (!(ADC1->SR & ADC_SR_EOC)) {}
    return (uint16_t)(ADC1->DR & 0x0FFFU);
}

/* ---------- USART2 Bluetooth Subsystem (HC-05) ---------- */

static void USART2_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    GPIOA->MODER &= ~((3U << 4) | (3U << 6));
    GPIOA->MODER |=  ((2U << 4) | (2U << 6));

    GPIOA->AFR[0] &= ~((0xFU << 8) | (0xFU << 12));
    GPIOA->AFR[0] |=  ((7U << 8) | (7U << 12));

    USART2->BRR = 0x0683U; /* 9600 Baud @ 16 MHz Clock */
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE;
    USART2->CR1 |= USART_CR1_UE;
}

static void UART_SendString(const char *s)
{
    while (*s)
    {
        while (!(USART2->SR & USART_SR_TXE)) {}
        USART2->DR = (uint8_t)*s++;
    }
}

/* Transmit telemetry packet formatted as [POT:<raw>,THR:<thresh>]\r\n */
static void UART_SendTelemetry(void)
{
    char buf[32];
    pot_value = ADC1_Read();
    uint32_t threshold = 20U + ((uint32_t)pot_value * 180U / 4095U);
    snprintf(buf, sizeof(buf), "[POT:%u,THR:%u]\r\n", (unsigned int)pot_value, (unsigned int)threshold);
    UART_SendString(buf);
}

/* ---------- Push Button Subsystem (PA1) ---------- */

static void Button_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    GPIOA->MODER &= ~(3U << 2);
    GPIOA->PUPDR &= ~(3U << 2);
    GPIOA->PUPDR |= (1U << 2);
}

static void Button_Process(void)
{
    static uint8_t last_btn_state = 1U;
    static uint32_t press_start = 0U;
    static uint8_t triggered = 0U;

    uint8_t current_state = (GPIOA->IDR & (1U << 1)) ? 1U : 0U;

    if (current_state == 0U)
    {
        if (last_btn_state == 1U)
        {
            press_start = milliseconds;
            triggered = 0U;
        }
        else if (!triggered && ((uint32_t)(milliseconds - press_start) >= 30U))
        {
            knock_system_enabled = !knock_system_enabled;
            knock_count = 0U;
            triggered = 1U;

            if (knock_system_enabled)
            {
                UART_SendString("\r\nKnocking system ON\r\n");
                LCD_ShowStatus("DOOR LOCKED", "Knock Mode: ON");
            }
            else
            {
                UART_SendString("\r\nKnocking system OFF\r\n");
                LCD_ShowStatus("DOOR LOCKED", "BT PIN Mode");
            }
        }
    }
    last_btn_state = current_state;
}

/* ---------- SG90 Servo Actuator Subsystem (TIM3 CH1 PA6) ---------- */

static void Servo_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    GPIOA->MODER &= ~(3U << 12);
    GPIOA->MODER |= (2U << 12);
    GPIOA->AFR[0] &= ~(0xFU << 24);
    GPIOA->AFR[0] |= (2U << 24);

    TIM3->PSC = 15U;
    TIM3->ARR = 19999U;

    TIM3->CCMR1 &= ~((7U << 4) | (1U << 3));
    TIM3->CCMR1 |= (6U << 4) | (1U << 3);
    TIM3->CCER |= TIM_CCER_CC1E;
    TIM3->CCR1 = 0U;

    TIM3->CR1 |= TIM_CR1_ARPE;
    TIM3->EGR |= TIM_EGR_UG;
    TIM3->CR1 |= TIM_CR1_CEN;
}

static void Servo_SetPosition(uint16_t pulse_us)
{
    TIM3->CCR1 = pulse_us;
    servo_power_start = milliseconds;
    servo_active = 1U;
}

static void Servo_Process(void)
{
    if (servo_active && ((uint32_t)(milliseconds - servo_power_start) >= SERVO_MOVE_TIME))
    {
        TIM3->CCR1 = 0U;
        servo_active = 0U;
    }
}

static void Door_Lock(void)
{
    Servo_SetPosition(SERVO_POS_LOCK);
    door_unlocked = 0U;
    UART_SendString("\r\nDOOR LOCKED\r\n");
    if (knock_system_enabled)
        LCD_ShowStatus("DOOR LOCKED", "Knock Mode: ON");
    else
        LCD_ShowStatus("DOOR LOCKED", "BT PIN Mode");
}

static void Door_Unlock(void)
{
    Servo_SetPosition(SERVO_POS_UNLOCK);
    door_unlocked = 1U;
    unlock_time = milliseconds;
    UART_SendString("\r\nDOOR UNLOCKED\r\n");
    LCD_ShowStatus("ACCESS GRANTED", "DOOR UNLOCKED");
}

/* ---------- Bluetooth Duplex Command Parser ---------- */

static void Bluetooth_Process(void)
{
    static char input[24];
    static uint8_t length = 0U;
    char c;
    uint8_t start, end;

    while (USART2->SR & USART_SR_RXNE)
    {
        c = (char)(USART2->DR & 0xFFU);

        if (c == '\r' || c == '\n')
        {
            if (length == 0U) continue;

            input[length] = '\0';

            start = 0U;
            while (input[start] == ' ' || input[start] == '\t' || input[start] == '"')
                start++;

            end = length;
            while (end > start && (input[end - 1U] == ' ' || input[end - 1U] == '\t' || input[end - 1U] == '"'))
                end--;

            uint8_t cmd_len = end - start;

            /* Remote Command: Force Lock */
            if (cmd_len == 4U && strncmp(&input[start], "LOCK", 4) == 0)
            {
                Door_Lock();
            }
            /* Remote Command: Toggle Mode */
            else if (cmd_len == 4U && strncmp(&input[start], "MODE", 4) == 0)
            {
                knock_system_enabled = !knock_system_enabled;
                knock_count = 0U;
                if (knock_system_enabled)
                {
                    UART_SendString("\r\nKnocking system ON\r\n");
                    LCD_ShowStatus("DOOR LOCKED", "Knock Mode: ON");
                }
                else
                {
                    UART_SendString("\r\nKnocking system OFF\r\n");
                    LCD_ShowStatus("DOOR LOCKED", "BT PIN Mode");
                }
            }
            /* PIN Verification */
            else if (cmd_len == 4U && strncmp(&input[start], PASSWORD, 4) == 0)
            {
                if (knock_system_enabled == 0U)
                {
                    UART_SendString("\r\nCorrect password\r\n");
                    Door_Unlock();
                }
                else
                {
                    UART_SendString("\r\nTurn knocking system OFF first\r\n");
                    LCD_ShowStatus("DENIED: TURN OFF", "KNOCK MODE 1ST");
                    delay_ms(1500U);
                    LCD_ShowStatus("DOOR LOCKED", "Knock Mode: ON");
                }
            }
            else
            {
                if (knock_system_enabled == 0U)
                {
                    UART_SendString("\r\nWrong password - access denied\r\n");
                    LCD_ShowStatus("WRONG PASSWORD", "ACCESS DENIED");
                    delay_ms(1500U);
                    LCD_ShowStatus("DOOR LOCKED", "BT PIN Mode");
                }
                else
                {
                    UART_SendString("\r\nKnock pattern required to unlock\r\n");
                }
            }

            length = 0U;
            input[0] = '\0';
        }
        else if (length < sizeof(input) - 1U)
        {
            input[length++] = c;
        }
        else
        {
            length = 0U;
            input[0] = '\0';
            UART_SendString("\r\nInput too long - try again\r\n");
        }
    }
}

/* ---------- ADXL345 Knock Detection Algorithm ---------- */

static void Knock_Detection(void)
{
    int16_t x, y, z;
    int32_t dx, dy, dz;
    uint32_t strength, threshold, now;
    uint32_t interval1, interval2, difference;

    if (!ADXL_ReadAxes(&x, &y, &z)) return;

    if (first_sample)
    {
        old_x = x;
        old_y = y;
        old_z = z;
        first_sample = 0U;
        return;
    }

    dx = (int32_t)x - old_x;
    dy = (int32_t)y - old_y;
    dz = (int32_t)z - old_z;

    old_x = x;
    old_y = y;
    old_z = z;

    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    if (dz < 0) dz = -dz;

    strength = (uint32_t)dx;
    if ((uint32_t)dy > strength) strength = (uint32_t)dy;
    if ((uint32_t)dz > strength) strength = (uint32_t)dz;

    pot_value = ADC1_Read();
    threshold = 20U + ((uint32_t)pot_value * 180U / 4095U);
    now = milliseconds;

    if (strength > threshold && (uint32_t)(now - last_knock) >= KNOCK_DEBOUNCE)
    {
        last_knock = now;

        if (knock_count == 0U)
        {
            knock_time[0] = now;
            knock_count = 1U;
            UART_SendString("Knock 1\r\n");
            LCD_SetCursor(1U, 0U);
            LCD_Print("Knock: [*  ]    ");
        }
        else if (knock_count == 1U)
        {
            interval1 = now - knock_time[0];
            if (interval1 >= KNOCK_MIN_MS && interval1 <= KNOCK_MAX_MS)
            {
                knock_time[1] = now;
                knock_count = 2U;
                UART_SendString("Knock 2\r\n");
                LCD_SetCursor(1U, 0U);
                LCD_Print("Knock: [** ]    ");
            }
            else
            {
                knock_time[0] = now;
                knock_count = 1U;
                LCD_SetCursor(1U, 0U);
                LCD_Print("Knock: [*  ]    ");
            }
        }
        else
        {
            interval1 = knock_time[1] - knock_time[0];
            interval2 = now - knock_time[1];

            if (interval2 >= KNOCK_MIN_MS && interval2 <= KNOCK_MAX_MS)
            {
                difference = (interval1 > interval2) ? interval1 - interval2 : interval2 - interval1;

                if (difference * 100U <= interval1 * PATTERN_TOLERANCE)
                {
                    knock_count = 0U;
                    UART_SendString("Correct knock pattern\r\n");
                    LCD_SetCursor(1U, 0U);
                    LCD_Print("Knock: [***] OK ");
                    Door_Unlock();
                }
                else
                {
                    knock_time[0] = now;
                    knock_count = 1U;
                    LCD_SetCursor(1U, 0U);
                    LCD_Print("Knock: [*  ]    ");
                }
            }
            else
            {
                knock_time[0] = now;
                knock_count = 1U;
                LCD_SetCursor(1U, 0U);
                LCD_Print("Knock: [*  ]    ");
            }
        }
    }

    if (knock_count > 0U && (uint32_t)(now - knock_time[knock_count - 1U]) > KNOCK_TIMEOUT)
    {
        knock_count = 0U;
        if (!door_unlocked)
        {
            LCD_SetCursor(1U, 0U);
            LCD_Print("Knock Mode: ON  ");
        }
    }
}

/* ---------- Main System Execution Loop ---------- */

int main(void)
{
    static uint32_t last_sample_time = 0U;
    static uint32_t last_telemetry_time = 0U;

    SysTick_Init();
    I2C1_Init();
    ADC1_Init();
    USART2_Init();
    Button_Init();
    Servo_Init();

    delay_ms(100U);

    UART_SendString("\r\nSTM32 Smart Access Control System Initialized\r\n");

    LCD_Init();
    LCD_ShowStatus("SMART ACCESS CTL", "Initializing...");

    adxl_ok = ADXL_Init();
    if (adxl_ok)
    {
        UART_SendString("ADXL345 Sensor: ONLINE\r\n");
    }
    else
    {
        UART_SendString("ADXL345 Sensor: ERROR - Check I2C Wiring\r\n");
        LCD_ShowStatus("ADXL345 ERROR!", "Check Wiring");
        delay_ms(1500U);
    }

    delay_ms(200U);
    Door_Lock();

    while (1)
    {
        Button_Process();
        Bluetooth_Process();
        Servo_Process();

        /* Sample ADXL345 every 20ms */
        if (adxl_ok && knock_system_enabled)
        {
            if ((uint32_t)(milliseconds - last_sample_time) >= 20U)
            {
                last_sample_time = milliseconds;
                Knock_Detection();
            }
        }

        /* Transmit Potentiometer ADC Telemetry every 600ms */
        if ((uint32_t)(milliseconds - last_telemetry_time) >= TELEMETRY_INTERVAL_MS)
        {
            last_telemetry_time = milliseconds;
            UART_SendTelemetry();
        }

        /* Auto-relock timer */
        if (door_unlocked && ((uint32_t)(milliseconds - unlock_time) >= AUTO_LOCK_MS))
        {
            Door_Lock();
        }
    }
}
