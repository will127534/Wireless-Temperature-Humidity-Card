#include <NRF24.h>
#include <SPI.h>

#include <msp430fr4133.h>
#include <string.h>
#include "Energia.h"
#include "sensor.h"
#include "I2C_SoftwareLibrary.h"


NRF24 nrf24(P5_5, P5_0);

struct data
{
  float temperature;
  long pressure;
  float humidity;
  uint32_t lux;
  uint16_t voltage;
};
data senddata;


const unsigned char OSS = 0;  // Oversampling Setting
// Calibration values
int ac1;
int ac2;
int ac3;
unsigned int ac4;
unsigned int ac5;
unsigned int ac6;
int b1;
int b2;
int mb;
int mc;
int md;

// b5 is calculated in bmp085GetTemperature(...), this variable is also used in bmp085GetPressure(...)
// so ...Temperature(...) must be called before ...Pressure(...).
long b5; 

SoftwareWire SWire(SDA_PIN, SCL_PIN);

uint16_t _reading;


const uint8_t digit[10] =
{
  0xEB,  0x60,  0xC7,  0xE5,  0x6C,  0xAD,  0xAF,  0xE8,  0xEF,  0xEC   
};

double t ;
double h ;
uint16_t readingT=0;
uint16_t readingH = 0;

void setup(void) {

  Serial.begin(9600);
  Serial.println("START");
  setLCD();
  pinMode(P1_4,OUTPUT);
  Serial.print("I2C begin... ");
  SWire.begin();

  bmp085Calibration();
    //hdc1000_init();
//    sht21_init();
  opt3001begin();

  nrf24.init();
  nrf24.setChannel(5);
    //Serial.println("setChannel failed");
  nrf24.setThisAddress((uint8_t*)"clie1", 5);

  nrf24.setPayloadSize(sizeof(data));
  nrf24.setRF(NRF24::NRF24DataRate2Mbps, NRF24::NRF24TransmitPowerm6dBm);
  nrf24.setTransmitAddress((uint8_t*)"serv2", 5);
  nrf24.powerDown();
    for(int i=0;i<10;i++){
     writedigit((int)(i*100+i*10+i),1,1);
     writedigit((int)(i*100+i*10+i),0,1);
     sleep(100);
   }

 }
 int count=0;
 void loop(void)
 {
  //hdc1000_start();
  sht21_start_temp();
  if(count>=60){
    bmp180_start();
  }
  //sleep_16ms();
  sleep_1000ms();
  sht21_get_temp();
  sht21_start_humid();
  //sleep_16ms();
  sleep_1000ms();
  sht21_get_humid();
  //hdc1000Get();
  

 // digitalWrite(P1_4, HIGH);

 // digitalWrite(P1_4, LOW);
 
 writedigit((int)(h*10),0,1);
 writedigit((int)(t*10),1,1);
 Serial.print("T: ");Serial.println(t);
 Serial.print("H: ");Serial.println(h);
  /*
  Serial.print("V: ");Serial.println(senddata.voltage);
  Serial.print("P: ");Serial.println(senddata.pressure);
  Serial.print("L: ");Serial.println(senddata.lux);
  */
  //PMMCTL0_H = PMMPW_H;                                       // Open PMM Registers for write
	//PMMCTL0_L |= PMMREGOFF_L;                                  // and set PMMREGOFF
	//__bis_SR_register(LPM3_bits);                              // Enter LPM3.5


  if(count>=60){
    senddata.temperature = t;
    senddata.humidity = h;
    senddata.voltage = analogRead(INTERNAL1V5ADC);
    float temperature = bmp085GetTemperature(bmp085ReadUT()); //MUST be called first
    senddata.pressure = bmp085GetPressure(bmp085ReadUP());
    senddata.lux = opt3001readResult();
    
    nrf24.send((uint8_t*)&senddata, sizeof(senddata));
    nrf24.waitPacketSent();
    nrf24.powerDown();
    
    count=0;
  }
  count++;
  sleep_1000ms();
 //  // Should report IDLE
}

void setLCD(){
    // put your setup code here, to run once:
    LCDCTL0 &= ~LCDON;
    // pin assignment for FR4133 - if more Launchpads gets LCDs added this may should go to the energia_pins.h file
    LCDPCTL0 |= 0xFFFF;
    LCDPCTL1 |= 0x001F;

    LCDCSSEL0 = 0x000F;

    LCDCTL0 = (LCDMX0 | LCDMX1 | LCDSSEL_0
     | LCDLP | LCDSON | LCDDIV_2);

    // LCD Operation - Mode 3, internal 3.02v, charge pump 256Hz
    LCDVCTL = (LCDREFEN| LCDCPEN | VLCD_6 | 
     LCDCPFSEL3 | LCDCPFSEL2 | LCDCPFSEL1 | LCDCPFSEL0);

    // Clear LCD memory
    LCDMEMCTL |= LCDCLRM | LCDCLRBM;

    // Configure COMs and SEGs
    // L0 = COM0, L1 = COM1, L2 = COM2, L3 = COM3
    //LCD_E_setPinAsCOM( LCD_E_BASE, LCD_E_SEGMENT_LINE_0,   );
    //LCD_E_setPinAsCOM( LCD_E_BASE, LCD_E_SEGMENT_LINE_1, LCD_E_MEMORY_COM1 );
    //LCD_E_setPinAsCOM( LCD_E_BASE, LCD_E_SEGMENT_LINE_2, LCD_E_MEMORY_COM2 );
    //LCD_E_setPinAsCOM( LCD_E_BASE, LCD_E_SEGMENT_LINE_3, LCD_E_MEMORY_COM3 );
    LCDM0W = 0x1248;
    LCDBM0W = 0x1248;

    // Select to display main LCD memory
    LCDMEMCTL &= ~LCDDISP;
    //LCDMEMCTL |= 0;

    // Turn blinking features off
    LCDBLKCTL &= ~(LCDBLKPRE2 | LCDBLKPRE1 | LCDBLKPRE0 | LCDBLKMOD_3);
    LCDBLKCTL |= (LCDBLKPRE2 | LCDBLKMOD_0);

    //Turn LCD on
    LCDCTL0 |= LCDON;
  }

  void writedigit(int i,byte pos,bool dp){
    switch (pos) {
      case 0:
      i=i%1000;
      LCDMEM[3] =  digit[i/100];
      i=i%100;
      LCDMEM[4] =  digit[i/10];
      i=i%10;
      LCDMEM[5] =  digit[i];
      if(dp){
        LCDMEM[4] |= 0x10;
      }
      break;
      case 1:

            LCDMEM[6] = 0x09; //symbo
            LCDMEM[7] = 0;
            LCDMEM[8] = 0;
            LCDMEM[9] = 0;
            i=i%1000;
            LCDMEM[8] |=  digit[i/100] & 0xF0;
            LCDMEM[9] |=  digit[i/100] & 0x0F;
            i=i%100;
            LCDMEM[7] |=  digit[i/10] & 0xF0;
            LCDMEM[8] |=  digit[i/10] & 0x0F;
            i=i%10;
            LCDMEM[6] |=  digit[i] & 0xF0;
            LCDMEM[7] |=  digit[i] & 0x0F;
            if(dp){
              LCDMEM[7] |= 0x10;
            }
            break;
          }
        }
//SHT21
void sht21_init(){
  uint8_t config;
  //config = 0xC1; //RH 11bit,T 11bit
  SWire.beginTransmission(HDC1080_ADDRESS);
  SWire.write(0xE6);              //accessing the configuration register
  SWire.write(0xC1);                  //sending the config bits (MSB)
  SWire.endTransmission();
}

void sht21_start_temp(){
  SWire.beginTransmission(HDC1080_ADDRESS);
  SWire.write(0xF3);
  SWire.endTransmission();
}
void sht21_start_humid(){
  SWire.beginTransmission(HDC1080_ADDRESS);
  SWire.write(0xF5);
  SWire.endTransmission();
}

void sht21_get_temp(){
 SWire.requestFrom(HDC1080_ADDRESS, 3);
 while(!SWire.available());
 uint16_t reading=0;
 reading = (SWire.read()<<8);
 reading+= SWire.read();
 Serial.println(reading);
 t = (-46.85 + 175.72 / 65536.0 * (float)(reading));
}

void sht21_get_humid(){
 SWire.requestFrom(HDC1080_ADDRESS, 3);
 uint16_t reading=0;
 while(!SWire.available());
 reading = (SWire.read()<<8);
 reading+= SWire.read();
 Serial.println(reading);
 h = -6.0 + 125.0 / 65536.0 * (float)(reading);
}

//HDC1080
void hdc1000_init(){
  uint8_t config;
  config = HDC1000_RST|HDC1000_BOTH_TEMP_HUMI|HDC1000_HUMI_8BIT|HDC1000_HEAT_OFF;
  SWire.beginTransmission(HDC1080_ADDRESS);
  SWire.write(HDC1000_CONFIG);              //accessing the configuration register
  SWire.write(config);                  //sending the config bits (MSB)
  SWire.write(0x00);                    //the last 8 bits must be zeroes
  SWire.endTransmission();
}
void hdc1000_start(){
  SWire.beginTransmission(HDC1080_ADDRESS);
  SWire.write(0x00);
  SWire.write(0x00);
  SWire.endTransmission();
}
void hdc1000Get(){

  SWire.requestFrom(HDC1080_ADDRESS, 4);
  if(4 <= SWire.available())
  {
   readingT=0;
   readingH = 0;
   readingT = (SWire.read()<<8);
   readingT+= SWire.read();
   readingH = (SWire.read()<<8);
   readingH+= SWire.read();
 }
 t = readingT/65536.0*165.0-40.0;
 h = readingH/65536.0*100.0;
}


//BMP180
void bmp180_start(){
  // Write 0x2E into Register 0xF4
  // This requests a temperature reading
  SWire.beginTransmission(BMP085_ADDRESS);
  SWire.write(0xF4);
  SWire.write(0x2E);
  SWire.endTransmission();
}
void bmp085Calibration(){
  ac1 = bmp085ReadInt(0xAA);
  ac2 = bmp085ReadInt(0xAC);
  ac3 = bmp085ReadInt(0xAE);
  ac4 = bmp085ReadInt(0xB0);
  ac5 = bmp085ReadInt(0xB2);
  ac6 = bmp085ReadInt(0xB4);
  b1 = bmp085ReadInt(0xB6);
  b2 = bmp085ReadInt(0xB8);
  mb = bmp085ReadInt(0xBA);
  mc = bmp085ReadInt(0xBC);
  md = bmp085ReadInt(0xBE);
}

float bmp085GetTemperature(unsigned int ut){
  long x1, x2;

  x1 = (((long)ut - (long)ac6)*(long)ac5) >> 15;
  x2 = ((long)mc << 11)/(x1 + md);
  b5 = x1 + x2;

  float temp = ((b5 + 8)>>4);
  temp = temp /10;

  return temp;
}

long bmp085GetPressure(unsigned long up){
  long x1, x2, x3, b3, b6, p;
  unsigned long b4, b7;

  b6 = b5 - 4000;
  // Calculate B3
  x1 = (b2 * (b6 * b6)>>12)>>11;
  x2 = (ac2 * b6)>>11;
  x3 = x1 + x2;
  b3 = (((((long)ac1)*4 + x3)<<OSS) + 2)>>2;

  // Calculate B4
  x1 = (ac3 * b6)>>13;
  x2 = (b1 * ((b6 * b6)>>12))>>16;
  x3 = ((x1 + x2) + 2)>>2;
  b4 = (ac4 * (unsigned long)(x3 + 32768))>>15;

  b7 = ((unsigned long)(up - b3) * (50000>>OSS));
  if (b7 < 0x80000000)
  p = (b7<<1)/b4;
  else
  p = (b7/b4)<<1;

  x1 = (p>>8) * (p>>8);
  x1 = (x1 * 3038)>>16;
  x2 = (-7357 * p)>>16;
  p += (x1 + x2 + 3791)>>4;

  long temp = p;
  return temp;
}

char bmp085Read(unsigned char address){
  unsigned char data;

  SWire.beginTransmission(BMP085_ADDRESS);
  SWire.write(address);
  SWire.endTransmission();

  SWire.requestFrom(BMP085_ADDRESS, 1);
  while(!SWire.available())
  ;

  return SWire.read();
}

int bmp085ReadInt(unsigned char address){
  unsigned char msb, lsb;

  SWire.beginTransmission(BMP085_ADDRESS);
  SWire.write(address);
  SWire.endTransmission();

  SWire.requestFrom(BMP085_ADDRESS, 2);
  while(SWire.available()<2)
  ;
  msb = SWire.read();
  lsb = SWire.read();

  return (int) msb<<8 | lsb;
}
unsigned int bmp085ReadUT(){
  unsigned int ut;
  // Read two bytes from registers 0xF6 and 0xF7
  ut = bmp085ReadInt(0xF6);
  return ut;
}

unsigned long bmp085ReadUP(){

  unsigned char msb, lsb, xlsb;
  unsigned long up = 0;

  // Write 0x34+(OSS<<6) into register 0xF4
  // Request a pressure reading w/ oversampling setting
  SWire.beginTransmission(BMP085_ADDRESS);
  SWire.write(0xF4);
  SWire.write(0x34 + (OSS<<6));
  SWire.endTransmission();

  // Wait for conversion, delay time dependent on OSS
  delay(2 + (3<<OSS));

  // Read register 0xF6 (MSB), 0xF7 (LSB), and 0xF8 (XLSB)
  msb = bmp085Read(0xF6);
  lsb = bmp085Read(0xF7);
  xlsb = bmp085Read(0xF8);

  up = (((unsigned long) msb << 16) | ((unsigned long) lsb << 8) | (unsigned long) xlsb) >> (8-OSS);

  return up;
}


void writeRegister(int deviceAddress, byte address, byte val) {
  SWire.beginTransmission(deviceAddress); // start transmission to device 
  SWire.write(address);       // send register address
  SWire.write(val);         // send value to write
  SWire.endTransmission();     // end transmission
}

int readRegister(int deviceAddress, byte address){

  int v;
  SWire.beginTransmission(deviceAddress);
  SWire.write(address); // register to read
  SWire.endTransmission();

  SWire.requestFrom(deviceAddress, 1); // read a byte

  while(!SWire.available()) {
    // waiting
  }

  v = SWire.read();
  return v;
}


//OPT3002
void opt3001begin(){
	uint16_t writeByte = DEFAULT_CONFIG_100;
	// Initialize Wire

	// Use I2C module 1 (I2C1SDA & I2C1SCL per BoosterPack standard) and begin
	
	SWire.begin();
	
	
  /* Begin Tranmission at address of device on bus */
  SWire.beginTransmission(OPT3001_ADDR);
  /* Send Pointer Register Byte */

  SWire.write(CONFIG_REG);

  /* Read*/
  SWire.write((unsigned char)(writeByte>>8));
  SWire.write((unsigned char)(writeByte&0x00FF));

  /* Sends Stop */
	//Serial.println("before ending transmission");
	SWire.endTransmission();
	//Serial.println("return to life");
	return;
}

uint16_t readRegister(uint8_t registerName){
	int8_t lsb;
	int8_t msb;
	int16_t result;


	// Initialize Wire
	SWire.begin();

	/* Begin Transmission at address of device on bus */
	SWire.beginTransmission(OPT3001_ADDR);

	/* Send Pointer to register you want to read */
	SWire.write(registerName);

	/* Sends Stop */
	SWire.endTransmission();

	/* Requests 2 bytes from Slave */
	SWire.requestFrom(OPT3001_ADDR, 2);

	/* Wait Until 2 Bytes are Ready*/
	while(SWire.available() < 2)	{}

	/* Read*/
	msb = SWire.read();
	lsb = SWire.read();
	result = (msb << 8) | lsb;

	return result;
}
uint32_t opt3001readResult(){
	uint16_t exponent = 0;
	uint32_t result = 0;
	int16_t raw;
	raw = readRegister(RESULT_REG);
	
	/*Convert to LUX*/
	//extract result & exponent data from raw readings
	result = raw&0x0FFF;
	exponent = (raw>>12)&0x000F;

	//convert raw readings to LUX
	switch(exponent){
		case 0: //*0.015625
   result = result>>6;
   break;
		case 1: //*0.03125
   result = result>>5;
   break;
		case 2: //*0.0625
   result = result>>4;
   break;
		case 3: //*0.125
   result = result>>3;
   break;
		case 4: //*0.25
   result = result>>2;
   break;
		case 5: //*0.5
   result = result>>1;
   break;
   case 6:
   result = result;
   break;
		case 7: //*2
   result = result<<1;
   break;
		case 8: //*4
   result = result<<2;
   break;
		case 9: //*8
   result = result<<3;
   break;
		case 10: //*16
   result = result<<4;
   break;
		case 11: //*32
   result = result<<5;
   break;
 }

 return result;
}

