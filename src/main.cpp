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
#include "Event.h"
#include "LEDPanel.h"
#include "MeanCut.h"
#include "Mode.h"
#include "ModeSequence.h"
#include "Spotify.h"
#include "Utils.h"

/*** Function Prototypes ***/

// Display Functions
bool copy_jpg_data(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);  // callback function for JPG decoder
void display_full_art(uint8_t offset_row, uint8_t offset_col);
void decode_art(uint8_t *art_data, unsigned long art_num_bytes);

void run_audio(AudioProcessor *ap, int audio_mode);

/*** Globals ***/

// Queues to move data between tasks
QueueHandle_t q_spotify;
QueueHandle_t q_servo;
QueueHandle_t q_mode;
QueueHandle_t q_audio_done;
QueueHandle_t q_event;
QueueHandle_t q_task_servo;
QueueHandle_t q_task_audio;
QueueHandle_t q_task_spotify;
QueueHandle_t q_task_buttons;
QueueHandle_t q_task_display;

// Semaphores
SemaphoreHandle_t mutex_leds;

// Task & Queue Handles
TaskHandle_t task_buttons;
TaskHandle_t task_spotify;
TaskHandle_t task_audio;
TaskHandle_t task_display;
TaskHandle_t task_servo;
TaskHandle_t task_setup;

// Task functions
void task_spotify_code(void *parameter);
void task_audio_code(void *parameter);
void task_buttons_code(void *parameter);
void task_display_code(void *parameter);
void task_servo_code(void *parameter);
void task_setup_code(void *parameter);

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
    // q_servo = xQueueCreate(10, sizeof(int));
    // q_mode = xQueueCreate(1, sizeof(ModeSequence));  // we only want one active mode at a time
    // q_audio_done = xQueueCreate(1, sizeof(uint8_t));

    q_event = xQueueCreate(10, sizeof(event_t));
    q_task_spotify = xQueueCreate(10, sizeof(event_t));
    q_task_display = xQueueCreate(10, sizeof(event_t));
    q_task_buttons = xQueueCreate(10, sizeof(event_t));
    q_task_servo = xQueueCreate(10, sizeof(event_t));
    q_task_audio = xQueueCreate(10, sizeof(event_t));

    disableCore0WDT();  // hack to avoid various kernel panics

    xTaskCreatePinnedToCore(
        task_spotify_code,  // Function to implement the task
        "task_spotify",     // Name of the task
        10000,              // Stack size in words
        NULL,               // Task input parameter
        1,                  // Priority of the task (don't use 0!)
        &task_spotify,      // Task handle
        0                   // Pinned core - 0 is the same core as WiFi
    );

    xTaskCreatePinnedToCore(
        task_display_code,  // Function to implement the task
        "task_display",     // Name of the task
        10000,              // Stack size in words
        NULL,               // Task input parameter
        1,                  // Priority of the task (don't use 0!)
        &task_display,      // Task handle
        1                   // Pinned core, 1 is preferred to avoid glitches (see: https://www.reddit.com/r/FastLED/comments/rfl6rz/esp32_wifi_on_core_1_fastled_on_core_0/)
    );

    xTaskCreatePinnedToCore(
        task_buttons_code,  // Function to implement the task
        "task_buttons",     // Name of the task
        10000,              // Stack size in words
        NULL,               // Task input parameter
        1,                  // Priority of the task (don't use 0!)
        &task_buttons,      // Task handle
        1                   // Pinned core
    );

    xTaskCreatePinnedToCore(
        task_audio_code,  // Function to implement the task
        "task_audio",     // Name of the task
        30000,            // Stack size in words
        NULL,             // Task input parameter
        1,                // Priority of the task (don't use 0!)
        &task_audio,      // Task handle
        1                 // Pinned core
    );

    xTaskCreatePinnedToCore(
        task_servo_code,  // Function to implement the task
        "task_servo",     // Name of the task
        2000,             // Stack size in words
        NULL,             // Task input parameter
        1,                // Priority of the task (don't use 0!)
        &task_servo,      // Task handle
        1                 // Pinned core
    );

    event_t ev = {.event_type = NO_EVENT,
                  .data = 0};
    BaseType_t q_return;

    for (;;) {
        q_return = xQueueReceive(q_event, &ev, portMAX_DELAY);

        if (q_return == pdTRUE) {
            switch (ev.event_type) {
                case EventType::SERVO_POS_CHANGED:
                    xQueueSend(q_task_servo, &ev, 0);
                    break;
                case EventType::SPOTIFY_UPDATED:
                    xQueueSend(q_task_display, &ev, 0);
                    xQueueSend(q_task_buttons, &ev, 0);
                    break;
                case EventType::AUDIO_FRAME_DONE:
                    xQueueSend(q_task_display, &ev, 0);
                    break;
                case EventType::MODE_CHANGED:
                    xQueueSend(q_task_display, &ev, 0);
                    xQueueSend(q_task_audio, &ev, 0);
                    break;
                default:
                    break;
            }
        }
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

    ModeSequence mode_seq;

    // Initialise the xLastWakeTime variable with the current time.
    xLastWakeTime = xTaskGetTickCount();
    event_t ev;

    for (;;) {
        // Check for the current mode
        q_return = xQueueReceive(q_task_display, )
            q_return = xQueueReceive(q_spotify, &sp_data, 0);  // get the spotify update if there is one
        if (q_return == pdTRUE) {
            if (sp_data.is_active) {
                percent_complete = sp_data.track_progress * 100;
                // Serial.println("Playing..." + String(percent_complete) + "%");
            }
        }

        q_return = xQueueReceive(q_mode, &mode_seq, 0);  // get a new mode if one is available
        // Display based on the mode
        switch (mode_seq.mode().id()) {
            case MODE_MAIN_ART: {
                // Display art and current elapsed regardless of if we have new data from the queue
                if (sp_data.album_art.loaded && sp_data.is_active) {
                    xSemaphoreTake(mutex_leds, portMAX_DELAY);

                    display_full_art(0, 0);
                    switch (mode_seq.submode().id()) {
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

                    int new_pos = SERVO_POS_ART;
                    xQueueSend(q_servo, &new_pos, 0);  // move after we have displayed
                } else {                               // no art, go blank
                    int new_pos = SERVO_POS_NOISE;
                    xQueueSend(q_servo, &new_pos, 0);  // move before we display
                    FastLED.clear();
                    show_leds();
                }
                break;
            }
            case MODE_MAIN_AUDIO: {
                // int new_pos = SERVO_NOISE_POS;  // this moves to the audio function
                // xQueueSend(q_servo, &new_pos, 0);

                uint8_t done;
                xQueueReceive(q_audio_done, &done, portMAX_DELAY);
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

    ModeSequence mode_seq;

    CRGB last_leds[NUM_LEDS] = {0};  // capture the last led state before transitioning to a new mode;
    BaseType_t q_return;
    int blend_counter = 0;

    // calculate the number of cycles to take to fully blend based on servo move time
    // TODO: is this correct? since this task is not operating at FPS speed
    int max_blend_count = int((SERVO_CYCLE_TIME_MS / 1000.0 * (SERVO_POS_ART - SERVO_POS_NOISE)) * FPS * 1.25);

    xQueueReceive(q_mode, &mode_seq, portMAX_DELAY);  // at the start, wait until we get a mode indication

    // Initialise the xLastWakeTime variable with the current time.
    xLastWakeTime = xTaskGetTickCount();

    int last_audio_mode = -1;
    for (;;) {
        if (ap.is_active()) {                                // don't run the loop if AP failed to init
            q_return = xQueueReceive(q_mode, &mode_seq, 0);  // check if there is a new mode, if not, we just use the last mode

            if (mode_seq.mode().id() == MODE_MAIN_AUDIO) {
                int audio_mode = mode_seq.submode().id();

                int new_pos = mode_seq.submode().get_servo_pos();
                xQueueSend(q_servo, &new_pos, 0);  // move the servo

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
                uint8_t done;
                xQueueSend(q_audio_done, &done, 0);
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

// Runs a loop that checks the spotify connection
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
            if (sp_data.album_art.changed) {
                decode_art(sp_data.album_art.data, sp_data.album_art.num_bytes);
            }
            xQueueSend(q_spotify, &sp_data, 0);  // set timeout to zero so loop will continue until display is updated
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

    ButtonFSM button1_fsm = ButtonFSM(PIN_BUTTON_MODE);
    ButtonFSM button2_fsm = ButtonFSM(PIN_BUTTON2_MODE);

    ButtonFSM::button_fsm_state_t button1_state;
    ButtonFSM::button_fsm_state_t button2_state;

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

    BaseType_t q_return;

    Spotify::public_data_t sp_data;

    for (;;) {
        // Check for power off
        if (digitalRead(PIN_POWER_SWITCH) == HIGH) {
            int new_pos = SERVO_POS_NOISE;
            xQueueSend(q_servo, &new_pos, 0);

            xSemaphoreTake(mutex_leds, portMAX_DELAY);

            vTaskDelay(((SERVO_POS_ART - SERVO_POS_NOISE) * SERVO_CYCLE_TIME_MS) / portTICK_RATE_MS);  // wait for servo move
            print("Switch is high, going to sleep\n");
            FastLED.clear(true);
            esp_deep_sleep_start();
        }
        button1_fsm.advance();
        button1_state = button1_fsm.get_state();

        button2_fsm.advance();
        button2_state = button2_fsm.get_state();

        if (button1_state == ButtonFSM::HOLD_TRIGGERED) {  // change modes
            print("Hold button1\n");
        }
        if (button1_state == ButtonFSM::MOMENTARY_TRIGGERED) {  // move the servos
            main_modes.submode_next();

            print("Momentary button1, advancing submode\n");
        }
        if (button2_state == ButtonFSM::HOLD_TRIGGERED) {  // change modes
            print("Hold button2\n");
        }
        if (button2_state == ButtonFSM::MOMENTARY_TRIGGERED) {  // move the servos
            main_modes.next();                                  // advance the main mode
            print("Momentary button2, advancing main mode\n");
        }

        // check if mode timers have elapsed
        if (main_modes.mode().elapsed()) main_modes.next();
        if (main_modes.submode().elapsed()) main_modes.submode_next();

        q_return = xQueueReceive(q_spotify, &sp_data, 0);  // get the spotify update if there is one
        if (q_return == pdTRUE) {
            if (!sp_data.is_active || sp_data.album_art.changed) {
                main_modes.set(MODE_MAIN_ART);  // show art on track change
            }
        }

        // send new mode
        xQueueSend(q_mode, &main_modes, 0);

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
        q_return = xQueueReceive(q_servo, &target_pos, 0);  // check if there is a new value in the queue
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
