/*
  ESP8266_LED_64x16_Matrix.cpp - A 64x16 matrix driver for MQTT.
  Qi Sun
  https://github.com
*/

#include "ESP8266_LED_64x16_Matrix.h"
#include "Arduino.h"


ESP8266_LED_64x16_Matrix::ESP8266_LED_64x16_Matrix()
{
}

//screen mode is an integer code: 0:64x16 1:128x16;
void ESP8266_LED_64x16_Matrix::setDisplay(uint8_t displayMode)
{
	ESP8266_LED_64x16_Matrix::isrInstance = this;
	switch (displayMode)
	{
	case 0:
		used_buffer_size = 144;
		columnNumber = 8;
		rowCount = 16;
		break;
	case 1:
		used_buffer_size = 272;
		columnNumber = 16;
		rowCount = 16;
		break;
	}

	scrollPointer = 0;
	scanRow = 0;
	clear_buffer();

	timer1_attachInterrupt(interruptHandler);

	
	timer1_write(nextT);
	timer1_disable();
	delay(100);
	//timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);

	//delay(100);
}

void ESP8266_LED_64x16_Matrix::setPins(uint8_t pins[8])
{
	latchPin = pins[0];
	clockPin = pins[1];
	data_R1 = pins[2];
	//data_R2 = ;
	en_74138 = pins[3];
	uint8_t la_74138 = pins[4];
	uint8_t lb_74138 = pins[5];
	uint8_t lc_74138 = pins[6];
	uint8_t ld_74138 = pins[7];

	rowPin = (1 << la_74138) | (1 << lb_74138) | (1 << lc_74138) | (1 << ld_74138);

	pinMode(latchPin, OUTPUT);  pinMode(clockPin, OUTPUT);
	pinMode(data_R1, OUTPUT);   //pinMode(data_R2, OUTPUT);

	pinMode(en_74138, OUTPUT);
	pinMode(la_74138, OUTPUT);  pinMode(lb_74138, OUTPUT);
	pinMode(lc_74138, OUTPUT);  pinMode(ld_74138, OUTPUT);

}

void ESP8266_LED_64x16_Matrix::turnOn()
{
	//timer1_isr_init();
	timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
	//Set up ESP watchdog
	//ESP.wdtDisable();
	//ESP.wdtEnable(WDTO_8S);
}

void ESP8266_LED_64x16_Matrix::turnOff()
{
	timer1_disable();
	digitalWrite(en_74138, HIGH);

	clear_buffer();
	scanRow = 0;
	scrollPointer = 0;

}


void ESP8266_LED_64x16_Matrix::clear_buffer()
{
	for (int i = 0; i <used_buffer_size; i++)
	{
		buffer[i] = 0x00;
	}
}

void ESP8266_LED_64x16_Matrix::clear_buffer2()
{
	for (int i = 0; i <used_buffer_size; i++)
	{
		buffer2[i] = 0x00;
	}
}

void ESP8266_LED_64x16_Matrix::drawChar(uint16_t xcol, uint16_t ycol, uint8_t n, uint8_t whichBuffer) {
	uint8_t charbytes[rowCount], fontrows;
	int index;
	fontrows = 16;
	index = (n-32)*fontrows; // go to the right code for this character																						 // addressing start at buffer and add y (rows) * (WIDTH is 64 so WIDTH/8) is 8 plus (x / 8) is 0 to 7
	for (byte i = 0; i<fontrows; i++) {  // fill up the charbytes array with the right bits
		charbytes[i] = font8x16_basic[index + i];
		 //charbytes[i] = 'A';
	};
																				 // addressing start at buffer and add y (rows) * (WIDTH is 64 so WIDTH/8) is 8 plus (x / 8) is 0 to 7
	byte *pDst = buffer + (ycol * (columnNumber + 1)) + xcol;

	//byte *pDst;
	//if (whichBuffer == 2)
	//{
	//	pDst = buffer2 + (ycol * (columnNumber + 1)) + xcol;
	//}
	//else
	//{
	//	pDst = buffer + (ycol * (columnNumber + 1)) + xcol;
	//}
	byte *pSrc = charbytes; // point at the first set of 8 pixels    
	for (byte i = 0; i<fontrows; i++) {
		*pDst = *pSrc;     // populate the destination byte
		pDst += columnNumber + 1;         // go to next row on buffer
		pSrc++;            // go to next set of 8 pixels in character
	}

	//Serial.println(buffer[0]);
	//Serial.println("draw buffer111");
};


void ESP8266_LED_64x16_Matrix::moveLeft(uint8_t pixels, uint8_t rowstart, uint8_t rowstop) { // routine to move certain rows on the screen "pixels" pixels to the left
	uint8_t row, column;
	uint16_t index;
	for (column = 0; column<(columnNumber + 1); column++) {
		for (row = rowstart; row<rowstop; row++) {
			index = (row * (columnNumber + 1)) + column; /// right here!
			if (column == (columnNumber))
				buffer[index] = buffer[index] << pixels; // shuffle pixels left on last column and fill with a blank
			else {                // shuffle pixels left and add leftmost pixels from next column
				uint8_t incomingchar = buffer[index + 1];
				buffer[index] = buffer[index] << pixels;
				for (byte x = 0; x<pixels; x++) { buffer[index] += ((incomingchar & (0x80 >> x)) >> (7 - x)) << (pixels - x - 1); };
			}
		}
	}
};

void ESP8266_LED_64x16_Matrix::scrollTextHorizontal(uint16_t delaytime)
{
	// display next character of message
	drawChar(columnNumber, 0, message[scrollPointer % (message.length())], 1);
	scrollPointer++;
	if (scrollPointer >= message.length())
	{
		scrollPointer = 0;
	}
	// move the text 1 pixel at a time
	for (uint8_t i = 0; i<8; i++) {
		delay(delaytime);
		moveLeft(1, 0, rowCount);

	};

}

void ESP8266_LED_64x16_Matrix::BreakTextInFrames(uint16_t delaytime)
{
	clear_buffer();
	for (uint8_t i = 0; i < columnNumber; i++)
	{
		drawChar(i, 0, message[scrollPointer], 1);
		scrollPointer++;
		if (scrollPointer >= message.length())
		{
			scrollPointer = 0;
			break;
		}
	}
	delay(delaytime);
}

void ESP8266_LED_64x16_Matrix::scrollTextVertical(uint16_t delaytime)
{
	clear_buffer2();
	for (uint8_t i = 0; i < columnNumber; i++)
	{
		drawChar(i, 0, message[scrollPointer], 2);
		scrollPointer++;
		if (scrollPointer >= message.length())
		{
			scrollPointer = 0;
			break;
		}
	}

	for (uint8_t t = 0; t < rowCount; t++)
	{
		for (uint8_t i = 0; i < rowCount - 1; i++)
		{
			for (uint8_t j = 0; j < columnNumber; j++)
			{
				buffer[i*(columnNumber + 1) + j] = buffer[(i + 1)*(columnNumber + 1) + j];
			}
		}

		for (uint8_t j = 0; j < columnNumber; j++)
		{
			buffer[(rowCount - 1)*(columnNumber + 1) + j] = buffer2[t*(columnNumber + 1) + j];
		}
		delay(50);
	}
	delay(delaytime);
}

void  ESP8266_LED_64x16_Matrix::ISR_TIMER_SCAN()
{
	//noInterrupts();
	digitalWrite(en_74138, HIGH);     // Turn off display
									  // Shift out 8 columns
	for (byte column = 0; column<columnNumber; column++) {
		byte index = column + (scanRow *(columnNumber+1));
		shiftOut(data_R1, clockPin, MSBFIRST, buffer[index]);
	};

	digitalWrite(latchPin, LOW);
	digitalWrite(latchPin, HIGH);

	WRITE_PERI_REG(PERIPHS_GPIO_BASEADDR + 8, rowPin);
	uint32_t rowPinSet = ((scanRow >> 3) & 0x01) << ld_74138;
	rowPinSet = rowPinSet | (((scanRow >> 2) & 0x01) << lc_74138);
	rowPinSet = rowPinSet | (((scanRow >> 1) & 0x01) << lb_74138);
	rowPinSet = rowPinSet | ((scanRow & 0x01) << la_74138);
	WRITE_PERI_REG(PERIPHS_GPIO_BASEADDR + 4, rowPinSet);
	digitalWrite(en_74138, LOW);     // Turn on display
	scanRow++;                       // Do the next pair of rows next time this routine is called
	if (scanRow == rowCount)
	{
		scanRow = 0;
		//Serial.print(buffer[27]);
	}
	timer1_write(nextT);
	//interrupts();
}

ESP8266_LED_64x16_Matrix * ESP8266_LED_64x16_Matrix::isrInstance;

void ESP8266_LED_64x16_Matrix::interruptHandler()
{
	isrInstance->ISR_TIMER_SCAN();
}