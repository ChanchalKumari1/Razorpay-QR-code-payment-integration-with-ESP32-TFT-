# Razorpay-QR-code-payment-integration-with-ESP32-TFT-
Razorpay QR code payment integration with ESP32 TFT Display 
ESP32 QR / UPI Payment System with TFT 

# 1. System Overview
The ESP32 Razorpay UPI Payment System is an embedded application that enables merchants to accept UPI payments through QR codes displayed on an ILI9341 TFT touchscreen. The system features WiFi connectivity, web-based merchant registration, and real-time payment status monitoring with PNG QR code decoding.

# 2. Architecture
Entry Point: main.c initializes the system, creates task queues, and starts WiFi and web server. Core Components:

# Component	Description
TFT Display	320×240 ILI9341 SPI display with text/image rendering
WiFi	Station mode, configurable SSID/password
Web Server	HTTP server for merchant registration and token exchange
Payment API	REST API calls for registration, QR fetch, and payment status
PNG Decoder	pngle-based QR code image streaming and display

# 3. Hardware Configuration
Signal	GPIO	Interface	Notes
MOSI	11	SPI2	Data In
SCLK	12	SPI2	Clock
CS	10	GPIO	Chip Select
DC	9	GPIO	Data/Command
RST	4	GPIO	Reset
BL	21	GPIO	Backlight

# 4. Color Definitions
Color	Hex Code	RGB565
Black	0x0000	000000
White	0xFFFF	FFFFFF
Red	0xF800	FF0000
Green	0x07E0	00FF00
Blue	0x001F	0000FF
Yellow	0xFFE0	FFFF00

# 5. Key Data Structures
# 5.1 QR Task Data
typedef struct { char token[64]; char merchant_id[16]; char merchant_name[100]; char merchant_upi[100]; } qr_task_data_t;
Contains merchant information and authentication token for QR generation.

# 5.2 Payment Status
typedef struct { char status[32]; char amount[32]; char payer_name[100]; char payer_upi[100]; char paid_at[64]; char error_message[256]; } payment_status_t;
Stores payment response data from API. Status values: "success", "pending", "failed", "error".

# 6. TFT Display Module (tft_display.c/h)
# 6.1 Functions
Function	Description
tft_init()	Initialize ILI9341 display, SPI bus, GPIO. One-time setup.
tft_fill_screen()	Fill entire 320×240 display with color. DMA-accelerated.
tft_draw_image()	Draw bitmap image at (x,y) with width w, height h.
tft_fill_rect()	Fill rectangle with color. Thread-safe via semaphore.
tft_draw_pixel()	Draw single pixel with bounds checking.
tft_draw_text()	Draw ASCII text with 8×8 bitmap font, scalable, newline support, text wrapping.

# 6.2 Implementation Details
Display: ILI9341 320×240 pixels over SPI2. Semaphore-based thread safety (tft_lock). 8×8 bitmap font for ASCII (32-126). Dual-width buffering: stack buffer for widths ≤64 pixels, heap-allocated DMA buffer otherwise.

# 7. Payment Module (upi_payment.h)
# 7.1 Functions
Function	Description
call_payment_status_api()	HTTP GET to payment status endpoint. Returns JSON response.
parse_payment_response()	Parse JSON, extract status, amount, payer details into payment_status_t struct.
check_payment_status_task()	FreeRTOS task. Polls payment status every 3 sec (max 20 checks), updates TFT display.
on_draw()	PNG decoder callback. Filters black pixels, scales to 200×200, centers on display.
reset_png_decoder_state()	Reset line buffers and PNG state flags between decode cycles.

# 7.2 PNG Decoder
Decodes PNG QR codes using pngle library. on_draw() callback scales images to 200×200 pixels centered on 320×240 display. Filters black pixels (RGBA: alpha >200, R<100, G<100, B<100). reset_png_decoder_state() resets line buffers and flags between decodes.

# 8. Web Server (upi_server.h)
# 8.1 Endpoints
Method	Endpoint	Parameters	Response
GET	/	None	HTML form
POST	/register	name, upi_id	JSON (success, token)

# 8.2 Registration Flow
1. User enters merchant name and UPI ID via HTML form.
2. POST /register sends data to backend.
3. call_upi_api() requests token from remote server.
4. Token and merchant data saved to NVS.
5. QR task queued with new token.
6. QR code fetched and displayed on TFT.

# 9. WiFi Module (upi_wifi.h)
Connects to configurable SSID/password. Event handlers manage connection retries (max 2 attempts). Blocks in app_main() until WiFi connected or failed.

# 10. Global Variables
Variable	Purpose
qr_task_queue	FreeRTOS queue for QR API requests (capacity: 5)
api_response	HTTP response buffer for API calls
qr_image_buffer	PNG QR code image data (heap allocated, max 20 KB)
cached_token	Last valid merchant token (persistent)
panel_handle	TFT display panel handle for LCD operations
png_line_buffer[320]	Staging buffer for PNG decoder output lines

# 11. API Constants (upi_defines.h)
Base URL: https://digitalmonk.biz/ upi
Endpoints: /admin/add_merchant.php (registration), /api/get_qr.php (QR code), /api/payment_status.php (payment check)

# 12. Critical Sections
TFT Semaphore: Mutual exclusion for display writes. 
QR Task Queue: Handles asynchronous 
QR fetches. PNG Decoder 
State: Managed between decodes with reset_png_decoder_state(). 
NVS Flash: Persistent merchant data storage.

# 13. Task Priorities
Task	Priority	Stack (KB)	Purpose
qr_api_task	5	16	QR fetching
payment_monitor	5	24	Status polling

# 14. Memory Allocation
QR API task: 16 KB stack.
Payment monitoring: 24 KB stack. 
Web server: 10 KB stack. 
TFT line buffers: DMA-capable, allocated per-draw. 
PNG QR image: Up to 20 KB (MAX_QR_IMAGE_SIZE).

# 15. Flow Diagrams
15.1 Payment Flow
User Registration → call_upi_api() → Token saved → QR fetched and decoded → Displayed on TFT → Payment monitoring starts → Status polled every 3 seconds (max 20 attempts) → Display updated with success/failure

# 16. Dependencies
FreeRTOS (tasks, queues, semaphores). ESP-IDF (WiFi, NVS, HTTP client/server). cJSON (JSON parsing). pngle (PNG decoding). ESP LCD driver (ILI9341 panel).





