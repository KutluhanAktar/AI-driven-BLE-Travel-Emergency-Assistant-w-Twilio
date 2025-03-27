         /////////////////////////////////////////////  
        //   AI-driven BLE Travel Emergency        //
       //          Assistant w/ Twilio            //
      //             ---------------             //
     //          (XIAO ESP32S3 (Sense))         //           
    //             by Kutluhan Aktar           // 
   //                                         //
  /////////////////////////////////////////////

//
// Detect keychains to inform emergency contacts via WhatsApp / SMS. Display results via BLE. Let contacts request location info from Maps API.
//
// For more information:
// https://www.theamplituhedron.com/projects/AI_driven_BLE_Travel_Emergency_Assistant
//
//
// Connections
// XIAO ESP32S3 (Sense) :
//                                XIAO Round Display 
// https://wiki.seeedstudio.com/seeedstudio_round_display_usage/#getting-started                  


// Include the required libraries:
#include <Arduino.h>
#include <ArduinoBLE.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "esp_camera.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"

// Add the icons to be shown on the XIAO round display.
#include "logo.h"

// Include the Edge Impulse FOMO model converted to an Arduino library:
#include <AI-driven_BLE_Travel_Emergency_Assistant_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"

// Define the required parameters to run an inference with the Edge Impulse FOMO model.
#define CAPTURED_IMAGE_BUFFER_COLS        240
#define CAPTURED_IMAGE_BUFFER_ROWS        240
#define EI_CAMERA_FRAME_BYTE_SIZE         3
uint8_t *ei_camera_capture_out;

// Define the emergency class names:
String classes[] = {"Fine", "Danger", "Assist", "Stolen", "Call"};

// Create the BLE service:
BLEService Emergency_Assistant("23bc2b0f-3081-402e-8ba2-300280c91740");

// Create data characteristics and allow the remote device (central) to write, read, and notify:
BLEFloatCharacteristic detection_Characteristic("23bc2b0f-3081-402e-8ba2-300280c91741", BLERead | BLENotify);
BLEByteCharacteristic class_Characteristic("23bc2b0f-3081-402e-8ba2-300280c91742", BLERead | BLEWrite);

// Define the OV2640 camera pin configuration on the XIAO ESP32S3 Sense expansion board.
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39
#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13
#define LED_GPIO_NUM      21

// Utilize the SD card reader on the expansion board without cutting the J3 solder bridge.
#define SD_CS_PIN 21 // #define SD_CS_PIN D2 // XIAO Round Display SD Card Reader

// Define the XIAO round display object and image dimensions.
TFT_eSPI tft = TFT_eSPI();
const int img_width = 240;
const int img_height = 240;

// Define the data holders:
#define TOUCH_INT D7
int predicted_class = -1;
volatile boolean camera_activated = false;
volatile boolean sd_activated = false;
volatile boolean _connected = false;
int sample_number[5] = {0, 0, 0, 0, 0};
long timer;


void setup(){
  Serial.begin(115200);

  // Define the OV2640 camera pin and frame settings.
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
  config.xclk_freq_hz = 10000000; // Set XCLK_FREQ_HZ as 10KHz to avoid the EV-VSYNC-OVF error.
  config.frame_size = FRAMESIZE_240X240; // FRAMESIZE_UXGA, FRAMESIZE_SVGA
  config.pixel_format = PIXFORMAT_RGB565; // PIXFORMAT_JPEG
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 2;

  // Initialize the OV2640 camera.
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  // If successful:
  Serial.println("OV2640 camera initialized successfully!");
  camera_activated = true;

  // Initialize the XIAO round display.
  tft.init();
  tft.setRotation(2);
  tft.fillScreen(TFT_WHITE);

  // Initialize the SD card module on the expansion board.
  if(!SD.begin(SD_CS_PIN)){
    Serial.println("SD Card => No module found!");
    return;
  }
  // If successful:
  Serial.println("SD card initialized successfully!");
  sd_activated = true;

  // Check the BLE initialization status:
  while(!BLE.begin()){
    Serial.println("BLE initialization is failed!");
  }
  Serial.println("\nBLE initialization is successful!\n");
  // Print this peripheral device's address information:
  Serial.print("MAC Address: "); Serial.println(BLE.address());
  Serial.print("Service UUID Address: "); Serial.println(Emergency_Assistant.uuid()); Serial.println();

  // Set the local name this peripheral advertises: 
  BLE.setLocalName("BLE Emergency Assistant");
  // Set the UUID for the service this peripheral advertises:
  BLE.setAdvertisedService(Emergency_Assistant);

  // Add the given data characteristics to the service:
  Emergency_Assistant.addCharacteristic(detection_Characteristic);
  Emergency_Assistant.addCharacteristic(class_Characteristic);

  // Add the given service to the advertising device:
  BLE.addService(Emergency_Assistant);

  // Assign event handlers for connected and disconnected devices to/from this peripheral:
  BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);

  // Assign event handlers for the data characteristics modified (written) by the central device (via the Android application).
  // In this regard, obtain the transferred (written) commands from the Android application over BLE.
  class_Characteristic.setEventHandler(BLEWritten, get_selected_class);

  // Start advertising:
  BLE.advertise();
  Serial.println("Bluetooth device active, waiting for connections...");

  delay(5000);
  
  timer = millis();
}

void loop(){
  // Poll for BLE events:
  BLE.poll();

  if(camera_activated && sd_activated){
    // Capture a frame (RGB565 buffer) with the OV2640 camera.
    camera_fb_t *fb = esp_camera_fb_get();
    if(!fb){ Serial.println("Camera => Cannot capture the frame!"); return; }

    // Display the captured frame on the XIAO round display.
    tft.startWrite();
    tft.setAddrWindow(0, 0, img_width, img_height);
    tft.pushColors(fb->buf, fb->len);
    tft.endWrite();

    // Every 30 seconds, run the Edge Impulse FOMO model to make predictions on the emergency classes.
    if(millis() - timer > 30*1000){
      // Run inference.
      run_inference_to_make_predictions(fb);
      // If the Edge Impulse FOMO model detects an emergency class (keychain) successfully:
      if(predicted_class > -1){
        // Update the detection characteristic via BLE.
        update_characteristics(predicted_class);
        // Notify the user on the XIAO round display depending on the detected class.
        tft.drawXBitmap((img_width/2)-(detect_width/2), 2, detect_bits, detect_width, detect_height, TFT_BLACK);
        if(predicted_class == 0) tft.drawXBitmap((img_width/2)-(fine_width/2), (img_height/2)-(fine_height/2), fine_bits, fine_width, fine_height, TFT_GREEN);
        if(predicted_class == 1) tft.drawXBitmap((img_width/2)-(danger_width/2), (img_height/2)-(danger_height/2), danger_bits, danger_width, danger_height, TFT_ORANGE);
        if(predicted_class == 2) tft.drawXBitmap((img_width/2)-(assist_width/2), (img_height/2)-(assist_height/2), assist_bits, assist_width, assist_height, TFT_SILVER);
        if(predicted_class == 3) tft.drawXBitmap((img_width/2)-(stolen_width/2), (img_height/2)-(stolen_height/2), stolen_bits, stolen_width, stolen_height, TFT_NAVY);
        if(predicted_class == 4) tft.drawXBitmap((img_width/2)-(call_width/2), (img_height/2)-(call_height/2), call_bits, call_width, call_height, TFT_GREENYELLOW);
        delay(2000);
        // Clear the predicted class (label).
        predicted_class = -1;
      }
      // Update the timer:
      timer = millis();
    }

    // If the user touches the XIAO round display, show the current sample numbers for each emergency class on the SD card.
    if(display_is_pressed()){
      String f_n = "Fine: " + String(sample_number[0]);
      String d_n = "Danger: " + String(sample_number[1]);
      String a_n = "Assist: " + String(sample_number[2]);
      String s_n = "Stolen: " + String(sample_number[3]);
      String c_n = "Call: " + String(sample_number[4]);
      int x = 75, y = 25, d = 40;
      tft.setTextSize(2);
      tft.setTextColor(TFT_WHITE, TFT_GREEN, false);
      tft.drawString(f_n.c_str(), x, y, 2);
      tft.setTextColor(TFT_WHITE, TFT_ORANGE, false);
      tft.drawString(d_n.c_str(), x, y+d, 2);
      tft.setTextColor(TFT_WHITE, TFT_SILVER, false);
      tft.drawString(a_n.c_str(), x, y+(2*d), 2);
      tft.setTextColor(TFT_WHITE, TFT_NAVY, false);
      tft.drawString(s_n.c_str(), x, y+(3*d), 2);
      tft.setTextColor(TFT_WHITE, TFT_GREENYELLOW, false);
      tft.drawString(c_n.c_str(), x, y+(4*d), 2);
      delay(1000);
    }

    // Release the image buffers.
    esp_camera_fb_return(fb);
    delay(10);
  }
}

void run_inference_to_make_predictions(camera_fb_t *fb){
  // Summarize the Edge Impulse FOMO model inference settings (from model_metadata.h):
  ei_printf("\nInference settings:\n");
  ei_printf("\tImage resolution: %dx%d\n", EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT);
  ei_printf("\tFrame size: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
  ei_printf("\tNo. of classes: %d\n", sizeof(ei_classifier_inferencing_categories) / sizeof(ei_classifier_inferencing_categories[0]));
  
  if(fb){
    // Convert the captured RGB565 buffer to RGB888 buffer.
    ei_camera_capture_out = (uint8_t*)malloc(CAPTURED_IMAGE_BUFFER_COLS * CAPTURED_IMAGE_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);
    if(!fmt2rgb888(fb->buf, fb->len, PIXFORMAT_RGB565, ei_camera_capture_out)){ Serial.println("Camera => Cannot convert the RGB565 buffer to RGB888!"); return; }

    // Depending on the given model, resize the converted RGB888 buffer by utilizing built-in Edge Impulse functions.
    ei::image::processing::crop_and_interpolate_rgb888(
      ei_camera_capture_out, // Output image buffer, can be same as input buffer
      CAPTURED_IMAGE_BUFFER_COLS,
      CAPTURED_IMAGE_BUFFER_ROWS,
      ei_camera_capture_out,
      EI_CLASSIFIER_INPUT_WIDTH,
      EI_CLASSIFIER_INPUT_HEIGHT);

    // Run inference:
    ei::signal_t signal;
    // Create a signal object from the converted and resized image buffer.
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data = &ei_camera_cutout_get_data;
    // Run the classifier:
    ei_impulse_result_t result = { 0 };
    EI_IMPULSE_ERROR _err = run_classifier(&signal, &result, false);
    if(_err != EI_IMPULSE_OK){
      ei_printf("ERR: Failed to run classifier (%d)\n", _err);
      return;
    }

    // Print the inference timings on the serial monitor.
    ei_printf("\nPredictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
        result.timing.dsp, result.timing.classification, result.timing.anomaly);

    // Obtain the object detection results and bounding boxes for the detected labels (classes). 
    bool bb_found = result.bounding_boxes[0].value > 0;
    for(size_t ix = 0; ix < EI_CLASSIFIER_OBJECT_DETECTION_COUNT; ix++){
      auto bb = result.bounding_boxes[ix];
      if(bb.value == 0) continue;
      // Print the calculated bounding box measurements on the serial monitor.
      ei_printf("    %s (", bb.label);
      ei_printf_float(bb.value);
      ei_printf(") [ x: %u, y: %u, width: %u, height: %u ]\n", bb.x, bb.y, bb.width, bb.height);
      // Get the predicted label (class).
      if(bb.label == "fine") predicted_class = 0;
      if(bb.label == "danger") predicted_class = 1;
      if(bb.label == "assist") predicted_class = 2;
      if(bb.label == "stolen") predicted_class = 3;
      if(bb.label == "call") predicted_class = 4;
      Serial.print("\nPredicted Class: "); Serial.println(bb.label);
    }
    if(!bb_found) ei_printf("    No objects found!\n");

    // Detect anomalies, if any:
    #if EI_CLASSIFIER_HAS_ANOMALY == 1
      ei_printf("Anomaly: ");
      ei_printf_float(result.anomaly);
      ei_printf("\n");
    #endif 

    // Release the image buffers.
    free(ei_camera_capture_out);
  }
}

static int ei_camera_cutout_get_data(size_t offset, size_t length, float *out_ptr){
  // Convert the given image data (buffer) to the out_ptr format required by the Edge Impulse FOMO model.
  size_t pixel_ix = offset * 3;
  size_t pixels_left = length;
  size_t out_ptr_ix = 0;
  // Since the image data is converted to an RGB888 buffer, directly recalculate offset into pixel index.
  while(pixels_left != 0){  
    out_ptr[out_ptr_ix] = (ei_camera_capture_out[pixel_ix] << 16) + (ei_camera_capture_out[pixel_ix + 1] << 8) + ei_camera_capture_out[pixel_ix + 2];
    // Move to the next pixel.
    out_ptr_ix++;
    pixel_ix+=3;
    pixels_left--;
  }
  return 0;
}

void update_characteristics(float detection){
  // Update the detection characteristic.
  detection_Characteristic.writeValue(detection);
  Serial.println("\n\nBLE: Data Characteristics Updated Successfully!\n");
}

void get_selected_class(BLEDevice central, BLECharacteristic characteristic){
  // Get the recently transferred commands over BLE.
  if(characteristic.uuid() == class_Characteristic.uuid()){
    Serial.print("\nSelected Class => "); Serial.println(class_Characteristic.value());
    // Capture a new frame (RGB565 buffer) with the OV2640 camera.
    camera_fb_t *fb = esp_camera_fb_get();
    if(!fb){ Serial.println("Camera => Cannot capture the frame!"); return; }
    // Convert the captured RGB565 buffer to JPEG buffer.
    size_t con_len;
    uint8_t *con_buf = NULL;
    if(!frame2jpg(fb, 10, &con_buf, &con_len)){ Serial.println("Camera => Cannot convert the RGB565 buffer to JPEG!"); return; }
    // Depending on the selected emergency class, save the converted frame as a sample to the SD card.
    String file_name = "";
    switch(class_Characteristic.value()){
      case 0:
        // Save the given frame as an image file.
        file_name = "/" + classes[0] + "_" + String(sample_number[0]) + ".jpg";
        save_image(SD, file_name.c_str(), con_buf, con_len);
        // Notify the user on the XIAO round display.
        tft.drawXBitmap((img_width/2)-(save_width/2), 2, save_bits, save_width, save_height, TFT_BLACK);
        tft.drawXBitmap((img_width/2)-(fine_width/2), (img_height/2)-(fine_height/2), fine_bits, fine_width, fine_height, TFT_GREEN);
        // Increase the sample number.
        sample_number[0]+=1;
        delay(2000);
      break;
      case 1:
        // Save the given frame as an image file.
        file_name = "/" + classes[1] + "_" + String(sample_number[1]) + ".jpg";
        save_image(SD, file_name.c_str(), con_buf, con_len);
        // Notify the user on the XIAO round display.
        tft.drawXBitmap((img_width/2)-(save_width/2), 2, save_bits, save_width, save_height, TFT_BLACK);
        tft.drawXBitmap((img_width/2)-(danger_width/2), (img_height/2)-(danger_height/2), danger_bits, danger_width, danger_height, TFT_ORANGE);
        // Increase the sample number.
        sample_number[1]+=1;
        delay(2000);
      break;
      case 2:
        // Save the given frame as an image file.
        file_name = "/" + classes[2] + "_" + String(sample_number[2]) + ".jpg";
        save_image(SD, file_name.c_str(), con_buf, con_len);
        // Notify the user on the XIAO round display.
        tft.drawXBitmap((img_width/2)-(save_width/2), 2, save_bits, save_width, save_height, TFT_BLACK);
        tft.drawXBitmap((img_width/2)-(assist_width/2), (img_height/2)-(assist_height/2), assist_bits, assist_width, assist_height, TFT_SILVER);
        // Increase the sample number.
        sample_number[2]+=1;
        delay(2000);
      break;
      case 3:
        // Save the given frame as an image file.
        file_name = "/" + classes[3] + "_" + String(sample_number[3]) + ".jpg";
        save_image(SD, file_name.c_str(), con_buf, con_len);
        // Notify the user on the XIAO round display.
        tft.drawXBitmap((img_width/2)-(save_width/2), 2, save_bits, save_width, save_height, TFT_BLACK);
        tft.drawXBitmap((img_width/2)-(stolen_width/2), (img_height/2)-(stolen_height/2), stolen_bits, stolen_width, stolen_height, TFT_NAVY);
        // Increase the sample number.
        sample_number[3]+=1;
        delay(2000);
      break;
      case 4:
        // Save the given frame as an image file.
        file_name = "/" + classes[4] + "_" + String(sample_number[4]) + ".jpg";
        save_image(SD, file_name.c_str(), con_buf, con_len);
        // Notify the user on the XIAO round display.
        tft.drawXBitmap((img_width/2)-(save_width/2), 2, save_bits, save_width, save_height, TFT_BLACK);
        tft.drawXBitmap((img_width/2)-(call_width/2), (img_height/2)-(call_height/2), call_bits, call_width, call_height, TFT_GREENYELLOW);
        // Increase the sample number.
        sample_number[4]+=1;
        delay(2000);
      break;
    }
    // Release the image buffers.
    free(con_buf);
    esp_camera_fb_return(fb);
  }
}

void save_image(fs::FS &fs, const char *file_name, uint8_t *data, size_t len){
  // Create a new file on the SD card.
  File file = fs.open(file_name, FILE_WRITE);
  if(!file){ Serial.println("SD Card => Cannot create file!"); return; }
  // Save the given image buffer to the created file on the SD card.
  if(file.write(data, len) == len){
      Serial.printf("SD Card => IMG saved: %s\n", file_name);
  }else{
      Serial.println("SD Card => Cannot save the given image!");
  }
  file.close();    
}

// Detect when the user touches the XIAO round display.
bool display_is_pressed(){
  if(digitalRead(TOUCH_INT) != LOW){
    delay(10);
    if(digitalRead(TOUCH_INT) != LOW) return false;
  }
  return true;
}

void blePeripheralConnectHandler(BLEDevice central){
  // Central connected event handler:
  Serial.print("\nConnected event, central: ");
  Serial.println(central.address());
  _connected = true;
}

void blePeripheralDisconnectHandler(BLEDevice central){
  // Central disconnected event handler:
  Serial.print("\nDisconnected event, central: ");
  Serial.println(central.address());
  _connected = false;
}
