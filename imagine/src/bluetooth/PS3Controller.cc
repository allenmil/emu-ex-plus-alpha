/*  This file is part of Imagine.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Imagine.  If not, see <http://www.gnu.org/licenses/> */

#define LOGTAG "PS3Ctrl"
#include "PS3Controller.hh"
#include <base/Base.hh>
#include <util/bits.h>
#include <util/algorithm.h>

using namespace IG;

extern StaticArrayList<BluetoothInputDevice*, Input::MAX_BLUETOOTH_DEVS_PER_TYPE * 2> btInputDevList;
StaticArrayList<PS3Controller*, Input::MAX_BLUETOOTH_DEVS_PER_TYPE> PS3Controller::devList;

static const uint CELL_PAD_BTN_OFFSET_DIGITAL1 = 0, CELL_PAD_BTN_OFFSET_DIGITAL2 = 1;

// CELL_PAD_BTN_OFFSET_DIGITAL1
#define CELL_PAD_CTRL_LEFT      (1 << 7)
#define CELL_PAD_CTRL_DOWN      (1 << 6)
#define CELL_PAD_CTRL_RIGHT     (1 << 5)
#define CELL_PAD_CTRL_UP        (1 << 4)
#define CELL_PAD_CTRL_START     (1 << 3)
#define CELL_PAD_CTRL_R3        (1 << 2)
#define CELL_PAD_CTRL_L3        (1 << 1)
#define CELL_PAD_CTRL_SELECT    (1 << 0)

// CELL_PAD_BTN_OFFSET_DIGITAL2
#define CELL_PAD_CTRL_SQUARE    (1 << 7)
#define CELL_PAD_CTRL_CROSS     (1 << 6)
#define CELL_PAD_CTRL_CIRCLE    (1 << 5)
#define CELL_PAD_CTRL_TRIANGLE  (1 << 4)
#define CELL_PAD_CTRL_R1        (1 << 3)
#define CELL_PAD_CTRL_L1        (1 << 2)
#define CELL_PAD_CTRL_R2        (1 << 1)
#define CELL_PAD_CTRL_L2        (1 << 0)

#define CELL_PAD_CTRL_PS        (1 << 0)

using namespace Input;
static const PackedInputAccess padDataAccess[] =
{
	{ CELL_PAD_BTN_OFFSET_DIGITAL1, CELL_PAD_CTRL_SELECT, PS3::SELECT },
	{ CELL_PAD_BTN_OFFSET_DIGITAL1, CELL_PAD_CTRL_L3, PS3::L3 },
	{ CELL_PAD_BTN_OFFSET_DIGITAL1, CELL_PAD_CTRL_R3, PS3::R3 },
	{ CELL_PAD_BTN_OFFSET_DIGITAL1, CELL_PAD_CTRL_START, PS3::START },
	{ CELL_PAD_BTN_OFFSET_DIGITAL1, CELL_PAD_CTRL_UP, PS3::UP },
	{ CELL_PAD_BTN_OFFSET_DIGITAL1, CELL_PAD_CTRL_RIGHT, PS3::RIGHT },
	{ CELL_PAD_BTN_OFFSET_DIGITAL1, CELL_PAD_CTRL_DOWN, PS3::DOWN },
	{ CELL_PAD_BTN_OFFSET_DIGITAL1, CELL_PAD_CTRL_LEFT, PS3::LEFT },

	{ CELL_PAD_BTN_OFFSET_DIGITAL2, CELL_PAD_CTRL_L2, PS3::L2 },
	{ CELL_PAD_BTN_OFFSET_DIGITAL2, CELL_PAD_CTRL_R2, PS3::R2 },
	{ CELL_PAD_BTN_OFFSET_DIGITAL2, CELL_PAD_CTRL_L1, PS3::L1 },
	{ CELL_PAD_BTN_OFFSET_DIGITAL2, CELL_PAD_CTRL_R1, PS3::R1 },
	{ CELL_PAD_BTN_OFFSET_DIGITAL2, CELL_PAD_CTRL_TRIANGLE, PS3::TRIANGLE },
	{ CELL_PAD_BTN_OFFSET_DIGITAL2, CELL_PAD_CTRL_CIRCLE, PS3::CIRCLE },
	{ CELL_PAD_BTN_OFFSET_DIGITAL2, CELL_PAD_CTRL_CROSS, PS3::CROSS },
	{ CELL_PAD_BTN_OFFSET_DIGITAL2, CELL_PAD_CTRL_SQUARE, PS3::SQUARE },

	{ CELL_PAD_BTN_OFFSET_DIGITAL2+1, CELL_PAD_CTRL_PS, PS3::PS },
};

CallResult PS3Controller::open(BluetoothAdapter &adapter)
{
	return UNSUPPORTED_OPERATION;
}

CallResult PS3Controller::open1Ctl(BluetoothAdapter &adapter, BluetoothPendingSocket &pending)
{
	ctlSock.onData() = intSock.onData() =
		[this](const char *packet, size_t size)
		{
			return dataHandler(packet, size);
		};
	ctlSock.onStatus() = intSock.onStatus() =
		[this](BluetoothSocket &sock, uint status)
		{
			return statusHandler(sock, status);
		};
	logMsg("accepting PS3 control channel");
	if(ctlSock.open(pending) != OK)
	{
		logErr("error opening control socket");
		return IO_ERROR;
	}
	return OK;
}

CallResult PS3Controller::open2Int(BluetoothAdapter &adapter, BluetoothPendingSocket &pending)
{
	logMsg("accepting PS3 interrupt channel");
	if(intSock.open(pending) != OK)
	{
		logErr("error opening interrupt socket");
		return IO_ERROR;
	}
	return OK;
}

uint PS3Controller::statusHandler(BluetoothSocket &sock, uint status)
{
	if(status == BluetoothSocket::STATUS_OPENED && &sock == (BluetoothSocket*)&ctlSock)
	{
		logMsg("opened PS3 control socket, waiting for interrupt socket");
		return 0; // don't add ctlSock to event loop
	}
	else if(status == BluetoothSocket::STATUS_OPENED && &sock == (BluetoothSocket*)&intSock)
	{
		logMsg("PS3 controller opened successfully");
		player = findFreeDevId();
		if(devList.isFull() || btInputDevList.isFull() || Input::devList.isFull())
		{
			logErr("No space left in BT input device list");
			removeFromSystem();
			delete this;
			return 1;
		}
		devList.push_back(this);
		btInputDevList.push_back(this);
		sendFeatureReport();
		devId = player;
		setJoystickAxisAsDpadBits(joystickAxisAsDpadBitsDefault());
		Input::addDevice(*this);
		Input::onInputDevChange(*this, { Input::Device::Change::ADDED });
		return BluetoothSocket::OPEN_USAGE_READ_EVENTS;
	}
	else if(status == BluetoothSocket::STATUS_CONNECT_ERROR)
	{
		logErr("PS3 controller connection error");
		Input::onInputDevChange(*this, { Input::Device::Change::CONNECT_ERROR });
		close();
		delete this;
	}
	else if(status == BluetoothSocket::STATUS_READ_ERROR)
	{
		logErr("PS3 controller read error, disconnecting");
		removeFromSystem();
		delete this;
	}
	return 1;
}

void PS3Controller::close()
{
	intSock.close();
	ctlSock.close();
}

void PS3Controller::removeFromSystem()
{
	close();
	devList.remove(this);
	if(btInputDevList.remove(this))
	{
		removeDevice(*this);
		Input::onInputDevChange(*this, { Input::Device::Change::REMOVED });
	}
}

bool PS3Controller::dataHandler(const char *packetPtr, size_t size)
{
	auto packet = (const uchar*)packetPtr;
	/*logMsg("data with size %d", (int)size);
	iterateTimes(size, i)
	{
		logger_printf(0, "0x%X ", packet[i]);
	}
	if(size)
		logger_printf(0, "\n");*/

	if(unlikely(!didSetLEDs))
	{
		setLEDs(player);
		didSetLEDs = true;
	}

	switch(packet[0])
	{
		bcase 0xA1:
		{
			const uchar *digitalBtnData = &packet[3];
			forEachInArray(padDataAccess, e)
			{
				int newState = e->updateState(prevData, digitalBtnData);
				if(newState != -1)
				{
					//logMsg("%s %s @ PS3 Pad %d", device->keyName(e->keyEvent), newState ? "pushed" : "released", player);
					Base::endIdleByUserActivity();
					Event event{player, Event::MAP_PS3PAD, (Key)e->keyEvent, newState ? PUSHED : RELEASED, 0, 0, this};
					startKeyRepeatTimer(event);
					Base::onInputEvent(Base::mainWindow(), event);
				}
			}
			memcpy(prevData, digitalBtnData, sizeof(prevData));

			const uchar *stickData = &packet[7];
			//logMsg("left: %d,%d right: %d,%d", stickData[0], stickData[1], stickData[2], stickData[3]);
			iterateTimes(4, i)
			{
				if(axisKey[i].dispatch(stickData[i], player, Event::MAP_PS3PAD, *this, Base::mainWindow()))
					Base::endIdleByUserActivity();
			}
		}
	}

	return 1;
}

static const uint HIDP_TRANSACTION_SET_REPORT = 0x50;
static const uint HIDP_DATA_HEADER_RTYPE_OUTPUT = 0x02;
static const uint HIDP_DATA_HEADER_RTYPE_FEATURE = 0x03;

void PS3Controller::sendFeatureReport()
{
	logMsg("sending feature report");
	const uchar featureReport[]
	{
		HIDP_TRANSACTION_SET_REPORT | HIDP_DATA_HEADER_RTYPE_FEATURE,
		0xf4, 0x42, 0x03, 0x00, 0x00
	};
	ctlSock.write(featureReport, sizeof(featureReport));
}

void PS3Controller::setLEDs(uint player)
{
	logMsg("setting LEDs for player %d", player);
	uchar setLEDs[] =
	{
		HIDP_TRANSACTION_SET_REPORT | HIDP_DATA_HEADER_RTYPE_OUTPUT,
		0x01,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0xff, 0x27, 0x10, 0x00, 0x32,
		0xff, 0x27, 0x10, 0x00, 0x32,
		0xff, 0x27, 0x10, 0x00, 0x32,
		0xff, 0x27, 0x10, 0x00, 0x32,
		0x00, 0x00, 0x00, 0x00, 0x00
	};
	setLEDs[11] = playerLEDs(player);
	ctlSock.write(setLEDs, sizeof(setLEDs));
}

uchar PS3Controller::playerLEDs(uint player)
{
	switch(player)
	{
		default:
		case 0: return bit(1);
		case 1: return bit(2);
		case 2: return bit(3);
		case 3: return bit(4);
		case 4: return bit(4) | bit(1);
	}
}

uint PS3Controller::findFreeDevId()
{
	uint id[5] {0};
	for(auto e : devList)
	{
		id[e->player] = 1;
	}
	forEachInArray(id, e)
	{
		if(*e == 0)
			return e_i;
	}
	logMsg("too many devices");
	return 0;
}

uint PS3Controller::joystickAxisBits()
{
	return Device::AXIS_BITS_STICK_1 | Device::AXIS_BITS_STICK_2;
}

uint PS3Controller::joystickAxisAsDpadBitsDefault()
{
	return Device::AXIS_BITS_STICK_1;
}

void PS3Controller::setJoystickAxisAsDpadBits(uint axisMask)
{
	using namespace Input;
	if(joystickAxisAsDpadBits_ == axisMask)
		return;

	joystickAxisAsDpadBits_ = axisMask;
	logMsg("mapping joystick axes for player: %d", player);
	{
		bool on = axisMask & Device::AXIS_BIT_X;
		axisKey[0].lowKey = on ? Input::PS3::LEFT : Input::PS3::LSTICK_LEFT;
		axisKey[0].highKey = on ? Input::PS3::RIGHT : Input::PS3::LSTICK_RIGHT;
	}
	{
		bool on = axisMask & Device::AXIS_BIT_Y;
		axisKey[1].lowKey = on ? Input::PS3::UP : Input::PS3::LSTICK_UP;
		axisKey[1].highKey = on ? Input::PS3::DOWN : Input::PS3::LSTICK_DOWN;
	}
	{
		bool on = axisMask & Device::AXIS_BIT_Z;
		axisKey[2].lowKey = on ? Input::PS3::LEFT : Input::PS3::RSTICK_LEFT;
		axisKey[2].highKey = on ? Input::PS3::RIGHT : Input::PS3::RSTICK_RIGHT;
	}
	{
		bool on = axisMask & Device::AXIS_BIT_RZ;
		axisKey[3].lowKey = on ? Input::PS3::UP : Input::PS3::RSTICK_UP;
		axisKey[3].highKey = on ? Input::PS3::DOWN : Input::PS3::RSTICK_DOWN;
	}
}
