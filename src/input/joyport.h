#pragma once
// Atari type joystick connector (DE-9) of Libor Lasota's boards (MPH-1V K4/K2, ANK-1M2 K2): what is
// plugged in and the levels it gives. The six signal pins are read as
//   D0 pin 2, D1 pin 1, D2 pin 4, D3 pin 3, D4 pin 6, D5 pin 9   (active low, pull-ups = 1)
// so a joystick gives D5..D0 = FR FL left right up down (the order of Bus::joystick, HostJoy).
// A parallel mouse turns two wheels with quadrature outputs:
//   Atari ST: pin 2 XA, pin 1 XB, pin 4 YB, pin 3 YA;  Amiga: pin 2 H, pin 1 V, pin 4 HQ, pin 3 VQ;
//   buttons: left = pin 6, right = pin 9.
#include <stdint.h>
#include <string>
#include "core/bus.h"
#include "json.hpp"

enum JoyDevice { JOY_DEV_NONE = 0, JOY_DEV_JOYSTICK, JOY_DEV_MOUSE_ATARI, JOY_DEV_MOUSE_AMIGA };

// "none", "joystick", "mouse_atari", "mouse_amiga"
bool joy_device_parse(const nlohmann::json& v, int& out, std::string& err, const char* key);
const char* joy_device_name(int dev);
static inline bool joy_device_is_mouse(int dev) { return dev >= JOY_DEV_MOUSE_ATARI; }

// The wheels of a mouse following the host mouse (Bus::mouse_dx/dy) at most RATE quadrature steps
// per second and axis: a real mouse cannot turn faster, a program polling the port would miss steps.
struct QuadMouse {
    static const int RATE = 1000;
    uint8_t mx = 0, my = 0;    // quadrature phase 0-3 of the X and Y wheels
    void step(Bus* bus);       // one step of each wheel towards the movement the host still owes
};

// Levels of the connector: D5..D0 as above, D7 D6 = 1. host_joy = which host controller a
// joystick follows (Bus::joystick index).
uint8_t joy_port_read(int dev, int host_joy, const QuadMouse& mouse, const Bus* bus);
