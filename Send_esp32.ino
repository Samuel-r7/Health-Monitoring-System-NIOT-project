#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h> // only for esp_wifi_set_channel()
#include <Wire.h>
#include <DFRobot_MAX30102.h>
 
// MAX30105 particleSensor
DFRobot_MAX30102 oximeter;
 
// variables
int32_t SPO2; //SPO2
int8_t SPO2Valid; //Flag to display if SPO2 calculation is valid==1 or invalid==0
int32_t heartRate; //Heart-rate
int8_t heartRateValid; //Flag to display if heart-rate calculation is valid==1 or invalid==0
int heartRateHold, SPO2Hold; //Variables to hold valid calculations
String tips; //User Interface tips
const byte GROUP_SIZE = 4; //Increase for more averaging. 4 is good.
byte heartRates[GROUP_SIZE]; //Array of heart rates
byte rateSpot = 0; //Cyclic variable to go 0-1-...n-0-1...
byte SPO2s[GROUP_SIZE]; //Array of SPO2 data
byte oxygenSpot = 0; //Cyclic variable
int beatAvg, SPO2Avg; //Averages to be displayed
int irValue = 0;
int temperature = 0;
unsigned long lastTimeSent = 0;
 
int SLAVE_ID = 2;
 
// Global copy of slave
esp_now_peer_info_t slave;
#define CHANNEL 1 // Change channel after checking receiver AP channel.
#define PRINTSCANRESULTS 0
#define DELETEBEFOREPAIR 0
 
// Init ESP Now with fallback
void InitESPNow() {
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  }
  else {
    Serial.println("ESPNow Init Failed");
    // Retry InitESPNow, add a counte and then restart?
    // InitESPNow();
    // or Simply Restart
    ESP.restart();
  }
}
 
// Scan for slaves in AP mode
void ScanForSlave() {
  int16_t scanResults = WiFi.scanNetworks(false, false, false, 300, CHANNEL); // Scan only on one channel
  // reset on each scan
  bool slaveFound = 0;
  memset(&slave, 0, sizeof(slave));
 
  Serial.println("");
  if (scanResults == 0) {
    Serial.println("No WiFi devices in AP Mode found");
  } else {
    Serial.print("Found "); Serial.print(scanResults); Serial.println(" devices ");
    for (int i = 0; i < scanResults; ++i) {
      // Print SSID and RSSI for each device found
      String SSID = WiFi.SSID(i);
      int32_t RSSI = WiFi.RSSI(i);
      String BSSIDstr = WiFi.BSSIDstr(i);
 
      if (PRINTSCANRESULTS) {
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(SSID);
        Serial.print(" (");
        Serial.print(RSSI);
        Serial.print(")");
        Serial.println("");
      }
      delay(10);
      // Check if the current device starts with `Slave`
      if (SSID.indexOf("Slave") == 0) {
        // SSID of interest
        Serial.println("Found a Slave.");
        Serial.print(i + 1); Serial.print(": "); Serial.print(SSID); Serial.print(" ["); Serial.print(BSSIDstr); Serial.print("]"); Serial.print(" ("); Serial.print(RSSI); Serial.print(")"); Serial.println("");
        // Get BSSID => Mac Address of the Slave
        int mac[6];
        if ( 6 == sscanf(BSSIDstr.c_str(), "%x:%x:%x:%x:%x:%x",  &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5] ) ) {
          for (int ii = 0; ii < 6; ++ii ) {
            slave.peer_addr[ii] = (uint8_t) mac[ii];
          }
        }
 
        slave.channel = CHANNEL; // pick a channel
        slave.encrypt = 0; // no encryption
 
        slaveFound = 1;
        // we are planning to have only one slave in this example;
        // Hence, break after we find one, to be a bit efficient
        break;
      }
    }
  }
 
  if (slaveFound) {
    Serial.println("Slave Found, processing..");
  } else {
    Serial.println("Slave Not Found, trying again.");
  }
 
  // clean up ram
  WiFi.scanDelete();
}
 
// Check if the slave is already paired with the master.
// If not, pair the slave with master
bool manageSlave() {
  if (slave.channel == CHANNEL) {
    if (DELETEBEFOREPAIR) {
      deletePeer();
    }
 
    Serial.print("Slave Status: ");
    // check if the peer exists
    bool exists = esp_now_is_peer_exist(slave.peer_addr);
    if ( exists) {
      // Slave already paired.
      Serial.println("Already Paired");
      return true;
    } else {
      // Slave not paired, attempt pair
      esp_err_t addStatus = esp_now_add_peer(&slave);
      if (addStatus == ESP_OK) {
        // Pair success
        Serial.println("Pair success");
        return true;
      } else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT) {
        // How did we get so far!!
        Serial.println("ESPNOW Not Init");
        return false;
      } else if (addStatus == ESP_ERR_ESPNOW_ARG) {
        Serial.println("Invalid Argument");
        return false;
      } else if (addStatus == ESP_ERR_ESPNOW_FULL) {
        Serial.println("Peer list full");
        return false;
      } else if (addStatus == ESP_ERR_ESPNOW_NO_MEM) {
        Serial.println("Out of memory");
        return false;
      } else if (addStatus == ESP_ERR_ESPNOW_EXIST) {
        Serial.println("Peer Exists");
        return true;
      } else {
        Serial.println("Not sure what happened");
        return false;
      }
    }
  } else {
    // No slave found to process
    Serial.println("No Slave found to process");
    return false;
  }
}
 
void deletePeer() {
  esp_err_t delStatus = esp_now_del_peer(slave.peer_addr);
  Serial.print("Slave Delete Status: ");
  if (delStatus == ESP_OK) {
    // Delete success
    Serial.println("Success");
  } else if (delStatus == ESP_ERR_ESPNOW_NOT_INIT) {
    // How did we get so far!!
    Serial.println("ESPNOW Not Init");
  } else if (delStatus == ESP_ERR_ESPNOW_ARG) {
    Serial.println("Invalid Argument");
  } else if (delStatus == ESP_ERR_ESPNOW_NOT_FOUND) {
    Serial.println("Peer not found.");
  } else {
    Serial.println("Not sure what happened");
  }
}
 
// char data[] = "Hello, ESP-NOW!";
typedef struct struct_message {
  // int id;
  // int IR;
  // int BPM;
  // int SPO2;
  // int TEMP;
  int id;
  int IR;
  int BPM;
  int SPO2;
  int TEMP;
} struct_message;
 
struct_message data;
 
// send data
void sendData() {
  // data++;
  data.id = SLAVE_ID;
  data.IR = irValue;
  data.BPM = beatAvg;
  data.SPO2 = SPO2Avg;
  data.TEMP = temperature;
 
  // data.id = 0;
  // data.IR = 12343;
  // data.BPM = 66;
  // data.SPO2 = 94;
  // data.TEMP = 102;
  const uint8_t *peer_addr = slave.peer_addr;
  Serial.print("Sending: "); //Serial.println(data);
  esp_err_t result = esp_now_send(peer_addr, (uint8_t*)&data, sizeof(data));
  Serial.print("Send Status: ");
  if (result == ESP_OK) {
    Serial.println("Success");
  } else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
    // How did we get so far!!
    Serial.println("ESPNOW not Init.");
  } else if (result == ESP_ERR_ESPNOW_ARG) {
    Serial.println("Invalid Argument");
  } else if (result == ESP_ERR_ESPNOW_INTERNAL) {
    Serial.println("Internal Error");
  } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
    Serial.println("ESP_ERR_ESPNOW_NO_MEM");
  } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
    Serial.println("Peer not found.");
  } else {
    Serial.println("Not sure what happened");
  }
}
 
// callback when data is sent from Master to Slave
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Last Packet Sent to: "); Serial.println(macStr);
  Serial.print("Last Packet Send Status: "); Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}
 
void setup() {
  Serial.begin(115200);
  //Set device in STA mode to begin with
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
  Serial.println("ESPNow/Basic/Master Example");
  // This is the mac address of the Master in Station Mode
  Serial.print("STA MAC: "); Serial.println(WiFi.macAddress());
  Serial.print("STA CHANNEL "); Serial.println(WiFi.channel());
  // Init ESPNow with a fallback logic
  InitESPNow();
  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
 
  // sensor
  while (!oximeter.begin())
  {
    Serial.println("MAX30102 was not found");
    delay(1000);
  }
  oximeter.sensorConfiguration(/*ledBrightness=*/150, /*sampleAverage=*/SAMPLEAVG_4, \
                        /*ledMode=*/MODE_MULTILED, /*sampleRate=*/SAMPLERATE_3200, \
                        /*pulseWidth=*/PULSEWIDTH_411, /*adcRange=*/ADCRANGE_16384);
}
 
void loop() {
 
  // SENSOR CALCULATION
 
  irValue = oximeter.getIR();
  temperature = oximeter.readTemperatureF();
  oximeter.heartrateAndOxygenSaturation(/*SPO2=*/&SPO2, /*SPO2Valid=*/&SPO2Valid, /*heartRate=*/&heartRate, /*heartRateValid=*/&heartRateValid);
 
  // calculation heartrate
 
  if (heartRate < 130 && heartRate > 40)
  {
    heartRates[rateSpot++] = (byte)heartRate; //Store reading in the array
    rateSpot %= GROUP_SIZE; //Wrap variable
 
    beatAvg = 0;
    for (byte x = 0 ; x < GROUP_SIZE ; x++)
      beatAvg += heartRates[x];
    beatAvg /= GROUP_SIZE;
  }
 
  // calculate spo2
  /* Take the rolling average of
   * four SPO2 measurements
   */
  if (SPO2 <= 100 && SPO2 > 86)
  {
    SPO2s[oxygenSpot++] = (byte)SPO2; //Store reading in the array
    oxygenSpot %= GROUP_SIZE; //Wrap variable
    SPO2Avg = 0;
 
    for (byte y = 0 ; y < GROUP_SIZE ; y++)
      SPO2Avg += SPO2s[y];
    SPO2Avg /= GROUP_SIZE;
  }
 
  //Print result
  Serial.print(F("heartRate="));
  Serial.print(heartRate, DEC);
  Serial.print(F(", heartRateValid="));
  Serial.print(heartRateValid, DEC);
  Serial.print(F("; SPO2="));
  Serial.print(SPO2, DEC);
  Serial.print(F(", SPO2Valid="));
  Serial.println(SPO2Valid, DEC);
  Serial.print(F("AvgHeartRate="));
  Serial.print(beatAvg, DEC);
  Serial.print(F("; AvgSPO2="));
  Serial.println(SPO2Avg, DEC);
  Serial.print("TemperatureF= ");
  Serial.println(temperature);
 
  // Detect if a finger is absent*/
  if (oximeter.getIR() < 5000)
  {
    SPO2Avg = 0;
    beatAvg = 0; //reset to zero since finger is out
    tips="Place your finger on the sensor.\n";
    Serial.println(F("Finger absent."));
  }
 
  /*Print appropriate UI message in tips*/
  if (heartRateValid==1 && SPO2Valid==1) {
    tips="Measuring...\n";
  } else if (heartRateValid==1 || SPO2Valid==1) {
    tips="Try to stay still and quiet.\n";
  } else if (oximeter.getIR()>5000) {
    tips="Check that your finger is placed well.\n";
  }
 
  if (millis() - lastTimeSent > 15000) {
    // In the loop we scan for slave
  ScanForSlave();
  // If Slave is found, it would be populate in `slave` variable
  // We will check if `slave` is defined and then we proceed further
  if (slave.channel == CHANNEL) { // check if slave channel is defined
    // `slave` is defined
    // Add slave as peer if it has not been added already
    bool isPaired = manageSlave();
    if (isPaired) {
      // pair success or already paired
      // Send data to device
      sendData();
    } else {
      // slave pair failed
      Serial.println("Slave pair failed!");
    }
  }
  else {
    // No slave found to process
  }
 
  lastTimeSent = millis();
 
  }
}
 
boolean checkForBeat(long sample)
{
  static long lastBeatTime = 0;
  int beats = 0;
 
  if (sample > 50000)
  {
    if ((millis() - lastBeatTime) > 500)
    {
      beats++;
      lastBeatTime = millis();
      return true;
    }
  }
 
  return false;
}
