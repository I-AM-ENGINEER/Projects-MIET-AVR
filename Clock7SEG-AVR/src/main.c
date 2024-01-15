#define F_CPU 8000000UL 

#include <stdlib.h>
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

// Табличка для перевода числа в то, какие элементы семисегментника зажечь
#define DIGIT_NONE	0b00000000
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
	MODE_TIME,
	MODE_TIME_SET,
	MODE_TIMER_COUNTERDOWN,
	MODE_TIMER_SET,
	MODE_ALARM,
	MODE_ALARM_SET,
};

enum cursor_e {
	CURSOR_HR_H,
	CURSOR_HR_L,
	CURSOR_MN_H,
	CURSOR_MN_L,
	CURSOR_NONE,
};

enum button_e {
	BTN_TIME_SET,
	BTN_TIMER_SET,
	BTN_ALARM_SET,
	BTN_OK,
	BTN_NEXT,
	BTN_ALARM_ENABLE,
	BTN_NONE,
};

typedef struct{
	uint8_t seconds;
	uint8_t minutes;
	uint8_t hours;
}ds3234_time_t;

volatile enum mode_e mode = MODE_TIME;
volatile enum cursor_e cursor_pos;
volatile int32_t timer_timeout;
volatile bool alarm_enable;
ds3234_time_t bcdtime;

uint8_t digits[4];
uint8_t enter_time_digits[4];

// -------------------------------------------------- ds3234 SPI драйвер -------------------------------------------------- 

uint8_t spi_write( uint8_t tx_byte ){
	SPDR = tx_byte;
	while(!(SPSR & (1<<SPIF)));
	return SPDR;
}

void ds3234_select( void ){
	PORTB &= ~(1 << PB4);
}

void ds3234_unselect( void ){
	PORTB |= (1 << PB4);
}

uint8_t ds3234_reg_read( uint8_t addr ){
	ds3234_select();
	spi_write(addr);
	uint8_t data = spi_write(0xFF);
	ds3234_unselect();
	return data;
}

void ds3234_reg_write( uint8_t addr, uint8_t data ){
	ds3234_select();
	spi_write(addr | 0x80);
	spi_write(data);
	ds3234_unselect();
}

// -------------------------------------------------- ds3234 драйвер -------------------------------------------------- 

bool ds3234_alarm_check( void ) {
	uint8_t status_reg = ds3234_reg_read(0x0F);
	// 0x03 - Флаги будильников 
	if(status_reg & 0x03){
		// Сброс флага
		status_reg &= ~0x03;
		ds3234_reg_write(0x0F, status_reg);
		return true;
	}
	return false;
}

void ds3234_alarm_enable( void ) {
	// Сброс флага, на всякий случай, что бы будильник не сработал мгновенно
	ds3234_alarm_check();
	// Включение будильников
	ds3234_reg_write(0x0E, 0x03);
	alarm_enable = true;
	// Сброс настроек дня/года, т.к. они для нас не важны
	ds3234_reg_write(0x03, 0x00);
	ds3234_reg_write(0x04, 0x00);
	ds3234_reg_write(0x05, 0x00);
	ds3234_reg_write(0x06, 0x00);
}

void ds3234_alarm_disable( void ) {
	ds3234_reg_write(0x0E, 0x00);
	alarm_enable = false;
}

void ds3234_write_alarm( ds3234_time_t *time ) {
	// Использование сразу 2х будильников обусловлено тем, что будильник может быть как сегодня, так и завтра
	// Например, сейчас 20:00, первый будильник устанавливается на 12:00 текущего дня, второй на 12:00 следующего дня
	// первый будильник в таком случае никогда не сработает, т.к. событие в прошлом
	// Можно это сделать проверкой в какой день надо будильник поставить, но можно и такой хак использовать
	// Установка первого будильника
	ds3234_reg_write(0x07, time->seconds);
	ds3234_reg_write(0x08, time->minutes);
	ds3234_reg_write(0x09, time->hours);
	// Установка второго будильника
	ds3234_reg_write(0x0B, time->minutes);
	ds3234_reg_write(0x0C, time->hours);
	ds3234_reg_write(0x0D, 2); // установка следующего дня

	// Сброс настроек дня/месяца/года, что бы будильники сработали сегодня/завтра, а не через неделю/месяц
	ds3234_reg_write(0x03, 0x00);
	ds3234_reg_write(0x04, 0x00);
	ds3234_reg_write(0x05, 0x00);
	ds3234_reg_write(0x06, 0x00);
}

void ds3234_write_time( ds3234_time_t *time ) {
	ds3234_reg_write(0x00, time->seconds);
	ds3234_reg_write(0x01, time->minutes);
	ds3234_reg_write(0x02, time->hours);
	// Сброс настроек дня/года, т.к. они для нас не важны
	ds3234_reg_write(0x03, 0x00);
	ds3234_reg_write(0x04, 0x00);
	ds3234_reg_write(0x05, 0x00);
	ds3234_reg_write(0x06, 0x00);
}

void ds3234_read_time( ds3234_time_t *time ) {
	time->seconds = ds3234_reg_read(0x00);
	time->minutes = ds3234_reg_read(0x01);
	time->hours   = ds3234_reg_read(0x02);
}

// -------------------------------------------------- Обработка прерываний -------------------------------------------------- 

uint8_t bcd2int( uint8_t bcd ){
	return (uint8_t)((bcd & 0x0F) + (bcd >> 4) * 10); 
}

void btn_scan( void ){
	static enum button_e last_pressed_button = BTN_NONE;
	enum button_e pressed_button;

	uint8_t buttons_state = PINA;
	for(pressed_button = BTN_TIME_SET; pressed_button < BTN_NONE; pressed_button++){
		// Если какая то кнопка нажата
		if(((1 << pressed_button) & ~buttons_state)){
			break;
		}
	}

	// В зависимости от того, какая кнопка нажата, включаем нужное меню
	if(last_pressed_button != pressed_button){
		switch (pressed_button){
		case BTN_TIME_SET: 	mode = MODE_TIME_SET;  cursor_pos = CURSOR_HR_H; memset(enter_time_digits, 0, sizeof(enter_time_digits)); break;
		case BTN_TIMER_SET: mode = MODE_TIMER_SET; cursor_pos = CURSOR_HR_H; memset(enter_time_digits, 0, sizeof(enter_time_digits)); break;
		case BTN_ALARM_SET: mode = MODE_ALARM_SET; cursor_pos = CURSOR_HR_H; memset(enter_time_digits, 0, sizeof(enter_time_digits)); break;
		case BTN_OK:
			if(mode == MODE_ALARM){
				PORTD |= (1 << PD4); // ALARM led disable
				mode = MODE_TIME;
			}
			switch (cursor_pos) {
				// Установка времени (часы, минуты)
				case CURSOR_HR_H:
					enter_time_digits[CURSOR_HR_H]++;
					// Если это таймер, то максимальное время таймера - 99 минут 59 секунд
					if(enter_time_digits[CURSOR_HR_H] > 9){
						enter_time_digits[CURSOR_HR_H] = 0;
					}else if((mode != MODE_TIMER_SET) && (enter_time_digits[CURSOR_HR_H] > 2)){
						enter_time_digits[CURSOR_HR_H] = 0;
					}
					break;
				case CURSOR_HR_L:
					enter_time_digits[CURSOR_HR_L]++;
					if(enter_time_digits[CURSOR_HR_H] > 5){
						enter_time_digits[CURSOR_HR_L] = 0;
					}else if((mode != MODE_TIMER_SET) && (enter_time_digits[CURSOR_HR_H] == 2) && (enter_time_digits[CURSOR_HR_L] > 3)){
						enter_time_digits[CURSOR_HR_L] = 0;
					}

					if(enter_time_digits[CURSOR_HR_L] > 9){
						enter_time_digits[CURSOR_HR_L] = 0;
					}
					break;
				case CURSOR_MN_H: 
					enter_time_digits[CURSOR_MN_H]++;
					if(enter_time_digits[CURSOR_MN_H] > 5){
						enter_time_digits[CURSOR_MN_H] = 0;
					}
					break;
				case CURSOR_MN_L: 
					enter_time_digits[CURSOR_MN_L]++;
					if(enter_time_digits[CURSOR_MN_L] > 9){
						enter_time_digits[CURSOR_MN_L] = 0;
					}
					break;
				default: break;
			}
			break;
		case BTN_NEXT:
			if(cursor_pos != CURSOR_MN_L){
				cursor_pos++;
			}else{
				ds3234_time_t new_time;
				switch (mode){
				case MODE_TIME_SET:
					// сохранить новое время
					new_time.seconds = 0;
					new_time.minutes = (enter_time_digits[3] & 0x0F) | ((enter_time_digits[2] << 4) & 0xF0); 
					new_time.hours   = (enter_time_digits[1] & 0x0F) | ((enter_time_digits[0] << 4) & 0xF0); 
					ds3234_write_time(&new_time);
					mode = MODE_TIME;
					break;
				case MODE_TIMER_SET:
					// Запустить таймер
					new_time.seconds = bcd2int((enter_time_digits[3] & 0x0F) | ((enter_time_digits[2] << 4) & 0xF0)); 
					new_time.minutes = bcd2int((enter_time_digits[1] & 0x0F) | ((enter_time_digits[0] << 4) & 0xF0));
					timer_timeout = (new_time.seconds + new_time.minutes*60);
					mode = MODE_TIMER_COUNTERDOWN;
					break;
				case MODE_ALARM_SET:
					// сохранить новое время будильника
					new_time.seconds = 0;
					new_time.minutes = (enter_time_digits[3] & 0x0F) | ((enter_time_digits[2] << 4) & 0xF0); 
					new_time.hours   = (enter_time_digits[1] & 0x0F) | ((enter_time_digits[0] << 4) & 0xF0); 
					ds3234_write_alarm(&new_time);
					ds3234_alarm_enable();
					mode = MODE_TIME;
					break;
				case MODE_TIMER_COUNTERDOWN: break;
				default: mode = MODE_TIME; break;
				}
				
			}
			break;
		case BTN_ALARM_ENABLE: 
			alarm_enable = !alarm_enable;
			if(alarm_enable){
				ds3234_alarm_enable();
			}else{
				ds3234_alarm_disable();
			}
			break;
		default:break;
		}
	}
	last_pressed_button = pressed_button;
}

// Прерывание при переполнении таймера 0
// Вывод семисегментника + опрос клавиатуры
ISR(TIMER0_OVF_vect){
	static uint8_t digit_num;
	// Выключить все индикаторы
	PORTD |= 0x0F;
	// Включить только нужный
	PORTD &= ~(1 << digit_num) | 0xF0;
	// Вывести нужный символ
	PORTC = digits[digit_num];
	// Если вывели последний символ, начать сначала
	if(digit_num == 3){
		digit_num = 0;
	}else{
		digit_num++;
	}
	btn_scan();
}

// Прерывание раз в секунду для работы режима таймера
ISR(INT2_vect){
	timer_timeout--;
}


// -------------------------------------------------- Главный цикл -------------------------------------------------- 

int main( void ){
	// Выводы семисегметника и светодиодов в режим выхода
	DDRC = 0xFF;
	DDRD = 0xFF;
	PORTD = 0xFF;
	// Подтяжка на кнопках
	PORTA = 0xFF;
	// Подтяжка INT2
	PORTB = (1 << PB2);

	// Таймер для семимегментника и опроса кнопок
	// Делитель 64, при частоте контроллера 8МГц, таймер будет переполняться 8000000/(256*64)=500 раз в секунду
	TCCR0 = (1 << CS01) | (1 << CS00);
	// Включить прерывание при переполнениии
	TIMSK = (1 << TOIE0);

	// Настройка SPI
	DDRB = 0xB0;
	// Включить SPI в режим мастера, делитель 4, частота 2МГц
	SPCR = (1 << SPE) | (1 << MSTR) | (1 << CPHA);

	// Прерывание INT2 (1 раз в секунду для работы таймера)
	GICR = (1 << INT2);

	// Разрешить прерывания
	sei();
	
	_delay_ms(200); // Иначе не успевает инициализироваться часы
	// Выдача частоты 1 Гц на пине SQW, для работы таймера
	ds3234_reg_read(0x0E);
	ds3234_reg_write(0x0E, 0x00);

	while (1){
		// Вывод информации о включенном/выключенном будильнике
		if(alarm_enable){
			if(ds3234_alarm_check()){
				mode = MODE_ALARM;
				ds3234_alarm_disable();
			}
			PORTD &= ~(1 << PD5);
		}else{
			PORTD |= (1 << PD5);
		}

		// Если режим таймера
		if((mode == MODE_TIMER_COUNTERDOWN) || (mode == MODE_TIMER_SET)){
			PORTD &= ~(1 << PD6);
		}else{
			PORTD |= (1 << PD6);
		}
		
		static uint8_t blink_digit_counter;
		switch (mode){
		case MODE_TIMER_SET:
		case MODE_ALARM_SET:
		case MODE_TIME_SET:
			for(uint8_t i = 0; i < 4; i++){
				digits[i] = digits2seg_table[enter_time_digits[i]];
			}
			// Что бы выбранный символ моргал
			if(blink_digit_counter > 5){
				digits[cursor_pos] = DIGIT_NONE;
			}
			if(blink_digit_counter > 10){
				blink_digit_counter = 0;
			}
			blink_digit_counter++;
			break;	
		case MODE_TIME:
			ds3234_read_time(&bcdtime);
			digits[0] = digits2seg_table[(bcdtime.hours & 0xF0) >> 4];
			digits[1] = digits2seg_table[bcdtime.hours & 0x0F];
			digits[2] = digits2seg_table[(bcdtime.minutes & 0xF0) >> 4];
			digits[3] = digits2seg_table[bcdtime.minutes & 0x0F];
			break;	
		case MODE_TIMER_COUNTERDOWN:
			if(timer_timeout <= 0){
				mode = MODE_ALARM;
			}
			digits[3] = digits2seg_table[timer_timeout%10];
			digits[2] = digits2seg_table[(timer_timeout%60)/10];
			digits[1] = digits2seg_table[(timer_timeout/60)%10];
			digits[0] = digits2seg_table[(timer_timeout/600)%10];
			break;
		case MODE_ALARM:
			PORTD &= ~(1 << PD4); // Зажечь светодиод ALARM
			// Что бы символы мигали во время будильника/таймера
			for(uint8_t i = 0; i < 4; i++){
				if(blink_digit_counter > 5){
					digits[i] = DIGIT_NONE;
				}else{
					digits[i] = digits2seg_table[0];
				}
				if(blink_digit_counter > 10){
					blink_digit_counter = 0;
				}
			}
			blink_digit_counter++;
			break;
		default: break;
		}
		_delay_ms(50);
	}	
}
