/**********************************************************************************************
*
*   rcore_web - Functions to manage window, graphics device and inputs 
*
*   PLATFORM: WEB
*       - HTML5 (WebAssembly)
*
*   LIMITATIONS:
*       - Limitation 01
*       - Limitation 02
*
*   POSSIBLE IMPROVEMENTS:
*       - Replace glfw3 dependency by direct browser API calls (same as library_glfw3.js)
*
*   ADDITIONAL NOTES:
*       - TRACELOG() function is located in raylib [utils] module
*
*   CONFIGURATION:
*       #define RCORE_WEB_CUSTOM_FLAG
*           Custom flag for rcore on PLATFORM_WEB -not used-
*
*   DEPENDENCIES:
*       emscripten - Allow interaction between browser API and C
*       gestures - Gestures system for touch-ready devices (or simulated from mouse inputs)
*
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2013-2023 Ramon Santamaria (@raysan5) and contributors
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/

#include "rcore.h"

#define GLFW_INCLUDE_ES2 // GLFW3: Enable OpenGL ES 2.0 (translated to WebGL)
// #define GLFW_INCLUDE_ES3            // GLFW3: Enable OpenGL ES 3.0 (transalted to WebGL2?)
#include "GLFW/glfw3.h" // GLFW3: Windows, OpenGL context and Input management
#include <sys/time.h>   // Required for: timespec, nanosleep(), select() - POSIX

#include <emscripten/emscripten.h> // Emscripten functionality for C
#include <emscripten/html5.h>      // Emscripten HTML5 library

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
//...

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
extern CoreData CORE;           // Global CORE state context

//----------------------------------------------------------------------------------
// Module Internal Functions Declaration
//----------------------------------------------------------------------------------
static bool InitGraphicsDevice(int width, int height); // Initialize graphics device

// Error callback event
static void ErrorCallback(int error, const char *description);                             // GLFW3 Error Callback, runs on GLFW3 error

// Window callbacks events
static void WindowSizeCallback(GLFWwindow *window, int width, int height);         // GLFW3 WindowSize Callback, runs when window is resized
static void WindowIconifyCallback(GLFWwindow *window, int iconified);              // GLFW3 WindowIconify Callback, runs when window is minimized/restored
static void WindowMaximizeCallback(GLFWwindow *window, int maximized);             // GLFW3 Window Maximize Callback, runs when window is maximized
static void WindowFocusCallback(GLFWwindow *window, int focused);                  // GLFW3 WindowFocus Callback, runs when window get/lose focus
static void WindowDropCallback(GLFWwindow *window, int count, const char **paths); // GLFW3 Window Drop Callback, runs when drop files into window

// Input callbacks events
static void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods); // GLFW3 Keyboard Callback, runs on key pressed
static void CharCallback(GLFWwindow *window, unsigned int key);                           // GLFW3 Char Key Callback, runs on key pressed (get char value)
static void MouseButtonCallback(GLFWwindow *window, int button, int action, int mods);    // GLFW3 Mouse Button Callback, runs on mouse button pressed
static void MouseCursorPosCallback(GLFWwindow *window, double x, double y);               // GLFW3 Cursor Position Callback, runs on mouse move
static void MouseScrollCallback(GLFWwindow *window, double xoffset, double yoffset);      // GLFW3 Srolling Callback, runs on mouse wheel
static void CursorEnterCallback(GLFWwindow *window, int enter);                           // GLFW3 Cursor Enter Callback, cursor enters client area

// Emscripten callback events
static EM_BOOL EmscriptenFullscreenChangeCallback(int eventType, const EmscriptenFullscreenChangeEvent *event, void *userData);
static EM_BOOL EmscriptenWindowResizedCallback(int eventType, const EmscriptenUiEvent *event, void *userData);
static EM_BOOL EmscriptenResizeCallback(int eventType, const EmscriptenUiEvent *event, void *userData);

static EM_BOOL EmscriptenMouseCallback(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData);
static EM_BOOL EmscriptenTouchCallback(int eventType, const EmscriptenTouchEvent *touchEvent, void *userData);
static EM_BOOL EmscriptenGamepadCallback(int eventType, const EmscriptenGamepadEvent *gamepadEvent, void *userData);

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
// NOTE: Functions declaration is provided by raylib.h

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------

// Initialize window and OpenGL context
// NOTE: data parameter could be used to pass any kind of required data to the initialization
void InitWindow(int width, int height, const char *title)
{
    TRACELOG(LOG_INFO, "Initializing raylib %s", RAYLIB_VERSION);

    TRACELOG(LOG_INFO, "Supported raylib modules:");
    TRACELOG(LOG_INFO, "    > rcore:..... loaded (mandatory)");
    TRACELOG(LOG_INFO, "    > rlgl:...... loaded (mandatory)");
#if defined(SUPPORT_MODULE_RSHAPES)
    TRACELOG(LOG_INFO, "    > rshapes:... loaded (optional)");
#else
    TRACELOG(LOG_INFO, "    > rshapes:... not loaded (optional)");
#endif
#if defined(SUPPORT_MODULE_RTEXTURES)
    TRACELOG(LOG_INFO, "    > rtextures:. loaded (optional)");
#else
    TRACELOG(LOG_INFO, "    > rtextures:. not loaded (optional)");
#endif
#if defined(SUPPORT_MODULE_RTEXT)
    TRACELOG(LOG_INFO, "    > rtext:..... loaded (optional)");
#else
    TRACELOG(LOG_INFO, "    > rtext:..... not loaded (optional)");
#endif
#if defined(SUPPORT_MODULE_RMODELS)
    TRACELOG(LOG_INFO, "    > rmodels:... loaded (optional)");
#else
    TRACELOG(LOG_INFO, "    > rmodels:... not loaded (optional)");
#endif
#if defined(SUPPORT_MODULE_RAUDIO)
    TRACELOG(LOG_INFO, "    > raudio:.... loaded (optional)");
#else
    TRACELOG(LOG_INFO, "    > raudio:.... not loaded (optional)");
#endif

    // NOTE: Keep internal pointer to input title string (no copy)
    if ((title != NULL) && (title[0] != 0)) CORE.Window.title = title;

    // Initialize global input state
    memset(&CORE.Input, 0, sizeof(CORE.Input));
    CORE.Input.Keyboard.exitKey = KEY_ESCAPE;
    CORE.Input.Mouse.scale = (Vector2){1.0f, 1.0f};
    CORE.Input.Mouse.cursor = MOUSE_CURSOR_ARROW;
    CORE.Input.Gamepad.lastButtonPressed = 0; // GAMEPAD_BUTTON_UNKNOWN
    CORE.Window.eventWaiting = false;

    // Initialize graphics device (display device and OpenGL context)
    // NOTE: returns true if window and graphic device has been initialized successfully
    CORE.Window.ready = InitGraphicsDevice(width, height);

    // If graphic device is no properly initialized, we end program
    if (!CORE.Window.ready)
    {
        TRACELOG(LOG_FATAL, "Failed to initialize Graphic Device");
        return;
    }
    else SetWindowPosition(GetMonitorWidth(GetCurrentMonitor())/2 - CORE.Window.screen.width/2, GetMonitorHeight(GetCurrentMonitor())/2 - CORE.Window.screen.height/2);

    // Initialize hi-res timer
    InitTimer();

    // Initialize random seed
    srand((unsigned int)time(NULL));

    // Initialize base path for storage
    CORE.Storage.basePath = GetWorkingDirectory();

#if defined(SUPPORT_MODULE_RTEXT) && defined(SUPPORT_DEFAULT_FONT)
    // Load default font
    // WARNING: External function: Module required: rtext
    LoadFontDefault();
#if defined(SUPPORT_MODULE_RSHAPES)
    // Set font white rectangle for shapes drawing, so shapes and text can be batched together
    // WARNING: rshapes module is required, if not available, default internal white rectangle is used
    Rectangle rec = GetFontDefault().recs[95];
    if (CORE.Window.flags & FLAG_MSAA_4X_HINT)
    {
        // NOTE: We try to maxime rec padding to avoid pixel bleeding on MSAA filtering
        SetShapesTexture(GetFontDefault().texture, (Rectangle){rec.x + 2, rec.y + 2, 1, 1});
    }
    else
    {
        // NOTE: We set up a 1px padding on char rectangle to avoid pixel bleeding
        SetShapesTexture(GetFontDefault().texture, (Rectangle){rec.x + 1, rec.y + 1, rec.width - 2, rec.height - 2});
    }
#endif
#else
#if defined(SUPPORT_MODULE_RSHAPES)
    // Set default texture and rectangle to be used for shapes drawing
    // NOTE: rlgl default texture is a 1x1 pixel UNCOMPRESSED_R8G8B8A8
    Texture2D texture = {rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    SetShapesTexture(texture, (Rectangle){0.0f, 0.0f, 1.0f, 1.0f}); // WARNING: Module required: rshapes
#endif
#endif
#if defined(SUPPORT_MODULE_RTEXT) && defined(SUPPORT_DEFAULT_FONT)
    if ((CORE.Window.flags & FLAG_WINDOW_HIGHDPI) > 0)
    {
        // Set default font texture filter for HighDPI (blurry)
        // RL_TEXTURE_FILTER_LINEAR - tex filter: BILINEAR, no mipmaps
        rlTextureParameters(GetFontDefault().texture.id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_LINEAR);
        rlTextureParameters(GetFontDefault().texture.id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
    }
#endif

    // Setup callback functions for the DOM events
    emscripten_set_fullscreenchange_callback("#canvas", NULL, 1, EmscriptenFullscreenChangeCallback);

    // WARNING: Below resize code was breaking fullscreen mode for sample games and examples, it needs review
    // Check fullscreen change events(note this is done on the window since most browsers don't support this on #canvas)
    // emscripten_set_fullscreenchange_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, 1, EmscriptenResizeCallback);
    // Check Resize event (note this is done on the window since most browsers don't support this on #canvas)
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, 1, EmscriptenResizeCallback);
    
    // Trigger this once to get initial window sizing
    EmscriptenResizeCallback(EMSCRIPTEN_EVENT_RESIZE, NULL, NULL);

    // Support keyboard events -> Not used, GLFW.JS takes care of that
    // emscripten_set_keypress_callback("#canvas", NULL, 1, EmscriptenKeyboardCallback);
    // emscripten_set_keydown_callback("#canvas", NULL, 1, EmscriptenKeyboardCallback);

    // Support mouse events
    emscripten_set_click_callback("#canvas", NULL, 1, EmscriptenMouseCallback);

    // Support touch events
    emscripten_set_touchstart_callback("#canvas", NULL, 1, EmscriptenTouchCallback);
    emscripten_set_touchend_callback("#canvas", NULL, 1, EmscriptenTouchCallback);
    emscripten_set_touchmove_callback("#canvas", NULL, 1, EmscriptenTouchCallback);
    emscripten_set_touchcancel_callback("#canvas", NULL, 1, EmscriptenTouchCallback);

    // Support gamepad events (not provided by GLFW3 on emscripten)
    emscripten_set_gamepadconnected_callback(NULL, 1, EmscriptenGamepadCallback);
    emscripten_set_gamepaddisconnected_callback(NULL, 1, EmscriptenGamepadCallback);

#if defined(SUPPORT_EVENTS_AUTOMATION)
    events = (AutomationEvent *)RL_CALLOC(MAX_CODE_AUTOMATION_EVENTS, sizeof(AutomationEvent));
    CORE.Time.frameCounter = 0;
#endif
}

// Close window and unload OpenGL context
void CloseWindow(void)
{
#if defined(SUPPORT_GIF_RECORDING)
    if (gifRecording)
    {
        MsfGifResult result = msf_gif_end(&gifState);
        msf_gif_free(result);
        gifRecording = false;
    }
#endif

#if defined(SUPPORT_MODULE_RTEXT) && defined(SUPPORT_DEFAULT_FONT)
    UnloadFontDefault(); // WARNING: Module required: rtext
#endif

    rlglClose(); // De-init rlgl

    glfwDestroyWindow(CORE.Window.handle);
    glfwTerminate();

#if defined(_WIN32) && defined(SUPPORT_WINMM_HIGHRES_TIMER) && !defined(SUPPORT_BUSY_WAIT_LOOP)
    timeEndPeriod(1); // Restore time period
#endif

#if defined(SUPPORT_EVENTS_AUTOMATION)
    RL_FREE(events);
#endif

    CORE.Window.ready = false;
    TRACELOG(LOG_INFO, "Window closed successfully");
}

// Check if application should close
bool WindowShouldClose(void)
{
    // Emterpreter-Async required to run sync code
    // https://github.com/emscripten-core/emscripten/wiki/Emterpreter#emterpreter-async-run-synchronous-code
    // By default, this function is never called on a web-ready raylib example because we encapsulate
    // frame code in a UpdateDrawFrame() function, to allow browser manage execution asynchronously
    // but now emscripten allows sync code to be executed in an interpreted way, using emterpreter!
    emscripten_sleep(16);
    return false;
}

// Check if window is currently hidden
bool IsWindowHidden(void)
{
    return false;
}

// Check if window has been minimized
bool IsWindowMinimized(void)
{
    return false;
}

// Check if window has been maximized
bool IsWindowMaximized(void)
{
    return false;
}

// Check if window has the focus
bool IsWindowFocused(void)
{
    return ((CORE.Window.flags & FLAG_WINDOW_UNFOCUSED) == 0);
}

// Check if window has been resizedLastFrame
bool IsWindowResized(void)
{
    return CORE.Window.resizedLastFrame;
}

// Toggle fullscreen mode
void ToggleFullscreen(void)
{
    /*
        EM_ASM
        (
            // This strategy works well while using raylib minimal web shell for emscripten,
            // it re-scales the canvas to fullscreen using monitor resolution, for tools this
            // is a good strategy but maybe games prefer to keep current canvas resolution and
            // display it in fullscreen, adjusting monitor resolution if possible
            if (document.fullscreenElement) document.exitFullscreen();
            else Module.requestFullscreen(true, true); //false, true);
        );
    */
    // EM_ASM(Module.requestFullscreen(false, false););
    /*
        if (!CORE.Window.fullscreen)
        {
            // Option 1: Request fullscreen for the canvas element
            // This option does not seem to work at all:
            // emscripten_request_pointerlock() and emscripten_request_fullscreen() are affected by web security,
            // the user must click once on the canvas to hide the pointer or transition to full screen
            //emscripten_request_fullscreen("#canvas", false);

            // Option 2: Request fullscreen for the canvas element with strategy
            // This option does not seem to work at all
            // Ref: https://github.com/emscripten-core/emscripten/issues/5124
            // EmscriptenFullscreenStrategy strategy = {
                // .scaleMode = EMSCRIPTEN_FULLSCREEN_SCALE_STRETCH, //EMSCRIPTEN_FULLSCREEN_SCALE_ASPECT,
                // .canvasResolutionScaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_STDDEF,
                // .filteringMode = EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT,
                // .canvasResizedCallback = EmscriptenWindowResizedCallback,
                // .canvasResizedCallbackUserData = NULL
            // };
            //emscripten_request_fullscreen_strategy("#canvas", EM_FALSE, &strategy);

            // Option 3: Request fullscreen for the canvas element with strategy
            // It works as expected but only inside the browser (client area)
            EmscriptenFullscreenStrategy strategy = {
                .scaleMode = EMSCRIPTEN_FULLSCREEN_SCALE_ASPECT,
                .canvasResolutionScaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_STDDEF,
                .filteringMode = EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT,
                .canvasResizedCallback = EmscriptenWindowResizedCallback,
                .canvasResizedCallbackUserData = NULL
            };
            emscripten_enter_soft_fullscreen("#canvas", &strategy);

            int width, height;
            emscripten_get_canvas_element_size("#canvas", &width, &height);
            TRACELOG(LOG_WARNING, "Emscripten: Enter fullscreen: Canvas size: %i x %i", width, height);

            CORE.Window.fullscreen = true;          // Toggle fullscreen flag
            CORE.Window.flags |= FLAG_FULLSCREEN_MODE;
        }
        else
        {
            //emscripten_exit_fullscreen();
            //emscripten_exit_soft_fullscreen();

            int width, height;
            emscripten_get_canvas_element_size("#canvas", &width, &height);
            TRACELOG(LOG_WARNING, "Emscripten: Exit fullscreen: Canvas size: %i x %i", width, height);

            CORE.Window.fullscreen = false;          // Toggle fullscreen flag
            CORE.Window.flags &= ~FLAG_FULLSCREEN_MODE;
        }
    */

    CORE.Window.fullscreen = !CORE.Window.fullscreen; // Toggle fullscreen flag
}

// Set window state: maximized, if resizable
void MaximizeWindow(void)
{
    TRACELOG(LOG_WARNING, "MaximizeWindow() not available on PLATFORM_WEB");
}

// Set window state: minimized
void MinimizeWindow(void)
{
    TRACELOG(LOG_WARNING, "MinimizeWindow() not available on PLATFORM_WEB");
}

// Set window state: not minimized/maximized
void RestoreWindow(void)
{
    TRACELOG(LOG_WARNING, "RestoreWindow() not available on PLATFORM_WEB");
}

// Toggle borderless windowed mode
void ToggleBorderlessWindowed(void)
{
    TRACELOG(LOG_WARNING, "ToggleBorderlessWindowed() not available on PLATFORM_WEB");
}

// Set window configuration state using flags
void SetWindowState(unsigned int flags)
{
    TRACELOG(LOG_WARNING, "SetWindowState() not available on PLATFORM_WEB");
}

// Clear window configuration state flags
void ClearWindowState(unsigned int flags)
{
    TRACELOG(LOG_WARNING, "ClearWindowState() not available on PLATFORM_WEB");
}

// Set icon for window
void SetWindowIcon(Image image)
{
    TRACELOG(LOG_WARNING, "SetWindowIcon() not available on PLATFORM_WEB");
}

// Set icon for window, multiple images
void SetWindowIcons(Image *images, int count)
{
    TRACELOG(LOG_WARNING, "SetWindowIcons() not available on PLATFORM_WEB");
}

// Set title for window
void SetWindowTitle(const char *title)
{
    CORE.Window.title = title;
    emscripten_set_window_title(title);
}

// Set window position on screen (windowed mode)
void SetWindowPosition(int x, int y)
{
    TRACELOG(LOG_WARNING, "SetWindowPosition() not available on PLATFORM_WEB");
}

// Set monitor for the current window
void SetWindowMonitor(int monitor)
{
    TRACELOG(LOG_WARNING, "SetWindowMonitor() not available on PLATFORM_WEB");
}

// Set window minimum dimensions (FLAG_WINDOW_RESIZABLE)
void SetWindowMinSize(int width, int height)
{
    CORE.Window.screenMin.width = width;
    CORE.Window.screenMin.height = height;
    
    // Trigger the resize event once to update the window minimum width and height
    if ((CORE.Window.flags & FLAG_WINDOW_RESIZABLE) != 0) EmscriptenResizeCallback(EMSCRIPTEN_EVENT_RESIZE, NULL, NULL);
}

// Set window maximum dimensions (FLAG_WINDOW_RESIZABLE)
void SetWindowMaxSize(int width, int height)
{
    CORE.Window.screenMax.width = width;
    CORE.Window.screenMax.height = height;

    // Trigger the resize event once to update the window maximum width and height
    if ((CORE.Window.flags & FLAG_WINDOW_RESIZABLE) != 0) EmscriptenResizeCallback(EMSCRIPTEN_EVENT_RESIZE, NULL, NULL);
}

// Set window dimensions
void SetWindowSize(int width, int height)
{
    glfwSetWindowSize(CORE.Window.handle, width, height);
}

// Set window opacity, value opacity is between 0.0 and 1.0
void SetWindowOpacity(float opacity)
{
    TRACELOG(LOG_WARNING, "SetWindowOpacity() not available on PLATFORM_WEB");
}

// Set window focused
void SetWindowFocused(void)
{
    TRACELOG(LOG_WARNING, "SetWindowFocused() not available on PLATFORM_WEB");
}

// Get native window handle
void *GetWindowHandle(void)
{
    TRACELOG(LOG_WARNING, "GetWindowHandle() not implemented on PLATFORM_WEB");
    return NULL;
}

// Get number of monitors
int GetMonitorCount(void)
{
    TRACELOG(LOG_WARNING, "GetMonitorCount() not implemented on PLATFORM_WEB");
    return 1;
}

// Get number of monitors
int GetCurrentMonitor(void)
{
    TRACELOG(LOG_WARNING, "GetCurrentMonitor() not implemented on PLATFORM_WEB");
    return 0;
}

// Get selected monitor position
Vector2 GetMonitorPosition(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorPosition() not implemented on PLATFORM_WEB");
    return (Vector2){ 0, 0 };
}

// Get selected monitor width (currently used by monitor)
int GetMonitorWidth(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorWidth() not implemented on PLATFORM_WEB");
    return 0;
}

// Get selected monitor height (currently used by monitor)
int GetMonitorHeight(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorHeight() not implemented on PLATFORM_WEB");
    return 0;
}

// Get selected monitor physical width in millimetres
int GetMonitorPhysicalWidth(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorPhysicalWidth() not implemented on PLATFORM_WEB");
    return 0;
}

// Get selected monitor physical height in millimetres
int GetMonitorPhysicalHeight(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorPhysicalHeight() not implemented on PLATFORM_WEB");
    return 0;
}

// Get selected monitor refresh rate
int GetMonitorRefreshRate(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorRefreshRate() not implemented on PLATFORM_WEB");
    return 0;
}

// Get the human-readable, UTF-8 encoded name of the selected monitor
const char *GetMonitorName(int monitor)
{
    TRACELOG(LOG_WARNING, "GetMonitorName() not implemented on PLATFORM_WEB");
    return "";
}

// Get window position XY on monitor
Vector2 GetWindowPosition(void)
{
    TRACELOG(LOG_WARNING, "GetWindowPosition() not implemented on PLATFORM_WEB");
    return (Vector2){ 0, 0 };
}

// Get window scale DPI factor for current monitor
Vector2 GetWindowScaleDPI(void)
{
    TRACELOG(LOG_WARNING, "GetWindowScaleDPI() not implemented on PLATFORM_WEB");
    return (Vector2){ 1.0f, 1.0f };
}

// Set clipboard text content
void SetClipboardText(const char *text)
{
    // Security check to (partially) avoid malicious code
    if (strchr(text, '\'') != NULL) TRACELOG(LOG_WARNING, "SYSTEM: Provided Clipboard could be potentially malicious, avoid [\'] character");
    else EM_ASM({ navigator.clipboard.writeText(UTF8ToString($0)); }, text);
}

// Get clipboard text content
// NOTE: returned string is allocated and freed by GLFW
const char *GetClipboardText(void)
{
/*
    // Accessing clipboard data from browser is tricky due to security reasons
    // The method to use is navigator.clipboard.readText() but this is an asynchronous method
    // that will return at some moment after the function is called with the required data
    emscripten_run_script_string("navigator.clipboard.readText() \
        .then(text => { document.getElementById('clipboard').innerText = text; console.log('Pasted content: ', text); }) \
        .catch(err => { console.error('Failed to read clipboard contents: ', err); });"
    );

    // The main issue is getting that data, one approach could be using ASYNCIFY and wait
    // for the data but it requires adding Asyncify emscripten library on compilation

    // Another approach could be just copy the data in a HTML text field and try to retrieve it
    // later on if available... and clean it for future accesses
*/
    return NULL;
}

// Show mouse cursor
void ShowCursor(void)
{
    CORE.Input.Mouse.cursorHidden = false;
}

// Hides mouse cursor
void HideCursor(void)
{
    CORE.Input.Mouse.cursorHidden = true;
}

// Enables cursor (unlock cursor)
void EnableCursor(void)
{
    emscripten_exit_pointerlock();

    // Set cursor position in the middle
    SetMousePosition(CORE.Window.screen.width/2, CORE.Window.screen.height/2);

    CORE.Input.Mouse.cursorHidden = false;
}

// Disables cursor (lock cursor)
void DisableCursor(void)
{
    // TODO: figure out how not to hard code the canvas ID here.
    emscripten_request_pointerlock("#canvas", 1);

    // Set cursor position in the middle
    SetMousePosition(CORE.Window.screen.width/2, CORE.Window.screen.height/2);

    CORE.Input.Mouse.cursorHidden = true;
}

// Get elapsed time measure in seconds since InitTimer()
double GetTime(void)
{
    double time = glfwGetTime();   // Elapsed time since glfwInit()
    return time;
}

// Takes a screenshot of current screen (saved a .png)
void TakeScreenshot(const char *fileName)
{
#if defined(SUPPORT_MODULE_RTEXTURES)
    // Security check to (partially) avoid malicious code on PLATFORM_WEB
    if (strchr(fileName, '\'') != NULL) { TRACELOG(LOG_WARNING, "SYSTEM: Provided fileName could be potentially malicious, avoid [\'] character"); return; }

    Vector2 scale = GetWindowScaleDPI();
    unsigned char *imgData = rlReadScreenPixels((int)((float)CORE.Window.render.width*scale.x), (int)((float)CORE.Window.render.height*scale.y));
    Image image = { imgData, (int)((float)CORE.Window.render.width*scale.x), (int)((float)CORE.Window.render.height*scale.y), 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };

    char path[2048] = { 0 };
    strcpy(path, TextFormat("%s/%s", CORE.Storage.basePath, fileName));

    ExportImage(image, path);           // WARNING: Module required: rtextures
    RL_FREE(imgData);

    // Download file from MEMFS (emscripten memory filesystem)
    // saveFileFromMEMFSToDisk() function is defined in raylib/src/shell.html
    emscripten_run_script(TextFormat("saveFileFromMEMFSToDisk('%s','%s')", GetFileName(path), GetFileName(path)));

    TRACELOG(LOG_INFO, "SYSTEM: [%s] Screenshot taken successfully", path);
#else
    TRACELOG(LOG_WARNING,"IMAGE: ExportImage() requires module: rtextures");
#endif
}

// Open URL with default system browser (if available)
// NOTE: This function is only safe to use if you control the URL given.
// A user could craft a malicious string performing another action.
// Only call this function yourself not with user input or make sure to check the string yourself.
// Ref: https://github.com/raysan5/raylib/issues/686
void OpenURL(const char *url)
{
    // Security check to (partially) avoid malicious code on PLATFORM_WEB
    if (strchr(url, '\'') != NULL) TRACELOG(LOG_WARNING, "SYSTEM: Provided URL could be potentially malicious, avoid [\'] character");
    else emscripten_run_script(TextFormat("window.open('%s', '_blank')", url));
}

//----------------------------------------------------------------------------------
// Module Functions Definition: Inputs
//----------------------------------------------------------------------------------

// Set a custom key to exit program
// NOTE: default exitKey is ESCAPE
void SetExitKey(int key)
{
    CORE.Input.Keyboard.exitKey = key;
}

// Get gamepad internal name id
const char *GetGamepadName(int gamepad)
{
    const char *name = NULL;

    name = CORE.Input.Gamepad.name[gamepad];

    return name;
}

// Get gamepad axis count
int GetGamepadAxisCount(int gamepad)
{
    return CORE.Input.Gamepad.axisCount;
}

// Set internal gamepad mappings
int SetGamepadMappings(const char *mappings)
{
    TRACELOG(LOG_INFO, "SetGamepadMappings not implemented in rcore_web.c");

    return 0;
}

// Get mouse position X
int GetMouseX(void)
{
    return (int)((CORE.Input.Mouse.currentPosition.x + CORE.Input.Mouse.offset.x)*CORE.Input.Mouse.scale.x);
}

// Get mouse position Y
int GetMouseY(void)
{
    return (int)((CORE.Input.Mouse.currentPosition.y + CORE.Input.Mouse.offset.y)*CORE.Input.Mouse.scale.y);
}

// Get mouse position XY
Vector2 GetMousePosition(void)
{
    Vector2 position = { 0 };

    // TODO: Review touch position

    // NOTE: On PLATFORM_WEB, even on canvas scaling, mouse position is proportionally returned
    position.x = (CORE.Input.Mouse.currentPosition.x + CORE.Input.Mouse.offset.x)*CORE.Input.Mouse.scale.x;
    position.y = (CORE.Input.Mouse.currentPosition.y + CORE.Input.Mouse.offset.y)*CORE.Input.Mouse.scale.y;

    return position;
}

// Set mouse position XY
void SetMousePosition(int x, int y)
{
    CORE.Input.Mouse.currentPosition = (Vector2){ (float)x, (float)y };
    CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;

    // NOTE: emscripten not implemented
    glfwSetCursorPos(CORE.Window.handle, CORE.Input.Mouse.currentPosition.x, CORE.Input.Mouse.currentPosition.y);
}

// Get mouse wheel movement Y
float GetMouseWheelMove(void)
{
    float result = 0.0f;

    if (fabsf(CORE.Input.Mouse.currentWheelMove.x) > fabsf(CORE.Input.Mouse.currentWheelMove.y)) result = (float)CORE.Input.Mouse.currentWheelMove.x;
    else result = (float)CORE.Input.Mouse.currentWheelMove.y;

    return result;
}

// Set mouse cursor
void SetMouseCursor(int cursor)
{
    TRACELOG(LOG_INFO, "SetMouseCursor not implemented in rcore_web.c");
}

// Get touch position X for touch point 0 (relative to screen size)
int GetTouchX(void)
{
    return (int)CORE.Input.Touch.position[0].x;
}

// Get touch position Y for touch point 0 (relative to screen size)
int GetTouchY(void)
{
    return (int)CORE.Input.Touch.position[0].y;
}

// Get touch position XY for a touch point index (relative to screen size)
// TODO: Touch position should be scaled depending on display size and render size
Vector2 GetTouchPosition(int index)
{
    Vector2 position = { -1.0f, -1.0f };

    if (index < MAX_TOUCH_POINTS) position = CORE.Input.Touch.position[index];
    else TRACELOG(LOG_WARNING, "INPUT: Required touch point out of range (Max touch points: %i)", MAX_TOUCH_POINTS);

    return position;
}

// Swap back buffer with front buffer (screen drawing)
void SwapScreenBuffer(void)
{
    glfwSwapBuffers(CORE.Window.handle);
}

// Register all input events
void PollInputEvents(void)
{
#if defined(SUPPORT_GESTURES_SYSTEM)
    // NOTE: Gestures update must be called every frame to reset gestures correctly
    // because ProcessGestureEvent() is just called on an event, not every frame
    UpdateGestures();
#endif

    // Reset keys/chars pressed registered
    CORE.Input.Keyboard.keyPressedQueueCount = 0;
    CORE.Input.Keyboard.charPressedQueueCount = 0;
    // Reset key repeats
    for (int i = 0; i < MAX_KEYBOARD_KEYS; i++) CORE.Input.Keyboard.keyRepeatInFrame[i] = 0;

    // Reset last gamepad button/axis registered state
    CORE.Input.Gamepad.lastButtonPressed = 0;       // GAMEPAD_BUTTON_UNKNOWN
    CORE.Input.Gamepad.axisCount = 0;
    // Keyboard/Mouse input polling (automatically managed by GLFW3 through callback)

    // Register previous keys states
    for (int i = 0; i < MAX_KEYBOARD_KEYS; i++)
    {
        CORE.Input.Keyboard.previousKeyState[i] = CORE.Input.Keyboard.currentKeyState[i];
        CORE.Input.Keyboard.keyRepeatInFrame[i] = 0;
    }

    // Register previous mouse states
    for (int i = 0; i < MAX_MOUSE_BUTTONS; i++) CORE.Input.Mouse.previousButtonState[i] = CORE.Input.Mouse.currentButtonState[i];

    // Register previous mouse wheel state
    CORE.Input.Mouse.previousWheelMove = CORE.Input.Mouse.currentWheelMove;
    CORE.Input.Mouse.currentWheelMove = (Vector2){ 0.0f, 0.0f };

    // Register previous mouse position
    CORE.Input.Mouse.previousPosition = CORE.Input.Mouse.currentPosition;

    // Register previous touch states
    for (int i = 0; i < MAX_TOUCH_POINTS; i++) CORE.Input.Touch.previousTouchState[i] = CORE.Input.Touch.currentTouchState[i];

    // Reset touch positions
    // TODO: It resets on PLATFORM_WEB the mouse position and not filled again until a move-event,
    // so, if mouse is not moved it returns a (0, 0) position... this behaviour should be reviewed!
    //for (int i = 0; i < MAX_TOUCH_POINTS; i++) CORE.Input.Touch.position[i] = (Vector2){ 0, 0 };

    CORE.Window.resizedLastFrame = false;

    // Gamepad support using emscripten API
    // NOTE: GLFW3 joystick functionality not available in web

    // Get number of gamepads connected
    int numGamepads = 0;
    if (emscripten_sample_gamepad_data() == EMSCRIPTEN_RESULT_SUCCESS) numGamepads = emscripten_get_num_gamepads();

    for (int i = 0; (i < numGamepads) && (i < MAX_GAMEPADS); i++)
    {
        // Register previous gamepad button states
        for (int k = 0; k < MAX_GAMEPAD_BUTTONS; k++) CORE.Input.Gamepad.previousButtonState[i][k] = CORE.Input.Gamepad.currentButtonState[i][k];

        EmscriptenGamepadEvent gamepadState;

        int result = emscripten_get_gamepad_status(i, &gamepadState);

        if (result == EMSCRIPTEN_RESULT_SUCCESS)
        {
            // Register buttons data for every connected gamepad
            for (int j = 0; (j < gamepadState.numButtons) && (j < MAX_GAMEPAD_BUTTONS); j++)
            {
                GamepadButton button = -1;

                // Gamepad Buttons reference: https://www.w3.org/TR/gamepad/#gamepad-interface
                switch (j)
                {
                    case 0: button = GAMEPAD_BUTTON_RIGHT_FACE_DOWN; break;
                    case 1: button = GAMEPAD_BUTTON_RIGHT_FACE_RIGHT; break;
                    case 2: button = GAMEPAD_BUTTON_RIGHT_FACE_LEFT; break;
                    case 3: button = GAMEPAD_BUTTON_RIGHT_FACE_UP; break;
                    case 4: button = GAMEPAD_BUTTON_LEFT_TRIGGER_1; break;
                    case 5: button = GAMEPAD_BUTTON_RIGHT_TRIGGER_1; break;
                    case 6: button = GAMEPAD_BUTTON_LEFT_TRIGGER_2; break;
                    case 7: button = GAMEPAD_BUTTON_RIGHT_TRIGGER_2; break;
                    case 8: button = GAMEPAD_BUTTON_MIDDLE_LEFT; break;
                    case 9: button = GAMEPAD_BUTTON_MIDDLE_RIGHT; break;
                    case 10: button = GAMEPAD_BUTTON_LEFT_THUMB; break;
                    case 11: button = GAMEPAD_BUTTON_RIGHT_THUMB; break;
                    case 12: button = GAMEPAD_BUTTON_LEFT_FACE_UP; break;
                    case 13: button = GAMEPAD_BUTTON_LEFT_FACE_DOWN; break;
                    case 14: button = GAMEPAD_BUTTON_LEFT_FACE_LEFT; break;
                    case 15: button = GAMEPAD_BUTTON_LEFT_FACE_RIGHT; break;
                    default: break;
                }

                if (button != -1)   // Check for valid button
                {
                    if (gamepadState.digitalButton[j] == 1)
                    {
                        CORE.Input.Gamepad.currentButtonState[i][button] = 1;
                        CORE.Input.Gamepad.lastButtonPressed = button;
                    }
                    else CORE.Input.Gamepad.currentButtonState[i][button] = 0;
                }

                //TRACELOGD("INPUT: Gamepad %d, button %d: Digital: %d, Analog: %g", gamepadState.index, j, gamepadState.digitalButton[j], gamepadState.analogButton[j]);
            }

            // Register axis data for every connected gamepad
            for (int j = 0; (j < gamepadState.numAxes) && (j < MAX_GAMEPAD_AXIS); j++)
            {
                CORE.Input.Gamepad.axisState[i][j] = gamepadState.axis[j];
            }

            CORE.Input.Gamepad.axisCount = gamepadState.numAxes;
        }
    }
}

//----------------------------------------------------------------------------------
// Module Internal Functions Definition
//----------------------------------------------------------------------------------

// Initialize display device and framebuffer
// NOTE: width and height represent the screen (framebuffer) desired size, not actual display size
// If width or height are 0, default display size will be used for framebuffer size
// NOTE: returns false in case graphic device could not be created
static bool InitGraphicsDevice(int width, int height)
{
    CORE.Window.screen.width = width;           // User desired width
    CORE.Window.screen.height = height;         // User desired height
    CORE.Window.screenScale = MatrixIdentity(); // No draw scaling required by default

    // Set the screen minimum and maximum default values to 0
    CORE.Window.screenMin.width  = 0;
    CORE.Window.screenMin.height = 0;
    CORE.Window.screenMax.width  = 0;
    CORE.Window.screenMax.height = 0;

    // NOTE: Framebuffer (render area - CORE.Window.render.width, CORE.Window.render.height) could include black bars...
    // ...in top-down or left-right to match display aspect ratio (no weird scaling)

    glfwSetErrorCallback(ErrorCallback);
/*
    // TODO: Setup GLFW custom allocators to match raylib ones
    const GLFWallocator allocator = {
        .allocate = MemAlloc,
        .deallocate = MemFree,
        .reallocate = MemRealloc,
        .user = NULL
    };

    glfwInitAllocator(&allocator);
*/

    if (!glfwInit())
    {
        TRACELOG(LOG_WARNING, "GLFW: Failed to initialize GLFW");
        return false;
    }

    glfwDefaultWindowHints(); // Set default windows hints
    // glfwWindowHint(GLFW_RED_BITS, 8);             // Framebuffer red color component bits
    // glfwWindowHint(GLFW_GREEN_BITS, 8);           // Framebuffer green color component bits
    // glfwWindowHint(GLFW_BLUE_BITS, 8);            // Framebuffer blue color component bits
    // glfwWindowHint(GLFW_ALPHA_BITS, 8);           // Framebuffer alpha color component bits
    // glfwWindowHint(GLFW_DEPTH_BITS, 24);          // Depthbuffer bits
    // glfwWindowHint(GLFW_REFRESH_RATE, 0);         // Refresh rate for fullscreen window
    // glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API); // OpenGL API to use. Alternative: GLFW_OPENGL_ES_API
    // glfwWindowHint(GLFW_AUX_BUFFERS, 0);          // Number of auxiliar buffers

    // Check window creation flags
    if ((CORE.Window.flags & FLAG_FULLSCREEN_MODE) > 0) CORE.Window.fullscreen = true;

    if ((CORE.Window.flags & FLAG_WINDOW_HIDDEN) > 0) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // Visible window
    else glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE); // Window initially hidden

    if ((CORE.Window.flags & FLAG_WINDOW_UNDECORATED) > 0) glfwWindowHint(GLFW_DECORATED, GLFW_FALSE); // Border and buttons on Window
    else glfwWindowHint(GLFW_DECORATED, GLFW_TRUE); // Decorated window

    if ((CORE.Window.flags & FLAG_WINDOW_RESIZABLE) > 0) glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE); // Resizable window
    else glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // Avoid window being resizable

    // Disable FLAG_WINDOW_MINIMIZED, not supported on initialization
    if ((CORE.Window.flags & FLAG_WINDOW_MINIMIZED) > 0) CORE.Window.flags &= ~FLAG_WINDOW_MINIMIZED;

    // Disable FLAG_WINDOW_MAXIMIZED, not supported on initialization
    if ((CORE.Window.flags & FLAG_WINDOW_MAXIMIZED) > 0) CORE.Window.flags &= ~FLAG_WINDOW_MAXIMIZED;

    if ((CORE.Window.flags & FLAG_WINDOW_UNFOCUSED) > 0) glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
    else glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);

    if ((CORE.Window.flags & FLAG_WINDOW_TOPMOST) > 0) glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    else glfwWindowHint(GLFW_FLOATING, GLFW_FALSE);

        // NOTE: Some GLFW flags are not supported on HTML5
        // e.g.: GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_SCALE_TO_MONITOR, GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_MOUSE_PASSTHROUGH

    if (CORE.Window.flags & FLAG_MSAA_4X_HINT)
    {
        // NOTE: MSAA is only enabled for main framebuffer, not user-created FBOs
        TRACELOG(LOG_INFO, "DISPLAY: Trying to enable MSAA x4");
        glfwWindowHint(GLFW_SAMPLES, 4); // Tries to enable multisampling x4 (MSAA), default is 0
    }

    // NOTE: When asking for an OpenGL context version, most drivers provide the highest supported version
    // with backward compatibility to older OpenGL versions.
    // For example, if using OpenGL 1.1, driver can provide a 4.3 backwards compatible context.

    // Check selection OpenGL version
    if (rlGetVersion() == RL_OPENGL_21)
    {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2); // Choose OpenGL major version (just hint)
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1); // Choose OpenGL minor version (just hint)
    }
    else if (rlGetVersion() == RL_OPENGL_33)
    {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);                 // Choose OpenGL major version (just hint)
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);                 // Choose OpenGL minor version (just hint)
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // Profiles Hint: Only 3.3 and above!
                                                                       // Values: GLFW_OPENGL_CORE_PROFILE, GLFW_OPENGL_ANY_PROFILE, GLFW_OPENGL_COMPAT_PROFILE
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_FALSE); // Forward Compatibility Hint: Only 3.3 and above!
        // glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE); // Request OpenGL DEBUG context
    }
    else if (rlGetVersion() == RL_OPENGL_43)
    {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4); // Choose OpenGL major version (just hint)
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); // Choose OpenGL minor version (just hint)
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_FALSE);
#if defined(RLGL_ENABLE_OPENGL_DEBUG_CONTEXT)
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE); // Enable OpenGL Debug Context
#endif
    }
    else if (rlGetVersion() == RL_OPENGL_ES_20) // Request OpenGL ES 2.0 context
    {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
        glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_NATIVE_CONTEXT_API);
    }
    else if (rlGetVersion() == RL_OPENGL_ES_30) // Request OpenGL ES 3.0 context
    {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
        glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_NATIVE_CONTEXT_API);
    }

    // NOTE: Getting video modes is not implemented in emscripten GLFW3 version
    CORE.Window.display.width = CORE.Window.screen.width;
    CORE.Window.display.height = CORE.Window.screen.height;

    if (CORE.Window.fullscreen)
    {
        // remember center for switchinging from fullscreen to window
        if ((CORE.Window.screen.height == CORE.Window.display.height) && (CORE.Window.screen.width == CORE.Window.display.width))
        {
            // If screen width/height equal to the display, we can't calculate the window pos for toggling full-screened/windowed.
            // Toggling full-screened/windowed with pos(0, 0) can cause problems in some platforms, such as X11.
            CORE.Window.position.x = CORE.Window.display.width/4;
            CORE.Window.position.y = CORE.Window.display.height/4;
        }
        else
        {
            CORE.Window.position.x = CORE.Window.display.width/2 - CORE.Window.screen.width/2;
            CORE.Window.position.y = CORE.Window.display.height/2 - CORE.Window.screen.height/2;
        }

        if (CORE.Window.position.x < 0) CORE.Window.position.x = 0;
        if (CORE.Window.position.y < 0) CORE.Window.position.y = 0;

        // Obtain recommended CORE.Window.display.width/CORE.Window.display.height from a valid videomode for the monitor
        int count = 0;
        const GLFWvidmode *modes = glfwGetVideoModes(glfwGetPrimaryMonitor(), &count);

        // Get closest video mode to desired CORE.Window.screen.width/CORE.Window.screen.height
        for (int i = 0; i < count; i++)
        {
            if ((unsigned int)modes[i].width >= CORE.Window.screen.width)
            {
                if ((unsigned int)modes[i].height >= CORE.Window.screen.height)
                {
                    CORE.Window.display.width = modes[i].width;
                    CORE.Window.display.height = modes[i].height;
                    break;
                }
            }
        }
        
        TRACELOG(LOG_WARNING, "SYSTEM: Closest fullscreen videomode: %i x %i", CORE.Window.display.width, CORE.Window.display.height);

        // NOTE: ISSUE: Closest videomode could not match monitor aspect-ratio, for example,
        // for a desired screen size of 800x450 (16:9), closest supported videomode is 800x600 (4:3),
        // framebuffer is rendered correctly but once displayed on a 16:9 monitor, it gets stretched
        // by the sides to fit all monitor space...

        // Try to setup the most appropriate fullscreen framebuffer for the requested screenWidth/screenHeight
        // It considers device display resolution mode and setups a framebuffer with black bars if required (render size/offset)
        // Modified global variables: CORE.Window.screen.width/CORE.Window.screen.height - CORE.Window.render.width/CORE.Window.render.height - CORE.Window.renderOffset.x/CORE.Window.renderOffset.y - CORE.Window.screenScale
        // TODO: It is a quite cumbersome solution to display size vs requested size, it should be reviewed or removed...
        // HighDPI monitors are properly considered in a following similar function: SetupViewport()
        SetupFramebuffer(CORE.Window.display.width, CORE.Window.display.height);

        CORE.Window.handle = glfwCreateWindow(CORE.Window.display.width, CORE.Window.display.height, (CORE.Window.title != 0)? CORE.Window.title : " ", glfwGetPrimaryMonitor(), NULL);

        // NOTE: Full-screen change, not working properly...
        // glfwSetWindowMonitor(CORE.Window.handle, glfwGetPrimaryMonitor(), 0, 0, CORE.Window.screen.width, CORE.Window.screen.height, GLFW_DONT_CARE);
    }
    else
    {
        // No-fullscreen window creation
        CORE.Window.handle = glfwCreateWindow(CORE.Window.screen.width, CORE.Window.screen.height, (CORE.Window.title != 0)? CORE.Window.title : " ", NULL, NULL);

        if (CORE.Window.handle)
        {
            CORE.Window.render.width = CORE.Window.screen.width;
            CORE.Window.render.height = CORE.Window.screen.height;
        }
    }

    if (!CORE.Window.handle)
    {
        glfwTerminate();
        TRACELOG(LOG_WARNING, "GLFW: Failed to initialize Window");
        return false;
    }

    // WARNING: glfwCreateWindow() title doesn't work with emscripten
    emscripten_set_window_title((CORE.Window.title != 0)? CORE.Window.title : " ");

    // Set window callback events
    glfwSetWindowSizeCallback(CORE.Window.handle, WindowSizeCallback); // NOTE: Resizing not allowed by default!
    glfwSetWindowIconifyCallback(CORE.Window.handle, WindowIconifyCallback);
    glfwSetWindowFocusCallback(CORE.Window.handle, WindowFocusCallback);
    glfwSetDropCallback(CORE.Window.handle, WindowDropCallback);

    // Set input callback events
    glfwSetKeyCallback(CORE.Window.handle, KeyCallback);
    glfwSetCharCallback(CORE.Window.handle, CharCallback);
    glfwSetMouseButtonCallback(CORE.Window.handle, MouseButtonCallback);
    glfwSetCursorPosCallback(CORE.Window.handle, MouseCursorPosCallback); // Track mouse position changes
    glfwSetScrollCallback(CORE.Window.handle, MouseScrollCallback);
    glfwSetCursorEnterCallback(CORE.Window.handle, CursorEnterCallback);

    glfwMakeContextCurrent(CORE.Window.handle);

    // Try to enable GPU V-Sync, so frames are limited to screen refresh rate (60Hz -> 60 FPS)
    // NOTE: V-Sync can be enabled by graphic driver configuration, it doesn't need
    // to be activated on web platforms since VSync is enforced there.

    int fbWidth = CORE.Window.screen.width;
    int fbHeight = CORE.Window.screen.height;

    CORE.Window.render.width = fbWidth;
    CORE.Window.render.height = fbHeight;
    CORE.Window.currentFbo.width = fbWidth;
    CORE.Window.currentFbo.height = fbHeight;

    TRACELOG(LOG_INFO, "DISPLAY: Device initialized successfully");
    TRACELOG(LOG_INFO, "    > Display size: %i x %i", CORE.Window.display.width, CORE.Window.display.height);
    TRACELOG(LOG_INFO, "    > Screen size:  %i x %i", CORE.Window.screen.width, CORE.Window.screen.height);
    TRACELOG(LOG_INFO, "    > Render size:  %i x %i", CORE.Window.render.width, CORE.Window.render.height);
    TRACELOG(LOG_INFO, "    > Viewport offsets: %i, %i", CORE.Window.renderOffset.x, CORE.Window.renderOffset.y);

    // Load OpenGL extensions
    // NOTE: GL procedures address loader is required to load extensions
    rlLoadExtensions(glfwGetProcAddress);

    // Initialize OpenGL context (states and resources)
    // NOTE: CORE.Window.currentFbo.width and CORE.Window.currentFbo.height not used, just stored as globals in rlgl
    rlglInit(CORE.Window.currentFbo.width, CORE.Window.currentFbo.height);

    // Setup default viewport
    // NOTE: It updated CORE.Window.render.width and CORE.Window.render.height
    SetupViewport(CORE.Window.currentFbo.width, CORE.Window.currentFbo.height);

    if ((CORE.Window.flags & FLAG_WINDOW_MINIMIZED) > 0) MinimizeWindow();

    return true;
}

// GLFW3 Error Callback, runs on GLFW3 error
static void ErrorCallback(int error, const char *description)
{
    TRACELOG(LOG_WARNING, "GLFW: Error: %i Description: %s", error, description);
}

// GLFW3 WindowSize Callback, runs when window is resizedLastFrame
// NOTE: Window resizing not allowed by default
static void WindowSizeCallback(GLFWwindow *window, int width, int height)
{
    // Reset viewport and projection matrix for new size
    SetupViewport(width, height);

    CORE.Window.currentFbo.width = width;
    CORE.Window.currentFbo.height = height;
    CORE.Window.resizedLastFrame = true;

    if (IsWindowFullscreen()) return;

    // Set current screen size
    if ((CORE.Window.flags & FLAG_WINDOW_HIGHDPI) > 0)
    {
        Vector2 windowScaleDPI = GetWindowScaleDPI();

        CORE.Window.screen.width = (unsigned int)(width/windowScaleDPI.x);
        CORE.Window.screen.height = (unsigned int)(height/windowScaleDPI.y);
    }
    else
    {
        CORE.Window.screen.width = width;
        CORE.Window.screen.height = height;
    }

    // NOTE: Postprocessing texture is not scaled to new size
}

// GLFW3 WindowIconify Callback, runs when window is minimized/restored
static void WindowIconifyCallback(GLFWwindow *window, int iconified)
{
    if (iconified) CORE.Window.flags |= FLAG_WINDOW_MINIMIZED;  // The window was iconified
    else CORE.Window.flags &= ~FLAG_WINDOW_MINIMIZED;           // The window was restored
}

// GLFW3 Window Maximize Callback, runs when window is maximized
static void WindowMaximizeCallback(GLFWwindow *window, int maximized)
{
    
}

// GLFW3 WindowFocus Callback, runs when window get/lose focus
static void WindowFocusCallback(GLFWwindow *window, int focused)
{
    if (focused) CORE.Window.flags &= ~FLAG_WINDOW_UNFOCUSED;   // The window was focused
    else CORE.Window.flags |= FLAG_WINDOW_UNFOCUSED;            // The window lost focus
}

// GLFW3 Window Drop Callback, runs when drop files into window
static void WindowDropCallback(GLFWwindow *window, int count, const char **paths)
{
    if (count > 0)
    {
        // In case previous dropped filepaths have not been freed, we free them
        if (CORE.Window.dropFileCount > 0)
        {
            for (unsigned int i = 0; i < CORE.Window.dropFileCount; i++) RL_FREE(CORE.Window.dropFilepaths[i]);

            RL_FREE(CORE.Window.dropFilepaths);

            CORE.Window.dropFileCount = 0;
            CORE.Window.dropFilepaths = NULL;
        }

        // WARNING: Paths are freed by GLFW when the callback returns, we must keep an internal copy
        CORE.Window.dropFileCount = count;
        CORE.Window.dropFilepaths = (char **)RL_CALLOC(CORE.Window.dropFileCount, sizeof(char *));

        for (unsigned int i = 0; i < CORE.Window.dropFileCount; i++)
        {
            CORE.Window.dropFilepaths[i] = (char *)RL_CALLOC(MAX_FILEPATH_LENGTH, sizeof(char));
            strcpy(CORE.Window.dropFilepaths[i], paths[i]);
        }
    }
}


// GLFW3 Keyboard Callback, runs on key pressed
static void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (key < 0) return;    // Security check, macOS fn key generates -1

    // WARNING: GLFW could return GLFW_REPEAT, we need to consider it as 1
    // to work properly with our implementation (IsKeyDown/IsKeyUp checks)
    if (action == GLFW_RELEASE) CORE.Input.Keyboard.currentKeyState[key] = 0;
    else if(action == GLFW_PRESS) CORE.Input.Keyboard.currentKeyState[key] = 1;
    else if(action == GLFW_REPEAT) CORE.Input.Keyboard.keyRepeatInFrame[key] = 1;

    // Check if there is space available in the key queue
    if ((CORE.Input.Keyboard.keyPressedQueueCount < MAX_KEY_PRESSED_QUEUE) && (action == GLFW_PRESS))
    {
        // Add character to the queue
        CORE.Input.Keyboard.keyPressedQueue[CORE.Input.Keyboard.keyPressedQueueCount] = key;
        CORE.Input.Keyboard.keyPressedQueueCount++;
    }

    // Check the exit key to set close window
    if ((key == CORE.Input.Keyboard.exitKey) && (action == GLFW_PRESS)) glfwSetWindowShouldClose(CORE.Window.handle, GLFW_TRUE);

#if defined(SUPPORT_SCREEN_CAPTURE)
    if ((key == GLFW_KEY_F12) && (action == GLFW_PRESS))
    {
#if defined(SUPPORT_GIF_RECORDING)
        if (mods & GLFW_MOD_CONTROL)
        {
            if (gifRecording)
            {
                gifRecording = false;

                MsfGifResult result = msf_gif_end(&gifState);

                SaveFileData(TextFormat("%s/screenrec%03i.gif", CORE.Storage.basePath, screenshotCounter), result.data, (unsigned int)result.dataSize);
                msf_gif_free(result);

                // Download file from MEMFS (emscripten memory filesystem)
                // saveFileFromMEMFSToDisk() function is defined in raylib/templates/web_shel/shell.html
                emscripten_run_script(TextFormat("saveFileFromMEMFSToDisk('%s','%s')", TextFormat("screenrec%03i.gif", screenshotCounter - 1), TextFormat("screenrec%03i.gif", screenshotCounter - 1)));

                TRACELOG(LOG_INFO, "SYSTEM: Finish animated GIF recording");
            }
            else
            {
                gifRecording = true;
                gifFrameCounter = 0;

                Vector2 scale = GetWindowScaleDPI();
                msf_gif_begin(&gifState, (int)((float)CORE.Window.render.width*scale.x), (int)((float)CORE.Window.render.height*scale.y));
                screenshotCounter++;

                TRACELOG(LOG_INFO, "SYSTEM: Start animated GIF recording: %s", TextFormat("screenrec%03i.gif", screenshotCounter));
            }
        }
        else
#endif  // SUPPORT_GIF_RECORDING
        {
            TakeScreenshot(TextFormat("screenshot%03i.png", screenshotCounter));
            screenshotCounter++;
        }
    }
#endif  // SUPPORT_SCREEN_CAPTURE

#if defined(SUPPORT_EVENTS_AUTOMATION)
    if ((key == GLFW_KEY_F11) && (action == GLFW_PRESS))
    {
        eventsRecording = !eventsRecording;

        // On finish recording, we export events into a file
        if (!eventsRecording) ExportAutomationEvents("eventsrec.rep");
    }
    else if ((key == GLFW_KEY_F9) && (action == GLFW_PRESS))
    {
        LoadAutomationEvents("eventsrec.rep");
        eventsPlaying = true;

        TRACELOG(LOG_WARNING, "eventsPlaying enabled!");
    }
#endif
}

// GLFW3 Char Key Callback, runs on key down (gets equivalent unicode char value)
static void CharCallback(GLFWwindow *window, unsigned int key)
{
    //TRACELOG(LOG_DEBUG, "Char Callback: KEY:%i(%c)", key, key);

    // NOTE: Registers any key down considering OS keyboard layout but
    // does not detect action events, those should be managed by user...
    // Ref: https://github.com/glfw/glfw/issues/668#issuecomment-166794907
    // Ref: https://www.glfw.org/docs/latest/input_guide.html#input_char

    // Check if there is space available in the queue
    if (CORE.Input.Keyboard.charPressedQueueCount < MAX_CHAR_PRESSED_QUEUE)
    {
        // Add character to the queue
        CORE.Input.Keyboard.charPressedQueue[CORE.Input.Keyboard.charPressedQueueCount] = key;
        CORE.Input.Keyboard.charPressedQueueCount++;
    }
}

// GLFW3 Mouse Button Callback, runs on mouse button pressed
static void MouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
    // WARNING: GLFW could only return GLFW_PRESS (1) or GLFW_RELEASE (0) for now,
    // but future releases may add more actions (i.e. GLFW_REPEAT)
    CORE.Input.Mouse.currentButtonState[button] = action;

#if defined(SUPPORT_GESTURES_SYSTEM) && defined(SUPPORT_MOUSE_GESTURES)
    // Process mouse events as touches to be able to use mouse-gestures
    GestureEvent gestureEvent = { 0 };

    // Register touch actions
    if ((CORE.Input.Mouse.currentButtonState[button] == 1) && (CORE.Input.Mouse.previousButtonState[button] == 0)) gestureEvent.touchAction = TOUCH_ACTION_DOWN;
    else if ((CORE.Input.Mouse.currentButtonState[button] == 0) && (CORE.Input.Mouse.previousButtonState[button] == 1)) gestureEvent.touchAction = TOUCH_ACTION_UP;

    // NOTE: TOUCH_ACTION_MOVE event is registered in MouseCursorPosCallback()

    // Assign a pointer ID
    gestureEvent.pointId[0] = 0;

    // Register touch points count
    gestureEvent.pointCount = 1;

    // Register touch points position, only one point registered
    gestureEvent.position[0] = GetMousePosition();

    // Normalize gestureEvent.position[0] for CORE.Window.screen.width and CORE.Window.screen.height
    gestureEvent.position[0].x /= (float)GetScreenWidth();
    gestureEvent.position[0].y /= (float)GetScreenHeight();

    // Gesture data is sent to gestures-system for processing
    // Prevent calling ProcessGestureEvent() when Emscripten is present and there's a touch gesture, so EmscriptenTouchCallback() can handle it itself
    if (GetMouseX() != 0 || GetMouseY() != 0) ProcessGestureEvent(gestureEvent);

#endif
}

// GLFW3 Cursor Position Callback, runs on mouse move
static void MouseCursorPosCallback(GLFWwindow *window, double x, double y)
{
    CORE.Input.Mouse.currentPosition.x = (float)x;
    CORE.Input.Mouse.currentPosition.y = (float)y;
    CORE.Input.Touch.position[0] = CORE.Input.Mouse.currentPosition;

#if defined(SUPPORT_GESTURES_SYSTEM) && defined(SUPPORT_MOUSE_GESTURES)
    // Process mouse events as touches to be able to use mouse-gestures
    GestureEvent gestureEvent = { 0 };

    gestureEvent.touchAction = TOUCH_ACTION_MOVE;

    // Assign a pointer ID
    gestureEvent.pointId[0] = 0;

    // Register touch points count
    gestureEvent.pointCount = 1;

    // Register touch points position, only one point registered
    gestureEvent.position[0] = CORE.Input.Touch.position[0];

    // Normalize gestureEvent.position[0] for CORE.Window.screen.width and CORE.Window.screen.height
    gestureEvent.position[0].x /= (float)GetScreenWidth();
    gestureEvent.position[0].y /= (float)GetScreenHeight();

    // Gesture data is sent to gestures-system for processing
    ProcessGestureEvent(gestureEvent);
#endif
}

// GLFW3 Scrolling Callback, runs on mouse wheel
static void MouseScrollCallback(GLFWwindow *window, double xoffset, double yoffset)
{
    CORE.Input.Mouse.currentWheelMove = (Vector2){ (float)xoffset, (float)yoffset };
}

// GLFW3 CursorEnter Callback, when cursor enters the window
static void CursorEnterCallback(GLFWwindow *window, int enter)
{
    if (enter == true) CORE.Input.Mouse.cursorOnScreen = true;
    else CORE.Input.Mouse.cursorOnScreen = false;
}


// Register fullscreen change events
static EM_BOOL EmscriptenFullscreenChangeCallback(int eventType, const EmscriptenFullscreenChangeEvent *event, void *userData)
{
    // TODO: Implement EmscriptenFullscreenChangeCallback()?

    return 1; // The event was consumed by the callback handler
}

// Register window resize event
static EM_BOOL EmscriptenWindowResizedCallback(int eventType, const EmscriptenUiEvent *event, void *userData)
{
    // TODO: Implement EmscriptenWindowResizedCallback()?

    return 1; // The event was consumed by the callback handler
}

EM_JS(int, GetWindowInnerWidth, (), { return window.innerWidth; });
EM_JS(int, GetWindowInnerHeight, (), { return window.innerHeight; });

// Register DOM element resize event
static EM_BOOL EmscriptenResizeCallback(int eventType, const EmscriptenUiEvent *event, void *userData)
{
    // Don't resize non-resizeable windows
    if ((CORE.Window.flags & FLAG_WINDOW_RESIZABLE) == 0) return 1;

    // This event is called whenever the window changes sizes,
    // so the size of the canvas object is explicitly retrieved below
    int width = GetWindowInnerWidth();
    int height = GetWindowInnerHeight();

    if (width < CORE.Window.screenMin.width) width = CORE.Window.screenMin.width;
    else if (width > CORE.Window.screenMax.width && CORE.Window.screenMax.width > 0) width = CORE.Window.screenMax.width;

    if (height < CORE.Window.screenMin.height) height = CORE.Window.screenMin.height;
    else if (height > CORE.Window.screenMax.height && CORE.Window.screenMax.height > 0) height = CORE.Window.screenMax.height;

    emscripten_set_canvas_element_size("#canvas", width, height);

    SetupViewport(width, height); // Reset viewport and projection matrix for new size

    CORE.Window.currentFbo.width = width;
    CORE.Window.currentFbo.height = height;
    CORE.Window.resizedLastFrame = true;

    if (IsWindowFullscreen()) return 1;

    // Set current screen size
    CORE.Window.screen.width = width;
    CORE.Window.screen.height = height;

    // NOTE: Postprocessing texture is not scaled to new size

    return 0;
}

// Register mouse input events
static EM_BOOL EmscriptenMouseCallback(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData)
{
    // This is only for registering mouse click events with emscripten and doesn't need to do anything

    return 1; // The event was consumed by the callback handler
}

// Register connected/disconnected gamepads events
static EM_BOOL EmscriptenGamepadCallback(int eventType, const EmscriptenGamepadEvent *gamepadEvent, void *userData)
{
    /*
    TRACELOGD("%s: timeStamp: %g, connected: %d, index: %ld, numAxes: %d, numButtons: %d, id: \"%s\", mapping: \"%s\"",
           eventType != 0? emscripten_event_type_to_string(eventType) : "Gamepad state",
           gamepadEvent->timestamp, gamepadEvent->connected, gamepadEvent->index, gamepadEvent->numAxes, gamepadEvent->numButtons, gamepadEvent->id, gamepadEvent->mapping);

    for (int i = 0; i < gamepadEvent->numAxes; ++i) TRACELOGD("Axis %d: %g", i, gamepadEvent->axis[i]);
    for (int i = 0; i < gamepadEvent->numButtons; ++i) TRACELOGD("Button %d: Digital: %d, Analog: %g", i, gamepadEvent->digitalButton[i], gamepadEvent->analogButton[i]);
    */

    if ((gamepadEvent->connected) && (gamepadEvent->index < MAX_GAMEPADS))
    {
        CORE.Input.Gamepad.ready[gamepadEvent->index] = true;
        sprintf(CORE.Input.Gamepad.name[gamepadEvent->index], "%s", gamepadEvent->id);
    }
    else CORE.Input.Gamepad.ready[gamepadEvent->index] = false;

    return 1; // The event was consumed by the callback handler
}

// Register touch input events
static EM_BOOL EmscriptenTouchCallback(int eventType, const EmscriptenTouchEvent *touchEvent, void *userData)
{
    // Register touch points count
    CORE.Input.Touch.pointCount = touchEvent->numTouches;

    double canvasWidth = 0.0;
    double canvasHeight = 0.0;
    // NOTE: emscripten_get_canvas_element_size() returns canvas.width and canvas.height but
    // we are looking for actual CSS size: canvas.style.width and canvas.style.height
    // EMSCRIPTEN_RESULT res = emscripten_get_canvas_element_size("#canvas", &canvasWidth, &canvasHeight);
    emscripten_get_element_css_size("#canvas", &canvasWidth, &canvasHeight);

    for (int i = 0; (i < CORE.Input.Touch.pointCount) && (i < MAX_TOUCH_POINTS); i++)
    {
        // Register touch points id
        CORE.Input.Touch.pointId[i] = touchEvent->touches[i].identifier;

        // Register touch points position
        CORE.Input.Touch.position[i] = (Vector2){touchEvent->touches[i].targetX, touchEvent->touches[i].targetY};

        // Normalize gestureEvent.position[x] for CORE.Window.screen.width and CORE.Window.screen.height
        CORE.Input.Touch.position[i].x *= ((float)GetScreenWidth()/(float)canvasWidth);
        CORE.Input.Touch.position[i].y *= ((float)GetScreenHeight()/(float)canvasHeight);

        if (eventType == EMSCRIPTEN_EVENT_TOUCHSTART) CORE.Input.Touch.currentTouchState[i] = 1;
        else if (eventType == EMSCRIPTEN_EVENT_TOUCHEND) CORE.Input.Touch.currentTouchState[i] = 0;
    }

#if defined(SUPPORT_GESTURES_SYSTEM) // PLATFORM_WEB
    GestureEvent gestureEvent = {0};

    gestureEvent.pointCount = CORE.Input.Touch.pointCount;

    // Register touch actions
    if (eventType == EMSCRIPTEN_EVENT_TOUCHSTART) gestureEvent.touchAction = TOUCH_ACTION_DOWN;
    else if (eventType == EMSCRIPTEN_EVENT_TOUCHEND) gestureEvent.touchAction = TOUCH_ACTION_UP;
    else if (eventType == EMSCRIPTEN_EVENT_TOUCHMOVE) gestureEvent.touchAction = TOUCH_ACTION_MOVE;
    else if (eventType == EMSCRIPTEN_EVENT_TOUCHCANCEL) gestureEvent.touchAction = TOUCH_ACTION_CANCEL;

    for (int i = 0; (i < gestureEvent.pointCount) && (i < MAX_TOUCH_POINTS); i++)
    {
        gestureEvent.pointId[i] = CORE.Input.Touch.pointId[i];
        gestureEvent.position[i] = CORE.Input.Touch.position[i];

        // Normalize gestureEvent.position[i]
        gestureEvent.position[i].x /= (float)GetScreenWidth();
        gestureEvent.position[i].y /= (float)GetScreenHeight();
    }

    // Gesture data is sent to gestures system for processing
    ProcessGestureEvent(gestureEvent);

    // Reset the pointCount for web, if it was the last Touch End event
    if (eventType == EMSCRIPTEN_EVENT_TOUCHEND && CORE.Input.Touch.pointCount == 1) CORE.Input.Touch.pointCount = 0;
#endif

    return 1; // The event was consumed by the callback handler
}

// EOF