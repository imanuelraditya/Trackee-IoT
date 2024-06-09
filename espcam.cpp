#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "quirc.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h> // Make sure this library is installed

TaskHandle_t QRCodeReader_Task;

#define CAMERA_MODEL_AI_THINKER

#if defined(CAMERA_MODEL_AI_THINKER)
  #define PWDN_GPIO_NUM     32
  #define RESET_GPIO_NUM    -1
  #define XCLK_GPIO_NUM      0
  #define SIOD_GPIO_NUM     26
  #define SIOC_GPIO_NUM     27
  
  #define Y9_GPIO_NUM       35
  #define Y8_GPIO_NUM       34
  #define Y7_GPIO_NUM       39
  #define Y6_GPIO_NUM       36
  #define Y5_GPIO_NUM       21
  #define Y4_GPIO_NUM       19
  #define Y3_GPIO_NUM       18
  #define Y2_GPIO_NUM        5
  #define VSYNC_GPIO_NUM    25
  #define HREF_GPIO_NUM     23
  #define PCLK_GPIO_NUM     22
#else
  #error "Camera model not selected"
#endif

struct QRCodeData
{
  bool valid;
  int dataType;
  uint8_t payload[1024];
  int payloadLen;
};

struct quirc *q = NULL;
uint8_t *image = NULL;  
camera_fb_t * fb = NULL;
struct quirc_code code;
struct quirc_data data;
quirc_decode_error_t err;
struct QRCodeData qrCodeData;  
String QRCodeResult = "";

const char* ssid = "TB76A2";
const char* password = "tubis76A";
const char* supabaseUrl = "https://mzfjzdgvlmgmiuxrhrim.supabase.co";
const char* supabaseKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im16Zmp6ZGd2bG1nbWl1eHJocmltIiwicm9sZSI6ImFub24iLCJpYXQiOjE3MTc4MTM0ODMsImV4cCI6MjAzMzM4OTQ4M30.CKxS_B0GhvhzsCrLilY8XHEO4tQN_WK2MuzXwJDkub0";

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi.");

  Serial.println("Start configuring and initializing the camera...");
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_GRAYSCALE;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 15;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QVGA);

  Serial.println("Configure and initialize the camera successfully.");
  Serial.println();

  xTaskCreatePinnedToCore(
             QRCodeReader,
             "QRCodeReader_Task",
             10000,
             NULL,
             1,
             &QRCodeReader_Task,
             0);
}

void loop() {
  delay(1);
}

void QRCodeReader(void * pvParameters) {
  Serial.println("QRCodeReader is ready.");
  Serial.print("QRCodeReader running on core ");
  Serial.println(xPortGetCoreID());
  Serial.println();

  while (1) {
    q = quirc_new();
    if (q == NULL) {
      Serial.print("can't create quirc object\r\n");  
      continue;
    }
    
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      continue;
    }   
    
    quirc_resize(q, fb->width, fb->height);
    image = quirc_begin(q, NULL, NULL);
    memcpy(image, fb->buf, fb->len);
    quirc_end(q);
    
    int count = quirc_count(q);
    if (count > 0) {
      quirc_extract(q, 0, &code);
      err = quirc_decode(&code, &data);
    
      if (err) {
        Serial.println("Decoding FAILED");
        QRCodeResult = "Decoding FAILED";
      } else {
        Serial.printf("Decoding successful:\n");
        dumpData(&data);
        sendDataToSupabase((const char *)data.payload);
      } 
      Serial.println();
    }
    
    esp_camera_fb_return(fb);
    fb = NULL;
    image = NULL;  
    quirc_destroy(q);
  }
}

void dumpData(const struct quirc_data *data) {
  Serial.printf("Version: %d\n", data->version);
  Serial.printf("ECC level: %c\n", "MLHQ"[data->ecc_level]);
  Serial.printf("Mask: %d\n", data->mask);
  Serial.printf("Length: %d\n", data->payload_len);
  Serial.printf("Payload: %s\n", data->payload);
  
  QRCodeResult = (const char *)data->payload;
}

void sendDataToSupabase(const char* payload) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    String productPayload = "{\"product_id\":" + String(doc["product_id"].as<int>()) +
                            ", \"name\":\"" + String(doc["name"].as<const char*>()) +
                            "\", \"brand\":\"" + String(doc["brand"].as<const char*>()) +
                            "\", \"category\":\"" + String(doc["category"].as<const char*>()) +
                            "\"}";
                            
    String itemPayload = "{\"product_id\":" + String(doc["product_id"].as<int>()) +
                         ", \"rack_id\":" + String(doc["rack_id"].as<int>()) +
                         ", \"exp_date\":\"" + String(doc["exp_date"].as<const char*>()) + 
                         "\"}";

    int httpResponseCode;

    // Attempt to create the product
    http.begin(String(supabaseUrl) + "/rest/v1/product"); 
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", supabaseKey);
    http.addHeader("Authorization", "Bearer " + String(supabaseKey));
    httpResponseCode = http.POST(productPayload);
    if (httpResponseCode == 409) {
      Serial.println("Product already exists, updating instead.");
      
      // Retrieve the current frequency
      http.begin(String(supabaseUrl) + "/rest/v1/product?select=frequency&product_id=eq." + String(doc["product_id"].as<int>())); 
      http.addHeader("apikey", supabaseKey);
      http.addHeader("Authorization", "Bearer " + String(supabaseKey));
      httpResponseCode = http.GET();
      if (httpResponseCode == 200) {
        String response = http.getString();
        DynamicJsonDocument productDoc(1024);
        deserializeJson(productDoc, response);
        int frequency = productDoc[0]["frequency"].as<int>();
        
        // Update the product with new frequency
        String updatePayload = "{\"frequency\": " + String(frequency + 1) + "}";
        http.begin(String(supabaseUrl) + "/rest/v1/product?product_id=eq." + String(doc["product_id"].as<int>())); 
        http.addHeader("Content-Type", "application/json");
        http.addHeader("apikey", supabaseKey);
        http.addHeader("Authorization", "Bearer " + String(supabaseKey));
        httpResponseCode = http.PATCH(updatePayload);
        if (httpResponseCode > 0) {
          String response = http.getString();
          Serial.println("HTTP Response code (product update): " + String(httpResponseCode));
          Serial.println("Response: " + response);
        } else {
          Serial.println("Error on sending PATCH (product update): " + String(httpResponseCode));
        }
      } 
    } else if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("HTTP Response code (product): " + String(httpResponseCode));
      Serial.println("Response: " + response);
    } else {
      Serial.println("Error on sending POST (product): " + String(httpResponseCode));
    }
    http.end();

    // Send item data
    http.begin(String(supabaseUrl) + "/rest/v1/item"); 
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", supabaseKey);
    http.addHeader("Authorization", "Bearer " + String(supabaseKey));
    httpResponseCode = http.POST(itemPayload);
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("HTTP Response code (item): " + String(httpResponseCode));
      Serial.println("Response: " + response);
    } else {
      Serial.println("Error on sending POST (item): " + String(httpResponseCode));
    }
    http.end();

    // Fetch the largest item_id from the item table
    http.begin(String(supabaseUrl) + "/rest/v1/item?select=item_id&order=item_id.desc&limit=1"); 
    http.addHeader("apikey", supabaseKey);
    http.addHeader("Authorization", "Bearer " + String(supabaseKey));
    httpResponseCode = http.GET();
    int largestItemId = -1;
    if (httpResponseCode == 200) {
      String response = http.getString();
      DynamicJsonDocument itemDoc(1024);
      deserializeJson(itemDoc, response);
      if (itemDoc.size() > 0) {
        largestItemId = itemDoc[0]["item_id"].as<int>();
        Serial.println("Scanned item_id: " + String(largestItemId));
      } else {
        Serial.println("No items found in the item table.");
      }
    } else {
      Serial.println("Error on retrieving item data: " + String(httpResponseCode));
    }
    http.end();

    if (largestItemId != -1) {
      // Find an empty location in the rack
      http.begin(String(supabaseUrl) + "/rest/v1/rack_loc?rack_id=eq." + String(doc["rack_id"].as<int>()) + "&item_id=is.null"); 
      http.addHeader("apikey", supabaseKey);
      http.addHeader("Authorization", "Bearer " + String(supabaseKey));
      httpResponseCode = http.GET();
      if (httpResponseCode == 200) {
        String response = http.getString();
        DynamicJsonDocument locationDoc(1024);
        deserializeJson(locationDoc, response);
        if (locationDoc.size() > 0) {
          int loc_x = locationDoc[0]["loc_x"].as<int>();
          int loc_y = locationDoc[0]["loc_y"].as<int>();
          
          // Update the item_id of the found location
          String updateLocationPayload = "{\"item_id\": " + String(largestItemId) + "}";
          http.begin(String(supabaseUrl) + "/rest/v1/rack_loc?rack_id=eq." + String(doc["rack_id"].as<int>()) +
                     "&loc_x=eq." + String(loc_x) + "&loc_y=eq." + String(loc_y)); 
          http.addHeader("Content-Type", "application/json");
          http.addHeader("apikey", supabaseKey);
          http.addHeader("Authorization", "Bearer " + String(supabaseKey));
          httpResponseCode = http.PATCH(updateLocationPayload);
          if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("HTTP Response code (location update): " + String(httpResponseCode));
            Serial.println("Response: " + response);
          } else {
            Serial.println("Error on sending PATCH (location update): " + String(httpResponseCode));
          }
        } else {
          Serial.println("No empty location found in the rack.");
        }
      } else {
        Serial.println("Error on retrieving location data: " + String(httpResponseCode));
      }
      http.end();
    }
  }
}
