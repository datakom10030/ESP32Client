
#include "testprosjekt.h"

#include <WiFi.h>//Imports the needed WiFi libraries
#include <WiFiMulti.h> //We need a second one for the ESP32 (these are included when you have the ESP32 libraries)

#include <SocketIoClient.h> //Import the Socket.io library, this also imports all the websockets

int tempPin = A0;  //This is the Arduino Pin that will read the sensor output
int sensorInput;    //The variable we will use to store the sensor input
double temp;        //The variable we will use to store temperature in degrees.

void setup() {
    // put your setup code here, to run once:


}
void loop() {
    // put your main code here, to run repeatedly:
    sensorInput = analogRead(tempPin);    //read the analog sensor and store it
    temp = (double)sensorInput / 1024;       //find percentage of input reading
    temp = temp * 5;                 //multiply by 5V to get voltage
    temp = temp - 0.5;               //Subtract the offset
    temp = temp * 100;               //Convert to degrees
    Serial.print("Current Temperature: ");
    Serial.println(temp);
    socket.e
}