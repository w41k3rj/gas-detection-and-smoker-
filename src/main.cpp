#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ESP_Mail_Client.h>
#include <Stepper.h>

// =====================================================
// SENSOR PINS
// =====================================================

#define GAS_SENSOR 34
#define SMOKE_SENSOR 35
#define TEMP_SENSOR 32

// =====================================================
// OUTPUT PINS
// =====================================================

#define RED_LED 25
#define GREEN_LED 26
#define BUZZER 27

// =====================================================
// STEPPER MOTOR PINS
// =====================================================

#define IN1 14
#define IN2 12
#define IN3 13
#define IN4 15

// =====================================================
// WIFI
// =====================================================

const char* ssid = "simon";
const char* password = "simon1234";

// =====================================================
// EMAIL CONFIG
// =====================================================

#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465

#define AUTHOR_EMAIL "rustusjunior@gmail.com"
#define AUTHOR_PASSWORD "YOUR_GMAIL_APP_PASSWORD"

#define RECIPIENT_EMAIL "rustusjunior@gmail.com"

// =====================================================
// SERVER
// =====================================================

AsyncWebServer server(80);
SMTPSession smtp;

// =====================================================
// STEPPER
// =====================================================

const int stepsPerRevolution = 2048;

Stepper myStepper(stepsPerRevolution, IN1, IN3, IN2, IN4);

// =====================================================
// VARIABLES
// =====================================================

float temperature = 0;

int gasValue = 0;
int smokeValue = 0;

bool gasDetected = false;
bool smokeDetected = false;

bool alertSent = false;

String statusText = "SYSTEM SAFE";

// =====================================================
// SEND EMAIL
// =====================================================

void sendEmail(String subject, String message)
{
    SMTP_Message msg;

    msg.sender.name = "ESP32 Safety System";
    msg.sender.email = AUTHOR_EMAIL;

    msg.subject = subject;

    msg.addRecipient("User", RECIPIENT_EMAIL);

    msg.text.content = message.c_str();

    Session_Config config;

    config.server.host_name = SMTP_HOST;
    config.server.port = SMTP_PORT;

    config.login.email = AUTHOR_EMAIL;
    config.login.password = AUTHOR_PASSWORD;

    config.login.user_domain = "";

    smtp.connect(&config);

    if (!MailClient.sendMail(&smtp, &msg))
    {
        Serial.println("EMAIL FAILED");
    }
    else
    {
        Serial.println("EMAIL SENT");
    }
}

// =====================================================
// JSON DATA
// =====================================================

String processor()
{
    String json = "{";

    json += "\"temperature\":";
    json += String(temperature);
    json += ",";

    json += "\"gas\":";
    json += String(gasValue);
    json += ",";

    json += "\"smoke\":";
    json += String(smokeValue);
    json += ",";

    json += "\"status\":\"";
    json += statusText;
    json += "\"";

    json += "}";

    return json;
}

// =====================================================
// HTML WEB PAGE
// =====================================================

const char index_html[] PROGMEM = R"HTML(

<!DOCTYPE html>
<html>

<head>

<meta charset="UTF-8">

<meta name="viewport" content="width=device-width, initial-scale=1.0">

<title>Smart Safety System</title>

<link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css" rel="stylesheet">

<style>

body{
background:#f4f7f9;
font-family:Arial;
}

.card{
border-radius:20px;
box-shadow:0 10px 20px rgba(0,0,0,0.1);
border:none;
}

.tank{
width:170px;
height:280px;
border-radius:25px;
border:5px solid #ddd;
margin:auto;
overflow:hidden;
position:relative;
background:white;
}

.fill{
position:absolute;
bottom:0;
width:100%;
height:50%;
background:green;
transition:1s;
}

.big{
font-size:35px;
font-weight:bold;
}

.status{
font-size:28px;
font-weight:bold;
}

</style>

</head>

<body>

<div class="container py-4">

<h1 class="text-center fw-bold mb-4">
SMART GAS & SMOKE SAFETY SYSTEM
</h1>

<div class="row g-4">

<div class="col-md-4">

<div class="card p-4 text-center">

<div class="tank">
<div class="fill" id="fill"></div>
</div>

<h2 class="mt-3">
<span id="temp">0</span> °C
</h2>

</div>

</div>

<div class="col-md-8">

<div class="row g-3">

<div class="col-md-6">

<div class="card p-4 text-center">

<h3>Gas Sensor</h3>

<div class="big" id="gas">0</div>

</div>

</div>

<div class="col-md-6">

<div class="card p-4 text-center">

<h3>Smoke Sensor</h3>

<div class="big" id="smoke">0</div>

</div>

</div>

<div class="col-12">

<div class="card p-4 text-center">

<div class="status" id="status">
SYSTEM SAFE
</div>

<button class="btn btn-primary mt-3" onclick="speakStatus()">
VOICE ALERT
</button>

</div>

</div>

</div>

</div>

</div>

</div>

<script>

function speakStatus()
{
let s = document.getElementById("status").innerText;

speechSynthesis.speak(
new SpeechSynthesisUtterance(s)
);
}

async function updateData()
{
try
{
let response = await fetch("/data");

let data = await response.json();

document.getElementById("temp").innerHTML = data.temperature;

document.getElementById("gas").innerHTML = data.gas;

document.getElementById("smoke").innerHTML = data.smoke;

document.getElementById("status").innerHTML = data.status;

let fill = document.getElementById("fill");

let tempPercent = data.temperature;

fill.style.height = tempPercent + "%";

if(tempPercent < 40)
{
fill.style.background = "blue";
}
else if(tempPercent < 80)
{
fill.style.background = "green";
}
else
{
fill.style.background = "red";
}
}
catch(error)
{
console.log(error);
}
}

setInterval(updateData,1000);

</script>

</body>

</html>

)HTML";

// =====================================================
// SETUP
// =====================================================

void setup()
{
    Serial.begin(115200);

    pinMode(GAS_SENSOR, INPUT);

    pinMode(SMOKE_SENSOR, INPUT);

    pinMode(RED_LED, OUTPUT);

    pinMode(GREEN_LED, OUTPUT);

    pinMode(BUZZER, OUTPUT);

    digitalWrite(GREEN_LED, HIGH);

    myStepper.setSpeed(15);

    // =================================================
    // WIFI CONNECT
    // =================================================

    WiFi.begin(ssid, password);

    Serial.print("Connecting WiFi");

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");

    Serial.println("WiFi Connected");

    Serial.print("IP Address: ");

    Serial.println(WiFi.localIP());

    // =================================================
    // WEB SERVER
    // =================================================

    server.on("/", HTTP_GET,
    [](AsyncWebServerRequest *request)
    {
        request->send_P(
            200,
            "text/html",
            index_html
        );
    });

    server.on("/data", HTTP_GET,
    [](AsyncWebServerRequest *request)
    {
        request->send(
            200,
            "application/json",
            processor()
        );
    });

    server.begin();

    Serial.println("WEB SERVER STARTED");
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
    gasValue = analogRead(GAS_SENSOR);

    smokeValue = analogRead(SMOKE_SENSOR);

    int tempRaw = analogRead(TEMP_SENSOR);

    temperature = map(
        tempRaw,
        0,
        4095,
        0,
        100
    );

    gasDetected = gasValue > 2000;

    smokeDetected = smokeValue > 2000;

    // =================================================
    // TEMPERATURE WARNING
    // =================================================

    if (temperature >= 80)
    {
        digitalWrite(RED_LED, HIGH);
    }
    else
    {
        digitalWrite(RED_LED, LOW);
    }

    // =================================================
    // DANGER DETECTED
    // =================================================

    if (gasDetected || smokeDetected)
    {
        digitalWrite(BUZZER, HIGH);

        digitalWrite(GREEN_LED, LOW);

        digitalWrite(RED_LED, HIGH);

        statusText = "DANGER DETECTED";

        // CLOSE GAS VALVE

        myStepper.setSpeed(25);

        myStepper.step(30);

        // SEND EMAIL ONLY ONCE

        if (!alertSent)
        {
            String alertMessage = "";

            if (gasDetected)
            {
                alertMessage += "Gas Leakage Detected\n";
            }

            if (smokeDetected)
            {
                alertMessage += "Smoke Detected\n";
            }

            alertMessage += "Room Temperature: ";

            alertMessage += String(temperature);

            alertMessage += " C";

            sendEmail(
                "ESP32 SAFETY ALERT",
                alertMessage
            );

            alertSent = true;
        }
    }
    else
    {
        digitalWrite(BUZZER, LOW);

        digitalWrite(GREEN_LED, HIGH);

        digitalWrite(RED_LED, LOW);

        statusText = "SYSTEM SAFE";

        alertSent = false;
    }

    delay(1000);
}