// ============================================================
// web_server_progmem.cpp  —  PROGMEM-only static file serving
// ============================================================
// Drop-in replacement for the LittleFS static-file section in
// web_server.cpp.  All other handlers (REST, WebSocket) are
// unchanged — only the static file / 404 section differs.
//
// Prerequisites
// ─────────────
//   1. scripts/embed_dashboard.py is listed as a pre-script in
//      platformio.ini (see below).
//   2. src/generated/ is on the compiler include path (it is by
//      default because PlatformIO adds src/ recursively).
//   3. LittleFS is NOT mounted for the dashboard — the partition
//      table can therefore give all of that space back to the app
//      partition if desired.
//
// platformio.ini additions
// ────────────────────────
//   [env:esp32s3_zero]           ; (or whichever env)
//   extra_scripts = pre:scripts/embed_dashboard.py
//   build_flags =
//       ${env_common.build_flags}
//       -DWEB_UI_PROGMEM          ; enable this translation unit
//       ...
//
// ============================================================

// #define WEB_UI_PROGMEM

#ifdef WEB_UI_PROGMEM

#pragma message("WEB_UI_PROGMEM is ENABLED")

#include "web_server.h"
#include "generated/web_ui.h"
#include <ESPAsyncWebServer.h>

// ── Static file handler ───────────────────────────────────────────────────────

/**
 * @brief Serve a single file from the PROGMEM manifest.
 *
 * Handles:
 *  • ETag / If-None-Match caching  (304 Not Modified)
 *  • Content-Encoding: gzip        (transparent to the browser)
 *  • Cache-Control: max-age=600    (10 min, same as the LittleFS variant)
 *  • SPA fallback via webui_find() (routes without extension → index.html)
 */
static void serveProgmem(AsyncWebServerRequest* request) {
    const String url = request->url();

    const WebUIFile* f = webui_find(url.c_str());
    if (!f) {
        request->send(404, "text/plain", "Not found");
        return;
    }

    // ETag cache check
    if (request->hasHeader("If-None-Match")) {
        if (request->header("If-None-Match") == f->etag) {
            request->send(304);
            return;
        }
    }

    // Copy data from PROGMEM into a heap buffer.
    // For large files (JS bundle ≈ 100–120 KB gzipped) this is a one-shot
    // allocation; ESPAsyncWebServer owns the AsyncWebServerResponse and frees
    // it after transmission.
    uint8_t* buf = (uint8_t*)malloc(f->size);
    if (!buf) {
        request->send(500, "text/plain", "OOM");
        return;
    }
    memcpy_P(buf, f->data, f->size);

    AsyncWebServerResponse* resp =
        request->beginResponse(200, f->mime, f->data, f->size);

    // Tell the browser the body is already compressed
    if (f->gzip) {
        resp->addHeader("Content-Encoding", "gzip");
    }

    resp->addHeader("Cache-Control", "max-age=600");
    resp->addHeader("ETag",          f->etag);

    free(buf);   // beginResponse copied the pointer, we can free our tmp buf
    request->send(resp);
}

// ── Route registration (call this from WebServer::registerRoutes) ─────────────

/**
 * @brief Register the PROGMEM static-file handler on the AsyncWebServer.
 *
 * Call this instead of (or after removing) the LittleFS serveStatic() block:
 *
 *   // OLD (LittleFS):
 *   // server->serveStatic("/", LittleFS, "/www/").setDefaultFile("index.html");
 *
 *   // NEW (PROGMEM):
 *   registerProgmemRoutes(server);
 */
void registerProgmemRoutes(AsyncWebServer* server) {
    // Serve every URL through the PROGMEM handler.
    // The handler itself performs the lookup and falls back to index.html for
    // unknown routes so that the React SPA router keeps working.
    server->onNotFound(serveProgmem);

    // Also wire up the root explicitly so "/" → index.html works without the
    // onNotFound path (some clients send exactly "/" without trailing slash).
    server->on("/", HTTP_GET, serveProgmem);

    Serial.printf("[Web] PROGMEM UI: %u files embedded\n", WEB_UI_FILE_COUNT);
    for (size_t i = 0; i < WEB_UI_FILE_COUNT; i++) {
        Serial.printf("[Web]   %-50s %6u B%s\n",
            WEB_UI_FILES[i].url,
            WEB_UI_FILES[i].size,
            WEB_UI_FILES[i].gzip ? " (gz)" : "");
    }
}


#else

#pragma message("WEB_UI_PROGMEM is DISABLED")

#endif // WEB_UI_PROGMEM
