#include <msp430g2553.h>

#define	LED_RED	BIT0
#define LED_GREEN	BIT6

#define LED_ON(x)	P1OUT|=x
#define LED_OFF(x)	P1OUT&=~(x)

void uart_putc(unsigned char c) {
	while (!(IFG2 & UCA0TXIFG))
		;
	UCA0TXBUF = c;
}

void uart_puts(unsigned char *data) {
	while (*data)
		uart_putc(*data++);
	return;
}

void delay_us(unsigned int us) {
	while (us) {
		__delay_cycles(1); // 1 for 1 Mhz set 16 for 16 MHz
		us--;
	}
}

void delay_ms(unsigned int ms) {
	while (ms) {
		__delay_cycles(1000); //1000 for 1MHz and 16000 for 16MHz
		ms--;
	}
}

void blink_led(char led, char t) {
	char i = 0;
	for (i = 0; i < t; i++) {
		LED_ON(led);
		delay_ms(100);
		LED_OFF(led);
		delay_ms(100);
	}
}

unsigned char data_prev = 0;
unsigned char data_curr = 0;

unsigned char reic[128];
unsigned char index_reic = 0;

unsigned char isHasData = 0;
unsigned char isHasGps = 0;

unsigned char lat[20];
unsigned char index_lat = 0;
unsigned char lon[20];
unsigned char index_lon = 0;
unsigned char commas[10];
unsigned char index_comma = 0;

void process_gps(void) {
	unsigned char i = 0;

	index_comma = 0;
	commas[0] = 0;
	commas[1] = 0;
	commas[2] = 0;
	commas[3] = 0;

	while (i < 127 && reic[i] != '\0') {
		if (reic[i] == ',') {
			commas[index_comma] = i;
			index_comma++;
		}
		if (reic[i] == ',' && reic[i + 1] == 'V') {
			blink_led(LED_RED, 3);
			return;
		} else if (reic[i] == ',' && reic[i + 1] == 'A') {
			blink_led(LED_GREEN, 3);
		}
		i++;
	}

	lat[0] = '\0';
	index_lat = 0;

	lon[0] = '\0';
	index_lon = 0;

	// get lat
	for (i = commas[0] + 1; i < commas[1]; i++) {
		lat[index_lat] = reic[i];
		index_lat++;
	}
	lat[index_lat] = '\0';

	//get lon
	for (i = commas[2] + 1; i < commas[3]; i++) {
		lon[index_lon] = reic[i];
		index_lon++;
	}
	lon[index_lon] = '\0';
}

int main(void) {
	WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT

	/* Use Calibration values for 1MHz Clock DCO*/
	DCOCTL = 0;
	BCSCTL1 = CALBC1_1MHZ;
	DCOCTL = CALDCO_1MHZ;

	/* Configure Pin Muxing P1.1 RXD and P1.2 TXD */
	P1SEL |= BIT1 | BIT2;
	P1SEL2 |= BIT1 | BIT2;

	/* Place UCA0 in Reset to be configured */
	UCA0CTL1 = UCSWRST;

	/* Configure */
	UCA0CTL1 |= UCSSEL_2;                     // SMCLK
	UCA0BR0 = 104;                            // 1MHz 9600
	UCA0BR1 = 0;                              // 1MHz 9600
	UCA0MCTL = UCBRS0;                        // Modulation UCBRSx = 1

	/* Take UCA0 out of reset */
	UCA0CTL1 &= ~UCSWRST;

	/* Enable USCI_A0 RX interrupt */
	IE2 |= UCA0RXIE;

	/* Enable leds*/
	P1DIR |= LED_GREEN + LED_RED;
	LED_OFF(LED_GREEN|LED_RED);

	/* Blink leds when reset */
	blink_led(LED_RED | LED_GREEN, 5);

	/* Init Button */
	P1REN |= BIT3;                   // Enable internal pull-up/down resistors
	P1OUT |= BIT3;                   //Select pull-up mode for P1.3
	P1IE |= BIT3;                       // P1.3 interrupt enabled
	P1IES |= BIT3;                     // P1.3 Hi/lo edge
	P1IFG &= ~BIT3;                  // P1.3 IFG cleared

	__enable_interrupt();

	while (1) {
		if (isHasGps) {
			blink_led(LED_GREEN | LED_RED, 3);
			process_gps();
			index_reic = 0;
			reic[index_reic] = '\0';
			isHasData = 0;
			isHasGps = 0;
		}
	}
}

#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void) {
//	UCA0TXBUF = UCA0RXBUF;                    // TX -> RXed character
//	data[index] = UCA0RXBUF;
//	index++;
	data_prev = data_curr;
	data_curr = UCA0RXBUF;
	if (isHasGps)
		return;
	if (data_curr == '*') {
		if (isHasData) {
			isHasGps = 1;
			reic[index_reic] = '\0';
		} else {
			index_reic = 0;
			reic[index_reic] = '\0';
		}
	} else {
		reic[index_reic] = data_curr;
		index_reic++;
		if (index_reic > 4 && !isHasData) {
			if ((reic[index_reic - 5] == 'G') && (reic[index_reic - 4] == 'P')
					&& (reic[index_reic - 3] == 'G')
					&& (reic[index_reic - 2] == 'L')
					&& (reic[index_reic - 1] == 'L')) {
				index_reic = 0;
				reic[index_reic] = '\0';
				isHasData = 1;
			}
		}
	}
}

#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void) {
	blink_led(LED_GREEN, 3);
	P1IFG &= ~BIT3; // P1.3 interrupt flag cleared
}
