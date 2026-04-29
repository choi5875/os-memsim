#include <iostream>
#include <cstdint>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <limits>
#include "mmu.h"
#include "pagetable.h"

// 64 MB (64 * 1024 * 1024)
#define PHYSICAL_MEMORY 67108864

void printStartMessage(int page_size);
void createProcess(int text_size, int data_size, Mmu *mmu, PageTable *page_table);
void allocateVariable(uint32_t pid, std::string var_name, DataType type, uint32_t num_elements, Mmu *mmu, PageTable *page_table);
void setVariable(uint32_t pid, std::string var_name, uint32_t offset, const std::string &value, Mmu *mmu, PageTable *page_table, uint8_t *memory);
void freeVariable(uint32_t pid, std::string var_name, Mmu *mmu, PageTable *page_table);
void terminateProcess(uint32_t pid, Mmu *mmu, PageTable *page_table);

bool isValidPageSize(int page_size);
bool parseUInt32(const std::string &token, uint32_t &out_value);
bool parseInt(const std::string &token, int &out_value);
uint32_t dataTypeSize(DataType type);
bool parseDataType(const std::string &token, DataType &out_type);
bool reservePagesForRange(uint32_t pid, uint32_t start, uint32_t size, PageTable *page_table);
bool isPageUsedByVariables(uint32_t pid, int page_number, Mmu *mmu, int page_size);
void releaseUnusedPagesInRange(uint32_t pid, uint32_t start, uint32_t size, Mmu *mmu, PageTable *page_table);
std::vector<std::string> split(const std::string &line);
bool writeBytesAtVirtualAddress(uint32_t pid, uint32_t vaddr, const uint8_t *bytes, uint32_t length, PageTable *page_table, uint8_t *memory);
bool readBytesAtVirtualAddress(uint32_t pid, uint32_t vaddr, uint8_t *bytes, uint32_t length, PageTable *page_table, uint8_t *memory);
uint32_t processVirtualEnd(uint32_t pid, Mmu *mmu);
bool writeTypedValue(uint32_t pid, uint32_t vaddr, DataType type, const std::string &value, PageTable *page_table, uint8_t *memory);
bool printTypedValue(uint32_t pid, uint32_t vaddr, DataType type, uint32_t element_size, PageTable *page_table, uint8_t *memory);

int main(int argc, char **argv)
{
    // Ensure user specified page size as a command line parameter
    if (argc < 2)
    {
        std::cerr << "Error: you must specify the page size" << std::endl;
        return 1;
    }

    // Print opening instuction message
    int page_size = 0;
    if (!parseInt(argv[1], page_size) || !isValidPageSize(page_size))
    {
        std::cerr << "error out of range" << std::endl;
        return 1;
    }

    printStartMessage(page_size);

    // Create physical 'memory' (raw array of bytes)
    uint8_t *memory = new uint8_t[PHYSICAL_MEMORY];

    // Create MMU and Page Table
    Mmu *mmu = new Mmu(PHYSICAL_MEMORY);
    PageTable *page_table = new PageTable(page_size);

    // Prompt loop
    std::string command;
    std::cout << "> ";
    std::getline(std::cin, command);
    while (command != "exit")
    {
        // Handle command
        // TODO: implement this!
        std::vector<std::string> tokens = split(command);
        if (!tokens.empty())
        {
            if (tokens[0] == "create")
            {
                if (tokens.size() != 3)
                {
                    std::cout << "error: invalid arguments" << std::endl;
                }
                else
                {
                    int text_size = 0;
                    int data_size = 0;
                    if (!parseInt(tokens[1], text_size) || !parseInt(tokens[2], data_size))
                    {
                        std::cout << "error: invalid arguments" << std::endl;
                    }
                    else if (text_size < 2048 || text_size > 16384 || data_size < 0 || data_size > 1024)
                    {
                        std::cout << "error: invalid arguments" << std::endl;
                    }
                    else
                    {
                        createProcess(text_size, data_size, mmu, page_table);
                    }
                }
            }
            else if (tokens[0] == "allocate")
            {
                if (tokens.size() != 5)
                {
                    std::cout << "error: invalid arguments" << std::endl;
                }
                else
                {
                    uint32_t pid = 0;
                    uint32_t num_elements = 0;
                    DataType type = DataType::FreeSpace;

                    if (!parseUInt32(tokens[1], pid) || !parseDataType(tokens[3], type) || !parseUInt32(tokens[4], num_elements) || num_elements == 0)
                    {
                        std::cout << "error: invalid arguments" << std::endl;
                    }
                    else
                    {
                        allocateVariable(pid, tokens[2], type, num_elements, mmu, page_table);
                    }
                }
            }
            else if (tokens[0] == "set")
            {
                if (tokens.size() < 5)
                {
                    std::cout << "error: invalid arguments" << std::endl;
                }
                else
                {
                    uint32_t pid = 0;
                    uint32_t offset = 0;

                    if (!parseUInt32(tokens[1], pid) || !parseUInt32(tokens[3], offset))
                    {
                        std::cout << "error: invalid arguments" << std::endl;
                    }
                    else if (!mmu->processExists(pid))
                    {
                        std::cout << "error: process not found" << std::endl;
                    }
                    else if (!mmu->variableExists(pid, tokens[2]))
                    {
                        std::cout << "error: variable not found" << std::endl;
                    }
                    else
                    {
                        Variable *var = mmu->getVariable(pid, tokens[2]);
                        uint64_t requested = static_cast<uint64_t>(offset) + static_cast<uint64_t>(tokens.size() - 4);
                        if (requested > var->element_count)
                        {
                            std::cout << "error: index out of range" << std::endl;
                        }
                        else
                        {
                            for (size_t i = 4; i < tokens.size(); i++)
                            {
                                setVariable(pid, tokens[2], offset + static_cast<uint32_t>(i - 4), tokens[i], mmu, page_table, memory);
                            }
                        }
                    }
                }
            }
            else if (tokens[0] == "free")
            {
                if (tokens.size() != 3)
                {
                    std::cout << "error: invalid arguments" << std::endl;
                }
                else
                {
                    uint32_t pid = 0;
                    if (!parseUInt32(tokens[1], pid))
                    {
                        std::cout << "error: invalid arguments" << std::endl;
                    }
                    else
                    {
                        freeVariable(pid, tokens[2], mmu, page_table);
                    }
                }
            }
            else if (tokens[0] == "terminate")
            {
                if (tokens.size() != 2)
                {
                    std::cout << "error: invalid arguments" << std::endl;
                }
                else
                {
                    uint32_t pid = 0;
                    if (!parseUInt32(tokens[1], pid))
                    {
                        std::cout << "error: invalid arguments" << std::endl;
                    }
                    else
                    {
                        terminateProcess(pid, mmu, page_table);
                    }
                }
            }
            else if (tokens[0] == "print")
            {
                if (tokens.size() != 2)
                {
                    std::cout << "error: invalid arguments" << std::endl;
                }
                else if (tokens[1] == "mmu")
                {
                    mmu->print();
                }
                else if (tokens[1] == "page")
                {
                    page_table->print();
                }
                else if (tokens[1] == "processes")
                {
                    std::vector<uint32_t> pids = mmu->getProcessIds();
                    for (size_t i = 0; i < pids.size(); i++)
                    {
                        std::cout << pids[i] << std::endl;
                    }
                }
                else
                {
                    size_t sep = tokens[1].find(":");
                    if (sep == std::string::npos)
                    {
                        std::cout << "error: invalid arguments" << std::endl;
                    }
                    else
                    {
                        uint32_t pid = 0;
                        std::string pid_token = tokens[1].substr(0, sep);
                        std::string var_name = tokens[1].substr(sep + 1);

                        if (!parseUInt32(pid_token, pid))
                        {
                            std::cout << "error: invalid arguments" << std::endl;
                        }
                        else if (!mmu->processExists(pid))
                        {
                            std::cout << "error: process not found" << std::endl;
                        }
                        else if (!mmu->variableExists(pid, var_name))
                        {
                            std::cout << "error: variable not found" << std::endl;
                        }
                        else
                        {
                            Variable *var = mmu->getVariable(pid, var_name);
                            uint32_t to_print = std::min(static_cast<uint32_t>(4), var->element_count);

                            for (uint32_t i = 0; i < to_print; i++)
                            {
                                if (i > 0)
                                {
                                    std::cout << ", ";
                                }

                                uint32_t vaddr = var->virtual_address + i * var->element_size;
                                if (!printTypedValue(pid, vaddr, var->type, var->element_size, page_table, memory))
                                {
                                    std::cout << "?";
                                    continue;
                                }
                            }

                            if (var->element_count > 4)
                            {
                                std::cout << ", ... [" << var->element_count << " items]";
                            }

                            std::cout << std::endl;
                        }
                    }
                }
            }
            else
            {
                std::cout << "error: command not recognized" << std::endl;
            }
        }

        // Get next command
        std::cout << "> ";
        std::getline(std::cin, command);
    }

    // Cean up
    delete[] memory;
    delete mmu;
    delete page_table;

    return 0;
}

void printStartMessage(int page_size)
{
    std::cout << "Welcome to the Memory Allocation Simulator! Using a page size of " << page_size << " bytes." << std:: endl;
    std::cout << "Commands:" << std:: endl;
    std::cout << "  * create <text_size> <data_size> (initializes a new process)" << std:: endl;
    std::cout << "  * allocate <PID> <var_name> <data_type> <number_of_elements> (allocated memory on the heap)" << std:: endl;
    std::cout << "  * set <PID> <var_name> <offset> <value_0> <value_1> <value_2> ... <value_N> (set the value for a variable)" << std:: endl;
    std::cout << "  * free <PID> <var_name> (deallocate memory on the heap that is associated with <var_name>)" << std:: endl;
    std::cout << "  * terminate <PID> (kill the specified process)" << std:: endl;
    std::cout << "  * print <object> (prints data)" << std:: endl;
    std::cout << "    * If <object> is \"mmu\", print the MMU memory table" << std:: endl;
    std::cout << "    * if <object> is \"page\", print the page table" << std:: endl;
    std::cout << "    * if <object> is \"processes\", print a list of PIDs for processes that are still running" << std:: endl;
    std::cout << "    * if <object> is a \"<PID>:<var_name>\", print the value of the variable for that process" << std:: endl;
    std::cout << std::endl;
}

void createProcess(int text_size, int data_size, Mmu *mmu, PageTable *page_table)
{
    // TODO: implement this!
    //   - create new process in the MMU
    //   - allocate new variables for the <TEXT>, <GLOBALS>, and <STACK>
    //   - print pid
    uint32_t startup = static_cast<uint32_t>(text_size + data_size + 65536);
    int page_size = page_table->getPageSize();
    uint32_t required_pages = (startup + static_cast<uint32_t>(page_size) - 1) / static_cast<uint32_t>(page_size);

    if (page_table->getUsedFrameCount() + static_cast<int>(required_pages) > page_table->getFrameCount())
    {
        std::cout << "error: allocation exceeds system memory" << std::endl;
        return;
    }

    uint32_t pid = mmu->createProcess();

    if (!reservePagesForRange(pid, 0, startup, page_table))
    {
        mmu->removeProcess(pid);
        std::cout << "error: allocation exceeds system memory" << std::endl;
        return;
    }

    mmu->addVariableToProcess(pid, "<TEXT>", DataType::Char, static_cast<uint32_t>(text_size), 0, static_cast<uint32_t>(text_size), 1);
    mmu->addVariableToProcess(pid, "<GLOBALS>", DataType::Char, static_cast<uint32_t>(data_size), static_cast<uint32_t>(text_size), static_cast<uint32_t>(data_size), 1);
    mmu->addVariableToProcess(pid, "<STACK>", DataType::Char, 65536, static_cast<uint32_t>(text_size + data_size), 65536, 1);

    uint32_t rounded = required_pages * static_cast<uint32_t>(page_size);
    if (rounded > startup)
    {
        mmu->addVariableToProcess(pid, "<FREE_SPACE>", DataType::FreeSpace, rounded - startup, startup, 0, 1);
    }

    std::cout << pid << std::endl;
}

void allocateVariable(uint32_t pid, std::string var_name, DataType type, uint32_t num_elements, Mmu *mmu, PageTable *page_table)
{
    // TODO: implement this!
    //   - find first free space within a page already allocated to this process that is large enough to fit the new variable
    //   - if no hole is large enough, allocate new page(s)
    //   - insert variable into MMU
    //   - print virtual memory address
    if (!mmu->processExists(pid))
    {
        std::cout << "error: process not found" << std::endl;
        return;
    }

    if (mmu->variableExists(pid, var_name))
    {
        std::cout << "error: variable already exists" << std::endl;
        return;
    }

    uint32_t type_size = dataTypeSize(type);
    uint64_t total_u64 = static_cast<uint64_t>(type_size) * static_cast<uint64_t>(num_elements);
    if (total_u64 > std::numeric_limits<uint32_t>::max())
    {
        std::cout << "error: allocation exceeds system memory" << std::endl;
        return;
    }
    uint32_t size = static_cast<uint32_t>(total_u64);

    uint32_t address = 0;
    bool allocated = mmu->allocateFromFreeSpace(pid, var_name, type, size, num_elements, type_size, address);
    if (allocated)
    {
        if (!reservePagesForRange(pid, address, size, page_table))
        {
            mmu->removeVariable(pid, var_name);
            std::cout << "error: allocation exceeds system memory" << std::endl;
            return;
        }

        std::cout << address << std::endl;
        return;
    }

    uint32_t address_end = processVirtualEnd(pid, mmu);
    int page_size = page_table->getPageSize();
    uint32_t new_end = address_end + size;
    uint32_t rounded_end = ((new_end + static_cast<uint32_t>(page_size) - 1) / static_cast<uint32_t>(page_size)) * static_cast<uint32_t>(page_size);

    if (!reservePagesForRange(pid, address_end, size, page_table))
    {
        std::cout << "error: allocation exceeds system memory" << std::endl;
        return;
    }

    mmu->addVariableToProcess(pid, var_name, type, size, address_end, num_elements, type_size);
    if (rounded_end > new_end)
    {
        mmu->addVariableToProcess(pid, "<FREE_SPACE>", DataType::FreeSpace, rounded_end - new_end, new_end, 0, 1);
        mmu->mergeFreeSpace(pid);
    }

    std::cout << address_end << std::endl;
}

void setVariable(uint32_t pid, std::string var_name, uint32_t offset, const std::string &value, Mmu *mmu, PageTable *page_table, uint8_t *memory)
{
    // TODO: implement this!
    //   - look up physical address for variable based on its virtual address / offset
    //   - insert `value` into `memory` at physical address
    //   * note: this function only handles a single element (i.e. you'll need to call this within a loop when setting
    //           multiple elements of an array)
    Variable *var = mmu->getVariable(pid, var_name);
    if (var == nullptr)
    {
        return;
    }

    uint32_t vaddr = var->virtual_address + offset * var->element_size;

    try
    {
        writeTypedValue(pid, vaddr, var->type, value, page_table, memory);
    }
    catch (const std::exception &)
    {
    }
}

void freeVariable(uint32_t pid, std::string var_name, Mmu *mmu, PageTable *page_table)
{
    // TODO: implement this!
    //   - remove entry from MMU
    //   - free page if this variable was the only one on a given page
    if (!mmu->processExists(pid))
    {
        std::cout << "error: process not found" << std::endl;
        return;
    }

    if (!mmu->variableExists(pid, var_name))
    {
        std::cout << "error: variable not found" << std::endl;
        return;
    }

    Variable removed;
    if (!mmu->removeVariable(pid, var_name, &removed))
    {
        std::cout << "error: variable not found" << std::endl;
        return;
    }

    releaseUnusedPagesInRange(pid, removed.virtual_address, removed.size, mmu, page_table);
}

void terminateProcess(uint32_t pid, Mmu *mmu, PageTable *page_table)
{
    // TODO: implement this!
    //   - remove process from MMU
    //   - free all pages associated with given process
    if (!mmu->processExists(pid))
    {
        std::cout << "error: process not found" << std::endl;
        return;
    }

    page_table->removeProcess(pid);
    mmu->removeProcess(pid);
}

bool isValidPageSize(int page_size)
{
    if (page_size < 1024 || page_size > 32768)
    {
        return false;
    }

    return (page_size & (page_size - 1)) == 0;
}

bool parseUInt32(const std::string &token, uint32_t &out_value)
{
    try
    {
        unsigned long v = std::stoul(token);
        if (v > std::numeric_limits<uint32_t>::max())
        {
            return false;
        }
        out_value = static_cast<uint32_t>(v);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool parseInt(const std::string &token, int &out_value)
{
    try
    {
        out_value = std::stoi(token);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

uint32_t dataTypeSize(DataType type)
{
    if (type == DataType::Char)
    {
        return 1;
    }
    if (type == DataType::Short)
    {
        return 2;
    }
    if (type == DataType::Int || type == DataType::Float)
    {
        return 4;
    }
    if (type == DataType::Long || type == DataType::Double)
    {
        return 8;
    }
    return 1;
}

bool parseDataType(const std::string &token, DataType &out_type)
{
    if (token == "char")
    {
        out_type = DataType::Char;
        return true;
    }
    if (token == "short")
    {
        out_type = DataType::Short;
        return true;
    }
    if (token == "int")
    {
        out_type = DataType::Int;
        return true;
    }
    if (token == "float")
    {
        out_type = DataType::Float;
        return true;
    }
    if (token == "long")
    {
        out_type = DataType::Long;
        return true;
    }
    if (token == "double")
    {
        out_type = DataType::Double;
        return true;
    }

    return false;
}

bool reservePagesForRange(uint32_t pid, uint32_t start, uint32_t size, PageTable *page_table)
{
    if (size == 0)
    {
        return true;
    }

    uint32_t page_size = static_cast<uint32_t>(page_table->getPageSize());
    int first = static_cast<int>(start / page_size);
    int last = static_cast<int>((start + size - 1) / page_size);

    std::vector<int> added;
    for (int page = first; page <= last; page++)
    {
        if (page_table->hasEntry(pid, page))
        {
            continue;
        }

        if (!page_table->addEntry(pid, page))
        {
            for (size_t i = 0; i < added.size(); i++)
            {
                page_table->removeEntry(pid, added[i]);
            }
            return false;
        }
        added.push_back(page);
    }

    return true;
}

bool isPageUsedByVariables(uint32_t pid, int page_number, Mmu *mmu, int page_size)
{
    std::vector<Variable*> vars = mmu->getVariables(pid);
    uint32_t page_start = static_cast<uint32_t>(page_number * page_size);
    uint32_t page_end = page_start + static_cast<uint32_t>(page_size);

    for (size_t i = 0; i < vars.size(); i++)
    {
        Variable *v = vars[i];
        if (v == nullptr || v->type == DataType::FreeSpace)
        {
            continue;
        }

        uint32_t var_start = v->virtual_address;
        uint32_t var_end = v->virtual_address + v->size;
        if (var_start < page_end && var_end > page_start)
        {
            return true;
        }
    }

    return false;
}

void releaseUnusedPagesInRange(uint32_t pid, uint32_t start, uint32_t size, Mmu *mmu, PageTable *page_table)
{
    if (size == 0)
    {
        return;
    }

    int page_size = page_table->getPageSize();
    int first = static_cast<int>(start / static_cast<uint32_t>(page_size));
    int last = static_cast<int>((start + size - 1) / static_cast<uint32_t>(page_size));

    for (int page = first; page <= last; page++)
    {
        if (!page_table->hasEntry(pid, page))
        {
            continue;
        }

        if (!isPageUsedByVariables(pid, page, mmu, page_size))
        {
            page_table->removeEntry(pid, page);
        }
    }
}

std::vector<std::string> split(const std::string &line)
{
    std::vector<std::string> tokens;
    std::stringstream ss(line);
    std::string token;
    while (ss >> token)
    {
        tokens.push_back(token);
    }
    return tokens;
}

bool writeBytesAtVirtualAddress(uint32_t pid, uint32_t vaddr, const uint8_t *bytes, uint32_t length, PageTable *page_table, uint8_t *memory)
{
    for (uint32_t i = 0; i < length; i++)
    {
        int physical = page_table->getPhysicalAddress(pid, vaddr + i);
        if (physical < 0)
        {
            return false;
        }
        memory[physical] = bytes[i];
    }
    return true;
}

bool readBytesAtVirtualAddress(uint32_t pid, uint32_t vaddr, uint8_t *bytes, uint32_t length, PageTable *page_table, uint8_t *memory)
{
    for (uint32_t i = 0; i < length; i++)
    {
        int physical = page_table->getPhysicalAddress(pid, vaddr + i);
        if (physical < 0)
        {
            return false;
        }
        bytes[i] = memory[physical];
    }
    return true;
}

uint32_t processVirtualEnd(uint32_t pid, Mmu *mmu)
{
    std::vector<Variable*> vars = mmu->getVariables(pid);
    uint32_t end = 0;
    for (size_t i = 0; i < vars.size(); i++)
    {
        if (vars[i] == nullptr)
        {
            continue;
        }

        uint32_t candidate = vars[i]->virtual_address + vars[i]->size;
        if (candidate > end)
        {
            end = candidate;
        }
    }
    return end;
}

bool writeTypedValue(uint32_t pid, uint32_t vaddr, DataType type, const std::string &value, PageTable *page_table, uint8_t *memory)
{
    if (type == DataType::Char)
    {
        char parsed = value.empty() ? '\0' : value[0];
        return writeBytesAtVirtualAddress(pid, vaddr, reinterpret_cast<uint8_t*>(&parsed), sizeof(char), page_table, memory);
    }
    if (type == DataType::Short)
    {
        int16_t parsed = static_cast<int16_t>(std::stoi(value));
        return writeBytesAtVirtualAddress(pid, vaddr, reinterpret_cast<uint8_t*>(&parsed), sizeof(int16_t), page_table, memory);
    }
    if (type == DataType::Int)
    {
        int32_t parsed = static_cast<int32_t>(std::stol(value));
        return writeBytesAtVirtualAddress(pid, vaddr, reinterpret_cast<uint8_t*>(&parsed), sizeof(int32_t), page_table, memory);
    }
    if (type == DataType::Float)
    {
        float parsed = std::stof(value);
        return writeBytesAtVirtualAddress(pid, vaddr, reinterpret_cast<uint8_t*>(&parsed), sizeof(float), page_table, memory);
    }
    if (type == DataType::Long)
    {
        int64_t parsed = std::stoll(value);
        return writeBytesAtVirtualAddress(pid, vaddr, reinterpret_cast<uint8_t*>(&parsed), sizeof(int64_t), page_table, memory);
    }
    if (type == DataType::Double)
    {
        double parsed = std::stod(value);
        return writeBytesAtVirtualAddress(pid, vaddr, reinterpret_cast<uint8_t*>(&parsed), sizeof(double), page_table, memory);
    }

    return false;
}

bool printTypedValue(uint32_t pid, uint32_t vaddr, DataType type, uint32_t element_size, PageTable *page_table, uint8_t *memory)
{
    if (element_size > 8)
    {
        return false;
    }

    uint8_t buf[8] = {0};
    if (!readBytesAtVirtualAddress(pid, vaddr, buf, element_size, page_table, memory))
    {
        return false;
    }

    if (type == DataType::Char)
    {
        char value;
        std::memcpy(&value, buf, sizeof(char));
        std::cout << value;
        return true;
    }
    if (type == DataType::Short)
    {
        int16_t value;
        std::memcpy(&value, buf, sizeof(int16_t));
        std::cout << value;
        return true;
    }
    if (type == DataType::Int)
    {
        int32_t value;
        std::memcpy(&value, buf, sizeof(int32_t));
        std::cout << value;
        return true;
    }
    if (type == DataType::Float)
    {
        float value;
        std::memcpy(&value, buf, sizeof(float));
        std::cout << value;
        return true;
    }
    if (type == DataType::Long)
    {
        int64_t value;
        std::memcpy(&value, buf, sizeof(int64_t));
        std::cout << value;
        return true;
    }
    if (type == DataType::Double)
    {
        double value;
        std::memcpy(&value, buf, sizeof(double));
        std::cout << value;
        return true;
    }

    return false;
}
