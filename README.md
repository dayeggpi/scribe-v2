# Project Scribe
Simple, reliable, distraction free writer to nudge you towards a better life story - by surfacing your receipts, one day at a time.

> [!TIP]  
> This is the base configuration of Scribe, to serve one straight forward, important use case: it turns the Scribe platform into a simple and reliable short message writer (simple by design). It works very well as is.
> 
> However, if you wish, you can go beyond this and make this human-machine/ human-computer interface truly your own.
>
> The hardware is capable, the design is easily adaptable, and the firmware is easy to develop/ scale in whichever direction your imagination leads you towards!
> I'm excited to see what you come up with!

Base Scribe: a simple, reliable, open-source system that helps you capture the meaningful moments of your life. It leverages thermal printing to write a tangible log of your life's story, daily achievements, thoughts, or memories on a continuous roll of paper. It's designed to be a quiet companion that nudges you to live more intentionally.

This project is born from the idea of "bringing receipts" for the life you lead/ towards the life you want to lead. It's about creating a physical artifact of your journey. It is highly hackable, adaptable and scalable to fit your needs and wants.

## Features of the "out-of-the-box" configuration (v1)
- **Thermal Printing:** Clean, permanent-enough tactile records - no mess, no dried ink
- **Standard Rolls:** Very cheap and available paper - e.g. couple of bucks for 6 rolls 
- **Minimalist Web Interface:** Sleek, distraction-free input with just a text box and submit button (and some confetti for that extra delight)
- **API Integration:** Integrate Project Scribe with your favourite tools like Apple Shortcuts, IFTTT, or your own custom scripts.
- **Offline Operation:** No cloud services required (but cloud capable) - everything works locally
- **Organic Design:** Loosely Inspired by Dalí and Gaudí, with flowing curves that hint at life's organic nature
- **Low Power:** Runs on a single USB port, no separate power supply needed (about 0.5W at idle)
- **Open Source:** Completely free, hackable, and customisable


![pic for profiles](https://github.com/user-attachments/assets/56afd51b-3560-419a-93f4-af315ba2968f)

## Features added as v2

New 3D printed case (see https://github.com/dayeggpi/scribe-v2/blob/main/Scribe.stl to download it, don't forget support).

Note this case is Thermal Printer QR701 dimensions (103.5 x 57 x 65 mm).

<img width="600" alt="image" src="https://raw.githubusercontent.com/dayeggpi/scribe-v2/refs/heads/main/img/front.jpg">
<img width="600" alt="image" src="https://raw.githubusercontent.com/dayeggpi/scribe-v2/refs/heads/main/img/back.jpg">
<img width="600" alt="image" src="https://raw.githubusercontent.com/dayeggpi/scribe-v2/refs/heads/main/img/inside.jpg">



And the webapp:

<img width="600" alt="image" src="https://raw.githubusercontent.com/dayeggpi/scribe-v2/refs/heads/main/img/webapp_text.png">
<img width="600" alt="image" src="https://raw.githubusercontent.com/dayeggpi/scribe-v2/refs/heads/main/img/webapp_img.png">



🚀 New Features & Capabilities
- Markdown-style Text Formatting: Added support for inline string formatting (e.g., **bold** for bold text, _underline_ for underlined text, and ==highlighted for inverted/white-on-black text).
- Image Printing: Added ability to upload and print images. Images are automatically scaled proportionally to fit the printer max width (384px). Includes an "invert colors" option.
- QR Code Printing: Added a toggle to convert text input directly into a printed QR code (prints the code only, omitting the raw text).
- Advanced Layout Controls: Added settings to change font size, text alignment (Left, Center, Right), and adjust line spacing.
- Print Orientation & Direction: Added an "upside down" (flip) toggle to reverse the print direction based on how the paper exits the machine.
- Global Overrides: Added toggles to invert the entire text output (white text on a black background) and dynamically append the current Date to the printed message.

🐛 Bug Fixes & Stability

- International Character Support: Fixed encoding issues that caused Chinese characters to print instead of French accents (like é) and handled emojis to prevent formatting errors.
- WiFi Auto-Reconnect: Fixed the "IP unset" boot error. The app now automatically attempts to reconnect up to 5 times.
- Hardware Error Log: If the ESP8266 fails to connect after 5 tries, it will print the first 200 characters of the error log directly to the thermal printer for easier debugging.
- Memory Management: Fixed Out-of-Memory (OOM) errors and silent failures that occurred when uploading and processing 384px images.
- Form Submission Fix: Prevented the Enter key from accidentally triggering a premature form submission.

✨ Web Interface (UI) Improvements
- AJAX Submissions: Form submissions now happen in the background without reloading the webpage. Both text and image forms auto-reset upon successful print.
- WYSIWYG Text Box: The text area now accurately represents the physical print roll by auto-wrapping at 32 characters. Formatting tags (like ==) are mathematically excluded from the visible character limits to maintain wrapping accuracy.
- Character Tracking: Removed the strict 200-character limit and replaced it with a live character counter for better visibility.
- Image Tab Enhancements: Added a explicit "Clear" button to easily purge selected images from the queue.
- Aesthetics: Added a simple, lightweight favicon to the web page.
- State fixes: Default orientation toggles were fixed to match standard expected behavior (unchecked logic corrected).

💻 Developer & API Updates
- POST Request Support: The API now fully supports POST payloads alongside standard GET requests, allowing for larger and safer text formatting.
- New API Endpoints & Parameters: Added full programmatic support for layout modifications (showDate, inverted, upsideDown, qrcode, spacing, alignment, size).
- Documentation: Generated a comprehensive api.md file featuring complete documentation and simple curl examples utilizing both GET and POST methods for text, QR codes, and images.

## API

### Life Receipt — API Reference

Base URL: `http://<printer-ip>/`  (IP printed on boot, also in Serial monitor)

### POST /submit — Print a text receipt

Accepts `application/x-www-form-urlencoded` or `multipart/form-data`.  
Also accepts GET with query string (quick browser testing).

### Parameters

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `message` | string | **yes** | — | Text to print. `\n` creates new lines. Inline markup supported (see below). |
| `fontSize` | `S` \| `M` \| `L` | no | `S` | Font size. S = 1× (32 chars/line), M = 2× (16 chars/line), L = 3× (~10 chars/line). |
| `justify` | `L` \| `C` \| `R` | no | `L` | Alignment: Left, Center, Right. |
| `lineSpacing` | integer 24–60 | no | `30` | Line spacing in dots. Default ~30. Higher = more space between lines. |
| `showDate` | `1` | no | off | Print auto-formatted date as an inverted header. |
| `date` | string | no | — | Custom date: `YYYY-MM-DD` or `DD/MM/YYYY`. Only used when `showDate=1`. |
| `inverted` | `1` | no | off | Entire message white-on-black. |
| `upsideDown` | `1` | no | off | Rotate each character 180°. Receipt reads correctly when flipped after tearing. |
| `qrCode` | `1` | no | off | Print `message` as a QR code instead of text. |

### Inline markup (inside `message`)

All modes reset at end of each line.

| Syntax | Effect |
|--------|--------|
| `**text**` | Bold |
| `_text_` | Underline |
| `==text==` | Inline inverse (white on black) |

Markers are nestable: `**bold _bold+underline_ back**` works.

### Response

- `200 OK` — body: `OK`
- `400 Bad Request` — body: `Missing message`

### Examples

```bash
# Simple
curl -X POST http://192.168.1.x/submit -d "message=Hello"

# Accented characters (é â à ç etc. all work)
curl -X POST http://192.168.1.x/submit \
  --data-urlencode "message=Un gâteau au café, s'il vous plaît"

# Large centered bold heading
curl -X POST http://192.168.1.x/submit \
  -d "fontSize=L&justify=C" \
  --data-urlencode "message=**RECEIPT**"

# Mixed styles, date, tight spacing
curl -X POST http://192.168.1.x/submit \
  -d "showDate=1&fontSize=M&justify=C&lineSpacing=26" \
  --data-urlencode "message=**Total:** ==42.00 EUR==
_Merci pour votre commande_"

# Right-aligned, loose spacing
curl -X POST http://192.168.1.x/submit \
  -d "justify=R&lineSpacing=50" \
  --data-urlencode "message=Line one
Line two
Line three"

# QR code
curl -X POST http://192.168.1.x/submit \
  --data-urlencode "message=https://example.com" -d "qrCode=1"

# GET (paste in browser)
http://192.168.1.x/submit?message=Hello&fontSize=M&justify=C
```

### POST /print-image — Print a bitmap image

The web UI handles image conversion. For direct API use you must pre-process.

### Payload

`multipart/form-data` with one file field named `image`:

| Field | Type | Description |
|-------|------|-------------|
| `image` | binary file | Raw 1-bit-per-pixel bitmap, row-major, MSB-first. Filename **must** encode dimensions: `WxH.bin` (e.g. `384x512.bin`). |

- Bytes per row = `ceil(W / 8)`. Total = `ceil(W/8) × H`.
- Max width: 384 px (full printable area on 58 mm paper at 8 dots/mm).






## BOM

- D1 mini board (any Arduino board will do, but you will have to adjust the sketch)
- CSN-A4L thermal printer (any serial thermal printer might do, although I haven't tested for that - you may need to adjust power and pinout)
- Paper rolls (printer comes preloaded with one, and it'll last ages - here, you're looking for 57.5±0.5mm width and 30mm max diameter)

- 3D printer for the body (you may need some glue to fix the parts together - no screws required)
- Wires (for soldering and connecting components + USB wire to power the whole thing)
- 5V/ USB power supply capable of higher currents (only used during thermal printing)

Affiliate links to grab the components (if you want to use them):

| Component                         | Amazon US               | Amazon UK               | AliExpress                                |
| --------------------------------- | ----------------------- | ----------------------- | ----------------------------------------- |
| Microcontroller (USB-C D1 Mini)   | https://amzn.to/4h2zQYO | https://amzn.to/4gRFgFe | -                                         |
| Thermal Printer (CSN-A4L)         | https://amzn.to/4kr5ksq | -                       | https://s.click.aliexpress.com/e/_opjoNrw |
| Paper Rolls, BPA-free (57.5x30mm) | https://amzn.to/4kpOREP | https://amzn.to/44nqGCg | -                                         |

**Important Note (Thanks to the community for pointed this out!):** Do your own due diligence regarding thermal paper types - the thermal paper we handle everyday (e.g. through receipts from the grocery store, restaurants, takeaway, taxis, etc.) will contain BPA. When choosing your rolls for this, you should definitely go for BPA-free paper just to be on the safer side - the links provided are for BPA-free paper. If you can, go a step further and look for “phenol-free” paper. Three types that do not contain BPA or BPS and are competitively priced contain either ascorbic acid (vitamin C), urea-based Pergafast 201, or a technology without developers, Blue4est.

Note2: The components might be slightly different as listings always change silently - always check. If you notice any issues, please ping me to update the readme.

Note3: If you don't have a 3D printer but would like to build this, consider using the PCBWay affiliate link: https://pcbway.com/g/86cN6v (discount to you + some small help for the project).

Note4: Some thermal paper is treated against fading - can last e.g. 35+ years. If you're planning on using Scribe for archival purposes, consider ink fading when picking up the right rolls.

## Pin-out/ wiring during operation

The project uses SoftwareSerial to communicate with the printer, leaving the hardware serial pins free.

| Printer Pin | D1 mini Pin | Power Supply Pin | Description      |
| ----------- | ----------- | ---------------- | ---------------- |
| TTL RX      | D3          | -                | D1 Mini Transmit |
| TTL TX      | D4          | -                | D1 Mini Receive  |
| TTL GND     | GND         | GND              | Common Ground    |
| Power VH    | -           | 5V               | Printer VIN      |
| Power GND   | GND         | GND              | Printer GND      |

**Note:** Never power the printer directly from/ through the D1 Mini! You'll burn your microcontroller.

**Note2:** Only power the D1 Mini via one source (either via USB during firmware flashing, or via the 5V pin during normal operation from the shared power supply)

**Note3:** You can remove the other wires (e.g. TTL NC/ DTR) - you'll deal with less clutter since these will not be used.

## Microcontroller firmware

One sketch file, using the IDE of your choice (e.g. the main Arduino IDE works well with the added modules for D1 mini + libraries - that's what I use). Make sure you update the firmware variables before flashing it to the MCU (e.g. wifi details and other preferences you might choose to tweak).

Please check everything is working before soldering and squeezing everything into the 3D printed shell.

**Note:** As mentioned above - do not power the printer through the D1 Mini and do not power the D1 mini via both the USB and its pins at the same time.

## Assembly

In each set, there are 2x 3D Printed components:
- The head unit (in which your MCU + Thermal Printer + wiring slot it)
- The neck/ leg (connects with the head and has a channel to elegantly route/ feed your power cable through)

**Printing considerations**
- The head has fillets, so you will most likely require supports
- Smaller line heights will produce better results
- The neck/ leg can be printed without any supports upright
- The components may vary in size slightly, so will the tolerances/ clearances - you may need to us glue to put the pieces together in case they're lose, or sandpaper in case they're too tight

**Assembly considerations**
- Make sure you route the wire through the neck/ leg of Scribe before you crimp the connectors
- Important: make sure each connection and wire is well isolated before you cram all the wiring into the head unit! You really don't want a short circuit
- Always to a test run before final assembly
- Do not glue the electrical components together, in case you need to service this later (you shouldn't need to glue them together)
## User guide for standard configuration (v1)

1. **Power On:** Connect the device to a beefy 5V USB power source. Wait a few moments for it to boot, connect to WiFi, and print its startup receipt.
2. **Get the IP Address:** The local IP address will be printed on the startup receipt.
3. **Start Scribing!**
4. **Look back at your story**
5. **Improve and scribe some more**

**Message format**
The as-is firmware prints messages in the right orientation for the roll of paper to naturally wind downwards, with word wrap. The first line is the header, reminiscent of a calendar - date on black background. The following lines are the message itself.

**Scribing through a web browser**
- The D1 Mini creates a local web server and the as-is configuration includes a minimalist, light web app
- Open a web browser on any device on the same network and navigate to `http://<IP_ADDRESS>`. Type your entry (up to 200 characters) and press Enter or click the "Send" button.
- This limitation is not the limitation of the printer/ hardware/ system, I just like it to keep the messages concise - you can change it in firmware (just like everything else around here)

**Scribing through the API**
- You can also send entries directly from a browser or script. For example: `http://<IP_ADDRESS>/submit?message=Went%20for%20a%20hike`
- This is particularly useful when running automations - it works straight out of the box
- Different to the web app, when using the API there is no character limit out of the box. In addition, you can also backdate your entries, by adding the `date` parameter: `http://<IP_ADDRESS>/submit?message=Finished%20the%20book&date=2025-07-04`

## Beyond the as-is: Ideas to Extend or Replace the As-Is Functionality

The combination of a WiFi-enabled MCU, a Thermal Printer and an API-ready web server makes Scribe a powerful platform for all sorts of creative projects.

You can easily adapt the existing code to create a completely new experience. Here are a few ideas to get you started:

- **Daily Briefing Printer:** Modify the code to fetch data from public APIs every morning. It could print:
    - Your first few calendar events for the day.
    - The local weather forecast.
    - A curated news headline from an RSS feed.
    - The word or quote of the day.
- **Task & Issue Tracker:** Connect it to your productivity tools (like Todoist, Jira, or GitHub) via their APIs or webhooks.
    - Print new tasks or tickets as they are assigned to you.
    - Print your most important tasks for the day each morning.
- **Kitchen Companion:** Place it in your kitchen to print:
    - Shopping lists sent from a family messaging app or a shared note.
    - A recipe of the day.
    - Measurement conversion charts on demand.
- **Tiny Message Receiver:** Create a unique, private messaging system. Family members could send short messages to the printer from anywhere, creating a physical message board.
- **Daily Dose of Fun:** Make it a source of daily delight by having it print:
    - A random joke or comic strip (like XKCD).
    - A "shower thought" from a Reddit subreddit.
    - A custom fortune cookie message.
- **Photo Booth Printer:** Extend the functionality to accept an image URL via the API, dither the image in the firmware, and print low-resolution, stylised versions of your photos.

## Troubleshooting

In my testing and usage, I found this setup to be extremely reliable (after all, these printers are used in commercial settings). If the device is not printing as expected, this may be because of several reasons, e.g.:
- incorrect wiring/ a short
- paper not inserted correctly
- current on offer is not high enough

Thermal Printer Manual, in case you need to look into things further: https://www.manualslib.com/manual/3035820/Cashino-Csn-A4l.html

## Disclaimer

I've done my best to document everything accurately - however, there might be mistakes. If you see them, or opportunities to improve, please open an issue.  
This is an open-source project given for free, with no warranties or guarantees. It assumes a level of proficiency with electronics, assemblies, engineering, etc. Do your own due diligence - it's your responsibility. Stay safe. Stay productive. Work with what you have. Make the world a better place.
