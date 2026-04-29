#ifndef __MMU_H_
#define __MMU_H_

#include <cstdint>
#include <string>
#include <vector>

enum DataType : uint8_t {FreeSpace, Char, Short, Int, Float, Long, Double};

typedef struct Variable {
    std::string name;
    DataType type;
    uint32_t virtual_address;
    uint32_t size;
    uint32_t element_count;
    uint32_t element_size;
} Variable;

typedef struct Process {
    uint32_t pid;
    std::vector<Variable*> variables;
} Process;

class Mmu {
private:
    uint32_t _next_pid;
    uint32_t _max_size;
    std::vector<Process*> _processes;

    Process* findProcess(uint32_t pid);

public:
    Mmu(int memory_size);
    ~Mmu();

    uint32_t createProcess();
    bool processExists(uint32_t pid);
    std::vector<uint32_t> getProcessIds();
    void addVariableToProcess(uint32_t pid, std::string var_name, DataType type, uint32_t size, uint32_t address, uint32_t element_count, uint32_t element_size);
    Variable* getVariable(uint32_t pid, std::string var_name);
    bool variableExists(uint32_t pid, std::string var_name);
    std::vector<Variable*> getVariables(uint32_t pid);
    bool allocateFromFreeSpace(uint32_t pid, std::string var_name, DataType type, uint32_t size, uint32_t element_count, uint32_t element_size, uint32_t &out_address);
    bool removeVariable(uint32_t pid, std::string var_name, Variable *removed);
    bool removeVariable(uint32_t pid, std::string var_name);
    bool removeProcess(uint32_t pid);
    void mergeFreeSpace(uint32_t pid);
    void print();
};

#endif // __MMU_H_
