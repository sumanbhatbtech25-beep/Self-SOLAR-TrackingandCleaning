// Copy this file to secrets.h and fill in your details.
// secrets.h is listed in .gitignore, so your passwords and keys never reach GitHub.
#pragma once

// Wi-Fi (2.4 GHz only: the ESP32 can't join 5 GHz networks)
#define WIFI_SSID     "your-wifi-name"
#define WIFI_PASSWORD "your-wifi-password"

// ThingSpeak LIVE channel: power, voltage, current, light, LDRs, tilt, cleanliness.
// thingspeak.com -> Channels -> your channel -> API Keys. "" = off.
#define TS_CHANNEL_ID   ""   // e.g. "2712345"
#define TS_WRITE_KEY    ""   // Write API Key
#define TS_READ_KEY     ""   // Read API Key (used by the phone page, not the ESP32)

// ThingSpeak HEALTH channel: temperatures, humidity, supply, both current sensors,
// fault count, and the test / command results in the status. "" = off.
#define TS_HEALTH_CHANNEL_ID ""
#define TS_HEALTH_WRITE_KEY  ""
#define TS_HEALTH_READ_KEY   ""

// ThingSpeak TalkBack (Apps -> TalkBack): commands and tests from the phone. "" = off.
#define TS_TALKBACK_ID  ""   // e.g. "52345"
#define TS_TALKBACK_KEY ""   // TalkBack API Key
