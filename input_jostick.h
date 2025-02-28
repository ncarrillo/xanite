#ifndef INPUT_JOYSTICK_H
#define INPUT_JOYSTICK_H

#include <android/input.h>
#include <jni.h>
#include <android/log.h>

#define LOG_TAG "JoystickInput"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

enum JoystickButton {
    BUTTON_A    = 304,
    BUTTON_B    = 305,
    BUTTON_X    = 307,
    BUTTON_Y    = 308,
    BUTTON_LB   = 310,
    BUTTON_RB   = 311,
    BUTTON_BACK = 314,
    BUTTON_START= 315,
    BUTTON_LS   = 317,
    BUTTON_RS   = 318,
    BUTTON_LT   = 322,
    BUTTON_RT   = 323
};

struct JoystickState {
    bool buttons[12];
    float axisX;
    float axisY;
};

static JoystickState joystickState = { { false }, 0.0f, 0.0f };

static int mapKeyCodeToIndex(int keyCode) {
    switch(keyCode) {
        case BUTTON_A:    return 0;
        case BUTTON_B:    return 1;
        case BUTTON_X:    return 2;
        case BUTTON_Y:    return 3;
        case BUTTON_LB:   return 4;
        case BUTTON_RB:   return 5;
        case BUTTON_BACK: return 6;
        case BUTTON_START:return 7;
        case BUTTON_LS:   return 8;
        case BUTTON_RS:   return 9;
        case BUTTON_LT:   return 10;
        case BUTTON_RT:   return 11;
        default:          return -1;
    }
}

static void handleJoystickEvent(AInputEvent* event) {
    if (!event) {
        LOGE("Null event received");
        return;
    }
    int32_t eventType = AInputEvent_getType(event);
    if (eventType == AINPUT_EVENT_TYPE_KEY) {
        int32_t action = AKeyEvent_getAction(event);
        int32_t keyCode = AKeyEvent_getKeyCode(event);
        int index = mapKeyCodeToIndex(keyCode);
        if (index >= 0) {
            if (action == AKEY_EVENT_ACTION_DOWN) {
                joystickState.buttons[index] = true;
                LOGI("Button pressed: %d", keyCode);
            } else if (action == AKEY_EVENT_ACTION_UP) {
                joystickState.buttons[index] = false;
                LOGI("Button released: %d", keyCode);  //Ziunx
            }
        }
    } else if (eventType == AINPUT_EVENT_TYPE_MOTION) {
        int32_t motionAction = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
        if (motionAction == AMOTION_EVENT_ACTION_MOVE) {
            float x = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_X, 0);
            float y = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_Y, 0);
            joystickState.axisX = x;
            joystickState.axisY = y;
            LOGI("Joystick axis moved: X = %f, Y = %f", x, y);
        }
    } else {
        LOGI("Unhandled event type: %d", eventType);
    }
}

#endif 