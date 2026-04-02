// ESP32C3FN4 SuperMini Board
// ===============================================================
// Arduino IDE settings:
//   - Board: ESP32C3 Dev Module
//   - ESP CDC On Boot: Enabled
//   - CPU Frequency: 80MHz (WiFi)
//   - Core Debug Level: None
//   - Erase All Flash Before Sketch Upload: Disabled
//   - Flash frequency: 80Mhz
//   - Flash Mode: QIO
//   - Flash Size: 4MB (32Mb)
//   - JTAG Adapter: Disabled
//   - Partition Scheme: Huge APP (3MB No OTA/1MB SPIFFS)
//   - Upload Speed: 921600
//   - Zigbee Mode: Disabled
//   - Programmer: Esptool
// ===============================================================

/**************************************************************************************/
/*                                   Configuration                                    */

// Comment this line if you want only one switch that disables doorbell sound and
// notifications. If this line is uncommented the second switch will be added so you
// can control doorbell notifications and sound separately.
#define ENABLE_NOTIFICATIONS_SWITCH
// Comment this line if you do not use the doorbell with HomeBridge cameras.
#define USE_HOMEBRIDGE_MQTT

/**************************************************************************************/


#include <HomeSpan.h>
#ifdef USE_HOMEBRIDGE_MQTT
    #include <PubSubClient.h>
    #include <WiFi.h>
#endif


// Do not change the lines below.
    #ifndef ENABLE_NOTIFICATIONS_SWITCH
        #define DISABLE_NOTIFICATIONS_SWITCH
    #endif
    #ifndef USE_HOMEBRIDGE_MQTT
        #define USE_HOMEKIT_NOTIFICATION
    #endif
// Do not change the lines above.


/**************************************************************************************/
/*                                        Pins                                        */

// HomeSpan status LED pin.
#define STATUS_LED_PIN                  GPIO_NUM_8
// HomeSpan control button pin.
#define CONTROL_PIN                     GPIO_NUM_9

// The doorbell button input pin (radio signal)
#define BELL_BUTTON_PIN                 GPIO_NUM_3
// The doorbell signal pin (wired to the sound chip)
#define BELL_SIGNAL_PIN                 GPIO_NUM_10

/**************************************************************************************/


/**************************************************************************************/
/*                                  Firmware defines                                  */

// The doorbell button signal duration in milliseconds.
#define BELL_BUTTON_SIGNAL_DURATION     500
// The doorbell play signal duration in milliseconds.
#define BELL_SIGNAL_DURATION            250

/**************************************************************************************/


/**************************************************************************************/
/*                                   MQTT  settings                                   */

#ifdef USE_HOMEBRIDGE_MQTT
    // MQTT server port.
    const uint16_t MQTT_PORT = 1883;
    // MQTT server address.
    const char* const MQTT_SERVER = "192.168.1.13";
    // MQTT client ID.
    const char* const MQTT_CLIENT_ID = "DroneTales Doorbell";
    // MQTT server user name.
    const char* const MQTT_USER_NAME = "dronetales";
    // MQTT server password.
    const char* const MQTT_PASSWORD = "dronetales";
    // Camera UI MQTT doorbell topic.
    const char* const MQTT_DOORBELL_TOPIC = "doorcam/bell";
    // Camera UI MQTT doorbell ring message.
    const char* const MQTT_DOORBELL_MESSAGE = "RING";
#endif

/**************************************************************************************/


/**************************************************************************************/
/*                                  Global variables                                  */

// Indicates when the ring button was pressed.
static volatile bool IsRinging = false;

// The doorbell notifications state. True if the doorbell notifications are
// enabled. False otherwise. By default the notifications is enabled.
static bool NotificationsEnabled = true;
// The doorbell state. True if the doorbell is enabled and should play a sound.
// False if the doorbell is disabled and should not play any sound. By default
// the doorbell sound is enabled.
static bool DoorbellEnabled = true;

#ifdef USE_HOMEBRIDGE_MQTT
    // The WiFi client instalce.
    static WiFiClient NetClient;
    // The MQTT instance.
    static PubSubClient MqttClient(NetClient);
#endif
#ifdef USE_HOMEKIT_NOTIFICATION
    // The doorbell event object.
    static SpanCharacteristic* DoorbellEvent = nullptr;
#endif

/**************************************************************************************/


/**************************************************************************************/
/*                                  Doorbell service                                  */

#ifdef USE_HOMEKIT_NOTIFICATION
    struct Doorbell : Service::Doorbell
    {
        Doorbell() : Service::Doorbell()
        {
            DoorbellEvent = new Characteristic::ProgrammableSwitchEvent();
        }
    };
#endif

/**************************************************************************************/


/**************************************************************************************/
/*                             Virtual  Doorbell switches                             */

struct DoorbellSwitch : Service::Switch
{
    SpanCharacteristic* Switch;
    
    DoorbellSwitch() : Service::Switch()
    {
        // Default is false (bell is turned off) and we store current value in NVS.
        Switch = new Characteristic::On(false, true);

        // Get current states.
        DoorbellEnabled = Switch->getVal();
        #ifdef DISABLE_NOTIFICATIONS_SWITCH
            // If the second (notifications) switch is disabled then use the
            // same state for notifications as for doorbell sound.
            NotificationsEnabled = DoorbellEnabled;
        #endif
    }
    
    bool update()
    {
        DoorbellEnabled = Switch->getNewVal();
        #ifdef DISABLE_NOTIFICATIONS_SWITCH
            // If the second (notifications) switch is disabled then use the
            // same state for notifications as for doorbell sound.
            NotificationsEnabled = DoorbellEnabled;
        #endif

        return true;
    }
};

#ifdef ENABLE_NOTIFICATIONS_SWITCH
    struct NotificationSwitch : Service::Switch
    {
        SpanCharacteristic* Switch;
    
        NotificationSwitch() : Service::Switch()
        {
            // Default is false (bell is turned off) and we store current value in NVS.
            Switch = new Characteristic::On(false, true);
            // Get current state.
            NotificationsEnabled = Switch->getVal();
        }
    
        bool update()
        {
            NotificationsEnabled = Switch->getNewVal();
            return true;
        }
    };
#endif

/**************************************************************************************/


/**************************************************************************************/
/*                              Doorbell signal interrup                              */

void IRAM_ATTR RingInterrupt()
{
    static bool WasHigh = false;
    static uint32_t LastMillis = 0;

    if (IsRinging)
        return;

    // Read the ring signal pin.
    uint32_t Level = digitalRead(BELL_BUTTON_PIN);
    // If it is rased from LOW to HIGH...
    if (Level == HIGH && !WasHigh)
    {
        // ...then set flag and store the time when the event appeared.
        WasHigh = true;
        LastMillis = millis();
        // Exit from ISR as we need to wait when it downs from HIGH to LOW.
        return;
    }

    // If the current level is LOW but was HIGH (FALING edge detected).
    if (Level == LOW && WasHigh)
    {
        // Reset the previous state to normal (LOW).
        WasHigh = false;

        // Now calculate the pulse duration and if it looks like bell signal
        // duration set the ringing flag.
        uint32_t CurrentMillis = millis(); // We need this to be able to use unsigned values.
        IsRinging = ((CurrentMillis - LastMillis >= BELL_BUTTON_SIGNAL_DURATION));
    }
}

/**************************************************************************************/


/**************************************************************************************/
/*                                  Arduino routines                                  */

// Arduino initialization routine.
void setup()
{
    // Initialize debug serial port.
    Serial.begin(115200);
    
    // Initialize pins door bell pins.
    pinMode(BELL_SIGNAL_PIN, OUTPUT);
    pinMode(BELL_BUTTON_PIN, INPUT_PULLDOWN);
    digitalWrite(BELL_SIGNAL_PIN, LOW);

    // Initialize HomeSpan pins
    pinMode(CONTROL_PIN, INPUT);
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, HIGH);

    // Initialize HomeSpan.
    homeSpan.setControlPin(CONTROL_PIN);
    homeSpan.setStatusPin(STATUS_LED_PIN);
    homeSpan.setPairingCode("63005612");
    homeSpan.begin(Category::Bridges, "DroneTales Doorbell Bridge");
    
    // Build device's serial number.
    char Sn[24];
    snprintf(Sn, 24, "DRONETALES-%llX", ESP.getEfuseMac());

    // Configure the Bridge accessory. We do not need to add name as it is taken
    // from the begin() method.
    new SpanAccessory();
	new Service::AccessoryInformation();
	new Characteristic::Identify();
    new Characteristic::Manufacturer("DroneTales");
    new Characteristic::SerialNumber(Sn);
    new Characteristic::Model("DroneTales Doorbell");
    new Characteristic::FirmwareRevision("1.0.1.0");
    
    #ifdef USE_HOMEKIT_NOTIFICATION
        // Configure doorbell.
        new SpanAccessory();
        new Service::AccessoryInformation();
        new Characteristic::Identify();
        new Characteristic::Manufacturer("DroneTales");
        new Characteristic::SerialNumber(Sn);
        new Characteristic::Model("DroneTales Doorbell");
        new Characteristic::FirmwareRevision("1.0.1.0");
        new Characteristic::Name("Doorbell");
        new Doorbell();
    #endif

    // Configure doorbell switch.
    new SpanAccessory();
    new Service::AccessoryInformation();
    new Characteristic::Identify();
    new Characteristic::Manufacturer("DroneTales");
    new Characteristic::SerialNumber(Sn);
    new Characteristic::Model("DroneTales Doorbell");
    new Characteristic::FirmwareRevision("1.0.1.0");
    new Characteristic::Name("Doorbell Sound Switch");
    new DoorbellSwitch();

    #ifdef ENABLE_NOTIFICATIONS_SWITCH
        // Configure notification switch.
        new SpanAccessory();
        new Service::AccessoryInformation();
        new Characteristic::Identify();
        new Characteristic::Manufacturer("DroneTales");
        new Characteristic::SerialNumber(Sn);
        new Characteristic::Model("DroneTales Doorbell");
        new Characteristic::FirmwareRevision("1.0.1.0");
        new Characteristic::Name("Doorbell Notification Switch");
        new NotificationSwitch();
    #endif

    #ifdef USE_HOMEBRIDGE_MQTT
        // Configure MQTT client.
        MqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    #endif
    
    // Attached the interrupt to the ring signal pin.
    attachInterrupt(BELL_BUTTON_PIN, RingInterrupt, CHANGE);
}

// Arduino main loop.
void loop()
{
    // We need to copy the ring indicator. We need it because the IsRinging may change
    // right in the middle of the routine. So we may not process notification but process
    // only sound. So to make it working as expected we use the local copy.
    bool RingDetected = IsRinging;
    if (RingDetected)
        // Reset it only if the ring was detected. Why? Because interrupt
        // may happen right after flag copying and before the if statement.
        IsRinging = false;
    // If ring was detected (interrupt happened) after those statements then it will be
    // processed on next iteration because the IsRinging flag will be set to True.

    #ifdef USE_HOMEBRIDGE_MQTT
        // Connect to MQTT server and send an MQTT message only if a ring was detected
        // and notificatios are enabled. Of course, check the Wi-Fi network status too.
        if (RingDetected && NotificationsEnabled && WiFi.status() == WL_CONNECTED)
        {
            // If the MQTT client is not connected then try to connect.
            if (!MqttClient.connected())
                MqttClient.connect(MQTT_CLIENT_ID, MQTT_USER_NAME, MQTT_PASSWORD);
            
            // Check the MQTT connection status as it may not connect by any reason.
            if (MqttClient.connected())
            {
                // Now we can send a MQTT message about doorbell ring.
                MqttClient.publish(MQTT_DOORBELL_TOPIC, MQTT_DOORBELL_MESSAGE);
                // And disconnect from MQTT broker as we do not need to keep
                // connection active for so long time (I am sure, no one rings the bell
                // every single minute).
                MqttClient.disconnect();
            }
        }
    #endif
    #ifdef USE_HOMEKIT_NOTIFICATION
        // If we should send notification then just do it.
        if (RingDetected && NotificationsEnabled)
            DoorbellEvent->setVal(SpanButton::SINGLE);
    #endif

    // Now process the HomeSpan messages.
    homeSpan.poll();

    // If bell is ringing.
    if (RingDetected)
    {
        // And if the bell sound is enabled.
        if (DoorbellEnabled)
        {
            // Play sound.
            digitalWrite(BELL_SIGNAL_PIN, HIGH);
            delay(BELL_SIGNAL_DURATION);
            digitalWrite(BELL_SIGNAL_PIN, LOW);
        }
    }
}

/**************************************************************************************/
