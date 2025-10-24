/*
 * main.h
 *
 *  Created on: Dec 8, 2024
 *      Author: admin
 */

#ifndef MAIN_H_
#define MAIN_H_

void I2C1_Init(void);
void GPIO_Init(void);
void I2C1_Write(uint8_t addr, uint8_t data, uint8_t length);
void I2C1_Writea(uint8_t addr, uint8_t data, uint8_t length);
void enableBacklight();
void sendCommand(uint8_t command);
void lcdInit();
void send4BitData(uint8_t data, uint8_t mode);
void sendByteToLCD(uint8_t byte, uint8_t mode);

#endif /* MAIN_H_ */
