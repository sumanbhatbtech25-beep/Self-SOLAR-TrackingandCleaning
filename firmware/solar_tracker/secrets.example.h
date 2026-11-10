// Copy this file to secrets.h and fill in your details.
// secrets.h is listed in .gitignore, so your passwords and keys never reach GitHub.
#pragma once

// Wi-Fi (2.4 GHz only: the ESP32 can't join 5 GHz networks)
#define WIFI_SSID     "your-wifi-name"
#define WIFI_PASSWORD "your-wifi-password"

// ThingSpeak (thingspeak.com -> your channel -> API Keys). Leave as "" to turn cloud upload off.
#define TS_CHANNEL_ID   ""   // e.g. "2712345"
#define TS_WRITE_KEY    ""   // Write API Key
#define TS_READ_KEY     ""   // Read API Key (only used by the phone page)

// ThingSpeak TalkBack (Apps -> TalkBack): remote commands from the phone. "" = off.
#define TS_TALKBACK_ID  ""   // e.g. "52345"
#define TS_TALKBACK_KEY ""   // TalkBack API Key
