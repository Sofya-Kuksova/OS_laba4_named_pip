#include <windows.h>
#include <iostream>
#include <string>
#include <limits>

HANDLE hPipe = INVALID_HANDLE_VALUE;
HANDLE hEvent = NULL;
OVERLAPPED ov = {0};

void CreatePipeInstance() {
    const TCHAR* name = TEXT("\\\\.\\pipe\\PipeSrv");
    hPipe = CreateNamedPipe(
        name,
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,       // max instances
        1024,    // out buffer
        1024,    // in buffer
        0,
        NULL
    );
    if (hPipe == INVALID_HANDLE_VALUE) {
        std::cerr << "CreateNamedPipe failed, GLE=" << GetLastError() << "\n";
        exit(1);
    }
    std::cout << "Pipe created: " << name << "\n";
}

int ReadMenuChoice() {
    while (true) {
        std::string line;
        std::getline(std::cin, line);
        try {
            int c = std::stoi(line);
            return c;
        } catch (...) {
            std::cout << "Invalid input, please enter a number: ";
        }
    }
}

void ConnectClient() {
    // если от предыдущего раза есть событие — закроем
    if (hEvent) {
        CloseHandle(hEvent);
        ZeroMemory(&ov, sizeof(ov));
    }
    hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    ov.hEvent = hEvent;

    BOOL res = ConnectNamedPipe(hPipe, &ov);
    if (!res) {
        DWORD e = GetLastError();
        if (e == ERROR_IO_PENDING) {
            std::cout << "Waiting for client...\n";
            WaitForSingleObject(hEvent, INFINITE);
        } else if (e != ERROR_PIPE_CONNECTED) {
            std::cerr << "ConnectNamedPipe failed, GLE=" << e << "\n";
            exit(1);
        }
    }
    std::cout << "Client connected.\n";
}

void WriteAsync() {
    std::cout << "Enter message: ";
    std::string msg;
    std::getline(std::cin, msg);

    ZeroMemory(&ov, sizeof(ov));
    ov.hEvent = hEvent;

    DWORD written = 0;
    BOOL res = WriteFile(hPipe, msg.c_str(), (DWORD)msg.size()+1, NULL, &ov);
    if (!res && GetLastError() == ERROR_IO_PENDING) {
        WaitForSingleObject(hEvent, INFINITE);
        GetOverlappedResult(hPipe, &ov, &written, FALSE);
    } else if (res) {
        GetOverlappedResult(hPipe, &ov, &written, TRUE);
    } else {
        std::cerr << "WriteFile failed, GLE=" << GetLastError() << "\n";
        return;
    }
    std::cout << "Bytes written successfully" << "\n";
}

void DisconnectClient() {
    DisconnectNamedPipe(hPipe);
    std::cout << "Client disconnected.\n";
    // очистим event, overlapped — на следующий ConnectClient будет создан новый
    CloseHandle(hEvent);
    hEvent = NULL;
    ZeroMemory(&ov, sizeof(ov));
}

int main() {
    CreatePipeInstance();

    bool run = true;
    while (run) {
        std::cout << "\n--- Server Menu ---\n"
                  << "1. Connect to client\n"
                  << "2. Send message\n"
                  << "3. Disconnect client\n"
                  << "4. Exit\n"
                  << "Select option: ";

        int choice = ReadMenuChoice();
        switch (choice) {
            case 1: ConnectClient();         break;
            case 2: WriteAsync();           break;
            case 3: DisconnectClient();     break;
            case 4: run = false;            break;
            default: std::cout << "Invalid choice.\n";
        }
    }

    if (hEvent) CloseHandle(hEvent);
    if (hPipe != INVALID_HANDLE_VALUE) CloseHandle(hPipe);
    return 0;
}
