/*  
   bmp085.c - User space bmp085 sensor header data

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/

#include "bmp085.h"

static struct bmp085_data bmp085Data;

/*Open bmp085 device in i2c-1*/
int bmp085_open()
{

  //printf("Open device file\n");
  if((bmp085Data.fd = open(FILE_PATH, O_RDWR)) < 0)
  {
   printf("Cann't open file\n");
   exit(1);//return -1;
  }  
  
  if(ioctl(bmp085Data.fd, I2C_SLAVE, BMP085_ADDR) < 0)
  {
   printf("ioctl function failed\n");
   close(bmp085Data.fd);
   exit(1);//return -1;
  }
  return 1;
}

/*smpbus functions*/
__s32 bmp085_i2c_read_long(__u8 addr)
{
 __s32 reg = i2c_smbus_read_word_data(bmp085Data.fd, addr);
 if(reg < 0)
 { 
  printf("smbus read word failed: %x", addr);
  exit(1);//return -1;
 }
 //printf("reg %x\n",reg);
 //printf("**********reg shift %x, %x\n",((reg << 8) & 0xff00),((reg >> 8) & 0xff )); 
 reg = (((reg << 8) & 0xff00) | ((reg >> 8) & 0xff )) ;
 //printf("reg shift %x, %x\n",reg); 
 return reg;
} 

/*Calibration of Sensor device*/
void bmp085Calibration()
{
  
  bmp085Data.ccParameter.ac1 = bmp085_i2c_read_long(0xAA);
  bmp085Data.ccParameter.ac2 = bmp085_i2c_read_long(0xAC);
  bmp085Data.ccParameter.ac3 = bmp085_i2c_read_long(0xAE);
  bmp085Data.ccParameter.ac4 = bmp085_i2c_read_long(0xB0);
  bmp085Data.ccParameter.ac5 = bmp085_i2c_read_long(0xB2);
  bmp085Data.ccParameter.ac6 = bmp085_i2c_read_long(0xB4);
  bmp085Data.ccParameter.b1 = bmp085_i2c_read_long(0xB6);
  bmp085Data.ccParameter.b2 = bmp085_i2c_read_long(0xB8);
  bmp085Data.ccParameter.mb = bmp085_i2c_read_long(0xBA);
  bmp085Data.ccParameter.mc = bmp085_i2c_read_long(0xBC);
  bmp085Data.ccParameter.md = bmp085_i2c_read_long(0xBE);

}

/*Tempature Calculation */
void bmp085_RealTemperature()
{
 int x1, x2;

 /*write temp value to control reg*/
 i2c_smbus_write_word_data(bmp085Data.fd, BMP085_CONTROL_REG, BMP085_TEMP_VALUE);
 /*Wait for conversion*/
 usleep(5000);
 bmp085Data.ut=bmp085_i2c_read_long(0xF6);

 x1 = (((int)bmp085Data.ut - (int) bmp085Data.ccParameter.ac6) *(int) bmp085Data.ccParameter.ac5) >> 15;
 x2 = (((int)bmp085Data.ccParameter.mc << 11)/(x1+bmp085Data.ccParameter.md));
 bmp085Data.ccParameter.b5 = x1+x2;
 bmp085Data.temperature = (unsigned int)(bmp085Data.ccParameter.b5+8) >> 4;  
 printf("Temperature\t%0.1f%cC\n", ((double)bmp085Data.temperature)/10,0x00B0);
}


/*Presure Calculation*/
void bmp085_RealPressure()
{
  int b3,b6,x1,x2,x3,p;
  unsigned int b4,b7;
  __u8 buf[3];

  i2c_smbus_write_word_data(bmp085Data.fd, BMP085_CONTROL_REG,ctrlReg[BMP085_OSS].usCtrRegValue+(BMP085_OSS << 6));
  usleep(ctrlReg[BMP085_OSS].maxConvTime*1000);
  
  //bmp085Data.up = (bmp085_i2c_read_long(0xF6)+bmp085_i2c_read_long(0xF8)) >> (8-BMP085_OSS);
  if (i2c_smbus_read_i2c_block_data(bmp085Data.fd, 0xF6, 3, buf) < 0)
    printf("Block data failed %x\n", 0xF6);
  
  bmp085Data.up =(((unsigned int)buf[0] << 16) + ((unsigned int)buf[1] << 8) + (unsigned int)buf[2]) >> (8-BMP085_OSS);
  
  
  b6 = bmp085Data.ccParameter.b5 - 4000;
  x1 = (bmp085Data.ccParameter.b2*((b6*b6)>>12)) >> 11;
  x2 = (bmp085Data.ccParameter.ac2*b6) >> 11;
  x3 = x1 + x2;
  b3 =(((bmp085Data.ccParameter.ac1*4+x3) << BMP085_OSS) + 2) >> 2; 
  x1 = (bmp085Data.ccParameter.ac3*b6) >> 13;
  x2 = (bmp085Data.ccParameter.b1*((b6*b6)>>12))>>16;
  x3 = ((x1+x2)+2)>>2;
  b4 = (bmp085Data.ccParameter.ac4*(unsigned long)(x3+32768))>>15;
  b7 = ((unsigned long)bmp085Data.up-b3)*(50000 >> BMP085_OSS);
  if(b7 < 0x80000000)
   p = (b7<<1)/b4;
  else
   p = (b7/b4)<<1;
  x1 = (p >> 8)*(p >> 8);
  x1 = (x1*3038) >> 16;
  x2 = (-7357*p) >> 16;
  bmp085Data.pressure = p = p+((x1+x2+3791) >> 4);
  
  printf("Pressure\t%0.2fhPa\n", ((double) bmp085Data.pressure)/100);
  printf("Altitude %0.2fm\n", (double)(((bmp085Data.pressure)/100)*8.43)); 
}

int main(int argc, char *argv[])
{

 bmp085_open();
 bmp085Calibration(); 
 bmp085_RealTemperature();
 bmp085_RealPressure();
 return 0;

}

