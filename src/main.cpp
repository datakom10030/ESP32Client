/***********************************************************************************************************************
 * ROBOT CLIENT
 * THIS IS A PROGRAM FOR A ROBOT CONTROLLING HEATING AND AIR QUALITY OF A ROOM
 * WRITTEN AS A PART OF THE SUBJECT IELEA2001
 ***********************************************************************************************************************/
#include <WiFi.h>
#include <SocketIoClient.h>
#include <ArduinoJson.h>

/// Access-point Settings ///
const char* SSID     = "Example-network-SSID";       // Name of access-point
const char* PASSWORD = "password";                   // Password for access-point

/// Socket.IO Settings ///
char HOST[] = "192.168.137.105";                     // Socket.IO Server Address
int PORT = 3000;                                     // Socket.IO Port Address
char PATH[] = "/socket.io/?transport=websocket";     // Socket.IO Base Path
String SERVER_PASSWORD = "\"123456789\"";            // Password sent to server for authentication

StaticJsonBuffer<200> jsonBuffer;                    // Buffer storage for JSON values


/// Internal temperature sensor initializing ///
#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif
uint8_t temprature_sens_read();


/// Pin Settings ///
int TEMP_INPUT_PIN = 35;
int CO2_INPUT_PIN = 34;

int HEATER_OUTPUT_PIN = 4;
int VENTILATION_OUTPUT_PIN = 5;


/// Variables for algorithms, variables are grouped according to the same function but for different regulation pairs ///
// The setpoints that the server emits to the robot
float temperatureSetpoint;
float co2Setpoint;

// The most recent value of the senors
float tempValue;
float internalTempValue;
float co2Value;

// The sensor values from the last iteration
float previousTempValue;
float previousInternalTempValue;
float previousCo2Value;

// Bool that is evaluated true if server validated the robot
bool authenticatedByServer = false;

// Determines what kind of regulation method should be used
bool tempActuatorReversed = false;
bool co2ActuatorReversed = true;

// The output states from the last iteration
bool previousTempOutputState;
bool previousCo2OutputState;

// If true determines that the sensor should only be used for surveillance and no regulation
bool surveillanceModeTemp;
bool surveillanceModeCo2;

// Stores the value of the time for the next timeout
unsigned long nextTimeout = 0;

// Parameter that specifies how long before each timeout
int timeout = 5000;

// System identification and keys for JSON communication parameters
const String ROBOT_ID = "\"001\"";
const String TEMP_SENSOR_KEY = "001";
const String CO2_SENSOR_KEY = "002";
const String INTERNAL_TEMP_SENSOR_KEY = "003";

// Instances for communication and wifi.
SocketIoClient webSocket;
WiFiClient client;

/**
 * Function that is called when the ESP32 creates a connection with the server. On connection
 * some sentences is printed to the console. Then emits a password to the server for authentication.
 * @param payload     contains the data sent from server with event "connect"
 * @param length      gives the size of the payload
 */
void socketConnected(const char * payload, size_t length) {
    // Prints information to the console for user information
    Serial.println("Socket.IO Connected!");
    Serial.println("Sending PASSWORD to server for authentication");

    // Sending the password for the robot to the server to get authenticated
    webSocket.emit("authentication", SERVER_PASSWORD.c_str());
}

/**
 * Function that is called if connection with the server is lost. And sets variable that describes the ESP32 is not
 * authenticated.
 * @param payload     contains the data sent from server with event "disconnect"
 * @param length      gives the size of the payload
 */
void socketDisconnected(const char * payload, size_t length) {
    Serial.println("Socket.IO Disconnected!");
    authenticatedByServer = false;
}

/**
 * Function that handles the server response on the password sent for authentication. The function
 * saves the payload to a datatype that can be compared in an if statement. The if statements checks if
 * the server emitted true / false, or something invalid. If the response ia false or invalid, prints text
 * describing the error to console. If the server gives approved feedback a new emit function is called with
 * the robots ID, and sets variable that describes the ESP32 is authenticated.
 * @param payload     contains the data sent from server with event "authentication"
 * @param length      gives the size of the payload
 */
void authenticateFeedback (const char * payload, size_t length) {
    // Changes datatype of feedback to a string
    String feedback = payload;

    if (feedback == "true") {
        Serial.println("Authentication successful!");
        // Sets the robot to authenticated
        authenticatedByServer = true;
        // Sends the robots ID to the robot-server so that a profile can be set up
        webSocket.emit("robotID", ROBOT_ID.c_str());
    } else if (feedback == "false") {
        Serial.println("Authentication unsuccessful, wrong PASSWORD");
    } else {
        Serial.println("Unrecognized feedback / corrupted payload");
    }
}

/**
 * Function that checks the value of each key that is received as set-points, has a float number or the value of none.
 * If the value is none, then surveillance-mode for that sensor and corresponding actuator is set. If a float for a
 * set-point is received, then that sensor and corresponding actuator is in normal regulation mode. The mode selected is
 * also printed to the console.
 * @param serverData     contains the server sensor keys and corresponding set-points sent from the server in JSON format
 */
void determineMode (JsonObject& serverData) {
    // Checks if the data associated with the key is equal to the string "none"
    if (serverData[TEMP_SENSOR_KEY] == "none") {
        surveillanceModeTemp = true;
        Serial.println("Surveillance mode for temp is activated");
        digitalWrite(HEATER_OUTPUT_PIN, LOW);
        previousTempOutputState = false;
    // If its not, it will be a numeric value and regulation should be started with this value
    } else {
        temperatureSetpoint = serverData[TEMP_SENSOR_KEY];
        surveillanceModeTemp = false;
        Serial.println("Normal regulation mode for temp is activated");
    }
    // Checks if the data associated with the key is equal to the string "none"
    if (serverData[CO2_SENSOR_KEY] == "none") {
        surveillanceModeCo2 = true;
        Serial.println("Surveillance mode for co2 is activated");
        digitalWrite(VENTILATION_OUTPUT_PIN, LOW);
        previousCo2OutputState = false;
    // If its not, it will be a numeric value and regulation should be started with this value
    } else {
        co2Setpoint = serverData[CO2_SENSOR_KEY];
        surveillanceModeCo2 = false;
        Serial.println("Normal regulation mode for co2 is activated");

    }
}

/**
 * Function that manipulated the payload data into a string that can be formatted with ArduinoJSON methods. Saves the
 * unique keys and corresponding set-points and saves it in JSON format. Error checking if the formatting worked, if
 * not prints a status message to console for troubleshooting. Calls the function determineMode with the JSON object
 * for determining the operation mode of the sensors and corresponding actuators.
 * @param payload     contains the data sent from server with event "setpoints"
 * @param length      gives the size of the payload
 */
void manageServerSetpoints(const char * payload, size_t length) {
    // Changes the incoming data in the string datatype
    String str_payload = payload;
    // Because the set-points is not in proper JSON format, modulates the string to JSON
    str_payload.replace("\\", "" );

    // Defines an array to store the data in JSON
    char setpoints_array[100];
    str_payload.toCharArray(setpoints_array, 100);

    // Stores the data as a JsonObject datatype
    JsonObject& server_data = jsonBuffer.parseObject(setpoints_array);

    // Control check if the JSON processing worked, if not prints error message to console
    if(!server_data.success()) {
        Serial.println("parseObject() from index.js set-points payload failed");
    }

    determineMode(server_data);

    // TODO - Remove troubleshooting console printing
    Serial.println(temperatureSetpoint);
    Serial.println(co2Setpoint);

}

/**
 * Function that depending on if is called with sensor type as temperature or co2, and what ESP32 pin number, reads
 * the analog value of the relevant pin and scales the value appropriately. Then returns the value as a float number to
 * the global variable that holds the sensor value for the relevant sensor.
 * @param sensorType     specifies what type of sensor is connected to the inputPin
 * @param inputPin       specifies the ESP32 pin that the analog value is read from
 * @return               The sensor value in proper physical units
 */
float readSensorValue (String sensorType, int inputPin) {
    // Defines local variables that are used to store the values read from ESP32 pins
    float raw_co2;
    float raw_temp;

    if (sensorType == "temperature") {
        raw_temp = analogRead(inputPin);
        // Returns the value read scaled to reflect proper temperature value
        return (raw_temp / 4095.0) * 70.0;
    } else if (sensorType == "co2") {
        raw_co2 = analogRead(inputPin);
        // Returns the value read scaled to reflect proper CO2 value
        return (raw_co2 / 4095.0) * 2000.0;
    } else {
        // If the sensor type is not defined according to a possible type
        // Returns a value that symbolizes an error
        return -1000.0;
    }
}

/**
 * Function that checks what kind of output actuator type is used for the regulation, and a logic statement that
 * checks depending on if the output actuator is reversed, if the current value of the sensor is lower / higher than
 * the set point. Returns either true or false, which respectively turns the output HIGH or LOW.
 * @param setPoint             the value of the set-point given from the server
 * @param currentValue         the most recent process value of the sensor
 * @param outputReversed       determines what kind of regulation is used for the actuator (direct / reverse control)
 * @return                     the bool value that is used to turn output HIGH or LOW
 */
bool checkSensor (float setPoint, float currentValue, bool outputReversed) {
    if (outputReversed) {
        if (setPoint < currentValue) {
            return true;
        } else {
            return false;
        }
    } else {
        if (setPoint < currentValue) {
            return false;
        } else {
            return true;
        }
    }
}

/**
 * Function that takes in a bool value for the output and the outputPin that should be controlled. Uses these values to
 * turn on or off the output. An input or true gives HIGH output, and input of false gives LOW output.
 * @param outputPin     specifies the output pin of the ESP32 that is used to control the actuator
 * @param output        the value of what the output should be in bool values
 */
void setOutput (int outputPin, bool output) {
    if (output) {
        digitalWrite(outputPin, HIGH);
    } else {
        digitalWrite(outputPin, LOW);
    }
}

/**
 * Function that takes in a value in milliseconds, and sets a time in the future based on that value.
 * @param timeout     specifies how long the timeouts should be
 */
void startTimer (int timeout) {
    nextTimeout = millis() + timeout;
}

/**
 * Function that checks if the current time is equal or has passed the timeout that was set
 * @return              value that returns true if timer has expired and false if not
 */
bool isTimerExpired () {
    return (millis() >= nextTimeout);
}

/**
 * Function that first checks what kind of data that should be sent, could be output states or sensor values, as this
 * function is used for all sending of data to server. Further formats the output data to JSON format, with sensor values
 * if the type of data is "sensorValues". Or sends the output state if type of data is "output". Then sends the values
 * with websockets using event "sensorData".
 * @param typeOfData       argument that determines if the data should be sent as output states or sensor values
 * @param idKey            name of the sensor / actuator ID
 * @param sensorValue      the current value of the sensor [only used when this function is used for sending sensor values]
 * @param outputState      the current value of the output [only used when this function is used for sending output states]
 */
void sendDataToServer(String typeOfData, String idKey, float sensorValue, bool outputState) {
    if (typeOfData == "output") {
        // Formats the outgoing data as a JSON string and sends it to robot-server
        String data = String("{\"ControlledItemID\":\"" + String(idKey) + "\",\"value\":" + String(outputState) + "}");
        webSocket.emit("sensorData", data.c_str());

    } else if (typeOfData == "sensorValues") {
        // Formats the outgoing data as a JSON string and sends it to robot-server
        String data = String("{\"SensorID\":\"" + String(idKey) + "\",\"value\":" + String(sensorValue) + "}");
        webSocket.emit("sensorData", data.c_str());

    } else {
        // Prints to console for error notification
        Serial.println("Invalid type of data");
    }

}

/**
 * Function that finds out what state the output should be in by calling the checkSensor functions with relevant
 * parameters. Depending on what kind of sensor / actuator pair of regulation is called with this function, goes into
 * the appropriate if statement condition. Then checks if the output state has changed since the last iteration, if the
 * output has changed the if statement is entered, and output is set. Further variables that holds the previous output
 * state is set again to that value, for future iterations to check against. Then this output state is sent to the server
 * using the function sendDataToServer.
 * @param typeOfData          argument that states the output actuator type
 * @param setpoint            the value of the set-point given from the server
 * @param currentValue        the most recent process value of the sensor
 * @param outputPin           specifies the output pin of the ESP32 that is used to control the actuator
 * @param outputReversed      determines what kind of regulation is used for the actuator (direct / reverse control)
 * @param idKey               name of the sensor / actuator ID
 */
void setOutputState (String typeOfData, float setpoint, float currentValue, int outputPin, bool outputReversed, String idKey) {
    // Calls function to check what state the output should be in and stores this value
    bool output = checkSensor(setpoint, currentValue, outputReversed);

    //  Checks what type of data the sensor and corresponding actuator is, and goes into correct instruction set
    if (typeOfData == "temperature") {
        // If the output has changed since last iteration, data is sent and output is changed to new state
        if (output != previousTempOutputState) {
            setOutput(outputPin, output);
            // Sets new previous output state to be used for next iteration of program
            previousTempOutputState = output;
            sendDataToServer("output", idKey, 0.0, output);
        }
    } else if (typeOfData == "co2") {
        // If the output has changed since last iteration, data is sent and output is changed to new state
        if (output != previousCo2OutputState) {
            setOutput(outputPin, output);
            // Sets new previous output state to be used for next iteration of program
            previousCo2OutputState = output;
            sendDataToServer("output", idKey, 0.0, output);
        }
    }
}

/**
 * Function that takes in a float number and rounds the number to a specified number of decimal places, which is
 * given in the second parameter, decimals.
 * @param input           input number in float that will be manipulated to a rounded decimal number
 * @param decimals        specifies how many decimal places the rounding algorithm will perform
 * @return                returns value in float that is the rounded value of input value
 */
float decimalRound(float input, int decimals) {
    float scale=pow(10,decimals);
    return round(input*scale)/scale;
}

/**
 * Function that first checks what kind of process value is being processed. Then finds the rounded value of the
 * process current value called with the function. Further uses this value to check if the process value has changed
 * enough to warrant a message sent with websockets to the server. The limits for how much the process value has to
 * change depends on what kind of type of sensor is called. When the data is sent, a new previous sensor value is set
 * for future iterations to check against.
 * @param typeOfData         argument that determines if the data should be sent as output states or sensor values
 * @param idKey              name of the sensor / actuator ID
 * @param currentValue       the most recent process value of the sensor
 */
void checkForSensorChange (String typeOfData, String idKey, float currentValue) {
    // Local variable that stores a local value of the sensor value
    float rounded_value;

    if (typeOfData == "temperature") {
        // Calls function to round the sensor value to one decimal point
        rounded_value = decimalRound(currentValue, 1);

        // Checks if the temperature has changed within the threshold values
        if ((previousTempValue - .2) > rounded_value or (previousTempValue + .2) < rounded_value) {
            // Sends the new value to server
            sendDataToServer("sensorValues", idKey, rounded_value, false);
            // Sets the new temperature value for text iteration comparison
            previousTempValue = rounded_value;
        }

    } else if (typeOfData == "co2") {
        // Calls function to round the sensor value to one decimal point
        rounded_value = decimalRound(currentValue, 1);

        // Checks if the CO2 level has changed within the threshold values
        if ((previousCo2Value - 1) > rounded_value or (previousCo2Value + 1) < rounded_value) {
            // Sends the new value to server
            sendDataToServer("sensorValues", idKey, rounded_value, false);
            // Sets the new CO2 value for text iteration comparison
            previousCo2Value = rounded_value;
        }
    } else if (typeOfData == "internal-temp") {
        // Reads the internal temperature and changes the value from fahrenheit to celsius
        internalTempValue = ((temprature_sens_read() - 32) / 1.8);
        // Rounds the internal temperature to a number with a one decimal place
        rounded_value = decimalRound(internalTempValue, 1);

        // Check if the internal temperature level has changed within the threshold values
        if ((previousInternalTempValue - .2) > rounded_value or (previousInternalTempValue + .2) < rounded_value) {
            // Sends the new value to server
            sendDataToServer("sensorValues", idKey, rounded_value, false);
            // Sets the new internal temperature value for text iteration comparison
            previousInternalTempValue = rounded_value;
        }
    }
}


void setup() {
    // Starts the serial communication between the editor and the surveillance of the robot
    Serial.begin(9600);
    delay(10);

    // Sets relevant outputs for the actuators
    pinMode(HEATER_OUTPUT_PIN, OUTPUT);
    pinMode(VENTILATION_OUTPUT_PIN, OUTPUT);

    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(SSID);

    WiFi.begin(SSID, PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());


    // Listen events for all websockets events from raspberryPiServer
    webSocket.on("connect", socketConnected);
    webSocket.on("disconnect", socketDisconnected);
    webSocket.on("authentication", authenticateFeedback);
    webSocket.on("setpoints", manageServerSetpoints);


    // Setup Connection with raspberryPiServer
    webSocket.begin(HOST, PORT, PATH);
}

void loop() {
    // Reads the value of the the sensor connected to associated pin and saves the value in a global variable
    tempValue = readSensorValue("temperature", TEMP_INPUT_PIN);
    co2Value = readSensorValue("co2", CO2_INPUT_PIN);

    // If the function over has returned -1000, this statements knows that an error has occurred and prints message
    if (tempValue == -1000.0 or co2Value == -1000.0) {
        Serial.println("Invalid sensor type argument given to readSensorValue function");
    }

    // If the robot has been authenticated regulation and regular communication can be established
    if (authenticatedByServer) {
        // If the timer that controls the update speed has expired, the robot can proceed with regulation and communication
        if (isTimerExpired()) {
            // If surveillance mode is deactivated, the robot knows that it is active regulation and outputs can be set
            if (!surveillanceModeTemp) {
                setOutputState("temperature", temperatureSetpoint, tempValue, HEATER_OUTPUT_PIN, tempActuatorReversed,
                               TEMP_SENSOR_KEY);
            }
            // If surveillance mode is deactivated, the robot knows that it is active regulation and outputs can be set
            if (!surveillanceModeCo2) {
                setOutputState("co2", co2Setpoint, co2Value, VENTILATION_OUTPUT_PIN, co2ActuatorReversed,
                               CO2_SENSOR_KEY);
            }

            // Checks if the sensors has read a large enough chance to exceed the threshold to send the values to server
            checkForSensorChange("temperature", TEMP_SENSOR_KEY, tempValue);
            checkForSensorChange("co2", CO2_SENSOR_KEY, co2Value);
            checkForSensorChange("internal-temp", INTERNAL_TEMP_SENSOR_KEY, 0);

            // Starts a timer for when the next time the robot can set output states and send current values to server
            // This value is used in the statement that is evaluated to enter this part of the code
            startTimer (timeout);
        }

    }

    webSocket.loop();
}
