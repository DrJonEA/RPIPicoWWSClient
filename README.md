# RPIPicoWWSClient

A small C++ web service client for the Raspberry Pi Pico W and Pico 2 W.
It makes HTTP and HTTPS requests (GET and POST) from a FreeRTOS task, with
TLS 1.3 or 1.2 from [wolfSSL](https://www.wolfssl.com/) and optional
certificate checking against a root CA you supply.

I use it to push sensor data to REST APIs such as
[ThingsBoard](https://thingsboard.io) without needing MQTT.

## Features

- `get()`, `post()` (form parameters), `postJSON()` and `postPayload()`
- Query strings built from a `std::map`, with URL encoding
- HTTPS through wolfSSL, TLS 1.3 by default, with a switch to force TLS 1.2
- Server certificate checks: root CA, chain and domain name
- Response status, headers and payload, or a callback through
  `RequestObserver`
- `WifiHelper` for joining Wi-Fi, reading IP details and starting SNTP time
  sync

## The quickest way in: the example project

Start from
[DrJonEA/ThingsBoard-HTTP](https://github.com/DrJonEA/ThingsBoard-HTTP),
which already includes this library as a submodule, with all the other
libraries and port configuration it needs.

| Example | What it shows |
|---|---|
| `exp/simpleHTTP` | Post the Pico's temperature to ThingsBoard every five seconds |
| `exp/batchHTTP` | Read and store, then post a batch of readings in one request |
| `exp/lowpowerHTTP` | Batch posting with sleep between readings |
| `exp/secureHTTPS` | HTTPS with wolfSSL, verifying ThingsBoard's certificate |

New to the library? Build `simpleHTTP` first, then move on to
`secureHTTPS` when you want it secured properly. How to clone and build
my projects, submodules included, is in
[this post](https://drjonea.co.uk/2025/12/15/building-my-projects-from-repo/).

## What it depends on

| Library | Used for |
|---|---|
| [Pico SDK](https://github.com/raspberrypi/pico-sdk) 2.x | `pico_cyw43_arch_lwip_sys_freertos`, `pico_aon_timer`, `pico_lwip_sntp` |
| [FreeRTOS-Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel) | Tasks; requests block the calling task |
| lwIP (in the SDK) | Sockets, netconn and DNS |
| [coreHTTP](https://github.com/FreeRTOS/coreHTTP) | HTTP request and response handling |
| [wolfSSL](https://github.com/wolfSSL/wolfssl) | TLS for HTTPS |
| [uri-library](https://github.com/ben-zen/uri-library) | URL parsing |
| [json-maker](https://github.com/rafagafe/json-maker) | Linked by the library; handy for building payloads |

Each needs a CMake import and a port configuration. Copy them from one of
the example projects rather than writing them from scratch: the
`*.cmake` files in the project root and the `port/` folder.

## Adding it to your own project

Add the library as a submodule:

```bash
git submodule add https://github.com/DrJonEA/RPIPicoWWSClient \
    lib/RPIPicoWWSClient
```

Then, in your top-level `CMakeLists.txt`, after the other libraries are
imported:

```cmake
set(PICOW_WSCLIENT_PATH ${CMAKE_CURRENT_LIST_DIR}/lib/RPIPicoWWSClient)
include(${PICOW_WSCLIENT_PATH}/PicoW_WSClient.cmake)
```

And link it to your executable:

```cmake
target_link_libraries(${NAME}
    pico_stdlib
    FreeRTOS-Kernel-Heap4
    freertos_config
    LWIP_PORT
    pico_cyw43_arch_lwip_sys_freertos
    picow_wsclient
)
```

Things the port configuration must get right:

- **lwIP:** `LWIP_SOCKET`, `LWIP_NETCONN` and `LWIP_DNS` set to 1.
- **wolfSSL:** a `user_settings.h` with `WOLFSSL_TLS13` and `HAVE_SNI`,
  and `XTIME` pointing at a function that returns real seconds since 1970
  (see the time note under HTTPS).
- **Stack:** give any task that makes HTTPS requests at least 16 KB of
  stack (`1024 * 4` words in `xTaskCreate`).

## Using it

### Join Wi-Fi and start the clock

```cpp
#include "WifiHelper.h"
#include "Request.h"

void main_task(void *params){
    cyw43_arch_init();
    cyw43_arch_enable_sta_mode();

    while (!WifiHelper::join(WIFI_SSID, WIFI_PASSWORD)){
        vTaskDelay(2000);
    }

    WifiHelper::sntpAddServer("0.uk.pool.ntp.org");
    WifiHelper::sntpStartSync();
    ...
}
```

### Make a request

Create the `Request` inside a task. Give it a working buffer, or let it
allocate its own with `Request req;`.

```cpp
#define REQ_BUF_LEN 1024
char reqBuf[REQ_BUF_LEN];

void sensor_task(void *params){
    Request req(reqBuf, REQ_BUF_LEN);
    const char *url = "http://example.com/api/v1/telemetry";
    const char *json = "{\"temperature\": 21.5}";

    if (req.postPayload(url, json)){
        printf("Status: %d\n", req.getStatusCode());
        printf("%.*s\n", req.getPayloadLen(), req.getPayload());
    }
}
```

The other calls work the same way:

```cpp
std::map<std::string, std::string> query;
query["id"] = "42";
req.get("http://example.com/api/item", &query);

std::map<std::string, std::string> form;
form["name"] = "pico";
req.post("http://example.com/api/form", &form);
```

`postJSON()` takes any object that implements `JSONSerialisable`
(a single `char * json()` method).

The payload is not null terminated, so always use `getPayloadLen()` with
`getPayload()`. Call `freeMemory()` when you've finished with a response
if you're short of RAM.

### HTTPS

Change the URL to `https://` and that's it: the request is encrypted.
Without a root CA, though, the Pico doesn't check who it's talking to,
and the library prints this on every connection:

```
WARNING: TLS without server verification
```

To check the server's certificate, give the request a root CA in PEM
format. The string must stay in memory while the request is in use:

```cpp
#include "tbRootCA.h"   // const char TB_ROOT_CA[] = "-----BEGIN ...";

Request req(reqBuf, REQ_BUF_LEN);
req.setRootCA(TB_ROOT_CA);

req.postPayload("https://eu.thingsboard.cloud/api/v1/TOKEN/telemetry",
    json);
printf("TLS: %s %s\n", req.getTLSVersion(), req.getTLSCipher());
```

With a root CA set, the library checks the chain up to that root and that
the certificate is for the host in the URL. A server that fails either
check is refused.

`exp/secureHTTPS/tools/getRootCA.py` in the ThingsBoard-HTTP repo looks up
a server's chain and writes the root CA out as a header for you.

Other switches:

- `setForceTLS12(true)` uses TLS 1.2 even when TLS 1.3 is compiled in.
  Some servers need it.
- `setInsecure(true)` skips the certificate checks even when a root CA is
  set. Handy for a quick test, never for a real device.

Three things that catch people out with HTTPS:

1. **Time.** Certificates have "not before" and "not after" dates, so the
   Pico needs the real time. Start SNTP and wait for it (for example
   `aon_timer_is_running()`) before the first HTTPS request, and make your
   wolfSSL `XTIME` function return seconds since 1970, not time since boot.
2. **Key size.** ThingsBoard's root, ISRG Root X1, is a 4096-bit RSA key.
   With wolfSSL's fast maths, `FP_MAX_BITS` must be at least twice the
   biggest key in the chain: 8192. Too small and the handshake fails with
   error -132.
3. **Certificates change.** Roots expire and services move to new ones.
   Plan how a device in the field gets a new root CA.

### Getting the result through an observer

Instead of reading the response after the call, subclass
`RequestObserver` and override `requestComplete(Request *req)`, then pass
it to `req.setObserver(&myObserver)`.

## Buffer sizes

| Define | Default | What it limits |
|---|---|---|
| `REQUEST_BUFFER_SIZE` | 256 | Working buffer for building requests |
| `REQUEST_MAX_PATH` | 512 | Longest URL, including the query string |
| `WIFI_RETRIES` | 3 | Attempts `WifiHelper::join()` makes |

Define them before including the headers, or with
`target_compile_definitions` in CMake.

## Videos

I build and explain projects using this library on my YouTube channel,
[Dr Jon EA](https://www.youtube.com/@drjonea):

- [ThingsBoard HTTP vs MQTT: Which is Better for Low Power?](https://www.youtube.com/watch?v=TyHzCU0hrPY)

[Subscribe](https://www.youtube.com/@drjonea?sub_confirmation=1) to catch
the next one. HTTPS with wolfSSL, using this library's certificate
checks, is coming soon.

## Licence

GPL-3.0. See [LICENSE](LICENSE).

---

## More from Dr Jon EA

- 📺 **Video:** [ThingsBoard HTTP vs MQTT: Which is Better for Low Power?](https://www.youtube.com/watch?v=TyHzCU0hrPY)
- 💻 **Example project:** [DrJonEA/ThingsBoard-HTTP](https://github.com/DrJonEA/ThingsBoard-HTTP)
- 🛠️ **Building my projects:** [How to clone and build from my repos](https://drjonea.co.uk/2025/12/15/building-my-projects-from-repo/)
- 🎓 **Courses and training:** [drjonea.co.uk/training](https://drjonea.co.uk/training/)

Made by [Dr Jon Durrant](https://jondurrant.com) at [Dr Jon EA Ltd](https://drjonea.co.uk) · [YouTube](https://www.youtube.com/@drjonea)
