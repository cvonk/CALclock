# CALclock

[![GitHub Discussions](https://img.shields.io/github/discussions/sandervonk/CALclock)](https://github.com/sandervonk/CALclock/discussions)
![GitHub release (latest by date including pre-releases)](https://img.shields.io/github/v/release/sandervonk/CALclock?include_prereleases&logo=DocuSign&logoColor=%23fff)
![GitHub](https://img.shields.io/github/license/sandervonk/CALclock)

Shows Google Calendar events on a LED circle incorporated in a clock faceplate.

It can be used as anything from a decorative/interactive art piece to a normal clock that can remind you of upcoming appointments in a fun and cleanly designed way. I used this to remind me of upcoming appointments once thatschool moved online.

![Backward facing glass clock with LED circle](media/forward_facing_250px.jpg) ![Forward facing glass clock with LED circle](media/backward_facing_250px.jpg)

## Features:

  - [x] Synchronizes with Google Calendar
  - [x] Shows calendar events in different colors using an LED circle on a clock.
  - [x] Push notifications for timely updates to calendar changes.
  - [x] Open source!

## Hardware

Two approaches can be used when deciding the look of you clock. One of which is to have the ring fully visible, with the other being to use the leds as an artsy-backlight.

![Internals of Backward facing glass clock with LED circle](media/forward_facing_int_250px.jpg) ![Internals of Forward facing glass clock with LED circle](media/backward_facing_int_250px.jpg)

### Parts

- [ ] RGB LED Pixel Ring containing 60 WS2812B SMD5050 addressable LEDs (e.g. "Chinly Addressable 60 Pixel LED Ring"). These WS2812B pixels are 5V, and draw about 60 mA each at full brightness. If you plan to use it in a bedroom, you probably want less bright LEDS such as WS2812 (without the "B").
- [ ] ESP32 board with 4 MByte flash memory, such as [ESP32-DevKitC-VB](https://www.espressif.com/en/products/devkits/esp32-devkitc/overview), LOLIN32 or MELIFE ESP32.
- [ ] 5 Volt, 3 Amp power adapter
- [ ] Capacitor (470 uF / 16V)
- [ ] Resistor (470 Ohm)
- [ ] Analog clock with glass face plate (e.g. Tempus TC6065S Wall Clock with Glass Metal Frame or a Selko 11" Brushed Metal Wall Clock)
- [ ] Optional frosting spray (e.g. Rust-Oleum Frosted Glass Spray Paint)
- [ ] Glass glue (e.g. Loctite Glass Glue)
- [ ] Molex 2 pin connector set

### Connect

> :warning: **THIS PROJECT IS OFFERED AS IS. IF YOU USE IT YOU ASSUME ALL RISKS. NO WARRENTIES.**

Connect the 5 Volt adapter to the ESP32 and LED strip.

Connect the data from the ESP32 module to the LED circle as shown below. 

| ESP32 module | LED circle     |
|:-------------|:---------------|
| `GPIO#18`    | WS2812 DATA-IN |

The `DATA-IN` of the LED circle should be driven with TTL signal levels, but we seem to get away with using the 3.3 Volt output from ESP32 in series with a 470 Ohms resistor. We didn't notice a diffeence when using a level shifter.

## Software

Clone the repository and its submodules to a local directory. The `--recursive` flag automatically initializes and updates the submodules in the repository.

```bash
git clone --recursive https://github.com/sandervonk/CALclock.git
```

If you haven't installed ESP-IDF, I suggest using Microsoft Visual Studio Code (vscode).  From vscode, add the [Microsoft's C/C++ extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools). Then add the [Espressif IDF extension](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension) and follow its configuration to install ESP-IDF 4.4.

### Google Apps Script

The software is a symbiosis between [Google Apps Script](https://developers.google.com/apps-script/guides/web) and firmware running on the ESP32. The script reads events from your Google Calendar and presents them as JSON to the ESP32 device.

To create the Web app:
  - Create a new project on [script.google.com](https://script.google.com);
  - Rename the project to e.g. `CALclock-doGet`
  - Copy and paste the code from `script\Code.gs`
  - Add the `Google Calendar API` service .
  - Select the function `test` and click `Debug`. This will ask for permissions. Don't expect output.
  - Click `Deploy` and chose `New deployment`, choose
    - Service tye = `Web app`
    - Execute as = `Me`
    - Who has access = `Anyone`, make sure you understand what the script does!
    - Copy the Web app URL to the clipboard

Open the URL in a web browser. You should get a reply like
```json
{
    "time": "2022-04-20 12:56:37",
    "pushId": "some_id",
    "events": [
        { 
            "start": "2022-04-20 10:55:00",
            "end": "2022-04-20 15:45:00"
        },
        { 
            "start": "2022-04-20 15:55:00",
            "end": "2022-04-20 17:15:00"
        }
    ]
}
```

Now, copy the `clock/main/Kconfig.example` to `clock/main/Kconfig` and paste the URL that ends in `/exec` to `clock/main/Kconfig` under `CALCLOCK_GAS_CALENDAR_URL`.

As we see in the next sections, the ESP32 does a `HTTP GET` on this URL. That way it retrieves a list of upcoming events from your calendar, and update the LEDs accordingly.

### ESP32 Device

In `menuconfig`, scroll down to CALclock and select "Use hardcoded Wi-Fi credentials" and specify the SSID and password of your Wi-Fi access point.

```bash
git clone https://github.com/cvonk/CALclock.git
cd CALclock/clock
idf.py set-target esp32
idf.py menuconfig
idf.py flash
```

*Your clock is now functional!*

## Push notification from Google

Normally, the devices polls for changes in the Google Calendar every 2 minutes. We can improve this response time by pushing notifications from Google to your device. You should only enable this, if your router has a SSL certificate and you're familiar with configuring a reverse proxy on your router.

The [Push Notifications API](https://developers.google.com/calendar/v3/push) documentation says:
> Allows you to improve the response time of your application. It allows you to eliminate the extra network and compute costs involved with polling resources to determine if they have changed. Whenever a watched resource changes, the Google Calendar API notifies your application. To use push notifications, you need to do three things:
> 1. Set up your receiving URL, or "Webhook" callback receiver.
> 2. Set up a notification channel for each resource endpoint you want to watch.

To meet the first requirement, the push notification need to be able to traverse your access router to reach your ESP32 device. This requires a SSL certificate and a reverse proxy. This implies you need to configure a reverse proxy (Nginx or Pound) on your router.

Setup this reverse proxy so that it forwards HTTPS request from Google, as HTTP to the device. On the device, the module `http_post_server.c` is the endpoint for these push notifications. 

The second requirement is already met by the Google apps script that we installed earlier.

## Feedback

We love to hear from you. Please use the Github discussions to provide feedback.
