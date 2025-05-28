// client.cpp: клиент асинхронного чтения из именованного канала
#include <windows.h>
#include <iostream>
#include <string>
#include <algorithm>

HANDLE hPipe = INVALID_HANDLE_VALUE;  // дескриптор канала

// Функция чтения выбора из меню (безопасная конвертация в int)
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

// Подключение к серверу через именованный канал
void ConnectToServer() {
    const TCHAR* name = TEXT("\\\\.\\pipe\\PipeSrv");

    if (hPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(hPipe);
        hPipe = INVALID_HANDLE_VALUE;
    }

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

// Обработчик завершения асинхронного чтения
VOID CALLBACK ReadCompleted(
    DWORD dwErrorCode,
    DWORD dwNumberOfBytesTransfered,
    LPOVERLAPPED lpOverlapped
) {
    // Буфер расположен сразу после структуры OVERLAPPED
    char* buf = reinterpret_cast<char*>(lpOverlapped) + sizeof(OVERLAPPED);
    if (dwErrorCode == 0 && dwNumberOfBytesTransfered > 0) {
        buf[dwNumberOfBytesTransfered] = '\0';
        std::cout << "Received: " << buf << "\n";
    } else {
        std::cerr << "Read error or no data. code=" << dwErrorCode << "\n";
    }
    // Освобождаем память, выделенную под OVERLAPPED + буфер
    HeapFree(GetProcessHeap(), 0, lpOverlapped);
}

// Функция асинхронного чтения из канала
void ReadAsync() {
    if (hPipe == INVALID_HANDLE_VALUE) {
        std::cout << "You must connect first.\n";
        return;
    }

    // Проверяем наличие данных без чтения
    DWORD totalBytesAvail = 0;
    if (!PeekNamedPipe(hPipe, NULL, 0, NULL, &totalBytesAvail, NULL)) {
        std::cerr << "PeekNamedPipe failed, GLE=" << GetLastError() << "\n";
        return;
    }
    if (totalBytesAvail == 0) {
        std::cout << "No data available right now. Try later.\n";
        return;
    }

    // Выделяем блок памяти: OVERLAPPED + буфер
    const DWORD bufSize = 1024;
    BYTE* block = static_cast<BYTE*>(
        HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(OVERLAPPED) + bufSize)
    );
    auto* ov = reinterpret_cast<OVERLAPPED*>(block);
    char* buffer = reinterpret_cast<char*>(block + sizeof(OVERLAPPED));

    // Запускаем асинхронное чтение с CALLBACK-функцией
    BOOL okRead = ReadFileEx(
        hPipe,
        buffer,
        std::min(bufSize - 1, totalBytesAvail),
        ov,
        ReadCompleted
    );
    if (!okRead) {
        std::cerr << "ReadFileEx failed, GLE=" << GetLastError() << "\n";
        HeapFree(GetProcessHeap(), 0, block);
        return;
    }

    std::cout << "Waiting for data...\n";
    // SleepEx с TRUE позволяет обработать APC-колбэки
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

    if (hPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(hPipe);
    }
    return 0;
}
