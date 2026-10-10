#include <math.h>
#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "telemetry_store.h"
#include "wifi_ap.h"

static const char *TAG = "WIFI_AP";

static const char index_html[] =
    "<!doctype html><html lang=\"pt-br\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>Fenix Telemetria</title><style>"
    "body{font-family:sans-serif;margin:0;padding:16px;background:#101418;color:#e8edf2}"
    "h1{font-size:1.2rem;margin:0 0 12px}"
    ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:12px}"
    ".card{background:#1b222a;border-radius:8px;padding:12px}"
    ".card span{display:block;font-size:.8rem;color:#8fa1b3}"
    ".card b{font-size:1.6rem}"
    "#link{display:inline-block;padding:2px 10px;border-radius:12px;font-size:.85rem}"
    ".ok{background:#1e7a3c}.off{background:#8a2a2a}"
    "#reset{margin-top:16px;padding:8px 14px;border:0;border-radius:6px;background:#2b3641;color:#e8edf2}"
    "</style></head><body>"
    "<h1>Fenix Telemetria <span id=\"link\" class=\"off\">sem link</span></h1>"
    "<div class=\"grid\">"
    "<div class=\"card\"><span>Tens&atilde;o (V)</span><b id=\"v\">--</b></div>"
    "<div class=\"card\"><span>Corrente (A)</span><b id=\"i\">--</b></div>"
    "<div class=\"card\"><span>Pot&ecirc;ncia (W)</span><b id=\"p\">--</b></div>"
    "<div class=\"card\"><span>Energia usada (Wh)</span><b id=\"e\">--</b></div>"
    "<div class=\"card\"><span>Energia restante (Wh)</span><b id=\"r\">--</b></div>"
    "<div class=\"card\"><span>Autonomia (min)</span><b id=\"t\">--</b></div>"
    "</div><button id=\"reset\">Bateria carregada (zerar energia usada)</button><script>"
    "const f=(id,x,d)=>document.getElementById(id).textContent=x==null?'--':x.toFixed(d);"
    "async function tick(){try{const r=await fetch('/data');const d=await r.json();"
    "f('v',d.voltage,2);f('i',d.current,2);"
    "f('p',d.voltage==null||d.current==null?null:d.voltage*d.current,0);"
    "f('e',d.used_energy,1);f('r',d.energy_remaining,0);f('t',d.time_remaining_min,0);"
    "const l=document.getElementById('link');"
    "l.textContent=d.link_ok?'link ok':'sem link';l.className=d.link_ok?'ok':'off';"
    "}catch(e){}}"
    "document.getElementById('reset').onclick=async()=>{"
    "if(confirm('Zerar a energia usada? Use somente com a bateria 100% carregada.')){"
    "await fetch('/reset',{method:'POST'});tick();}};"
    "tick();setInterval(tick,1000);"
    "</script></body></html>";

// Acrescenta "nome":valor, em buf. JSON nao tem NaN/inf: valor desconhecido vira null.
// Retorna false se nao couber (nada e escrito alem do limite).
static bool append_field(char *buf, size_t size, size_t *used, const char *name, double value) {
    size_t room = size - *used;
    int n = isfinite(value)
        ? snprintf(buf + *used, room, "\"%s\":%.2f,", name, value)
        : snprintf(buf + *used, room, "\"%s\":null,", name);

    if (n < 0 || (size_t)n >= room) {
        return false;
    }

    *used += (size_t)n;
    return true;
}

static esp_err_t index_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t data_get_handler(httpd_req_t *req) {
    struct boat_state state;
    char buf[256];
    size_t used = 0;

    telemetry_store_get(&state);

    buf[used++] = '{';
    bool fits = append_field(buf, sizeof(buf), &used, "voltage", state.voltage_instant)
        && append_field(buf, sizeof(buf), &used, "current", state.current_instant)
        && append_field(buf, sizeof(buf), &used, "used_energy", state.used_energy)
        && append_field(buf, sizeof(buf), &used, "time_remaining_min", state.time_remaining_min)
        && append_field(buf, sizeof(buf), &used, "energy_remaining", state.energy_remaining);

    if (fits) {
        int n = snprintf(buf + used, sizeof(buf) - used, "\"link_ok\":%s}", state.link_ok ? "true" : "false");
        fits = n > 0 && (size_t)n < sizeof(buf) - used;
        used += fits ? (size_t)n : 0;
    }

    if (!fits) {
        return httpd_resp_send_500(req);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, buf, used);
}

static esp_err_t reset_post_handler(httpd_req_t *req) {
    telemetry_store_request_energy_reset();
    ESP_LOGI(TAG, "energy reset requested from web page");
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (id == WIFI_EVENT_AP_STACONNECTED) {
        const wifi_event_ap_staconnected_t *ev = data;
        ESP_LOGI(TAG, "client connected, aid=%d", ev->aid);
    } else if (id == WIFI_EVENT_AP_STADISCONNECTED) {
        const wifi_event_ap_stadisconnected_t *ev = data;
        ESP_LOGI(TAG, "client disconnected, aid=%d", ev->aid);
    }
}

static esp_err_t start_http_server(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = CONFIG_WIFI_AP_MAX_CONN + 1;
    config.lru_purge_enable = true;

    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        return ret;
    }

    const httpd_uri_t index_uri = {.uri = "/", .method = HTTP_GET, .handler = index_get_handler};
    const httpd_uri_t data_uri = {.uri = "/data", .method = HTTP_GET, .handler = data_get_handler};
    httpd_register_uri_handler(server, &index_uri);
    httpd_register_uri_handler(server, &data_uri);

    const httpd_uri_t reset_uri = {.uri = "/reset", .method = HTTP_POST, .handler = reset_post_handler};
    httpd_register_uri_handler(server, &reset_uri);

    return ESP_OK;
}

void init_wifi_ap(void) {
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to init netif, error code: %d", ret);
        return;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "failed to create event loop, error code: %d", ret);
        return;
    }

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&init_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to init wifi, error code: %d", ret);
        return;
    }

    ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to register wifi event handler, error code: %d", ret);
        return;
    }

    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len = strlen(CONFIG_WIFI_AP_SSID),
            .channel = CONFIG_WIFI_AP_CHANNEL,
            .max_connection = CONFIG_WIFI_AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    strlcpy((char *)wifi_config.ap.ssid, CONFIG_WIFI_AP_SSID, sizeof(wifi_config.ap.ssid));
    strlcpy((char *)wifi_config.ap.password, CONFIG_WIFI_AP_PASSWORD, sizeof(wifi_config.ap.password));

    ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (ret == ESP_OK) {
        ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    }
    if (ret == ESP_OK) {
        ret = esp_wifi_start();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to start access point, error code: %d", ret);
        return;
    }

    ret = start_http_server();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to start http server, error code: %d", ret);
        return;
    }

    ESP_LOGI(TAG, "access point \"%s\" up, open http://192.168.4.1", CONFIG_WIFI_AP_SSID);
}
