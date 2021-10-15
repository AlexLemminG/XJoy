

#include "stdafx.h"
#include "Windows.h"
#include "ViGEmClient.h"
#include "hidapi.h"
#include <iostream>
#include <string>
#include <csignal>
#include <tuple>
#include <unordered_map>
#include "cpptoml.h"

#include "Bthsdpdef.h"
#include "bluetoothapis.h"

#define u8 uint8_t
#define u16 uint16_t

const unsigned short NINTENDO = 1406; // 0x057e
const unsigned short JOYCON_L = 8198; // 0x2006
const unsigned short JOYCON_R = 8199; // 0x2007
//const int XBOX_ANALOG_MIN = -32768;
const int ANALOG_MIN = 0;
const int ANALOG_MAX = 127;
//const int XBOX_ANALOG_MAX = 32767;
const int ANALOG_DIAG_MAX = round(ANALOG_MIN * 0.5 * sqrt(2.0));
const int ANALOG_DIAG_MIN = round(ANALOG_MIN * 0.5 * sqrt(2.0));

// Config file
std::shared_ptr<cpptoml::table> config;

#define DATA_BUFFER_SIZE 49
#define OUT_BUFFER_SIZE 49
u8 data[DATA_BUFFER_SIZE];
u16 stick_cal[14];
u8 global_counter[2] = { 0,0 };

PVIGEM_CLIENT client = vigem_alloc();
hid_device* left_joycon = NULL;
hid_device* right_joycon = NULL;
PVIGEM_TARGET target;
DS4_REPORT report;
DS4_REPORT wipReport;
DS4_REPORT leftReport;
DS4_REPORT rightReport;
int res;
HANDLE left_thread;
DWORD left_thread_id;
HANDLE right_thread;
DWORD right_thread_id;
HANDLE report_mutex;
USHORT left_buttons = 0;
USHORT right_buttons = 0;
float left_battery_percent = 0.f;
float right_battery_percent = 0.f;
bool kill_threads = false;

enum JOYCON_REGION {
	LEFT_DPAD,
	LEFT_ANALOG,
	LEFT_AUX,
	RIGHT_BUTTONS,
	RIGHT_ANALOG,
	RIGHT_AUX
};

enum JOYCON_BUTTON {
	L_DPAD_LEFT = 1,            // left dpad
	L_DPAD_DOWN = 2,
	L_DPAD_UP = 4,
	L_DPAD_RIGHT = 8,
	L_DPAD_SL = 16,
	L_DPAD_SR = 32,
	L_ANALOG_LEFT = 4,          // left 8-way analog
	L_ANALOG_UP_LEFT = 5,
	L_ANALOG_UP = 6,
	L_ANALOG_UP_RIGHT = 7,
	L_ANALOG_RIGHT = 0,
	L_ANALOG_DOWN_RIGHT = 1,
	L_ANALOG_DOWN = 2,
	L_ANALOG_DOWN_LEFT = 3,
	L_ANALOG_NONE = 8,
	L_SHOULDER = 64,            // left aux area
	L_TRIGGER = 128,
	L_CAPTURE = 32,
	L_MINUS = 1,
	L_STICK = 4,
	R_BUT_A = 1,                // right buttons area
	R_BUT_B = 4,
	R_BUT_Y = 8,
	R_BUT_X = 2,
	R_BUT_SL = 16,
	R_BUT_SR = 32,
	R_SHOULDER = 64,            // right aux area
	R_TRIGGER = 128,
	R_HOME = 16,
	R_PLUS = 2,
	R_STICK = 8,
	R_ANALOG_LEFT = 0,          // right 8-way analog
	R_ANALOG_UP_LEFT = 1,
	R_ANALOG_UP = 2,
	R_ANALOG_UP_RIGHT = 3,
	R_ANALOG_RIGHT = 4,
	R_ANALOG_DOWN_RIGHT = 5,
	R_ANALOG_DOWN = 6,
	R_ANALOG_DOWN_LEFT = 7,
	R_ANALOG_NONE = 8
};

std::unordered_map<JOYCON_BUTTON, DS4_BUTTONS> button_mappings;

std::tuple<JOYCON_REGION, JOYCON_BUTTON> string_to_joycon_button(std::string input) {
	if (input == "L_DPAD_LEFT") return std::make_tuple(LEFT_DPAD, L_DPAD_LEFT);
	if (input == "L_DPAD_DOWN") return std::make_tuple(LEFT_DPAD, L_DPAD_DOWN);
	if (input == "L_DPAD_UP") return std::make_tuple(LEFT_DPAD, L_DPAD_UP);
	if (input == "L_DPAD_RIGHT") return std::make_tuple(LEFT_DPAD, L_DPAD_RIGHT);
	if (input == "L_DPAD_SL") return std::make_tuple(LEFT_DPAD, L_DPAD_SL);
	if (input == "L_DPAD_SR") return std::make_tuple(LEFT_DPAD, L_DPAD_SR);
	if (input == "L_ANALOG_LEFT") return std::make_tuple(LEFT_ANALOG, L_ANALOG_LEFT);
	if (input == "L_ANALOG_UP_LEFT") return std::make_tuple(LEFT_ANALOG, L_ANALOG_UP_LEFT);
	if (input == "L_ANALOG_UP") return std::make_tuple(LEFT_ANALOG, L_ANALOG_UP);
	if (input == "L_ANALOG_UP_RIGHT") return std::make_tuple(LEFT_ANALOG, L_ANALOG_UP_RIGHT);
	if (input == "L_ANALOG_RIGHT") return std::make_tuple(LEFT_ANALOG, L_ANALOG_RIGHT);
	if (input == "L_ANALOG_DOWN_RIGHT") return std::make_tuple(LEFT_ANALOG, L_ANALOG_DOWN_RIGHT);
	if (input == "L_ANALOG_DOWN") return std::make_tuple(LEFT_ANALOG, L_ANALOG_DOWN);
	if (input == "L_ANALOG_DOWN_LEFT") return std::make_tuple(LEFT_ANALOG, L_ANALOG_DOWN_LEFT);
	if (input == "L_ANALOG_NONE") return std::make_tuple(LEFT_ANALOG, L_ANALOG_NONE);
	if (input == "L_SHOULDER") return std::make_tuple(LEFT_AUX, L_SHOULDER);
	if (input == "L_TRIGGER") return std::make_tuple(LEFT_AUX, L_TRIGGER);
	if (input == "L_CAPTURE") return std::make_tuple(LEFT_AUX, L_CAPTURE);
	if (input == "L_MINUS") return std::make_tuple(LEFT_AUX, L_MINUS);
	if (input == "L_STICK") return std::make_tuple(LEFT_AUX, L_STICK);
	if (input == "R_BUT_A") return std::make_tuple(RIGHT_BUTTONS, R_BUT_A);
	if (input == "R_BUT_B") return std::make_tuple(RIGHT_BUTTONS, R_BUT_B);
	if (input == "R_BUT_Y") return std::make_tuple(RIGHT_BUTTONS, R_BUT_Y);
	if (input == "R_BUT_X") return std::make_tuple(RIGHT_BUTTONS, R_BUT_X);
	if (input == "R_BUT_SL") return std::make_tuple(RIGHT_BUTTONS, R_BUT_SL);
	if (input == "R_BUT_SR") return std::make_tuple(RIGHT_BUTTONS, R_BUT_SR);
	if (input == "R_SHOULDER") return std::make_tuple(RIGHT_AUX, R_SHOULDER);
	if (input == "R_TRIGGER") return std::make_tuple(RIGHT_AUX, R_TRIGGER);
	if (input == "R_HOME") return std::make_tuple(RIGHT_AUX, R_HOME);
	if (input == "R_PLUS") return std::make_tuple(RIGHT_AUX, R_PLUS);
	if (input == "R_STICK") return std::make_tuple(RIGHT_AUX, R_STICK);
	if (input == "R_ANALOG_LEFT") return std::make_tuple(RIGHT_ANALOG, R_ANALOG_LEFT);
	if (input == "R_ANALOG_UP_LEFT") return std::make_tuple(RIGHT_ANALOG, R_ANALOG_UP_LEFT);
	if (input == "R_ANALOG_UP") return std::make_tuple(RIGHT_ANALOG, R_ANALOG_UP);
	if (input == "R_ANALOG_UP_RIGHT") return std::make_tuple(RIGHT_ANALOG, R_ANALOG_UP_RIGHT);
	if (input == "R_ANALOG_RIGHT") return std::make_tuple(RIGHT_ANALOG, R_ANALOG_RIGHT);
	if (input == "R_ANALOG_DOWN_RIGHT") return std::make_tuple(RIGHT_ANALOG, R_ANALOG_DOWN_RIGHT);
	if (input == "R_ANALOG_DOWN") return std::make_tuple(RIGHT_ANALOG, R_ANALOG_DOWN);
	if (input == "R_ANALOG_DOWN_LEFT") return std::make_tuple(RIGHT_ANALOG, R_ANALOG_DOWN_LEFT);
	if (input == "R_ANALOG_NONE") return std::make_tuple(RIGHT_ANALOG, R_ANALOG_NONE);
	throw "invalid joycon button: " + input;
}

bool string_to_xbox_button(std::string input, DS4_BUTTONS& button) {
	button = (DS4_BUTTONS)(1 << 32);
	if (input == "DS4_BUTTON_TRIGGER_RIGHT") button = DS4_BUTTON_TRIGGER_RIGHT;
	if (input == "DS4_BUTTON_TRIGGER_LEFT") button = DS4_BUTTON_TRIGGER_LEFT;
	if (input == "DS4_BUTTON_THUMB_LEFT") button = DS4_BUTTON_THUMB_LEFT;
	if (input == "DS4_BUTTON_THUMB_RIGHT") button = DS4_BUTTON_THUMB_RIGHT;
	if (input == "DS4_BUTTON_OPTIONS") button = DS4_BUTTON_OPTIONS;
	if (input == "DS4_BUTTON_SHARE") button = DS4_BUTTON_SHARE;
	if (input == "DS4_BUTTON_SHOULDER_LEFT") button = DS4_BUTTON_SHOULDER_LEFT;
	if (input == "DS4_BUTTON_SHOULDER_RIGHT") button = DS4_BUTTON_SHOULDER_RIGHT;
	if (input == "DS4_BUTTON_CROSS") button = DS4_BUTTON_CROSS;
	if (input == "DS4_BUTTON_CIRCLE") button = DS4_BUTTON_CIRCLE;
	if (input == "DS4_BUTTON_SQUARE") button = DS4_BUTTON_SQUARE;
	if (input == "DS4_BUTTON_TRIANGLE") button = DS4_BUTTON_TRIANGLE;
	if (button == (DS4_BUTTONS)(1 << 32))
		return false;
	return true;
	throw "invalid xbox button: " + input;
}
bool string_to_xbox_special_button(std::string input, DS4_SPECIAL_BUTTONS& button) {
	button = (DS4_SPECIAL_BUTTONS)(1 << 32);
	if (input == "DS4_SPECIAL_BUTTON_PS") button = DS4_SPECIAL_BUTTON_PS;
	if (input == "DS4_SPECIAL_BUTTON_TOUCHPAD") button = DS4_SPECIAL_BUTTON_TOUCHPAD;
	if (button == (DS4_SPECIAL_BUTTONS)(1 << 32))
		return false;
	return true;
	throw "invalid xbox button: " + input;
}
bool string_to_xbox_dpad_direction(std::string input, DS4_DPAD_DIRECTIONS& button) {
	button = DS4_BUTTON_DPAD_NONE;
	if (input == "DS4_BUTTON_DPAD_NORTH") button = DS4_BUTTON_DPAD_NORTH;
	if (input == "DS4_BUTTON_DPAD_EAST") button = DS4_BUTTON_DPAD_EAST;
	if (input == "DS4_BUTTON_DPAD_SOUTH") button = DS4_BUTTON_DPAD_SOUTH;
	if (input == "DS4_BUTTON_DPAD_WEST") button = DS4_BUTTON_DPAD_WEST;
	if (button == DS4_BUTTON_DPAD_NONE)
		return false;
	return true;
	throw "invalid xbox button: " + input;
}

std::string joycon_button_to_string(JOYCON_REGION region, JOYCON_BUTTON button) {
	switch (region) {
	case LEFT_DPAD:
		switch (button) {
		case L_DPAD_LEFT: return "L_DPAD_LEFT";
		case L_DPAD_DOWN: return "L_DPAD_DOWN";
		case L_DPAD_UP: return "L_DPAD_UP";
		case L_DPAD_RIGHT: return "L_DPAD_RIGHT";
		case L_DPAD_SL: return "L_DPAD_SL";
		case L_DPAD_SR: return "L_DPAD_SR";
		}
	case LEFT_ANALOG:
		switch (button) {
		case L_ANALOG_LEFT: return "L_ANALOG_LEFT";
		case L_ANALOG_UP_LEFT: return "L_ANALOG_UP_LEFT";
		case L_ANALOG_UP: return "L_ANALOG_UP";
		case L_ANALOG_UP_RIGHT: return "L_ANALOG_UP_RIGHT";
		case L_ANALOG_RIGHT: return "L_ANALOG_RIGHT";
		case L_ANALOG_DOWN_RIGHT: return "L_ANALOG_DOWN_RIGHT";
		case L_ANALOG_DOWN: return "L_ANALOG_DOWN";
		case L_ANALOG_DOWN_LEFT: return "L_ANALOG_DOWN_LEFT";
		case L_ANALOG_NONE: return "L_ANALOG_NONE";
		}
	case LEFT_AUX:
		switch (button) {
		case L_SHOULDER: return "L_SHOULDER";
		case L_TRIGGER: return "L_TRIGGER";
		case L_CAPTURE: return "L_CAPTURE";
		case L_MINUS: return "L_MINUS";
		case L_STICK: return "L_STICK";
		}
	case RIGHT_BUTTONS:
		switch (button) {
		case R_BUT_A: return "R_BUT_A";
		case R_BUT_B: return "R_BUT_B";
		case R_BUT_Y: return "R_BUT_Y";
		case R_BUT_X: return "R_BUT_X";
		case R_BUT_SL: return "R_BUT_SL";
		case R_BUT_SR: return "R_BUT_SR";
		}
	case RIGHT_AUX:
		switch (button) {
		case R_SHOULDER: return "R_SHOULDER";
		case R_TRIGGER: return "R_TRIGGER";
		case R_HOME: return "R_HOME";
		case R_PLUS: return "R_PLUS";
		case R_STICK: return "R_STICK";
		}
	case RIGHT_ANALOG:
		switch (button) {
		case R_ANALOG_LEFT: return "R_ANALOG_LEFT";
		case R_ANALOG_UP_LEFT: return "R_ANALOG_UP_LEFT";
		case R_ANALOG_UP: return "R_ANALOG_UP";
		case R_ANALOG_UP_RIGHT: return "R_ANALOG_UP_RIGHT";
		case R_ANALOG_RIGHT: return "R_ANALOG_RIGHT";
		case R_ANALOG_DOWN_RIGHT: return "R_ANALOG_DOWN_RIGHT";
		case R_ANALOG_DOWN: return "R_ANALOG_DOWN";
		case R_ANALOG_DOWN_LEFT: return "R_ANALOG_DOWN_LEFT";
		case R_ANALOG_NONE: return "R_ANALOG_NONE";
		}
	default:
		throw "invalid region";
	}
	throw "invalid button";
}

std::string vigem_error_to_string(VIGEM_ERROR err) {
	switch (err) {
	case VIGEM_ERROR_NONE: return "none";
	case VIGEM_ERROR_ALREADY_CONNECTED: return "already connected";
	case VIGEM_ERROR_BUS_ACCESS_FAILED: return "bus access failed";
	case VIGEM_ERROR_BUS_ALREADY_CONNECTED: return "bus already connected";
	case VIGEM_ERROR_BUS_NOT_FOUND: return "bus not found";
	case VIGEM_ERROR_BUS_VERSION_MISMATCH: return "bus version mismatch";
	case VIGEM_ERROR_CALLBACK_ALREADY_REGISTERED: return "callback already registered";
	case VIGEM_ERROR_CALLBACK_NOT_FOUND: return "callback not found";
	case VIGEM_ERROR_INVALID_TARGET: return "invalid target";
	case VIGEM_ERROR_NO_FREE_SLOT: return "no free slot";
	case VIGEM_ERROR_REMOVAL_FAILED: return "removal failed";
	case VIGEM_ERROR_TARGET_NOT_PLUGGED_IN: return "target not plugged in";
	case VIGEM_ERROR_TARGET_UNINITIALIZED: return "target uninitialized";
	default: return "none";
	}
}

void load_config() {
	try {
		config = cpptoml::parse_file("config.toml");
		std::cout << "=> Successfully loaded configuration file." << std::endl;
	}
	catch (cpptoml::parse_exception) {
		config = NULL;
		std::cerr << "=> There was an error loading the configuration file, default values will be used." << std::endl;
	}
}

DS4_BUTTONS get_button_mapping_from_config(std::string key, DS4_BUTTONS fallback) {
	if (config != NULL) {
		// Restrict the keys to the mapping section of the config
		std::string str_value = *config->get_qualified_as<std::string>("mappings." + key);
		DS4_BUTTONS button;
		if (string_to_xbox_button(str_value, button)) {
			return button;
		}
	}
	return fallback;
}


void modify_ds4_report(JOYCON_REGION region, JOYCON_BUTTON button, DS4_REPORT& report) {
	if (config != NULL) {
		std::string key = joycon_button_to_string(region, button);
		// Restrict the keys to the mapping section of the config
		std::string str_value = *config->get_qualified_as<std::string>("mappings." + key);
		DS4_BUTTONS button;
		DS4_DPAD_DIRECTIONS direction;
		DS4_SPECIAL_BUTTONS special;
		if (string_to_xbox_button(str_value, button)) {
			report.wButtons |= button;
		}
		else if (string_to_xbox_dpad_direction(str_value, direction)) {
			DS4_DPAD_DIRECTIONS reportDirection = (DS4_DPAD_DIRECTIONS)(report.wButtons & 0xf);
			switch (direction)
			{
			case DS4_BUTTON_DPAD_NORTH:
				if (reportDirection == DS4_BUTTON_DPAD_WEST) {
					direction = DS4_BUTTON_DPAD_NORTHWEST;
				}
				else if (reportDirection == DS4_BUTTON_DPAD_EAST) {
					direction = DS4_BUTTON_DPAD_NORTHEAST;
				}
				break;
			case DS4_BUTTON_DPAD_WEST:
				if (reportDirection == DS4_BUTTON_DPAD_NORTH) {
					direction = DS4_BUTTON_DPAD_NORTHWEST;
				}
				else if (reportDirection == DS4_BUTTON_DPAD_SOUTH) {
					direction = DS4_BUTTON_DPAD_SOUTHWEST;
				}
				break;
			case DS4_BUTTON_DPAD_SOUTH:
				if (reportDirection == DS4_BUTTON_DPAD_WEST) {
					direction = DS4_BUTTON_DPAD_SOUTHWEST;
				}
				else if (reportDirection == DS4_BUTTON_DPAD_EAST) {
					direction = DS4_BUTTON_DPAD_SOUTHEAST;
				}
				break;
			case DS4_BUTTON_DPAD_EAST:
				if (reportDirection == DS4_BUTTON_DPAD_NORTH) {
					direction = DS4_BUTTON_DPAD_NORTHEAST;
				}
				else if (reportDirection == DS4_BUTTON_DPAD_SOUTH) {
					direction = DS4_BUTTON_DPAD_SOUTHEAST;
				}
				break;
			default:
				break;
			}
			DS4_SET_DPAD(&report, direction);
			//report.wButtons |= direction;
		}
		else if (string_to_xbox_special_button(str_value, special)) {
			report.bSpecial |= special;
		}
	}
}

void subcomm(hid_device* joycon, u8* in, u8 len, u8 comm, u8 get_response, u8 is_left)
{
	u8 buf[OUT_BUFFER_SIZE] = { 0 };
	buf[0] = 0x1;
	buf[1] = global_counter[is_left];
	buf[2] = 0x0;
	buf[3] = 0x1;
	buf[4] = 0x40;
	buf[5] = 0x40;
	buf[6] = 0x0;
	buf[7] = 0x1;
	buf[8] = 0x40;
	buf[9] = 0x40;
	buf[10] = comm;
	for (int i = 0; i < len; ++i) {
		buf[11 + i] = in[i];
	}
	if (is_left) {
		if (global_counter[is_left] == 0xf) global_counter[is_left] = 0;
		else ++global_counter[is_left];
	}
	else {
		if (global_counter[is_left] == 0xf) global_counter[is_left] = 0;
		else ++global_counter[is_left];
	}
	//for (int i = 0; i < 15; ++i) {
	//	printf("%x ", buf[i]);
	//}
	//printf("\n");
	hid_write(joycon, buf, OUT_BUFFER_SIZE);
	if (get_response) {
		int n = hid_read_timeout(joycon, data, DATA_BUFFER_SIZE, 50);

		/*printf("response: ");
		for (int i = 0; i < 35; ++i) {
			printf("%x ", data[i]);
		}
		printf("\n");

		if (data[14] != comm) {
			printf("subcomm return fail\n");
		}
		else printf("subcomm return correct\n");
		*/
	}
}

u8* read_spi(hid_device* jc, u8 addr1, u8 addr2, int len, u8 is_left)
{
	u8 buf[] = { addr2, addr1, 0x00, 0x00, (u8)len };
	int tries = 0;
	do {
		++tries;
		subcomm(jc, buf, 5, 0x10, 1, is_left);
	} while (tries < 10 && !(data[15] == addr2 && data[16] == addr1));
	return data + 20;
}

void get_stick_cal(hid_device* jc, u8 is_left)
{
	// dump calibration data
	u8* out = read_spi(jc, 0x80, is_left ? 0x12 : 0x1d, 9, is_left);
	u8 found = 0;
	for (int i = 0; i < 9; ++i)
	{
		if (out[i] != 0xff)
		{
			// User calibration data found
			std::cout << "user cal found" << std::endl;
			found = 1;
			break;
		}
	}
	if (!found)
	{
		std::cout << "User cal not found" << std::endl;
		out = read_spi(jc, 0x60, is_left ? 0x3d : 0x46, 9, is_left);
	}
	stick_cal[is_left ? 4 : 7] = ((out[7] << 8) & 0xf00) | out[6]; // X Min below center
	stick_cal[is_left ? 5 : 8] = ((out[8] << 4) | (out[7] >> 4));  // Y Min below center
	stick_cal[is_left ? 0 : 9] = ((out[1] << 8) & 0xf00) | out[0]; // X Max above center
	stick_cal[is_left ? 1 : 10] = ((out[2] << 4) | (out[1] >> 4));  // Y Max above center
	stick_cal[is_left ? 2 : 11] = ((out[4] << 8) & 0xf00 | out[3]); // X Center
	stick_cal[is_left ? 3 : 12] = ((out[5] << 4) | (out[4] >> 4));  // Y Center
	out = read_spi(jc, 0x60, is_left ? 0x86 : 0x98, 9, is_left);
	stick_cal[is_left ? 6 : 13] = ((out[4] << 8) & 0xF00 | out[3]);			   // Deadzone
}

void setup_joycon(hid_device* jc, u8 leds, u8 is_left) {
	u8 send_buf = 0x3f;
	subcomm(jc, &send_buf, 1, 0x3, 1, is_left);
	get_stick_cal(jc, is_left);
	/*	TODO: improve bluetooth pairing
		send_buf = 0x1;
		subcomm(jc, &send_buf, 1, 0x1, 1, is_left);
		send_buf = 0x2;
		subcomm(jc, &send_buf, 1, 0x1, 1, is_left);
		send_buf = 0x3;
		subcomm(jc, &send_buf, 1, 0x1, 1, is_left);*/
	send_buf = leds;
	subcomm(jc, &send_buf, 1, 0x30, 1, is_left);
	send_buf = 0x30;
	subcomm(jc, &send_buf, 1, 0x3, 1, is_left);
	// Enable vibration
	send_buf = 0x01; // Enabled
	subcomm(jc, &send_buf, 1, 0x48, 1, is_left);
}

void initialize_left_joycon() {
	struct hid_device_info* left_joycon_info = NULL;
	while (left_joycon_info == NULL) {
		left_joycon_info = hid_enumerate(NINTENDO, JOYCON_L);
		if (left_joycon_info != NULL)
			std::cout << " => found left Joy-Con" << std::endl;
		else {
			std::cout << " => could not find left Joy-Con" << std::endl;
			Sleep(1000);
			continue;
		}
	}
	left_joycon = hid_open(NINTENDO, JOYCON_L, left_joycon_info->serial_number);
	if (left_joycon != NULL) std::cout << " => successfully connected to left Joy-Con" << std::endl;
	else {
		std::cout << " => could not connect to left Joy-Con" << std::endl;
		hid_exit();
		vigem_free(client);
		std::cout << "press [ENTER] to exit" << std::endl;
		getchar();
		exit(1);
	}

	setup_joycon(left_joycon, 0x1, 1);
}

void initialize_right_joycon() {
	struct hid_device_info* right_joycon_info = NULL;
	while (right_joycon_info == NULL) {
		right_joycon_info = hid_enumerate(NINTENDO, JOYCON_R);
		if (right_joycon_info != NULL)
			std::cout << " => found right Joy-Con" << std::endl;
		else {
			std::cout << " => could not find right Joy-Con" << std::endl;
			Sleep(1000);
			continue;
		}
	}
	right_joycon = hid_open(NINTENDO, JOYCON_R, right_joycon_info->serial_number);
	if (right_joycon != NULL) std::cout << " => successfully connected to right Joy-Con" << std::endl;
	else {
		std::cout << " => could not connect to right Joy-Con" << std::endl;
		hid_exit();
		vigem_free(client);
		std::cout << "press [ENTER] to exit" << std::endl;
		getchar();
		exit(1);
	}
	setup_joycon(right_joycon, 0x1, 0);
}

void handle_ds4_notification(
	PVIGEM_CLIENT Client,
	PVIGEM_TARGET Target,
	UCHAR LargeMotor,
	UCHAR SmallMotor,
	DS4_LIGHTBAR_COLOR LightbarColor);
void hid_exchange(hid_device* handle, unsigned char* buf, int len) {
	if (!handle) return;

	int res;

	res = hid_write(handle, buf, len);

	//if (res < 0) {
	//	printf("Number of bytes written was < 0!\n");
	//} else {
	//	printf("%d bytes written.\n", res);
	//}

	//// set non-blocking:
	//hid_set_nonblocking(handle, 1);

	res = hid_read(handle, buf, 0x40);

	//if (res < 1) {
	//	printf("Number of bytes read was < 1!\n");
	//} else {
	//	printf("%d bytes read.\n", res);
	//}
}
void send_command(hid_device* handle, int command, uint8_t* data, int len) {
	unsigned char buf[0x40];
	memset(buf, 0, 0x40);
	bool bluetooth = true;
	if (!bluetooth) {
		buf[0x00] = 0x80;
		buf[0x01] = 0x92;
		buf[0x03] = 0x31;
	}

	buf[bluetooth ? 0x0 : 0x8] = command;
	if (data != nullptr && len != 0) {
		memcpy(buf + (bluetooth ? 0x1 : 0x9), data, len);
	}

	hid_exchange(handle, buf, len + (bluetooth ? 0x1 : 0x9));

	if (data) {
		memcpy(data, buf, 0x40);
	}
}
static int global_count = 0;
void rumble(bool isLeft, int frequency, int intensity) {

	unsigned char buf[0x400];
	memset(buf, 0, 0x40);
	buf[0] = (++global_count) & 0xF;

	// intensity: (0, 8)
	// frequency: (0, 255)

	//	 X	AA	BB	 Y	CC	DD
	//[0 1 x40 x40 0 1 x40 x40] is neutral.


	//for (int j = 0; j <= 8; j++) {
	//	buf[1 + intensity] = 0x1;//(i + j) & 0xFF;
	//}

	buf[1 + 0 + intensity] = 0x1;
	buf[1 + 4 + intensity] = 0x1;

	// Set frequency to increase
	if (isLeft) {
		buf[1 + 0] = frequency;// (0, 255)
	}
	else {
		buf[1 + 4] = frequency;// (0, 255)
	}

	auto hid_handle = isLeft ? left_joycon : right_joycon;
	// set non-blocking:
	hid_set_nonblocking(hid_handle, 1);

	send_command(hid_handle, 0x10, (uint8_t*)buf, 0x9);
}
//void rumble(bool isLeft, int frequency, int intensity) {
//	static int global_count = 0;
//	unsigned char buf[0x400];
//	memset(buf, 0, 0x40);
//	global_count = intensity;
//	// intensity: (0, 8)
//	// frequency: (0, 255)
//
//	//	 X	AA	BB	 Y	CC	DD
//	//[0 1 x40 x40 0 1 x40 x40] is neutral.
//
//
//	//for (int j = 0; j <= 8; j++) {
//	//	buf[1 + intensity] = 0x1;//(i + j) & 0xFF;
//	//}
//
//
//	// Set frequency to increase
//	if (isLeft) {
//		buf[1 + 0] = frequency;// (0, 255)
//		buf[1 + 0 + 1] = intensity;
//	}
//	else {
//		buf[1 + 4] = frequency;// (0, 255)
//		buf[1 + 4 + 1] = intensity;
//	}
//	auto hid_handle = isLeft ? left_joycon : right_joycon;
//	if (hid_handle == NULL) {
//		return;
//	}
//	// set non-blocking:
//	hid_set_nonblocking(hid_handle, 1);
//
//	send_command(hid_handle, 0x10, (uint8_t*)buf, 0x9);
//	hid_set_nonblocking(hid_handle, 0);
//}

void rumble2(bool isLeft, uint16_t hf, uint8_t hfa, uint8_t lf, uint16_t lfa) {
	unsigned char buf[0x400];
	memset(buf, 0, 0x40);


	//int hf		= HF;
	//int hf_amp	= HFA;
	//int lf		= LF;
	//int lf_amp	= LFA;
	// maybe:
	//int hf_band = hf + hf_amp;

	int off = 0;// offset
	if (!isLeft) {
		off = 4;
	}


	// Byte swapping
	buf[0 + off] = hf & 0xFF;
	buf[1 + off] = hfa + ((hf >> 8) & 0xFF); //Add amp + 1st byte of frequency to amplitude byte

											 // Byte swapping
	buf[2 + off] = lf + ((lfa >> 8) & 0xFF); //Add freq + 1st byte of LF amplitude to the frequency byte
	buf[3 + off] = lfa & 0xFF;

	auto hid_handle = isLeft ? left_joycon : right_joycon;
	if (hid_handle == NULL) {
		return;
	}

	// set non-blocking:
	hid_set_nonblocking(hid_handle, 1);

	send_command(hid_handle, 0x10, (uint8_t*)buf, 0x9);
	hid_set_nonblocking(hid_handle, 0);
}

void rumble3(bool isLeft, float frequency, uint8_t hfa, uint16_t lfa) {

	//Float frequency to hex conversion
	if (frequency < 0.0f) {
		frequency = 0.0f;
	}
	else if (frequency > 1252.0f) {
		frequency = 1252.0f;
	}
	uint8_t encoded_hex_freq = (uint8_t)round(log2((double)frequency / 10.0) * 32.0);

	//uint16_t encoded_hex_freq = (uint16_t)floor(-32 * (0.693147f - log(frequency / 5)) / 0.693147f + 0.5f); // old

	//Convert to Joy-Con HF range. Range in big-endian: 0x0004-0x01FC with +0x0004 steps.
	uint16_t hf = (encoded_hex_freq - 0x60) * 4;
	//Convert to Joy-Con LF range. Range: 0x01-0x7F.
	uint8_t lf = encoded_hex_freq - 0x40;

	rumble2(isLeft, hf, hfa, lf, lfa);
}

void initialize_xbox() {
	std::cout << "initializing emulated Xbox 360 controller..." << std::endl;
	VIGEM_ERROR err = vigem_connect(client);
	if (err == VIGEM_ERROR_NONE) {
		std::cout << " => connected successfully" << std::endl;
	}
	else {
		std::cout << "connection error: " << vigem_error_to_string(err) << std::endl;
		vigem_free(client);
		std::cout << "press [ENTER] to exit" << std::endl;
		getchar();
		exit(1);
	}
	target = vigem_target_ds4_alloc();
	vigem_target_add(client, target);
	vigem_target_ds4_register_notification(client, target, handle_ds4_notification);
	DS4_REPORT_INIT(&report);
	std::cout << " => added target Xbox 360 Controller" << std::endl;
	std::cout << std::endl;
}

void disconnect_exit() {
	hid_exit();
	vigem_target_ds4_register_notification(client, target, handle_ds4_notification);
	vigem_target_remove(client, target);
	vigem_target_free(target);
	vigem_disconnect(client);
	vigem_free(client);
	exit(0);
}

void process_stick(bool is_left, uint8_t a, uint8_t b, uint8_t c) {
	u16 raw[] = { (uint16_t)(a | ((b & 0xf) << 8)), \
						 (uint16_t)((b >> 4) | (c << 4)) };
	float s[] = { 0, 0 };
	u8 offset = is_left ? 0 : 7;
	for (u8 i = 0; i < 2; ++i)
	{
		s[i] = (raw[i] - stick_cal[i + 2 + offset]);
		if (abs(s[i]) < stick_cal[6 + offset]) s[i] = 0; // inside deadzone
		else if (s[i] > 0)						// axis is above center
		{
			s[i] /= stick_cal[i + offset];
		}
		else									// axis is below center
		{
			s[i] /= stick_cal[i + 4 + offset];
		}
		if (s[i] > 1)  s[i] = 1;
		if (s[i] < -1) s[i] = -1;
		s[i] *= (ANALOG_MAX);
	}

	if (is_left) {
		//wipReport.bThumbLX = ((((USHORT)s[0]) + ((USHRT_MAX / 2) + 1)) / 257);
		wipReport.bThumbLX = (BYTE)(s[0] + 127);
		wipReport.bThumbLY = -(BYTE)(s[1] + 127);
	}
	else {
		wipReport.bThumbRX = (BYTE)(s[0] + 127);
		wipReport.bThumbRY = -(BYTE)(s[1] + 127);
	}
}

void process_button(JOYCON_REGION region, JOYCON_BUTTON button) {
	if (!((region == LEFT_ANALOG && button == L_ANALOG_NONE) || (region == RIGHT_ANALOG && button == R_ANALOG_NONE)))
		std::cout << joycon_button_to_string(region, button) << " ";
	modify_ds4_report(region, button, wipReport);
	auto got = button_mappings.find(button);
	if (got != button_mappings.end()) {
		DS4_BUTTONS target = got->second;
		switch (region) {
		case LEFT_DPAD:
		case LEFT_AUX:
			left_buttons = left_buttons | target;
			return;
		case LEFT_ANALOG:
		case RIGHT_ANALOG:
			break;
		case RIGHT_BUTTONS:
		case RIGHT_AUX:
			right_buttons = right_buttons | target;
			return;
		}
	}
	switch (region) {
	case LEFT_ANALOG:
		switch (button) {
		case L_ANALOG_DOWN:
			wipReport.bThumbLX = 0;
			wipReport.bThumbLY = ANALOG_MIN;
			break;
		case L_ANALOG_UP:
			wipReport.bThumbLX = 0;
			wipReport.bThumbLY = ANALOG_MAX;
			break;
		case L_ANALOG_LEFT:
			wipReport.bThumbLX = ANALOG_MIN;
			wipReport.bThumbLY = 0;
			break;
		case L_ANALOG_RIGHT:
			wipReport.bThumbLX = ANALOG_MAX;
			wipReport.bThumbLY = 0;
			break;
		case L_ANALOG_DOWN_LEFT:
			wipReport.bThumbLX = ANALOG_DIAG_MIN;
			wipReport.bThumbLY = ANALOG_DIAG_MIN;
			break;
		case L_ANALOG_DOWN_RIGHT:
			wipReport.bThumbLX = ANALOG_DIAG_MAX;
			wipReport.bThumbLY = ANALOG_DIAG_MIN;
			break;
		case L_ANALOG_UP_LEFT:
			wipReport.bThumbLX = ANALOG_DIAG_MIN;
			wipReport.bThumbLY = ANALOG_DIAG_MAX;
			break;
		case L_ANALOG_UP_RIGHT:
			wipReport.bThumbLX = ANALOG_DIAG_MAX;
			wipReport.bThumbLY = ANALOG_DIAG_MAX;
			break;
		case L_ANALOG_NONE:
			wipReport.bThumbLX = 0;
			wipReport.bThumbLY = 0;
			break;
		}
		break;
	case LEFT_AUX:
		switch (button) {
		case L_TRIGGER:
			wipReport.bTriggerL = 255;
			break;
		}
		break;
	case RIGHT_ANALOG:
		switch (button) {
		case R_ANALOG_DOWN:
			wipReport.bThumbRX = 0;
			wipReport.bThumbRY = ANALOG_MIN;
			break;
		case R_ANALOG_UP:
			wipReport.bThumbRX = 0;
			wipReport.bThumbRY = ANALOG_MAX;
			break;
		case R_ANALOG_LEFT:
			wipReport.bThumbRX = ANALOG_MIN;
			wipReport.bThumbRY = 0;
			break;
		case R_ANALOG_RIGHT:
			wipReport.bThumbRX = ANALOG_MAX;
			wipReport.bThumbRY = 0;
			break;
		case R_ANALOG_DOWN_LEFT:
			wipReport.bThumbRX = ANALOG_DIAG_MIN;
			wipReport.bThumbRY = ANALOG_DIAG_MIN;
			break;
		case R_ANALOG_DOWN_RIGHT:
			wipReport.bThumbRX = ANALOG_DIAG_MAX;
			wipReport.bThumbRY = ANALOG_DIAG_MIN;
			break;
		case R_ANALOG_UP_LEFT:
			wipReport.bThumbRX = ANALOG_DIAG_MIN;
			wipReport.bThumbRY = ANALOG_DIAG_MAX;
			break;
		case R_ANALOG_UP_RIGHT:
			wipReport.bThumbRX = ANALOG_DIAG_MAX;
			wipReport.bThumbRY = ANALOG_DIAG_MAX;
			break;
		case R_ANALOG_NONE:
			wipReport.bThumbRX = 0;
			wipReport.bThumbRY = 0;
			break;
		}
		break;
	case RIGHT_AUX:
		switch (button) {
		case R_TRIGGER:
			wipReport.bTriggerR = 255;
			break;
		}
		break;
	}
}

void process_buttons(JOYCON_REGION region, JOYCON_BUTTON a) {
	process_button(region, a);
}

void process_buttons(JOYCON_REGION region, JOYCON_BUTTON a, JOYCON_BUTTON b) {
	process_button(region, a);
	process_button(region, b);
}

void process_buttons(JOYCON_REGION region, JOYCON_BUTTON a, JOYCON_BUTTON b, JOYCON_BUTTON c) {
	process_button(region, a);
	process_button(region, b);
	process_button(region, c);
}

void process_buttons(JOYCON_REGION region, JOYCON_BUTTON a, JOYCON_BUTTON b, JOYCON_BUTTON c, JOYCON_BUTTON d) {
	process_button(region, a);
	process_button(region, b);
	process_button(region, c);
	process_button(region, d);
}

inline bool has_button(unsigned char data, JOYCON_BUTTON button) {
	return !!(data & button);
}

inline void region_part(unsigned char data, JOYCON_REGION region, JOYCON_BUTTON button) {
	if (has_button(data, button)) process_buttons(region, button);
}

void process_left_joycon() {
	left_buttons = 0;
	u8 offset = 0;
	u8 shift = 0;
	u8 offset2 = 0;
	if (data[0] == 0x30 || data[0] == 0x21) {
		// 0x30 input reports order the button status data differently
		// this approach is ugly, but doesn't require changing the enum
		offset = 2;
		offset2 = 3;
		shift = 1;
		process_stick(true, data[6], data[7], data[8]);
	}
	else {
		process_buttons(LEFT_ANALOG, (JOYCON_BUTTON)data[3]);
	}
	region_part(data[1 + offset * 2] << shift, LEFT_DPAD, L_DPAD_UP);
	region_part(data[1 + offset * 2] << shift, LEFT_DPAD, L_DPAD_DOWN);
	region_part(data[1 + offset * 2] >> (shift * 3), LEFT_DPAD, L_DPAD_LEFT);
	region_part(data[1 + offset * 2] << shift, LEFT_DPAD, L_DPAD_RIGHT);
	region_part(data[1 + offset * 2] >> shift, LEFT_DPAD, L_DPAD_SL);
	region_part(data[1 + offset * 2] << shift, LEFT_DPAD, L_DPAD_SR);
	region_part(data[2 + offset2], LEFT_AUX, L_TRIGGER);
	region_part(data[2 + offset2], LEFT_AUX, L_SHOULDER);
	region_part(data[2 + offset], LEFT_AUX, L_CAPTURE);
	region_part(data[2 + offset], LEFT_AUX, L_MINUS);
	region_part(data[2 + offset] >> shift, LEFT_AUX, L_STICK);
	left_battery_percent = (((data[2] & 0xF0) >> 4) * 1.f / 8);
	//wipReport.wButtons = right_buttons | left_buttons;
}

void process_right_joycon() {
	right_buttons = 0;
	u8 offset = 0;
	u8 shift = 0;
	u8 offset2 = 0;
	if (data[0] == 0x30 || data[0] == 0x21) {
		// 0x30 input reports order the button status data differently
		// this approach is ugly, but doesn't require changing the enum
		offset = 2;
		offset2 = 1;
		shift = 1;
		process_stick(false, data[9], data[10], data[11]);
	}
	else process_buttons(RIGHT_ANALOG, (JOYCON_BUTTON)data[3]);
	region_part(data[1 + offset] >> (shift * 3), RIGHT_BUTTONS, R_BUT_A);
	region_part(data[1 + offset], RIGHT_BUTTONS, R_BUT_B);
	region_part(data[1 + offset], RIGHT_BUTTONS, R_BUT_X);
	region_part(data[1 + offset] << (shift * 3), RIGHT_BUTTONS, R_BUT_Y);
	region_part(data[1 + offset] >> shift, RIGHT_BUTTONS, R_BUT_SL);
	region_part(data[1 + offset] << shift, RIGHT_BUTTONS, R_BUT_SR);
	region_part(data[2 + offset2], RIGHT_AUX, R_TRIGGER);
	region_part(data[2 + offset2], RIGHT_AUX, R_SHOULDER);
	region_part(data[2 + offset], RIGHT_AUX, R_HOME);
	region_part(data[2 + offset], RIGHT_AUX, R_PLUS);
	region_part(data[2 + offset] << shift, RIGHT_AUX, R_STICK);
	right_battery_percent = (((data[2] & 0xF0) >> 4) * 1.f / 8);
}

void joycon_cleanup(hid_device* jc, u8 is_left)
{
	u8 send_buf = 0x3f;
	subcomm(jc, &send_buf, 1, 0x3, 1, is_left);
}

DS4_REPORT merge_reports(DS4_REPORT a, DS4_REPORT b) {
	DS4_REPORT report;

	int zeroPos = 128;

	report.bThumbLX = a.bThumbLX != zeroPos ? a.bThumbLX : b.bThumbLX;
	report.bThumbLY = a.bThumbLY != zeroPos ? a.bThumbLY : b.bThumbLY;
	report.bThumbRX = a.bThumbRX != zeroPos ? a.bThumbRX : b.bThumbRX;
	report.bThumbRY = a.bThumbRY != zeroPos ? a.bThumbRY : b.bThumbRY;
	report.wButtons = a.wButtons | b.wButtons;
	report.bSpecial = a.bSpecial | b.bSpecial;
	report.bTriggerL = a.bTriggerL | b.bTriggerL;
	report.bTriggerR = a.bTriggerR | b.bTriggerR;

	if ((a.wButtons & 0xf) != DS4_BUTTON_DPAD_NONE) {
		DS4_SET_DPAD(&report, (DS4_DPAD_DIRECTIONS)(a.wButtons & 0xf));
	}
	else {
		DS4_SET_DPAD(&report, (DS4_DPAD_DIRECTIONS)(b.wButtons & 0xf));
	}
	//if ((report.wButtons & 0xF) != DS4_BUTTON_DPAD_NONE) {
	//	report.wButtons = report.wButtons & (-1 ^ DS4_BUTTON_DPAD_NONE);
	//}
	return report;
}

bool leftInitialized = false;
bool rightInitialized = false;
DWORD WINAPI left_joycon_thread(__in LPVOID lpParameter) {
	WaitForSingleObject(report_mutex, INFINITE);
	std::cout << " => left Joy-Con thread started" << std::endl;
	initialize_left_joycon();
	ReleaseMutex(report_mutex);
	leftInitialized = true;
	for (;;) {
		if (kill_threads) break;
		hid_read(left_joycon, data, DATA_BUFFER_SIZE);
		WaitForSingleObject(report_mutex, INFINITE);
		wipReport = DS4_REPORT();
		DS4_REPORT_INIT(&wipReport);
		process_left_joycon();
		leftReport = wipReport;
		report = merge_reports(leftReport, rightReport);
		vigem_target_ds4_update(client, target, report);
		ReleaseMutex(report_mutex);
	}
	leftInitialized = false;
	joycon_cleanup(left_joycon, 1);
	return 0;
}
DWORD WINAPI right_joycon_thread(__in LPVOID lpParameter) {
	WaitForSingleObject(report_mutex, INFINITE);
	std::cout << " => right Joy-Con thread started" << std::endl;
	initialize_right_joycon();
	ReleaseMutex(report_mutex);
	rightInitialized = true;
	for (;;) {
		if (kill_threads) break;
		hid_read(right_joycon, data, DATA_BUFFER_SIZE);
		WaitForSingleObject(report_mutex, INFINITE);
		wipReport = DS4_REPORT();
		DS4_REPORT_INIT(&wipReport);
		process_right_joycon();
		rightReport = wipReport;
		report = merge_reports(leftReport, rightReport);
		vigem_target_ds4_update(client, target, report);
		ReleaseMutex(report_mutex);

		std::wstring titleText;

		titleText =
			L"LeftBattery: " + std::to_wstring(std::lroundf(left_battery_percent * 100)) + L"%" +
			L" RightBattery: " + std::to_wstring(std::lroundf(right_battery_percent * 100)) + L"%";
		SetConsoleTitle(titleText.c_str());
	}
	rightInitialized = false;
	joycon_cleanup(right_joycon, 0);
	return 0;
}

void terminate() {
	kill_threads = true;
	Sleep(10);
	TerminateThread(left_thread, 0);
	TerminateThread(right_thread, 0);
	std::cout << "disconnecting and exiting..." << std::endl;
	disconnect_exit();
}

void exit_handler(int signum) {
	terminate();
	exit(signum);
}

void FindDevices() {
	bool foundLeft = false;
	bool foundRight = false;
	while (!foundLeft || !foundRight) {
		std::cout << "Searching for joycons to connect" << std::endl;
		BLUETOOTH_DEVICE_SEARCH_PARAMS params{};
		params.dwSize = sizeof(params);
		params.fIssueInquiry = true;
		params.fReturnConnected = false;
		params.fReturnAuthenticated = false;
		params.fReturnRemembered = true;
		params.fReturnUnknown = true;
		params.cTimeoutMultiplier = 1;
		BLUETOOTH_DEVICE_INFO info{};
		info.dwSize = sizeof(info);
		auto handle = BluetoothFindFirstDevice(&params, &info);
		if (handle == NULL) {
			HRESULT error = GetLastError();
			if (error != 0x103) {
				std::cout << "BluetoothFindFirstDevice failed" << std::endl;
				return;
			}
		}
		else {
			do {
				bool isLeft = false;
				if (std::wstring(info.szName) == L"Joy-Con (L)") {
					foundLeft = true;
					isLeft = true;
				}
				else if (std::wstring(info.szName) == L"Joy-Con (R)") {
					foundRight = true;
				}
				else {
					continue;
				}
				if (!info.fConnected) {
					HRESULT authResult;
					authResult = BluetoothAuthenticateDeviceEx(NULL, NULL, &info, NULL, AUTHENTICATION_REQUIREMENTS::MITMProtectionNotRequiredGeneralBonding);
					if (authResult != 0) {
						std::cout << L"Failed to auth to \'" << std::wstring(info.szName).c_str() << L"\' result: <<(int)authResult" << std::endl;
						BluetoothRemoveDevice(&info.Address);
						BluetoothUpdateDeviceRecord(&info);
						info.fAuthenticated = 0;
						authResult = BluetoothAuthenticateDeviceEx(NULL, NULL, &info, NULL, AUTHENTICATION_REQUIREMENTS::MITMProtectionNotRequiredGeneralBonding);
					}
					if (authResult != 0) {
						if (isLeft) {
							foundLeft = false;
						}
						else {
							foundRight = false;
						}
					}
				}
			} while (BluetoothFindNextDevice(handle, &info));
			BluetoothFindDeviceClose(handle);
		}
		if (!foundLeft || !foundRight) {
			if (!foundLeft) {
				std::cout << "Left joycon not found ";
			}
			if (!foundRight) {
				std::cout << "Right joycon not found";
			}
			std::cout << std::endl;
			Sleep(1000);
		}
	}
}

void handle_ds4_notification(
	PVIGEM_CLIENT Client,
	PVIGEM_TARGET Target,
	UCHAR LargeMotor,
	UCHAR SmallMotor,
	DS4_LIGHTBAR_COLOR LightbarColor) {
	rumble3(false, 25, 255 - SmallMotor, 255 - LargeMotor);
	rumble3(true, 25, 255 - SmallMotor, 255 - LargeMotor);
	//rumble(false, 1, 255 - (255 - 64) * SmallMotor / 255);
	//rumble(true, 1, 255 - (255 - 64) * SmallMotor / 255);
	std::cout << "LargeMotor" << (int)LargeMotor << " SmallMotor" << (int)SmallMotor << std::endl;
}

int main() {
	signal(SIGINT, exit_handler);
	std::cout << "XJoy v0.1.8" << std::endl << std::endl;
	FindDevices();

	load_config();
	initialize_xbox();
	hid_init();

	std::cout << std::endl;
	std::cout << "initializing threads..." << std::endl;
	report_mutex = CreateMutex(NULL, FALSE, NULL);
	if (report_mutex == NULL) {
		printf("CreateMutex error: %d\n", GetLastError());
		return 1;
	}
	std::cout << " => created report mutex" << std::endl;
	left_thread = CreateThread(0, 0, left_joycon_thread, 0, 0, &left_thread_id);
	right_thread = CreateThread(0, 0, right_joycon_thread, 0, 0, &right_thread_id);
	Sleep(500);
	//std::cout << std::endl;
	bool leftRumbled = false;
	bool rightRumbled = false;
	for (; true;) {
		if (leftInitialized && !leftRumbled) {
			leftRumbled = true;
			//rumble(true, 1, 1);
			//Sleep(500);
			rumble3(true, 25, 255 - 55, 255 - 100);
			Sleep(300);
			rumble3(true, 2000, 255, 255);
		}
		if (rightInitialized && !rightRumbled) {
			rightRumbled = true;
			//Sleep(500);
			//rumble(false, 1, 1);
			rumble3(false, 25, 255 - 55, 255 - 100);
			Sleep(300);
			rumble3(false, 2000, 255, 255);
		}
	}
	getchar();
	terminate();
}