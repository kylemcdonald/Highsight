// write motor id to EEPROM address 0 so don't need to edit code before uploading to each motor driver
// confirm by reading back and dumping to serial
 

#include <EEPROM.h>

int addr = 0;
int MYSERIALNUMBER = 0;

void setup()
{
  Serial.begin(9600);
  EEPROM.write(addr, MYSERIALNUMBER);
  
  int confirm = EEPROM.read(addr);
  
  Serial.print("Tried to write ");
  Serial.println(MYSERIALNUMBER);
  Serial.print("Read back ");
  Serial.println(confirm);
}

void loop()
{
 
}
