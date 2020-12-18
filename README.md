# ESP-32 Client

This is a ESP-32 client for reading different types of sensors, and for integrating the client to a server. This ESP-32
communicates with a server, and receives information on how to regulate with potential set-points for the actuators. 
Furthermore the ESP-32 regulates the output based on this information, in addition to sending system parameters, like 
output states, to the server. 

## Installation
The program can be opened in different code editors for Arduino C, as for example CLion or Arduino IDE.

 
## Usage
### Configuration of additional sensors
#### System constants for every system
Some system constants have to be set for the ESP-32 to work as intended. 
Values like SSID name and corresponding password has to be set in the variables SSID and PASSWORD. 

Furthermore the IP address, port number and path for the server has to be set in the variables HOST, PORT and PATH respectively.

The robots ID has to be set in ROBOT_ID, and password for the robot to be set in SERVER_PASSWORD. Server password has to correspond 
to one of the passwords set in the robot server. 

#### System example details
As an example the program is now configured for one internal temperature sensor, one normal temperature sensor and one
CO2 sensor. Both the normal temperature and the CO2 sensors is configured with an output for regulation.

#### Instructions for adding additional sensors / actuators
If user wants to connect more sensors and outputs, additional global variables has to be set, and more function calls in the
main loop has to be called.

##### New variables
For each new sensor and actuator the following types of variables has to be set with their own unique name:

```
int INPUT_PIN;
int OUTPUT_PIN;
float setpoint;
float value;
float previousValue;
bool actuatorReversed;
bool previousOutputState;
bool surveillanceMode;
String SENSOR_KEY;
```
Examples for each of these potential new variables are already set in the program and are currently named:

```
int TEMP_INPUT_PIN;
int HEATER_OUTPUT_PIN;
float temperatureSetpoint;
float tempValue;
float previousTempValue;
bool tempActuatorreversed;
bool previousTempOutputState;
bool surveillanceModeTemp;
String TEMP_SENSOR_KEY;
```
##### New function calls
Every new output has to be initialized in the setup of the program with this function:

```pinMode(OUTPUT_PIN, OUTPUT);```

Furthermore if the user wants to add more sensors, the following extra function calls have to be called in the main loop:

```value = readSensorValue("sensor_type", INPUT_PIN);```

And inside the first if statement in the main loop that is checking if the robot is authenticated and timer has expired. Here a new if statement
with the function call and parameters need to be called:
```
if (!surveillanceMode) {
setOutputState("sensor_type", setpoint, value, OUTPUT_PIN, reversed, SENSOR_KEY);
}
```
Additionally this function for sending values to server has to be called with the following parameters:

```checkForSensorChange("sensor_type", SENSOR_KEY, value);```




## Contributing
If you want to contribute to this project you need to use the same structure and guidelines followed by this project.

This project is made in CLion, as an CMake project Using the PlatformIO Plugin for building and flashing of the microcontroller. There are alternative methods
but they are not covered here.

You need to install [Platform IO Core](https://docs.platformio.org/en/latest/core/installation.html), and add the
Install Shell Commands as instructed [here](https://docs.platformio.org/en/latest/core/installation.html#piocore-install-shell-commands), [here](https://www.architectryan.com/2018/03/17/add-to-the-PATH-on-windows-10/)
are some supplementary help for Windows 10. The plugin for CLion is also required,
for building and flashing of the program. The plugin can be found in the plugin marketplace in CLion (Settings/Preferences | Plugins).

After cloning the project to CLion you need to Re-Init (Tools->PlatformIO->Re-Init) the project to build and flash the program.

Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.

Please make sure to update tests as appropriate.

## Branches
feat - Feature I'm adding or expanding

bug - Bug fix or experiment



## License
[MIT](https://choosealicense.com/licenses/mit/)