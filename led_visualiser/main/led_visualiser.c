/**
 * @file led_visualiser.c
 * @brief LED matrix animation engine for 16×16 WS2812B addressable LEDs
 * 
 * @author Abhigya
 * @date 2024-2026
 * 
 * This module provides control and animation support for a 256-pixel LED matrix
 * (16×16 grid) using WS2812B addressable RGB LEDs connected via SPI2 to an ESP32.
 * 
 * **Key Features:**
 * - SPI-based LED strip initialization and control via ESP-IDF
 * - 2D-to-1D coordinate mapping with serpentine (zigzag) wiring support
 * - Automatic bounds checking for pixel writes
 * - Framebuffer for effect rendering (pixel_t musicNotes_fb)
 * - Raindrop animation effect with motion blur
 * 
 * **Hardware Requirements:**
 * - ESP32-WROOM (or compatible)
 * - WS2812B RGB LEDs (256 total, 16×16 matrix)
 * - SPI2 interface, GPIO5 for clock signal
 * - External capacitors/resistors per WS2812B datasheet
 * 
 * **Dependencies:**
 * - ESP-IDF v5.0+ (led_strip, SPI master, FreeRTOS)
 * - espressif/led_strip component
 * 
 * **Quick Example:**
 * ```c
 * void app_main(void) {
 *     led_init();        // Initialize SPI and LED strip (call once at boot)
 *     rain_effect();     // Run blocking rain animation
 * }
 * ```
 * 
 * **Architecture Overview:**
 * - **LED Hardware Layer**: led_init(), matrix_set_pixel(), matrix_clear()
 * - **Coordinate Transform**: xy_to_index() (handles 2D↔1D with serpentine wiring)
 * - **Framebuffer**: musicNotes_fb[256] stores pixel RGB values
 * - **Animation**: rain_init(), rain_update(), rain_draw(), rain_effect()
 * 
 * **Thread Safety:** ⚠️ **NOT thread-safe**. All functions assume single-threaded
 * execution. Do not call from multiple FreeRTOS tasks without external synchronization.
 * Use musicNotesQueue for thread-safe animation control (future enhancement).
 * 
 * **Future Improvements:**
 * - Move rain_effect() into a dedicated FreeRTOS task for non-blocking operation
 * - Integrate audio input (sample queue → animation parameters)
 * - Implement additional effects (color gradients, pulse, frequency response)
 * - Add brightness control and gamma correction
 * 
 * @see diagram/architecture.md (System architecture and module relationships)
 * @see https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32/api-reference/peripherals/spi_master.html
 * @see https://github.com/espressif/led_strip
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_err.h"
#include "esp_log.h"

/* ====================================================================
 *                        CONFIGURATION CONSTANTS
 * ==================================================================== */

/** @defgroup matrix_config LED Matrix Configuration
 * @{ */

#define MATRIX_WIDTH 16     /**< Number of LED columns in the matrix [0-15 valid x coords] */
#define MATRIX_HEIGHT 16    /**< Number of LED rows in the matrix [0-15 valid y coords] */
#define LED_PIN 5           /**< GPIO pin number for SPI clock signal to LED strip */
#define NUM_LEDS 256        /**< Total number of addressable LEDs (MATRIX_WIDTH × HEIGHT) */
#define MAX_DROPS 20        /**< Maximum concurrent raindrops in animation effect */

/** @} */

/* ====================================================================
 *                        GLOBAL STATE & HANDLES
 * ==================================================================== */

/** @brief SPI device handle for the LED strip.
 *  Initialized by led_init() and used by all downstream functions.
 *  @note Opaque handle managed by ESP-IDF led_strip component.
 */
led_strip_handle_t led_strip;

/** @brief Logging tag for ESP_LOG macros.
 *  Used to filter console output by module name.
 */
static const char *TAG = "matrix";

/* ====================================================================
 *                        DATA STRUCTURES
 * ==================================================================== */

/**
 * @struct raindrop_t
 * @brief Represents a single animated raindrop on the LED matrix.
 * 
 * Tracks position and motion state of one falling raindrop in the
 * rain_effect() animation. Position is updated each frame; LED rendering
 * is handled by rain_draw().
 */
typedef struct
{
    int x;  /**< Column position [0–15]; x=0 is left, x=15 is right */
    int y;  /**< Row position [0–15]; y=0 is top, y=15 is bottom */
} raindrop_t;

/**
 * @struct pixel_t
 * @brief RGB color value for a single addressable LED.
 * 
 * Represents a single pixel in the LED matrix framebuffer (musicNotes_fb).
 * Each channel is 8-bit brightness (0–255). Used for storing animation
 * state before flushing to physical LEDs via led_strip_refresh().
 */
typedef struct
{
    uint8_t red;    /**< Red channel brightness [0–255] */
    uint8_t green;  /**< Green channel brightness [0–255] */
    uint8_t blue;   /**< Blue channel brightness [0–255] */
} pixel_t;

/* ====================================================================
 *                        ANIMATION STATE
 * ==================================================================== */

/** @brief Array of active raindrops in the animation.
 * 
 *  Initialized by rain_init() and updated each frame by rain_update().
 *  @invariant All x values ∈ [0, MATRIX_WIDTH-1], y values ∈ [0, MATRIX_HEIGHT-1]
 *  @see rain_init(), rain_update()
 */
raindrop_t drops[MAX_DROPS];

/** @brief Pixel framebuffer for music visualization effects.
 * 
 *  Stores RGB color values for each LED in the 256-pixel matrix.
 *  Used as an off-screen buffer before flushing to physical LEDs.
 *  Enables multi-effect rendering without directly calling led_strip_set_pixel().
 *  
 *  Index mapping: Use xy_to_index(x, y) to convert 2D coords to 1D array index.
 *  
 *  Example usage:
 *  @code
 *  musicNotes_fb[xy_to_index(5, 3)].red = 255;
 *  musicNotes_fb[xy_to_index(5, 3)].green = 100;
 *  @endcode
 *  
 *  @size 256 pixels (MATRIX_WIDTH × MATRIX_HEIGHT)
 *  @see matrix_set_pixel(), fb_clear()
 */
pixel_t musicNotes_fb[NUM_LEDS];

/** @brief FreeRTOS queue for thread-safe music note visualization commands.
 * 
 *  Enables decoupling audio processing tasks from LED rendering.
 *  Currently allocated but not actively used; available for future audio integration.
 *  
 *  @see producer_task, consumer_task (in led_visualiser_test.c)
 *  @todo Implement queue-based animation control for concurrent audio processing
 */
QueueHandle_t musicNotesQueue;

/* ====================================================================
 *                    LED HARDWARE CONTROL LAYER
 * ==================================================================== */

/**
 * @brief Initialize SPI interface and LED strip hardware.
 * 
 * Configures ESP32 SPI2 with DMA enabled, establishes communication with
 * the WS2812B LED strip controller, and sets all LEDs to off (black).
 * Must be called once at startup before any other matrix_* or led_* functions.
 * 
 * **Configuration:**
 * - SPI Bus: SPI2_HOST
 * - GPIO Pin: LED_PIN (GPIO5) for clock signal
 * - Max LEDs: NUM_LEDS (256)
 * - Features: DMA enabled for efficient transfers
 * 
 * **Error Handling:**
 * Calls ESP_ERROR_CHECK() macro; will abort if LED strip initialization fails.
 * (Production code should use error-aware variant instead.)
 * 
 * @return void (ESP_ERROR_CHECK will halt on failure)
 * 
 * @note **Call once at system startup.** Subsequent calls may cause undefined behavior.
 * @note **Not thread-safe.** Do not call from multiple FreeRTOS tasks.
 * 
 * @example
 * @code
 * void app_main(void) {
 *     led_init();           // Configure SPI + LED strip
 *     rain_effect();        // Start animation
 * }
 * @endcode
 * 
 * @see matrix_set_pixel(), matrix_clear()
 */
void led_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_PIN,
        .max_leds = NUM_LEDS
    };

    led_strip_spi_config_t spi_config = {
        .spi_bus = SPI2_HOST,
        .flags.with_dma = true
    };

    ESP_ERROR_CHECK(
    led_strip_new_spi_device(&strip_config, &spi_config, &led_strip)
    );

    led_strip_clear(led_strip);
    led_strip_refresh(led_strip);
}

/**
 * @brief Convert 2D matrix coordinates to 1D LED array index.
 * 
 * Maps a 2D position (x, y) on the 16×16 matrix to a single index (0–255)
 * suitable for the linear LED array. Handles serpentine (zigzag) row wiring:
 * - Even rows (y=0,2,4...): left-to-right indexing (normal)
 * - Odd rows (y=1,3,5...): right-to-left indexing (reversed, "zigzag")
 * 
 * This pattern is common in physical LED matrix products to simplify wiring;
 * the serpentine layout reduces PCB trace crossings.
 * 
 * **Algorithm:**
 * ```
 * if (y is even):
 *     index = y * WIDTH + x                    // Normal row
 * else (y is odd):
 *     index = y * WIDTH + (WIDTH - 1 - x)     // Reversed row
 * ```
 * 
 * @param x Column position [0–15]
 * @param y Row position [0–15]
 * 
 * @return LED array index [0–255]
 * 
 * @note **No bounds checking.** Caller must ensure x ∈ [0,15], y ∈ [0,15].
 *       Out-of-range values produce undefined behavior.
 * @note This is a pure function (no side effects); safe to call repeatedly.
 * 
 * @example
 * @code
 * // Set pixel at column 5, row 7
 * int idx = xy_to_index(5, 7);  // Returns 7*16 + (16-1-5) = 122 (odd row, reversed)
 * led_strip_set_pixel(led_strip, idx, 255, 100, 50);
 * @endcode
 * 
 * @see matrix_set_pixel() (higher-level wrapper with bounds checking)
 */
int xy_to_index(int x, int y)
{
    if (y % 2 == 0)
        return y * MATRIX_WIDTH + x;  // Even rows: left→right
    else
        return y * MATRIX_WIDTH + (MATRIX_WIDTH - 1 - x);  // Odd rows: right←left (serpentine)
}


/**
 * @brief Set pixel color in the framebuffer using 2D coordinates.
 * 
 * Writes an RGB color value to a single LED in the matrix, storing it in
 * the musicNotes_fb framebuffer. Coordinates are automatically validated;
 * out-of-bounds writes are silently ignored (no error flag).
 * 
 * **Workflow:**
 * 1. Validate x ∈ [0,15] and y ∈ [0,15]
 * 2. Convert (x,y) → 1D index using xy_to_index() with serpentine wiring
 * 3. Store RGB values in musicNotes_fb[index]
 * 4. (Does NOT directly update physical LEDs; call led_strip_refresh() to flush)
 * 
 * * **Performance:** O(1) time, no temporary allocations.
 * 
 * @param x Column [0–15]
 * @param y Row [0–15]
 * @param r Red channel [0–255]
 * @param g Green channel [0–255]
 * @param b Blue channel [0–255]
 * 
 * @return void (silently returns if out-of-bounds)
 * 
 * @note Safe with out-of-bounds coordinates (gracefully ignored).
 * @note **Does NOT directly update physical LEDs.** Must call led_strip_refresh()
 *       after all pixel writes to flush framebuffer to hardware.
 * @note Not thread-safe; do not call concurrently from multiple tasks.
 * 
 * @example
 * @code
 * // Set pixel at (5, 7) to bright cyan
 * matrix_set_pixel(5, 7, 0, 255, 255);
 * 
 * // Out-of-bounds write (safely ignored, no error)
 * matrix_set_pixel(20, 20, 255, 0, 0);  // x,y exceed MATRIX_WIDTH/HEIGHT
 * 
 * // Flush to physical LEDs
 * led_strip_refresh(led_strip);
 * @endcode
 * 
 * @see xy_to_index() (coordinate conversion with serpentine wiring)
 * @see matrixy_clear()
 * @see led_strip_refresh() (flush framebuffer to hardware)
 */
void matrix_set_pixel(int x, int y, int r, int g, int b)
{
    // Bounds checking: silently ignore out-of-range coordinates
    if (x < 0 || x >= MATRIX_WIDTH || y < 0 || y >= MATRIX_HEIGHT)
        return;

    int index = xy_to_index(x, y);

    // Store in framebuffer (does not update physical LEDs until led_strip_refresh())
    musicNotes_fb[index].red = r;
    musicNotes_fb[index].green = g;
    musicNotes_fb[index].blue = b;
}

/**
 * @brief Clear all LEDs in the physical strip to off (black).
 * 
 * Immediately turns off all 256 LEDs by issuing a clear command to the
 * SPI LED strip and flushing the change to hardware.
 * 
 * @note This clears the **physical LEDs**, not the framebuffer.
 *       To clear the framebuffer without updating hardware, use fb_clear().
 * @note This is a direct hardware operation and may flicker if called frequently.
 * 
 * @return void
 * 
 * @see fb_clear() (clear framebuffer without updating hardware)
 * @see led_strip_refresh() (flush framebuffer to hardware)
 */
void matrix_clear(void)
{
    led_strip_clear(led_strip);
}

/**
 * @brief Clear the framebuffer to all black (0,0,0 RGB).
 * 
 * Resets all pixel values in musicNotes_fb to off. Does NOT directly affect
 * physical LEDs; must call led_strip_refresh() after to flush to hardware.
 * 
 * **Use Cases:**
 * - Pre-frame clearing in animation loops (calls this once per frame, then draws)
 * - Safe framebuffer reset between effects without flickering
 * 
 * @return void
 * 
 * @note This clears the **framebuffer only**, not physical LEDs.
 *       Use matrix_clear() for immediate hardware update; use fb_clear()
 *       when implementing double-buffering patterns.
 * @note O(n) time where n = NUM_LEDS (256 assignments).
 * 
 * @see matrix_clear() (clear physical LEDs directly)
 * @see led_strip_refresh() (flush framebuffer to hardware)
 * 
 * @example
 * @code
 * // Typical animation frame pattern:
 * fb_clear();                        // Reset framebuffer
 * matrix_set_pixel(5, 5, 255, 0, 0);  // Draw to framebuffer
 * matrix_set_pixel(5, 6, 0, 255, 0);  // More drawing
 * led_strip_refresh(led_strip);       // Flush to hardware (single SPI transaction)
 * @endcode
 */
void fb_clear(void)
{
    for(int i = 0; i < NUM_LEDS; i++)
    {
        musicNotes_fb[i].red = 0;
        musicNotes_fb[i].green = 0;
        musicNotes_fb[i].blue = 0;
    }
}
/* ====================================================================
 *                      RAIN ANIMATION EFFECT
 * ==================================================================== */

/**
 * @brief Initialize raindrop positions for the rain animation.
 * 
 * Creates MAX_DROPS raindrops at random horizontal positions along the top
 * row (y=0). Each drop's initial x coordinate is randomly chosen; y is always
 * initialized to 0 (top of matrix).
 * 
 * Called once at the start of rain_effect() to set up initial state.
 * 
 * @return void
 * 
 * **Initialization Pattern:**
 * ```
 * For each drop i in [0, MAX_DROPS):
 *   drops[i].x = random_int % MATRIX_WIDTH  // Random column
 *   drops[i].y = 0                          // Always start at top
 * ```
 * 
 * @note Uses standard rand() (non-cryptographic). For deterministic behavior,
 *       seed with srand() before calling rain_effect().
 * @note Must be called before rain_update() and rain_draw().
 * 
 * @see rain_update() (move drops downward each frame)
 * @see rain_draw() (render drops to LED matrix)
 * @see rain_effect() (main animation loop)
 */
void rain_init(void)
{
    for(int i = 0; i < MAX_DROPS; i++)
    {
        drops[i].x = rand() % MATRIX_WIDTH;
        drops[i].y = 0;  // All drops start at top of matrix
    }
}

/**
 * @brief Update raindrop positions (physics step).
 * 
 * Moves each raindrop downward by 1 pixel per frame. When a drop reaches
 * the bottom (y >= MATRIX_HEIGHT), it respawns at the top with a new random x.
 * Additionally, each drop has a ~10% per-frame chance of spontaneous respawn
 * to add organic variation and prevent drops from forming regular patterns.
 * 
 * **Motion Logic:**
 * 1. Increment y for each drop
 * 2. If y exceeds matrix height: reset y=0, pick new random x
 * 3. Else if rand() yields ~10% trigger: also respawn
 * 
 * Called every animation frame in the rain_effect() loop.
 * 
 * @return void
 * 
 * @note **Modifies global drops[] array** (side effect).
 * @note **Not thread-safe.** Do not call concurrently with rain_draw().
 * @note O(n) time where n = MAX_DROPS (20).
 * 
 * @see rain_init() (initialize drops before calling this)
 * @see rain_draw() (render updated drops after calling this)
 * @see rain_effect() (calls in main animation loop: update → draw → delay)
 * 
 * @example
 * @code
 * // Typical animation frame:
 * rain_update();              // Move all drops down, handle respawn timing
 * rain_draw();                // Render updated positions to LED matrix
 * vTaskDelay(pdMS_TO_TICKS(100));  // 100ms frame delay (~10 FPS)
 * @endcode
 */
void rain_update(void)
{
    for(int i = 0; i < MAX_DROPS; i++)
    {
        drops[i].y++;  // Move downward

        // Hard reset when drop falls off bottom of screen
        if(drops[i].y >= MATRIX_HEIGHT)
        {
            drops[i].y = 0;
            drops[i].x = rand() % MATRIX_WIDTH;
        }
        // Soft reset: ~10% per-frame spontaneous respawn for variation
        // (prevents drops from falling in synchronized patterns)
        else if(rand() % 10 == 0)
        {
            drops[i].y = 0;
            drops[i].x = rand() % MATRIX_WIDTH;
        }
    }
}

/**
 * @brief Render all raindrops to the LED matrix (graphics step).
 * 
 * Clears the physical LED strip and redraws each raindrop as a 3-pixel
 * vertical "comet" trail using motion blur colors:
 * - **Head** (y): Bright blue (10,10,50) – current drop position
 * - **Tail 1** (y−1): Medium blue (5,5,20) – 1 pixel above, fading
 * - **Tail 2** (y−2): Dim blue (2,2,10) – 2 pixels above, near-invisible
 * 
 * The multi-pixel trail creates a smooth motion-blur effect without requiring
 * a full framebuffer or complex rendering logic.
 * 
 * **Rendering Pipeline:**
 * 1. Clear all physical LEDs (matrix_clear)
 * 2. For each active drop:
 *    - Draw bright head at current (x, y)
 *    - Draw fading tail 1 pixel above (if in bounds)
 *    - Draw fading tail 2 pixels above (if in bounds)
 * 3. Flush to physical hardware (led_strip_refresh)
 * 
 * Called every animation frame after rain_update().
 * 
 * @return void
 * 
 * @note **Direct hardware operation.** Calls matrix_clear() and led_strip_refresh(),
 *       which may be slow (~microseconds per operation). Avoid calling excessively.
 * @note **Reads global drops[] array.** Not thread-safe with concurrent 
 *       rain_update() calls.
 * @note O(DROP_COUNT) time (MAX_DROPS = 20).
 * 
 * @see rain_update() (call before this to update drop positions)
 * @see rain_effect() (main loop)
 * 
 * @example
 * @code
 * // Standard animation frame in rain_effect():
 * rain_update();                         // Update physics
 * rain_draw();                           // Render
 * vTaskDelay(pdMS_TO_TICKS(100));       // Wait 100ms
 * // Loop back to rain_update()...
 * @endcode
 */
void rain_draw(void)
{
    matrix_clear();  // Clear all physical LEDs

    for(int i = 0; i < MAX_DROPS; i++)
    {
        int x = drops[i].x;
        int y = drops[i].y;

        // Pixel 0 (head): Bright blue at current position
        matrix_set_pixel(x, y, 10, 10, 50);

        // Pixel 1 (tail fade 1): Dimmer blue 1 pixel above
        if(y - 1 >= 0)
            matrix_set_pixel(x, y - 1, 5, 5, 20);

        // Pixel 2 (tail fade 2): Dimmest blue 2 pixels above
        if(y - 2 >= 0)
            matrix_set_pixel(x, y - 2, 2, 2, 10);
    }

    led_strip_refresh(led_strip);  // Flush framebuffer to physical LEDs
}

/**
 * @brief Main animation loop for the rain effect.
 * 
 * Runs an infinite blocking loop that repeatedly:
 * 1. Updates raindrop positions (physics)
 * 2. Renders raindrops to the LED matrix (graphics)
 * 3. Delays 100ms per frame (~10 FPS)
 * 
 * This is a **blocking function** and will not return once called; all system
 * control is consumed by the animation loop. To run concurrent tasks (e.g., audio
 * processing), refactor this into a dedicated FreeRTOS task.
 * 
 * **Typical Usage:**
 * ```c
 * void app_main(void) {
 *     led_init();        // Initialize hardware
 *     rain_effect();     // Run animation (blocking, never returns)
 * }
 * ```
 * 
 * @return void (never returns; infinite loop)
 * 
 * @note **BLOCKING FUNCTION** — consumes entire system once called. Not suitable
 *       for applications requiring concurrent operations (audio input, networking, etc.).
 *       For non-blocking animation, create a dedicated FreeRTOS task calling
 *       rain_update() and rain_draw() with frequency control via task scheduler.
 * @note **Not thread-safe** — do not call from multiple FreeRTOS tasks.
 * @note Calls rain_init() once, then updates/draws in ~100ms intervals forever.
 * 
 * @see rain_init() (called once at start)
 * @see rain_update() (called every 100ms)
 * @see rain_draw() (called every 100ms)
 * 
 * **Future Enhancement:**
 * Refactor into a non-blocking FreeRTOS task:
 * ```c
 * void rain_animation_task(void *param) {
 *     rain_init();
 *     while(1) {
 *         rain_update();
 *         rain_draw();
 *         vTaskDelay(pdMS_TO_TICKS(100));
 *     }
 * }
 * 
 * void app_main(void) {
 *     led_init();
 *     xTaskCreate(rain_animation_task, "rain", 4096, NULL, 1, NULL);
 *     // Now other tasks can run concurrently (audio input, network, etc.)
 * }
 * ```
 */
void rain_effect(void)
{
    rain_init();

    while(1)
    {
        rain_update();
        rain_draw();

        // 100ms frame delay (~10 FPS)
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ====================================================================
 *                          APPLICATION ENTRY
 * ==================================================================== */

/**
 * @brief FreeRTOS application entry point.
 * 
 * Called by the FreeRTOS scheduler after system boot. Initializes hardware,
 * then starts the rain animation effect.
 * 
 * @return void (can return to reboot ESP32, or run infinite loop)
 * 
 * @note This is the recommended entry point for ESP32/FreeRTOS projects.
 *       Automatically called at boot; do not call manually.
 * 
 * @see led_init() (hardware initialization)
 * @see rain_effect() (main animation loop)
 */
void app_main(void)
{
    // Initialize LED strip hardware (SPI + WS2812B controller)
    led_init();

    // Run blocking rain animation forever
    rain_effect();
}
