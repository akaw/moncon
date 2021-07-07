/*
Firmware for NeowiseStudioMonCon Project based on ESP32
*/
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#include <ShiftRegister74HC595.h>
#include <ESPRotary.h>
#include <Keypad.h>
#include "config.h"

#define SHCP 2 // connected to SHCP
#define STCP 4 // connected to STCP
#define DS 5   // connected to DS

byte counter = 0;

const char ssid[] = WIFI_SSID;
const char password[] = WIFI_PASSWD;

WebServer server(80);

// Change char to int for handleButton()
char buttons[] = {'A', 'B', 'C', 'D', 'E', 'F', 'K', 'G', 'H', 'I', 'J'};

//Rotary
ESPRotary r = ESPRotary(22, 23, 4, 0, 255); //pin1, pin2, steps_per_click

//ShiftRegister
ShiftRegister74HC595<1> shiftRegister(DS, SHCP, STCP);
bool shiftRegisterStatus[8] = {0, 0, 0, 0, 0, 0, 0, 0};
int shiftRegisterMomentary = 0;
int statusLed = 7;

//Keypad
const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] =
    {
        {'K', 'G', 'X'},
        {'D', 'H', 'A'},
        {'E', 'I', 'B'},
        {'F', 'J', 'C'}};
        
byte rowPins[ROWS] = {27, 26, 25, 33};
byte colPins[COLS] = {19, 18, 21};
Keypad customKeypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

//Volume
byte volume = 0;
byte lowVolume = 60;
byte midVolume = 120;
byte highVolume = 200;

void setup()
{
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  shiftRegister.setAllLow();
  toggleStatusLed();

  pinMode(STCP, OUTPUT);
  pinMode(SHCP, OUTPUT);
  pinMode(DS, OUTPUT);

  r.setChangedHandler(changeVolume);

  customKeypad.addEventListener(keypadEvent); // Add an event listener for this keypad
  customKeypad.setDebounceTime(50);

  Serial.println("Ready!");
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    toggleStatusLed();
    counter += 1;
    if (counter > 30) {
      shiftRegisterStatus[7] = 1;
      updateShiftRegister();
      break;
    }
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("moncon"))
  {
    Serial.println("MDNS responder started: http://moncon.local");
  }else{
    Serial.println("Error setting up MDNS responder!");
  }

  server.on("/", handlePage);
  server.on("/get/states", getStates);
  server.on("/button/0", []() { handleButton(0); });
  server.on("/button/1", []() { handleButton(1); });
  server.on("/button/2", []() { handleButton(2); });
  server.on("/button/3", []() { handleButton(3); });
  server.on("/button/4", []() { handleButton(4); });
  server.on("/button/5", []() { handleButton(5); });
  server.on("/button/6", []() { handleButton(6); });
  server.on("/button/7", []() { handleButton(7); });
  server.on("/button/8", []() { handleButton(8); });
  server.on("/button/9", []() { handleButton(9); });
  server.on("/button/10", []() { handleButton(10); });
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop()
{
  r.loop();
  char key = customKeypad.getKey();
  if (key != NO_KEY)
  {
    Serial.println(key);
  }
  server.handleClient();
}

void setStatusLed(bool ledState = false)
{
  if(ledState){
    shiftRegisterStatus[statusLed] = 1;
  }else{
    shiftRegisterStatus[statusLed] = 0;
  }
  updateShiftRegister();
}

void keypadEvent(KeypadEvent key)
{
  switch (customKeypad.getState())
  {
  case PRESSED:
    setStatusLed(1);
    processPressedState(key);
    break;
  case HOLD:
    switch (key)
    {
    case 'A': //Input Switch 1
      break;
    case 'B': //Input Switch 2
      break;
    case 'C': //Input Switch 3
      break;
    case 'D': //Output Switch 1
      break;
    case 'E': //Output Switch 2
      break;
    case 'F': //Talkback
      shiftRegisterMomentary = 5;
      break;
    case 'G':
      break;
    case 'H':
      break;
    case 'I':
      break;
    case 'J': //Reset
      setVolume(midVolume);
      resetShiftRegister();
      break;
    case 'K': //Mute
      break;
    }
    break;
  case RELEASED:
    if (shiftRegisterMomentary >= 0)
    {
      toggleShiftRegister(shiftRegisterMomentary);
      shiftRegisterMomentary = -1;
      updateShiftRegister();
    }
    setStatusLed(0);
    break;
  }
}

bool processPressedState(char key)
{
  bool switchState = 0;
  switch (key)
  {
  case 'A': //Input Switch 1
    switchState = toggleShiftRegister(0);
    break;
  case 'B': //Input Switch 2
    switchState = toggleShiftRegister(1);
    break;
  case 'C': //Input Switch 3
    switchState = toggleShiftRegister(2);
    break;
  case 'D': //Output Switch 1
    switchState = toggleShiftRegister(3);
    break;
  case 'E': //Output Switch 2
    switchState = toggleShiftRegister(4);
    break;
  case 'F': //Talkback
    switchState = toggleShiftRegister(5);
    break;
  case 'G': //Set Volume Low
    setVolume(lowVolume);
    switchState = 1;
    break;
  case 'H': //Set Volume Normal
    setVolume(midVolume);
    switchState = 1;
    break;
  case 'I': //Set Volume High
    setVolume(highVolume);
    switchState = 1;
    break;
  case 'J': //A/B
    if (shiftRegisterStatus[0] && shiftRegisterStatus[3])
    {
      shiftRegisterStatus[0] = 0;
      shiftRegisterStatus[3] = 0;
      switchState = 0;
    }
    else
    {
      shiftRegisterStatus[0] = 1;
      shiftRegisterStatus[3] = 1;
      switchState = 1;
    }
    break;
  case 'K': //Mute
    switchState = toggleShiftRegister(6);
    break;
  }
  updateShiftRegister();
  return switchState;
}

void toggleStatusLed()
{
  if (shiftRegisterStatus[statusLed])
    setStatusLed(0);
  else
    setStatusLed(1);
}

bool toggleShiftRegister(int i)
{
  if (shiftRegisterStatus[i])
  {
    shiftRegisterStatus[i] = 0;
    return 0;
  }
  else
  {
    shiftRegisterStatus[i] = 1;
    return 1;
  }
}

void updateShiftRegister()
{
  for (int i = 0; i < 8; i++)
  {
    shiftRegister.set(i, shiftRegisterStatus[i]);
    Serial.print(shiftRegisterStatus[i]);
  }
  Serial.println();
}

void resetShiftRegister()
{
  for (int i = 0; i < 7; i++)
  {
    shiftRegisterStatus[i] = 0;
  }
}

void setVolume(byte value)
{
  volume = value;
  r.resetPosition(value);
  Serial.print("Volume: ");
  Serial.println(r.getPosition());
}

void changeVolume(ESPRotary &r)
{
  volume = r.getPosition();
  Serial.print("Volume: ");
  Serial.println(volume);
}

// WEBSERVER
void handleNotFound()
{
  toggleStatusLed();
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  toggleStatusLed();
}

void handleButton(byte id)
{
  bool switchState = 0;
  switchState = processPressedState(buttons[id]);
  updateShiftRegister();
  server.send(200, "text/plain", String(switchState));
}

void getStates()
{
  //shiftRegister buttons 0-6
  String jsonData = "[";
  for (int i = 0; i < 7; i++)
  {
    jsonData += "{\"id\":\"button-";
    jsonData += i;
    jsonData += "\",\"value\":";
    jsonData += (shiftRegisterStatus[i]) ? "1" : "0";
    jsonData += "},";
  }
  //Volumes
  jsonData += "{\"id\":\"button-7\",\"value\":";
  jsonData += (volume == lowVolume) ? "1" : "0";
  jsonData += "},";
  jsonData += "{\"id\":\"button-8\",\"value\":";
  jsonData += (volume == midVolume) ? "1" : "0";
  jsonData += "},";
  jsonData += "{\"id\":\"button-9\",\"value\":";
  jsonData += (volume == highVolume) ? "1" : "0";
  jsonData += "},";
  // A/B
  jsonData += "{\"id\":\"button-10\",\"value\":";
  jsonData += (shiftRegisterStatus[0] == 1 && shiftRegisterStatus[3]) == 1 ? "1" : "0";
  jsonData += "}";
  jsonData += "]";
  server.send(200, "text/plain", String(jsonData));
}

const char index_html[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="de">
  <head>
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
    <meta
      name="viewport"
      content="width=device-width, initial-scale=1.0,user-scalable=no"
    />
    <title>NeowiseMonCon</title>
    <style>
      html {
        height: -webkit-fill-available;
      }
      body {
        min-height: 100vh;
        /* mobile viewport bug fix */
        min-height: -webkit-fill-available;
        background-color: black;
        margin: 0;
        padding: 0;
      }
      .container {
        font-family: Helvetica, sans-serif;
        margin: 0;
        padding: 0;
        font-size: 16px;
        height: calc(var(--vh, 1vh) * 100);
        display: flex;
        flex-flow: row wrap;
      }
      .button,
      .button:active,
      .button:focus {
        flex-grow: 1;
        box-sizing: border-box;
        border-radius: 0rem;
        background-color: #3498db;
        border: 1px solid black;
        color: white;
        text-decoration: none;
        font-size: 1rem;
        cursor: pointer;
        margin: 0;
        padding: 0;
        flex-basis: 33%;
      }

      .button-on {
        background-color: #3498db;
      }
      .button-on:active {
        background-color: #2980b9;
      }
      .button-off {
        background-color: #34495e;
      }
      .button-off:active {
        background-color: #2c3e50;
      }
      @media only screen and (max-width: 480px) {
        /* For mobile phones: */
        .button,
        .button:active,
        .button:focus {
          font-size: 1rem;
          flex-basis: 50%;
        }
      }
      @media only screen and (min-width: 768px) {
        /* For mobile phones: */
        .button,
        .button:active,
        .button:focus {
          font-size: 2rem;
        }
      }      
      @media only screen and (min-width: 1024px) {
        /* For mobile phones: */
        .button,
        .button:active,
        .button:focus {
          font-size: 3rem;
        }
      }
    </style>
  </head>
  <body>
    <div class="container">
      <button
        type="button"
        id="button-0"
        data-buttonId="0"
        class="button button-off"
      >
        Input B
      </button>
      <button
        type="button"
        id="button-1"
        data-buttonId="1"
        class="button button-off"
      >
        Input C
      </button>
      <button
        type="button"
        id="button-2"
        data-buttonId="2"
        class="button button-off"
      >
        Bluetooth
      </button>
      <button
        type="button"
        id="button-3"
        data-buttonId="3"
        class="button button-off"
      >
        Output B
      </button>

      <button
        type="button"
        id="button-4"
        data-buttonId="4"
        class="button button-off"
      >
        Output C
      </button>
      <button
        type="button"
        id="button-10"
        data-buttonId="10"
        class="button button-off"
      >
        A/B
      </button>
      <button
        type="button"
        id="button-7"
        data-buttonId="7"
        class="button button-off"
      >
        Dim
      </button>
      <button
        type="button"
        id="button-8"
        data-buttonId="8"
        class="button button-off"
      >
        Mix
      </button>
      <button
        type="button"
        id="button-9"
        data-buttonId="9"
        class="button button-off"
      >
        Loud
      </button>
      <button
        type="button"
        id="button-6"
        data-buttonId="6"
        class="button button-off"
      >
        Mute
      </button>
      <button
        type="button"
        id="button-5"
        data-buttonId="5"
        class="button button-off"
      >
        Talkback
      </button>
    </div>
    <script type="text/javascript">
      var buttons = document.getElementsByClassName("button");
      for (var i = 0; i < buttons.length; i++) {
        buttons[i].addEventListener("click", pushButton, false);
      }

      function pushButton() {
        id = this.getAttribute("data-buttonId");
        var xhttpPushButton = new XMLHttpRequest();
        xhttpPushButton.onreadystatechange = function () {
          if (this.readyState == 4 && this.status == 200) {
            setState("button-" + id, this.responseText);
          }
        };
        xhttpPushButton.open("GET", "/button/" + id, true);
        xhttpPushButton.send();
        setStates();
      }

      function setStates() {
        var xhttpSetStates = new XMLHttpRequest();
        xhttpSetStates.onreadystatechange = function () {
          if (this.readyState == 4 && this.status == 200) {
            var jsonData = JSON.parse(this.responseText);
            if (jsonData) {
              for (var i = 0; i < jsonData.length; i++) {
                setState(jsonData[i].id, jsonData[i].value);
              }
            }
          }
        };
        xhttpSetStates.open("GET", "/get/states", true);
        xhttpSetStates.send();
      }

      function setState(id, value) {
        var element = document.getElementById(id);
        element.classList.remove("button-on");
        element.classList.remove("button-off");
        if (value == 1) {
          element.classList.add("button-on");
        } else {
          element.classList.add("button-off");
        }
      }

      setInterval(function ( ) {
        setStates();
      }, 1000 ) ;
      
      function setDocHeight() {
        document.documentElement.style.setProperty(
          "--vh",
          `${window.innerHeight / 100}px`
        );
      }
      window.addEventListener("resize", setDocHeight);
      window.addEventListener("orientationchange", setDocHeight);
      setDocHeight();
    </script>
  </body>
</html>)rawliteral";

void handlePage()
{
  server.send(200, "text/html", String(index_html));
}
