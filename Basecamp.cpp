/*
   Basecamp - ESP32 library to simplify the basics of IoT projects
   Written by Merlin Schumacher (mls@ct.de) for c't magazin für computer technik (https://www.ct.de)
   Licensed under GPLv3. See LICENSE for details.
   */

#include <iomanip>
#include "Basecamp.hpp"
#include "debug.hpp"

namespace {
	const constexpr uint16_t defaultThreadStackSize = 3072;
	const constexpr UBaseType_t defaultThreadPriority = 0;
	// Default length for access point mode password
	const constexpr unsigned defaultApSecretLength = 8;
}

Basecamp::Basecamp(SetupModeWifiEncryption setupModeWifiEncryption, ConfigurationUI configurationUi)
	: configuration(String{"/basecamp.json"})
	, setupModeWifiEncryption_(setupModeWifiEncryption)
	, configurationUi_(configurationUi)
{
}

/**
 * This function generates a cleaned string from the device name set by the user.
 */
String Basecamp::_cleanHostname()
{
	String pixelTubeNumber = configuration.get("pixelTubeNumber");

	// If hostname is not set, return default
	if (pixelTubeNumber == "") {
		return "pixel-tube-unconfigured";
	}

	return "pixel-tube-" + pixelTubeNumber;
};

/**
 * Returns true if a secret for the setup WiFi AP is set
 */
bool Basecamp::isSetupModeWifiEncrypted(){
	return (setupModeWifiEncryption_ == SetupModeWifiEncryption::secured);
}

/**
 * Returns the SSID of the setup WiFi network
 */
String Basecamp::getSetupModeWifiName(){
	return wifi.getAPName();
}

/**
 * Returns the secret of the setup WiFi network
 */
String Basecamp::getSetupModeWifiSecret(){
	return configuration.get(ConfigurationKey::accessPointSecret);
}

/**
 * This is the initialisation function for the Basecamp class.
 */
bool Basecamp::begin(String fixedWiFiApEncryptionPassword)
{
	// Make sure we only accept valid passwords for ap
	if (fixedWiFiApEncryptionPassword.length() != 0) {
		if (fixedWiFiApEncryptionPassword.length() >= wifi.getMinimumSecretLength()) {
			setupModeWifiEncryption_ = SetupModeWifiEncryption::secured;
		} else {
			Serial.println("Error: Given fixed ap secret is too short. Refusing.");
		}
	}

	// Enable serial output
	Serial.begin(115200);
	// Display a simple lifesign
	Serial.println("");
	Serial.println("Basecamp Startup");

	// Load configuration from internal flash storage.
	// If configuration.load() fails, reset the configuration
	if (!configuration.load()) {
		DEBUG_PRINTLN("Configuration is broken. Resetting.");
		configuration.reset();
	};

	// Get a cleaned version of the device name.
	// It is used as a hostname for DHCP.
	hostname = _cleanHostname();
	DEBUG_PRINTLN(hostname);

	// Have checkResetReason() control if the device configuration
	// should be reset or not.
	checkResetReason();

#ifndef BASECAMP_NOWIFI

	// If there is no access point secret set yet, generate one and save it.
	// It will survive the default config reset.
	if (!configuration.isKeySet(ConfigurationKey::accessPointSecret) ||
		fixedWiFiApEncryptionPassword.length() >= wifi.getMinimumSecretLength())
	{
		String apSecret = fixedWiFiApEncryptionPassword;
		if (apSecret.length() < wifi.getMinimumSecretLength()) {
			// Not set or too short. Generate a random one.
			Serial.println("Generating access point secret.");
			apSecret = wifi.generateRandomSecret(defaultApSecretLength);
		} else {
			Serial.println("Using fixed access point secret.");
		}
		configuration.set(ConfigurationKey::accessPointSecret, apSecret);
		configuration.save();
	}

	DEBUG_PRINTF("Secret: %s\n", configuration.get(ConfigurationKey::accessPointSecret).c_str());

	pixelTubeNumber = configuration.get("pixelTubeNumber").toInt();
	artNetUniverse = configuration.get("artNetUniverse").toInt();
	artNetStartAddress = configuration.get("artNetStartAddress").toInt();

	// Initialize Wifi with the stored configuration data.
	wifi.begin(
			configuration.get("WifiEssid"), // The (E)SSID or WiFi-Name
			configuration.get("WifiPassword"), // The WiFi password
			configuration.get("WifiConfigured"), // Has the WiFi been configured
			pixelTubeNumber,
			hostname, // The system hostname to use for DHCP
			(setupModeWifiEncryption_ == SetupModeWifiEncryption::none)?"":configuration.get(ConfigurationKey::accessPointSecret)
	);

	// Get WiFi MAC
	mac = wifi.getSoftwareMacAddress(":");
#endif

#ifndef BASECAMP_NOWEB
	if (shouldEnableConfigWebserver())
	{
		String deviceName = (pixelTubeNumber == 0) ? "Unconfigured Pixel Tube" : "Pixel Tube " + String(pixelTubeNumber);
		web.addInterfaceElement("title", "title", deviceName, "head");
		web.addInterfaceElement("devicename", "span", deviceName, "#heading");

		web.addInterfaceElement("configform", "form", "", "#wrapper");
		web.setInterfaceElementAttribute("configform", "action", "#");
		web.setInterfaceElementAttribute("configform", "onsubmit", "collectConfiguration()");

		web.addInterfaceElement("pixelTubeNumber", "input", "Pixel Tube number (1 to 99):", "#configform", "pixelTubeNumber");
		web.setInterfaceElementAttribute("pixelTubeNumber", "type", "number");
		web.setInterfaceElementAttribute("pixelTubeNumber", "min", "1");
		web.setInterfaceElementAttribute("pixelTubeNumber", "max", "99");
		web.setInterfaceElementAttribute("pixelTubeNumber", "step", "1");

		web.addInterfaceElement("artNetUniverse", "input", "Art-Net Universe (0 to 32767):", "#configform", "artNetUniverse");
		web.setInterfaceElementAttribute("artNetUniverse", "type", "number");
		web.setInterfaceElementAttribute("artNetUniverse", "min", "0");
		web.setInterfaceElementAttribute("artNetUniverse", "max", "32767");
		web.setInterfaceElementAttribute("artNetUniverse", "step", "1");

		web.addInterfaceElement("artNetStartAddress", "input", "Art-Net Start Address (1 to 387, one pixel tube occupies 125 channels):", "#configform", "artNetStartAddress");
		web.setInterfaceElementAttribute("artNetStartAddress", "type", "number");
		web.setInterfaceElementAttribute("artNetStartAddress", "min", "1");
		web.setInterfaceElementAttribute("artNetStartAddress", "max", "387");
		web.setInterfaceElementAttribute("artNetStartAddress", "step", "1");

		web.addInterfaceElement("WifiEssid", "input", "WIFI SSID:", "#configform", "WifiEssid");

		web.addInterfaceElement("WifiPassword", "input", "WIFI Password:", "#configform", "WifiPassword");
		web.setInterfaceElementAttribute("WifiPassword", "type", "password");

		web.addInterfaceElement("WifiConfigured", "input", "", "#configform", "WifiConfigured");
		web.setInterfaceElementAttribute("WifiConfigured", "type", "hidden");
		web.setInterfaceElementAttribute("WifiConfigured", "value", "true");

		web.addInterfaceElement("saveform", "button", "Save", "#configform");
		web.setInterfaceElementAttribute("saveform", "type", "submit");

		String macAddressHint = "This device has MAC-Address " + mac + ".";
		web.addInterfaceElement("macAddressHint", "p", macAddressHint, "#wrapper");


		if (configuration.get("WifiConfigured") != "True") {
			dnsServer.start(53, "*", wifi.getSoftAPIP());
			xTaskCreatePinnedToCore(&DnsHandling, "DNSTask", 4096, (void*) &dnsServer, 5, NULL,0);
		}

		// Start webserver and pass the configuration object to it
		// Also pass a Lambda-function that restarts the device after the configuration has been saved.
		web.begin(configuration, [](){
			delay(2000);
			ESP.restart();
		});
	}
	#endif
	Serial.println(showSystemInfo());

	// TODO: only return true if everything setup up correctly
	return true;
}

/**
 * This is the background task function for the Basecamp class. To be called from Arduino loop.
 */
void Basecamp::handle (void)
{
}

bool Basecamp::shouldEnableConfigWebserver() const
{
	return (configurationUi_ == ConfigurationUI::always ||
		(configurationUi_ == ConfigurationUI::accessPoint && wifi.getOperationMode() == WifiControl::Mode::accessPoint));
}


// This is a task that handles DNS requests from clients
void Basecamp::DnsHandling(void * dnsServerPointer)
{
		DNSServer * dnsServer = (DNSServer *) dnsServerPointer;
		while(1) {
			// handle each request
			dnsServer->processNextRequest();
			vTaskDelay(1000);
		}
}

// This function checks the reset reason returned by the ESP and resets the configuration if neccessary.
// It counts all system reboots that occured by power cycles or button resets.
// If the ESP32 receives an IP the boot counts as successful and the counter will be reset by Basecamps
// WiFi management.
void Basecamp::checkResetReason()
{
	// Instead of the internal flash it uses the somewhat limited, but sufficient preferences storage
	preferences.begin("basecamp", false);
	// Get the reset reason for the current boot
	int reason = rtc_get_reset_reason(0);
	DEBUG_PRINT("Reset reason: ");
	DEBUG_PRINTLN(reason);
	// If the reason is caused by a power cycle (1) or a RTC reset / button press(16) evaluate the current
	// bootcount and act accordingly.
	if (reason == 1 || reason == 16) {
		// Get the current number of unsuccessful boots stored
		unsigned int bootCounter = preferences.getUInt("bootcounter", 0);
		// increment it
		bootCounter++;
		DEBUG_PRINT("Unsuccessful boots: ");
		DEBUG_PRINTLN(bootCounter);

		// If the counter is bigger than 3 it will be the fifths consecutive unsucessful reboot.
		// This forces a reset of the WiFi configuration and the AP will be opened again
		if (bootCounter > 3){
			DEBUG_PRINTLN("Configuration forcibly reset.");
			// Mark the WiFi configuration as invalid
			configuration.set("WifiConfigured", "False");
			// Save the configuration immediately
			configuration.save();
			// Reset the boot counter
			preferences.clear();
			// Call the destructor for preferences so that all data is safely stored befor rebooting
			preferences.end();
			Serial.println("Resetting the WiFi configuration.");
			// Reboot
			ESP.restart();

			// If the WiFi is unconfigured and the device is rebooted twice format the internal flash storage
		} else if (bootCounter > 2 && configuration.get("WifiConfigured") == "False") {
			Serial.println("Factory reset was forced.");
			// Format the flash storage
			SPIFFS.format();
			// Reset the boot counter
			preferences.clear();
			// Call the destructor for preferences so that all data is safely stored befor rebooting
			preferences.end();
			Serial.println("Rebooting.");
			// Reboot
			ESP.restart();

		// In every other case: store the current boot count
		} else {
			preferences.putUInt("bootcounter", bootCounter);
		};

	// if the reset has been for any other cause, reset the counter
	} else {
		preferences.clear();
	};
	// Call the destructor for preferences so that all data is safely stored
	preferences.end();
};

// This shows basic information about the system. Currently only the mac
// TODO: expand infos
String Basecamp::showSystemInfo() {
	std::ostringstream info;
	info << "MAC-Address: " << mac.c_str();
	info << ", Hardware MAC: " << wifi.getHardwareMacAddress(":").c_str() << std::endl;

	if (configuration.isKeySet(ConfigurationKey::accessPointSecret)) {
			info << "*******************************************" << std::endl;
			info << "* ACCESS POINT PASSWORD: ";
			info << configuration.get(ConfigurationKey::accessPointSecret).c_str() << std::endl;
			info << "*******************************************" << std::endl;
	}

	return {info.str().c_str()};
}

