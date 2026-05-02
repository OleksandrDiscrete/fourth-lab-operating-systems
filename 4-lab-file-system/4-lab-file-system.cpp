#define _CRT_SECURE_NO_WARNINGS 
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>            
#include <windows.h>

using namespace std;

const int BLOCK_SIZE = 512;
const int DISK_SIZE = 1024 * 1024;
const int TOTAL_BLOCKS = DISK_SIZE / BLOCK_SIZE;

struct SuperBlock {
    int bitmapStartBlock;
    int catalogStartBlock;
    int dataStartBlock;
    int maxFiles;
};

struct FileInfo {
    char name[24];
    int sizeBytes;
    int startBlock;
    int blockCount;
    int attributes;
    bool isUsed;
    bool isDeleted;
};

class MemoryFS {
private:
    vector<char> disk;
    SuperBlock* sb;
    char* bitmap;
    FileInfo* catalog;

    bool isBlockUsed(int blockIndex) {
        int byteIdx = blockIndex / 8;
        int bitIdx = blockIndex % 8;
        return (bitmap[byteIdx] & (1 << bitIdx)) != 0;
    }

    void setBlockStatus(int blockIndex, bool used) {
        int byteIdx = blockIndex / 8;
        int bitIdx = blockIndex % 8;
        if (used) bitmap[byteIdx] |= (1 << bitIdx);
        else bitmap[byteIdx] &= ~(1 << bitIdx);
    }

    int findFreeContiguousBlocks(int requiredBlocks) {
        int contiguousCount = 0;
        int startBlock = -1;

        for (int i = sb->dataStartBlock; i < TOTAL_BLOCKS; i++) {
            if (!isBlockUsed(i)) {
                if (contiguousCount == 0) startBlock = i;
                contiguousCount++;
                if (contiguousCount == requiredBlocks) return startBlock;
            }
            else {
                contiguousCount = 0;
            }
        }
        return -1;
    }

public:
    MemoryFS() {
        disk.resize(DISK_SIZE, 0);
        sb = reinterpret_cast<SuperBlock*>(&disk[0]);
    }

    void format() {
        fill(disk.begin(), disk.end(), 0);

        sb->bitmapStartBlock = 1;
        int bitmapBlocks = 1;

        sb->catalogStartBlock = sb->bitmapStartBlock + bitmapBlocks;
        int catalogBlocks = 4;
        sb->maxFiles = (catalogBlocks * BLOCK_SIZE) / sizeof(FileInfo);

        sb->dataStartBlock = sb->catalogStartBlock + catalogBlocks;

        bitmap = &disk[sb->bitmapStartBlock * BLOCK_SIZE];
        catalog = reinterpret_cast<FileInfo*>(&disk[sb->catalogStartBlock * BLOCK_SIZE]);

        for (int i = 0; i < sb->dataStartBlock; i++) {
            setBlockStatus(i, true);
        }
        cout << "[ФС] Диск вiдформатовано. Загальна кiлькiсть блокiв: " << TOTAL_BLOCKS << "\n";
    }

    void writeFile(const string& filename, const string& data) {
        int sizeBytes = data.length();
        int requiredBlocks = (sizeBytes + BLOCK_SIZE - 1) / BLOCK_SIZE;
        if (requiredBlocks == 0) requiredBlocks = 1;

        FileInfo* fileEntry = nullptr;
        for (int i = 0; i < sb->maxFiles; i++) {
            if (!catalog[i].isUsed) {
                fileEntry = &catalog[i];
                break;
            }
        }

        if (!fileEntry) {
            cout << "[Помилка] Каталог переповнений!\n";
            return;
        }

        int startBlock = findFreeContiguousBlocks(requiredBlocks);
        if (startBlock == -1) {
            cout << "[Помилка] Немає вiльного мiсця!\n";
            return;
        }

        strncpy(fileEntry->name, filename.c_str(), 23);
        fileEntry->sizeBytes = sizeBytes;
        fileEntry->startBlock = startBlock;
        fileEntry->blockCount = requiredBlocks;
        fileEntry->attributes = 0;
        fileEntry->isUsed = true;
        fileEntry->isDeleted = false;

        for (int i = 0; i < requiredBlocks; i++) {
            setBlockStatus(startBlock + i, true);
        }

        char* dataPtr = &disk[startBlock * BLOCK_SIZE];
        memcpy(dataPtr, data.c_str(), sizeBytes);

        cout << "[ФС] Файл '" << filename << "' записано (Стартовий блок: " << startBlock << ").\n";
    }

    void readFile(const string& filename) {
        for (int i = 0; i < sb->maxFiles; i++) {
            if (catalog[i].isUsed && !catalog[i].isDeleted && strcmp(catalog[i].name, filename.c_str()) == 0) {
                char* dataPtr = &disk[catalog[i].startBlock * BLOCK_SIZE];
                string content(dataPtr, catalog[i].sizeBytes);

                cout << "\n--- Вмiст файлу '" << filename << "' ---\n";
                cout << content << "\n--------------------------------\n";
                return;
            }
        }
        cout << "[Помилка] Файл '" << filename << "' не знайдено.\n";
    }

    void deleteFile(const string& filename) {
        for (int i = 0; i < sb->maxFiles; i++) {
            if (catalog[i].isUsed && !catalog[i].isDeleted && strcmp(catalog[i].name, filename.c_str()) == 0) {
                catalog[i].isDeleted = true;

                for (int b = 0; b < catalog[i].blockCount; b++) {
                    setBlockStatus(catalog[i].startBlock + b, false);
                }
                cout << "[ФС] Файл '" << filename << "' ВИДАЛЕНО.\n";
                return;
            }
        }
        cout << "[Помилка] Файл '" << filename << "' не знайдено.\n";
    }

    void undeleteFile(const string& filename) {
        for (int i = 0; i < sb->maxFiles; i++) {
            if (catalog[i].isUsed && catalog[i].isDeleted && strcmp(catalog[i].name, filename.c_str()) == 0) {

                bool canRestore = true;
                for (int b = 0; b < catalog[i].blockCount; b++) {
                    if (isBlockUsed(catalog[i].startBlock + b)) {
                        canRestore = false;
                        break;
                    }
                }

                if (canRestore) {
                    catalog[i].isDeleted = false;

                    for (int b = 0; b < catalog[i].blockCount; b++) {
                        setBlockStatus(catalog[i].startBlock + b, true);
                    }
                    cout << "[ФС] Файл '" << filename << "' ВIДНОВЛЕНО!\n";
                }
                else {
                    cout << "[Помилка] Неможливо вiдновити '" << filename << "'.\n";
                    catalog[i].isUsed = false;
                }
                return;
            }
        }
        cout << "[Помилка] Видалений файл '" << filename << "' не знайдено.\n";
    }

    void listFiles() {
        cout << "\n=== Каталог файлiв ===\n";
        for (int i = 0; i < sb->maxFiles; i++) {
            if (catalog[i].isUsed) {
                cout << "Файл: " << catalog[i].name
                    << " | Розмiр: " << catalog[i].sizeBytes << " байт"
                    << " | Блок: " << catalog[i].startBlock
                    << " | Статус: " << (catalog[i].isDeleted ? "[ВИДАЛЕНО]" : "[АКТИВНИЙ]") << "\n";
            }
        }
        cout << "======================\n";
    }
};

int main() {
    SetConsoleOutputCP(1251);
    SetConsoleCP(1251);

    MemoryFS fs;

    cout << "ТЕСТУВАННЯ ВЛАСНОЇ ФАЙЛОВОЇ СИСТЕМИ\n\n";

    fs.format();

    fs.writeFile("hello.txt", "Привіт! Це мій перший файл у власній Файловій Системі.");
    fs.writeFile("secret.dat", "Дуже важливі дані, які не можна втратити.");

    fs.listFiles();
    fs.readFile("hello.txt");

    fs.deleteFile("hello.txt");
    fs.listFiles();

    fs.readFile("hello.txt");

    fs.undeleteFile("hello.txt");
    fs.readFile("hello.txt");

    cout << "\nРоботу завершено. Натисніть Enter для виходу";
    cin.get();

    return 0;
}