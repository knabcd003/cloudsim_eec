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
    // Find the parameters of the clusters
    // Get the total number of machines
    // For each machine:
    //      Get the type of the machine
    //      Get the memory of the machine
    //      Get the number of CPUs
    //      Get if there is a GPU or not
    //
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
    // Update your data structure. The VM now can receive new tasks
    (void)time;
    (void)vm_id;
    migrating = false;
}

void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
    // Get the task parameters
    //  IsGPUCapable(task_id);
    //  GetMemory(task_id);
    //  RequiredVMType(task_id);
    //  RequiredSLA(task_id);
    //  RequiredCPUType(task_id);
    // Decide to attach the task to an existing VM, 
    //      vm.AddTask(taskid, Priority_T priority); or
    // Create a new VM, attach the VM to a machine
    //      VM vm(type of the VM)
    //      vm.Attach(machine_id);
    //      vm.AddTask(taskid, Priority_t priority) or
    // Turn on a machine, create a new VM, attach it to the VM, then add the task
    //
    // Turn on a machine, migrate an existing VM from a loaded machine....
    //
    // Other possibilities as desired
    (void)now;

    CPUType_t need_cpu = RequiredCPUType(task_id);
    VMType_t need_vm = RequiredVMType(task_id);
    bool need_gpu = IsTaskGPUCapable(task_id);
    unsigned need_mem = GetTaskMemory(task_id);
    Priority_t priority = PriorityFromSLA(RequiredSLA(task_id));

    int best_i = -1;
    double best_slack = -1.0;

    for(unsigned i = 0; i < machines.size(); i++) {
        MachineInfo_t mi = Machine_GetInfo(machines[i]);

        if(mi.s_state != S0) continue;
        // testing allowing different cpu types for now
        //if(mi.cpu != need_cpu) continue; 
        if(mi.memory_used + need_mem > mi.memory_size) continue;

        double u = 0.0;
        if(mi.num_cpus > 0) u = (double)mi.active_tasks / (double)mi.num_cpus;
        double v = (double)(mi.memory_used + need_mem) / (double)mi.memory_size;
        double slack = 1.0 - (u + v);
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
        for(unsigned i = 0; i < vms.size(); i++) {
            VMInfo_t vi = VM_GetInfo(vms[i]);
            if(vi.machine_id == host && vi.vm_type == need_vm && vi.cpu == need_cpu) {
                chosen_vm = vms[i];
                found = true;
                break;
            }
        }

        if(!found) {
            MachineInfo_t mh = Machine_GetInfo(host);
            VMId_t vm = VM_Create(need_vm, mh.cpu);
            VM_Attach(vm, host);
            vms.push_back(vm);
            chosen_vm = vm;
        }

        VM_AddTask(chosen_vm, task_id, priority);
        SimOutput("Scheduler::NewTask(): Task " + to_string(task_id) + " assigned to VM " + to_string(chosen_vm) + " on machine " + to_string(host), 3);
        return;
    }

    SimOutput("Scheduler::NewTask(): No compatible host found for task " + to_string(task_id) + " - leaving unallocated", 0);
}

void Scheduler::PeriodicCheck(Time_t now) {
    // This method should be called from SchedulerCheck()
    // SchedulerCheck is called periodically by the simulator to allow you to monitor, make decisions, adjustments, etc.
    // Unlike the other invocations of the scheduler, this one doesn't report any specific event
    // Recommendation: Take advantage of this function to do some monitoring and adjustments as necessary
    (void)now;
}

void Scheduler::Shutdown(Time_t time) {
    // Do your final reporting and bookkeeping here.
    // Report about the total energy consumed
    // Report about the SLA compliance
    // Shutdown everything to be tidy :-)
    for(auto & vm: vms) {
        VM_Shutdown(vm);
    }
    SimOutput("SimulationComplete(): Finished!", 4);
    SimOutput("SimulationComplete(): Time is " + to_string(time), 4);
}

void Scheduler::TaskComplete(Time_t now, TaskId_t task_id) {
    // Do any bookkeeping necessary for the data structures
    // Decide if a machine is to be turned off, slowed down, or VMs to be migrated according to your policy
    // This is an opportunity to make any adjustments to optimize performance/energy
    SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) + " is complete at " + to_string(now), 4);
}

// Public interface below

static Scheduler Scheduler;

void InitScheduler() {
    SimOutput("InitScheduler(): Initializing scheduler", 4);
    Scheduler.Init();
}

void HandleNewTask(Time_t time, TaskId_t task_id) {
    SimOutput("HandleNewTask(): Received new task " + to_string(task_id) + " at time " + to_string(time), 4);
    Scheduler.NewTask(time, task_id);
}

void HandleTaskCompletion(Time_t time, TaskId_t task_id) {
    SimOutput("HandleTaskCompletion(): Task " + to_string(task_id) + " completed at time " + to_string(time), 4);
    Scheduler.TaskComplete(time, task_id);
}

void MemoryWarning(Time_t time, MachineId_t machine_id) {
    // The simulator is alerting you that machine identified by machine_id is overcommitted
    SimOutput("MemoryWarning(): Overflow at " + to_string(machine_id) + " was detected at time " + to_string(time), 0);
}

void MigrationDone(Time_t time, VMId_t vm_id) {
    // The function is called on to alert you that migration is complete
    SimOutput("MigrationDone(): Migration of VM " + to_string(vm_id) + " was completed at time " + to_string(time), 4);
    Scheduler.MigrationComplete(time, vm_id);
    migrating = false;
}

void SchedulerCheck(Time_t time) {
    // This function is called periodically by the simulator, no specific event
    SimOutput("SchedulerCheck(): SchedulerCheck() called at " + to_string(time), 4);
    Scheduler.PeriodicCheck(time);
}

void SimulationComplete(Time_t time) {
    // This function is called before the simulation terminates Add whatever you feel like.
    cout << "SLA violation report" << endl;
    cout << "SLA0: " << GetSLAReport(SLA0) << "%" << endl;
    cout << "SLA1: " << GetSLAReport(SLA1) << "%" << endl;
    cout << "SLA2: " << GetSLAReport(SLA2) << "%" << endl;     // SLA3 do not have SLA violation issues
    cout << "Total Energy " << Machine_GetClusterEnergy() << "KW-Hour" << endl;
    cout << "Simulation run finished in " << double(time)/1000000 << " seconds" << endl;
    SimOutput("SimulationComplete(): Simulation finished at time " + to_string(time), 4);
    Scheduler.Shutdown(time);
}

void SLAWarning(Time_t time, TaskId_t task_id) {
    // If a task is late, boost its priority
    (void)time;
    (void)task_id;
}

void StateChangeComplete(Time_t time, MachineId_t machine_id) {
    // Called in response to an earlier request to change the state of a machine
    (void)time;
    (void)machine_id;
}
