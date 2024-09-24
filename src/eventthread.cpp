/*
 ** eventthread.cpp
 **
 ** This file is part of mkxp.
 **
 ** Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
 **
 ** mkxp is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** mkxp is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "eventthread.h"
#include "SDL3/SDL_properties.h"
#include <SDL3/SDL_video.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_touch.h>
#include <SDL3/SDL_rect.h>

#ifndef MKXPZ_NO_OPENAL
#include <al.h>
#include <alc.h>
#include <alext.h>
#endif
#include <cmath>

#include "sharedstate.h"
#include "graphics.h"

#ifndef MKXPZ_BUILD_XCODE
#include "settingsmenu.h"
#include "gamecontrollerdb.txt.xxd"
#else
#include "system/system.h"
#include "filesystem/filesystem.h"
#include "TouchBar.h"
#endif

#ifndef MKXPZ_NO_OPENAL
#include "al-util.h"
#endif
#include "debugwriter.h"

#ifndef SDL_PLATFORM_APPLE
#include "util/string-util.h"
#endif

#include <string.h>

#ifndef MKXPZ_NO_OPENAL
typedef void (ALC_APIENTRY *LPALCDEVICEPAUSESOFT) (ALCdevice *device);
typedef void (ALC_APIENTRY *LPALCDEVICERESUMESOFT) (ALCdevice *device);

#define AL_DEVICE_PAUSE_FUN \
AL_FUN(DevicePause, LPALCDEVICEPAUSESOFT) \
AL_FUN(DeviceResume, LPALCDEVICERESUMESOFT)

struct ALCFunctions
{
#define AL_FUN(name, type) type name;
    AL_DEVICE_PAUSE_FUN
#undef AL_FUN
} static alc;

static void
initALCFunctions(ALCdevice *alcDev)
{
    if (!strstr(alcGetString(alcDev, ALC_EXTENSIONS), "ALC_SOFT_pause_device"))
        return;
    
    Debug() << "ALC_SOFT_pause_device present";
    
#define AL_FUN(name, type) alc. name = (type) alcGetProcAddress(alcDev, "alc" #name "SOFT");
    AL_DEVICE_PAUSE_FUN;
#undef AL_FUN
}

#define HAVE_ALC_DEVICE_PAUSE alc.DevicePause
#endif

uint8_t EventThread::keyStates[];
EventThread::ControllerState EventThread::controllerState;
EventThread::MouseState EventThread::mouseState;
EventThread::TouchState EventThread::touchState;
SDL_AtomicInt EventThread::verticalScrollDistance;

/* User event codes */
enum
{
    REQUEST_SETFULLSCREEN = 0,
    REQUEST_WINRESIZE,
    REQUEST_WINREPOSITION,
    REQUEST_WINRENAME,
    REQUEST_WINCENTER,
    REQUEST_MESSAGEBOX,
    REQUEST_SETCURSORVISIBLE,
    
    REQUEST_TEXTMODE,
    
    REQUEST_SETTINGS,
    REQUEST_NEW_WINDOW,
    REQUEST_DESTROY_WINDOW,
    REQUEST_WINDOW_VISIBLE,
    
    UPDATE_FPS,
    UPDATE_SCREEN_RECT,
    
    EVENT_COUNT
};
volatile SDL_Window *new_window;

static uint32_t usrIdStart;

bool EventThread::allocUserEvents()
{
    usrIdStart = SDL_RegisterEvents(EVENT_COUNT);
    
    if (usrIdStart == (uint32_t) -1)
        return false;
    
    return true;
}

EventThread::EventThread()
: ctrl(0),
fullscreen(false),
showCursor(false)
{
    textInputLock = SDL_CreateMutex();
}

EventThread::~EventThread()
{
    SDL_DestroyMutex(textInputLock);
}

SDL_TimerID hideCursorTimerID = 0;
Uint32 cursorTimerCallback( void* param, SDL_TimerID timerID, Uint32 interval)
{
	EventThread *ethread = static_cast<EventThread*>(param);
	hideCursorTimerID = 0;
	ethread->requestShowCursor(ethread->getShowCursor());
	return 0;
}
void EventThread::cursorTimer()
{
	SDL_RemoveTimer(hideCursorTimerID);
	hideCursorTimerID = SDL_AddTimer(500, cursorTimerCallback, this);
}

void EventThread::process(RGSSThreadData &rtData)
{
    SDL_Event event;
    SDL_Window *win = rtData.window;
    UnidirMessage<Vec2i> &windowSizeMsg = rtData.windowSizeMsg;
    UnidirMessage<Vec2i> &drawableSizeMsg = rtData.drawableSizeMsg;
    
    #ifndef MKXPZ_NO_OPENAL
    initALCFunctions(rtData.alcDev);
    #endif
    
    // XXX this function breaks input focus on OSX
#ifndef SDL_PLATFORM_APPLE
    SDL_SetEventFilter(eventFilter, &rtData);
#endif
    
    fullscreen = rtData.config.fullscreen;
    int toggleFSMod = rtData.config.anyAltToggleFS ? SDL_KMOD_ALT : SDL_KMOD_LALT;
    
    bool displayingFPS = rtData.config.displayFPS;
    
    if (displayingFPS || rtData.config.printFPS)
        fps.sendUpdates.set();

    bool cursorInWindow = false;
    /* Will be updated eventually */
    SDL_Rect gameScreen = { 0, 0, 0, 0 };
    
    /* SDL doesn't send an initial FOCUS_GAINED event */
    bool windowFocused = true;
    
    bool terminate = false;
    
#ifdef MKXPZ_BUILD_XCODE
    SDL_AddGamepadMappingsFromFile(mkxp_fs::getPathForAsset("gamecontrollerdb", "txt").c_str());
#else
    SDL_AddGamepadMappingsFromIO(
        SDL_IOFromConstMem(___assets_gamecontrollerdb_txt, ___assets_gamecontrollerdb_txt_len),
    1);
#endif
    
    SDL_UpdateJoysticks();
    int joystickCount;
    SDL_GetJoysticks(&joystickCount);
    if (joystickCount > 0 && SDL_IsGamepad(0)) {
            ctrl = SDL_OpenGamepad(0);
    }
    
    char buffer[128];
    
    char pendingTitle[128];
    bool havePendingTitle = false;
    
    bool resetting = false;
    
    int winW, winH;
    int i, rc;
    
    SDL_DisplayMode dm = {0};
    
    SDL_GetWindowSize(win, &winW, &winH);
    
    // Just in case it's started when the window is opened
    // for some dumb reason
    SDL_StopTextInput(win);
    
    textInputBuffer.clear();
#ifndef MKXPZ_BUILD_XCODE
    SettingsMenu *sMenu = 0;
#else
    // Will always be 0
    void *sMenu = 0;
#endif
    
    while (true)
    {
        if (!SDL_WaitEvent(&event))
        {
            Debug() << "EventThread: Event error";
            break;
        }
#ifndef MKXPZ_BUILD_XCODE
        if (sMenu && sMenu->onEvent(event))
        {
            if (sMenu->destroyReq())
            {
                delete sMenu;
                sMenu = 0;
                
                updateCursorState(cursorInWindow && windowFocused, gameScreen);
            }
            
            continue;
        }
#endif
        
        /* Preselect and discard unwanted events here */
        switch (event.type)
        {
            case SDL_EVENT_MOUSE_BUTTON_DOWN :
            case SDL_EVENT_MOUSE_BUTTON_UP :
            case SDL_EVENT_MOUSE_MOTION :
                if (event.button.which == SDL_TOUCH_MOUSEID)
                    continue;
                break;
                
            case SDL_EVENT_FINGER_DOWN :
            case SDL_EVENT_FINGER_UP :
            case SDL_EVENT_FINGER_MOTION :
                if (event.tfinger.fingerID >= MAX_FINGERS)
                    continue;
                break;
        }
        
        /* Now process the rest */
        switch (event.type)
        {
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED :
                winW = event.window.data1;
                winH = event.window.data2;
                
                int drwW, drwH;
                SDL_GetWindowSizeInPixels(win, &drwW, &drwH);
                
                windowSizeMsg.post(Vec2i(winW, winH));
                drawableSizeMsg.post(Vec2i(drwW, drwH));
                resetInputStates();
                break;
                
            case SDL_EVENT_WINDOW_MOUSE_ENTER :
                cursorInWindow = true;
                mouseState.inWindow = true;
                updateCursorState(cursorInWindow && windowFocused && !sMenu, gameScreen);
                
                break;
                
            case SDL_EVENT_WINDOW_MOUSE_LEAVE :
                cursorInWindow = false;
                mouseState.inWindow = false;
                updateCursorState(cursorInWindow && windowFocused && !sMenu, gameScreen);
                
                break;
                
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED :
                if (rtData.allowExit) {
                    terminate = true;
                } else {
                    rtData.triedExit.set();
                }
                
                break;
                
            case SDL_EVENT_WINDOW_FOCUS_GAINED :
                windowFocused = true;
                updateCursorState(cursorInWindow && windowFocused && !sMenu, gameScreen);
                
                break;
                
            case SDL_EVENT_WINDOW_FOCUS_LOST :
                windowFocused = false;
                updateCursorState(cursorInWindow && windowFocused && !sMenu, gameScreen);
                resetInputStates();
                
                break;

            case SDL_EVENT_TEXT_INPUT :
                lockText(true);
                if (textInputBuffer.size() < 512 && acceptingTextInput) {
                    textInputBuffer += event.text.text;
                }
                lockText(false);
                break;
                
            case SDL_EVENT_QUIT :
                if (rtData.allowExit) {
                    terminate = true;
                    Debug() << "EventThread termination requested";
                } else {
                    rtData.triedExit.set();
                }
                
                break;
                
            case SDL_EVENT_KEY_DOWN :
                if (event.key.scancode == SDL_SCANCODE_RETURN &&
                    (event.key.mod & toggleFSMod))
                {
                    setFullscreen(win, !fullscreen);
                    if (!fullscreen && havePendingTitle)
                    {
                        SDL_SetWindowTitle(win, pendingTitle);
                        pendingTitle[0] = '\0';
                        havePendingTitle = false;
                    }
                    
                    break;
                }
                
                if (event.key.scancode == SDL_SCANCODE_F1 && rtData.config.enableSettings)
                {
                    // Do not open settings menu until initializing shared state.
                    // Opening before initializing shared state will crash (segmentation fault).
                    if (!shState)
                    {
                        break;
                    }

#ifndef MKXPZ_BUILD_XCODE
                    if (!sMenu)
                    {
                        sMenu = new SettingsMenu(rtData);
                        updateCursorState(false, gameScreen);
                    }
                    
                    sMenu->raise();
#else
                    openSettingsWindow();
#endif
                }
                
                if (event.key.scancode == SDL_SCANCODE_F2)
                {
                    if (!displayingFPS)
                    {
                        
                        fps.sendUpdates.set();
                        displayingFPS = true;
                    }
                    else
                    {
                        displayingFPS = false;
                        
                        if (!rtData.config.printFPS)
                            fps.sendUpdates.clear();
                        
                        if (fullscreen)
                        {
                            /* Prevent fullscreen flicker */
                            strncpy(pendingTitle, rtData.config.windowTitle.c_str(),
                                    sizeof(pendingTitle));
                            havePendingTitle = true;
                            
                            break;
                        }
                        
                        SDL_SetWindowTitle(win, rtData.config.windowTitle.c_str());
                    }
                    
                    break;
                }
                
                if (event.key.scancode == SDL_SCANCODE_F12)
                {
                    if (!rtData.config.enableReset)
                        break;
                    
                    if (resetting)
                        break;
                    
                    resetting = true;
                    rtData.rqResetFinish.clear();
                    rtData.rqReset.set();
                    break;
                }

                if (acceptingTextInput && event.key.key == SDLK_BACKSPACE) {
                    // remove one unicode character
                    lockText(true);
                    while (textInputBuffer.length() != 0 && (textInputBuffer.back() & 0xc0) == 0x80) {
                        textInputBuffer.pop_back();
                    }
                    if (textInputBuffer.length() != 0) {
                        textInputBuffer.pop_back();
                    }
                    lockText(false);
                }
                
                keyStates[event.key.scancode] = true;
                break;
                
            case SDL_EVENT_KEY_UP :
                if (event.key.scancode == SDL_SCANCODE_F12)
                {
                    if (!rtData.config.enableReset)
                        break;
                    
                    resetting = false;
                    rtData.rqResetFinish.set();
                    break;
                }
                
                keyStates[event.key.scancode] = false;
                break;
                
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                controllerState.buttons[event.gbutton.button] = true;
                break;
                
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                controllerState.buttons[event.gbutton.button] = false;
                break;
                
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                controllerState.axes[event.gaxis.axis] = event.gaxis.value;
                break;
                
            case SDL_EVENT_GAMEPAD_ADDED:
                if (event.cdevice.which > 0)
                    break;
                
                ctrl = SDL_OpenGamepad(0);
                break;
                
            case SDL_EVENT_GAMEPAD_REMOVED:
                resetInputStates();
                ctrl = 0;
                break;
                
            case SDL_EVENT_MOUSE_BUTTON_DOWN :
                mouseState.buttons[event.button.button] = true;
                break;
                
            case SDL_EVENT_MOUSE_BUTTON_UP :
                mouseState.buttons[event.button.button] = false;
                break;
                
            case SDL_EVENT_MOUSE_MOTION :
                mouseState.x = event.motion.x;
                mouseState.y = event.motion.y;
                cursorTimer();
                updateCursorState(cursorInWindow, gameScreen);
                break;
                
            case SDL_EVENT_MOUSE_WHEEL :
                /* Only consider vertical scrolling for now */
                SDL_AtomicAdd(&verticalScrollDistance, event.wheel.y);
                
            case SDL_EVENT_FINGER_DOWN :
                i = event.tfinger.fingerID;
                touchState.fingers[i].down = true;
                
            case SDL_EVENT_FINGER_MOTION :
                i = event.tfinger.fingerID;
                touchState.fingers[i].x = event.tfinger.x * winW;
                touchState.fingers[i].y = event.tfinger.y * winH;
                break;
                
            case SDL_EVENT_FINGER_UP :
                i = event.tfinger.fingerID;
                memset(&touchState.fingers[i], 0, sizeof(touchState.fingers[0]));
                break;
                
            default :
                /* Handle user events */
                switch(event.type - usrIdStart)
                {
                    case REQUEST_SETFULLSCREEN :
                        setFullscreen(win, static_cast<bool>(event.user.code));
                        break;
                        
                    case REQUEST_WINRESIZE :
                        SDL_SetWindowSize(win, event.window.data1, event.window.data2);
                        rtData.rqWindowAdjust.clear();
                        break;
                        
                    case REQUEST_WINREPOSITION :
                        SDL_SetWindowPosition(win, event.window.data1, event.window.data2);
                        rtData.rqWindowAdjust.clear();
                        break;
                        
                    case REQUEST_WINCENTER : {
                            dm = *SDL_GetDesktopDisplayMode(SDL_GetDisplayForWindow(win));
                            SDL_Rect rect;
                            SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(win), &rect);
                            if (!rc)
                                SDL_SetWindowPosition(win,
                                                    rect.x + (dm.w / 2) - (winW / 2),
                                                    rect.y + (dm.h / 2) - (winH / 2));
                            rtData.rqWindowAdjust.clear();
                        }
                        break;
                        
                    case REQUEST_WINRENAME :
                        rtData.config.windowTitle = (const char*)event.user.data1;
                        SDL_SetWindowTitle(win, rtData.config.windowTitle.c_str());
                        break;
                        
                    case REQUEST_TEXTMODE :
                        if (event.user.code)
                        {
                            SDL_StartTextInput(win);
                            acceptingTextInput = true;
                        }
                        else
                        {
                            SDL_StopTextInput(win);
                            acceptingTextInput = false;
                        }
                        break;
                        
                    case REQUEST_MESSAGEBOX :
                    {
#ifndef SDL_PLATFORM_APPLE
                        // Try to format the message with additional newlines
                        std::string message = copyWithNewlines((const char*) event.user.data1,
                                                               70);
                        SDL_ShowSimpleMessageBox(event.user.code,
                                                 rtData.config.windowTitle.c_str(),
                                                 message.c_str(), win);
#else
                        SDL_ShowSimpleMessageBox(event.user.code,
                                                 rtData.config.windowTitle.c_str(),
                                                 (const char*)event.user.data1, win);
#endif
                        free(event.user.data1);
                        msgBoxDone.set();
                        break;
                    }
                    case REQUEST_SETCURSORVISIBLE :
                        showCursor = event.user.code;
                        updateCursorState(cursorInWindow, gameScreen);
                        break;
                        
                    case REQUEST_SETTINGS :
#ifndef MKXPZ_BUILD_XCODE
                        if (!sMenu)
                        {
                            sMenu = new SettingsMenu(rtData);
                            updateCursorState(false, gameScreen);
                        }
                        
                        sMenu->raise();
#else
                        openSettingsWindow();
#endif
                        break;
                    case REQUEST_NEW_WINDOW:
                    {
                        const CreateWindowArgs *args = static_cast<const CreateWindowArgs*>(event.user.data1);
                        SDL_PropertiesID props = SDL_CreateProperties();
                        SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, args->name);
                        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, args->x);
                        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, args->y);
                        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, args->w);
                        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, args->h);
                        SDL_SetNumberProperty(props, "flags", args->flags);
                        new_window = SDL_CreateWindowWithProperties(props);
                        SDL_DestroyProperties(props);
                        break;
                    }
                    case REQUEST_DESTROY_WINDOW:
                    {
                        SDL_DestroyWindow(static_cast<SDL_Window*>(event.user.data1));
                        break;
                    }
                    case REQUEST_WINDOW_VISIBLE:
                    {
                        SDL_Window *window = static_cast<SDL_Window*>(event.user.data1);
                        if (event.user.data2)
                            SDL_ShowWindow(window);
                        else
                            SDL_HideWindow(window);
                        break;
                    }
                        
                    case UPDATE_FPS :
                        if (rtData.config.printFPS)
                            Debug() << "FPS:" << event.user.code;
                        
                        if (!fps.sendUpdates)
                            break;
                        
                        snprintf(buffer, sizeof(buffer), "%s - %d FPS",
                                 rtData.config.windowTitle.c_str(), event.user.code);
                        
                        /* Updating the window title in fullscreen
                         * mode seems to cause flickering */
                        if (fullscreen)
                        {
                            strncpy(pendingTitle, buffer, sizeof(pendingTitle));
                            havePendingTitle = true;
                            
                            break;
                        }
                        
                        SDL_SetWindowTitle(win, buffer);
                        break;
                        
                    case UPDATE_SCREEN_RECT :
                        gameScreen.x = event.user.windowID;
                        gameScreen.y = event.user.code;
                        gameScreen.w = reinterpret_cast<intptr_t>(event.user.data1);
                        gameScreen.h = reinterpret_cast<intptr_t>(event.user.data2);
                        updateCursorState(cursorInWindow, gameScreen);
                        
                        break;
                }
        }
        
        if (terminate)
            break;
    }
    
    /* Just in case */
    rtData.syncPoint.resumeThreads();
    
    if (SDL_GamepadConnected(ctrl))
        SDL_CloseGamepad(ctrl);
    
#ifndef MKXPZ_BUILD_XCODE
    delete sMenu;
#endif
}

int EventThread::eventFilter(void *data, SDL_Event *event)
{
    RGSSThreadData &rtData = *static_cast<RGSSThreadData*>(data);
    
    switch (event->type)
    {
        case SDL_EVENT_WILL_ENTER_BACKGROUND :
            Debug() << "SDL_EVENT_WILL_ENTER_BACKGROUND";
            
            #ifndef MKXPZ_NO_OPENAL
            if (HAVE_ALC_DEVICE_PAUSE)
                alc.DevicePause(rtData.alcDev);
            #endif
            
            rtData.syncPoint.haltThreads();
            
            return 0;
            
        case SDL_EVENT_DID_ENTER_BACKGROUND :
            Debug() << "SDL_EVENT_DID_ENTER_BACKGROUND";
            return 0;
            
        case SDL_EVENT_WILL_ENTER_FOREGROUND :
            Debug() << "SDL_EVENT_WILL_ENTER_FOREGROUND";
            return 0;
            
        case SDL_EVENT_DID_ENTER_FOREGROUND :
            Debug() << "SDL_EVENT_DID_ENTER_FOREGROUND";
            
            #ifndef MKXPZ_NO_OPENAL
            if (HAVE_ALC_DEVICE_PAUSE)
                alc.DeviceResume(rtData.alcDev);
            #endif
            
            rtData.syncPoint.resumeThreads();
            
            return 0;
            
        case SDL_EVENT_TERMINATING :
            Debug() << "SDL_EVENT_TERMINATING";
            return 0;
            
        case SDL_EVENT_LOW_MEMORY :
            Debug() << "SDL_EVENT_LOW_MEMORY";
            return 0;
        /* Workaround for Windows pausing on drag */
        default:
            // sdl3 removed SDL_WINDOWEVENT, so we just check if the type is in the expected range
            if (event->type >= SDL_EVENT_WINDOW_FIRST && event->type <= SDL_EVENT_WINDOW_LAST) {
                {
                    unsigned int win_id = SDL_GetWindowID(rtData.window);
                    if (win_id != event->window.windowID) // filter out events from other windows
                        return 0;
                }

                if (event->type == SDL_EVENT_WINDOW_MOVED)
                {
                    if (shState != NULL && shState->rgssVersion > 0)
                    {
                        shState->oneshot().setWindowPos(event->window.data1, event->window.data2);
                        // shState->graphics().update(false);
                    }
                    return 0;
                }
                return 1;
            }
            
            //	case SDL_EVENT_RENDER_TARGETS_RESET :
            //		Debug() << "****** SDL_EVENT_RENDER_TARGETS_RESET";
            //		return 0;
            
            //	case SDL_EVENT_RENDER_DEVICE_RESET :
            //		Debug() << "****** SDL_EVENT_RENDER_DEVICE_RESET";
            //		return 0;
            
    }
    
    return 1;
}

void EventThread::cleanup()
{
    SDL_Event event;
    
    while (SDL_PollEvent(&event))
        if ((event.type - usrIdStart) == REQUEST_MESSAGEBOX)
            free(event.user.data1);
}

void EventThread::resetInputStates()
{
    memset(&keyStates, 0, sizeof(keyStates));
    memset(&controllerState, 0, sizeof(controllerState));
    memset(&mouseState.buttons, 0, sizeof(mouseState.buttons));
    memset(&touchState, 0, sizeof(touchState));
}

void EventThread::setFullscreen(SDL_Window *win, bool mode)
{
    SDL_SetWindowFullscreen
    (win, mode ? SDL_WINDOW_FULLSCREEN : 0);
    fullscreen = mode;
}

void EventThread::updateCursorState(bool inWindow,
                                    const SDL_Rect &screen)
{
    SDL_Point pos = { mouseState.x, mouseState.y };
    bool inScreen = inWindow && SDL_PointInRect(&pos, &screen);
    
    if (inScreen)
        if (showCursor || hideCursorTimerID)
            SDL_ShowCursor();
        else
            SDL_HideCursor();
    else
        SDL_ShowCursor();
}

void EventThread::requestTerminate()
{
    SDL_Event event;
    event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&event);
}

void EventThread::requestFullscreenMode(bool mode)
{
    if (mode == fullscreen)
        return;
    
    SDL_Event event;
    event.type = usrIdStart + REQUEST_SETFULLSCREEN;
    event.user.code = static_cast<Sint32>(mode);
    SDL_PushEvent(&event);
}

void EventThread::requestWindowResize(int width, int height)
{
    shState->rtData().rqWindowAdjust.set();
    SDL_Event event;
    event.type = usrIdStart + REQUEST_WINRESIZE;
    event.window.data1 = width;
    event.window.data2 = height;
    SDL_PushEvent(&event);
}

void EventThread::requestWindowReposition(int x, int y)
{
    shState->rtData().rqWindowAdjust.set();
    SDL_Event event;
    event.type = usrIdStart + REQUEST_WINREPOSITION;
    event.window.data1 = x;
    event.window.data2 = y;
    SDL_PushEvent(&event);
}

void EventThread::requestWindowCenter()
{
    shState->rtData().rqWindowAdjust.set();
    SDL_Event event;
    event.type = usrIdStart + REQUEST_WINCENTER;
    SDL_PushEvent(&event);
}

void EventThread::requestWindowRename(const char *title)
{
    SDL_Event event;
    event.type = usrIdStart + REQUEST_WINRENAME;
    event.user.data1 = (void*)title;
    SDL_PushEvent(&event);
}

void EventThread::requestShowCursor(bool mode)
{
    SDL_Event event;
    event.type = usrIdStart + REQUEST_SETCURSORVISIBLE;
    event.user.code = mode;
    SDL_PushEvent(&event);
}

void EventThread::requestTextInputMode(bool mode)
{
    SDL_Event event;
    event.type = usrIdStart + REQUEST_TEXTMODE;
    event.user.code = mode;
    SDL_PushEvent(&event);
}

void EventThread::requestSettingsMenu()
{
    SDL_Event event;
    event.type = usrIdStart + REQUEST_SETTINGS;
    SDL_PushEvent(&event);
}

SDL_Window *EventThread::requestNewWindow(const CreateWindowArgs *args)
{
    SDL_Event event;
    event.type = usrIdStart + REQUEST_NEW_WINDOW;
    event.user.data1 = (void*)args;
    new_window = nullptr; // reset 
    SDL_PushEvent(&event);
    while (!new_window)
        // we want to delay ~0.1ms
        // ideally the delay should be long enough that the window is created in one loop, 
        // but short enough that we aren't waiting for a long time after the window is created
        SDL_DelayNS(10000);
    return (SDL_Window*) new_window;
}

void EventThread::requestWindowVisible(SDL_Window *window, bool visible)
{
    SDL_Event event;
    event.type = usrIdStart + REQUEST_WINDOW_VISIBLE;
    event.user.data1 = window;
    event.user.data2 = (void*) visible;

    SDL_PushEvent(&event);
}

void EventThread::destroySDLWindow(SDL_Window *window)
{
    SDL_Event event;
    event.type = usrIdStart + REQUEST_DESTROY_WINDOW;
    event.user.data1 = window;
    SDL_PushEvent(&event);
}

void EventThread::showMessageBox(const char *body, int flags)
{
    msgBoxDone.clear();
    
    // mkxp has already been asked to quit.
    // Don't break things if the window wants to close
    if (shState->rtData().rqTerm)
        return;
    
    SDL_Event event;
    event.user.code = flags;
    event.user.data1 = strdup(body);
    event.type = usrIdStart + REQUEST_MESSAGEBOX;
    SDL_PushEvent(&event);
    
    /* Keep repainting screen while box is open */
    try{
        shState->graphics().repaintWait(msgBoxDone);
    }catch(...){}
    /* Prevent endless loops */
    resetInputStates();
}

bool EventThread::getFullscreen() const
{
    return fullscreen;
}

bool EventThread::getShowCursor() const
{
    return showCursor;
}

bool EventThread::getControllerConnected() const
{
    return ctrl != 0;
}

SDL_Gamepad *EventThread::controller() const
{
    return ctrl;
}

void EventThread::notifyFrame()
{
#ifdef MKXPZ_BUILD_XCODE
    uint32_t frames = round(shState->graphics().averageFrameRate());
    updateTouchBarFPSDisplay(frames);
#endif
    if (!fps.sendUpdates)
        return;
    
    SDL_Event event;
#ifdef MKXPZ_BUILD_XCODE
    event.user.code = frames;
#else
    event.user.code = round(shState->graphics().averageFrameRate());
#endif
    event.user.type = usrIdStart + UPDATE_FPS;
    SDL_PushEvent(&event);
}

void EventThread::notifyGameScreenChange(const SDL_Rect &screen)
{
    /* We have to get a bit hacky here to fit the rectangle
     * data into the user event struct */
    SDL_Event event;
    event.type = usrIdStart + UPDATE_SCREEN_RECT;
    event.user.windowID = screen.x;
    event.user.code = screen.y;
    event.user.data1 = reinterpret_cast<void*>(screen.w);
    event.user.data2 = reinterpret_cast<void*>(screen.h);
    SDL_PushEvent(&event);
}

void EventThread::lockText(bool lock)
{
    lock ? SDL_LockMutex(textInputLock) : SDL_UnlockMutex(textInputLock);
}

void SyncPoint::haltThreads()
{
    if (mainSync.locked)
        return;
    
    /* Lock the reply sync first to avoid races */
    reply.lock();
    
    /* Lock main sync and sleep until RGSS thread
     * reports back */
    mainSync.lock();
    reply.waitForUnlock();
    
    /* Now that the RGSS thread is asleep, we can
     * safely put the other threads to sleep as well
     * without causing deadlocks */
    secondSync.lock();
}

void SyncPoint::resumeThreads()
{
    if (!mainSync.locked)
        return;
    
    mainSync.unlock(false);
    secondSync.unlock(true);
}

bool SyncPoint::mainSyncLocked()
{
    return mainSync.locked;
}

void SyncPoint::waitMainSync()
{
    reply.unlock(false);
    mainSync.waitForUnlock();
}

void SyncPoint::passSecondarySync()
{
    if (!secondSync.locked)
        return;
    
    secondSync.waitForUnlock();
}

SyncPoint::Util::Util()
{
    mut = SDL_CreateMutex();
    cond = SDL_CreateCondition();
}

SyncPoint::Util::~Util()
{
    SDL_DestroyCondition(cond);
    SDL_DestroyMutex(mut);
}

void SyncPoint::Util::lock()
{
    locked.set();
}

void SyncPoint::Util::unlock(bool multi)
{
    locked.clear();
    
    if (multi)
        SDL_BroadcastCondition(cond);
    else
        SDL_SignalCondition(cond);
}

void SyncPoint::Util::waitForUnlock()
{
    SDL_LockMutex(mut);
    
    while (locked)
        SDL_WaitCondition(cond, mut);
    
    SDL_UnlockMutex(mut);
}
