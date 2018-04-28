#include <DHT.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <PubSubClient.h>

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

//Wifi connection and mqtt server adress
const char* ssid = "WlanWastman";
const char* password = "kreuzer1908";
const char* mqtt_server = "192.168.178.233";
WiFiClient espClient;
PubSubClient client(espClient);

//Define mqtt topics
#define STATUS_AC_TOPIC    "AcControl/Status"
#define MODE_TOPIC    "AcControl/Mode"

//global variables
#define AUTO 1
#define HEAT 2
#define COOL 3
#define OFF 4
boolean updateMeasurement = false;
char msg[20];
byte modeAcControl = AUTO;

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

//connecting with wifi
wificonnect();

//connecting with mqtt
client.setServer(mqtt_server, 1883);//MQTT server with IPaddress,port
client.setCallback(receivedCallback);//receivedCallback function invoked client received subscribed topic
mqttconnect();
  
}

void loop() {
//reconnect wifi if not connected
if (WiFi.status() != WL_CONNECTED){
  wificonnect();
}

//reconnect mqtt if not connected
if (!client.connected()) {
  mqttconnect();
}

//Every 0,5 seconds update measurement
if (updateMeasurement == true){
  UpdateMeasurement();
}

//Check for RFID tag
CheckForRFIDCard();

//Check for mqtt msg
client.loop();
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

if (isnan(tempratureDHTGreenhouse) || isnan(humidityDHTGreenhouse) || isnan(tempratureDHTEquipment)){
Serial.println("Sensor Error");
client.publish("AcControl/Status/Sensor", "ERROR");
} else {
client.publish("AcControl/Status/Sensor", "ONLINE");

//Sending mqtt
snprintf (msg, 20, "%lf", tempratureDHTGreenhouse);
client.publish("AcControl/Status/TempratureGreenhouse", msg);

snprintf (msg, 20, "%lf", humidityDHTGreenhouse);
client.publish("AcControl/Status/HumidityGreenhouse", msg);

snprintf (msg, 20, "%lf", tempratureDHTEquipment);
client.publish("AcControl/Status/TempratureEquipmentroom", msg);

Serial.print("Greenhouse ");
Serial.print(tempratureDHTGreenhouse); 
Serial.print(" ");
Serial.print(humidityDHTGreenhouse);
Serial.print(" Equipment "); 
Serial.print(tempratureDHTEquipment);

switch (modeAcControl){ 
  case AUTO:
    Serial.print(" Mode:Auto ");
    client.publish("AcControl/Status/Mode", "AUTO");
    if ( tempratureDHTGreenhouse < tempratureDHTEquipment-2){
      digitalWrite(COOL_AC_BLUE_LED, HIGH);
      digitalWrite(HEAT_AC_RED_LED, LOW);
      client.publish("AcControl/Status/Heat", "OFF");
      client.publish("AcControl/Status/Cool", "ON");
      Serial.println("Heat:OFF Cool:ON");
  } else if ( tempratureDHTGreenhouse > tempratureDHTEquipment+2){
      digitalWrite(HEAT_AC_RED_LED, HIGH);
      digitalWrite(COOL_AC_BLUE_LED, LOW);
      client.publish("AcControl/Status/Heat", "ON");
      client.publish("AcControl/Status/Cool", "OFF");
      Serial.println("Heat:ON Cool:OFF");
  } else {
      digitalWrite(HEAT_AC_RED_LED, LOW);
      digitalWrite(COOL_AC_BLUE_LED, LOW);
      client.publish("AcControl/Status/Heat", "OFF");
      client.publish("AcControl/Status/Cool", "OFF");
      Serial.println("Heat:OFF Cool:OFF");
   }
  break;
  case HEAT:
    Serial.print(" Mode:Heat ");
    client.publish("AcControl/Status/Mode", "HEAT");
    digitalWrite(HEAT_AC_RED_LED, HIGH);
    digitalWrite(COOL_AC_BLUE_LED, LOW);
    client.publish("AcControl/Status/Heat", "ON");
    client.publish("AcControl/Status/Cool", "OFF");
    Serial.println("Heat:ON Cool:OFF");
  break;
  case COOL:
    Serial.print(" Mode:Cool ");
    client.publish("AcControl/Status/Mode", "COOL");
    digitalWrite(COOL_AC_BLUE_LED, HIGH);
    digitalWrite(HEAT_AC_RED_LED, LOW);
    client.publish("AcControl/Status/Heat", "OFF");
    client.publish("AcControl/Status/Cool", "ON");
    Serial.println("Heat:OFF Cool:ON");
  break;
  case OFF:
    Serial.print(" Mode:Off ");
    client.publish("AcControl/Status/Mode", "OFF");
    digitalWrite(COOL_AC_BLUE_LED, LOW);
    digitalWrite(HEAT_AC_RED_LED, LOW);
    client.publish("AcControl/Status/Heat", "OFF");
    client.publish("AcControl/Status/Cool", "OFF");
    Serial.println("Heat:OFF Cool:OFF");
  break;
}
}
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
Serial.print("RFID scanned: ");
Serial.println(rfidIDString);
rfidIDString.toCharArray(msg, rfidIDString.length()+1); 
client.publish("AcControl/Status/RFID", msg);
 } 
}

//receiving mqtt msg
void receivedCallback(char* topic, byte* payload, unsigned int length) {
  String recMsg = "";
  Serial.print("Message received: ");
  Serial.println(topic);

//msg receiving
  for (int i = 0; i < length; i++) {
    recMsg = recMsg + (char)payload[i];
  }

if ((char)payload[0] == '1'){
modeAcControl=AUTO;
Serial.print("Mode: AUTO");
}else if ((char)payload[0] == '2'){
modeAcControl=HEAT;
Serial.print("Mode: HEAT");
}else if ((char)payload[0] == '3'){
modeAcControl=COOL;
Serial.print("Mode: COOL");
}else if ((char)payload[0] == '4'){
modeAcControl=OFF;
Serial.print("Mode: OFF");
}else{
Serial.print("this is no valid command: ");
Serial.print(recMsg);
}
  Serial.println();
} 

void wificonnect() {
Serial.print("Connecting to ");
Serial.println(ssid);
WiFi.begin(ssid, password);
while (WiFi.status() != WL_CONNECTED) {
  delay(500);
  Serial.print(".");
}
Serial.println("");
Serial.println("WiFi connected");
Serial.println("IP address: ");
Serial.println(WiFi.localIP());  
}

void mqttconnect() {
  while (!client.connected()) {
    Serial.print("MQTT connecting ...");
    String clientId = "AcControl";
    //connect now
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe(MODE_TOPIC);
    } else {
      Serial.print("failed, status code =");
      Serial.print(client.state());
      Serial.println("try again in 3 seconds");
      delay(3000);
    }
  }
}

void UpdateTimerInterrupt(){
updateMeasurement = true;
}
