#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ESP_Mail_Client.h>

// =====================================================
// ESP32-S3 SAFE PINS
// =====================================================

#define GAS_SENSOR     1
#define SMOKE_SENSOR   2
#define TEMP_SENSOR    3

#define SMOKE_LED      8
#define GAS_LED        9
#define BUZZER         10
#define WIFI_LED       12

// =====================================================
// L298N MOTOR DRIVER PINS (ENA/ENB jumpered to 5V)
// =====================================================

#define IN1 4
#define IN2 5
#define IN3 6
#define IN4 7

// =====================================================
// WIFI
// =====================================================

const char* ssid = "simon";
const char* password = "SIMON12345";

// =====================================================
// DEVICE IDENTITY
// =====================================================

#define COMPONENT_ID   "COMP 1"
#define DEVICE_LOCATION "ATC HOSTEL 1, FIRST FLOOR, ROOM 106"

// =====================================================
// EMAIL CONFIG
// =====================================================

#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465

#define AUTHOR_EMAIL "patricklyatuu824@gmail.com"

// PUT YOUR REAL GMAIL APP PASSWORD
#define AUTHOR_PASSWORD "hfvu cunn mycz wfxe"

#define RECIPIENT_EMAIL "patricklyatuu824@gmail.com"

// =====================================================
// WEB SERVER
// =====================================================

AsyncWebServer server(80);

SMTPSession smtp;

// =====================================================
// MOTOR (L298N)
// =====================================================

const unsigned long gasSmokeWarmupMs = 20000;
const int calibrationSamples = 10;
const int calibrationSampleDelayMs = 200;

const int gasMargin = 400;
const int smokeMargin = 400;

int gasBaseline = 0;
int smokeBaseline = 0;

int gasThreshold = 2000;
int smokeThreshold = 2000;

const int tempFaultRawThreshold = 4090;

const int loopDelayMs = 150;

const unsigned long motorRunDurationMs = 15000;

bool motorActive = false;
unsigned long motorStartTime = 0;
bool previousAlertActive = false;

void motorRun()
{
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);

    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
}

void motorStop()
{
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);

    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
}

void calibrateGasSmokeSensors()
{
    Serial.println("CALIBRATING GAS/SMOKE BASELINE...");

    long gasSum = 0;
    long smokeSum = 0;

    for(int i = 0; i < calibrationSamples; i++)
    {
        gasSum += analogRead(GAS_SENSOR);
        smokeSum += analogRead(SMOKE_SENSOR);

        delay(calibrationSampleDelayMs);
    }

    gasBaseline = gasSum / calibrationSamples;
    smokeBaseline = smokeSum / calibrationSamples;

    gasThreshold = gasBaseline + gasMargin;
    smokeThreshold = smokeBaseline + smokeMargin;

    Serial.print("GAS BASELINE: ");
    Serial.print(gasBaseline);
    Serial.print(" | GAS THRESHOLD: ");
    Serial.println(gasThreshold);

    Serial.print("SMOKE BASELINE: ");
    Serial.print(smokeBaseline);
    Serial.print(" | SMOKE THRESHOLD: ");
    Serial.println(smokeThreshold);
}

// =====================================================
// VARIABLES
// =====================================================

float temperature = 0;

int gasValue = 0;
int smokeValue = 0;

bool gasDetected = false;
bool smokeDetected = false;
bool tempSensorFault = false;

String lastEmailState = "SAFE";

String statusText = "SYSTEM SAFE";

String tankStatus = "COOL";

String pumpStatus = "STOPPED";

String monthlyLog = "";

// =====================================================
// HTML PAGE
// =====================================================

const char index_html[] PROGMEM = R"rawliteral(

<!DOCTYPE html>
<html lang="en">

<head>

<meta charset="UTF-8">

<meta name="viewport"
content="width=device-width, initial-scale=1.0">

<title>Smart Smoke  and Gas leak Detection and Auto Shut Off and Alert System</title>

<link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css" rel="stylesheet">

<link rel="stylesheet"
href="https://cdn.jsdelivr.net/npm/bootstrap-icons@1.11.0/font/bootstrap-icons.css">

<link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;500;700&display=swap"
rel="stylesheet">

<style>

:root{
--primary-blue:#007aff;
--success-green:#34c759;
--danger-red:#ff3b30;
--warning-orange:#ff9500;
--bg-light:#f4f7f9;
}

body{
background-color:var(--bg-light);
font-family:'Outfit',sans-serif;
}

.smart-card{
border-radius:28px;
background:white;
box-shadow:0 10px 40px rgba(0,0,0,0.04);
padding:20px;
}

.tank-visual{
width:140px;
height:220px;
border:4px solid #eee;
border-radius:25px;
position:relative;
margin:0 auto;
overflow:hidden;
background:white;
}

.water-wave{
position:absolute;
bottom:0;
width:100%;
height:0%;
transition:1s;
background:blue;
}

.water-wave::before{
content:"";
position:absolute;
top:-15px;
left:0;
width:200%;
height:30px;
background:url('https://raw.githubusercontent.com/front-end-relative/water-wave-animation/main/wave.png');
background-size:50% 30px;
animation:move-wave 2s linear infinite;
opacity:0.5;
}

@keyframes move-wave{
0%{transform:translateX(0);}
100%{transform:translateX(-50%);}
}

.bi-spin{
display:inline-block;
animation:spin 1.5s linear infinite;
}

@keyframes spin{
from{transform:rotate(0deg);}
to{transform:rotate(360deg);}
}

.status-dot{
height:10px;
width:10px;
border-radius:50%;
display:inline-block;
}

.voice-btn{
border-radius:50px;
font-size:0.7rem;
padding:3px 12px;
}

.temp-big{
font-size:45px;
font-weight:bold;
}

</style>

</head>

<body>

<div class="container py-4">

<div class="row mb-4 align-items-center">

<div class="col-6">

<h2 class="fw-bold mb-0">
Smart Smoke and Gas Leak Detection
<span class="text-primary">Auto Shut Off and Alert System</span>
</h2>

<small>
<span class="status-dot bg-success"></span>
Live &middot; COMP 1 &middot; ATC Hostel 1, First Floor, Room 106
</small>

<button id="voiceBtn"
class="btn btn-outline-primary voice-btn ms-2"
onclick="toggleVoice()">

ENABLE VOICE

</button>

<br><br>

<a href="/monthly-log"
class="btn btn-dark">

DOWNLOAD MONTH LOG

</a>

</div>

<div class="col-6 text-end">

<h5 id="clock"
class="fw-bold mb-0">
--:--:--
</h5>

</div>

</div>

<div class="row g-3">

<div class="col-md-4">

<div class="smart-card text-center">

<div class="tank-visual mb-3">

<div class="water-wave"
id="wave-fill">
</div>

</div>

<div class="temp-big">
<span id="temp">0</span>°C
</div>

<small class="text-muted fw-bold">
LIVE TEMPERATURE
</small>

</div>

</div>

<div class="col-md-8">

<div class="row g-3">

<div class="col-6">

<div class="smart-card d-flex align-items-center">

<i class="bi bi-thermometer-half
text-danger fs-1 me-3"></i>

<div>

<small class="text-muted fw-bold">
TEMPERATURE STATUS
</small>

<div class="fw-bold h4 mb-0"
id="tank">

COOL

</div>

</div>

</div>

</div>

<div class="col-6">

<div id="pump-card"
class="smart-card d-flex align-items-center">

<i id="pump-icon"
class="bi bi-fan fs-1 me-3"></i>

<div>

<small class="text-muted fw-bold">
MOTOR (L298N)
</small>

<div class="fw-bold h4 mb-0"
id="pump">

STOPPED

</div>

</div>

</div>

</div>

<div class="col-12">

<div class="smart-card">

<div class="row">

<div class="col-4 text-center">

<h5>Gas</h5>

<div class="h2 fw-bold"
id="gas">
0
</div>

</div>

<div class="col-4 text-center">

<h5>Smoke</h5>

<div class="h2 fw-bold"
id="smoke">
0
</div>

</div>

<div class="col-4 text-center">

<h5>Status</h5>

<div class="fw-bold"
id="status">
SYSTEM SAFE
</div>

</div>

</div>

</div>

</div>

</div>

</div>

</div>

<script>

let voiceEnabled = false;

function toggleVoice()
{
    voiceEnabled = !voiceEnabled;

    const btn =
    document.getElementById(
    "voiceBtn");

    if(voiceEnabled)
    {
        btn.innerText =
        "DISABLE VOICE";

        btn.className =
        "btn btn-primary voice-btn ms-2";

        speak("Voice enabled");
    }
    else
    {
        btn.innerText =
        "ENABLE VOICE";

        btn.className =
        "btn btn-outline-primary voice-btn ms-2";
    }
}

function speak(text)
{
    if(!voiceEnabled) return;

    speechSynthesis.cancel();

    speechSynthesis.speak(
    new SpeechSynthesisUtterance(text));
}

setInterval(() =>
{
document.getElementById("clock")
.innerHTML =
new Date().toLocaleTimeString();
},1000);

let lastTankStatus = "";
let lastSystemStatus = "";

async function update()
{
    try
    {
        let response =
        await fetch('/data');

        let data =
        await response.json();

        document.getElementById(
        "temp").innerHTML =
        data.tempOk ?
        data.temperature.toFixed(1) :
        "--";

        document.getElementById(
        "gas").innerHTML =
        data.gas;

        document.getElementById(
        "smoke").innerHTML =
        data.smoke;

        document.getElementById(
        "status").innerHTML =
        data.status;

        document.getElementById(
        "tank").innerHTML =
        data.tank;

        document.getElementById(
        "pump").innerHTML =
        data.pump;

        let fill =
        document.getElementById(
        "wave-fill");

        if(!data.tempOk)
        {
            fill.style.height = "0%";

            fill.style.background =
            "linear-gradient(180deg,#9e9e9e,#616161)";
        }
        else
        {
            fill.style.height =
            data.temperature + "%";

            if(data.temperature < 40)
            {
                fill.style.background =
                "linear-gradient(180deg,#00d2ff,#007aff)";
            }
            else if(data.temperature < 70)
            {
                fill.style.background =
                "linear-gradient(180deg,#34c759,#00c853)";
            }
            else
            {
                fill.style.background =
                "linear-gradient(180deg,#ff3b30,#ff0000)";
            }
        }

        const pumpIcon =
        document.getElementById(
        "pump-icon");

        if(data.pump == "ROTATING")
        {
            pumpIcon.classList.add(
            "bi-spin");
        }
        else
        {
            pumpIcon.classList.remove(
            "bi-spin");
        }

        if(lastSystemStatus != data.status)
        {
            if(data.status == "GAS DETECTED")
            {
                speak(
                "Warning. Gas detected. Motor is closing the valve.");
            }
            else if(data.status == "SMOKE DETECTED")
            {
                speak(
                "Warning. Smoke detected. Motor is closing the valve.");
            }
            else if(data.status == "GAS AND SMOKE DETECTED")
            {
                speak(
                "Warning. Gas and smoke detected. Motor is closing the valve.");
            }
            else if(data.status == "SYSTEM SAFE")
            {
                speak(
                "Gas and smoke alert cleared.");
            }

            lastSystemStatus =
            data.status;
        }

        if(lastTankStatus != data.tank &&
           data.status != "GAS DETECTED" &&
           data.status != "SMOKE DETECTED" &&
           data.status != "GAS AND SMOKE DETECTED")
        {
            if(data.tank == "HOT")
            {
                speak(
                "Danger. High temperature detected.");
            }
            else if(data.tank == "WARM")
            {
                speak(
                "Temperature is increasing.");
            }
            else if(data.tank == "UNKNOWN")
            {
                speak(
                "Temperature sensor error.");
            }
            else
            {
                speak(
                "System temperature normal.");
            }

            lastTankStatus =
            data.tank;
        }
    }
    catch(error)
    {
        console.log(error);
    }
}

setInterval(update,1000);

</script>

</body>

</html>

)rawliteral";

// =====================================================
// SEND EMAIL
// =====================================================

void sendEmail(String subject, String body)
{
    SMTP_Message message;

    message.sender.name =
    "Smart Smoke and Gas Leak Detection Auto Shut Off and Alert System - " COMPONENT_ID;

    message.sender.email =
    AUTHOR_EMAIL;

    message.subject =
    subject;

    message.addRecipient(
    "User",
    RECIPIENT_EMAIL);

    message.text.content =
    body.c_str();

    Session_Config config;

    config.server.host_name =
    SMTP_HOST;

    config.server.port =
    SMTP_PORT;

    config.login.email =
    AUTHOR_EMAIL;

    config.login.password =
    AUTHOR_PASSWORD;

    config.login.user_domain = "";

    smtp.connect(&config);

    if(!MailClient.sendMail(
       &smtp,
       &message))
    {
        Serial.println(
        smtp.errorReason());
    }
    else
    {
        Serial.println(
        "EMAIL SENT");
    }
}

String getAlertState()
{
    if(gasDetected && smokeDetected)
    {
        return "GAS_AND_SMOKE";
    }

    if(gasDetected)
    {
        return "GAS_ONLY";
    }

    if(smokeDetected)
    {
        return "SMOKE_ONLY";
    }

    return "SAFE";
}

String getAlertLabel(const String &alertState)
{
    if(alertState == "GAS_AND_SMOKE")
    {
        return "GAS AND SMOKE DETECTED";
    }

    if(alertState == "GAS_ONLY")
    {
        return "GAS DETECTED";
    }

    if(alertState == "SMOKE_ONLY")
    {
        return "SMOKE DETECTED";
    }

    return "ALERT CLEARED";
}

String buildStatusEmailBody(const String &alertState)
{
    String body =
    getAlertLabel(alertState);

    body += "\n\n";

    body += "Component: ";
    body += COMPONENT_ID;
    body += "\n";

    body += "Location: ";
    body += DEVICE_LOCATION;
    body += "\n\n";

    body += "Temperature: ";
    body += String(temperature, 1);
    body += " C\n";

    body += "Temperature Status: ";
    body += tankStatus;
    body += "\n";

    body += "Gas Sensor: ";
    body += String(gasValue);

    if(gasDetected)
    {
        body += " (DETECTED)\n";
    }
    else
    {
        body += " (CLEAR)\n";
    }

    body += "Smoke Sensor: ";
    body += String(smokeValue);

    if(smokeDetected)
    {
        body += " (DETECTED)\n";
    }
    else
    {
        body += " (CLEAR)\n";
    }

    body += "Motor Status: ";
    body += pumpStatus;
    body += "\n";

    body += "System Status: ";
    body += statusText;
    body += "\n";

    body += "Valve Action: ";

    if(pumpStatus == "ROTATING")
    {
        body += "Closing gas valve now";
    }
    else
    {
        body += "Motor stopped";
    }

    return body;
}

String buildLogEntry(const String &alertState)
{
    String entry =
    "COMP: ";
    entry += COMPONENT_ID;

    entry += " | LOCATION: ";
    entry += DEVICE_LOCATION;

    entry += " | ";
    entry += getAlertLabel(alertState);

    entry += " | TEMP: ";
    entry += String(temperature, 1);
    entry += " C";

    entry += " | TEMP STATUS: ";
    entry += tankStatus;

    entry += " | GAS: ";
    entry += String(gasValue);

    if(gasDetected)
    {
        entry += " DETECTED";
    }
    else
    {
        entry += " CLEAR";
    }

    entry += " | SMOKE: ";
    entry += String(smokeValue);

    if(smokeDetected)
    {
        entry += " DETECTED";
    }
    else
    {
        entry += " CLEAR";
    }

    entry += " | MOTOR: ";
    entry += pumpStatus;

    entry += " | STATUS: ";
    entry += statusText;
    entry += "\n";

    return entry;
}

void sendStatusEmailIfNeeded()
{
    String currentEmailState =
    getAlertState();

    if(currentEmailState ==
       lastEmailState)
    {
        return;
    }

    String subject;

    if(currentEmailState == "SAFE")
    {
        subject =
        "[" COMPONENT_ID "] ALERT CLEARED - MOTOR STOPPED";
    }
    else
    {
        subject =
        "[" COMPONENT_ID "] SAFETY ALERT - ";

        subject +=
        getAlertLabel(
        currentEmailState);
    }

    sendEmail(
    subject,
    buildStatusEmailBody(
    currentEmailState));

    monthlyLog +=
    buildLogEntry(
    currentEmailState);

    lastEmailState =
    currentEmailState;
}

// =====================================================
// JSON FUNCTION
// =====================================================

String processor()
{
    String json = "{";

    json += "\"temperature\":";
    json += String(temperature, 1);
    json += ",";

    json += "\"tempOk\":";
    json += tempSensorFault ? "false" : "true";
    json += ",";

    json += "\"gas\":";
    json += String(gasValue);
    json += ",";

    json += "\"smoke\":";
    json += String(smokeValue);
    json += ",";

    json += "\"tank\":\"";
    json += tankStatus;
    json += "\",";

    json += "\"pump\":\"";
    json += pumpStatus;
    json += "\",";

    json += "\"status\":\"";
    json += statusText;
    json += "\"";

    json += "}";

    return json;
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
    Serial.begin(115200);

    delay(2000);

    Serial.println();
    Serial.println("ESP32 STARTING");

    pinMode(GAS_SENSOR, INPUT);

    pinMode(SMOKE_SENSOR, INPUT);

    pinMode(TEMP_SENSOR, INPUT);

    pinMode(SMOKE_LED, OUTPUT);

    pinMode(GAS_LED, OUTPUT);

    pinMode(BUZZER, OUTPUT);

    pinMode(WIFI_LED, OUTPUT);

    digitalWrite(SMOKE_LED, LOW);

    digitalWrite(GAS_LED, LOW);

    digitalWrite(BUZZER, LOW);

    digitalWrite(WIFI_LED, LOW);

    pinMode(IN1, OUTPUT);

    pinMode(IN2, OUTPUT);

    pinMode(IN3, OUTPUT);

    pinMode(IN4, OUTPUT);

    motorStop();

    // =================================================
    // GAS/SMOKE SENSOR WARM-UP + CALIBRATION
    // =================================================

    Serial.println("WARMING UP GAS/SMOKE SENSORS...");

    delay(gasSmokeWarmupMs);

    calibrateGasSmokeSensors();

    // =================================================
    // WIFI
    // =================================================

    WiFi.mode(WIFI_STA);

    delay(1000);

    Serial.println("CONNECTING WIFI");

    WiFi.begin(ssid, password);

    int timeout = 0;

    while (
        WiFi.status() != WL_CONNECTED
        &&
        timeout < 20
    )
    {
        delay(500);

        Serial.print(".");

        timeout++;
    }

    Serial.println();

    if(WiFi.status() ==
       WL_CONNECTED)
    {
        Serial.println(
        "WIFI CONNECTED");

        Serial.print(
        "IP ADDRESS: ");

        Serial.println(
        WiFi.localIP());

        digitalWrite(
        WIFI_LED,
        HIGH);
    }
    else
    {
        Serial.println(
        "WIFI FAILED");
    }

    // =================================================
    // WEB SERVER
    // =================================================

    server.on("/",
    HTTP_GET,
    [](AsyncWebServerRequest *request)
    {
        request->send_P(
            200,
            "text/html",
            index_html
        );
    });

    server.on("/data",
    HTTP_GET,
    [](AsyncWebServerRequest *request)
    {
        request->send(
            200,
            "application/json",
            processor()
        );
    });

    // =================================================
    // MONTHLY LOG DOWNLOAD
    // =================================================

    server.on("/monthly-log",
    HTTP_GET,
    [](AsyncWebServerRequest *request)
    {
        request->send(
        200,
        "text/plain",
        monthlyLog);
    });

    server.begin();

    Serial.println(
    "WEB SERVER STARTED");
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
    digitalWrite(
    WIFI_LED,
    WiFi.status() == WL_CONNECTED ? HIGH : LOW);

    gasValue =
    analogRead(GAS_SENSOR);

    smokeValue =
    analogRead(SMOKE_SENSOR);

    int tempRaw =
    analogRead(TEMP_SENSOR);

    temperature =
    (tempRaw * 100.0) / 4095.0;

    gasDetected =
    gasValue > gasThreshold;

    smokeDetected =
    smokeValue > smokeThreshold;

    // =================================================
    // TEMPERATURE STATUS
    // =================================================

    tempSensorFault =
    tempRaw >= tempFaultRawThreshold;

    if(tempSensorFault)
    {
        tankStatus = "UNKNOWN";
    }
    else if(temperature >= 70)
    {
        tankStatus = "HOT";
    }
    else if(temperature >= 40)
    {
        tankStatus = "WARM";
    }
    else
    {
        tankStatus = "COOL";
    }

    // =================================================
    // ALERT STATUS AND OUTPUTS
    // =================================================

    digitalWrite(
    GAS_LED,
    gasDetected ? HIGH : LOW);

    digitalWrite(
    SMOKE_LED,
    smokeDetected ? HIGH : LOW);

    bool alertActive =
    gasDetected || smokeDetected;

    if(alertActive)
    {
        digitalWrite(
        BUZZER,
        HIGH);

        if(!previousAlertActive)
        {
            motorActive = true;

            motorStartTime = millis();
        }

        if(motorActive &&
           millis() - motorStartTime >= motorRunDurationMs)
        {
            motorActive = false;
        }

        if(motorActive)
        {
            motorRun();

            pumpStatus =
            "ROTATING";
        }
        else
        {
            motorStop();

            pumpStatus =
            "STOPPED";
        }

        if(gasDetected &&
           smokeDetected)
        {
            statusText =
            "GAS AND SMOKE DETECTED";
        }
        else if(gasDetected)
        {
            statusText =
            "GAS DETECTED";
        }
        else
        {
            statusText =
            "SMOKE DETECTED";
        }
    }
    else if(tankStatus == "HOT")
    {
        digitalWrite(
        BUZZER,
        HIGH);

        motorStop();

        pumpStatus =
        "STOPPED";

        statusText =
        "HIGH TEMPERATURE";
    }
    else if(tankStatus == "UNKNOWN")
    {
        digitalWrite(
        BUZZER,
        LOW);

        motorStop();

        pumpStatus =
        "STOPPED";

        statusText =
        "TEMPERATURE SENSOR ERROR";
    }
    else
    {
        digitalWrite(
        BUZZER,
        LOW);

        motorStop();

        pumpStatus =
        "STOPPED";

        if(tankStatus == "WARM")
        {
            statusText =
            "TEMPERATURE NORMAL";
        }
        else
        {
            statusText =
            "SYSTEM SAFE";
        }
    }

    if(!alertActive)
    {
        motorActive = false;
    }

    previousAlertActive =
    alertActive;

    sendStatusEmailIfNeeded();

    Serial.print("TEMP: ");
    Serial.print(temperature, 1);

    Serial.print(" | TEMP STATUS: ");
    Serial.print(tankStatus);

    Serial.print(" | GAS: ");
    Serial.print(gasValue);

    Serial.print(" | SMOKE: ");
    Serial.print(smokeValue);

    Serial.print(" | MOTOR: ");
    Serial.print(pumpStatus);

    Serial.print(" | STATUS: ");
    Serial.println(statusText);

    delay(loopDelayMs);
}
