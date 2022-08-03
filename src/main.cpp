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
#include "Spotify.h"
#include "Utils.h"

/*** Function Prototypes ***/

// Display Functions
bool copy_jpg_data(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);  // callback function for JPG decoder
void display_full_art(uint8_t offset_row, uint8_t offset_col);
void decode_art(uint8_t *art_data, unsigned long art_num_bytes);

void run_audio(AudioProcessor *ap, int audio_mode);

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

// Task functions
void task_eventhandler_code(void *parameter);
void task_buttons_code(void *parameter);
void task_spotify_code(void *parameter);
void task_audio_code(void *parameter);
void task_display_code(void *parameter);
void task_servo_code(void *parameter);

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
        print("Missing preferences! Power off and power on while pressing the main button to configure preferences.\n");
    }

    // Wifi setup
    connect_wifi();

    // JPG decoder setup
    print("Setting up JPG decoder\n");
    TJpgDec.setJpgScale(1);              // Assuming 64x64 image, will rescale to 16x16
    TJpgDec.setCallback(copy_jpg_data);  // The decoder must be given the exact name of the rendering function above

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

    // Setup eventhandler object and task
    q_events = xQueueCreate(MAX_EVENTHANDLER_EVENTS, sizeof(event_t));
    eh = EventHandler(q_events);
    xTaskCreatePinnedToCore(
        task_eventhandler_code,  // Function to implement the task
        "task_eventhandler",     // Name of the task
        2000,                    // Stack size in words
        NULL,                    // Task input parameter
        2,                       // Priority of the task (don't use 0!)
        &task_eventhandler,      // Task handle
        1                        // Pinned core
    );

    q_spotify = xQueueCreate(10, sizeof(event_t));
    xTaskCreatePinnedToCore(
        task_spotify_code,  // Function to implement the task
        "task_spotify",     // Name of the task
        10000,              // Stack size in words
        q_spotify,          // Task input parameter
        1,                  // Priority of the task (don't use 0!)
        &task_spotify,      // Task handle
        0                   // Pinned core - 0 is the same core as WiFi
    );
    eh.register_task(task_spotify, q_spotify);

    q_display = xQueueCreate(10, sizeof(event_t));
    xTaskCreatePinnedToCore(
        task_display_code,  // Function to implement the task
        "task_display",     // Name of the task
        10000,              // Stack size in words
        q_display,          // Task input parameter
        1,                  // Priority of the task (don't use 0!)
        &task_display,      // Task handle
        1                   // Pinned core, 1 is preferred to avoid glitches (see: https://www.reddit.com/r/FastLED/comments/rfl6rz/esp32_wifi_on_core_1_fastled_on_core_0/)
    );
    eh.register_task(task_display, q_display, EVENT_BUTTON_PRESSED | EVENT_SPOTIFY_UPDATED | EVENT_AUDIO_FRAME_DONE);

    q_buttons = xQueueCreate(10, sizeof(event_t));
    xTaskCreatePinnedToCore(
        task_buttons_code,  // Function to implement the task
        "task_buttons",     // Name of the task
        10000,              // Stack size in words
        q_buttons,          // Task input parameter
        1,                  // Priority of the task (don't use 0!)
        &task_buttons,      // Task handle
        1                   // Pinned core
    );
    eh.register_task(task_buttons, q_buttons);

    q_audio = xQueueCreate(10, sizeof(event_t));
    xTaskCreatePinnedToCore(
        task_audio_code,  // Function to implement the task
        "task_audio",     // Name of the task
        30000,            // Stack size in words
        q_audio,          // Task input parameter
        1,                // Priority of the task (don't use 0!)
        &task_audio,      // Task handle
        1                 // Pinned core
    );
    eh.register_task(task_audio, q_audio, EVENT_MODE_CHANGED);

    q_servo = xQueueCreate(10, sizeof(event_t));
    xTaskCreatePinnedToCore(
        task_servo_code,  // Function to implement the task
        "task_servo",     // Name of the task
        2000,             // Stack size in words
        q_servo,          // Task input parameter
        1,                // Priority of the task (don't use 0!)
        &task_servo,      // Task handle
        1                 // Pinned core
    );
    eh.register_task(task_servo, q_servo, EVENT_SERVO_POS_CHANGED);
}

void task_eventhandler_code(void *parameter) {
    print("task_eventhandler_code running on core ");
    print("%d\n", xPortGetCoreID());

    event_t e = {};
    for (;;) {
        xQueueReceive(q_events, &e, portMAX_DELAY);
        print("Received event, %d\n", e.event_type);
        eh.process(e);
    }
}

void loop() {
    vTaskDelete(NULL);  // delete setup and loop tasks
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

    Modes_t modes;

    // initial modes
    modes.display = Mode(DISPLAY_ART, DISPLAY_MODES_MAX, DISPLAY_DURATIONS);
    modes.art = Mode(ART_WITHOUT_ELAPSED, ART_MODES_MAX);
    modes.audio = Mode(AUDIO_NOISE, AUDIO_MODES_MAX);

    // Initialise the xLastWakeTime variable with the current time.
    xLastWakeTime = xTaskGetTickCount();

    QueueHandle_t q = (QueueHandle_t)parameter;  // queue for events
    event_t received_event = {};                 // event to be received

    for (;;) {
        // Check for button push and mode change
        q_return = xQueueReceive(q, &received_event, 0);
        if (q_return == pdTRUE && received_event.event_type == EVENT_BUTTON_PRESSED) {
            button_state = received_event.button_state;
            switch (button_state) {
                case ButtonFSM::MOMENTARY_TRIGGERED:  // change the sub-mode
                    switch (modes.display.get_mode()) {
                        case DISPLAY_ART:
                            print("Changing art display type\n");
                            modes.art.cycle_mode();
                            break;
                        case DISPLAY_AUDIO:
                            print("Changing audio display type\n");
                            modes.audio.cycle_mode();
                            break;
                    }
                    break;

                case ButtonFSM::HOLD_TRIGGERED:  // change the main mode
                    print("Changing display mode\n");
                    modes.display.cycle_mode();
                    break;
            }
            event_t e = {.event_type = EVENT_MODE_CHANGED, {.modes = modes}};
            eh.emit(e);  // send the mode so the audio task knows what state we're in
        }

        if (modes.display.mode_elapsed()) {
            print("Timer elapsed, cycling display mode\n");
            modes.display.cycle_mode();
            event_t e = {.event_type = EVENT_MODE_CHANGED, {.modes = modes}};
            eh.emit(e);  // send the mode so the audio task knows what state we're in
        }

        if (q_return == pdTRUE && received_event.event_type == EVENT_SPOTIFY_UPDATED) {
            sp_data = received_event.sp_data;
            if (sp_data.is_active) {
                percent_complete = sp_data.track_progress * 100;
                // Serial.println("Playing..." + String(percent_complete) + "%");
            } else {
                modes.display.set_mode(DISPLAY_ART);  // will be blank when there's no art
                event_t e = {.event_type = EVENT_MODE_CHANGED, {.modes = modes}};
                eh.emit(e);  // send the mode so the audio task knows what state we're in
            }
            if (sp_data.art_changed) {
                modes.display.set_mode(DISPLAY_ART);  // show art on track change
                event_t e = {.event_type = EVENT_MODE_CHANGED, {.modes = modes}};
                eh.emit(e);  // send the mode so the audio task knows what state we're in
            }
        }

        // Display based on the mode
        switch (modes.display.get_mode()) {
            case DISPLAY_ART: {
                // Display art and current elapsed regardless of if we have new data from the queue
                if (sp_data.art_loaded && sp_data.is_active) {
                    xSemaphoreTake(mutex_leds, portMAX_DELAY);

                    display_full_art(0, 0);
                    switch (modes.art.get_mode()) {
                        case ART_WITH_ELAPSED: {  // Update the LED indicator at the bottom of the array
                            int grid_pos = int(round(percent_complete / 100 * GRID_W));

                            for (int i = 0; i < GRID_W; i++) {
                                if (i < grid_pos) {
                                    lp.set(i, CRGB::Red);
                                }
                            }
                            break;
                        }
                        case ART_WITH_PALETTE:  // Update the bottom of the array with the palette
                            for (int i = 0; i < PALETTE_ENTRIES; i++) {
                                lp.set(i, album_art.palette_crgb[i]);
                            }
                            break;
                    }
                    xSemaphoreGive(mutex_leds);
                    show_leds();

                    int new_pos = SERVO_ART_POS;
                    event_t e = {.event_type = EVENT_SERVO_POS_CHANGED, {.servo_pos = new_pos}};
                    eh.emit(e);  // move after we have displayed
                } else {         // no art, go blank
                    int new_pos = SERVO_NOISE_POS;
                    event_t e = {.event_type = EVENT_SERVO_POS_CHANGED, {.servo_pos = new_pos}};
                    eh.emit(e);  // move before we display
                    FastLED.clear();
                    show_leds();
                }
                break;
            }
            case DISPLAY_AUDIO: {
                // int new_pos = SERVO_NOISE_POS;  // this moves to the audio function
                // event_t e = {.event_type = EVENT_SERVO_POS_CHANGED, .servo_pos = new_pos};
                // eh.emit(e);

                uint8_t done;
                // xQueueReceive(q_audio_done, &done, portMAX_DELAY);    TODO: need to fix the loop so we can still catch this event
                show_leds();
                break;
            }
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
    Modes_t modes;
    CRGB last_leds[NUM_LEDS] = {0};  // capture the last led state before transitioning to a new mode;
    BaseType_t q_return;
    int blend_counter = 0;

    // calculate the number of cycles to take to fully blend based on servo move time
    // TODO: is this correct? since this task is not operating at FPS speed
    int max_blend_count = int((SERVO_CYCLE_TIME_MS / 1000.0 * (SERVO_ART_POS - SERVO_NOISE_POS)) * FPS * 1.25);

    QueueHandle_t q = (QueueHandle_t)parameter;  // q for receiving events
    event_t received_event = {};

    xQueueReceive(q, &received_event, portMAX_DELAY);  // at the start, wait until we get a mode indication, TODO: need to check that event is of type MODE_CHANGE!!
    modes = received_event.modes;

    // Initialise the xLastWakeTime variable with the current time.
    xLastWakeTime = xTaskGetTickCount();

    int last_audio_mode = -1;
    for (;;) {
        if (ap.is_active()) {                                 // don't run the loop if AP failed to init
            q_return = xQueueReceive(q, &received_event, 0);  // check if there is a new mode, if not, we just use the last mode

            if (q_return == pdTRUE && received_event.event_type == EVENT_MODE_CHANGED) {
                modes = received_event.modes;
            }
            if (modes.display.get_mode() == DISPLAY_AUDIO) {
                int audio_mode = modes.audio.get_mode();

                int new_pos = AUDIO_SERVO_POSITIONS[audio_mode];
                event_t e = {.event_type = EVENT_SERVO_POS_CHANGED, {.servo_pos = new_pos}};
                eh.emit(e);

                run_audio(&ap, audio_mode);

                xSemaphoreTake(mutex_leds, portMAX_DELAY);
                // Blend with the last image on the led before we changed modes
                if (blend_counter == 0) {  // if the counter reset to 0 it means we changed modes
                    lp.copy_leds(last_leds, NUM_LEDS);
                }

                if (last_audio_mode != audio_mode) {
                    lp.set_audio_pattern(audio_mode);
                }
                last_audio_mode = audio_mode;

                lp.display_audio(ap.get_intensity());

                if (blend_counter <= max_blend_count) {
                    for (int i = 0; i < NUM_LEDS; i++) {
                        int blend_factor = int(round(pow(float(blend_counter) / max_blend_count, 2.2) * 255));
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

    if (audio_mode == AUDIO_SNAKE_GRID) {
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

    if (!prefs.getString(PREFS_SPOTIFY_CLIENT_ID_KEY, client_id, CLI_MAX_CHARS) ||
        !prefs.getString(PREFS_SPOTIFY_AUTH_B64_KEY, auth_b64, CLI_MAX_CHARS) ||
        !prefs.getString(PREFS_SPOTIFY_REFRESH_TOKEN_KEY, refresh_token, CLI_MAX_CHARS)) {
        print("Spotify credentials not found!\n");
    }

    Spotify sp = Spotify(client_id, auth_b64, refresh_token);
    prefs.end();

    for (;;) {
        if ((WiFi.status() == WL_CONNECTED)) {
            sp.update();
            Spotify::public_data_t sp_data = sp.get_data();
            if (sp_data.art_changed) {
                decode_art(sp_data.art_data, sp_data.art_num_bytes);
            }
            event_t e = {.event_type = EVENT_SPOTIFY_UPDATED, {.sp_data = sp_data}};
            eh.emit(e);  // set timeout to zero so loop will continue until display is updated
        } else {
            print("Error: WiFi not connected! status = %d\n", WiFi.status());
        }
        vTaskDelay(SPOTIFY_CYCLE_TIME_MS / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}

void task_buttons_code(void *parameter) {
    print("task_buttons_code running on core ");
    print("%d\n", xPortGetCoreID());

    ButtonFSM button_fsm = ButtonFSM(PIN_BUTTON_MODE);
    ButtonFSM::button_fsm_state_t button_state;

    for (;;) {
        // Check for power off
        if (digitalRead(PIN_POWER_SWITCH) == HIGH) {
            int new_pos = SERVO_NOISE_POS;
            event_t e = {.event_type = EVENT_SERVO_POS_CHANGED, {.servo_pos = new_pos}};
            eh.emit(e);

            xSemaphoreTake(mutex_leds, portMAX_DELAY);

            vTaskDelay(((SERVO_ART_POS - SERVO_NOISE_POS) * SERVO_CYCLE_TIME_MS) / portTICK_RATE_MS);  // wait for servo move
            print("Switch is high, going to sleep\n");
            FastLED.clear(true);
            esp_deep_sleep_start();
        }
        button_fsm.advance();
        button_state = button_fsm.get_state();

        if (button_state == ButtonFSM::HOLD_TRIGGERED) {  // change modes
            event_t e = {.event_type = EVENT_BUTTON_PRESSED, {.button_state = button_state}};
            eh.emit(e);

            print("Hold button\n");
        }
        if (button_state == ButtonFSM::MOMENTARY_TRIGGERED) {  // move the servos
            event_t e = {.event_type = EVENT_BUTTON_PRESSED, {.button_state = button_state}};
            eh.emit(e);

            print("Momentary button\n");
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
    myservo.write(SERVO_NOISE_POS);  // Set servo zero position prior to attaching in order to mitigate power-on glitch
    myservo.attach(PIN_SERVO, SERVO_MIN_US, SERVO_MAX_US);
    // servo_pos = myservo.read();  // Determine where the servo is -- Note: this will always report 82, regardless of where servo actually is.
    // we will instead always park the servo at the same position prior to sleeping

    int target_pos, curr_pos;
    BaseType_t q_return;

    // Initialize variables
    curr_pos = SERVO_NOISE_POS;  // this is the default position we should end at on power down
    myservo.write(curr_pos);

    target_pos = curr_pos;

    QueueHandle_t q = (QueueHandle_t)parameter;  // q for receiving events
    event_t received_event = {};

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
        if (q_return == pdTRUE && received_event.event_type == EVENT_SERVO_POS_CHANGED) {
            target_pos = received_event.servo_pos;
        }
        if ((q_return == pdTRUE) && (curr_pos != target_pos)) {
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
