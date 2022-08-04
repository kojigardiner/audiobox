/*** Includes ***/
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <FastLED.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>

#include "AudioProcessor.h"
#include "ButtonFSM.h"
#include "CLI.h"
#include "Constants.h"
#include "EventHandler.h"
#include "LEDPanel.h"
#include "MeanCut.h"
#include "Mode.h"
#include "ModeSequence.h"
#include "Spotify.h"
#include "Utils.h"

/*** Function Prototypes ***/

// Display Functions
bool copy_jpg_data(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);  // callback function for JPG decoder
bool display_jpg_data(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);

void display_full_art(uint8_t offset_row, uint8_t offset_col);
void decode_art(uint8_t *art_data, unsigned long art_num_bytes);

void display_image(const char *filepath);
bool download_image(const char *url, const char *filepath);

void run_audio(AudioProcessor *ap, int audio_mode);
void test_modes();

/*** Globals ***/

// EventHandler
EventHandler eh;

// Semaphores
SemaphoreHandle_t mutex_leds;

// Task & Queue Handles
TaskHandle_t task_eventhandler;
QueueHandle_t q_events;

TaskHandle_t task_buttons;
QueueHandle_t q_buttons;

TaskHandle_t task_spotify;
QueueHandle_t q_spotify;

TaskHandle_t task_audio;
QueueHandle_t q_audio;

TaskHandle_t task_display;
QueueHandle_t q_display;

TaskHandle_t task_servo;
QueueHandle_t q_servo;

TaskHandle_t task_mode;
QueueHandle_t q_mode;

// Task functions
void task_eventhandler_code(void *parameter);
void task_buttons_code(void *parameter);
void task_spotify_code(void *parameter);
void task_audio_code(void *parameter);
void task_display_code(void *parameter);
void task_servo_code(void *parameter);
void task_mode_code(void *parameter);

typedef struct AlbumArt {
    uint16_t full_art_rgb565[64][64] = {{0}};     // full resolution RGB565 artwork
    uint16_t palette_art_rgb565[16][16] = {{0}};  // artwork to use for palette creation
    CRGB palette_crgb[PALETTE_ENTRIES] = {0};     // color palette from album art
} AlbumArt_t;
AlbumArt_t album_art;

LEDPanel lp = LEDPanel(GRID_W, GRID_H, NUM_LEDS, PIN_LED_CONTROL, MAX_BRIGHT, true, LEDPanel::BOTTOM_LEFT);

void setup() {
    pinMode(PIN_LED_STATUS, OUTPUT);
    pinMode(PIN_BUTTON_LED, OUTPUT);
    pinMode(PIN_POWER_SWITCH, INPUT_PULLUP);

    // Turn on status LED to indicate program has loaded
    // digitalWrite(PIN_LED_STATUS, HIGH);

    Serial.begin(115200);  // debug serial terminal

    esp_sleep_enable_ext0_wakeup(PIN_POWER_SWITCH, LOW);  // setup wake from sleep
    if (digitalRead(PIN_POWER_SWITCH) == HIGH) {
        print("Switch is high, going to sleep\n");
        esp_deep_sleep_start();  // go to sleep if switch is high
    }

    print("Initial safety delay\n");
    delay(500);  // power-up safety delay to avoid brown out

    // Drop into debug CLI if button is depressed
    pinMode(PIN_BUTTON_MODE, INPUT_PULLUP);
    if (digitalRead(PIN_BUTTON_MODE) == LOW) {
        start_cli();
    }

    print("Loading preferences\n");

    if (!check_prefs()) {
        print("Missing preferences! Power off and power on while holding down the main button to configure preferences.\n");
    }

    // Wifi setup
    if (!connect_wifi()) {
        print("Wifi could not connect! Power off and power on while holding down the main button to configure preferences.\n");
    }

    // test_modes();

    // Filessystem setup
    print("Setting up filesystem\n");
    if (!SPIFFS.begin(true)) {
        print("An Error has occurred while mounting SPIFFS\n");
        return;
    }

    // LED Setup
    print("Setting up LEDs\n");
    lp.init();

    print("Setup complete\n\n");

    // Turn on button LED to indicate setup is complete
    digitalWrite(PIN_BUTTON_LED, HIGH);

    // Task setup
    mutex_leds = xSemaphoreCreateMutex();

    // q_spotify = xQueueCreate(1, sizeof(Spotify::public_data_t));
    // q_button_to_display = xQueueCreate(10, sizeof(ButtonFSM::button_fsm_state_t));
    // q_button_to_audio = xQueueCreate(10, sizeof(ButtonFSM::button_fsm_state_t));
    // q_servo = xQueueCreate(10, sizeof(int));
    // q_mode = xQueueCreate(1, sizeof(Modes_t));
    // q_audio_done = xQueueCreate(1, sizeof(uint8_t));
    // q_event = xQueueCreate(10, sizeof(event_t));

    disableCore0WDT();  // hack to avoid various kernel panics

    // setup event handler
    q_events = xQueueCreate(MAX_EVENTHANDLER_EVENTS, sizeof(event_t));
    eh = EventHandler(q_events);

    q_spotify = xQueueCreate(10, sizeof(event_t));
    eh.register_task(&task_spotify, q_spotify, EVENT_START | EVENT_MODE_CHANGED);

    q_display = xQueueCreate(10, sizeof(event_t));
    eh.register_task(&task_display, q_display, EVENT_START | EVENT_MODE_CHANGED | EVENT_SPOTIFY_UPDATED | EVENT_AUDIO_FRAME_DONE);

    q_buttons = xQueueCreate(10, sizeof(event_t));
    eh.register_task(&task_buttons, q_buttons, EVENT_START);

    q_audio = xQueueCreate(10, sizeof(event_t));
    eh.register_task(&task_audio, q_audio, EVENT_START | EVENT_MODE_CHANGED);

    q_servo = xQueueCreate(10, sizeof(event_t));
    eh.register_task(&task_servo, q_servo, EVENT_START | EVENT_SERVO_POS_CHANGED | EVENT_MODE_CHANGED);

    q_mode = xQueueCreate(10, sizeof(event_t));
    eh.register_task(&task_mode, q_mode, EVENT_START | EVENT_BUTTON_PRESSED | EVENT_SPOTIFY_UPDATED);

    xTaskCreatePinnedToCore(
        task_spotify_code,  // Function to implement the task
        "task_spotify",     // Name of the task
        10000,              // Stack size in words
        q_spotify,          // Task input parameter
        1,                  // Priority of the task (don't use 0!)
        &task_spotify,      // Task handle
        0                   // Pinned core - 0 is the same core as WiFi
    );

    xTaskCreatePinnedToCore(
        task_display_code,  // Function to implement the task
        "task_display",     // Name of the task
        10000,              // Stack size in words
        q_display,          // Task input parameter
        1,                  // Priority of the task (don't use 0!)
        &task_display,      // Task handle
        1                   // Pinned core, 1 is preferred to avoid glitches (see: https://www.reddit.com/r/FastLED/comments/rfl6rz/esp32_wifi_on_core_1_fastled_on_core_0/)
    );

    xTaskCreatePinnedToCore(
        task_buttons_code,  // Function to implement the task
        "task_buttons",     // Name of the task
        5000,               // Stack size in words
        q_buttons,          // Task input parameter
        1,                  // Priority of the task (don't use 0!)
        &task_buttons,      // Task handle
        1                   // Pinned core
    );

    xTaskCreatePinnedToCore(
        task_mode_code,  // Function to implement the task
        "task_mode",     // Name of the task
        5000,            // Stack size in words
        q_mode,          // Task input parameter
        1,               // Priority of the task (don't use 0!)
        &task_mode,      // Task handle
        1                // Pinned core
    );

    xTaskCreatePinnedToCore(
        task_audio_code,  // Function to implement the task
        "task_audio",     // Name of the task
        30000,            // Stack size in words
        q_audio,          // Task input parameter
        1,                // Priority of the task (don't use 0!)
        &task_audio,      // Task handle
        1                 // Pinned core
    );

    xTaskCreatePinnedToCore(
        task_servo_code,  // Function to implement the task
        "task_servo",     // Name of the task
        2000,             // Stack size in words
        q_servo,          // Task input parameter
        1,                // Priority of the task (don't use 0!)
        &task_servo,      // Task handle
        1                 // Pinned core
    );

    // Setup eventhandler task last
    xTaskCreatePinnedToCore(
        task_eventhandler_code,  // Function to implement the task
        "task_eventhandler",     // Name of the task
        2000,                    // Stack size in words
        NULL,                    // Task input parameter
        1,                       // Priority of the task (don't use 0!)
        &task_eventhandler,      // Task handle
        1                        // Pinned core
    );
}

void task_eventhandler_code(void *parameter) {
    print("task_eventhandler_code running on core ");
    print("%d\n", xPortGetCoreID());

    event_t e = {.event_type = EVENT_START};
    eh.emit(e);  // seed all other tasks to start
    for (;;) {
        xQueueReceive(q_events, &e, portMAX_DELAY);
        // print("Received event, %d\n", e.event_type);
        eh.process(e);
    }
}

void loop() {
    vTaskDelete(NULL);  // delete setup and loop tasks
}

void test_modes() {
    Mode MAIN_MODES_LIST[] = {
        Mode(MODE_MAIN_ART, SERVO_POS_ART, DURATION_MS_ART),
        Mode(MODE_MAIN_AUDIO, SERVO_POS_NOISE, DURATION_MS_AUDIO)};

    Mode ART_SUB_MODES_LIST[] = {
        Mode(MODE_ART_WITHOUT_ELAPSED, SERVO_POS_ART),
        Mode(MODE_ART_WITH_ELAPSED, SERVO_POS_ART),
        Mode(MODE_ART_WITH_PALETTE, SERVO_POS_ART)};

    Mode AUDIO_SUB_MODES_LIST[] = {
        Mode(MODE_AUDIO_NOISE, SERVO_POS_NOISE),
        Mode(MODE_AUDIO_SNAKE_GRID, SERVO_POS_GRID),
        Mode(MODE_AUDIO_BARS, SERVO_POS_BARS),
        Mode(MODE_AUDIO_CENTER_BARS, SERVO_POS_BARS),
        Mode(MODE_AUDIO_WATERFALL, SERVO_POS_NOISE)};

    ModeSequence art_sub_modes = ModeSequence(ART_SUB_MODES_LIST, ARRAY_SIZE(ART_SUB_MODES_LIST));
    ModeSequence audio_sub_modes = ModeSequence(AUDIO_SUB_MODES_LIST, ARRAY_SIZE(AUDIO_SUB_MODES_LIST));
    ModeSequence sub_modes[] = {art_sub_modes, audio_sub_modes};

    ModeSequence main_modes = ModeSequence(MAIN_MODES_LIST, ARRAY_SIZE(MAIN_MODES_LIST), sub_modes);
    main_modes.description();

    print("\n\n");
    main_modes.next();
    main_modes.description();
    main_modes.mode().description();
    main_modes.submode().description();
    main_modes.get_curr_mode();

    print("\n\n");
    main_modes.submode_next();
    main_modes.description();
    main_modes.mode().description();
    main_modes.submode().description();

    print("\n\n");
    main_modes.submode_next();
    main_modes.description();
    main_modes.mode().description();
    main_modes.submode().description();

    print("\n\n");
    main_modes.submode_next();
    main_modes.description();
    main_modes.mode().description();
    main_modes.submode().description();

    print("\n\n");
    main_modes.submode_next();
    main_modes.description();
    main_modes.mode().description();
    main_modes.submode().description();

    print("\n\n");
    main_modes.submode_next();
    main_modes.description();
    main_modes.mode().description();
    main_modes.submode().description();
}

void show_leds() {
    xSemaphoreTake(mutex_leds, portMAX_DELAY);
    FastLED.show();
    xSemaphoreGive(mutex_leds);
}

void task_display_code(void *parameter) {
    print("task_display_code running on core ");
    print("%d\n", xPortGetCoreID());

    TickType_t xLastWakeTime;
    const TickType_t xFrequency = (1000.0 / FPS) / portTICK_RATE_MS;

    ButtonFSM::button_fsm_state_t button_state;
    Spotify::public_data_t sp_data;
    BaseType_t q_return;
    double percent_complete = 0;
    int counter = 0;

    curr_mode_t curr_mode;

    // Initialise the xLastWakeTime variable with the current time.
    xLastWakeTime = xTaskGetTickCount();

    QueueHandle_t q = (QueueHandle_t)parameter;  // queue for events
    event_t received_event = {};                 // event to be received
    event_t e = {};

    q_return = xQueueReceive(q, &received_event, portMAX_DELAY);  // wait for start signal
    if (q_return == pdTRUE && received_event.event_type == EVENT_START) {
        print("Starting task\n");
    }

    q_return = xQueueReceive(q, &received_event, portMAX_DELAY);  // at the start, wait until we get a mode indication
    if (q_return == pdTRUE && received_event.event_type == EVENT_MODE_CHANGED) {
        curr_mode = received_event.mode;
    } else {
        print("WARNING: Did not get a valid initial mode!\n");
    }

    for (;;) {
        // Check for received events
        q_return = xQueueReceive(q, &received_event, 0);
        if (q_return == pdTRUE) {
            switch (received_event.event_type) {
                case EVENT_MODE_CHANGED:
                    curr_mode = received_event.mode;
                    break;
                case EVENT_SPOTIFY_UPDATED:
                    sp_data = received_event.sp_data;
                    percent_complete = sp_data.track_progress * 100;
                    break;
                case EVENT_AUDIO_FRAME_DONE:
                    if (curr_mode.main.id() == MODE_MAIN_AUDIO) {
                        // int new_pos = SERVO_NOISE_POS;  // this moves to the audio function
                        // xQueueSend(q_servo, &new_pos, 0);
                        show_leds();
                    }
                    break;
                default:
                    print("WARNING: task_display_code received unexpected event type %d!\n", received_event.event_type);
            }
        }

        switch (curr_mode.main.id()) {
            case MODE_MAIN_ART: {
                // Display art and current elapsed regardless of if we have new data from the queue
                if (sp_data.art_loaded && sp_data.is_active) {
                    xSemaphoreTake(mutex_leds, portMAX_DELAY);

                    display_full_art(0, 0);
                    switch (curr_mode.sub.id()) {
                        case MODE_ART_WITH_ELAPSED: {  // Update the LED indicator at the bottom of the array
                            int grid_pos = int(round(percent_complete / 100 * GRID_W));

                            for (int i = 0; i < GRID_W; i++) {
                                if (i < grid_pos) {
                                    lp.set(i, CRGB::Red);
                                }
                            }
                            break;
                        }
                        case MODE_ART_WITH_PALETTE:  // Update the bottom of the array with the palette
                            for (int i = 0; i < PALETTE_ENTRIES; i++) {
                                lp.set(i, album_art.palette_crgb[i]);
                            }
                            break;
                    }
                    xSemaphoreGive(mutex_leds);
                    show_leds();
                } else {  // no art, go blank
                    // // move before we display
                    // event_t e = {.event_type = EVENT_SERVO_POS_CHANGED, {.servo_pos = SERVO_POS_NOISE}};
                    // eh.emit(e);

                    FastLED.clear();
                    show_leds();
                }
                break;
            }
            case MODE_MAIN_IMAGE:
                xSemaphoreTake(mutex_leds, portMAX_DELAY);
                char rcvd[CLI_MAX_CHARS];
                const char *filepath = "/image.jpg";

                // print("Enter url to jpg image: \n");
                // get_input(rcvd);

                if (!download_image("https://i.pinimg.com/736x/1a/b7/51/1ab75139f3b1e6ecc1f59ffc2a4b0f2e--mario-bros-mario-kart.jpg", filepath)) {
                    print("Failed to download image!\n");
                } else {
                    display_image(filepath);
                }

                xSemaphoreGive(mutex_leds);
                show_leds();
                break;
        }
        lp.blend_palettes(PALETTE_CHANGE_RATE);

        // vTaskDelay((1000 / FPS) / portTICK_RATE_MS);
        //   Serial.println(FastLED.getFPS());
        taskYIELD();  // yield first in case the next line doesn't actually delay
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void task_audio_code(void *parameter) {
    print("task_audio_code running on core ");
    print("%d\n", xPortGetCoreID());

    TickType_t xLastWakeTime;
    const TickType_t xFrequency = ((FFT_SAMPLES / 2.0) / I2S_SAMPLE_RATE * 1000) / portTICK_RATE_MS;

    AudioProcessor ap = AudioProcessor(false, false, true, true);

    CRGB last_leds[NUM_LEDS] = {0};  // capture the last led state before transitioning to a new mode;
    BaseType_t q_return;
    int servo_pos_delta = abs(SERVO_POS_ART - SERVO_POS_NOISE);
    int blend_counter = 0;
    int max_blend_count = int((SERVO_CYCLE_TIME_MS / 1000.0 * servo_pos_delta) * FPS * 1.25);

    QueueHandle_t q = (QueueHandle_t)parameter;  // q for receiving events
    event_t received_event = {};
    event_t e = {};
    curr_mode_t curr_mode, last_mode;

    q_return = xQueueReceive(q, &received_event, portMAX_DELAY);  // wait for start signal
    if (q_return == pdTRUE && received_event.event_type == EVENT_START) {
        print("Starting task\n");
    }

    q_return = xQueueReceive(q, &received_event, portMAX_DELAY);  // at the start, wait until we get a mode indication
    if (q_return == pdTRUE && received_event.event_type == EVENT_MODE_CHANGED) {
        curr_mode = received_event.mode;
    } else {
        print("WARNING: Did not get a valid initial mode!\n");
    }

    // Initialise the xLastWakeTime variable with the current time.
    xLastWakeTime = xTaskGetTickCount();

    int last_audio_mode = -1;
    for (;;) {
        if (ap.is_active()) {                                 // don't run the loop if AP failed to init
            q_return = xQueueReceive(q, &received_event, 0);  // check if there is a new mode, if not, we just use the last mode

            if (q_return == pdTRUE) {
                switch (received_event.event_type) {
                    case EVENT_MODE_CHANGED:
                        last_mode = curr_mode;
                        curr_mode = received_event.mode;
                        break;
                    default:
                        print("WARNING: task_audio_code received unexpected event type %d!\n", received_event.event_type);
                }
            }
            if (curr_mode.main.id() == MODE_MAIN_AUDIO) {
                int audio_mode = curr_mode.sub.id();

                run_audio(&ap, audio_mode);

                xSemaphoreTake(mutex_leds, portMAX_DELAY);
                // Blend with the last image on the led before we changed modes
                if (blend_counter == 0) {  // if the counter reset to 0 it means we changed modes
                    lp.copy_leds(last_leds, NUM_LEDS);
                    // calculate the number of cycles to take to fully blend based on servo move time, note this changes based on the mode change
                    servo_pos_delta = abs(curr_mode.sub.get_servo_pos() - last_mode.sub.get_servo_pos());
                    max_blend_count = int((SERVO_CYCLE_TIME_MS / 1000.0 * servo_pos_delta) * FPS * 1.25);
                }

                if (last_audio_mode != audio_mode) {
                    lp.set_audio_pattern(audio_mode);
                }
                last_audio_mode = audio_mode;

                lp.display_audio(ap.get_intensity());

                if (blend_counter <= max_blend_count) {
                    for (int i = 0; i < NUM_LEDS; i++) {
                        int blend_factor = (float)blend_counter / max_blend_count * 255;  // int(round(pow(float(blend_counter) / max_blend_count, 2.2) * 255));
                        lp.set(i, blend(last_leds[i], lp.get(i), blend_factor));
                    }
                    blend_counter += 1;
                }

                xSemaphoreGive(mutex_leds);

                e = {.event_type = EVENT_AUDIO_FRAME_DONE};
                eh.emit(e);
            } else {
                blend_counter = 0;  // reset the blend counter
            }
        }
        vTaskDelay(1);  // TODO: check this
        // taskYIELD();  // yield first in case the next line doesn't actually delay
        // vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }

    vTaskDelete(NULL);
}

void run_audio(AudioProcessor *ap, int audio_mode) {
    ap->get_audio_samples_gapless();
    ap->update_volume();
    ap->run_fft();

    if (audio_mode == MODE_AUDIO_SNAKE_GRID) {
        ap->calc_intensity(NUM_LEDS / 2);  // for a symmetric pattern, we only calculate intensities for half the LEDs
    } else {
        ap->calc_intensity_simple();
    }
}

void task_spotify_code(void *parameter) {
    print("task_spotify_code running on core ");
    print("%d\n", xPortGetCoreID());

    Preferences prefs;
    prefs.begin(APP_NAME);

    char client_id[CLI_MAX_CHARS];
    char auth_b64[CLI_MAX_CHARS];
    char refresh_token[CLI_MAX_CHARS];

    QueueHandle_t q = (QueueHandle_t)parameter;
    event_t received_event = {};
    BaseType_t q_return;
    curr_mode_t curr_mode;

    q_return = xQueueReceive(q, &received_event, portMAX_DELAY);  // wait for start signal
    if (q_return == pdTRUE && received_event.event_type == EVENT_START) {
        print("Starting task\n");
    }

    q_return = xQueueReceive(q, &received_event, portMAX_DELAY);  // at the start, wait until we get a mode indication
    if (q_return == pdTRUE && received_event.event_type == EVENT_MODE_CHANGED) {
        curr_mode = received_event.mode;
    } else {
        print("WARNING: Did not get a valid initial mode!\n");
    }

    if (!prefs.getString(PREFS_SPOTIFY_CLIENT_ID_KEY, client_id, CLI_MAX_CHARS) ||
        !prefs.getString(PREFS_SPOTIFY_AUTH_B64_KEY, auth_b64, CLI_MAX_CHARS) ||
        !prefs.getString(PREFS_SPOTIFY_REFRESH_TOKEN_KEY, refresh_token, CLI_MAX_CHARS)) {
        print("Spotify credentials not found!\n");
    }

    Spotify sp = Spotify(client_id, auth_b64, refresh_token);
    prefs.end();
    for (;;) {
        q_return = xQueueReceive(q, &received_event, 0);
        if (q_return == true && received_event.event_type == EVENT_MODE_CHANGED) {
            curr_mode = received_event.mode;
        }
        if ((WiFi.status() == WL_CONNECTED)) {
            if (curr_mode.main.id() != MODE_MAIN_IMAGE) {  // only run spotify loop if we are not in image download mode; otherwise the https code will
                sp.update();
                Spotify::public_data_t sp_data = sp.get_data();
                if (sp_data.art_changed && sp_data.is_active) {  // only update art if spotify is active
                    decode_art(sp_data.art_data, sp_data.art_num_bytes);
                }
                event_t e = {.event_type = EVENT_SPOTIFY_UPDATED, {.sp_data = sp_data}};
                eh.emit(e);  // set timeout to zero so loop will continue until display is updated
            }
        } else {
            print("Error: WiFi not connected! status = %d\n", WiFi.status());
        }
        vTaskDelay(SPOTIFY_CYCLE_TIME_MS / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}

void task_mode_code(void *parameter) {
    print("task_mode_code running on core ");
    print("%d\n", xPortGetCoreID());

    BaseType_t q_return;
    QueueHandle_t q = (QueueHandle_t)parameter;
    event_t received_event = {};

    Mode MAIN_MODES_LIST[] = {
        Mode(MODE_MAIN_ART, SERVO_POS_ART, DURATION_MS_ART),
        Mode(MODE_MAIN_AUDIO, SERVO_POS_NOISE, DURATION_MS_AUDIO)};  //,
    // Mode(MODE_MAIN_IMAGE, SERVO_POS_ART, DURATION_MS_AUDIO)};

    Mode ART_SUB_MODES_LIST[] = {
        Mode(MODE_ART_WITHOUT_ELAPSED, SERVO_POS_ART),
        Mode(MODE_ART_WITH_ELAPSED, SERVO_POS_ART),
        Mode(MODE_ART_WITH_PALETTE, SERVO_POS_ART)};

    Mode AUDIO_SUB_MODES_LIST[] = {
        Mode(MODE_AUDIO_NOISE, SERVO_POS_NOISE),
        Mode(MODE_AUDIO_SNAKE_GRID, SERVO_POS_GRID),
        Mode(MODE_AUDIO_BARS, SERVO_POS_BARS),
        Mode(MODE_AUDIO_CENTER_BARS, SERVO_POS_BARS),
        Mode(MODE_AUDIO_WATERFALL, SERVO_POS_NOISE)};

    // Mode IMAGE_SUB_MODES_LIST[] = {};

    ModeSequence art_sub_modes = ModeSequence(ART_SUB_MODES_LIST, ARRAY_SIZE(ART_SUB_MODES_LIST));
    ModeSequence audio_sub_modes = ModeSequence(AUDIO_SUB_MODES_LIST, ARRAY_SIZE(AUDIO_SUB_MODES_LIST));
    // ModeSequence image_sub_modes = ModeSequence(IMAGE_SUB_MODES_LIST, ARRAY_SIZE(IMAGE_SUB_MODES_LIST));
    ModeSequence sub_modes[] = {art_sub_modes, audio_sub_modes};  //, image_sub_modes};

    ModeSequence main_modes = ModeSequence(MAIN_MODES_LIST, ARRAY_SIZE(MAIN_MODES_LIST), sub_modes);

    Spotify::public_data_t sp_data;

    bool mode_changed;

    q_return = xQueueReceive(q, &received_event, portMAX_DELAY);  // wait for start signal
    if (q_return == pdTRUE && received_event.event_type == EVENT_START) {
        print("Starting task\n");
    }

    // seed the event handler with our default mode
    curr_mode_t curr_mode = main_modes.get_curr_mode();
    button_event_t button_info;
    event_t e = {.event_type = EVENT_MODE_CHANGED, {.mode = curr_mode}};
    eh.emit(e);

    for (;;) {
        mode_changed = false;

        // Check for power off
        if (digitalRead(PIN_POWER_SWITCH) == HIGH) {
            // we want to park the display at the SERVO_POS_NOISE, which is the start position
            e = {.event_type = EVENT_SERVO_POS_CHANGED, {.servo_pos = SERVO_POS_NOISE}};
            eh.emit(e);

            xSemaphoreTake(mutex_leds, portMAX_DELAY);
            FastLED.clear(true);
            int servo_pos_delta = abs(curr_mode.sub.get_servo_pos() - SERVO_POS_NOISE);
            vTaskDelay((servo_pos_delta * SERVO_CYCLE_TIME_MS) / portTICK_RATE_MS);  // wait for servo move
            print("Switch is high, going to sleep\n");
            esp_deep_sleep_start();
        }

        q_return = xQueueReceive(q, &received_event, 0);  // get any updates
        if (q_return == pdTRUE) {
            switch (received_event.event_type) {
                case EVENT_SPOTIFY_UPDATED:
                    sp_data = received_event.sp_data;
                    if (sp_data.art_changed || !sp_data.is_active) {
                        main_modes.set(MODE_MAIN_ART);  // show art on track change
                        mode_changed = true;
                    }
                    break;
                case EVENT_BUTTON_PRESSED:
                    button_info = received_event.button_info;
                    switch (button_info.state) {
                        case ButtonFSM::HOLD_TRIGGERED:
                            print("Hold, button %d\n", button_info.id);
                            break;
                        case ButtonFSM::MOMENTARY_TRIGGERED:
                            print("Momentary, button %d\n", button_info.id);
                            switch (button_info.id) {
                                case PIN_BUTTON_MODE:
                                    main_modes.submode_next();
                                    mode_changed = true;
                                    break;
                                case PIN_BUTTON2_MODE:
                                    main_modes.next();
                                    mode_changed = true;
                                    break;
                            }
                            break;
                        default:
                            print("WARNING: button state %d not recognized\n", button_info.state);
                            break;
                    }
                    break;
                default:
                    print("WARNING: task_mode_code received unexpected event type %d!\n", received_event.event_type);
                    break;
            }
        }

        // check if mode timers have elapsed
        if (main_modes.mode().elapsed()) {
            print("Main mode elapsed, cycling modes\n");
            main_modes.next();
            mode_changed = true;
        }
        if (main_modes.submode().elapsed()) {
            print("Submode elapsed, cycling submodes\n");
            main_modes.submode_next();
            mode_changed = true;
        }

        // let other tasks know the mode has changed
        if (mode_changed) {
            curr_mode = main_modes.get_curr_mode();
            event_t e = {.event_type = EVENT_MODE_CHANGED, {.mode = curr_mode}};
            eh.emit(e);
        }

        // Serial.println(uxTaskGetStackHighWaterMark(NULL));
        vTaskDelay(BUTTON_FSM_CYCLE_TIME_MS / portTICK_RATE_MS);  // added to avoid starving other tasks
    }
    vTaskDelete(NULL);
}

void task_buttons_code(void *parameter) {
    print("task_buttons_code running on core ");
    print("%d\n", xPortGetCoreID());

    BaseType_t q_return;
    QueueHandle_t q = (QueueHandle_t)parameter;
    event_t received_event = {};

    int nbuttons = 2;
    ButtonFSM button1 = ButtonFSM(PIN_BUTTON_MODE);
    ButtonFSM button2 = ButtonFSM(PIN_BUTTON2_MODE);

    ButtonFSM buttons[] = {button1, button2};
    ButtonFSM::button_fsm_state_t state;

    q_return = xQueueReceive(q, &received_event, portMAX_DELAY);  // wait for start signal
    if (q_return == pdTRUE && received_event.event_type == EVENT_START) {
        print("Starting task\n");
    }

    for (;;) {
        for (int i = 0; i < nbuttons; i++) {
            buttons[i].advance();
            state = buttons[i].get_state();

            if (state == ButtonFSM::HOLD_TRIGGERED || state == ButtonFSM::MOMENTARY_TRIGGERED) {
                button_event_t button_event = {.id = buttons[i].get_id(), .state = state};
                event_t e = {.event_type = EVENT_BUTTON_PRESSED,
                             {.button_info = button_event}};
                eh.emit(e);
            }
        }

        // Serial.println(uxTaskGetStackHighWaterMark(NULL));
        vTaskDelay(BUTTON_FSM_CYCLE_TIME_MS / portTICK_RATE_MS);  // added to avoid starving other tasks
    }
    vTaskDelete(NULL);
}

void task_servo_code(void *parameter) {
    print("task_servo_code running on core ");
    print("%d\n", xPortGetCoreID());

    // Servo Setup
    Servo myservo;
    print("Setting up servo\n");
    myservo.write(SERVO_POS_NOISE);  // Set servo zero position prior to attaching in order to mitigate power-on glitch
    myservo.attach(PIN_SERVO, SERVO_MIN_US, SERVO_MAX_US);
    // servo_pos = myservo.read();  // Determine where the servo is -- Note: this will always report 82, regardless of where servo actually is.
    // we will instead always park the servo at the same position prior to sleeping

    int target_pos, curr_pos;
    BaseType_t q_return;

    // Initialize variables
    curr_pos = SERVO_POS_NOISE;  // this is the default position we should end at on power down
    myservo.write(curr_pos);

    target_pos = curr_pos;

    QueueHandle_t q = (QueueHandle_t)parameter;  // q for receiving events
    event_t received_event = {};

    q_return = xQueueReceive(q, &received_event, portMAX_DELAY);  // wait for start signal
    if (q_return == pdTRUE && received_event.event_type == EVENT_START) {
        print("Starting task\n");
    }

    for (;;) {
        curr_pos = myservo.read();

#ifdef SERVO_DEBUG
        if (Serial.available()) {  // if wants manual control, honor it
            char c = Serial.read();
            switch (c) {
                case 'f':
                    target_pos += 1;
                    break;
                case 'b':
                    target_pos -= 1;
                    break;
            }
            print("Current servo pos: %d, Requested servo pos: %d\n", curr_pos, target_pos);
        }
#else
        q_return = xQueueReceive(q, &received_event, 0);  // check if there is a new value in the queue
        if (q_return == pdTRUE) {
            switch (received_event.event_type) {
                case EVENT_SERVO_POS_CHANGED:
                    target_pos = received_event.servo_pos;
                    break;

                case EVENT_MODE_CHANGED:
                    target_pos = received_event.mode.sub.get_servo_pos();
                    break;
                default:
                    print("WARNING: task_servo_code received unexpected event type %d!\n", received_event.event_type);
                    break;
            }
            print("Current servo pos: %d, Requested servo pos: %d\n", curr_pos, target_pos);
        }
#endif
        target_pos = constrain(target_pos, SERVO_MIN_POS, SERVO_MAX_POS);

        // if (curr_pos != target_pos) {  // only try to move if we are going to a new position
        //     Serial.println("Servo starting pos: " + String(curr_pos));
        if (curr_pos < target_pos) {
            // for (int i = curr_pos; i <= target_pos; i++) {
            myservo.write(curr_pos + 1);
            //     vTaskDelay(SERVO_CYCLE_TIME_MS / portTICK_RATE_MS);
            // }
        } else if (curr_pos > target_pos) {
            // for (int i = curr_pos; i >= target_pos; i--) {
            myservo.write(curr_pos - 1);
            // vTaskDelay(SERVO_CYCLE_TIME_MS / portTICK_RATE_MS);
        }
        // }
        // Serial.println("Servo ending pos: " + String(target_pos));
        vTaskDelay(SERVO_CYCLE_TIME_MS / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}

void display_full_art(uint8_t offset_row, uint8_t offset_col) {
    // static int counter_static = 0;
    // static int offset_row_static = 0;
    // static int offset_col_static = 0;

    // if (counter_static % 10 == 0) {
    //     offset_row_static = rand() % 4;
    //     offset_col_static = rand() % 4;
    // }
    // counter_static++;

    for (int row = 0; row < GRID_H; row++) {
        for (int col = 0; col < GRID_W; col++) {
            // uint8_t full_row = row * 4 + offset_row_static;
            // uint8_t full_col = col * 4 + offset_col_static;
            uint8_t full_row = row * 4 + offset_row;
            uint8_t full_col = col * 4 + offset_col;

            // Select the last row/col so artwork with borders looks cleaner
            if (row == GRID_H - 1) full_row = GRID_H * 4 - 1;
            if (col == GRID_W - 1) full_col = GRID_W * 4 - 1;

            uint16_t rgb565 = album_art.full_art_rgb565[full_row][full_col];

            uint8_t r5 = (rgb565 >> 11) & 0x1F;
            uint8_t g6 = (rgb565 >> 5) & 0x3F;
            uint8_t b5 = (rgb565)&0x1F;

            uint8_t r8 = round(pow(float(r5) / 31, LED_GAMMA_R / JPG_GAMMA) * 255);
            uint8_t g8 = round(pow(float(g6) / 63, LED_GAMMA_G / JPG_GAMMA) * 255);
            uint8_t b8 = round(pow(float(b5) / 31, LED_GAMMA_B / JPG_GAMMA) * 255);

            // Serial.println(r5);
            // Serial.println(g6);
            // Serial.println(b5);

            // Serial.println(r8);
            // Serial.println(g8);
            // Serial.println(b8);

            // Serial.println();

            int idx = lp.grid_to_idx(col, row, true);
            if (idx >= 0) {
                lp.set(idx, blend(lp.get(idx), CRGB(r8, g8, b8), int(round(LED_SMOOTHING * 0.25 * 255))));
            }
        }
    }
}

bool copy_jpg_data(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            uint8_t full_row = y + row;
            uint8_t full_col = x + col;
            uint8_t bitmap_idx = row * w + col;
            album_art.full_art_rgb565[full_row][full_col] = bitmap[bitmap_idx];  // copy full 64x64 image into full_art
            if ((full_row % 4 == 0) && (full_col % 4 == 0)) {
                album_art.palette_art_rgb565[int(full_row / 4)][int(full_col / 4)] = bitmap[bitmap_idx];  // copy a decimated version for palette calc
            }
        }
    }

    return true;
}

// Decode art from jpg into full_art, and calculate palette
void decode_art(uint8_t *art_data, unsigned long art_num_bytes) {
    print("Decoding art, %d bytes\n", art_num_bytes);

    TJpgDec.setJpgScale(1);                          // no rescaling
    TJpgDec.setCallback(copy_jpg_data);              // The decoder must be given the exact name of the rendering function above
    TJpgDec.drawJpg(0, 0, art_data, art_num_bytes);  // decode and copy jpg data into full_art

    // Calculate color palette
    uint8_t palette_results_rgb888[PALETTE_ENTRIES][3] = {0};
    mean_cut((uint16_t *)album_art.palette_art_rgb565, 16 * 16, MEAN_CUT_DEPTH, (uint8_t *)palette_results_rgb888);
    print("Finished mean cut, printing returned results\n");
    for (int i = 0; i < PALETTE_ENTRIES; i++) {
        print("%d, %d, %d\n", palette_results_rgb888[i][0], palette_results_rgb888[i][1], palette_results_rgb888[i][2]);
        uint8_t r8 = round(pow(float(palette_results_rgb888[i][0]) / 255, LED_GAMMA_R / JPG_GAMMA) * 255);
        uint8_t g8 = round(pow(float(palette_results_rgb888[i][1]) / 255, LED_GAMMA_G / JPG_GAMMA) * 255);
        uint8_t b8 = round(pow(float(palette_results_rgb888[i][2]) / 255, LED_GAMMA_B / JPG_GAMMA) * 255);
        album_art.palette_crgb[i] = CRGB(r8, g8, b8);
    }

    lp.set_target_palette(CRGBPalette16(album_art.palette_crgb[0], album_art.palette_crgb[1], album_art.palette_crgb[2], album_art.palette_crgb[3],
                                        album_art.palette_crgb[4], album_art.palette_crgb[5], album_art.palette_crgb[6], album_art.palette_crgb[7],
                                        album_art.palette_crgb[8], album_art.palette_crgb[9], album_art.palette_crgb[10], album_art.palette_crgb[11],
                                        album_art.palette_crgb[12], album_art.palette_crgb[13], album_art.palette_crgb[14], album_art.palette_crgb[15]));
}

bool download_image(const char *url, const char *filepath) {
    bool ret;
    int start_ms = millis();
    print("Downloading %s\n", url);

    File file = SPIFFS.open(filepath, FILE_WRITE);
    if (!file) {
        print("Could not open filepath: %s\n", filepath);
        return false;
    }

    HTTPClient http;
    http.begin(url);

    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        // Get length of document (is -1 when Server sends no Content-Length header)
        int len = http.getSize();
        print("%d bytes\n", len);

        // Create buffer for read
        uint8_t buff[128] = {0};

        // Get tcp stream
        WiFiClient *stream = http.getStreamPtr();

        // Read all data from server
        while (http.connected() && (len > 0 || len == -1)) {
            // Get available data size
            size_t size = stream->available();

            if (size) {
                // Read up to 128 bytes
                int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

                // Write it to file
                file.write(buff, c);

                // Calculate remaining bytes
                if (len > 0) {
                    len -= c;
                }
            }
            yield();
        }
        print("%dms to download file\n", millis() - start_ms);
        ret = true;
    } else {
        print("%d:%s: Unrecognized error\n", httpCode, __func__);
        ret = false;
    }
    file.close();
    http.end();

    return ret;
}

void display_image(const char *filepath) {
    // Check that file is accessible
    File file = SPIFFS.open(filepath);
    if (!file) {
        print("Failed to open filepath: %s\n", filepath);
        return;
    }
    print("file size is: %d\n", file.size());
    file.close();

    uint16_t w, h, max_dim;
    TJpgDec.setCallback(display_jpg_data);  // The decoder must be given the exact name of the rendering function above
    TJpgDec.getFsJpgSize(&w, &h, filepath);
    print("Image w = %d, h = %d\n", w, h);
    if (w > GRID_W || h > GRID_H) {
        max_dim = max(w, h);
        double raw_scale = (double)max_dim / GRID_H;
        int scale2 = constrain(int(round(log2(raw_scale))), 0, 3);
        int scale = (int)pow(2, scale2);
        print("Setting jpg scale to %d\n", scale);
        TJpgDec.setJpgScale(scale);
    } else {
        TJpgDec.setJpgScale(1);
    }

    TJpgDec.drawFsJpg(0, 0, filepath);

    FastLED.show();
}

// Display image directly to LEDs
bool display_jpg_data(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
    // Note bitmap is in RGB565 format!!!
    // Stop further decoding as image is running off bottom of screen
    if (y >= GRID_H) return false;

    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            uint16_t rgb565 = bitmap[row * w + col];
            uint8_t r5 = (rgb565 >> 11) & 0x1F;
            uint8_t g6 = (rgb565 >> 5) & 0x3F;
            uint8_t b5 = (rgb565)&0x1F;

            uint8_t r8 = round((float(r5) / 31) * 255);
            uint8_t g8 = round((float(g6) / 63) * 255);
            uint8_t b8 = round((float(b5) / 31) * 255);

            // Serial.println(r5);
            // Serial.println(g6);
            // Serial.println(b5);

            // Serial.println(r8);
            // Serial.println(g8);
            // Serial.println(b8);

            // Serial.println();

            lp.set_xy(x + col, y + row, CRGB(r8, g8, b8), true);
        }
    }

    return true;
}
