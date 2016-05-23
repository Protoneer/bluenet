/**
 * Author: Dominik Egger
 * Copyright: Distributed Organisms B.V. (DoBots)
 * Date: Oct 21, 2014
 * License: LGPLv3+, Apache License, or MIT, your choice
 */

#include <services/cs_IndoorLocalisationService.h>

#include <structs/buffer/cs_MasterBuffer.h>
#include <cfg/cs_UuidConfig.h>
#include <processing/cs_CommandHandler.h>
#include <structs/cs_TrackDevices.h>
#include <processing/cs_Tracker.h>

using namespace BLEpp;

IndoorLocalizationService::IndoorLocalizationService() : EventListener(),
		_rssiCharac(NULL), _peripheralCharac(NULL),
		_trackedDeviceListCharac(NULL), _trackedDeviceCharac(NULL)
{
	EventDispatcher::getInstance().addListener(this);

	setUUID(UUID(INDOORLOCALISATION_UUID));
	setName(BLE_SERVICE_INDOOR_LOCALIZATION);
	
	init();

	Timer::getInstance().createSingleShot(_appTimerId, (app_timer_timeout_handler_t)IndoorLocalizationService::staticTick);
}

void IndoorLocalizationService::init() {
	LOGi("Create indoor localization service");

#if CHAR_RSSI==1
	LOGi("add Signal Strength characteristics");
	addSignalStrengthCharacteristic();
#else
	LOGi("skip Signal Strength characteristics");
#endif
#if CHAR_SCAN_DEVICES==1
	LOGi("add Scan Devices characteristics");
	addScanControlCharacteristic();
	addPeripheralListCharacteristic();
#else
	LOGi("skip Scan/Devices characteristics");
#endif
#if CHAR_TRACK_DEVICES==1
	LOGi("add Tracked Device characteristics");
	addTrackedDeviceListCharacteristic();
	addTrackedDeviceCharacteristic();
#else
	LOGi("skip Tracked Device characteristics");
#endif

	addCharacteristicsDone();
}

void IndoorLocalizationService::tick() {
//	LOGi("Tack: %d", RTC::now());

#if CHAR_RSSI==1
#ifdef PWM_ON_RSSI
//	//! Map [-90, -40] to [0, 100]
//	int16_t pwm = (_averageRssi + 90) * 10 / 5;
//	if (pwm < 0) {
//		pwm = 0;
//	}
//	if (pwm > 100) {
//		pwm = 100;
//	}
//	PWM::getInstance().setValue(0, pwm);

	if (_averageRssi > -70 && PWM::getInstance().getValue(0) < 1) {
		PWM::getInstance().setValue(0, 255);
	}

	if (_averageRssi < -80 && PWM::getInstance().getValue(0) > 0) {
		PWM::getInstance().setValue(0, 0);
	}
#endif
#endif

	scheduleNextTick();
}

void IndoorLocalizationService::scheduleNextTick() {
	Timer::getInstance().start(_appTimerId, HZ_TO_TICKS(LOCALIZATION_SERVICE_UPDATE_FREQUENCY), this);
}

void IndoorLocalizationService::addSignalStrengthCharacteristic() {
	_rssiCharac = new Characteristic<int8_t>();
	addCharacteristic(_rssiCharac);

	_rssiCharac->setUUID(UUID(getUUID(), RSSI_UUID)); //! there is no BLE_UUID for rssi level(?)
	_rssiCharac->setName(BLE_CHAR_RSSI);
	_rssiCharac->setDefaultValue(1);
	_rssiCharac->setNotifies(true);
#ifdef PWM_ON_RSSI
	_averageRssi = -90; // Start with something..
#endif
}

void IndoorLocalizationService::addScanControlCharacteristic() {
	_scanControlCharac = new Characteristic<uint8_t>();
	addCharacteristic(_scanControlCharac);

	_scanControlCharac->setUUID(UUID(getUUID(), SCAN_DEVICE_UUID));
	_scanControlCharac->setName(BLE_CHAR_SCAN);
	_scanControlCharac->setDefaultValue(255);
	_scanControlCharac->setWritable(true);
	_scanControlCharac->onWrite([&](const uint8_t& value) -> void {
		CommandHandler::getInstance().handleCommand(CMD_SCAN_DEVICES, (buffer_ptr_t)&value, 1);
	});
}

void IndoorLocalizationService::addPeripheralListCharacteristic() {

	MasterBuffer& mb = MasterBuffer::getInstance();
	buffer_ptr_t buffer = NULL;
	uint16_t maxLength = 0;
	mb.getBuffer(buffer, maxLength);

	_peripheralCharac = new Characteristic<buffer_ptr_t>();
	addCharacteristic(_peripheralCharac);

	_peripheralCharac->setUUID(UUID(getUUID(), LIST_DEVICE_UUID));
	_peripheralCharac->setName("Devices");
	_peripheralCharac->setWritable(false);
	_peripheralCharac->setNotifies(true);
	_peripheralCharac->setValue(buffer);
	_peripheralCharac->setMaxLength(maxLength);
	_peripheralCharac->setDataLength(0);
}

void IndoorLocalizationService::addTrackedDeviceListCharacteristic() {

	MasterBuffer& mb = MasterBuffer::getInstance();
	buffer_ptr_t buffer = NULL;
	uint16_t maxLength = 0;
	mb.getBuffer(buffer, maxLength);

	_trackedDeviceListCharac = new Characteristic<buffer_ptr_t>();
	addCharacteristic(_trackedDeviceListCharac);

	_trackedDeviceListCharac->setUUID(UUID(getUUID(), TRACKED_DEVICE_LIST_UUID));
	_trackedDeviceListCharac->setName(BLE_CHAR_TRACK);
	_trackedDeviceListCharac->setWritable(false);
	_trackedDeviceListCharac->setNotifies(false);
	_trackedDeviceListCharac->setValue(buffer);
	_trackedDeviceListCharac->setMaxLength(maxLength);
	_trackedDeviceListCharac->setDataLength(0);
}

void IndoorLocalizationService::addTrackedDeviceCharacteristic() {

	MasterBuffer& mb = MasterBuffer::getInstance();
	buffer_ptr_t buffer = NULL;
	uint16_t maxLength = 0;
	mb.getBuffer(buffer, maxLength);

	_trackedDeviceCharac = new Characteristic<buffer_ptr_t>();
	addCharacteristic(_trackedDeviceCharac);

	_trackedDeviceCharac->setUUID(UUID(getUUID(), TRACKED_DEVICE_UUID));
	_trackedDeviceCharac->setName("Add tracked device");
	_trackedDeviceCharac->setWritable(true);
	_trackedDeviceCharac->setNotifies(false);

	_trackedDeviceCharac->setValue(buffer);
	_trackedDeviceCharac->setMaxLength(maxLength);
	_trackedDeviceCharac->setDataLength(0);

	_trackedDeviceCharac->onWrite([&](const buffer_ptr_t& value) -> void {
		Tracker::getInstance().handleTrackedDeviceCommand(_trackedDeviceCharac->getValue(),
				_trackedDeviceCharac->getValueLength());
	});
}

void IndoorLocalizationService::on_ble_event(ble_evt_t * p_ble_evt) {

	Service::on_ble_event(p_ble_evt);

	switch (p_ble_evt->header.evt_id) {
#if CHAR_RSSI==1
	case BLE_GAP_EVT_CONNECTED: {

#if (SOFTDEVICE_SERIES == 130 && SOFTDEVICE_MAJOR == 1 && SOFTDEVICE_MINOR == 0) || \
	(SOFTDEVICE_SERIES == 110 && SOFTDEVICE_MAJOR == 8)
		sd_ble_gap_rssi_start(p_ble_evt->evt.gap_evt.conn_handle, 0, 0);
#else
		sd_ble_gap_rssi_start(p_ble_evt->evt.gap_evt.conn_handle);
#endif
		break;
	}
	case BLE_GAP_EVT_DISCONNECTED: {
		sd_ble_gap_rssi_stop(p_ble_evt->evt.gap_evt.conn_handle);
		break;
	}
	case BLE_GAP_EVT_RSSI_CHANGED: {
		onRSSIChanged(p_ble_evt->evt.gap_evt.params.rssi_changed.rssi);
		break;
	}
#endif

	default: {
	}
	}
}

void IndoorLocalizationService::onRSSIChanged(int8_t rssi) {

#ifdef RGB_LED
	//! set LED here
	int sine_index = (rssi - 170) * 2;
	if (sine_index < 0) sine_index = 0;
	if (sine_index > 100) sine_index = 100;
	//			__asm("BKPT");
	//			int sine_index = (rssi % 10) *10;
	PWM::getInstance().setValue(0, sin_table[sine_index]);
	PWM::getInstance().setValue(1, sin_table[(sine_index + 33) % 100]);
	PWM::getInstance().setValue(2, sin_table[(sine_index + 66) % 100]);
	//			counter = (counter + 1) % 100;

	//! Add a delay to control the speed of the sine wave
	nrf_delay_us(8000);
#endif

	setRSSILevel(rssi);
}

void IndoorLocalizationService::setRSSILevel(int8_t RSSILevel) {
#ifdef MICRO_VIEW
	//! Update rssi at the display
	write("2 %i\r\n", RSSILevel);
#endif
	if (_rssiCharac) {
		*_rssiCharac = RSSILevel;
#ifdef PWM_ON_RSSI
		//! avg = 0.4*rssi + (1-0.4)*avg
		_averageRssi = (RSSILevel*4 + _averageRssi*6) / 10;
		LOGd("RSSI: %d avg: %d", RSSILevel, _averageRssi);
#endif
	}
}

void IndoorLocalizationService::handleEvent(uint16_t evt, void* p_data, uint16_t length) {
	switch(evt) {
	case EVT_SCANNED_DEVICES: {
		_peripheralCharac->setValue((buffer_ptr_t)p_data);
		_peripheralCharac->setDataLength(length);
		_peripheralCharac->notify();
		break;
	}
	case EVT_TRACKED_DEVICES: {
		_trackedDeviceListCharac->setValue((buffer_ptr_t)p_data);
		_trackedDeviceListCharac->setDataLength(length);
		_trackedDeviceListCharac->notify();
		break;
	}
	}
}
