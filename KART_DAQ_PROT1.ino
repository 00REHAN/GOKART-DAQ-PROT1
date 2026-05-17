#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>

// ======================================================
//                        MPU6050
// ======================================================

#define MPU_ADDR 0x69

void mpuWrite(uint8_t reg, uint8_t data) {

    Wire.beginTransmission(MPU_ADDR);
    Wire.write(reg);
    Wire.write(data);
    Wire.endTransmission();
}

void mpuRead(uint8_t reg, uint8_t count, uint8_t *data) {

    Wire.beginTransmission(MPU_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);

    Wire.requestFrom(MPU_ADDR, count);

    for (int i = 0; i < count; i++) {
        data[i] = Wire.read();
    }
}

void mpuInit() {

    mpuWrite(0x6B, 0x00);   // Wake MPU6050
    mpuWrite(0x1B, 0x00);   // Gyro ±250 dps
    mpuWrite(0x1C, 0x00);   // Accel ±2g
}

// ======================================================
//                  HALL EFFECT SENSOR
// ======================================================

#define HALL_SENSOR_PIN 27

volatile unsigned long pulseCount = 0;

unsigned long lastRPMTime = 0;

float rpm = 0.0;
float speedKmph = 0.0;

/*
    Wheel circumference in meters

    Example:
    ~0.94m circumference
*/

float wheelCircumference = 0.94;

/*
    Gear Ratio

    Driver  = 13T
    Driven  = 23T

    Wheel RPM = Motor RPM × (13 / 23)
*/

float gearRatio = 13.0 / 23.0;

// ======================================================
//                HALL SENSOR INTERRUPT
// ======================================================

void IRAM_ATTR hallISR() {

    // Active LOW hall sensor
    pulseCount++;
}

// ======================================================
//                 RPM + SPEED UPDATE
// ======================================================

void calculateRPMAndSpeed() {

    unsigned long currentTime = millis();

    if (currentTime - lastRPMTime >= 1000) {

        noInterrupts();

        unsigned long pulses = pulseCount;
        pulseCount = 0;

        interrupts();

        /*
            Motor RPM

            Assuming:
            1 pulse = 1 motor revolution
        */

        rpm = pulses * 60.0;

        /*
            Convert motor RPM
            to wheel RPM
        */

        float wheelRPM = rpm * gearRatio;

        /*
            Speed Calculation

            speed =
            wheel circumference × wheel rev/sec
        */

        float wheelRPS = wheelRPM / 60.0;

        float speedMS =
            wheelRPS * wheelCircumference;

        speedKmph = speedMS * 3.6;

        lastRPMTime = currentTime;
    }
}

// ======================================================
//                    ESP-NOW PACKET
// ======================================================

typedef struct {

    int speed;
    int rpm;

} Datapacket;

Datapacket packet;

// Receiver MAC Address
uint8_t receiverMac[] = {
    0x24, 0xEC, 0x4A, 0x02, 0xD2, 0xBC
};

// ======================================================
//                 GLOBAL VARIABLES
// ======================================================

float pitch = 0.0;
float roll  = 0.0;
float yaw   = 0.0;

unsigned long lastIMUTime = 0;

unsigned long entryCount = 0;

// ======================================================
//                ORIENTATION CALCULATION
// ======================================================

void computeOrientation(
    float ax,
    float ay,
    float az,
    float gx,
    float gy,
    float gz
) {

    unsigned long now = micros();

    float dt =
        (now - lastIMUTime) / 1000000.0;

    lastIMUTime = now;

    if (dt <= 0 || dt > 0.1) {
        return;
    }

    // Accelerometer Angles
    float accelPitch =
        atan2(
            ay,
            sqrt(ax * ax + az * az)
        ) * 180 / PI;

    float accelRoll =
        atan2(-ax, az) * 180 / PI;

    // Gyroscope Integration
    pitch += gx * dt;
    roll  += gy * dt;
    yaw   += gz * dt;

    // Complementary Filter
    pitch =
        0.98 * pitch +
        0.02 * accelPitch;

    roll =
        0.98 * roll +
        0.02 * accelRoll;
}

// ======================================================
//                ESP-NOW CALLBACK
// ======================================================

void OnDataSent(
    const wifi_tx_info_t *info,
    esp_now_send_status_t status
) {

    /*
    Serial.print("ESP-NOW Delivery: ");

    Serial.println(
        status == ESP_NOW_SEND_SUCCESS ?
        "SUCCESS" :
        "FAILED"
    );
    */
}

// ======================================================
//                         SETUP
// ======================================================

void setup() {

    Serial.begin(115200);

    // I2C
    Wire.begin(21, 22);

    // MPU6050
    mpuInit();

    lastIMUTime = micros();

    Serial.println("MPU6050 Ready");

    // Hall Sensor
    pinMode(HALL_SENSOR_PIN, INPUT_PULLUP);

    attachInterrupt(
        digitalPinToInterrupt(HALL_SENSOR_PIN),
        hallISR,
        FALLING
    );

    // WiFi
    WiFi.mode(WIFI_STA);

    // ESP-NOW Init
    if (esp_now_init() != ESP_OK) {

        Serial.println("ESP-NOW init failed!");
        return;
    }

    esp_now_register_send_cb(OnDataSent);

    // Peer Config
    esp_now_peer_info_t peerInfo = {};

    memcpy(peerInfo.peer_addr, receiverMac, 6);

    peerInfo.channel = 1;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {

        Serial.println("Failed to add peer!");
        return;
    }

    Serial.println("MPU + HALL + ESP-NOW READY");
}

// ======================================================
//                          LOOP
// ======================================================

void loop() {

    // ==================================================
    //                  MPU6050 READ
    // ==================================================

    uint8_t rawData[14];

    mpuRead(0x3B, 14, rawData);

    int16_t ax =
        (rawData[0] << 8) | rawData[1];

    int16_t ay =
        (rawData[2] << 8) | rawData[3];

    int16_t az =
        (rawData[4] << 8) | rawData[5];

    int16_t gx =
        (rawData[8] << 8) | rawData[9];

    int16_t gy =
        (rawData[10] << 8) | rawData[11];

    int16_t gz =
        (rawData[12] << 8) | rawData[13];

    // ==================================================
    //                 SENSOR CONVERSION
    // ==================================================

    float accelX = ax / 16384.0;
    float accelY = ay / 16384.0;
    float accelZ = az / 16384.0;

    float gyroX = gx / 131.0;
    float gyroY = gy / 131.0;
    float gyroZ = gz / 131.0;

    // ==================================================
    //                ORIENTATION UPDATE
    // ==================================================

    computeOrientation(
        accelX,
        accelY,
        accelZ,
        gyroX,
        gyroY,
        gyroZ
    );

    // ==================================================
    //               RPM + SPEED UPDATE
    // ==================================================

    calculateRPMAndSpeed();

    entryCount++;

    // ==================================================
    //                  SERIAL OUTPUT
    // ==================================================

    Serial.print(entryCount);
    Serial.print(", ");

    Serial.print(speedKmph);
    Serial.print(" km/h, ");

    Serial.print(rpm);
    Serial.print(" RPM, ");

    Serial.print(accelX);
    Serial.print(", ");

    Serial.print(accelY);
    Serial.print(", ");

    Serial.print(accelZ);
    Serial.print(", ");

    Serial.print(gyroX);
    Serial.print(", ");

    Serial.print(gyroY);
    Serial.print(", ");

    Serial.print(gyroZ);
    Serial.print(", ");

    Serial.print(yaw, 2);
    Serial.print(", ");

    Serial.print(pitch, 2);
    Serial.print(", ");

    Serial.println(roll, 2);

    // ==================================================
    //                 PACKET UPDATE
    // ==================================================

    packet.speed = speedKmph;
    packet.rpm   = rpm;

    // ==================================================
    //                 ESP-NOW SEND
    // ==================================================

    esp_now_send(
        receiverMac,
        (uint8_t*)&packet,
        sizeof(packet)
    );

    delay(50);
}