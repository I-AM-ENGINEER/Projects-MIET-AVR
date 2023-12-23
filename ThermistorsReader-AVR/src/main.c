// Такая частота выбрана потому, что хорошо подходит для синхронизации на стандартных скоростях UART
#define F_CPU 8000000UL 

#include <stdlib.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#define DIGIT_NONE	0b00000000
#define DIGIT_MINUS	0b01000000
#define DIGIT_C		0b00111001
#define DIGIT_F		0b01110001

// Табличка для перевода числа в то, какие элементы семисегментника зажечь
const uint8_t digits2seg_table[] = {
	0b00111111,	// 0
	0b00000110,	// 1
	0b01011011,	// 2
	0b01001111, // 3
	0b01100110, // 4
	0b01101101, // 5
	0b01111101, // 6
	0b00000111, // 7
	0b01111111, // 8
	0b01101111, // 9
};

enum mode_e {
	MODE_CURRENT,
	MODE_MIN,
	MODE_MAX,
};

enum mode_e mode; 
bool is_farenheit;
uint8_t sensor_select;

uint8_t digits[4];
volatile int16_t adc_readed_value;
int16_t adc_readed_min[2] = {1023, 1023};
int16_t adc_readed_max[2];

// Прерывание при переполнении таймера 0
ISR(TIMER0_OVF_vect){
	static uint8_t digit_num;
	// Выключить все индикаторы
	PORTC |= 0x0F;
	// Включить только нужный
	PORTC &= ~(1 << digit_num) | 0xF0;
	// Вывести нужный символ
	PORTB = digits[digit_num];
	// Если вывели последний символ, начать сначала
	if(digit_num == 3){
		digit_num = 0;
	}else{
		digit_num++;
	}
}

ISR(PCINT2_vect){
	static uint8_t last_btn_state = 0xE0;
	uint8_t current_btn_state = PIND & 0xE0;
	uint8_t changes = last_btn_state ^ current_btn_state;
	// Кнопка Current/min/max
	// Если состояние изменилось
	if(changes & (1 << PD5)){
		// И кнопка не была нажата, а теперь нажата
		if(current_btn_state & (1 << PD5)){
			if(mode == MODE_MAX){
				mode = 0;
			}else{
				mode++;
			}
			PORTD |= (1 << PD4) | (1 << PD3) | (1 << PD2);
			switch (mode){
				case MODE_CURRENT: 
				PORTD &= ~(1 << PD4);
				break;
				case MODE_MIN: 
				PORTD &= ~(1 << PD3);
				break;
				case MODE_MAX: 
				PORTD &= ~(1 << PD2);
				break;
				default: break;
			}
		}
	}
	// Градусы цельсия/фаренгейта
	if(changes & (1 << PD6)){
		if(current_btn_state & (1 << PD6)){
			is_farenheit = !is_farenheit;
		}
	}
	// Выбор сенсора
	if(changes & (1 << PD7)){
		if(current_btn_state & (1 << PD7)){
			// Переключаем сенсор
			if(sensor_select){
				// Переключение мултиплексора АЦП
				ADMUX = (6 << MUX0);
				// Индикация светодиодами
				PORTD |=  (1 << PD1);
				PORTD &= ~(1 << PD0);
				sensor_select = 0;
			}else{
				ADMUX = (7 << MUX0);
				// Индикация светодиодами
				PORTD |=  (1 << PD0);
				PORTD &= ~(1 << PD1);
				sensor_select = 1;
			}
		}
	}
	last_btn_state = current_btn_state;
}

ISR(ADC_vect){
	// Сработало прерывание - читаем значение АЦП
	adc_readed_value = (int16_t)ADC;
}

void print_value(int16_t value){
	// Если число нельзя отрисовать на 3 элементах
	if(value >= 1000) return;
	if(value <= -100) return;

	uint8_t cursor = 0;
	uint8_t digit = 0;
	if(value < 0){
		if(value <= -10){
			digits[cursor++] = DIGIT_MINUS;
		}else{
			digits[cursor++] = DIGIT_NONE;
			digits[cursor++] = DIGIT_MINUS;
		}
		value = -value;
	}else{
		while(value >= 100){
			value -= 100;
			digit++;
		}
		if(digit == 0){
			digits[cursor++] = DIGIT_NONE;
		}else{
			digits[cursor++] = digits2seg_table[digit];
			digit = 0;
		}
	}
	while(value >= 10){
		value -= 10;
		digit++;
	}

	if((digit == 0) && (digits[0] == DIGIT_NONE)){
		digits[cursor] = DIGIT_NONE;
	}else{ 
		digits[cursor] = digits2seg_table[digit];
	}
	digits[2] = digits2seg_table[value];
}

int16_t adc2temperature( uint16_t adc_value ){
	const float B = 4050.0f;
	// Формула для NTC термистора
	float f = 20000.0f/(1024.0f / adc_value - 1.0f);
	float R_R0 = f / 20000.0f;
	float lnR_R0 = logf(R_R0)/B;
	lnR_R0 += 1 / (25.0f + 273.15f);
	float T = 1/lnR_R0 - 273.15;
	return (int16_t)(T);
}

int16_t celsium2farenheit( int16_t celsium ){
	return (int16_t)((float)celsium * 9.0f / 5.0f) + 32;
}

int main( void ){
	// Выводы семисегметника в режим выхода
	DDRB = 0xFF;
	DDRC = 0x0F;

	// Светодиоды в режим выхода
	DDRD = 0x1F;
	// Подтяжка на кнопках, светодиоды выключены
	PORTD = 0xEE;

	// Делитель 64, при частоте контроллера 8МГц, таймер будет переполняться 8000000/(256*64)=500 раз в секунду
	TCCR0B = (1 << CS01) | (1 << CS00);
	// Включить прерывание при переполнениии
	TIMSK0 = (1 << TOIE0);

	// Выбрать 6 канал АЦП
	ADMUX = (6 << MUX0); 
	// Велючитт АЦП, автоматический триггер, предделитель 16, начать преобразование 
	ADCSRA = (1 << ADEN) | (1 << ADIE) | (1 << ADATE) | (1 << ADPS2) | (1 << ADSC);

	// Включить внешние прерывания PCINT21, PCINT22, PCINT23
	PCICR = (1 << PCIE2);
	PCMSK2 = (1 << PCINT21) | (1 << PCINT22) | (1 << PCINT23);

	// Разрешить прерывания
	sei();
	while (1){
		_delay_ms(10);

		if(adc_readed_value > adc_readed_max[sensor_select]){
			adc_readed_max[sensor_select] = adc_readed_value;
		}
		if(adc_readed_value < adc_readed_min[sensor_select]){
			adc_readed_min[sensor_select] = adc_readed_value;
		}

		// Считаем температуру по сложным формулам
		int16_t T;
		switch(mode){
			case MODE_CURRENT: T = adc2temperature(adc_readed_value); break;
			case MODE_MAX: T = adc2temperature(adc_readed_max[sensor_select]); break;
			case MODE_MIN: T = adc2temperature(adc_readed_min[sensor_select]); break;
			default: break;
		}
		// Переводим в фаренгейты, если надо, отображаем нужный знак
		if(is_farenheit){
			T = celsium2farenheit(T);
			digits[3] = DIGIT_F;
		}else{
			digits[3] = DIGIT_C;
		}
		print_value(T);
	}	
}
