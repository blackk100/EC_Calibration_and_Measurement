const int probeDriver[2] = {A0, A2};      // Source voltage
const int probeSense[2] = {A1, A3};       // Sense voltage
const int temperaturePin = A5;
const int Rs = 660;                       // External static resistance
const int Ra = 25;                        // Resistance of analog pins
const float Vin = 5.0;                    // Input voltage
const float calibrationEC = 0.81;         // EC value of calibration solution in ms/cm
const float calibrationTempCoeff = 0.019; // Temperature Coefficient of resistance of the calibration solution in %/100 K
const float tempCoeff = 0.019;            // Temperature Coefficient of resistance of the nutrient solution in %/100 K
const int ECDelay = 50;                   // Delay between swithing voltage on a pin in ms
float K = 0.0;                            // Calibrated cell constant value in cm
//char ECUnits = ' ';                       // ' ' = e+0 (Standard); 'm' = e-3 (Milli); 'u' = e-6 (Micro) ; Need to implement properly

// Calibration solution used: 10% Ca(NO3)2 by weight

void setup() {
  Serial.begin(9600);

  pinMode(probeSense[0], INPUT);
  pinMode(probeSense[1], INPUT);
  pinMode(probeDriver[0], OUTPUT);
  pinMode(probeDriver[1], OUTPUT);
  pinMode(temperaturePin, INPUT);
  digitalWrite(probeDriver[0], LOW);
  digitalWrite(probeDriver[1], LOW);

  // Use only with Arduino Uno Rev3 to keep the Pin13 LED off
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);

  Serial.println("Starting calibration");
  while (calibrateProbe());
  Serial.println("Calibration completed!\n");
}

void loop() {
  float EC = measureEC(false);
  float temp = getTemp();
  float ECTemp = EC / (1.0 + (tempCoeff * (temp - 25.0)));
  Serial.print("Temperature = ");
  Serial.print(temp, 6);
  Serial.print(" deg C\nConductivity = ");
  Serial.print(EC, 6);
  Serial.print(" mS per cm\nTemperature compensated EC = ");
  Serial.print(ECTemp, 6);
  Serial.println(" mS per cm\n");

  delay(500); // Aprox. 1 sec between readings
}

bool calibrateProbe() {
  int tempStart = getTempRaw();
  Serial.print("Start Temp: ");
  Serial.print(getTemp(tempStart), 6);
  Serial.println(" deg C");

  unsigned long Rsum = 0;
  unsigned int i = 0;
  unsigned int iter = min(60000 / (ECDelay * 10), 65535);
  /*
  Size of unsigned int         = 2^16 - 1        = 65535
  Size of unsigned long / 1024 = 2^32 - 1 / 2^10 ~ 2^22 ; 2^22 - 1 = 4194303
  For ECDelay = 50:
    65535 iterations will take ~ 54.61 min
  4194303 iterations will take ~ 58.25 hr
  Limiting to 1 minute for calibration, 60/0.05 = 1200 iterations
  */

  for (i = 0; i < iter; i++) {
    Rsum += measureEC(true);
  }

  float R = (Rsum * 1.0) / (iter * 1.0);

  float tempEnd = getTempRaw();
  Serial.print("End Temp: ");
  Serial.print(getTemp(tempEnd), 6);
  Serial.println(" deg C");

  if (tempStart == tempEnd) {
    float EC = calibrationEC * (1.0 + (calibrationTempCoeff * (getTemp(tempEnd) - 25.0)));
    float i_K = R * EC;

    Serial.print("Results are trustworthy\nStandard Calibration EC: ");
    Serial.print(calibrationEC, 6);
    Serial.print(" mS per cm\nActual Calibration EC: ");
    Serial.print(EC, 6);
    Serial.print(" mS per cm\nCalibrated Cell Constant K: ");
    Serial.print(i_K, 6);
    Serial.println(" cm");

    K = i_K;
    return false; // Break out of calibration loop
  } else {
    Serial.println("Error - Wait For Temperature to settle\n");
    return true;  // Continue calibration loop
  }
}

float measureEC(bool calib) {
  unsigned long Vsum[] = {0, 0};
  unsigned int i = 0;

  while (i < 10) {
    digitalWrite(probeDriver[i % 2 ? 0 : 1], HIGH);
    digitalWrite(probeDriver[i % 2 ? 1 : 0], LOW);
    delay(ECDelay);

    Vsum[0] += analogRead(probeSense[i % 2 ? 0 : 1]);
    Vsum[1] += analogRead(probeSense[i % 2 ? 1 : 0]);

    i++;
  }

  for (i = 0; i < 2; i++) {
    digitalWrite(probeDriver[i], LOW);
  }

  float R = (((Vsum[0] * 1.0) / (Vsum[1] * 1.0)) - 1.0) * ((Rs + Ra) * 1.0);
  R *= 1000.0;      // Convert to milliohm
  float EC = K / R;

  if (calib) {
    return R;
  } else {
    return EC;
  }
}

int getTempRaw() {
  return analogRead(temperaturePin);
}

float getTemp() {
  int value = getTempRaw();
  float voltage = (value * 5.0) / 1024.0;
  float temp = (voltage * 100.0) - 50.0; // (mV - 500) / 10 = deg C for TMP36. Replace with appropriate equation for DSB18B20 when module is acquired.
  return temp;
}

float getTemp(int value) {               // Overloaded function when I already have the analog int reading
  float voltage = (value * 5.0) / 1024.0;
  float temp = (voltage * 100.0) - 50.0; // (mV - 500) / 10 = deg C for TMP36. Replace with appropriate equation for DSB18B20 when module is acquired.
  return temp;
}

