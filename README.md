# Go-Kart Telemetry & DAQ System — Prototype V1

This repository contains the first iteration of a custom-built **Data Acquisition and Telemetry (DAQ) System** developed for an electric go-kart platform. The system is designed to collect, process, and display real-time vehicle telemetry data for driver feedback, testing, and performance analysis.

The project uses **two ESP32-based systems**:

- **ESP32 DAQ Unit** → Collects and processes sensor data from the vehicle
- **ESP32-S3 Display Unit** → Receives telemetry wirelessly over Bluetooth and displays it on a 2.8” LCD using **LVGL v8**

The DAQ unit is assembled on a soldered Vero/perf board and interfaces with multiple sensors and vehicle subsystems.

---

# Features

## Real-Time Telemetry

The system collects and transmits:

- Motor RPM
- Vehicle speed
- GPS coordinates
- Timestamped sensor data
- Acceleration and G-force data
- Gyroscope and orientation data
- Motion analysis using IMU data

---

# System Architecture

## 1. DAQ Unit (ESP32)

The main DAQ ESP32 is responsible for:

- Reading all sensor inputs
- Processing telemetry data
- Timestamping data packets
- Sending processed data over Bluetooth
- Managing real-time vehicle parameters

### Sensors & Inputs

#### Hall Effect Sensor

Used for:
- RPM calculation
- Wheel speed sensing

#### GPS Module

Provides:
- GPS-based speed
- Latitude & longitude coordinates
- Position tracking

#### MPU6050 IMU

Provides:
- Accelerometer data
- Gyroscope data
- Vehicle orientation
- Motion tracking
- G-force measurements

---

## 2. Display Unit (ESP32-S3)

The display subsystem uses an **ESP32-S3** connected to a **2.8” LCD display**.

### Features

- Wireless Bluetooth telemetry reception
- Real-time dashboard UI
- Driver-focused interface
- No wired connection between steering and chassis electronics
- Built using **LVGL v8.0**

The display shows:

- RPM
- Speed
- GPS data
- IMU data
- Live telemetry parameters

---

# Wireless Communication

Telemetry data is transmitted from the DAQ ESP32 to the display ESP32-S3 using **Bluetooth communication**.

This eliminates the need for wired connections running through the steering assembly, improving:

- Reliability
- Cable management
- Steering movement
- Overall system aesthetics

---

# Hardware Used

- ESP32
- ESP32-S3
- MPU6050
- Hall Effect Sensor
- GPS Module
- 2.8” LCD Display
- Vero/Perf Board
- Bluetooth Communication
- LVGL v8.0

---

# Pin Configuration

## DAQ ESP32 Pinout

| Component | Signal | ESP32 Pin |
|----------|----------|----------|
| Hall Effect Sensor | Digital Output | GPIO 27 |
| MPU6050 SDA | I2C SDA | GPIO 21 |
| MPU6050 SCL | I2C SCL | GPIO 22 |
| GPS TX | UART RX | GPIO 16 |
| GPS RX | UART TX | GPIO 17 |

---

## ESP32-S3 Display Pinout

| Component | Signal | ESP32-S3 Pin |
|----------|----------|----------|
| LCD MOSI | SPI MOSI | GPIO 11 |
| LCD MISO | SPI MISO | GPIO 13 |
| LCD SCK | SPI Clock | GPIO 12 |
| LCD CS | Chip Select | GPIO 10 |
| LCD DC | Data/Command | GPIO 9 |
| LCD RST | Reset | GPIO 14 |

---

# Software Stack

## DAQ Firmware

Handles:

- Sensor interfacing
- Data acquisition
- RPM calculations
- IMU processing
- Bluetooth transmission
- Timestamp management

## Display Firmware

Handles:

- Bluetooth data reception
- UI rendering
- Real-time dashboard updates
- LVGL graphics and animations

---

# Schematic

The schematic and hardware wiring diagrams for both the DAQ unit and display subsystem are included in this repository.

They contain:
- ESP32 sensor connections
- Power distribution
- MPU6050 wiring
- GPS module connections
- Hall sensor interface
- LCD display connections
- Bluetooth communication architecture

---

# Future Improvements

- SD card logging
- LoRa telemetry support
- CAN bus integration
- Cloud telemetry dashboard
- Battery and motor analytics
- Lap timing and performance analysis
- Fault detection system

---

# Author
Rehan Saleem

Developed as part of a go-kart electronics and telemetry system project focused on embedded systems, wireless communication, and real-time vehicle data acquisition..
