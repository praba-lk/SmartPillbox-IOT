#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>  // I2C LCD library
#include <ESP_Mail_Client.h>     // Email client library
#include <Servo.h>               // Include Servo library
#include <time.h>                // Include time library

// WiFi credentials
const char* ssid = "temp";                // Change as needed
const char* password = "temp12345";       // Change as needed

// Email credentials
#define SENDER_EMAIL "lkpraba2004@gmail.com"       // CHANGE IT
#define SENDER_PASSWORD "xmtm xcls vqyv cnak"  // CHANGE IT to your Google App password
#define RECIPIENT_EMAIL "2216102@saec.ac.in"    // CHANGE IT

#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 587

// Initialize I2C LCD with address 0x27 and 16x2 display size
LiquidCrystal_I2C lcd(0x27, 16, 2);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);  // Adjust timezone here (in seconds)

// IR sensor pin
const int irSensorPin = D0;  // Digital pin connected to IR sensor
const int redLEDPin = D3;    // Red LED pin for alerts
const int greenLEDPin = D4;  // Green LED pin for normal operation
const int buzzerPin = D5;    // Buzzer pin for alerts

// Servo pins
const int servo1Pin = D8;    // Pin for servo 1
const int servo2Pin = D7;    // Pin for servo 2

Servo servo1; // Create servo object for servo 1
Servo servo2; // Create servo object for servo 2

int initialCount = 100;  // Set the initial count for items (e.g., tablets)
int count = initialCount;  // Object detection count
bool objectDetected = false;

// Email sending session
SMTPSession smtp;

// Alarm times (HH:MM format in 24-hour)
int alertTimes[3][2] = {
  {12,10},   // Alarm time 1 morning
  {13,30},   // Alarm time 2 afternoon
  {9,30}    // Alarm time 3 night
};

void setup() {
  Serial.begin(115200);
  
  // Initialize the I2C LCD display
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // Set up LEDs and buzzer pins
  pinMode(redLEDPin, OUTPUT);
  pinMode(greenLEDPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");

  // Initialize time client
  timeClient.setTimeOffset(5 * 3600 + 30 * 60); // Adjust for your timezone
  timeClient.begin();

  // Set IR sensor pin as input
  pinMode(irSensorPin, INPUT);
  
  // Initialize servo motors
  servo1.attach(servo1Pin);
  servo2.attach(servo2Pin);
}

void loop() {
  timeClient.update();  // Update the NTP client

  // Extract current time
  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();
  String formattedTime = String(currentHour) + ":" + (currentMinute < 10 ? "0" : "") + String(currentMinute);

  // Extract current date
  time_t rawtime = timeClient.getEpochTime(); // Get the current epoch time
  struct tm * dt = localtime(&rawtime); // Convert epoch time to tm structure

  // Get date components
  int currentDay = dt->tm_mday;
  int currentMonth = dt->tm_mon + 1; // tm_mon is 0-11, so we add 1
  int currentYear = dt->tm_year + 1900; // tm_year is years since 1900

  String formattedDate = String(currentDay) + "/" + (currentMonth < 10 ? "0" : "") + String(currentMonth) + "/" + String(currentYear);

  // Display date and time on LCD
  lcd.setCursor(0, 0);  // Set cursor to the first row
  lcd.print("D: " + formattedDate);
  lcd.setCursor(0, 1);  // Set cursor to the second row
  lcd.print("T: " + formattedTime);

  // Check for alarm times
  bool alarmActive = false; // Flag to check if any alarm is active
  for (int i = 0; i < 3; i++) {
    if (currentHour == alertTimes[i][0] && currentMinute == alertTimes[i][1]) {
      alarmActive = true; // Set the alarm active flag
      break;
    }
  }

  // Handle alarm and IR sensor state
  if (alarmActive) {
    // Activate buzzer and red LED during alarm time
    activateBuzzerAndLED();

    // Open and close the servos during alarm time
    activateServos();

    // Check for object detection during alarm time
    if (digitalRead(irSensorPin) == HIGH) {
      // Object detected during alarm time, turn off buzzer and turn on green LED
      deactivateBuzzerAndLED();
    } 
  } else {
    // Not an alarm time, turn off buzzer and LEDs
    deactivateBuzzerAndLED();
  }

  // Check the IR sensor for detecting object entry and exit
  if (digitalRead(irSensorPin) == HIGH && !objectDetected && count > 0) {
    // Object detected by the IR sensor
    count--;  // Decrement count
    objectDetected = true;
    Serial.println("Object detected!");

    // Send email notification
    String subject = "Email Notification from ESP8266";
    String textMsg = "This is an email sent from ESP8266.\n";
    textMsg += "Pill has been taken.";

    gmail_send(subject, textMsg);
    delay(500);  // Debounce delay
  } else if (digitalRead(irSensorPin) == LOW) {
    // Reset object detected status when no object is present
    objectDetected = false;
  }

  // Update the I2C LCD display with count
  lcd.setCursor(10, 1);
  lcd.print("C: ");
  lcd.print(count);
  lcd.print("   ");  // Clear leftover characters

  delay(100);  // Reduce delay to make the loop more responsive
}

void activateBuzzerAndLED() {
  digitalWrite(buzzerPin, HIGH);   // Turn on buzzer
  digitalWrite(redLEDPin, HIGH);    // Turn on red LED
  digitalWrite(greenLEDPin, LOW);   // Turn off green LED
}

void deactivateBuzzerAndLED() {
  digitalWrite(buzzerPin, LOW);    // Turn off buzzer
  digitalWrite(redLEDPin, LOW);     // Turn off red LED
  digitalWrite(greenLEDPin, HIGH);  // Turn on green LED
}

void activateServos() {
  servo1.write(0); // Move servo 1 to 0 degrees (open)
  servo2.write(180); // Move servo 2 to 180 degrees (open)
  delay(20000); // Wait for 5 seconds to let the servos move

  // Return servos to 180 degrees (close)
  servo1.write(180); // Move servo 1 back to 180 degrees (close)
  servo2.write(0); // Move servo 2 back to 0 degrees (close)
    delay(5000); // Wait for 5 seconds to let the servos move
}

void gmail_send(String subject, String textMsg) {
  // Set the network reconnection option
  MailClient.networkReconnect(true);

  smtp.debug(1);
  smtp.callback(smtpCallback);
  Session_Config config;

  // Set the session config
  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = SENDER_EMAIL;
  config.login.password = SENDER_PASSWORD;
  config.login.user_domain = F("127.0.0.1");
  config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
  config.time.gmt_offset = 3;
  config.time.day_light_offset = 0;

  // Declare the message class
  SMTP_Message message;

  // Set the message headers
  message.sender.name = F("ESP8266");
  message.sender.email = SENDER_EMAIL;
  message.subject = subject;
  message.addRecipient(F("Recipient"), RECIPIENT_EMAIL);
  message.text.content = textMsg;
  message.text.transfer_encoding = "base64";
  message.text.charSet = F("utf-8");
  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;

  // Connect to the server
  if (!smtp.connect(&config)) {
    Serial.printf("Connection error, Status Code: %d, Error Code: %d, Reason: %s\n", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    return;
  }

  // Send the email
  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.printf("Error sending Email, Status Code: %d\n", smtp.statusCode());
  }

  smtp.closeSession();
}

void smtpCallback(SMTP_Status status) {
  // Print the status information
  Serial.println(status.info());

  if (status.success()) {
    Serial.println("Message sent successfully!");      } else {
    Serial.println("Error sending the message");
  }
}