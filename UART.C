/**
 
 * Conexiones LCD:
 * - PA0: RS
 * - PA1: E
 * - PA2: D4
 * - PA3: D5
 * - PA4: D6
 * - PA5: D7
 * 
 * Conexiones Teclado 4x4:
 * - Filas: PB0, PB1, PB2, PB3 (salidas)
 * - Columnas: PB4, PB5, PB6, PB7 (entradas con pull-up)
 * 
 * Conexiones USART:
 * - USART2: PD5 (TX), PD6 (RX)
 * - USART6: PG14 (TX), PG9 (RX)
 */

 #include "stm32f7xx.h"
 #include <stdio.h>
 #include <string.h>
 volatile uint32_t tick_ms = 0;
 #define ROWS 4
 #define COLS 4
 const char keymap[ROWS][COLS] = {
     {'H', 'O', 'L', 'A'},
     {'M', 'U', 'N', 'B'},
     {'T', 'E', 'S', 'C'},
     {'*', ' ', '#', 'D'}
 };
 volatile uint8_t current_row = 0;
 volatile char last_key = '\0';
 volatile uint8_t key_pressed = 0;
 volatile uint32_t last_debounce_time = 0;
 #define DEBOUNCE_DELAY 150  
 #define MAX_INPUT_LENGTH 64
 char input_buffer[MAX_INPUT_LENGTH + 1];
 uint8_t buffer_index = 0;
 volatile uint8_t usart2_active = 0;
 volatile uint8_t usart6_active = 0;
 // Buffers para recepción USART
 #define USART_RX_BUFFER_SIZE 64
 char usart2_rx_buffer[USART_RX_BUFFER_SIZE];
 char usart6_rx_buffer[USART_RX_BUFFER_SIZE];
 volatile uint8_t usart2_rx_index = 0;
 volatile uint8_t usart6_rx_index = 0;
 volatile uint8_t usart2_rx_complete = 0;
 volatile uint8_t usart6_rx_complete = 0;
 void SysTick_Handler(void) {
     tick_ms++;
 }
 void process_keypress(uint8_t row, uint8_t col) {
     if ((tick_ms - last_debounce_time) < DEBOUNCE_DELAY) {
         return;
     }
     last_debounce_time = tick_ms;
     last_key = keymap[row][col];
     key_pressed = 1;
 }
 void EXTI4_IRQHandler(void) {
     if (EXTI->PR & EXTI_PR_PR4) {
         process_keypress(current_row, 0);
         EXTI->PR = EXTI_PR_PR4;
     }
 }
 void EXTI9_5_IRQHandler(void) {
     if (EXTI->PR & EXTI_PR_PR5) {
         process_keypress(current_row, 1);  
         EXTI->PR = EXTI_PR_PR5; 
     }
     else if (EXTI->PR & EXTI_PR_PR6) {
         process_keypress(current_row, 2); 
         EXTI->PR = EXTI_PR_PR6; 
     }
     else if (EXTI->PR & EXTI_PR_PR7) {
         process_keypress(current_row, 3);  
         EXTI->PR = EXTI_PR_PR7;  
     }
 }
 void USART2_IRQHandler(void) {
     if (USART2->ISR & USART_ISR_RXNE) {
         // Recibir dato
         char data = USART2->RDR;
         // Solo procesar si USART2 está activo
         if (usart2_active) {
             if (usart2_rx_index < USART_RX_BUFFER_SIZE - 1) {
                 usart2_rx_buffer[usart2_rx_index++] = data;
                 usart2_rx_buffer[usart2_rx_index] = '\0';
                 usart2_rx_complete = 1; 
             }
         }
     }
 }
 void USART6_IRQHandler(void) {
     if (USART6->ISR & USART_ISR_RXNE) {
         // Recibir dato
         char data = USART6->RDR;
         // Solo procesar si USART6 está activo
         if (usart6_active) {
             if (usart6_rx_index < USART_RX_BUFFER_SIZE - 1) {
                 usart6_rx_buffer[usart6_rx_index++] = data;
                 usart6_rx_buffer[usart6_rx_index] = '\0';
                 usart6_rx_complete = 1;  // Indicar que hay datos para mostrar
             }
         }
     }
 }
 void SystemClock_Config(void) {
     // Habilitar HSE (cristal externo)
     RCC->CR |= RCC_CR_HSEON;
     while(!(RCC->CR & RCC_CR_HSERDY)); 
     RCC->APB1ENR |= RCC_APB1ENR_PWREN;
     PWR->CR1 |= PWR_CR1_VOS;   
     FLASH->ACR = FLASH_ACR_LATENCY_7WS | FLASH_ACR_PRFTEN | FLASH_ACR_ARTEN;
     RCC->PLLCFGR = 0;  
     RCC->PLLCFGR |= (8);                     // PLLM = 8
     RCC->PLLCFGR |= (216 << 6);              // PLLN = 216
     RCC->PLLCFGR |= (0 << 16);         
     RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC_HSE;  
     RCC->CR |= RCC_CR_PLLON;
     while(!(RCC->CR & RCC_CR_PLLRDY)); 
     PWR->CR1 |= PWR_CR1_ODEN;
     while(!(PWR->CSR1 & PWR_CSR1_ODRDY));
     PWR->CR1 |= PWR_CR1_ODSWEN;
     while(!(PWR->CSR1 & PWR_CSR1_ODSWRDY));
     RCC->CFGR |= RCC_CFGR_HPRE_DIV1;    
     RCC->CFGR |= RCC_CFGR_PPRE1_DIV4;  
     RCC->CFGR |= RCC_CFGR_PPRE2_DIV2;   
     RCC->CFGR |= RCC_CFGR_SW_PLL;
     while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
     SystemCoreClock = 216000000;
 }
 void SysTick_Init(void) {
     SysTick->LOAD = (SystemCoreClock / 1000) - 1; // 216MHz = 216000 ciclos por 1ms
     SysTick->VAL = 0;
     SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
 }
 void delay_ms(uint32_t ms) {
     uint32_t start = tick_ms;
     while ((tick_ms - start) < ms);
 }
 // Definiciones para el control del LCD
 #define LCD_RS_HIGH() (GPIOA->BSRR = GPIO_BSRR_BS0)
 #define LCD_RS_LOW()  (GPIOA->BSRR = GPIO_BSRR_BR0)
 #define LCD_E_HIGH()  (GPIOA->BSRR = GPIO_BSRR_BS1)
 #define LCD_E_LOW()   (GPIOA->BSRR = GPIO_BSRR_BR1)
 void LCD_Send4Bits(uint8_t data) {
     GPIOA->BSRR = (GPIO_BSRR_BR2 | GPIO_BSRR_BR3 | GPIO_BSRR_BR4 | GPIO_BSRR_BR5);
     if (data & 0x01) GPIOA->BSRR = GPIO_BSRR_BS2;
     if (data & 0x02) GPIOA->BSRR = GPIO_BSRR_BS3; 
     if (data & 0x04) GPIOA->BSRR = GPIO_BSRR_BS4; 
     if (data & 0x08) GPIOA->BSRR = GPIO_BSRR_BS5; 
 }
 void LCD_EnablePulse(void) {
     LCD_E_HIGH();
     delay_ms(1);
     LCD_E_LOW();
     delay_ms(1);
 }
 void LCD_SendCommand(uint8_t cmd) {
     LCD_RS_LOW(); 
     LCD_Send4Bits(cmd >> 4);
     LCD_EnablePulse();
     LCD_Send4Bits(cmd & 0x0F); 
     LCD_EnablePulse();
     delay_ms(2); 
 }
 // Envía un dato (carácter) al LCD
 void LCD_SendData(uint8_t data) {
     LCD_RS_HIGH(); 
     LCD_Send4Bits(data >> 4);
     LCD_EnablePulse();
     LCD_Send4Bits(data & 0x0F); 
     LCD_EnablePulse();
     delay_ms(2); 
 }
 void LCD_Init(void) {
     RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; 
     GPIOA->MODER &= ~(0xFFF << 0);
     GPIOA->MODER |= 0x555;
     GPIOA->OTYPER &= ~(0x3F << 0);
     GPIOA->PUPDR &= ~(0xFFF << 0);
     GPIOA->OSPEEDR |= (0xAAA << 0);
     GPIOA->BSRR = (GPIO_BSRR_BR0 | GPIO_BSRR_BR1 | GPIO_BSRR_BR2 | 
                    GPIO_BSRR_BR3 | GPIO_BSRR_BR4 | GPIO_BSRR_BR5);
     delay_ms(100);
     LCD_RS_LOW();
     LCD_E_LOW();
     LCD_Send4Bits(0x3);
     LCD_EnablePulse();
     delay_ms(5);
     LCD_Send4Bits(0x3);
     LCD_EnablePulse();
     delay_ms(5);
     LCD_Send4Bits(0x3);
     LCD_EnablePulse();
     delay_ms(5);
     LCD_Send4Bits(0x2);
     LCD_EnablePulse();
     delay_ms(5);
     LCD_SendCommand(0x28); 
     delay_ms(2);
     LCD_SendCommand(0x08); 
     delay_ms(2);
     LCD_SendCommand(0x01); 
     delay_ms(5); 
     LCD_SendCommand(0x06); 
     delay_ms(2);
     LCD_SendCommand(0x0C); 
     delay_ms(2);
 }
 void LCD_Print(const char* str) {
     while (*str) {
         LCD_SendData(*str++);
     }
 }
 // Limpia la pantalla del LCD
 void LCD_Clear(void) {
     LCD_SendCommand(0x01);
     delay_ms(5); 
 }
 void LCD_SetCursor(uint8_t row, uint8_t col) {
     uint8_t address;
     if (row == 0) {
         address = 0x80 + col;
     } else {
         address = 0xC0 + col; 
     }
     LCD_SendCommand(address);
     delay_ms(1);
 }
 void LCD_DisplayBuffer(const char* buffer) {
     LCD_Clear();
     size_t len = strlen(buffer);
     // Primera línea (máximo 16 caracteres)
     LCD_SetCursor(0, 0);
     for (int i = 0; i < 16 && i < len; i++) {
         LCD_SendData(buffer[i]);
     }
     // Segunda línea si hay más de 16 caracteres
     if (len > 16) {
         LCD_SetCursor(1, 0);
         for (int i = 16; i < 32 && i < len; i++) {
             LCD_SendData(buffer[i]);
         }
     }
 }
 // Inicializa el teclado matricial
 void Keypad_Init(void) {
     RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
     RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
     GPIOB->MODER &= ~(0xFF << 0); 
     GPIOB->MODER |= 0x55;         
     GPIOB->MODER &= ~(0xFF << 8);        
     GPIOB->PUPDR &= ~(0xFF << 8);        
     GPIOB->PUPDR |= (0x55 << 8);         
     // Conectar EXTI4-7 a GPIOB
     SYSCFG->EXTICR[1] |= SYSCFG_EXTICR2_EXTI4_PB | 
                           SYSCFG_EXTICR2_EXTI5_PB |
                           SYSCFG_EXTICR2_EXTI6_PB |
                           SYSCFG_EXTICR2_EXTI7_PB;
     EXTI->FTSR |= EXTI_FTSR_TR4 | EXTI_FTSR_TR5 | EXTI_FTSR_TR6 | EXTI_FTSR_TR7;
     EXTI->IMR |= EXTI_IMR_MR4 | EXTI_IMR_MR5 | EXTI_IMR_MR6 | EXTI_IMR_MR7;
     // Configurar NVIC para EXTI4 y EXTI9_5 (para PB4-PB7)
     NVIC_SetPriority(EXTI4_IRQn, 1);
     NVIC_EnableIRQ(EXTI4_IRQn);
     NVIC_SetPriority(EXTI9_5_IRQn, 1);
     NVIC_EnableIRQ(EXTI9_5_IRQn);
     GPIOB->BSRR = GPIO_BSRR_BS0 | GPIO_BSRR_BS1 | GPIO_BSRR_BS2 | GPIO_BSRR_BS3;
 }
 void Keypad_ScanNextRow(void) {
     GPIOB->BSRR = GPIO_BSRR_BS0 | GPIO_BSRR_BS1 | GPIO_BSRR_BS2 | GPIO_BSRR_BS3;
     current_row = (current_row + 1) % ROWS;
     GPIOB->BSRR = (GPIO_BSRR_BR0 << current_row);
 }
 void USART2_Init(void) {
     RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
     RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
     // Configurar PD5 (TX) y PD6 (RX) para USART2
     GPIOD->MODER &= ~(0xF << 10);  
     GPIOD->MODER |= (0xA << 10); 
     GPIOD->AFR[0] &= ~(0xFF << 20);  
     GPIOD->AFR[0] |= (0x77 << 20);  
     USART2->CR1 &= ~USART_CR1_UE;
     // Configurar velocidad (baud rate) a 9600
     USART2->BRR = 2813;
     USART2->CR1 &= ~(USART_CR1_M | USART_CR1_PCE | USART_CR1_PS);
     USART2->CR2 &= ~USART_CR2_STOP;
     USART2->CR1 |= USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
     NVIC_SetPriority(USART2_IRQn, 2);
     NVIC_EnableIRQ(USART2_IRQn);
     USART2->CR1 |= USART_CR1_UE;
 }
 void USART6_Init(void) {
     RCC->AHB1ENR |= RCC_AHB1ENR_GPIOGEN;
     RCC->APB2ENR |= RCC_APB2ENR_USART6EN;
     // Configurar PG14 (TX) y PG9 (RX)
     GPIOG->MODER &= ~(0x3 << (14*2));       
     GPIOG->MODER |= (0x2 << (14*2));
     GPIOG->MODER &= ~(0x3 << (9*2));        
     GPIOG->MODER |= (0x2 << (9*2));         
     GPIOG->AFR[1] &= ~(0xF << ((14-8)*4));  
     GPIOG->AFR[1] |= (0x8 << ((14-8)*4));   
     GPIOG->AFR[1] &= ~(0xF << ((9-8)*4));   
     GPIOG->AFR[1] |= (0x8 << ((9-8)*4));    
     USART6->CR1 &= ~USART_CR1_UE;
     // Configurar velocidad (baud rate) a 28800 baudios (en lugar de 115200)
     USART6->BRR = 3750; // Cambiado de 1874 a 3750 para 28800 baudios
     USART6->CR1 &= ~(USART_CR1_M | USART_CR1_PCE | USART_CR1_PS);
     USART6->CR2 &= ~USART_CR2_STOP;
     USART6->CR1 |= USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
     NVIC_SetPriority(USART6_IRQn, 2);
     NVIC_EnableIRQ(USART6_IRQn);
     USART6->CR1 |= USART_CR1_UE;
 }
 void USART2_SendChar(char c) {
     while (!(USART2->ISR & USART_ISR_TXE));
     USART2->TDR = c;
 }
 void USART6_SendChar(char c) {  
     while (!(USART6->ISR & USART_ISR_TXE));   
     USART6->TDR = c;
 }
 void USART2_SendString(const char* str) {
     while (*str) {
         USART2_SendChar(*str++);
     }
 }
 void USART6_SendString(const char* str) {
     while (*str) {
         USART6_SendChar(*str++);
     }
 }
 int main(void) {
     SystemClock_Config();
     SysTick_Init();
     delay_ms(200);
     LCD_Init();
     Keypad_Init();
     USART2_Init();
     USART6_Init();
     LCD_Clear();
     LCD_SetCursor(0, 0);
     LCD_Print("Listo para");
     LCD_SetCursor(1, 0);
     LCD_Print("escribir...");
     delay_ms(500);
     LCD_Clear();
     while (1) {
         static uint32_t last_scan_time = 0;
         if ((tick_ms - last_scan_time) >= 5) {
             last_scan_time = tick_ms;
             Keypad_ScanNextRow();
         }
         if (key_pressed) {
             if (last_key == '#') {
                 USART2_SendString(input_buffer);
                 usart2_active = 1;
                 usart6_active = 0;
                 LCD_Clear();
                 LCD_SetCursor(0, 0);
                 LCD_Print("ENVIADO A USART2");
                 delay_ms(500);
                                 buffer_index = 0;
                                 input_buffer[0] = '\0';
                 LCD_Clear();
             }
             else if (last_key == '*') {
                 USART6_SendString(input_buffer);
                 usart2_active = 0;
                 usart6_active = 1;
                 LCD_Clear();
                 LCD_SetCursor(0, 0);
                 LCD_Print("ENVIADO A USART6");
                 delay_ms(500);
                                 buffer_index = 0;
                 input_buffer[0] = '\0';
                 LCD_Clear();
             }
           else if (last_key == 'S') {
                 if (buffer_index > 0) {
                     buffer_index--;
                     input_buffer[buffer_index] = '\0';
                 }
                 LCD_DisplayBuffer(input_buffer);
             }
             else {
                 if (buffer_index < MAX_INPUT_LENGTH) {
                     input_buffer[buffer_index++] = last_key;
                     input_buffer[buffer_index] = '\0';
                 }
                 LCD_DisplayBuffer(input_buffer);
             }
             key_pressed = 0;
         }
 // Procesar datos recibidos por USART2
 if (usart2_rx_complete) {
     // Mostrar datos en LCD
     LCD_Clear();
     LCD_SetCursor(0, 0);
     LCD_Print("CELL_RX:");
         delay_ms(500);
     LCD_Clear();              
         LCD_SetCursor(0, 0);    
         int len = strlen(usart2_rx_buffer);
         int i;
         for (i = 0; i < len; i++) {
     if (i == 16) {
                         LCD_SetCursor(1, 0);
                 }
                 LCD_SendData(usart2_rx_buffer[i]);
         }
     memset(usart2_rx_buffer, 0, USART_RX_BUFFER_SIZE);
     usart2_rx_index = 0;
     usart2_rx_complete = 0;
 }
 if (usart6_rx_complete) {
                         LCD_Clear();
                         LCD_SetCursor(0, 0);
                         LCD_Print("PC_RX:");
                         delay_ms(500);
                         LCD_Clear();             
                         LCD_SetCursor(0, 0);
                             int len = strlen(usart6_rx_buffer);
                             int i;
                             for (i = 0; i < len; i++) {
                             if (i == 16) {
                                             LCD_SetCursor(1, 0);
                                     }
                                     LCD_SendData(usart6_rx_buffer[i]);
                             }
                         memset(usart6_rx_buffer, 0, USART_RX_BUFFER_SIZE);
                         usart6_rx_index = 0;
                         usart6_rx_complete = 0;
                 }
     }
 }