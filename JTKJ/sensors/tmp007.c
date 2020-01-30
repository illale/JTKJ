/*
 * tmp007.c
 *
 *  Created on: 28.9.2016
 *  Author: Teemu Leppanen / UBIComp / University of Oulu
 *
 *  Datakirja: http://www.ti.com/lit/ds/symlink/tmp007.pdf
 */

#include <xdc/runtime/System.h>
#include <string.h>
#include "Board.h"
#include "tmp007.h"
#include <inttypes.h>

I2C_Transaction i2cTransaction;
char itxBuffer[4];
char irxBuffer[2];

void tmp007_setup(I2C_Handle *i2c) {

	System_printf("TMP007: Config not needed!\n");
    System_flush();
}

int tmp007_get_data(I2C_Handle *i2c) {
    
	float temperature = 0.0;
	uint16_t temp;

	uint8_t txBuffer[1];
    uint8_t rxBuffer[2];
    txBuffer[0] = 0x03;
    i2cTransaction.slaveAddress = Board_TMP007_ADDR;
    i2cTransaction.writeBuf = txBuffer;
    i2cTransaction.writeCount = 1;
    i2cTransaction.readBuf = rxBuffer;
    i2cTransaction.readCount = 2; 
    
	if (I2C_transfer(*i2c, &i2cTransaction)) {
	    
        temp = (rxBuffer[0] << 6) | (rxBuffer[1] >> 2);
        
        if (rxBuffer[0] & 0x80) {
                temp |= 0xF000;
        }
            
        return (temp/32);

	} else {

		System_printf("TMP007: Data read failed!\n");
		System_flush();
	}

	return temperature * 0.03125;
}


