#define F_CPU 8000000UL

#include <stdbool.h>
#include <stdint.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/cpufunc.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>

#define COMMAND 0xF0			/* 0xFFF0 */
#define CMD_ARG 0xF1			/* 0xFFF1 */

#define CMD_EXIT    0
#define CMD_GETCH   1
#define CMD_PUTCH   2
#define CMD_GETTIME 3
#define CMD_PRTIME  4

const uint8_t code65[] PROGMEM = {
#include "bootloader.h"
};

const uint8_t test65[] PROGMEM = {
#include "test65.h"
};

char hex[] = "0123456789ABCDEF";

uint8_t getp = 0;
uint8_t putp = 0;
uint8_t count = 0;
uint8_t buf[256];

volatile uint16_t timer_ov_count;

void GPIO_Init(void)
{
	/* D
	   0 RXD0
	   1 TXD0
	   2 RDY (IN, no-pull-up <=> OUT, low), outer pull-up
	   3 BE (OUT)     init 0 (active 1)
	   4 RESB (OUT)   init 0 (active 0)
	   5 IRQB (OUT)   init 1 (active 0)
	   6 RD (OUT)     init 0 (active 1)
	   7 WR (OUT)     init 0 (active 1)

	   76543210
	   11111000  DDRD
	   00100011  PORTD (output or pull-up)
	*/
	DDRD = 0xF8;
	PORTD = 0x23;

	/* A: A0-A7 input, pull-up */
	DDRA = 0x00;
	PORTA = 0xFF;

	/* C: D0-D7 input, pull-up */
	DDRC = 0x00;
	PORTC = 0xFF;

	/* B
	   0
	   1
	   2
	   3 OC0A (timer, output setting is needed)
	   4
	   5
	   6
	   7

	   76543210
	   11111111  DDRB
	   00000000  PORTB
	 */
	DDRB = 0xFF;				/* output */
	PORTB = 0x00;				/* all 0 */
}

void TCNT0_Init(void)
{
	/* CTC mode, Toggle OC0A on Compare Match */
//	TCCR0A = _BV(COM0A0) | _BV(WGM01);
	TCCR0A = (1 << 6) | (1 << 1);

	/* clk_{I/O} (No prescaling) */
	TCCR0B = _BV(CS00);

	/* freq = clk / 2(OCR0A + 1) */
	OCR0A = 1;

	/* output PB3 = OC0A (needed) */
	DDRB |= 1 << 3;
}

void TCNT1_Init(void)
{
	TCCR1B = 1 << 1;  /* clk_{I/O} / 8 */
	TIMSK1 = 1;		  /* TOIE1: Timer/Counter1, Overflow Interrupt Enable */
	TIFR1 = 1;		  /* TOV1: Timer/Counter1, Overflow Flag */
}

ISR (TIMER1_OVF_vect)    // Timer1 ISR
{
	timer_ov_count++;
}

uint16_t get_time(uint16_t *p_highword)
{
	uint8_t sreg;
	sreg = SREG;
	cli();
	uint16_t low = TCNT1;
	uint16_t high = timer_ov_count;
	SREG = sreg;
	*p_highword = high;
	return low;
}

void USART_Init(unsigned int baud)
{
	/* Set baud rate */
	uint16_t rate = ((F_CPU / 8) / baud) - 1;
	UBRR0H = rate >> 8;
	UBRR0L = rate;

	/* async, non-parity, 1-bit stop, 8-bit data */
	UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);

	/* double speed */
	UCSR0A |= _BV(U2X0);

	/* receiver enable, transmitter enable */
	UCSR0B = _BV(RXEN0) | _BV(TXEN0);
//	UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0);
}

void USART_Transmit(unsigned char data)
{
	/* Wait for empty transmit buffer */
	while (!(UCSR0A & _BV(UDRE0)))
		;
	/* Put data into buffer, sends the data */
	UDR0 = data;
}

unsigned char USART_Receive(void)
{
	/* Wait for data to be received */
	while (!(UCSR0A & _BV(RXC0)))
		;
	/* Get and return received data from buffer */
	return UDR0;
}

void send_hex1(unsigned char x)
{
    USART_Transmit(hex[x & 0x0F]);
}

void send_hex2(unsigned char x)
{
    send_hex1(x >> 4);
    send_hex1(x);
}

void send_hex4(uint16_t x)
{
    send_hex2(x >> 8);
    send_hex2(x);
}

#if 1
void SetCLK(uint8_t b)
{
	if (b)
		PORTB |= 1 << 3;
	else
		PORTB &= ~(1 << 3);
}

void SetSYNC(uint8_t b)
{
	if (b)
		PORTB |= 1 << 0;
	else
		PORTB &= ~(1 << 0);
}
#endif

void PullDownRDY(void)
{
	/* PORTD bit 2 is 1 on GPIO_Init */
	DDRD |= 1 << 2;				/* output */
}

void OpenRDY(void)
{
	DDRD &= ~(1 << 2);			/* input */
}

uint8_t GetRDY(void)
{
	return PIND & (1 << 2);
}

void SetBE(uint8_t b)
{
	if (b)
		PORTD |= 1 << 3;
	else
		PORTD &= ~(1 << 3);
}

uint8_t GetBE(void)
{
	return PORTD & (1 << 3);
}

void SetRESB(uint8_t b)
{
	if (b)
		PORTD |= 1 << 4;
	else
		PORTD &= ~(1 << 4);
}

void SetIRQB(uint8_t b)
{
	if (b)
		PORTD |= 1 << 5;
	else
		PORTD &= ~(1 << 5);
}

void SetRD(uint8_t b)
{
	if (b)
		PORTD |= 1 << 6;
	else
		PORTD &= ~(1 << 6);
}

void SetWR(uint8_t b)
{
	if (b)
		PORTD |= 1 << 7;
	else
		PORTD &= ~(1 << 7);
}

void write_mem(uint8_t addr, uint8_t data)
{
	if (GetBE()) {				/* for safety */
		USART_Transmit('!');
		return;
	}

	DDRA = 0xFF;				/* address output */
	DDRC = 0xFF;				/* data output */
	PORTA = addr;
	PORTC = data;

	SetWR(1);
	_delay_us(1);
	SetWR(0);
	_NOP();
	_NOP();

	PORTA = 0xFF;
	DDRA = 0x00;
	PORTC = 0xFF;
	DDRC = 0x00;
}

uint8_t read_mem(uint8_t addr)
{
 	if (GetBE()) {				/* for safety */
		USART_Transmit('!');
		return 0;
	}

	DDRA = 0xFF;				/* address output */
	PORTA = addr;

	SetRD(1);
	_delay_us(1);
	uint8_t data = PINC;
	SetRD(0);
	_NOP();
	_NOP();

	PORTA = 0xFF;
	DDRA = 0x00;

	return data;
}

void write_bootloader65(void)
{
	for (int i = 0; i < sizeof(code65); ++i) {
		uint8_t x = pgm_read_byte(&(code65[i]));
		write_mem(i, x);
	}
}

void write_test_program(void)
{
	for (int i = 0; i < sizeof(test65); ++i) {
		uint8_t x = pgm_read_byte(&(test65[i]));
		write_mem(i, x);
	}
}

int poll(void)
{
	if (UCSR0A & _BV(RXC0)) {
		uint8_t c = UDR0;
		if (count < 255) {
			buf[putp] = c;
			putp++;
			count++;
		}
		return c;
	} else {
		return -1;
	}
}

uint8_t getch(void)
{
	if (count > 0) {
		uint8_t c = buf[getp];
		getp++;
		count--;
		return c;
	} else {
		return USART_Receive();
	}
}

uint8_t command_loop(void)
{
	count = 0;
	getp = 0;
	putp = 0;

	for ( ; ; ) {
		uint8_t c = poll();
#if 0
		if (c == 0x1F) {
			PullDownRDY();
			_delay_us(2);
			SetBE(0);
			break;
		}
#endif

		/* check RDY */
		uint8_t b = GetRDY();	/* 0 or non-0 */
		if (b == 0) {
//			_delay_us(1);
			_NOP();
			_NOP();
			_NOP();
//			_NOP();
			uint8_t b = GetRDY();	/* 0 or non-0 */
			if (b == 0) {
				SetBE(0);
//				_delay_us(1);
				uint8_t cmd = read_mem(COMMAND);
				if (cmd == CMD_EXIT) {
					USART_Transmit(0x19);
					break;
				} else if (cmd == CMD_GETCH) {
					uint8_t c = getch();
//					if (c == 0x1F) break;
					write_mem(CMD_ARG, c);
				} else if (cmd == CMD_PUTCH) {
					uint8_t c = read_mem(CMD_ARG);
					USART_Transmit(c);
				} else if (cmd == CMD_GETTIME) {
					uint16_t h;
					uint16_t l = get_time(&h);
					write_mem(CMD_ARG, l);
					write_mem(CMD_ARG + 1, l >> 8);
					write_mem(CMD_ARG + 2, h);
					write_mem(CMD_ARG + 3, h >> 8);
				} else if (cmd == CMD_PRTIME) {
					uint16_t h;
					uint16_t l = get_time(&h);
					send_hex4(h);
					send_hex4(l);
					USART_Transmit(0x0D);
				} else {
					/* unknown command */
					USART_Transmit('?');
				}
				SetBE(1);
//				_delay_us(2);
				/* trigger interrupt to wake up */
				SetIRQB(0);
				_delay_us(2);
				SetIRQB(1);
			}
		}
	}
}

void write_vectors(void)
{
	/* reset vector: 0xFF00 */
	write_mem(0xFC, 0x00);
	write_mem(0xFD, 0xFF);
}

void dump_mem(void)
{
	for (uint8_t i = 0; i < 16; ++i) {
		send_hex4(i * 16);
		for (uint8_t j = 0; j < 16; ++j) {
			uint8_t x = read_mem(i * 16 + j);
			USART_Transmit(' ');
			send_hex2(x);
		}
		USART_Transmit(0x0d);
	}
}

void test_mem_fill(uint8_t fill)
{
	for (uint16_t i = 0; i < 256; ++i) {
		write_mem(i, fill);
	}
	for (uint16_t i = 0; i < 256; ++i) {
		uint8_t x = read_mem(i);
		if (x != fill)
			USART_Transmit('X');
	}
}

void test_mem_addr(void)
{
	for (uint16_t i = 0; i < 256; ++i) {
		write_mem(i, i);
	}
	for (uint16_t i = 0; i < 256; ++i) {
		uint8_t x = read_mem(i);
		if (x != i)
			USART_Transmit('X');
	}
}

void test_mem(void)
{
	test_mem_fill(0x00);
	test_mem_fill(0xFF);
	test_mem_fill(0x55);
	test_mem_fill(0xAA);
	test_mem_addr();
}

int main(void)
{
	GPIO_Init();
	TCNT0_Init();
	TCNT1_Init();
	USART_Init(38400);
	sei();

	for ( ; ; ) {
		USART_Transmit('#');
		uint8_t c = USART_Receive();
		USART_Transmit(c);
		if (c == 'e') {
			PullDownRDY();
			SetBE(0);
			write_vectors();
			write_bootloader65();
			SetRESB(0);
			OpenRDY();
			SetBE(1);
			_delay_ms(100);
			SetRESB(1);
			command_loop();
		} else if (c == 't') {	/* test65: echo */
			PullDownRDY();
			SetBE(0);
			write_vectors();
			write_test_program();
			SetRESB(0);
			OpenRDY();
			SetBE(1);
			_delay_ms(100);
			SetRESB(1);
			command_loop();
		} else if (c == 'c') {
			uint16_t h;
			uint16_t l = get_time(&h);
			USART_Transmit(0x0D);
			send_hex4(h);
			send_hex4(l);
		} else if (c == 'm') {
			test_mem();
		} else if (c == 'd') {
			dump_mem();
		} else if (c == 'r') {
			SetRESB(0);
			_delay_ms(100);
			SetRESB(1);
		} else if (c == 'i') {
			SetIRQB(0);
			_delay_us(2);
			SetIRQB(1);
		} else {
		}
	}
	return 0;
}
