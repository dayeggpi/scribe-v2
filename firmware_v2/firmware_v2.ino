#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <SoftwareSerial.h>

// === Function Declarations ===
void connectToWiFi();
void setupWebServer();
void handleRoot();
void handleSubmit();
void handleImageUpload();
void handle404();
String getFormattedDateTime();
String formatCustomDate(String customDate);
void initializePrinter();
void printReceipt();
void printServerInfo();
void ensureLatinEncoding();
void setInverse(bool enable);
void setItalic(bool enable);
void setBold(bool enable);
void setUnderline(bool enable);
void setJustify(char j);
void setFontSize(char s);
void setLineSpacing(uint8_t dots);
String transcodeToCP1252(String s);
char asciiFallback(uint16_t uni);
int charsForSize(char fontSize);
void printLine(String line);
void printFormattedLine(String line, bool baseInverted);
void advancePaper(int lines);
int collectWrappedLines(String text, String* lines, int maxLines, int cpl);
void printWrapped(String text, bool inverted, char fontSize);
void printWrappedUpsideDown(String text, bool inverted, char fontSize);
void printQRCode(String data);

// === WiFi Configuration ===
const char* ssid = "PEW";
const char* password = "Au6ECQ2UdU9ywyN3jP";
const bool printerForceAscii = true;

// === Time Configuration ===
const long utcOffsetInSeconds = 0;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds, 60000);

// === Web Server ===
ESP8266WebServer server(80);

// === Printer Setup ===
SoftwareSerial printer(D4, D3); // D4=TX(GPIO2), D3=RX(GPIO0)

// === Storage for form data ===
struct Receipt {
  String message;
  String timestamp;
  bool hasData;
  bool showDate;
  bool inverted;
  bool upsideDown;
  bool qrCode;
  char fontSize;       // 'S', 'M', 'L'
  char justify;        // 'L', 'C', 'R'
  uint8_t lineSpacing; // dots; 0 = printer default (~30)
};

Receipt currentReceipt = {"", "", false, false, false, true, false, 'S', 'L', 0};

// === Image upload state ===
int  imgUploadW       = 0;
int  imgUploadH       = 0;
bool imgUploadStarted = false;

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Thermal Printer Server Starting ===");
  initializePrinter();
  connectToWiFi();
  setupWebServer();
  server.begin();
  Serial.println("Web server started");
  printServerInfo();
  timeClient.begin();
  Serial.println("Time client initialized");
  Serial.println("=== Setup Complete ===");
}

void loop() {
  server.handleClient();
  timeClient.update();
  if (currentReceipt.hasData) {
    printReceipt();
    currentReceipt.hasData = false;
  }
  delay(10);
}

// === WiFi Connection ===
void connectToWiFi() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  delay(300);

  const int maxRetries = 1;
  for (int attempt = 1; attempt <= maxRetries; attempt++) {
    Serial.printf("WiFi connect: %s\n", ssid);
    WiFi.begin(ssid, password);
    int waited = 0;
    while (WiFi.status() != WL_CONNECTED && waited < 80) {
      delay(500);
      Serial.print(".");
      waited++;
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi connected! IP: " + WiFi.localIP().toString());
      return;
    }
  }

  Serial.println("WiFi failed. Server not started.");
  setInverse(true);
  printLine("WIFI FAILED");
  setInverse(false);
  printLine("Check SSID/password/router");
  advancePaper(3);
  while (true) { delay(1000); }
}

// === Web Server Setup ===
void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/submit", HTTP_POST, handleSubmit);
  server.on("/submit", HTTP_GET, handleSubmit);
  server.on("/print-image", HTTP_POST,
    []() { server.send(200, "text/plain", "OK"); },
    handleImageUpload
  );
  server.onNotFound(handle404);
}

// === Web Server Handlers ===
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en" class="bg-gray-50 text-gray-900">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Life Receipt</title>
  <link rel="icon" type="image/png" sizes="16x16" href="data:image/png;base64,AAABAAEAEBAQAAEABAAoAQAAFgAAACgAAAAQAAAAIAAAAAEABAAAAAAAgAAAAAAAAAAAAAAAEAAAAAAAAAAAAAAA7e3tAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABEREQAAAAAAAAAAAAAAAAAAAAAAAAAAABEREREAAAAAAAAAAAAAAAAAAAAAAAAAABERERERAAAAAAAAAAAAAAAREREREREAAAAAAAAAAAAAEREREREREAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" />
  <script src="https://cdn.tailwindcss.com"></script>
  <script src="https://cdn.jsdelivr.net/npm/canvas-confetti@1.6.0/dist/confetti.browser.min.js"></script>
  <script defer>
    // === Tabs ===
    function showTab(tab) {
      ['text','image'].forEach(function(t) {
        document.getElementById('section-'+t).classList.toggle('hidden', t !== tab);
        var btn = document.getElementById('tab-'+t);
        btn.classList.toggle('bg-gray-900', t === tab);
        btn.classList.toggle('text-white',  t === tab);
        btn.classList.toggle('text-gray-500', t !== tab);
        btn.classList.toggle('bg-white', t !== tab);
      });
    }

    // === Toggle button groups ===
    function activateBtn(groupClass, activeId) {
      document.querySelectorAll('.' + groupClass).forEach(function(b) {
        var on = b.id === activeId;
        b.classList.toggle('bg-gray-900', on);
        b.classList.toggle('text-white', on);
        b.classList.toggle('bg-white', !on);
        b.classList.toggle('text-gray-600', !on);
        b.classList.toggle('border', !on);
        b.classList.toggle('border-gray-200', !on);
      });
    }

    function setSize(s) {
      document.getElementById('fontSize-val').value = s;
      activateBtn('size-btn', 'size-' + s);
      handleInput(document.querySelector('textarea[name="message"]'));
    }

    function setJustify(j) {
      document.getElementById('justify-val').value = j;
      activateBtn('just-btn', 'just-' + j);
      renderPreview(document.querySelector('textarea[name="message"]').value);
    }

    function updateSpacingLabel(el) {
      document.getElementById('spacing-label').textContent = el.value;
    }

    // === Text form ===
    function handleInput(el) {
      var s = document.getElementById('fontSize-val').value;
      var text = el ? el.value : document.querySelector('textarea[name="message"]').value;
      var printable = text.replace(/\*\*/g,'').replace(/_/g,'').replace(/==/g,'').length;
      document.getElementById('char-counter').textContent = printable + ' chars';
      renderPreview(text);
    }

    function renderPreview(text) {
      var pv = document.getElementById('msg-preview');
      if (!text) { pv.classList.add('hidden'); return; }
      pv.classList.remove('hidden');

      var j = document.getElementById('justify-val').value;
      var s = document.getElementById('fontSize-val').value;
      pv.style.textAlign = j === 'C' ? 'center' : j === 'R' ? 'right' : 'left';
      var fz = { 'S': '0.8rem', 'M': '1.1rem', 'L': '1.5rem' }[s] || '0.8rem';
      pv.style.fontSize = fz;

      var html = text.split('\n').map(function(line) {
        var res = '', i = 0, n = line.length;
        var invOn = false, boldOn = false, underOn = false;
        while (i < n) {
          if (line[i] === '=' && i+1 < n && line[i+1] === '=') {
            if (invOn) res += '</span>'; else res += '<span style="background:#111;color:#fff">';
            invOn = !invOn; i += 2;
          } else if (line[i] === '*' && i+1 < n && line[i+1] === '*') {
            if (boldOn) res += '</strong>'; else res += '<strong>';
            boldOn = !boldOn; i += 2;
          } else if (line[i] === '_') {
            if (underOn) res += '</u>'; else res += '<u>';
            underOn = !underOn; i++;
          } else {
            var c = line[i];
            res += c==='&'?'&amp;':c==='<'?'&lt;':c==='>'?'&gt;':c;
            i++;
          }
        }
        if (underOn) res += '</u>';
        if (boldOn)  res += '</strong>';
        if (invOn)   res += '</span>';
        return res;
      }).join('\n');
      pv.innerHTML = html;
    }

    function submitForm(e) {
      if (e) e.preventDefault();
      var form = document.getElementById('receipt-form');
      var btn  = document.getElementById('submit-btn');
      btn.disabled = true;
      btn.textContent = 'Sending…';
      fetch('/submit', { method: 'POST', body: new FormData(form) })
        .then(function(r) { return r.text(); })
        .then(function() {
          btn.textContent = '✓ Sent!';
          confetti({ particleCount: 100, spread: 70, origin: { y: 0.6 } });
          setTimeout(function() {
            form.reset();
            document.getElementById('char-counter').textContent = '0 chars';
            document.getElementById('msg-preview').classList.add('hidden');
            // reset toggle buttons to defaults
            document.getElementById('fontSize-val').value = 'S';
            document.getElementById('justify-val').value = 'L';
            activateBtn('size-btn', 'size-S');
            activateBtn('just-btn', 'just-L');
            document.getElementById('lineSpacing-range').value = 30;
            document.getElementById('spacing-label').textContent = '30';
            btn.textContent = 'Send';
            btn.disabled = false;
          }, 2000);
        })
        .catch(function() {
          btn.textContent = 'Error — retry';
          btn.disabled = false;
        });
    }

    // === Image form ===
    var imgBitmap = null, imgW = 0, imgH = 0;

    function updatePreview() {
      var bpr = Math.ceil(imgW / 8);
      var pv = document.getElementById('img-preview');
      pv.width = imgW; pv.height = imgH; pv.style.display = 'block';
      var pCtx = pv.getContext('2d');
      var pd = pCtx.createImageData(imgW, imgH);
      for (var y = 0; y < imgH; y++) {
        for (var x = 0; x < imgW; x++) {
          var bit = (imgBitmap[y*bpr + (x>>3)] >> (7-(x&7))) & 1;
          var idx = (y*imgW+x)*4, v = bit ? 0 : 255;
          pd.data[idx]=v; pd.data[idx+1]=v; pd.data[idx+2]=v; pd.data[idx+3]=255;
        }
      }
      pCtx.putImageData(pd, 0, 0);
      document.getElementById('img-dims').textContent = imgW + 'x' + imgH + ' px';
      document.getElementById('img-submit').disabled = false;
    }

    function clearImage() {
      imgBitmap = null; imgW = 0; imgH = 0;
      document.getElementById('img-preview').style.display = 'none';
      document.getElementById('img-dims').textContent = '';
      document.getElementById('img-submit').disabled = true;
      document.getElementById('img-file-input').value = '';
    }

    function invertImage() {
      if (!imgBitmap) return;
      for (var i = 0; i < imgBitmap.length; i++) imgBitmap[i] ^= 0xFF;
      updatePreview();
    }

    function handleImageSelect(input) {
      if (!input.files || !input.files[0]) return;
      var reader = new FileReader();
      reader.onload = function(e) {
        var img = new Image();
        img.onload = function() {
          var w = 384;
          var h = Math.round(img.height * w / img.width);
          var src = document.createElement('canvas');
          src.width = w; src.height = h;
          var ctx = src.getContext('2d');
          ctx.fillStyle = 'white';
          ctx.fillRect(0, 0, w, h);
          ctx.drawImage(img, 0, 0, w, h);
          var px = ctx.getImageData(0, 0, w, h).data;
          var bpr = Math.ceil(w / 8);
          imgBitmap = new Uint8Array(bpr * h);
          imgW = w; imgH = h;
          for (var y = 0; y < h; y++) {
            for (var x = 0; x < w; x++) {
              var p = (y * w + x) * 4;
              var lum = (px[p]*299 + px[p+1]*587 + px[p+2]*114) / 1000;
              if (lum < 128) imgBitmap[y * bpr + (x >> 3)] |= (0x80 >> (x & 7));
            }
          }
          updatePreview();
        };
        img.src = e.target.result;
      };
      reader.readAsDataURL(input.files[0]);
    }

    function rotateBitmap180(src, w, h) {
      var bpr = Math.ceil(w / 8);
      var dst = new Uint8Array(bpr * h);
      for (var y = 0; y < h; y++) {
        var ry = h - 1 - y;
        for (var x = 0; x < w; x++) {
          var rx = w - 1 - x;
          if ((src[ry*bpr + (rx>>3)] >> (7-(rx&7))) & 1)
            dst[y*bpr + (x>>3)] |= (0x80 >> (x&7));
        }
      }
      return dst;
    }

    function submitImage(e) {
      e.preventDefault();
      if (!imgBitmap) return;
      var btn = document.getElementById('img-submit');
      btn.disabled = true;
      btn.textContent = 'Sending…';
      var bitmapToSend = document.getElementById('img-flip-check').checked
        ? imgBitmap
        : rotateBitmap180(imgBitmap, imgW, imgH);
      var fd = new FormData();
      fd.append('image', new Blob([bitmapToSend]), imgW + 'x' + imgH + '.bin');
      fetch('/print-image', { method: 'POST', body: fd })
        .then(function(r) { return r.text(); })
        .then(function() {
          btn.textContent = '✓ Printed!';
          confetti({ particleCount: 50, spread: 60, origin: { y: 0.6 } });
          setTimeout(function() {
            clearImage();
            btn.textContent = 'Print Image';
          }, 2000);
        })
        .catch(function() { btn.textContent = 'Error — retry'; btn.disabled = false; });
    }
  </script>
</head>
<body class="flex flex-col min-h-screen justify-between items-center py-12 px-4 font-sans">
  <main class="w-full max-w-md">
    <h1 class="text-3xl font-semibold mb-6 text-gray-900 tracking-tight text-center">Life Receipt:</h1>

    <!-- Tabs -->
    <div class="flex gap-2 mb-4">
      <button id="tab-text"  onclick="showTab('text')"  class="flex-1 py-2 rounded-xl text-sm font-medium transition-all bg-gray-900 text-white">Text</button>
      <button id="tab-image" onclick="showTab('image')" class="flex-1 py-2 rounded-xl text-sm font-medium transition-all bg-white text-gray-500 border border-gray-200">Image</button>
    </div>

    <!-- Text section -->
    <div id="section-text">
      <form id="receipt-form" onsubmit="submitForm(event)" class="bg-white shadow-2xl rounded-3xl p-8 space-y-5 border border-gray-100">
        <textarea
          name="message"
          oninput="handleInput(this)"
          placeholder="Type your receipt&#x2026;"
          class="font-mono text-sm p-4 rounded-xl border border-gray-200 focus:outline-none focus:ring-2 focus:ring-gray-400 focus:border-transparent resize-none text-gray-800 placeholder-gray-400 w-full"
          cols="32" rows="4" required autofocus
        ></textarea>

        <div id="msg-preview" class="hidden font-mono text-sm border border-dashed border-gray-200 rounded-xl p-4 bg-gray-50 whitespace-pre-wrap" style="width:calc(32ch + 2rem + 2px);overflow-wrap:break-word;"></div>

        <!-- hints + counter -->
        <div class="flex flex-wrap justify-between items-center text-xs text-gray-400 gap-1">
          <span><code class="font-mono">==inv==</code> &nbsp;<code class="font-mono">**bold**</code> &nbsp;<code class="font-mono">_under_</code></span>
          <span id="char-counter">0 chars</span>
        </div>

        <!-- font size -->
        <div class="flex items-center gap-3">
          <span class="text-sm text-gray-500 w-14 shrink-0">Size</span>
          <div class="flex gap-1">
            <button type="button" id="size-S" class="size-btn px-3 py-1 rounded-lg text-sm font-medium bg-gray-900 text-white" onclick="setSize('S')">S</button>
            <button type="button" id="size-M" class="size-btn px-3 py-1 rounded-lg text-sm font-medium bg-white text-gray-600 border border-gray-200" onclick="setSize('M')">M</button>
            <button type="button" id="size-L" class="size-btn px-3 py-1 rounded-lg text-sm font-medium bg-white text-gray-600 border border-gray-200" onclick="setSize('L')">L</button>
          </div>
          <input type="hidden" name="fontSize" id="fontSize-val" value="S" />
        </div>

        <!-- justify -->
        <div class="flex items-center gap-3">
          <span class="text-sm text-gray-500 w-14 shrink-0">Align</span>
          <div class="flex gap-1">
            <button type="button" id="just-L" class="just-btn px-3 py-1 rounded-lg text-sm font-medium bg-gray-900 text-white" onclick="setJustify('L')">Left</button>
            <button type="button" id="just-C" class="just-btn px-3 py-1 rounded-lg text-sm font-medium bg-white text-gray-600 border border-gray-200" onclick="setJustify('C')">Center</button>
            <button type="button" id="just-R" class="just-btn px-3 py-1 rounded-lg text-sm font-medium bg-white text-gray-600 border border-gray-200" onclick="setJustify('R')">Right</button>
          </div>
          <input type="hidden" name="justify" id="justify-val" value="L" />
        </div>

        <!-- line spacing -->
        <div class="flex items-center gap-3">
          <span class="text-sm text-gray-500 w-14 shrink-0">Spacing</span>
          <input type="range" name="lineSpacing" id="lineSpacing-range" min="24" max="60" value="30" step="2"
            class="flex-1 accent-gray-800" oninput="updateSpacingLabel(this)" />
          <span id="spacing-label" class="text-xs text-gray-400 w-6 text-right">30</span>
        </div>

        <!-- checkboxes -->
        <div class="flex flex-wrap gap-x-5 gap-y-3 text-sm text-gray-700">
          <label class="flex items-center gap-2 cursor-pointer select-none">
            <input type="checkbox" name="showDate" value="1" class="rounded accent-gray-800" />
            <span>Date</span>
          </label>
          <label class="flex items-center gap-2 cursor-pointer select-none">
            <input type="checkbox" name="inverted" value="1" class="rounded accent-gray-800" />
            <span>Inverted</span>
          </label>
          <label class="flex items-center gap-2 cursor-pointer select-none">
            <input type="checkbox" name="upsideDown" value="1" class="rounded accent-gray-800" />
            <span>Flip</span>
          </label>
          <label class="flex items-center gap-2 cursor-pointer select-none">
            <input type="checkbox" name="qrCode" value="1" class="rounded accent-gray-800" />
            <span>QR Code</span>
          </label>
        </div>

        <button id="submit-btn" type="submit"
          class="w-full bg-gray-900 hover:bg-gray-800 text-white py-3 rounded-xl font-medium transition-all duration-200 hover:scale-[1.02] hover:shadow-lg disabled:opacity-50 disabled:cursor-not-allowed disabled:scale-100 disabled:shadow-none">
          Send
        </button>
      </form>
    </div>

    <!-- Image section -->
    <div id="section-image" class="hidden">
      <form onsubmit="submitImage(event)" class="bg-white shadow-2xl rounded-3xl p-8 space-y-5 border border-gray-100">
        <label class="block">
          <span class="text-sm text-gray-600 block mb-2">Select image (scaled to 384 px wide)</span>
          <input id="img-file-input" type="file" accept="image/*" onchange="handleImageSelect(this)"
            class="block w-full text-sm text-gray-600
                   file:mr-4 file:py-2 file:px-4 file:rounded-lg file:border-0
                   file:text-sm file:font-medium file:bg-gray-100 file:text-gray-700
                   hover:file:bg-gray-200 cursor-pointer" />
        </label>
        <div>
          <canvas id="img-preview" style="display:none;max-width:100%;border-radius:8px;border:1px solid #e5e7eb;image-rendering:pixelated;"></canvas>
          <div id="img-dims" class="text-xs text-gray-400 text-right mt-1"></div>
        </div>
        <div class="flex gap-2">
          <button type="button" onclick="invertImage()"
            class="flex-1 bg-white border border-gray-200 hover:bg-gray-50 text-gray-700 py-2 rounded-xl text-sm font-medium transition-all duration-200">
            Invert
          </button>
          <button type="button" onclick="clearImage()"
            class="flex-1 bg-white border border-gray-200 hover:bg-gray-50 text-gray-700 py-2 rounded-xl text-sm font-medium transition-all duration-200">
            Clear
          </button>
        </div>
        <div class="flex flex-wrap gap-x-5 gap-y-3 text-sm text-gray-700">
          <label class="flex items-center gap-2 cursor-pointer select-none">
            <input type="checkbox" id="img-flip-check" class="rounded accent-gray-800" />
            <span>Flip</span>
          </label>
        </div>
        <button id="img-submit" type="submit" disabled
          class="w-full bg-gray-900 hover:bg-gray-800 text-white py-3 rounded-xl font-medium transition-all duration-200 hover:scale-[1.02] hover:shadow-lg disabled:opacity-50 disabled:cursor-not-allowed disabled:scale-100 disabled:shadow-none">
          Print Image
        </button>
      </form>
    </div>
  </main>

  <footer class="text-sm text-gray-400 mt-16">
    Designed with love by <a href="https://urbancircles.club" target="_blank" class="text-gray-500 hover:text-gray-700 transition-colors duration-200 underline decoration-gray-300 hover:decoration-gray-500 underline-offset-2">Peter / Urban Circles</a>. Iterated by <a href="https://github.com/dayeggpi" target="_blank"
      class="text-gray-500 hover:text-gray-700 transition-colors duration-200 underline decoration-gray-300 hover:decoration-gray-500 underline-offset-2">dayeggpi</a>
  </footer>  
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void handleSubmit() {
  if (!server.hasArg("message") || server.arg("message").length() == 0) {
    server.send(400, "text/plain", "Missing message");
    return;
  }
  currentReceipt.message    = server.arg("message");
  currentReceipt.showDate   = server.hasArg("showDate");
  currentReceipt.inverted   = server.hasArg("inverted");
  currentReceipt.upsideDown = !server.hasArg("upsideDown");
  currentReceipt.qrCode     = server.hasArg("qrCode");

  String fs = server.hasArg("fontSize") ? server.arg("fontSize") : "S";
  currentReceipt.fontSize = (fs == "M") ? 'M' : (fs == "L") ? 'L' : 'S';

  String jt = server.hasArg("justify") ? server.arg("justify") : "L";
  currentReceipt.justify = (jt == "C") ? 'C' : (jt == "R") ? 'R' : 'L';

  currentReceipt.lineSpacing = server.hasArg("lineSpacing")
    ? (uint8_t)constrain(server.arg("lineSpacing").toInt(), 24, 60)
    : 30;

  if (currentReceipt.showDate) {
    if (server.hasArg("date") && server.arg("date").length() > 0)
      currentReceipt.timestamp = formatCustomDate(server.arg("date"));
    else
      currentReceipt.timestamp = getFormattedDateTime();
  }
  currentReceipt.hasData = true;

  Serial.println("=== New Receipt ===");
  Serial.println("Msg: " + currentReceipt.message);
  Serial.printf("Font:%c Justify:%c Spacing:%d\n",
    currentReceipt.fontSize, currentReceipt.justify, currentReceipt.lineSpacing);
  Serial.println("==================");

  server.send(200, "text/plain", "OK");
}

void handleImageUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String fn = upload.filename;
    int xi = fn.indexOf('x');
    if (xi < 1) { imgUploadW = 0; return; }
    imgUploadW = fn.substring(0, xi).toInt();
    imgUploadH = fn.substring(xi + 1).toInt();
    imgUploadStarted = false;
    Serial.printf("Image upload start: %dx%d\n", imgUploadW, imgUploadH);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (!imgUploadStarted) {
      if (imgUploadW <= 0 || imgUploadH <= 0) return;
      int bpr = (imgUploadW + 7) / 8;
      printer.write(0x1D); printer.write(0x76); printer.write(0x30); printer.write((uint8_t)0x00);
      printer.write((uint8_t)(bpr & 0xFF));         printer.write((uint8_t)((bpr >> 8) & 0xFF));
      printer.write((uint8_t)(imgUploadH & 0xFF));  printer.write((uint8_t)((imgUploadH >> 8) & 0xFF));
      imgUploadStarted = true;
    }
    for (size_t i = 0; i < upload.currentSize; i++)
      printer.write(upload.buf[i]);
  } else if (upload.status == UPLOAD_FILE_END) {
    advancePaper(2);
    Serial.printf("Image done: %u bytes\n", upload.totalSize);
  }
}

void handle404() {
  server.send(404, "text/plain", "Page not found");
}

// === Time Utilities ===
String getFormattedDateTime() {
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  time_t rawTime = epochTime;
  struct tm* timeInfo = gmtime(&rawTime);

  String dayNames[]   = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  String monthNames[] = {"Jan","Feb","Mar","Apr","May","Jun",
                         "Jul","Aug","Sep","Oct","Nov","Dec"};

  String formatted = dayNames[timeInfo->tm_wday] + ", ";
  formatted += String(timeInfo->tm_mday < 10 ? "0" : "") + String(timeInfo->tm_mday) + " ";
  formatted += monthNames[timeInfo->tm_mon] + " ";
  formatted += String(timeInfo->tm_year + 1900);
  return formatted;
}

String formatCustomDate(String customDate) {
  String monthNames[] = {"Jan","Feb","Mar","Apr","May","Jun",
                         "Jul","Aug","Sep","Oct","Nov","Dec"};
  int day = 0, month = 0, year = 0;

  if (customDate.indexOf('-') != -1) {
    int fd = customDate.indexOf('-');
    int sd = customDate.indexOf('-', fd + 1);
    if (fd != -1 && sd != -1) {
      year  = customDate.substring(0, fd).toInt();
      month = customDate.substring(fd + 1, sd).toInt();
      day   = customDate.substring(sd + 1).toInt();
    }
  } else if (customDate.indexOf('/') != -1) {
    int fs = customDate.indexOf('/');
    int ss = customDate.indexOf('/', fs + 1);
    if (fs != -1 && ss != -1) {
      day   = customDate.substring(0, fs).toInt();
      month = customDate.substring(fs + 1, ss).toInt();
      year  = customDate.substring(ss + 1).toInt();
    }
  }

  if (day < 1 || day > 31 || month < 1 || month > 12 || year < 1900 || year > 2100)
    return getFormattedDateTime();
  return String(day < 10 ? "0" : "") + String(day) + " " + monthNames[month - 1] + " " + String(year);
}

// === Printer Initialization ===
void initializePrinter() {
  printer.begin(9600);
  delay(500);
  printer.write(0x1B); printer.write('@');  // ESC @ — full reset
  delay(50);
  ensureLatinEncoding();
  printer.write(0x1B); printer.write('7');
  printer.write(15);   // heating dots
  printer.write(150);  // heating time
  printer.write(250);  // heating interval
  Serial.println("Printer initialized");
}

void ensureLatinEncoding() {
  // 701print-driver-board.pdf: FS . (28,46) cancels Chinese character mode.
  printer.write(0x1C); printer.write('.');
  printer.write(0x1B); printer.write('R'); printer.write((uint8_t)1);  // ESC R 1 => France set
  printer.write(0x1B); printer.write('t'); printer.write((uint8_t)0);  // ESC t 0 => CP437 table
  printer.write(0x1D); printer.write('t'); printer.write((uint8_t)0);  // GS t 0 (clone compatibility)
}

char asciiFallback(uint16_t uni) {
  switch (uni) {
    case 0x00E0: case 0x00E1: case 0x00E2: case 0x00E4: case 0x00E5: return 'a';
    case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C4: case 0x00C5: return 'A';
    case 0x00E8: case 0x00E9: case 0x00EA: case 0x00EB: return 'e';
    case 0x00C8: case 0x00C9: case 0x00CA: case 0x00CB: return 'E';
    case 0x00EC: case 0x00ED: case 0x00EE: case 0x00EF: return 'i';
    case 0x00CC: case 0x00CD: case 0x00CE: case 0x00CF: return 'I';
    case 0x00F2: case 0x00F3: case 0x00F4: case 0x00F6: return 'o';
    case 0x00D2: case 0x00D3: case 0x00D4: case 0x00D6: return 'O';
    case 0x00F9: case 0x00FA: case 0x00FB: case 0x00FC: return 'u';
    case 0x00D9: case 0x00DA: case 0x00DB: case 0x00DC: return 'U';
    case 0x00E7: return 'c';
    case 0x00C7: return 'C';
    case 0x00FF: return 'y';
    case 0x00DF: return 's';
    default: return 0;
  }
}

// === Printer Helpers ===
void setInverse(bool enable) {
  printer.write(0x1D); printer.write('B');
  printer.write(enable ? 1 : 0);
}

void setItalic(bool enable) {
  printer.write(0x1B);
  printer.write(enable ? 0x34 : 0x35);
}

void setBold(bool enable) {
  printer.write(0x1B); printer.write('E');
  printer.write(enable ? 1 : 0);
}

void setUnderline(bool enable) {
  printer.write(0x1B); printer.write('-');
  printer.write(enable ? 1 : 0);
}

// j = 'L' (0), 'C' (1), 'R' (2)
void setJustify(char j) {
  uint8_t n = (j == 'C') ? 1 : (j == 'R') ? 2 : 0;
  printer.write(0x1B); printer.write('a');
  printer.write(n);
}

// s = 'S' (1x), 'M' (2x), 'L' (3x)  —  GS ! command
void setFontSize(char s) {
  uint8_t n = (s == 'L') ? 0x22 : (s == 'M') ? 0x11 : 0x00;
  printer.write(0x1D); printer.write('!');
  printer.write(n);
}

// dots=0 → ESC 2 (printer default ~30); dots>0 → ESC 3 n
void setLineSpacing(uint8_t dots) {
  if (dots == 0) {
    printer.write(0x1B); printer.write('2');
  } else {
    printer.write(0x1B); printer.write('3');
    printer.write(dots);
  }
}

int charsForSize(char fontSize) {
  if (fontSize == 'L') return 10;
  if (fontSize == 'M') return 16;
  return 32;
}

// PC437 lookup table: Unicode codepoint → PC437 byte.
// Accented chars land at 0x80-0x9F — below GB2312 first-byte range (0xA1-0xF7),
// so they print as single glyphs even if the printer stays in Chinese mode.
// Characters with no PC437 glyph use the nearest ASCII fallback.
struct CP437Entry { uint16_t uni; uint8_t cp; };
static const CP437Entry cp437Map[] = {
  {0x00C7, 0x80}, // Ç      {0x00FC, 0x81}, // ü
  {0x00E9, 0x82}, // é      {0x00E2, 0x83}, // â
  {0x00E4, 0x84}, // ä      {0x00E0, 0x85}, // à
  {0x00E5, 0x86}, // å      {0x00E7, 0x87}, // ç
  {0x00EA, 0x88}, // ê      {0x00EB, 0x89}, // ë
  {0x00E8, 0x8A}, // è      {0x00EF, 0x8B}, // ï
  {0x00EE, 0x8C}, // î      {0x00EC, 0x8D}, // ì
  {0x00C4, 0x8E}, // Ä      {0x00C5, 0x8F}, // Å
  {0x00C9, 0x90}, // É      {0x00E6, 0x91}, // æ
  {0x00C6, 0x92}, // Æ      {0x00F4, 0x93}, // ô
  {0x00F6, 0x94}, // ö      {0x00F2, 0x95}, // ò
  {0x00FB, 0x96}, // û      {0x00F9, 0x97}, // ù
  {0x00FF, 0x98}, // ÿ      {0x00D6, 0x99}, // Ö
  {0x00DC, 0x9A}, // Ü
  // 0xA0-range: safe if FS. worked; may still cause issues on some chipsets
  {0x00E1, 0xA0}, // á      {0x00ED, 0xA1}, // í
  {0x00F3, 0xA2}, // ó      {0x00FA, 0xA3}, // ú
  {0x00F1, 0xA4}, // ñ      {0x00D1, 0xA5}, // Ñ
  {0x00AA, 0xA6}, // ª      {0x00BA, 0xA7}, // º
  {0x00DF, 0xE1}, // ß      {0x00B5, 0xE6}, // µ
  {0x00B0, 0xF8}, // °
  // Uppercase without PC437 glyph → nearest ASCII
  {0x00C0, 'A'}, // À       {0x00C2, 'A'}, // Â
  {0x00C8, 'E'}, // È       {0x00CA, 'E'}, // Ê
  {0x00CB, 'E'}, // Ë       {0x00CE, 'I'}, // Î
  {0x00CF, 'I'}, // Ï       {0x00D4, 'O'}, // Ô
  {0x00D9, 'U'}, // Ù       {0x00DB, 'U'}, // Û
  {0x00CC, 'I'}, // Ì       {0x00CD, 'I'}, // Í
  {0x00D2, 'O'}, // Ò       {0x00D3, 'O'}, // Ó
  {0x00DA, 'U'}, // Ú       {0x00D5, 'O'}, // Õ
  {0x00F5, 'o'}, // õ       {0x0152, 'O'}, // Œ
  {0x0153, 'o'}, // œ       {0x0160, 'S'}, // Š
  {0x0161, 's'}, // š       {0x017D, 'Z'}, // Ž
  {0x017E, 'z'}, // ž       {0x0178, 'Y'}, // Ÿ
};
static const int cp437MapSize = (int)(sizeof(cp437Map) / sizeof(cp437Map[0]));

static uint8_t lookupCP437(uint16_t uni) {
  for (int i = 0; i < cp437MapSize; i++)
    if (cp437Map[i].uni == uni) return cp437Map[i].cp;
  return 0;
}

// Convert UTF-8 -> CP1252 (Latin-1 + Windows extras). ASCII passes through unchanged.
String transcodeToCP1252(String s) {
  String out;
  out.reserve(s.length());
  int i = 0, len = (int)s.length();
  while (i < len) {
    uint8_t b = (uint8_t)s[i];
    if (i + 3 < len &&
        b == 0xC3 &&
        (uint8_t)s[i + 1] == 0x83 &&
        (uint8_t)s[i + 2] == 0xC2 &&
        (((uint8_t)s[i + 3]) & 0xC0) == 0x80) {
      uint8_t rb2 = (uint8_t)s[i + 3];
      uint16_t uni = ((uint16_t)(0xC3 & 0x1F) << 6) | (rb2 & 0x3F);
      uint8_t cp = lookupCP437(uni);
      if (cp) out += (char)cp;
      i += 4;
      continue;
    }
    if (b < 0x80) {
      out += (char)b;
      i++;
    } else if ((b & 0xE0) == 0xC0 && i + 1 < len) {
      uint8_t b2 = (uint8_t)s[i + 1];
      if ((b2 & 0xC0) == 0x80) {
        uint16_t uni = ((uint16_t)(b & 0x1F) << 6) | (b2 & 0x3F);

        // Handle combining diacritics often produced by web/mobile keyboards.
        if (uni >= 0x0300 && uni <= 0x036F && out.length() > 0) {
          char base = out[out.length() - 1];
          uint8_t repl = 0;

          if (uni == 0x0301) { // acute
            if (base == 'e') repl = 0x82; else if (base == 'E') repl = 0x90;
            else if (base == 'a') repl = 0xA0; else if (base == 'i') repl = 0xA1;
            else if (base == 'o') repl = 0xA2; else if (base == 'u') repl = 0xA3;
          } else if (uni == 0x0300) { // grave
            if (base == 'a') repl = 0x85; else if (base == 'e') repl = 0x8A;
            else if (base == 'i') repl = 0x8D; else if (base == 'o') repl = 0x95;
            else if (base == 'u') repl = 0x97;
          } else if (uni == 0x0302) { // circumflex
            if (base == 'a') repl = 0x83; else if (base == 'e') repl = 0x88;
            else if (base == 'i') repl = 0x8C; else if (base == 'o') repl = 0x93;
            else if (base == 'u') repl = 0x96;
          } else if (uni == 0x0308) { // diaeresis
            if (base == 'a') repl = 0x84; else if (base == 'e') repl = 0x89;
            else if (base == 'i') repl = 0x8B; else if (base == 'o') repl = 0x94;
            else if (base == 'u') repl = 0x81; else if (base == 'y') repl = 0x98;
          } else if (uni == 0x0327) { // cedilla
            if (base == 'c') repl = 0x87;
          }

          if (repl) out.setCharAt(out.length() - 1, (char)repl);
        } else {
          uint8_t cp = lookupCP437(uni);
          if (printerForceAscii) {
            char a = asciiFallback(uni);
            if (a) out += a;
          } else if (cp) {
            out += (char)cp;
          }
        }
      }
      i += 2;
    } else if ((b & 0xF0) == 0xE0 && i + 2 < len) {
      uint8_t b2 = (uint8_t)s[i + 1];
      uint8_t b3 = (uint8_t)s[i + 2];
      if (b == 0xE2 && b2 == 0x80 && (b3 == 0x99 || b3 == 0x98)) out += '\'';
      else if (b == 0xE2 && b2 == 0x80 && (b3 == 0x9C || b3 == 0x9D)) out += '"';
      else if (b == 0xE2 && b2 == 0x80 && (b3 == 0x93 || b3 == 0x94)) out += '-';
      i += 3;
    } else if ((b & 0xF8) == 0xF0 && i + 3 < len) {
      i += 4;  // 4-byte (emoji) — drop
    } else {
      i++;
    }
  }
  return out;
}

void printLine(String line) {
  ensureLatinEncoding();
  printer.println(transcodeToCP1252(line));
}

// Prints one line handling ==inv==, **bold**, _under_ inline markers.
void printFormattedLine(String line, bool baseInverted) {
  ensureLatinEncoding();
  bool inInv = false, inBold = false, inUnder = false;
  setInverse(baseInverted);
  setBold(false);
  setUnderline(false);
  String seg = "";
  int len = (int)line.length();

  for (int i = 0; i < len; i++) {
    if (i + 1 < len && line[i] == '=' && line[i + 1] == '=') {
      if (seg.length()) { ensureLatinEncoding(); printer.print(transcodeToCP1252(seg)); seg = ""; }
      inInv = !inInv;
      setInverse(baseInverted ^ inInv);
      i++;
    } else if (i + 1 < len && line[i] == '*' && line[i + 1] == '*') {
      if (seg.length()) { ensureLatinEncoding(); printer.print(transcodeToCP1252(seg)); seg = ""; }
      inBold = !inBold;
      setBold(inBold);
      i++;
    } else if (line[i] == '_') {
      if (seg.length()) { ensureLatinEncoding(); printer.print(transcodeToCP1252(seg)); seg = ""; }
      inUnder = !inUnder;
      setUnderline(inUnder);
    } else {
      seg += line[i];
    }
  }
  if (seg.length()) { ensureLatinEncoding(); printer.print(transcodeToCP1252(seg)); }
  printer.println();
  // reset modes at end of every line
  setInverse(false);
  setBold(false);
  setUnderline(false);
}

void advancePaper(int lines) {
  for (int i = 0; i < lines; i++) printer.write(0x0A);
}

int collectWrappedLines(String text, String* lines, int maxLines, int cpl) {
  int lineCount = 0;
  int start = 0;
  int tlen = (int)text.length();
  while (start <= tlen && lineCount < maxLines) {
    int nlPos = text.indexOf('\n', start);
    String seg;
    if (nlPos == -1) { seg = text.substring(start); start = tlen + 1; }
    else             { seg = text.substring(start, nlPos); start = nlPos + 1; }
    while (seg.length() > 0 && lineCount < maxLines) {
      if ((int)seg.length() <= cpl) {
        lines[lineCount++] = seg; break;
      }
      int cut = seg.lastIndexOf(' ', cpl);
      if (cut <= 0) cut = cpl;
      lines[lineCount++] = seg.substring(0, cut);
      seg = seg.substring(cut);
      seg.trim();
    }
  }
  return lineCount;
}

void printWrapped(String text, bool inverted, char fontSize) {
  String lines[100];
  int lineCount = collectWrappedLines(text, lines, 100, charsForSize(fontSize));
  for (int i = 0; i < lineCount; i++) printFormattedLine(lines[i], inverted);
}

void printWrappedUpsideDown(String text, bool inverted, char fontSize) {
  String lines[100];
  int lineCount = collectWrappedLines(text, lines, 100, charsForSize(fontSize));
  for (int i = lineCount - 1; i >= 0; i--) printFormattedLine(lines[i], inverted);
}

// === Receipt Printing ===
void printReceipt() {
  Serial.println("Printing receipt...");

  String msg = currentReceipt.message;

  setFontSize(currentReceipt.fontSize);
  setJustify(currentReceipt.justify);
  setLineSpacing(currentReceipt.lineSpacing);

  printer.write(0x1B); printer.write('{');
  printer.write(currentReceipt.upsideDown ? 0x01 : 0x00);

  if (currentReceipt.upsideDown) {
    if (currentReceipt.qrCode) printQRCode(msg);
    else printWrappedUpsideDown(msg, currentReceipt.inverted, currentReceipt.fontSize);
    if (currentReceipt.showDate) {
      setInverse(true);
      printLine(currentReceipt.timestamp);
      setInverse(false);
    }
  } else {
    if (currentReceipt.showDate) {
      setInverse(true);
      printLine(currentReceipt.timestamp);
      setInverse(false);
    }
    if (currentReceipt.qrCode) printQRCode(msg);
    else printWrapped(msg, currentReceipt.inverted, currentReceipt.fontSize);
  }

  // restore defaults
  setFontSize('S');
  setJustify('L');
  setLineSpacing(0);
  advancePaper(2);
  Serial.println("Receipt printed.");
}

void printServerInfo() {
  Serial.println("=== Server Info ===");
  Serial.print("IP: "); Serial.println(WiFi.localIP());
  Serial.println("==================");

  String serverUrl = "http://" + WiFi.localIP().toString();

  printer.write(0x1B); printer.write('{'); printer.write(0x01);
  printWrappedUpsideDown(serverUrl, false, 'S');
  setInverse(true);
  printLine("PRINTER SERVER READY");
  setInverse(false);
  advancePaper(3);
}

// === QR Code (ESC/POS GS ( k) ===
void printQRCode(String data) {
  if (data.length() == 0) return;
  uint16_t storeLen = (uint16_t)(data.length() + 3);
  uint8_t pL = storeLen & 0xFF;
  uint8_t pH = (storeLen >> 8) & 0xFF;

  // Model 2
  printer.write(0x1D); printer.write(0x28); printer.write(0x6B);
  printer.write((uint8_t)0x04); printer.write((uint8_t)0x00);
  printer.write((uint8_t)0x31); printer.write((uint8_t)0x41);
  printer.write((uint8_t)0x32); printer.write((uint8_t)0x00);

  // Cell size 4
  printer.write(0x1D); printer.write(0x28); printer.write(0x6B);
  printer.write((uint8_t)0x03); printer.write((uint8_t)0x00);
  printer.write((uint8_t)0x31); printer.write((uint8_t)0x43); printer.write((uint8_t)0x04);

  // Error correction M
  printer.write(0x1D); printer.write(0x28); printer.write(0x6B);
  printer.write((uint8_t)0x03); printer.write((uint8_t)0x00);
  printer.write((uint8_t)0x31); printer.write((uint8_t)0x45); printer.write((uint8_t)0x31);

  // Store data
  printer.write(0x1D); printer.write(0x28); printer.write(0x6B);
  printer.write(pL); printer.write(pH);
  printer.write((uint8_t)0x31); printer.write((uint8_t)0x50); printer.write((uint8_t)0x30);
  for (int i = 0; i < (int)data.length(); i++) printer.write((uint8_t)data[i]);

  // Print
  printer.write(0x1D); printer.write(0x28); printer.write(0x6B);
  printer.write((uint8_t)0x03); printer.write((uint8_t)0x00);
  printer.write((uint8_t)0x31); printer.write((uint8_t)0x51); printer.write((uint8_t)0x30);

  advancePaper(1);
}
