//LEDs and control for AC heating/cooling control
#define COOL_AC_BLUE_LED 25
#define HEAT_AC_RED_LED 26

//debug serial port
#define BAUTRATE 9600

void setup() {
//initial LEDs and control for AC heating/cooling control
pinMode(COOL_AC_BLUE_LED, OUTPUT);//configurate output
pinMode(HEAT_AC_RED_LED, OUTPUT);//configurate output

//initial debug serial port
Serial.begin(BAUTRATE);//initial debug serial connection
  
}

void loop() {
  // put your main code here, to run repeatedly:

}
