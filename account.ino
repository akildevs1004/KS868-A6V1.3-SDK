
void getDeviceAccoutnDetails() {



  int httpCode;

  String company = "";  //"No COMPANY";
  String accountExpiry = "Not Set";
  cloudAccountActiveDaysRemaining = 40;

  StaticJsonDocument<256> accountDoc;

  struct tm timeInfo;
  int retry = 0;
  const int retryCount = 5;

  while (company.isEmpty() && retry < retryCount) {
    Serial.println("⏳ Waiting for Account Details...");
    String url = serverURL + "/get_device_company_info_arduino?serial_number=" + device_serial_number;
    Serial.println("-----------------------------------Reading Account Details...");
    Serial.println(url);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(5000);  // Set timeout to 5 seconds

    int httpCode = http.GET();

    if (httpCode > 0) {
      Serial.println("HTTP Response Code: " + String(httpCode));

      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println("Response: " + payload);

        DeserializationError error = deserializeJson(accountDoc, payload);
        if (!error) {
          company = accountDoc["company"]["name"] | "No COMPANY";
          accountExpiry = accountDoc["company"]["expiry"] | "Not Set";
        } else {
          Serial.print("JSON Parsing Failed: ");
          Serial.println(error.c_str());
        }
      } else {
        Serial.println("HTTP Error: " + http.errorToString(httpCode));
      }
    } else {
      Serial.println("HTTP Connection Failed: " + http.errorToString(httpCode));
    }

    http.end();  // Always close connection

    if (company.isEmpty()) {
      delay(1000);
      retry++;
    }
  }  //while




  // // Calculate remaining days
  cloudAccountActiveDaysRemaining = calculateRemainingDays(accountExpiry);
  Serial.println("Remaining Days to Expiry: " + String(cloudAccountActiveDaysRemaining));

  updateJsonConfig("config.json", "cloud_company_name", company);
  updateJsonConfig("config.json", "cloud_account_expire", accountExpiry);
  updateJsonConfig("config.json", "cloudAccountActiveDaysRemaining", String(cloudAccountActiveDaysRemaining));


  Serial.println("Parsed Values:");
  Serial.println("Company: " + company);
  Serial.println("Expiry: " + accountExpiry);
  Serial.println("Days Remaining: " + String(cloudAccountActiveDaysRemaining));

  Serial.println("---------------------------------------------------------------------------");
}

// String expiryDate = "2025/10/13";  // Example expiry date
// // Function to get the current date in YYYY/MM/DD format
String getCurrentDate() {



  struct tm timeInfo;
  int retry = 0;
  const int retryCount = 5;

  while (!getLocalTime(&timeInfo) && retry < retryCount) {
    Serial.println("⏳ Waiting for NTP time...");
    delay(1000);
    retry++;
  }



  char dateStr[11];                                           // 10 characters + null terminator
  strftime(dateStr, sizeof(dateStr), "%Y/%m/%d", &timeInfo);  // Format: YYYY/MM/DD

  return String(dateStr);
}

// Function to convert a date string (YYYY/MM/DD) into epoch time (seconds)
time_t getEpochFromDate(String dateStr) {
  struct tm tm;
  memset(&tm, 0, sizeof(struct tm));

  int year = dateStr.substring(0, 4).toInt();
  int month = dateStr.substring(5, 7).toInt();
  int day = dateStr.substring(8, 10).toInt();

  tm.tm_year = year - 1900;  // Years since 1900
  tm.tm_mon = month - 1;     // Month range: 0-11
  tm.tm_mday = day;

  return mktime(&tm);
}

// Function to calculate the remaining days from today to the expiry date
int calculateRemainingDays(String expiryDateStr) {
  time_t now;
  time(&now);  // Get current time (epoch time)


  Serial.println("Input Expiry " + expiryDateStr);

  time_t expiry = getEpochFromDate(expiryDateStr);  // Get expiry date as epoch time

  // Calculate the difference in seconds
  double secondsRemaining = difftime(expiry, now);
  int daysRemaining = secondsRemaining / 86400;  // Convert seconds to days

  return daysRemaining;
}