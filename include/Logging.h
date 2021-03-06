#pragma once

void CustomPrinter(int level, const char *format, va_list args);
void CustomPrintString(const char* string);

void Printf(const char* format, ...);
void Messagef(const char* format, ...);
void Displayf(const char* format, ...);
void Warningf(const char* format, ...);
void Errorf(const char* format, ...);

[[noreturn]] void Quitf(const char* format, ...);
[[noreturn]] void Abortf(const char* format, ...);

void ageDebug(int enabled, char const * format, ...);

void DebugLogInit(bool append);
void DebugLogShutdown();

static_var(0x5CECEC, int, PrinterFlags);
static_var(0x5CED24, void(*)(int, const char*, va_list), Printer);
static_var(0x5CECF0, void(*)(const char *), PrintString);
