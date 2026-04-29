#include <iostream>
#include <algorithm>
#include <iomanip>
#include "mmu.h"

Mmu::Mmu(int memory_size)
{
    _next_pid = 1024;
    _max_size = memory_size;
}

Mmu::~Mmu()
{
    for (size_t i = 0; i < _processes.size(); i++)
    {
        Process *proc = _processes[i];
        if (proc == nullptr)
        {
            continue;
        }

        for (size_t j = 0; j < proc->variables.size(); j++)
        {
            delete proc->variables[j];
        }
        proc->variables.clear();
        delete proc;
    }
    _processes.clear();
}

Process* Mmu::findProcess(uint32_t pid)
{
    for (size_t i = 0; i < _processes.size(); i++)
    {
        if (_processes[i] != nullptr && _processes[i]->pid == pid)
        {
            return _processes[i];
        }
    }

    return nullptr;
}

uint32_t Mmu::createProcess()
{
    Process *proc = new Process();
    proc->pid = _next_pid;

    _processes.push_back(proc);

    _next_pid++;
    
    return proc->pid;
}

bool Mmu::processExists(uint32_t pid)
{
    return findProcess(pid) != nullptr;
}

std::vector<uint32_t> Mmu::getProcessIds()
{
    std::vector<uint32_t> ids;
    for (size_t i = 0; i < _processes.size(); i++)
    {
        if (_processes[i] != nullptr)
        {
            ids.push_back(_processes[i]->pid);
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

void Mmu::addVariableToProcess(uint32_t pid, std::string var_name, DataType type, uint32_t size, uint32_t address, uint32_t element_count, uint32_t element_size)
{
    Process *proc = findProcess(pid);
    if (proc == nullptr)
    {
        return;
    }

    Variable *var = new Variable();
    var->name = var_name;
    var->type = type;
    var->virtual_address = address;
    var->size = size;
    var->element_count = element_count;
    var->element_size = element_size;
    proc->variables.push_back(var);
}

Variable* Mmu::getVariable(uint32_t pid, std::string var_name)
{
    Process *proc = findProcess(pid);
    if (proc == nullptr)
    {
        return nullptr;
    }

    for (size_t i = 0; i < proc->variables.size(); i++)
    {
        Variable *var = proc->variables[i];
        if (var != nullptr && var->name == var_name && var->type != DataType::FreeSpace)
        {
            return var;
        }
    }

    return nullptr;
}

bool Mmu::variableExists(uint32_t pid, std::string var_name)
{
    return getVariable(pid, var_name) != nullptr;
}

std::vector<Variable*> Mmu::getVariables(uint32_t pid)
{
    Process *proc = findProcess(pid);
    if (proc == nullptr)
    {
        return std::vector<Variable*>();
    }

    return proc->variables;
}

bool Mmu::allocateFromFreeSpace(uint32_t pid, std::string var_name, DataType type, uint32_t size, uint32_t element_count, uint32_t element_size, uint32_t &out_address)
{
    Process *proc = findProcess(pid);
    if (proc == nullptr)
    {
        return false;
    }

    std::sort(proc->variables.begin(), proc->variables.end(), [](Variable *a, Variable *b)
    {
        return a->virtual_address < b->virtual_address;
    });

    for (size_t i = 0; i < proc->variables.size(); i++)
    {
        Variable *hole = proc->variables[i];
        if (hole == nullptr)
        {
            continue;
        }

        if (hole->type != DataType::FreeSpace)
        {
            continue;
        }

        bool last_hole = (i == proc->variables.size() - 1);
        if (hole->size < size && !last_hole)
        {
            continue;
        }

        out_address = hole->virtual_address;
        addVariableToProcess(pid, var_name, type, size, out_address, element_count, element_size);

        if (hole->size <= size)
        {
            delete hole;
            proc->variables.erase(proc->variables.begin() + static_cast<long>(i));
        }
        else
        {
            hole->virtual_address += size;
            hole->size -= size;
        }

        return true;
    }

    return false;
}

bool Mmu::removeVariable(uint32_t pid, std::string var_name, Variable *removed)
{
    Process *proc = findProcess(pid);
    if (proc == nullptr)
    {
        return false;
    }

    for (size_t i = 0; i < proc->variables.size(); i++)
    {
        Variable *var = proc->variables[i];
        if (var != nullptr && var->name == var_name && var->type != DataType::FreeSpace)
        {
            if (removed != nullptr)
            {
                *removed = *var;
            }

            Variable *hole = new Variable();
            hole->name = "<FREE_SPACE>";
            hole->type = DataType::FreeSpace;
            hole->virtual_address = var->virtual_address;
            hole->size = var->size;
            hole->element_count = 0;
            hole->element_size = 1;

            delete var;
            proc->variables[i] = hole;
            mergeFreeSpace(pid);
            return true;
        }
    }

    return false;
}

bool Mmu::removeVariable(uint32_t pid, std::string var_name)
{
    return removeVariable(pid, var_name, nullptr);
}

bool Mmu::removeProcess(uint32_t pid)
{
    for (size_t i = 0; i < _processes.size(); i++)
    {
        Process *proc = _processes[i];
        if (proc == nullptr || proc->pid != pid)
        {
            continue;
        }

        for (size_t j = 0; j < proc->variables.size(); j++)
        {
            delete proc->variables[j];
        }
        proc->variables.clear();
        delete proc;
        _processes.erase(_processes.begin() + static_cast<long>(i));
        return true;
    }

    return false;
}

void Mmu::mergeFreeSpace(uint32_t pid)
{
    Process *proc = findProcess(pid);
    if (proc == nullptr)
    {
        return;
    }

    std::sort(proc->variables.begin(), proc->variables.end(), [](Variable *a, Variable *b)
    {
        return a->virtual_address < b->virtual_address;
    });

    std::vector<Variable*> merged;
    for (size_t i = 0; i < proc->variables.size(); i++)
    {
        Variable *cur = proc->variables[i];
        if (merged.empty())
        {
            merged.push_back(cur);
            continue;
        }

        Variable *prev = merged.back();
        if (prev->type == DataType::FreeSpace && cur->type == DataType::FreeSpace &&
            prev->virtual_address + prev->size == cur->virtual_address)
        {
            prev->size += cur->size;
            delete cur;
        }
        else
        {
            merged.push_back(cur);
        }
    }

    proc->variables = merged;
}

void Mmu::print()
{
    int i, j;

    std::cout << " PID  | Variable Name | Virtual Addr | Size" << std::endl;
    std::cout << "------+---------------+--------------+------------" << std::endl;
    for (i = 0; i < _processes.size(); i++)
    {
        std::sort(_processes[i]->variables.begin(), _processes[i]->variables.end(), [](Variable *a, Variable *b)
        {
            return a->virtual_address < b->virtual_address;
        });

        for (j = 0; j < _processes[i]->variables.size(); j++)
        {
            // TODO: print all variables (excluding those of type DataType::FreeSpace)
            Variable *var = _processes[i]->variables[j];
            if (var == nullptr || var->type == DataType::FreeSpace)
            {
                continue;
            }

            std::cout << std::setw(5) << _processes[i]->pid
                      << " | " << std::setw(13) << var->name
                      << " |   0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << var->virtual_address
                      << std::dec << std::setfill(' ') << " | " << std::setw(10) << var->size
                      << std::endl;
        }
    }
}
