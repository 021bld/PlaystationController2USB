/*************************************************************
 * PlaystationController2USB.ino
 * 
 * Description:
 *    This is an Arduino sketch that transforms a PSX (PlayStation 2) controller into a USB joystick using an Arduino Leonardo or Micro along with the "ArduinoJoystickLibrary." 
 *    This sketch enables users to connect a PSX controller to the Arduino board and convert it into a USB gamepad, making it compatible with various applications and games that support USB joystick input.
 *       
 * Credits:
 *    PsxControllerBitBang.h: Enables bit-banging communication with the PSX controller on Arduino Leonardo, reading button states and analog stick positions.
 *    Joystick.h: Creates a virtual USB joystick on the Arduino Leonardo or Micro, simulating joystick axes, buttons, and hat switches for seamless USB joystick communication with the host computer.
 *       
 * Author: Yoganath P
 * Contact: 021BLD - 021bld.lab@gmail.com
 * 
 * Preferred Links:
 *   - GitHub: https://github.com/021bld
 *   - YouTube: https://www.youtube.com/@021_BLD
 *   - Instagram: https://www.instagram.com/021_bld
 * 
 ************************************************************/

#include <PsxControllerBitBang.h>
#include <Joystick.h>

//Using the bit-banging interface, as SPI pins are only available on the ICSP header on the Leonardo.

const unsigned long POLLING_INTERVAL = 1000U / 50U;

PsxControllerBitBang<9, 3, 10, 2> psx;

Joystick_ usbStick (
	JOYSTICK_DEFAULT_REPORT_ID,
	JOYSTICK_TYPE_MULTI_AXIS,
	12,			// buttonCount
	1,			// hatSwitchCount (0-2)
	true,		// includeXAxis
	true,		// includeYAxis
	false,		// includeZAxis
	true,		// includeRxAxis
	true,		// includeRyAxis
	false,		// includeRzAxis
	false,		// includeRudder
	false,		// includeThrottle
	false,		// includeAccelerator
	false,		// includeBrake
	false		// includeSteering
);

#ifdef ENABLE_SERIAL_DEBUG
	#define dstart(spd) do {Serial.begin (spd); while (!Serial) {digitalWrite (LED_BUILTIN, (millis () / 500) % 2);}} while (0);
	#define debug(...) Serial.print (__VA_ARGS__)
	#define debugln(...) Serial.println (__VA_ARGS__)
#else
	#define dstart(...)
	#define debug(...)
	#define debugln(...)
#endif

boolean haveController = false;

#define	toDegrees(rad) (rad * 180.0 / PI)

#define deadify(var, thres) (abs (var) > thres ? (var) : 0)

const byte ANALOG_DEAD_ZONE = 50U;

void setup () {
	// Lit the builtin led whenever buttons are pressed
	pinMode (LED_BUILTIN, OUTPUT);

	// Init Joystick library
	usbStick.begin (false);	

	// This way we can output the same range of values we get from the PSX controller
	usbStick.setXAxisRange (ANALOG_MIN_VALUE, ANALOG_MAX_VALUE);
	usbStick.setYAxisRange (ANALOG_MIN_VALUE, ANALOG_MAX_VALUE);
	usbStick.setRxAxisRange (ANALOG_MIN_VALUE, ANALOG_MAX_VALUE);
	usbStick.setRyAxisRange (ANALOG_MIN_VALUE, ANALOG_MAX_VALUE);

	dstart (115200);

	debugln (F("Ready!"));
}

void loop () {
	static unsigned long last = 0;
	
	if (millis () - last >= POLLING_INTERVAL) {
		last = millis ();
		
		if (!haveController) {
			if (psx.begin ()) {
				debugln (F("Controller found!"));
				if (!psx.enterConfigMode ()) {
					debugln (F("Cannot enter config mode"));
				} else {
					// Try to enable analog sticks
					if (!psx.enableAnalogSticks ()) {
						debugln (F("Cannot enable analog sticks"));
					}
									
					if (!psx.exitConfigMode ()) {
						debugln (F("Cannot exit config mode"));
					}
				}
				
				haveController = true;
			}
		} else {
			if (!psx.read ()) {
				debugln (F("Controller lost :("));
				haveController = false;
			} else {
				byte x, y;

				// Read was successful, so let's make up data for Joystick
				// Buttons first!
				usbStick.setButton (0, psx.buttonPressed (PSB_SQUARE));
				usbStick.setButton (1, psx.buttonPressed (PSB_CROSS));
				usbStick.setButton (2, psx.buttonPressed (PSB_CIRCLE));
				usbStick.setButton (3, psx.buttonPressed (PSB_TRIANGLE));
				usbStick.setButton (4, psx.buttonPressed (PSB_L1));
				usbStick.setButton (5, psx.buttonPressed (PSB_R1));
				usbStick.setButton (6, psx.buttonPressed (PSB_L2));
				usbStick.setButton (7, psx.buttonPressed (PSB_R2));
				usbStick.setButton (8, psx.buttonPressed (PSB_SELECT));
				usbStick.setButton (9, psx.buttonPressed (PSB_START));
				usbStick.setButton (10, psx.buttonPressed (PSB_L3));		// Only available on DualShock and later controllers
				usbStick.setButton (11, psx.buttonPressed (PSB_R3));		// Ditto

				// D-Pad makes up the X/Y axes
				if (psx.buttonPressed (PSB_PAD_UP)) {
					usbStick.setYAxis (ANALOG_MIN_VALUE);
				} else if (psx.buttonPressed (PSB_PAD_DOWN)) {
					usbStick.setYAxis (ANALOG_MAX_VALUE);
				} else {
					usbStick.setYAxis (ANALOG_IDLE_VALUE);
				}
				
				if (psx.buttonPressed (PSB_PAD_LEFT)) {
					usbStick.setXAxis (ANALOG_MIN_VALUE);
				} else if (psx.buttonPressed (PSB_PAD_RIGHT)) {
					usbStick.setXAxis (ANALOG_MAX_VALUE);
				} else {
					usbStick.setXAxis (ANALOG_IDLE_VALUE);
				}

				// Left analog gets mapped to the X/Y rotation axes
				if (psx.getLeftAnalog (x, y)) {
					usbStick.setRxAxis (x);
					usbStick.setRyAxis (y);
				}

				// Right analog is the hat switch
				if (psx.getRightAnalog (x, y)) {		// [0 ... 255]
					// We flip coordinates to avoid having to invert them in atan2()
					int8_t rx = ANALOG_IDLE_VALUE - x - 1;	// [127 ... -128]
					rx = deadify (rx, ANALOG_DEAD_ZONE);

					int8_t ry = ANALOG_IDLE_VALUE - y - 1;
					ry = deadify (ry, ANALOG_DEAD_ZONE);

					if (rx == 0 && ry == 0) {
						usbStick.setHatSwitch (0, JOYSTICK_HATSWITCH_RELEASE);
					} else {

						float angle = atan2 (ry, rx) + 2 * PI - PI / 2;
						uint16_t intAngle = ((uint16_t) (toDegrees (angle) + 0.5)) % 360;
						usbStick.setHatSwitch (0, intAngle);
					}
				}

				// All done, send data for real!
				usbStick.sendState ();
			}
		}
	}
}