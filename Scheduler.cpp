//
//  Scheduler.cpp
//  CloudSim
//
//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
//

#include "Scheduler.hpp"
#include <climits>

static bool migrating = false;
static unsigned active_machines = 16;

static Priority_t PriorityFromSLA(SLAType_t s) {
    if(s == SLA0) return HIGH_PRIORITY;
    if(s == SLA1) return MID_PRIORITY;
    return LOW_PRIORITY;
}

void Scheduler::Init() {
    unsigned total = Machine_GetTotal();
    SimOutput("Scheduler::Init(): Total number of machines is " + to_string(total), 3);
    SimOutput("Scheduler::Init(): Initializing scheduler (Greedy)", 1);

    active_machines = total;
    vms.clear();
    machines.clear();
    vms.reserve(active_machines);
    machines.reserve(active_machines);

    for(unsigned i = 0; i < active_machines; i++) {
        MachineId_t mid = MachineId_t(i);
        MachineInfo_t mi = Machine_GetInfo(mid);
        if(mi.s_state != S0) Machine_SetState(mid, S0);
        machines.push_back(mid);
    }
}

void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    (void)time;
    (void)vm_id;
    migrating = false;
}

void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
    (void)now;

    CPUType_t need_cpu = RequiredCPUType(task_id);
    VMType_t  need_vm  = RequiredVMType(task_id);
    bool      need_gpu = IsTaskGPUCapable(task_id);
    unsigned  need_mem = GetTaskMemory(task_id);
    Priority_t priority = PriorityFromSLA(RequiredSLA(task_id));

    int best_i = -1;
    double best_slack = -1.0;

    // finding the best machine that matches the task requirements
    for(unsigned i = 0; i < machines.size(); i++) {
        MachineInfo_t mi = Machine_GetInfo(machines[i]);
        if(mi.s_state != S0) continue;
        if(mi.cpu != need_cpu) continue; 
        if(mi.memory_used + need_mem > mi.memory_size) continue;

        // calculating slack
        double u = 0.0;
        if(mi.num_cpus > 0) {
            u = (double)mi.active_tasks / (double)mi.num_cpus;
            if(u > 1.0) u = 1.0;
        }

        double v = (double)(mi.memory_used + need_mem) / (double)mi.memory_size;
        double slack = 1.0 - (u + v);

        // essentially a preference for GPU machines
        if(need_gpu && mi.gpus) slack += 0.05;

        if(slack > best_slack) {
            best_slack = slack;
            best_i = (int)i;
        }
    }

    if(best_i >= 0) {
        MachineId_t host = machines[best_i];

        VMId_t chosen_vm = 0;
        bool found = false;

        // reusing an existing VM if possible
        for(unsigned i = 0; i < vms.size(); i++) {
            VMInfo_t vi = VM_GetInfo(vms[i]);
            if(vi.machine_id == host && vi.vm_type == need_vm && vi.cpu == need_cpu) {
                chosen_vm = vms[i];
                found = true;
                break;
            }
        }

        // creating a new VM if needed
        if(!found) {
            MachineInfo_t mi = Machine_GetInfo(host);
            VMId_t vm = VM_Create(need_vm, mi.cpu); 
            VM_Attach(vm, host);
            vms.push_back(vm);
            chosen_vm = vm;
        }

        VM_AddTask(chosen_vm, task_id, priority);
        SimOutput("Scheduler::NewTask(): Task " + to_string(task_id) +
                  " assigned to VM " + to_string(chosen_vm) +
                  " on machine " + to_string(host), 3);
        return;
    }

    SimOutput("Scheduler::NewTask(): No compatible host found for task " +
              to_string(task_id) + " - leaving unallocated", 0);
}

void Scheduler::PeriodicCheck(Time_t now) {
    (void)now;
}

void Scheduler::Shutdown(Time_t time) {
    for(auto & vm: vms) {
        VM_Shutdown(vm);
    }
    SimOutput("SimulationComplete(): Finished!", 4);
    SimOutput("SimulationComplete(): Time is " + to_string(time), 4);
}

void Scheduler::TaskComplete(Time_t now, TaskId_t task_id) {
    SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) +
              " is complete at " + to_string(now), 4);
}

static Scheduler Scheduler;

void InitScheduler() {
    SimOutput("InitScheduler(): Initializing scheduler", 4);
    Scheduler.Init();
}

void HandleNewTask(Time_t time, TaskId_t task_id) {
    SimOutput("HandleNewTask(): Received new task " + to_string(task_id) +
              " at time " + to_string(time), 4);
    Scheduler.NewTask(time, task_id);
}

void HandleTaskCompletion(Time_t time, TaskId_t task_id) {
    SimOutput("HandleTaskCompletion(): Task " + to_string(task_id) +
              " completed at time " + to_string(time), 4);
    Scheduler.TaskComplete(time, task_id);
}

void MemoryWarning(Time_t time, MachineId_t machine_id) {
    SimOutput("MemoryWarning(): Overflow at " + to_string(machine_id) +
              " was detected at time " + to_string(time), 0);
}

void MigrationDone(Time_t time, VMId_t vm_id) {
    SimOutput("MigrationDone(): Migration of VM " + to_string(vm_id) +
              " was completed at time " + to_string(time), 4);
    Scheduler.MigrationComplete(time, vm_id);
    migrating = false;
}

void SchedulerCheck(Time_t time) {
    SimOutput("SchedulerCheck(): SchedulerCheck() called at " +
              to_string(time), 4);
    Scheduler.PeriodicCheck(time);
}

void SimulationComplete(Time_t time) {
    cout << "SLA violation report" << endl;
    cout << "SLA0: " << GetSLAReport(SLA0) << "%" << endl;
    cout << "SLA1: " << GetSLAReport(SLA1) << "%" << endl;
    cout << "SLA2: " << GetSLAReport(SLA2) << "%" << endl;
    cout << "Total Energy " << Machine_GetClusterEnergy() << "KW-Hour" << endl;
    cout << "Simulation run finished in " << double(time)/1000000 << " seconds" << endl;
    SimOutput("SimulationComplete(): Simulation finished at time " + to_string(time), 4);
    Scheduler.Shutdown(time);
}

void SLAWarning(Time_t time, TaskId_t task_id) {
    (void)time;
    (void)task_id;
}

void StateChangeComplete(Time_t time, MachineId_t machine_id) {
    (void)time;
    (void)machine_id;
}
