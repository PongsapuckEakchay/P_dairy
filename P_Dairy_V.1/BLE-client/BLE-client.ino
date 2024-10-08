#include "BLEDevice.h"
#include <PubSubClient.h>
#include <WiFi.h>
#include "config.h"
#define red_led 27
#define green_led 23
#define Switch 22
int last=0;

//BLE Server name (the other ESP32 name running the server sketch)
#define bleServerName "BME280_ESP32"

/* UUID's of the service, characteristic that we want to read*/
// BLE Service
static BLEUUID bmeServiceUUID("91bad492-b950-4226-aa2b-4ede9fa42f59");

// Humidity Characteristic
static BLEUUID humidityCharacteristicUUID("ca73b3ba-39f6-4ab3-91ae-186dc9577d99");

//Flags stating if should begin connecting and if the connection is up
static boolean doConnect = false;
static boolean connected = false;

//Address of the peripheral device. Address will be found during scanning...
static BLEAddress* pServerAddress;

//Characteristicd that we want to read
//static BLERemoteCharacteristic* temperatureCharacteristic;
static BLERemoteCharacteristic* humidityCharacteristic;

//Activate notify
const uint8_t notificationOn[] = { 0x1, 0x0 };
const uint8_t notificationOff[] = { 0x0, 0x0 };

//Variables to store temperature and humidity
char* temperatureChar;
char* humidityChar;

//Flags to check whether new temperature and humidity readings are available
boolean newTemperature = false;
boolean newHumidity = false;

//Connect to the BLE Server that has the name, Service, and Characteristics
bool connectToServer(BLEAddress pAddress) {
  BLEClient* pClient = BLEDevice::createClient();

  // Connect to the remove BLE Server.
  pClient->connect(pAddress);
  Serial.println(" - Connected to server");

  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService* pRemoteService = pClient->getService(bmeServiceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(bmeServiceUUID.toString().c_str());
    return (false);
  }

  // Obtain a reference to the characteristics in the service of the remote BLE server.
  humidityCharacteristic = pRemoteService->getCharacteristic(humidityCharacteristicUUID);

  if (humidityCharacteristic == nullptr) {
    Serial.print("Failed to find our characteristic UUID");
    return false;
  }
  Serial.println(" - Found our characteristics");

  //Assign callback functions for the Characteristics
  humidityCharacteristic->registerForNotify(humidityNotifyCallback);
  return true;
}

//Callback function that gets called, when another device's advertisement has been received
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.getName() == bleServerName) {                 //Check if the name of the advertiser matches
      advertisedDevice.getScan()->stop();                              //Scan can be stopped, we found what we are looking for
      pServerAddress = new BLEAddress(advertisedDevice.getAddress());  //Address of advertiser is the one we need
      doConnect = true;                                                //Set indicator, stating that we are ready to connect
      Serial.println("Device found. Connecting!");
    }
  }
};

//When the BLE Server sends a new humidity reading with the notify property
static void humidityNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  //store humidity value
  humidityChar = (char*)pData;
  newHumidity = true;
  //Serial.println(newHumidity);
}

WiFiClient wifiClient;
PubSubClient mqtt(MQTT_BROKER, 1883, wifiClient);
void connect_wifi() {
  printf("WiFi MAC address is %s\n", WiFi.macAddress().c_str());
  printf("Connecting to WiFi %s.\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    printf(".");
    fflush(stdout);
    delay(500);
  }
  printf("\nWiFi connected.\n");
}

void connect_mqtt() {
  printf("Connecting to MQTT broker at %s.\n", MQTT_BROKER);
  if (!mqtt.connect(MQTT_BROKER, MQTT_USER, MQTT_PASS)) {
    printf("Failed to connect to MQTT broker.\n");
    printf("%d\n", mqtt.state());
    for (;;) {}  // wait here forever
  }
  //  mqtt.setCallback(mqtt_callback);
  //mqtt.subscribe("#");
  printf("MQTT broker connected.\n");
}


void setup() {
  //Start serial communication

  //Serial.print("Grams1\tGrams\tGrams3\tGram4\tTotal\troll\tpitch\n");
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");
  pinMode(red_led, OUTPUT);
  pinMode(green_led, OUTPUT);
  pinMode(Switch, INPUT);
  //Init BLE device
  BLEDevice::init("");

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 30 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30);
  //connect_wifi();
  //connect_mqtt();
}

void loop() {
  int time;

  // Serial.print("Roll:"); Serial.println(roll);
  // Serial.print("Pitch:"); Serial.println(pitch);
  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
  // connected we set the connected flag to be true.
  if (doConnect == true) {
    if (connectToServer(*pServerAddress)) {
      Serial.println("We are now connected to the BLE Server.");
      //Activate the Notify property of each Characteristic
      //temperatureCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      humidityCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      digitalWrite(red_led, LOW);
      connected = true;
    } else {
      Serial.println("We have failed to connect to the server; Restart your device to scan for nearby BLE server again.");
      BLEDevice::init("");

      // Retrieve a Scanner and set the callback we want to use to be informed when we
      // have detected a new device.  Specify that we want active scanning and start the
      // scan to run for 30 seconds.
      BLEScan* pBLEScan = BLEDevice::getScan();
      pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
      pBLEScan->setActiveScan(true);
      pBLEScan->start(30);
      digitalWrite(red_led, HIGH);
    }
    doConnect = false;
  }

  if (newHumidity) {
    //newTemperature = false;
    newHumidity = false;
    if (*humidityChar == '1') {
      digitalWrite(green_led, HIGH);
    } else {
      digitalWrite(green_led, LOW);
    }

    //digitalWrite(23,LOW);
    //Serial.println(humidityChar);
    //printReadings();
    //mqtt.publish("apichai/RUN", humidityChar);
  }
  delay(1000);  // Delay a second between loops.
}