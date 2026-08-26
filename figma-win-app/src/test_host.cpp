#include <windows.h>
#include <cstdio>
#include <atomic>

volatile long gHealth = 100;
volatile float gSpeed = 6.0f;
volatile unsigned long long gTicks = 0;

int main(){
    SetConsoleTitleW(L"NeonDebugTarget.exe");
    std::printf("NeonDebugTarget.exe running. PID=%lu\n", GetCurrentProcessId());
    for(;;){
        ++gTicks;
        if(gHealth < 0) gHealth = 0;
        Sleep(1000);
    }
}
