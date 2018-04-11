#include <DHT.h>
#include <MFRC522.h>

//LEDs and control for AC heating/cooling control
#define COOL_AC_BLUE_LED 25
#define HEAT_AC_RED_LED 26

//debug serial port
#define BAUTRATE 9600

//DHT11 temperature and humidity sensor for greenhouse
#define DHT_PIN_GREENHOUSE 32
#define DHTTYPE DHT11
DHT dhtSensorGreenhouse(DHT_PIN_GREENHOUSE, DHTTYPE);

//DHT11 temperature and humidity sensor for equipment
#define DHT_PIN_EQUIPMENT 33
DHT dhtSensorEquipment(DHT_PIN_EQUIPMENT, DHTTYPE);

//RFID sensor
#define RST_PIN 22
#define SS_PIN 5
MFRC522 rfidSensor(SS_PIN, RST_PIN);

//global variables
boolean updateMeasurement = false;

void setup() {
//initial LEDs and control for AC heating/cooling control
pinMode(COOL_AC_BLUE_LED, OUTPUT);//configurate output
pinMode(HEAT_AC_RED_LED, OUTPUT);//configurate output

//initial debug serial port
Serial.begin(BAUTRATE);//initial debug serial connection

//initial DHT11 temperature and humidity sensor for greenhouse
dhtSensorGreenhouse.begin();//initial DHT11 Sensor

//initial DHT11 temperature and humidity sensor for equipment
dhtSensorEquipment.begin();//initial DHT11 Sensor

//initial RFID sensor
SPI.begin();//start SPI for RFID sensor
rfidSensor.PCD_Init();//initial RFID Sensor

//update timer interrupt (1s)
hw_timer_t * updateTimer = NULL;
updateTimer = timerBegin(0,80,true);
timerAttachInterrupt(updateTimer, &UpdateTimerInterrupt, true);
timerAlarmWrite(updateTimer,1000000, true);
timerAlarmEnable(updateTimer);


  
}

void loop() {

//Every 0,5 seconds update measurement
if (updateMeasurement == true){
  UpdateMeasurement();
}

//Check for RFID tag
CheckForRFIDCard();

}

void UpdateMeasurement() {
float tempratureDHTGreenhouse;
float humidityDHTGreenhouse;
float tempratureDHTEquipment;

//getting greenhouse values (temp,hum) 
tempratureDHTGreenhouse = dhtSensorGreenhouse.readTemperature();
humidityDHTGreenhouse = dhtSensorGreenhouse.readHumidity();

//getting greenhouse value (temp)
tempratureDHTEquipment = dhtSensorEquipment.readTemperature();

Serial.print("Greenhouse ");
Serial.print(tempratureDHTGreenhouse); 
Serial.print(" ");
Serial.print(humidityDHTGreenhouse);
Serial.print(" Equipment "); 
Serial.println(tempratureDHTEquipment);
updateMeasurement = false;
}

void CheckForRFIDCard(){
uint32_t rfidID;
String rfidIDString;
if (rfidSensor.PICC_IsNewCardPresent() && rfidSensor.PICC_ReadCardSerial() ) {
rfidID = (uint32_t) rfidSensor.uid.uidByte[0] << 24;
rfidID |= (uint32_t) rfidSensor.uid.uidByte[1] << 16;
rfidID |= (uint32_t) rfidSensor.uid.uidByte[2] << 8;
rfidID |= (uint32_t) rfidSensor.uid.uidByte[3]; 
rfidSensor.PICC_HaltA();
rfidIDString = String(rfidID, DEC);
Serial.println(rfidIDString);
 } 
}

void UpdateTimerInterrupt(){
updateMeasurement = true;
}
