#include <camundaC8.pb.h>
#include <pb_common.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include <pb.h>

extern "C" {
#include "sh2lib.h"
}

#include "soc/soc.h"          // Disable brownout problems
#include "soc/rtc_cntl_reg.h" // Disable brownout problems
#include "driver/rtc_io.h"


#define debug

#include <WiFiClientSecure.h>
#include <ssl_client.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>
#include <WiFiMulti.h>
#include <ArduinoJson.h>

#ifdef debug
#define debugprint(x) Serial.print(x)
#define debugprintln(x) Serial.println(x)
#define debugprintF(x) Serial.print(F(x))
#else
#define debugprint(x)
#define debugprintF(x)
#endif

// LED will blink when in config mode

#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
//for LED status
#include <Ticker.h>
Ticker ticker;

#ifndef LED_BUILTIN
#define LED_BUILTIN 13 // ESP32 DOES NOT DEFINE LED_BUILTIN
#endif

int LED = LED_BUILTIN;
#define FLASH_BULB 4
#define RESET_BTN 12
#define SHUTTER 13

String c8_server;
String c8_address;

struct Settings {
  char c8_server[100];
  char c8_auth[50];
  char c8_client_id[50];
  char c8_client_secret[80];
  char c8_cluster_addr[100];
} sett;

struct Auth {
  char access_token[512];
  char scope[50];
  uint32_t expires_in = 0;
  char token_type[24];
} auth;


volatile int interruptCounter;
int totalInterruptCounter;

hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  interruptCounter++;
  portEXIT_CRITICAL_ISR(&timerMux);

}


const char C8SSLCA[] PROGMEM =  R"EOF(
------BEGIN CERTIFICATE-----
MIIDzTCCArWgAwIBAgIQCjeHZF5ftIwiTv0b7RQMPDANBgkqhkiG9w0BAQsFADBa
MQswCQYDVQQGEwJJRTESMBAGA1UEChMJQmFsdGltb3JlMRMwEQYDVQQLEwpDeWJl
clRydXN0MSIwIAYDVQQDExlCYWx0aW1vcmUgQ3liZXJUcnVzdCBSb290MB4XDTIw
MDEyNzEyNDgwOFoXDTI0MTIzMTIzNTk1OVowSjELMAkGA1UEBhMCVVMxGTAXBgNV
BAoTEENsb3VkZmxhcmUsIEluYy4xIDAeBgNVBAMTF0Nsb3VkZmxhcmUgSW5jIEVD
QyBDQS0zMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEua1NZpkUC0bsH4HRKlAe
nQMVLzQSfS2WuIg4m4Vfj7+7Te9hRsTJc9QkT+DuHM5ss1FxL2ruTAUJd9NyYqSb
16OCAWgwggFkMB0GA1UdDgQWBBSlzjfq67B1DpRniLRF+tkkEIeWHzAfBgNVHSME
GDAWgBTlnVkwgkdYzKz6CFQ2hns6tQRN8DAOBgNVHQ8BAf8EBAMCAYYwHQYDVR0l
BBYwFAYIKwYBBQUHAwEGCCsGAQUFBwMCMBIGA1UdEwEB/wQIMAYBAf8CAQAwNAYI
KwYBBQUHAQEEKDAmMCQGCCsGAQUFBzABhhhodHRwOi8vb2NzcC5kaWdpY2VydC5j
b20wOgYDVR0fBDMwMTAvoC2gK4YpaHR0cDovL2NybDMuZGlnaWNlcnQuY29tL09t
bmlyb290MjAyNS5jcmwwbQYDVR0gBGYwZDA3BglghkgBhv1sAQEwKjAoBggrBgEF
BQcCARYcaHR0cHM6Ly93d3cuZGlnaWNlcnQuY29tL0NQUzALBglghkgBhv1sAQIw
CAYGZ4EMAQIBMAgGBmeBDAECAjAIBgZngQwBAgMwDQYJKoZIhvcNAQELBQADggEB
AAUkHd0bsCrrmNaF4zlNXmtXnYJX/OvoMaJXkGUFvhZEOFp3ArnPEELG4ZKk40Un
+ABHLGioVplTVI+tnkDB0A+21w0LOEhsUCxJkAZbZB2LzEgwLt4I4ptJIsCSDBFe
lpKU1fwg3FZs5ZKTv3ocwDfjhUkV+ivhdDkYD7fa86JXWGBPzI6UAPxGezQxPk1H
goE6y/SJXQ7vTQ1unBuCJN0yJV0ReFEQPaA1IwQvZW+cwdFD19Ae8zFnWSfda9J1
CZMRJCQUzym+5iPDuI9yP+kHyCREU3qzuWFloUwOxkgAyXVjBYdwRVKD05WdRerw
6DEdfgkfCv4+3ao8XnTSrLE=
-----END CERTIFICATE-----
)EOF";

const unsigned char C8ServerSSLCA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFvTCCBKWgAwIBAgISA9YO3aNN2ONJ7avFarHl6I6gMA0GCSqGSIb3DQEBCwUA
MDIxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1MZXQncyBFbmNyeXB0MQswCQYDVQQD
EwJSMzAeFw0yMjAzMzExMjA5MTRaFw0yMjA2MjkxMjA5MTNaMBcxFTATBgNVBAMM
DCouY2FtdW5kYS5pbzCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALA2
cNIJ0foZGX7d3LVV70GpcLgnKYZpFvOOs+5qLs8jv5x2nm4ughySjgANv/utD2lR
lo7c9FJXkh8i4dbtmcxLVpmwd5wKz3EnQEt4yFsYDYJ4SB2SLuox+hqm1raerlx1
qyx8mqmuhCL7AvGL7ZhNQRt3kLfhupmlwkG5Le6CuwKp8+cmZf1/VM2lb7TwAOlI
fmr+hItJNx2YFnCmk2C+uBFyx4ye4tNb2L7uAGnug9sOnqrFLHth4hNwC5704h8U
DpVSmCcWXTN88t9R0pz6hV9bYD49xI/2JTjpKy+VijLKaRvI8CHsRyjfCanBnoaY
GuURVS6XDbEXISBm+JsCAwEAAaOCAuYwggLiMA4GA1UdDwEB/wQEAwIFoDAdBgNV
HSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwDAYDVR0TAQH/BAIwADAdBgNVHQ4E
FgQUAWDHj4F+e8H5H/5u8HDsnD+NVZ0wHwYDVR0jBBgwFoAUFC6zF7dYVsuuUAlA
5h+vnYsUwsYwVQYIKwYBBQUHAQEESTBHMCEGCCsGAQUFBzABhhVodHRwOi8vcjMu
by5sZW5jci5vcmcwIgYIKwYBBQUHMAKGFmh0dHA6Ly9yMy5pLmxlbmNyLm9yZy8w
gbYGA1UdEQSBrjCBq4IYKi5icnUtMi56ZWViZS5jYW11bmRhLmlvggwqLmNhbXVu
ZGEuaW+CEiouY2xvdWQuY2FtdW5kYS5pb4IVKi5pbnRlcm5hbC5jYW11bmRhLmlv
ghQqLm9wZXJhdGUuY2FtdW5kYS5pb4IVKi5vcHRpbWl6ZS5jYW11bmRhLmlvghUq
LnRhc2tsaXN0LmNhbXVuZGEuaW+CEiouemVlYmUuY2FtdW5kYS5pbzBMBgNVHSAE
RTBDMAgGBmeBDAECATA3BgsrBgEEAYLfEwEBATAoMCYGCCsGAQUFBwIBFhpodHRw
Oi8vY3BzLmxldHNlbmNyeXB0Lm9yZzCCAQMGCisGAQQB1nkCBAIEgfQEgfEA7wB1
AN+lXqtogk8fbK3uuF9OPlrqzaISpGpejjsSwCBEXCpzAAABf+AYK3gAAAQDAEYw
RAIgH79yFySdX7Du1+d0u8P6VXgh19903sJdUQPdZHWapykCICjtwFs/actgfiZL
0Rp4IsA1tG9voTv6D7aESwNekyinAHYAKXm+8J45OSHwVnOfY6V35b5XfZxgCvj5
TV0mXCVdx4QAAAF/4BgrbAAABAMARzBFAiALyZnSgUmF+Uzhzm6FME95klRnOZZV
5O5Q7NmLHR0A1wIhAL5WeqQ5uGrPxQKMY8wygbvXUEmMiw87dRdOSzLkdSq9MA0G
CSqGSIb3DQEBCwUAA4IBAQAuRbvDPfzqQWwlYVc6JWRF2uOttmEEgk1I+Hp1+Kz3
cXhywtLfmMg0n8th01tleqGbsx9EwhlHNtolQG36pfB8hvI71DNWKiNMCqziM6f7
hQpGkfavxgIzrvvSmfIGf7HxgmjaQ16DH4Cbs/eqS0NJXDJVR1rX9U9TzyMYNc/2
GXPkEf5+eRNAM0jFTMVlzEH/xOtMYc3W9NK+XRImqfHjcHKzlfXP973EW7ymFoLi
R5DgcakBIDfLcN73XF1CljUHTj5JGaCsMxNPiqn5YKMR/uSDnsomR1/zP3fWhvQm
r8/E9+6TptoIExX65bCHe3geQs3ZBUXZC66N8LqdeNm1
-----END CERTIFICATE-----
)EOF";
bool starter = false;
uint32_t now_millis;
uint32_t int_millis;

void tick()
{
  //toggle state
  digitalWrite(LED, !digitalRead(LED));     // set pin to the opposite state
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  debugprintln("Entered config mode");
  debugprintln(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  debugprintln(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

// Not sure if WiFiClientSecure checks the validity date of the certificate.
// Setting clock just to be sure...
void setClock() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  debugprintF("Waiting for NTP time sync: ");
  time_t nowSecs = time(nullptr);
  while (nowSecs < 8 * 3600 * 2) {
    delay(500);
    debugprintF(".");
    yield();
    nowSecs = time(nullptr);
  }

  debugprintln();
  struct tm timeinfo;
  gmtime_r(&nowSecs, &timeinfo);
  debugprintF("Current time: ");
  debugprint(asctime(&timeinfo));
}

void setup() {
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  // put your setup code here, to run once:
#ifdef debug
    Serial.begin(115200);
#endif
  //set led pin as output
  pinMode(LED, OUTPUT);
  pinMode(FLASH_BULB, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RESET_BTN, INPUT);
  pinMode(SHUTTER, INPUT);
  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  timer = timerBegin(0, 40000, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 30 * 2000, false);
  
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;
  //reset settings - for testing
  //wm.resetSettings();
  WiFiManagerParameter camunda_auth_server("c8_auth_server", "ZeeBe Auth Server", "https://login.cloud.camunda.io/oauth/token", 50, " ");
  wm.addParameter(&camunda_auth_server);
  WiFiManagerParameter camunda_cloud_server("c8_server", "ZeeBe Address", "https://YOUR_SERVER_ID.bru-2.zeebe.camunda.io", 100, " ");
  wm.addParameter(&camunda_cloud_server);
  WiFiManagerParameter camunda_client_id("c8_client_id", "ZeeBe Client ID", "YOUR_CLIENT_ID", 50, " ");
  wm.addParameter(&camunda_client_id);
  WiFiManagerParameter camunda_client_secret("c8_client_secret", "ZeeBe Client Secret", "YOUR_SECRET", 80, " ");
  wm.addParameter(&camunda_client_secret);
  WiFiManagerParameter camunda_cluster_addr("c8_cluster_addr", "Camunda Cluster Address", "https://YOUR_CLUSTER_ADDRESS.bru-2.zeebe.camunda.io:443", 100, " ");
  wm.addParameter(&camunda_cluster_addr);
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wm.setAPCallback(configModeCallback);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  wm.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  if (!wm.autoConnect()) {
    debugprintln("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(1000);
  }

  //if you get here you have connected to the WiFi
  debugprintln("connected...yay :)");
  setClock();
  ticker.detach();
  //keep LED on
  digitalWrite(LED, LOW);
  sett.c8_server[99] = '\0';
  //strncpy(sett.c8_server, camunda_cloud_server.getValue(), 100);
  strncpy(sett.c8_server, "b5161a28-2fd3-4879-a99e-60f7478ad3d5.bru-2.zeebe.camunda.io", strlen("b5161a28-2fd3-4879-a99e-60f7478ad3d5.bru-2.zeebe.camunda.io"));
  sett.c8_client_id[49] = '\0';
//  strncpy(sett.c8_client_id, camunda_client_id.getValue(), 50);
  strncpy(sett.c8_client_id, "wROQIC_haG_T6932iWZYsFxwuSIbR~UG", strlen("wROQIC_haG_T6932iWZYsFxwuSIbR~UG"));
  sett.c8_auth[49] = '\0';
  strncpy(sett.c8_auth, camunda_auth_server.getValue(), 50);
  sett.c8_client_secret[79] = '\0';
//  strncpy(sett.c8_client_secret, camunda_client_secret.getValue(), 80);
  strncpy(sett.c8_client_secret, "wROQIC_haG_T6932iWZYsFxwuSIbR~UG", strlen("wROQIC_haG_T6932iWZYsFxwuSIbR~UG"));
  sett.c8_cluster_addr[99] = '\0';
  strncpy(sett.c8_cluster_addr, "https://b5161a28-2fd3-4879-a99e-60f7478ad3d5.bru-2.zeebe.camunda.io", strlen("https://b5161a28-2fd3-4879-a99e-60f7478ad3d5.bru-2.zeebe.camunda.io"));

  debugprint("ZeeBe Address: \t");
  debugprintln(sett.c8_server);
  debugprint("ZeeBe Client ID: \t");
  debugprintln(sett.c8_client_id);
  debugprint("ZeeBe Auth Server: \t");
  debugprintln(sett.c8_auth);
  debugprint("ZeeBe Client Secret: \t");
  debugprintln(sett.c8_client_secret);
  debugprint("ZeeBe Cluster Addr: ");
  debugprintln(sett.c8_cluster_addr);
  debugprintln();
  now_millis = millis();
  timerAlarmEnable(timer);
  //getCamundaOAuth();
}


void getCamundaOAuth() {
  timerAlarmDisable(timer);
  debugprintln("Timer Disabled...");
  debugprintln("Getting C8 Authorization ...");
    debugprintln("Connecting to Camunda Cloud ... ");
    WiFiClientSecure *client = new WiFiClientSecure;
    if(client) {
      client->setCACert(C8SSLCA);  
    }
    StaticJsonDocument<256> doc;
    doc["client_id"] = "cPnckauG-rxBvVOF6wwO4I6SHrifts2q";
    doc["client_secret"] = "OqJ.ha4qOgRHE0M12PslOU8ho57cuU95V0Kg8sxUMrXQo6x-cPEq6op9bQLFj67-";
    doc["audience"] = "zeebe.camunda.io";
    doc["grant_type"] = "client_credentials";
    debugprint("Overflowed: ");debugprintln(doc.overflowed());
    debugprint("Json Doc: ");
    serializeJson(doc, Serial);
    String buff;
    serializeJson(doc, buff);
    debugprintln();
    debugprintln(buff);
    String pBuff = "\r\n\r\n" + buff + "\r\n";
    debugprintln("Posting ...");
    debugprint("[HTTP] POST...\n");
    client->connect("login.cloud.camunda.io", 443);
    int tries = 0;
    while (!client->connected()) {
      Serial.printf("*** Can't connect. ***\n-------\n");
      delay(500);
      debugprint(".");
      client->connect("login.cloud.camunda.io", 443);
      tries++;
      if(tries > 10){
        return;
      }
  }
  debugprintln("Connected!");
  Serial.printf("Connected!\n-------\n");
  client->print("POST /oauth/token HTTP/1.0\r\n");
  client->print("Host: login.cloud.camunda.io\r\n");
  client->print("User-Agent: ESP8266\r\n");
  client->print("Content-Length: ");
  client->print(pBuff.length());
  client->print("\r\n");
  client->print("Content-Type: application/json\r\n");
  client->print("Accept-encoding: *\r\n");
  client->print("\r\n");
  client->print(pBuff);
  String in_buffer = "";
  uint32_t to = millis() + 10000;
  if (client->connected()) {
    debugprintln("Reading response ...");
    do {
      int avail = client->available();
      if(avail > 0){
        debugprintln("Data available!");
        break;
      }
      debugprint(".");
      delay(500);
    } while (millis() < to);
    debugprintln();
    to = millis() + 5000;
    do {
      char tmp[512];
      memset(tmp, 0, 512);
      int rlen = client->read((uint8_t*)tmp, sizeof(tmp) - 1);
      if (rlen < 0) {
        break;
      }
      debugprint(tmp);
      in_buffer += tmp;
    } while (millis() < to);
    debugprintln();
    debugprintln("Finished reading");
  }
  client->stop();
  Serial.printf("\nDone!\n-------\n\n");
  // char input[MAX_INPUT_LENGTH];
  debugprintln("Read in: ");
  debugprintln(in_buffer);
  
  in_buffer = in_buffer.substring(in_buffer.indexOf("{"), in_buffer.length());
  debugprintln();
  debugprintln("Just JSON: ");
  debugprintln(in_buffer); 
  // String input;

  StaticJsonDocument<1536> token_doc;
  DeserializationError error = deserializeJson(token_doc, in_buffer);

  if (error) {
    debugprint("deserializeJson() failed: ");
    debugprintln(error.c_str());
    return;
  }
  strncpy(auth.access_token, token_doc["access_token"], sizeof(auth.access_token));
  strncpy(auth.scope, token_doc["scope"], sizeof(auth.scope));
  strncpy(auth.token_type, token_doc["token_type"], sizeof(auth.token_type));
  auth.expires_in = uint32_t(token_doc["expires_in"]);
  
  Serial.printf("Token: %s\nScope: %s\nExpires: %6ld\nType: %s\n", auth.access_token, auth.scope, auth.expires_in, auth.token_type);
  timerAlarmWrite(timer, auth.expires_in * 2000, false);
  timerAlarmEnable(timer);
  debugprint("Timer will expire in ");
  debugprint(auth.expires_in/60);
  debugprintln(" minutes\n\n===========================================\n\n");
  starter = true;
}


void loop() {
  if (digitalRead(RESET_BTN) == HIGH) {
    debugprintln("Resetting!!");
    // reset and try again
//    ESP.restart();
    starter = true;
    delay(1000);
  }
  gateway_protocol_TopologyRequest getTopo;
  // put your main code here, to run repeatedly:
  if(interruptCounter > 0){
    portENTER_CRITICAL(&timerMux);
    interruptCounter--;
    portEXIT_CRITICAL(&timerMux);
    int_millis = millis();
    uint32_t passed = int_millis - now_millis;
    totalInterruptCounter++;
    debugprint("An interrupt as occurred. Total number: ");
    debugprintln(totalInterruptCounter);
    debugprint("Elapsed time: ");
    debugprint(passed/2000);
    debugprintln(" Seconds.");
    getCamundaOAuth();
  }
  if(starter){
      starter = !starter;
      xTaskCreate(http2_task, "http2_task", (2048 * 64), NULL, 5, NULL);

  } 

}

int handle_get_response(struct sh2lib_handle *handle, const char *data, size_t len, int flags)
{
    if (len > 0) {
        Serial.printf("%.*s\n", len, data);
    }

    if (flags == DATA_RECV_RST_STREAM) {
        debugprintln("STREAM CLOSED");
    }
    return 0;
}

void http2_task(void *args){
  delay(500);
  debugprintln("Encoding buffer ...");
  gateway_protocol_TopologyResponse topoResp = gateway_protocol_TopologyResponse_init_default;
  gateway_protocol_TopologyRequest topoReq = gateway_protocol_TopologyRequest_init_zero;
  uint8_t buff[128];
  pb_ostream_t stream = pb_ostream_from_buffer(buff, sizeof(buff));
  if(!pb_encode(&stream, gateway_protocol_TopologyRequest_fields, &topoReq)){
    debugprintln("Failed encoding protobuf");
    return;
  }
  debugprint("ProtoBuffer Size: ");
  debugprintln(sizeof(topoReq));
  debugprint("Message Length: ");
  debugprintln(stream.bytes_written);

  debugprint("Message: ");
  delay(500);
  for(int i = 0; i<stream.bytes_written; i++){
    Serial.printf("%02X",buff[i]);
  }
  debugprintln();
  debugprintln("Buffer encoding complete.");
  debugprintln("Connecting to Camunda Cloud ... ");
  delay(250);
//  struct sh2lib_handle hd;
//  struct sh2lib_config_t hdConfig;
  char* host = strdup(sett.c8_server);
  debugprint("Server: ");
  debugprintln(host);  
  struct sh2lib_config_t cfg = {
        .uri = "https://b5161a28-2fd3-4879-a99e-60f7478ad3d5.bru-2.zeebe.camunda.io",
        .cacert_buf = C8ServerSSLCA,
        .cacert_bytes = sizeof(C8ServerSSLCA)
//#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
//        .crt_bundle_attach = esp_crt_bundle_attach,
//#endif
    };
    struct sh2lib_handle hd;
//  hdConfig.uri = host;
//  hdConfig.cacert_buf = C8ServerSSLCA;
//  hdConfig.cacert_bytes = sizeof(C8ServerSSLCA);
  delay(250);
//   {
//     const char *uri;                    /*!< Pointer to the URI that should be connected to */
//     const unsigned char *cacert_buf;    /*!< Pointer to the buffer containing CA certificate */
//     unsigned int cacert_bytes;          /*!< Size of the CA certifiacte pointed by cacert_buf */
// };
  int try_count = 0;
  while (sh2lib_connect(&cfg, &hd) != ESP_OK) {
    debugprintln("Error connecting to HTTP2 server");
    starter = false;
    debugprint("ending ... ");
    debugprintln(try_count);
    if(try_count >= 10){
    vTaskDelete(NULL);
        return;
    }
    delay(2500);
    try_count++;
  }

  debugprintln("Connected");
  debugprintln(hd.hostname);
  char auth_toke[7+strlen(auth.access_token) + 1];
  sprintf(auth_toke, "Bearer %s", auth.access_token);
  debugprint("Auth header: ");
  debugprintln(auth_toke);
  const nghttp2_nv nva[] = { SH2LIB_MAKE_NV(":method", "GET"),
                           SH2LIB_MAKE_NV(":scheme", "https"),
                           SH2LIB_MAKE_NV(":authority", hd.hostname),
                           SH2LIB_MAKE_NV(":path", "/Topology"),
                           SH2LIB_MAKE_NV("accept-encoding", "gzip"),
                           SH2LIB_MAKE_NV("Authorization", auth_toke),
                          };
  sh2lib_do_get_with_nv(&hd, nva, sizeof(nva) / sizeof(nva[0]), handle_get_response);
// sh2lib_do_get(&hd, "/Topology", handle_get_response);

  while (1) {

    if (sh2lib_execute(&hd) != ESP_OK) {
      debugprintln("Error in send/receive");
      break;
    } 

    delay(10);
  }
  sh2lib_free(&hd);

  debugprintln("Disconnected");

  //   WiFiClientSecure *client = new WiFiClientSecure;
  //   if(client) {
  //     client->setCACert(C8ServerSSLCA);  
  //   }
    
  //   debugprint("Connecting to: ");
  //   debugprintln(sett.c8_server);
  //   debugprintln(":443");
  //   client->connect(sett.c8_server, 443);
  //   int tries = 0;
  //   while (!client->connected()) {
  //     Serial.printf("*** Can't connect. ***\n-------\n");
  //     delay(500);
  //     debugprint(".");
  //     client->connect(sett.c8_server, 443);
  //     tries++;
  //     if(tries > 10){
  //       client->stop();
  //       return;
  //     }
  // }
  // Serial.printf("Connected!\n-------\n");
  // client->print("Host: ");
  // client->print(sett.c8_cluster_addr); //login.cloud.camunda.io\r\n");
  // client->print("\r\nUser-Agent: ESP32\r\n");
  // client->print("Authorization: Bearer ");
  // client->print(auth.access_token);
  // client->print("\r\n\r\n");
  // for(int i = 0; i<stream.bytes_written; i++){
  //   client->print(buff[i]);
  // }
  // //client->print(buff);
  // debugprintln("Wrote: ");
  // debugprint("Host: ");
  // debugprint(sett.c8_cluster_addr);
  // debugprint("\r\nUser-Agent: ESP32\r\n");
  // debugprint("Authorization: Bearer ");
  // debugprint(auth.access_token);
  // debugprint("\r\n\r\n");
  // debugprintln("--------------------------------");
  // String in_buffer = "";
  // uint32_t to = millis() + 10000;
  // if (client->connected()) {
  //   debugprintln("Reading response ...");
  //   do {
  //     int avail = client->available();
  //     if(avail > 0){
  //       debugprintln("Data available!");
  //       break;
  //     }
  //     debugprint(".");
  //     delay(500);
  //   } while (millis() < to);
  //   debugprintln();
  //   to = millis() + 5000;
  //   do {
  //     char tmp[512];
  //     memset(tmp, 0, 512);
  //     int rlen = client->read((uint8_t*)tmp, sizeof(tmp) - 1);
  //     if (rlen < 0) {
  //       break;
  //     }
  //     debugprint(tmp);
  //     in_buffer += tmp;
  //   } while (millis() < to);
  //   debugprintln();
  //   debugprintln("Finished reading");
  // }
  // client->stop();
  // Serial.printf("\nDone!\n-------\n\n");
  // // char input[MAX_INPUT_LENGTH];
  // debugprintln("Read in: ");
  // debugprintln(in_buffer);
  
  // in_buffer = in_buffer.substring(in_buffer.indexOf("{"), in_buffer.length());
  // debugprintln();
  // debugprintln("Just JSON: ");
  // debugprintln(in_buffer); 
  // // String input;
  starter = false;
  
}
