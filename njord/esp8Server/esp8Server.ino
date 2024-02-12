#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <EasyDDNS.h>

#define waterPin 5

AsyncWebServer server(80);

const char* ssid = "njord";
const char* password = "vim123456";

const char* PARAM_MESSAGE = "period";

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
    <meta charset="utf-8" name="viewport" content="width=device-width, initial-scale=1">
    <title>Поливалка</title>
  <style>
    body {font-family: "Roboto", sans-serif;font-weight: 100;background: #2C2C2C;color: #FDFDFC;margin: 1em;}
    p {font-size: 1.5rem;letter-spacing: 1px;display: block;}
    .box {display: flex;flex-direction: row;justify-content: flex-start;}
    .slidecontainer {display: flex;align-items: center;width: 100vw;}
    #sliderValue {text-align: end;margin-right: 1rem;padding: 0;font-size: 2.5rem;width: 80px;}
    .slider {padding: 0;-webkit-appearance: none;width: 100%;height: 8px;background: #2C2C2C;outline: none;-webkit-transition: 0.2s;transition: opacity 0.2s;border: 1px solid;border-color: #FDFDFC;border-radius: 10px;}
    .slider::-moz-range-progress {border-radius: 5px;background-color: #4FBE57;height: 8px;}
    .slider::-webkit-slider-runnable-track{border-radius: 5px;background-color: #4FBE57;height: 8px;}
    .slider::-ms-fill-lower {border-radius: 5px;background-color: #4FBE57;height: 8px;}
    .slider::-webkit-slider-thumb {width: 25px;height: 25px;border: 1px solid;border-radius: 50%;background: #4FBE57;border-color: #FDFDFC;}
    .slider::-moz-range-thumb {width: 25px;height: 25px;border: 1px solid;border-radius: 50%;background: #4FBE57;border-color: #FDFDFC;}.divButton {margin-top: 1rem;}
    .button {font-family: "Roboto", sans-serif;background-color: #4FBE57; /* Green */border: none;color: #FDFDFC;padding: 1rem 1rem;text-align: center;text-decoration: none;display: inline-block;font-size: 2rem;}

  </style>
</head>
<body>
    <p>Система контроля полива</p>
    <div class="box">
      <div class="slidecontainer">
        <input type="range" min="1" max="100" value="50" class="slider" id="mySlider">
      </div>
      <div id="sliderValue">
        50
      </div>
    </div>
    <div class="divButton">
       <button class="button"  onclick="clickButton(this)" id="output">Включить полив</button>
    </div>
<script>
    var slider = document.getElementById("mySlider");
    var output = document.getElementById("sliderValue");
    output.innerHTML = slider.value;
    slider.oninput = function() {
        output.innerHTML = this.value;
    };
    function clickButton(element) {
        var sliderValue = slider.value;
        var count = sliderValue, timer = setInterval(function() {
            count--; 
            output.innerHTML = count;
            slider.value = count;
            if(count == 0){ 
                clearInterval(timer); 
                console.log(sliderValue);
                output.innerHTML = sliderValue;
                slider.value = sliderValue;
            }
        }, 1000);
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "/timerOpen?period=" + sliderValue);
        xhr.send();
    }
</script>
</body>
</html>
)rawliteral";
class Timer{
public:
    void timerStart(){
        timer = millis();
        timerState = 1; 
    }

    void timerStop(){
        timerState = 0;
    }

    void setPeriod(String setPer)
    {
        period = setPer.toInt() * 1000;
    }

    void setPeriod(uint32_t setPer)
    {
        period = setPer;
    }

    uint32_t getPeriod(){
        return period/1000;
    }

    bool timerEnabled(){
        return timerState;
    }

    bool timerTriggered(){
        if(timerState){
            if (millis() - timer >= period) {
                timerStop(); 
                return true;
            }
        }
        return false;
    }
private:
    bool timerState = 0; 
    uint32_t period = 0;
    uint32_t timer = 0;
};

Timer nTimer;

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

void setup() {

    pinMode(waterPin, OUTPUT);
    digitalWrite(waterPin, LOW);

    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.printf("WiFi Failed!\n");
        return;
    }

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    //------------------setup ddns server----------------//
    EasyDDNS.service("duckdns");
    EasyDDNS.client("njordvim.duckdns.org", "fd1a1e34-753e-44df-b07e-7ccaa34bc3bd");

    EasyDDNS.onUpdate([&](const char* oldIP, const char* newIP){
        Serial.print("EasyDDNS - IP Change Detected: ");
        Serial.println(newIP);
    });


    //-----------------setup server request-----------------//
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", index_html);
    });

    // Send a GET request to <IP>/get?message=<message>
    server.on("/timerOpen", HTTP_GET, [] (AsyncWebServerRequest *request) {
        if(nTimer.timerEnabled()){
            request->send(200, "text/plain", "Valve already open");
        }
        String message;
        if (request->hasParam(PARAM_MESSAGE)) {
            message = request->getParam(PARAM_MESSAGE)->value();
            nTimer.setPeriod(message);
        } else {
            nTimer.setPeriod("5"); 
        }

        Serial.println("Valve open");
        digitalWrite(waterPin, HIGH);
        nTimer.timerStart();
        request->send(200, "text/plain", String("Valve open") + nTimer.getPeriod() + " seconds");
    });

    server.on("/open", HTTP_GET, [](AsyncWebServerRequest *request){
        nTimer.timerStop();
        Serial.println("Valve open");
        digitalWrite(waterPin, HIGH);
        request->send(200, "text/plain", "Valve open");
    });

    server.on("/close", HTTP_GET, [](AsyncWebServerRequest *request){
        nTimer.timerStop();
        Serial.println("Valve close");
        digitalWrite(waterPin, LOW);
        request->send(200, "text/plain", "Valve close");
    });


    server.onNotFound(notFound);

    server.begin();
}

void loop() {
    if(nTimer.timerTriggered()){
        digitalWrite(waterPin, LOW);
        Serial.println("Valve close");
    }

    EasyDDNS.update(10000);
}
