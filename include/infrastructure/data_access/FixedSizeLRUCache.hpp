#pragma once

#include <string>
#include <cstring>
#include <iostream>

template<typename ValueType, size_t MaxSize>
class FixedSizeLRUCache {
private:
    struct CacheEntry {
        std::string key;
        ValueType value;
        int prev;
        int next;
        bool valid;

        CacheEntry() : prev(-1), next(-1), valid(false) {}
    };

    CacheEntry entries[MaxSize];
    int hashTable[MaxSize * 2];
    int head;
    int tail;
    size_t currentSize;

    static constexpr int EMPTY_SLOT = -1;

    uint32_t hash(const std::string& key) const {
        uint32_t hash = 0;
        for (char c : key) {
            hash = hash * 31 + static_cast<uint32_t>(c);
        }
        return hash;
    }

    int findHashSlot(const std::string& key) const {
        uint32_t h = hash(key) % (MaxSize * 2);
        int originalH = h;

        while (hashTable[h] != EMPTY_SLOT) {
            if (entries[hashTable[h]].valid && entries[hashTable[h]].key == key) {
                return h;
            }
            h = (h + 1) % (MaxSize * 2);
            if (h == originalH) break;
        }
        return -1;
    }

    int findEmptyHashSlot(const std::string& key) const {
        uint32_t h = hash(key) % (MaxSize * 2);
        int originalH = h;

        while (hashTable[h] != EMPTY_SLOT) {
            h = (h + 1) % (MaxSize * 2);
            if (h == originalH) return -1;
        }
        return h;
    }

    void moveToFront(int index) {
        if (index == head) return;

        if (entries[index].prev != -1) {
            entries[entries[index].prev].next = entries[index].next;
        }
        if (entries[index].next != -1) {
            entries[entries[index].next].prev = entries[index].prev;
        }
        if (index == tail) {
            tail = entries[index].prev;
        }

        entries[index].prev = -1;
        entries[index].next = head;
        if (head != -1) {
            entries[head].prev = index;
        }
        head = index;

        if (tail == -1) {
            tail = index;
        }
    }

    void removeFromList(int index) {
        if (entries[index].prev != -1) {
            entries[entries[index].prev].next = entries[index].next;
        } else {
            head = entries[index].next;
        }

        if (entries[index].next != -1) {
            entries[entries[index].next].prev = entries[index].prev;
        } else {
            tail = entries[index].prev;
        }

        entries[index].prev = -1;
        entries[index].next = -1;
    }

    int findEmptyEntry() const {
        for (int i = 0; i < MaxSize; i++) {
            if (!entries[i].valid) {
                return i;
            }
        }
        return -1;
    }

public:
    FixedSizeLRUCache() : head(-1), tail(-1), currentSize(0) {
        for (int i = 0; i < MaxSize * 2; i++) {
            hashTable[i] = EMPTY_SLOT;
        }
    }

    bool put(const std::string& key, const ValueType& value) {
        int hashSlot = findHashSlot(key);

        if (hashSlot != -1) {
            int entryIndex = hashTable[hashSlot];
            entries[entryIndex].value = value;
            moveToFront(entryIndex);
            return true;
        }

        int entryIndex = findEmptyEntry();
        if (entryIndex == -1) {
            if (currentSize >= MaxSize) {
                entryIndex = tail;
                int oldHashSlot = findHashSlot(entries[entryIndex].key);
                if (oldHashSlot != -1) {
                    hashTable[oldHashSlot] = EMPTY_SLOT;
                }
                removeFromList(entryIndex);
                currentSize--;
            } else {
                return false;
            }
        }

        int newHashSlot = findEmptyHashSlot(key);
        if (newHashSlot == -1) {
            return false;
        }

        entries[entryIndex].key = key;
        entries[entryIndex].value = value;
        entries[entryIndex].valid = true;
        hashTable[newHashSlot] = entryIndex;

        entries[entryIndex].prev = -1;
        entries[entryIndex].next = head;
        if (head != -1) {
            entries[head].prev = entryIndex;
        }
        head = entryIndex;

        if (tail == -1) {
            tail = entryIndex;
        }

        currentSize++;
        return true;
    }

    bool get(const std::string& key, ValueType& value) {
        int hashSlot = findHashSlot(key);
        if (hashSlot == -1) {
            return false;
        }

        int entryIndex = hashTable[hashSlot];
        value = entries[entryIndex].value;
        moveToFront(entryIndex);
        return true;
    }

    bool contains(const std::string& key) const {
        return findHashSlot(key) != -1;
    }

    bool remove(const std::string& key) {
        int hashSlot = findHashSlot(key);
        if (hashSlot == -1) {
            return false;
        }

        int entryIndex = hashTable[hashSlot];
        hashTable[hashSlot] = EMPTY_SLOT;
        removeFromList(entryIndex);

        entries[entryIndex].valid = false;
        entries[entryIndex].key.clear();
        currentSize--;

        return true;
    }

    size_t size() const {
        return currentSize;
    }

    bool empty() const {
        return currentSize == 0;
    }

    void clear() {
        for (int i = 0; i < MaxSize; i++) {
            entries[i].valid = false;
            entries[i].key.clear();
            entries[i].prev = -1;
            entries[i].next = -1;
        }
        for (int i = 0; i < MaxSize * 2; i++) {
            hashTable[i] = EMPTY_SLOT;
        }
        head = -1;
        tail = -1;
        currentSize = 0;
    }

    void printCache() const {
        std::cout << "=== LRU Cache (Size: " << currentSize << "/" << MaxSize << ") ===" << std::endl;
        int current = head;
        int index = 0;

        while (current != -1 && index < MaxSize) {
            if (entries[current].valid) {
                std::cout << "[" << index++ << "] Key: " << entries[current].key << std::endl;
            }
            current = entries[current].next;
        }
        std::cout << "======================" << std::endl << std::endl;
    }
};
