# RichArduino V7.0 GUI

This project implements a GUI application for controlling and interacting with a RichArduino V7.0 single-board microcontroller. The application provides features for firmware updates, real-time waveform visualization, and low-level microcontroller operations.
Very quick demo of the software [here](https://www.youtube.com/watch?v=elPzWEfb8gQ).

## Features

- Serial communication with RichArduino V7.0 board
- Firmware update functionality
- Real-time waveform visualization
- Low-level microcontroller operations (Peek, Poke, Version)
- DMA initialization
- SPI communication
- Oscilloscope-like features (triggering, snapshot, zoom)

## Dependencies

- Qt 5.x or later
- QtSerialPort

## Key Components

### MainWindow

The main application window that handles user interactions and displays the GUI.

Key functionalities:
- Serial port connection management
- Firmware update
- Waveform visualization and analysis
- Low-level microcontroller operations

### FirmwareUpdater

Handles the firmware update process for the RichArduino V7.0 board.

### Commands

Implements low-level communication commands with the microcontroller:

- `Poke`: Write data to a specific address
- `Peek`: Read data from a specific address
- `Version`: Retrieve the firmware version

## Usage

1. Connect the RichArduino V7.0 board to your computer via USB.
2. Launch the application.
3. Select the appropriate COM port and connect to the board.
4. Use the various features provided by the GUI to interact with the board.

## Building the Project

1. Ensure you have Qt and QtCreator installed.
2. Open the project file in QtCreator.
3. Configure the project for your Qt version and compiler.
4. Build the project.

## Authors
Hussein Aljorani

## Acknowledgments

- Thanks to the Qt community for providing excellent documentation and resources.
- Thanks to [DR. Richard](https://www.linkedin.com/in/william-richard-b6b59a101/) for the huge support 
