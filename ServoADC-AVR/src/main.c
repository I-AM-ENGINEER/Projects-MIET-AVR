// Такая частота выбрана потому, что хорошо подходит для синхронизации на стандартных скоростях UART
#define F_CPU 1000000UL 

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

// Скважность ШИМ
#define PDM_MIN_TIMING		1000	// мкс
#define PDM_MAX_TIMING		2000	// мкс

#define LCD_PORT			PORTA
#define LCD_PIN_RS			PA5
#define LCD_PIN_E			PA4

// Порядок экспоненциального скользящего среднего фильтра, 
// больше - лучше сглаживание, меньше - быстрее реакция  
#define ADC_FILTER_ORDER	8

// Настройки канала АЦП
enum adc_channel_e{
	adc_channel_set = 0x00,
	adc_channel_min = 0x01,
	adc_channel_max = 0x02,
};

enum adc_channel_e adc_selected_channel;

volatile int32_t adc_filter_angle_min;
volatile int32_t adc_filter_angle_max;
volatile int32_t adc_filter_angle_set;
volatile uint8_t current_angle;

uint8_t pdm_adc2angle(uint16_t adc10bit);
void pdm_set_angle( uint8_t angle );

/**************************** Драйвер ADC ******************************/

void adc_init( void ){
	ADCSRA = (1<<ADEN)|(1<<ADSC)|(1<<ADIE)|(1<<ADPS2);
	adc_selected_channel = adc_channel_set;
	ADMUX = adc_selected_channel;
}

uint16_t adc_get_filtered( enum adc_channel_e channel ){
	switch(channel){
		case adc_channel_set: return (uint16_t)(adc_filter_angle_set >> ADC_FILTER_ORDER);
		case adc_channel_max: return (uint16_t)(adc_filter_angle_max >> ADC_FILTER_ORDER);
		case adc_channel_min: return (uint16_t)(adc_filter_angle_min >> ADC_FILTER_ORDER);
		default: return 0;
	}
}

ISR(ADC_vect){
	int32_t adc_value = (int32_t)ADC << ADC_FILTER_ORDER;
	
	switch(adc_selected_channel){
		case adc_channel_set:
			// Если первый запуск, установка значения без фильтрации
			if(adc_filter_angle_set == 0){
				adc_filter_angle_set = adc_value;
			}
			adc_filter_angle_set += (adc_value - adc_filter_angle_set) >> ADC_FILTER_ORDER;
			break;
		case adc_channel_max:
			// Если первый запуск, установка значения без фильтрации
			if(adc_filter_angle_max == 0){
				adc_filter_angle_max = adc_value;
			}
			adc_filter_angle_max += (adc_value - adc_filter_angle_max) >> ADC_FILTER_ORDER;
			break;
		case adc_channel_min:
			// Если первый запуск, установка значения без фильтрации
			if(adc_filter_angle_min == 0){
				adc_filter_angle_min = adc_value;
			}
			adc_filter_angle_min += (adc_value - adc_filter_angle_min) >> ADC_FILTER_ORDER;
			break;
		default:
			adc_selected_channel = 0;
			return;
	}
	
	// Перевод значений из 10 битного числа в угол 0...180
	uint8_t min_value = pdm_adc2angle(adc_get_filtered(adc_channel_min));
	uint8_t max_value = pdm_adc2angle(adc_get_filtered(adc_channel_max));
	uint8_t set_value = pdm_adc2angle(adc_get_filtered(adc_channel_set));

	if(set_value > max_value){
		set_value = max_value;
	}
	if(set_value < min_value){
		set_value = min_value;
	}
	if(max_value < min_value){
		uint8_t avr = max_value/2 + min_value/2;
		max_value = avr;
		min_value = avr;
		set_value = avr;
	}

	pdm_set_angle(set_value);
	current_angle = set_value;
	if(adc_selected_channel == adc_channel_max){
		adc_selected_channel = 0;
	}else{
		adc_selected_channel++;
	}

	ADMUX = adc_selected_channel;
	ADCSRA |= (1<<ADSC);
}

/******************** Драйвер дисплея ********************/

void lcd_latch( void ){
	LCD_PORT |=  (1 << LCD_PIN_E); // Установить высокий уровень на тактовом пине шины
	_delay_us(20); // Задержка, что бы дисплей успел считать данные с шины, должно быть больше 450нс
	LCD_PORT &= ~(1 << LCD_PIN_E); // Установить низкий уровень на тактовом пине шины
	_delay_us(20); // Задержка, что бы дисплей успел считать данные с шины, должно быть больше 450нс
}

void lcd_write( uint8_t byte ){
	LCD_PORT = (byte >> 4) | (LCD_PORT & 0xF0); // Записываем старшие 4 байта на шину
	lcd_latch();
	LCD_PORT = (byte & 0x0F) | (LCD_PORT & 0xF0); // Записываем младшие 4 байта на шину
	lcd_latch();
	_delay_us(100); // Выполнение записи может занять до 40мкс, ждем
}

void lcd_write_cmd( uint8_t cmd ){
	LCD_PORT &= ~(1 << LCD_PIN_RS); // Дисплей в режим команд (0 на пине RS)
	lcd_write(cmd); // Запись команды
}

void lcd_putc( uint8_t data ){
	LCD_PORT |= (1 << LCD_PIN_RS); // Дисплей в режим данных
	lcd_write(data); // Запись данных
}

void lcd_puts( const char* str ){
	while(*str){
		lcd_putc(*str);
		str++;
	}
}

void lcd_set_cursor( uint8_t line, uint8_t columm ){
	uint8_t position = (line << 6) | (columm); // Установка позиции курсора
	lcd_write_cmd(0x80 | position);
}

void lcd_clear( void ){
	lcd_write_cmd(0x01); // Команда очистки дисплея
	_delay_ms(2); // Очистка занимает до 1.52мс
}

void lcd_init( void ){
	// Пины дисплея в режим выхода
	DDRA = 0x3F;
	// Последовательность иницаиализации из даташита
	_delay_ms(20); // Задержка, что бы дисплей успел включиться
	LCD_PORT = 0x03 | (LCD_PORT & 0xF0); // Записываем старшие 4 байта на шину
	lcd_latch();
	_delay_ms(5);
	LCD_PORT = 0x03 | (LCD_PORT & 0xF0); // Записываем старшие 4 байта на шину
	lcd_latch();
	_delay_us(100); // Выполнение записи может занять до 40мкс, ждем
	
	lcd_write_cmd(0x32);
	lcd_write_cmd(0x28);
	lcd_write_cmd(0x0C);
	lcd_write_cmd(0x06);

	lcd_clear();
}

/******************** Драйвер генератора PDM сигнала ********************/

uint8_t pdm_adc2angle(uint16_t adc10bit){
	adc10bit+=2; // Небольшой хак, что бы можно было получить дипапозон 0...180, а не 0...179
	return ((adc10bit/4)*180)/256;
}

void pdm_init( void ){
	// PWM пин OC1A в режим выхода
	DDRB = (1 << PB5);
	// Настройка таймера генерарующего PDM сигнал для управления серво (TIM1)
	// Выход OC1A установить в лог 0 при сравнении таймера с каналом А
	TCCR1A = (1<<COM1A1)|(1<<WGM11);
	// Делитель 1, максимальный период таймера (2^16)/(F_CPU) = 65,5мс, надо 20мс, подходит
	// Генерация ШИМ с точной частотой
	TCCR1B = (1<<CS10)|(1<<WGM13)|(1<<WGM12);
	// Установка частоты, в данном случае для F_CPU=1МГц и выходной частоты 50Гц,
	// надо сбрасывать таймер по достижению 19999
	ICR1 = (uint16_t)(F_CPU/(50)-1);
}

void pdm_set_angle( uint8_t angle ){
	uint16_t us = (uint32_t)((uint32_t)angle*(PDM_MAX_TIMING - PDM_MIN_TIMING)/180UL + PDM_MIN_TIMING);
	OCR1A = us*(F_CPU/1000000)-1;
}

/******************** Высокоуровневые обработчики ********************/

int main( void ){
	pdm_init();
	// Инициализация дисплея
	lcd_init();
	// запуск АЦП
	adc_init(); 
	// Глобальные прерывания включить
	sei();
	
	while (1){
		char print_str[8];
		// Отрисовка значений на дисплее
		lcd_set_cursor(0,0);
		snprintf(print_str, sizeof(print_str), "%04hu", adc_get_filtered(adc_channel_min));
		lcd_puts(print_str);
		lcd_putc(' ');
		snprintf(print_str, sizeof(print_str), "%04hu", adc_get_filtered(adc_channel_set));
		lcd_puts(print_str);
		lcd_putc(' ');
		snprintf(print_str, sizeof(print_str), "%04hu", adc_get_filtered(adc_channel_max));
		lcd_puts(print_str);
		lcd_set_cursor(1,0);
		snprintf(print_str, sizeof(print_str), "%03hhu", pdm_adc2angle(adc_get_filtered(adc_channel_min)));
		lcd_puts(print_str);
		lcd_putc(' ');
		lcd_putc(' ');
		snprintf(print_str, sizeof(print_str), "%03hhu", current_angle);
		lcd_puts(print_str);
		lcd_putc(' ');
		lcd_putc(' ');
		snprintf(print_str, sizeof(print_str), "%03hhu", pdm_adc2angle(adc_get_filtered(adc_channel_max)));
		lcd_puts(print_str);
		_delay_ms(20);
	}
}
