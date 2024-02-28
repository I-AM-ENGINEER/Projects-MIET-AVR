#define	F_CPU 1000000

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>

unsigned int period;
unsigned int rpm;
unsigned int rpm_target;
unsigned char duty;
char usart_rx_string[100];
unsigned int rx_sting_l;

// Копипаста из даташита
void USART_Init( unsigned int ubrr)
{
	/*Set baud rate */
	UBRR0H = (unsigned char)(ubrr>>8);
	UBRR0L = (unsigned char)ubrr;
	/* Enable receiver and transmitter, receive interrupt enable */
	UCSR0B = (1<<RXEN0)|(1<<TXEN0)|(1<<RXCIE0);
	/* Set frame format: 8data, 1stop bit */
	UCSR0C = (3<<UCSZ00);
}

// Таймер 1 для захвата периода (стр. 12 AVR130)
void init_tim1_encoder(void)
{
	/* Timer clock = I/O clock / 8 */
	TCCR1B = (1<<CS11);
	/* Clear ICF1. Clear pending interrupts */
	TIFR1   = 1<<ICF1;
	/* Enable Timer 1 Capture Event Interrupt */
	TIMSK1  = (1<<ICIE1)|(1<<TOIE1);
}

// Таймер 0 для генерации ШИМ (стр. 17 AVR130)
void init_tim0_pwm(void)
{
	/* Enable non inverting 8-Bit PWM */
	TCCR0A = (1<<COM0A1)|(1<<WGM01)|(1<<WGM00);
	/* Timer clock = I/O clock/8 */
	TCCR0B = (1<<CS01);
	/* Set the compare value to control duty cycle */
	OCR0A = 0x00;
	/* Set OC2A pin as output */
	DDRD |= (1 << PD6);
}

int main(){
	USART_Init(12); // Скорость 4800 бод (стр. 210, таблица 20-9)
	init_tim0_pwm();
	init_tim1_encoder();
	
	// Выводы PD0, PC1 в режим выхода (для управления направлением)
	DDRC = (1<<PC0)|(1<<PC1);

	// Разрешение прерываний
	sei();
	
	while (1){
		//char t[100];
		if(period == 0){
			rpm = 0;
		}else{
			rpm = 7500000 / period / 24;
		}
		// Если скорость меньше нужного, увеличиваем обороты
		if(rpm < rpm_target){
			if(duty < 255){
				duty += 1;
			}
		}
		// Если скорость больше нужного, уменьшаем обороты
		if(rpm > rpm_target){
			if(duty > 0){
				duty -= 1;
			}
		}
		// Если надо 0 оборотов, вырубаем
		if(rpm_target == 0){
			PORTC &= ~((1<<PC1)|(1<<PC0));
		}
		
		OCR0A = duty;
		_delay_ms(20);
	}
}

ISR (TIMER1_CAPT_vect){
	period = TCNT1;
	TCNT1 = 0; // Считать заново
}

ISR (TIMER1_OVF_vect){
	period = 0; // Если таймер переполнился - считаем, что движок встал
}

ISR (USART_RX_vect){
	// Эхо, что бы символ отображался в терминале
	char byte = UDR0;
	UDR0 = byte;
	
	usart_rx_string[rx_sting_l] = byte;
	rx_sting_l += 1;
	// Если строка закончилась
	if((byte == '\n') || (byte == '\r')){
		usart_rx_string[rx_sting_l] = 0;
		if(usart_rx_string[0] == 'F'){
			// Перевести строку в число
			rpm_target = atoi(usart_rx_string + 1);
			// Установить направление движения
			PORTC |= (1<<PC0);
			PORTC &= ~(1<<PC1);
		}
		if(usart_rx_string[0] == 'B'){
			// Перевести строку в число
			rpm_target = atoi(usart_rx_string + 1);
			// Установить направление движения
			PORTC |= (1<<PC1);
			PORTC &= ~(1<<PC0);
		}
		rx_sting_l = 0;
	}
}
