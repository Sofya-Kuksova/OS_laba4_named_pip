#include <windows.h>
#include <iostream>
#include <string>
#include <algorithm>


HANDLE hPipe = INVALID_HANDLE_VALUE;

int ReadMenuChoice() {
    while (true) {
        std::string line;
        std::getline(std::cin, line);
        try {
            return std::stoi(line);
        } catch (...) {
            std::cout << "Invalid input, please enter a number: ";
        }
    }
}

void ConnectToServer() {
    const TCHAR* name = TEXT("\\\\.\\pipe\\PipeSrv");

    if (hPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(hPipe);
        hPipe = INVALID_HANDLE_VALUE;
    }

    // Ждём не дольше 5 секунд
    if (!WaitNamedPipe(name, 2000 /* ms */)) {
        std::cerr << "Timeout waiting for pipe (2s). GLE=" 
                  << GetLastError() << "\n";
        return;
    }

    hPipe = CreateFile(
        name,
        GENERIC_READ | GENERIC_WRITE,
        0, NULL,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        NULL
    );
    
    if (hPipe == INVALID_HANDLE_VALUE) {
        std::cerr << "CreateFile failed, GLE=" << GetLastError() << "\n";
    } else {
        std::cout << "Connected to server.\n";
    }
}

VOID CALLBACK ReadCompleted(DWORD err, DWORD bytes, LPOVERLAPPED lpOv) {
    char* buf = reinterpret_cast<char*>(lpOv) + sizeof(OVERLAPPED);
    if (err == 0 && bytes > 0) {
        buf[bytes] = '\0';
        std::cout << "Received: " << buf << "\n";
    } else {
        std::cerr << "Read error or no data. code=" << err << "\n";
    }
    // удалим память, выделенную под OVERLAPPED+буфер
    HeapFree(GetProcessHeap(), 0, lpOv);
}

void ReadAsync() {
    if (hPipe == INVALID_HANDLE_VALUE) {
        std::cout << "You must connect first.\n";
        return;
    }

    // 1) Проверяем, есть ли данные в канале
    DWORD totalBytesAvail = 0;
    BOOL okPeek = PeekNamedPipe(
        hPipe,
        NULL,       // не читаем данные
        0,
        NULL,
        &totalBytesAvail,
        NULL
    );
    if (!okPeek) {
        std::cerr << "PeekNamedPipe failed, GLE=" << GetLastError() << "\n";
        return;
    }
    if (totalBytesAvail == 0) {
        std::cout << "No data available right now. Try later.\n";
        return;
    }

    // 2) Если данные есть — продолжаем асинхронное чтение
    const DWORD bufSize = 1024;
    BYTE* block = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                   sizeof(OVERLAPPED) + bufSize);
    auto* ov = reinterpret_cast<OVERLAPPED*>(block);
    char* buffer = reinterpret_cast<char*>(block + sizeof(OVERLAPPED));

    BOOL okRead = ReadFileEx(
        hPipe,
        buffer,
        std::min(bufSize - 1, totalBytesAvail),  // читаем не больше, чем есть
        ov,
        [](DWORD err, DWORD bytes, LPOVERLAPPED lpOv) {
            auto* buf = reinterpret_cast<char*>(
                reinterpret_cast<BYTE*>(lpOv) + sizeof(OVERLAPPED));
            if (err == 0 && bytes > 0) {
                buf[bytes] = '\0';
                std::cout << "Received: " << buf << "\n";
            } else {
                std::cerr << "Read error or no data. code=" << err << "\n";
            }
            HeapFree(GetProcessHeap(), 0, lpOv);
        }
    );
    if (!okRead) {
        std::cerr << "ReadFileEx failed, GLE=" << GetLastError() << "\n";
        HeapFree(GetProcessHeap(), 0, block);
        return;
    }

    std::cout << "Waiting for data...\n";
    SleepEx(INFINITE, TRUE);
}

int main() {
    bool run = true;
    while (run) {
        std::cout << "\n--- Client Menu ---\n"
                  << "1. Connect to server\n"
                  << "2. Read message\n"
                  << "3. Exit\n"
                  << "Select option: ";

        int choice = ReadMenuChoice();
        switch (choice) {
            case 1: ConnectToServer(); break;
            case 2: ReadAsync();       break;
            case 3: run = false;       break;
            default: std::cout << "Invalid choice.\n";
        }
    }

    if (hPipe != INVALID_HANDLE_VALUE) CloseHandle(hPipe);
    return 0;
}
