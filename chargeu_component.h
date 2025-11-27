#include "esphome.h"
#include <WiFi.h>
#include <HTTPClient.h>

const char* CHARGER_SSID = "CHARGEU base 2496"; 
const char* CHARGER_PASS = "34733223";
const char* CHARGER_URL  = "http://192.168.4.1";

#define OFFLINE_TIMEOUT_MS 45000 

class ChargeU {
 public:
  float val_voltage = 0.0;
  float val_current = 0.0;
  float val_session_energy = 0.0;
  float val_total_energy = 0.0;
  float val_duration = 0.0;
  std::string val_status_text = "Wait...";

  float val_target_amps = 6.0;
  bool val_led_on = false;
  bool val_ground_check = false;
  bool val_is_locked = false;
  bool val_session_active = false;

  bool val_is_online = false;
  bool new_data_available = false;

  bool initial_sync_done = false;
  unsigned long last_cmd_time = 0;
  bool cmd_pending = false;
  String pending_url = "";
  String pending_body = "";
  
  unsigned long verify_timer = 0; 
  bool verify_setup = false;      
  bool verify_pass = false;       

  unsigned long last_live_update = 0;
  unsigned long last_success_time = 0; // Таймер останнього успішного зв'язку
  float prev_duration = 0.0;

  int target_amps_pending = 0;
  int target_led_pending = -1;
  int target_gnd_pending = -1;
  int target_lock_pending = -1;
  int target_session_pending = -1;

  void setup() {
    ESP_LOGI("chargeu", "Starting Anti-Flap Bridge...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(CHARGER_SSID, CHARGER_PASS);
    last_success_time = millis(); // Даємо фору на старті
  }

  void loop() {

    if (val_is_online && (millis() - last_success_time > OFFLINE_TIMEOUT_MS)) {
        setOffline("Timeout (>45s)");
    }

    if (WiFi.status() != WL_CONNECTED) return;

    if (!initial_sync_done) {
        if (millis() > 5000) {
            fetchSettings();
            App.feed_wdt(); delay(50);
            fetchSecurity();
            initial_sync_done = true;
            new_data_available = true;
            last_live_update = millis();
        }
        return;
    }

    if (cmd_pending && (millis() - last_cmd_time > 1000)) {
        executeCommand();
        last_live_update = millis(); 
        return;
    }

    if (verify_timer > 0 && millis() > verify_timer) {
        if (verify_setup) { fetchSettings(); verify_setup = false; }
        if (verify_pass) { fetchSecurity(); verify_pass = false; }
        verify_timer = 0;
        new_data_available = true;
        return;
    }

    if (verify_timer == 0 && millis() - last_live_update > 7000) {
        fetchLiveData();
        last_live_update = millis();
    }
  }

  void markSuccess() {
      last_success_time = millis(); // Оновлюємо таймер життя
      if (!val_is_online) {
          val_is_online = true;
          ESP_LOGI("chargeu", "Connection RESTORED");
          new_data_available = true;
      }
  }

  void resetPending() {
      target_led_pending = -1; target_gnd_pending = -1;
      target_lock_pending = -1; target_session_pending = -1;
      target_amps_pending = 0;
  }

  void set_amps(float amps) { 
      resetPending(); target_amps_pending = (int)amps;
      queueCmd("/setup", "change=%24AMPS+" + String((int)amps), true, false); 
  }
  void set_led(bool state) { 
      resetPending(); target_led_pending = state ? 1 : 0;
      queueCmd("/setup", "change=%24LED+" + String(state ? 1 : 0), true, false); 
  }
  void set_ground(bool state) { 
      resetPending(); target_gnd_pending = state ? 1 : 0;
      queueCmd("/setup", "change=%24GROUND+" + String(state ? 1 : 0), true, false); 
  }
  
  void set_lock(bool state) {
      resetPending(); target_lock_pending = state ? 1 : 0;
      if (state) queueCmd("/pass", "change1=%24AVAIL+1", false, true);
      else       queueCmd("/pass", "change1=%24AVAIL+0", false, true);
  }
  
  void set_session(bool state) {
      if (!val_is_locked) return; 
      resetPending(); target_session_pending = state ? 1 : 0;
      queueCmd("/pass", "change1=%24TEMPS+" + String(state ? 1 : 0), false, true);
  }

 private:
  void queueCmd(String url, String body, bool check_setup, bool check_pass) {
    pending_url = url;
    pending_body = body;
    verify_setup = check_setup;
    verify_pass = check_pass;
    cmd_pending = true;
    last_cmd_time = millis();
  }

  void executeCommand() {
    HTTPClient http;
    http.setTimeout(5000);
    
    http.begin(String(CHARGER_URL) + pending_url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int code = http.POST(pending_body);
    http.end();
    App.feed_wdt();

    ESP_LOGI("chargeu", "CMD: %s -> %d", pending_body.c_str(), code);

    if (code == 200) {
        markSuccess(); // Вважаємо успішною дією
        
        if (target_amps_pending > 0) val_target_amps = target_amps_pending;
        if (target_led_pending != -1) val_led_on = (target_led_pending == 1);
        if (target_gnd_pending != -1) val_ground_check = (target_gnd_pending == 1);
        if (target_lock_pending != -1) val_is_locked = (target_lock_pending == 1);
        if (target_session_pending != -1) val_session_active = (target_session_pending == 1);
        
        new_data_available = true;
        verify_timer = millis() + 5000; 
    }
    cmd_pending = false;
    delay(50);
  }

  void fetchLiveData() {
    HTTPClient http;
    http.setTimeout(1500);
    if (!http.begin(String(CHARGER_URL) + "/")) return; // Тихий вихід, чекаємо таймауту
    
    int code = http.GET();
    if (code > 0) {
      markSuccess(); 

      String html = http.getString();
      val_status_text = extractStatusText(html);
      val_voltage = extractFloat(html, 3);
      val_current = extractFloat(html, 2);
      val_session_energy = extractFloat(html, 4); 
      
      float new_duration = extractDuration(html, 5);
      if (prev_duration > 0 && new_duration == 0) {
          verify_setup = true; verify_timer = millis() + 1000;
      }
      val_duration = new_duration;
      prev_duration = val_duration;

      new_data_available = true;
    }
    http.end();
    App.feed_wdt();
  }

  void fetchSettings() {
    HTTPClient http;
    http.setTimeout(2000);
    http.begin(String(CHARGER_URL) + "/setup");
    int code = http.GET();
    if (code > 0) {
      markSuccess(); 
      String html = http.getString();
      val_led_on = checkStatus(html, "Світлова");
      val_ground_check = checkStatus(html, "заземлення");
      float amps = extractTargetAmps(html);
      if (amps > 0) val_target_amps = amps;
      val_total_energy = extractTotalEnergy(html);
      new_data_available = true;
    }
    http.end();
    App.feed_wdt();
  }

  void fetchSecurity() {
    HTTPClient http;
    http.setTimeout(2000);
    http.begin(String(CHARGER_URL) + "/pass");
    int code = http.GET();
    if (code > 0) {
      markSuccess(); 
      String html = http.getString();
      val_is_locked = (html.indexOf("$AVAIL 0") != -1);
      if (val_is_locked) val_session_active = (html.indexOf("$TEMPS 0") != -1);
      else val_session_active = false;
      new_data_available = true;
    }
    http.end();
    App.feed_wdt();
  }

  void setOffline(String reason) {
      if (val_is_online) {
          val_is_online = false;
          val_status_text = reason.c_str();
          new_data_available = true;
          ESP_LOGW("chargeu", "Device OFFLINE: %s", reason.c_str());
      }
  }

  std::string extractStatusText(String& html) {
    int startDiv = html.indexOf("<div class=ins"); if (startDiv == -1) return "Unknown";
    int startText = html.indexOf(">", startDiv) + 1;
    while (html.substring(startText, startText+4) == "<br>" || html.charAt(startText) == '\n' || html.charAt(startText) == '\r') {
      if (html.substring(startText, startText+4) == "<br>") startText += 4; else startText++;
    }
    int endText = html.indexOf("<", startText);
    String res = html.substring(startText, endText); res.trim();
    return std::string(res.c_str());
  }
  float extractFloat(String& html, int index) {
    int pos = 0; for (int i=0; i<=index; i++) { pos = html.indexOf("class=ins", pos); if (pos == -1) return 0.0; pos += 9; }
    pos = html.indexOf(">", pos); if (pos == -1) return 0.0; pos++; int endPos = html.indexOf("<", pos);
    return html.substring(pos, endPos).toFloat();
  }
  int extractDuration(String& html, int index) {
    int pos = 0; for (int i=0; i<=index; i++) { pos = html.indexOf("class=ins", pos); if (pos == -1) return 0; pos += 9; }
    pos = html.indexOf(">", pos); if (pos == -1) return 0; pos++; int endPos = html.indexOf("<", pos);
    String t = html.substring(pos, endPos);
    int c1 = t.indexOf(':'); int c2 = t.lastIndexOf(':'); if (c1 == -1) return 0;
    return (t.substring(0, c1).toInt() * 3600) + (t.substring(c1+1, c2).toInt() * 60) + t.substring(c2+1).toInt();
  }
  bool checkStatus(String& html, String key) {
    int k = html.indexOf(key); if (k == -1) return false;
    int i = html.indexOf("class=ins", k); if (i == -1) return false;
    int s = html.indexOf(">", i) + 1; int e = html.indexOf("<", s);
    return (html.substring(s, e).indexOf("ВВІМК") != -1);
  }
  float extractTargetAmps(String& html) {
    String key = "selected value='$AMPS"; int startPos = html.indexOf(key); if (startPos == -1) return 0;
    int numStart = startPos + 22; int numEnd = html.indexOf("'", numStart);
    return html.substring(numStart, numEnd).toFloat();
  }
  float extractTotalEnergy(String& html) {
      int keyPos = html.indexOf("Передана потужність всього");
      if (keyPos == -1) return val_total_energy;
      int insPos = html.indexOf("class=ins", keyPos); if (insPos == -1) return val_total_energy;
      int start = html.indexOf(">", insPos) + 1;
      int end = html.indexOf(" ", start); if (end == -1) end = html.indexOf("<", start);
      return html.substring(start, end).toFloat();
  }
};

ChargeU *chargeu_device = new ChargeU();
