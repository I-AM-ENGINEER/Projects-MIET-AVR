// Такая частота выбрана потому, что хорошо подходит для синхронизации на стандартных скоростях UART
#define F_CPU 8000000UL 

#include <stdlib.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

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
	0b01000000, // -
};

uint8_t digits[4];
volatile int16_t adc_readed_value;

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

ISR(ADC_vect){
	// Сработало прерывание - читаем значение АЦП
	adc_readed_value = (int16_t)ADC;
}

void print_value(int16_t value){
	if(value < 0){
		digits[0] = digits2seg_table[10];
		value = abs(value);
	}else{
		digits[0] = 0x00;
	}

	digits[3] = digits2seg_table[value%10];
	value/=10;
	digits[2] = digits2seg_table[value%10];
	value/=10;
	digits[1] = digits2seg_table[value%10] | 0x80; // Поставить точку
}

int16_t adc2current_ma(int16_t adc_value){
	// Да, на AVR нет FPU и операции с float медленные, но операции с int32_t не быстрее, а делать математику с int16_t больно и бессмысленно
	float voltage_adc = ((float)adc_value / 1024.0f) * 5.0f;
	float voltage_acs712 = -voltage_adc*33.0f/100.0f;
	float current = (voltage_acs712-2.5f) / 0.200f; // 185 мВ = 1А, но в протеусе это 200 мВ на 1 А
	int16_t current_ma = (int16_t)(current*1000.0f);
	return current_ma;
}

int main( void ){
	// Выводы семисегметника в режим выхода
	DDRB = 0xFF;
	DDRC = 0x0F;
	// Делитель 64, при частоте контроллера 8МГц, таймер будет переполняться 8000000/(256*64)=500 раз в секунду
	TCCR0B = (1 << CS01) | (1 << CS00);
	// Включить прерывание при переполнениии
	TIMSK0 = (1 << TOIE0);


	// Выбрать 6 канал АЦП
	ADMUX = (6 << MUX0); 
	// Велючитт АЦП, автоматический триггер, предделитель 16, начать преобразование 
	ADCSRA = (1 << ADEN) | (1 << ADIE) | (1 << ADATE) | (1 << ADPS2) | (1 << ADSC);
	// Разрешить прерывания
	sei();
	int16_t i = 100;
	while (1){
		int16_t current = adc2current_ma(adc_readed_value)/10;
		print_value(current);
		_delay_ms(10);
	}	
}
