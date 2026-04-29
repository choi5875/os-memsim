#include <algorithm>
#include <set>
#include <iomanip>
#include "pagetable.h"

// 64 MB (64 * 1024 * 1024)
#define PHYSICAL_MEMORY 67108864

PageTable::PageTable(int page_size)
{
    _page_size = page_size;
    _frame_count = PHYSICAL_MEMORY / _page_size;
}

PageTable::~PageTable()
{
}

std::vector<std::string> PageTable::sortedKeys()
{
    std::vector<std::string> keys;

    std::map<std::string, int>::iterator it;
    for (it = _table.begin(); it != _table.end(); it++)
    {
        keys.push_back(it->first);
    }

    std::sort(keys.begin(), keys.end(), PageTableKeyComparator());

    return keys;
}

bool PageTable::addEntry(uint32_t pid, int page_number)
{
    // Combination of pid and page number act as the key to look up frame number
    std::string entry = std::to_string(pid) + "|" + std::to_string(page_number);

    if (_table.count(entry) > 0)
    {
        return true;
    }

    int frame = 0; 
    // Find free frame
    // TODO: implement this!
    std::set<int> used_frames;
    std::map<std::string, int>::iterator it;
    for (it = _table.begin(); it != _table.end(); it++)
    {
        used_frames.insert(it->second);
    }

    while (frame < _frame_count && used_frames.count(frame) > 0)
    {
        frame++;
    }

    if (frame >= _frame_count)
    {
        return false;
    }

    _table[entry] = frame;
    return true;
}

bool PageTable::hasEntry(uint32_t pid, int page_number)
{
    std::string entry = std::to_string(pid) + "|" + std::to_string(page_number);
    return _table.count(entry) > 0;
}

bool PageTable::removeEntry(uint32_t pid, int page_number)
{
    std::string entry = std::to_string(pid) + "|" + std::to_string(page_number);
    if (_table.count(entry) == 0)
    {
        return false;
    }

    _table.erase(entry);
    return true;
}

void PageTable::removeProcess(uint32_t pid)
{
    std::vector<std::string> keys = sortedKeys();
    for (size_t i = 0; i < keys.size(); i++)
    {
        size_t sep = keys[i].find("|");
        uint32_t key_pid = static_cast<uint32_t>(std::stoul(keys[i].substr(0, sep)));
        if (key_pid == pid)
        {
            _table.erase(keys[i]);
        }
    }
}

int PageTable::getPageSize()
{
    return _page_size;
}

int PageTable::getFrameCount()
{
    return _frame_count;
}

int PageTable::getUsedFrameCount()
{
    return static_cast<int>(_table.size());
}

int PageTable::getPhysicalAddress(uint32_t pid, uint32_t virtual_address)
{
    // Convert virtual address to page_number and page_offset
    // TODO: implement this!
    int page_number = static_cast<int>(virtual_address / static_cast<uint32_t>(_page_size));
    int page_offset = static_cast<int>(virtual_address % static_cast<uint32_t>(_page_size));

    // Combination of pid and page number act as the key to look up frame number
    std::string entry = std::to_string(pid) + "|" + std::to_string(page_number);
    
    // If entry exists, look up frame number and convert virtual to physical address
    int address = -1;
    if (_table.count(entry) > 0)
    {
        // TODO: implement this!
        int frame = _table[entry];
        address = frame * _page_size + page_offset;
    }

    return address;
}

void PageTable::print()
{
    int i;

    std::cout << " PID  | Page Number | Frame Number" << std::endl;
    std::cout << "------+-------------+--------------" << std::endl;

    std::vector<std::string> keys = sortedKeys();

    for (i = 0; i < keys.size(); i++)
    {
        // TODO: print all pages
        size_t sep = keys[i].find("|");
        uint32_t pid = static_cast<uint32_t>(std::stoul(keys[i].substr(0, sep)));
        int page = std::stoi(keys[i].substr(sep + 1));
        int frame = _table[keys[i]];

        std::cout << std::setw(5) << pid
                  << " | " << std::setw(11) << page
                  << " | " << std::setw(12) << frame
                  << std::endl;
    }
}
