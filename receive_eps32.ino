#include <esp_now.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
 
#define CHANNEL 1
bool sendHTTPReq = false;
 
// Replace with your network credentials
const char* ssid = "CSW1";
const char* password = "DESKTOP123";
 
// Local server settings - change the IP Address
const char* serverName = "http://192.168.137.247/sensor-api/PRINT.php";
 
// Declaration for SSD1306 display connected using I2C
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
 
typedef struct struct_message {
  int id;
  int IR;
  int BPM;
  int SPO2;
  int TEMP;
}struct_message;
 
struct_message data;
 
// Create a structure to hold the readings from each board
struct_message board1;
// struct_message board2;
// struct_message board3;
 
struct_message boardsStruct[1] = {board1};
 
// Init ESP Now with fallback
void InitESPNow() {
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  }
  else {
    Serial.println("ESPNow Init Failed");
    // Retry InitESPNow, add a counter and then restart?
    // InitESPNow();
    // or Simply Restart
    ESP.restart();
  }
}
 
// config AP SSID
void configDeviceAP() {
  const char *SSID = "Slave_1";
  bool result = WiFi.softAP(SSID, "Slave_1_Password", CHANNEL, 0);
  if (!result) {
    Serial.println("AP Config failed.");
  } else {
    Serial.println("AP Config Success. Broadcasting with AP: " + String(SSID));
    Serial.print("AP CHANNEL "); Serial.println(WiFi.channel());
  }
}
 
void setup() {
 
  Serial.begin(115200);
  Serial.println("ESPNow/Basic/Slave Example");
  WiFi.mode(WIFI_AP_STA);
 
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
 
  // configure device AP mode
  configDeviceAP();
  // This is the mac address of the Slave in AP Mode
  Serial.print("AP MAC: "); Serial.println(WiFi.softAPmacAddress());
  // Init ESPNow with a fallback logic
  InitESPNow();
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info.
 
  esp_now_register_recv_cb(OnDataRecv);
 
  Serial.println("Initializing...");
 
  // display
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  // Clear the display buffer.
  display.clearDisplay();
 
}
 
// callback when data is recv from Master
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int data_len) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Last Packet Recv from: "); Serial.println(macStr);
  Serial.print("Last Packet Recv Data: ");
  // Serial.println(*data);
 
  // Update the structures with the new incoming data
  memcpy(&data, incomingData, sizeof(data));
  Serial.printf("SLAVE ID %u: %u bytes\n", data.id, data_len);
  // Update the structures with the new incoming data
  boardsStruct[data.id-1].IR = data.IR;
  boardsStruct[data.id-1].BPM = data.BPM;
  boardsStruct[data.id-1].SPO2 = data.SPO2;
  boardsStruct[data.id-1].TEMP = data.TEMP;
  Serial.printf("BOARD ID: %d \n", data.id);
  Serial.printf("IR value: %d \n", boardsStruct[data.id-1].IR);
  Serial.printf("BPM value: %d \n", boardsStruct[data.id-1].BPM);
  Serial.printf("SPO2 value: %d \n", boardsStruct[data.id-1].SPO2);
  Serial.printf("TEMP value: %d \n", boardsStruct[data.id-1].TEMP);
 
  Serial.println();
 
  //Display
    // display info
  if (data.IR < 5000) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("NO FINGER DETECTED");
    display.display();
  }
  else
  {
    // Display heart rate and temperature on OLED
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("IR VALUE= ");
    display.println(data.IR);
    display.println(" ");
    display.print("HEARTRATE: ");
    display.println(data.BPM);
    display.println(" ");
    display.print("SPO2 LEVEL: ");
    display.println(data.SPO2);
    display.println(" ");
    display.print("TEMP(C): ");
    display.println(data.TEMP);
    display.display();
  }
 
  Serial.println();
  sendHTTPReq = true;
 
}
 
void loop() {
  if (sendHTTPReq) {
    // Send data to the local server
    Serial.println(sendHTTPReq);
    HTTPSendMode();
    if (WiFi.status() == WL_CONNECTED)
    {
      HTTPClient http;
      http.begin(String(serverName));
      String postData = "SLAVE_ID=" + String(data.id) +
                      "&BPM=" + String(data.BPM) +
                      "&TEMPERATURE=" + String(data.TEMP) +
                      "&SPO2=" + String(data.SPO2);
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");
      int httpResponseCode = http.POST(postData);
 
      if (httpResponseCode > 0)
      {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
      }
      else
      {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
 
      Serial.println("Data Sent to Server");
      http.end();
 
    }
 
  sendHTTPReq = false;
 
  }
}
 
void HTTPSendMode() {
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    Serial.println("Connecting");
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to WiFi network with IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.println("HTTP MODE ON");
    WiFi.printDiag(Serial);
}
