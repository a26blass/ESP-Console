#define DBG_BUTTON_PIN 0
#define INPUT_UP 13
#define INPUT_DOWN 12
#define INPUT_LEFT 14
#define INPUT_RIGHT 27
#define INPUT_A 26
#define INPUT_B 25
#define INPUT_START 32

#ifndef INPUTS_H
#define INPUTS_H

// Function to initialize the input pins
void inputs_init();

// Function to check if the UP button is pressed
bool get_up_pressed();

// Function to check if the DOWN button is pressed
bool get_down_pressed();

// Function to check if the LEFT button is pressed
bool get_left_pressed();

// Function to check if the RIGHT button is pressed
bool get_right_pressed();

// Function to check if the A button is pressed (one-shot)
bool get_a_pressed();

// Function to check if the B button is pressed (one-shot)
bool get_b_pressed();

// Function to check if the START button is pressed (one-shot)
bool get_start_pressed();

// Function to check if the debug button is pressed
bool debug_input_check();

#endif
