#ifndef EMULATOR_H
#define EMULATOR_H

#include <android/native_window.h>
#include <string>

class Emulator {
public:
    // Constructor declaration
    Emulator(ANativeWindow* window, const char* isoPath, const char* biosPath, const char* mcpxPath, const char* hddPath);
    
    // Method to initialize the emulator
    void initialize(const char* isoPath, const char* biosPath, const char* mcpxPath, const char* hddPath);

private:
    ANativeWindow* mWindow;
    std::string mIsoPath;
    std::string mBiosPath;
    std::string mMcpxPath;
    std::string mHddPath;
};

#endif // EMULATOR_H