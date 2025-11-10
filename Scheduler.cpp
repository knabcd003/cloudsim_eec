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

// Helper: convert SLA type to task priority
static Priority_t PriorityFromSLA(SLAType_t s) {
    switch (s) {
        case SLA0: return HIGH_PRIORITY;
        case SLA1: return MID_PRIORITY;
        default:   return LOW_PRIORITY;
    }
}

// Helper: choose a default VM type for a machine CPU
static VMType_t DefaultVMTypeForCPU(CPUType_t cpu) {
    // POWER tasks in inputs use AIX, others use LINUX
    if (cpu == POWER)  return AIX;
    return LINUX;
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
    SimOutput("Scheduler::Init(): Initializing scheduler (Load Balancing)", 1);

    // For pure load balancing, we will consider all machines.
    active_machines = total;

    vms.reserve(active_machines);
    machines.reserve(active_machines);

    // Create one VM per machine and attach it.
    // VM CPU type matches machine CPU type.
    // VM type chosen based on CPU type (AIX on POWER, LINUX otherwise).
    for (unsigned i = 0; i < active_machines; i++) {
        MachineId_t mid = MachineId_t(i);
        MachineInfo_t mi = Machine_GetInfo(mid);

        // Ensure machine is on and ready
        if (mi.s_state != S0) {
            Machine_SetState(mid, S0);
        }

        VMType_t vm_type = DefaultVMTypeForCPU(mi.cpu);
        VMId_t vm = VM_Create(vm_type, mi.cpu);

        vms.push_back(vm);
        machines.push_back(mid);
        VM_Attach(vm, mid);
    }

    if (!vms.empty()) {
        if (vms.size() >= 2) {
            SimOutput("Scheduler::Init(): VM ids are " +
                      to_string(vms[0]) + " and " + to_string(vms[1]), 3);
        } else {
            SimOutput("Scheduler::Init(): VM id is " + to_string(vms[0]), 3);
        }
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
    // or create a new one if necessary
    (void)now;

    CPUType_t need_cpu    = RequiredCPUType(task_id);
    VMType_t  need_vm     = RequiredVMType(task_id);
    bool      need_gpu    = IsTaskGPUCapable(task_id);
    unsigned  need_mem    = GetTaskMemory(task_id);
    Priority_t priority   = PriorityFromSLA(RequiredSLA(task_id));

    int best_idx = -1;
    unsigned best_load = UINT_MAX;

    // Load balancing:
    // Among all VMs, pick the least-loaded compatible machine
    for (unsigned i = 0; i < vms.size(); ++i) {
        VMInfo_t vminfo = VM_GetInfo(vms[i]);
        MachineInfo_t mi = Machine_GetInfo(vminfo.machine_id);

        // Machine must be on
        if (mi.s_state != S0)
            continue;

        // CPU type must match
        if (vminfo.cpu != need_cpu)
            continue;

        // VM type must match required VM type
        if (vminfo.vm_type != need_vm)
            continue;

        // If task needs GPU, machine must have GPU
        if (need_gpu && !mi.gpus)
            continue;

        // Check memory capacity
        if (mi.memory_used + need_mem > mi.memory_size)
            continue;

        // Use active_tasks for load balancing
        if (mi.active_tasks < best_load) {
            best_load = mi.active_tasks;
            best_idx = static_cast<int>(i);
        }
    }

    if (best_idx >= 0) {
        VM_AddTask(vms[best_idx], task_id, priority);
        SimOutput("Scheduler::NewTask(): Task " + to_string(task_id) +
                  " assigned to VM " + to_string(vms[best_idx]) +
                  " on machine " + to_string(machines[best_idx]), 4);
        return;
    }

    // No compatible host found:
    // For this project phase, treat this as an SLA violation (unallocated),
    // rather than forcing an incompatible placement that crashes.
    SimOutput("Scheduler::NewTask(): No compatible host found for task " +
              to_string(task_id) + " - leaving unallocated", 0);
}

void Scheduler::PeriodicCheck(Time_t now) {
    // This method should be called from SchedulerCheck()
    // SchedulerCheck is called periodically by the simulator to allow you to monitor, make decisions, adjustments, etc.
    // Unlike the other invocations of the scheduler, this one doesn't report any specific event
    // Recommendation: Take advantage of this function to do some monitoring and adjustments as necessary
    (void)now;

    // For the basic load balancing algorithm, we do not do periodic migrations or power changes.
}

void Scheduler::Shutdown(Time_t time) {
    // Do your final reporting and bookkeeping here.
    // Report about the total energy consumed
    // Report about the SLA compliance
    // Shutdown everything to be tidy :-)
    for (auto &vm : vms) {
        VM_Shutdown(vm);
    }
    SimOutput("SimulationComplete(): Finished!", 4);
    SimOutput("SimulationComplete(): Time is " + to_string(time), 4);
}

void Scheduler::TaskComplete(Time_t now, TaskId_t task_id) {
    // Do any bookkeeping necessary for the data structures
    // Decide if a machine is to be turned off, slowed down, or VMs to be migrated according to your policy
    // This is an opportunity to make any adjustments to optimize performance/energy
    SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) +
              " is complete at " + to_string(now), 4);

    // No dynamic power or migration behavior for the plain load balancer.
}

// Public interface below

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
    // The simulator is alerting you that machine identified by machine_id is overcommitted
    SimOutput("MemoryWarning(): Overflow at " + to_string(machine_id) +
              " was detected at time " + to_string(time), 0);
}

void MigrationDone(Time_t time, VMId_t vm_id) {
    // The function is called on to alert you that migration is complete
    SimOutput("MigrationDone(): Migration of VM " + to_string(vm_id) +
              " was completed at time " + to_string(time), 4);
    Scheduler.MigrationComplete(time, vm_id);
    migrating = false;
}

void SchedulerCheck(Time_t time) {
    // This function is called periodically by the simulator, no specific event
    SimOutput("SchedulerCheck(): SchedulerCheck() called at " +
              to_string(time), 4);
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
    SimOutput("SimulationComplete(): Simulation finished at time " +
              to_string(time), 4);

    Scheduler.Shutdown(time);
}

void SLAWarning(Time_t time, TaskId_t task_id) {
    // If a task is late, boost its priority
    (void)time;
    SetTaskPriority(task_id, HIGH_PRIORITY);
}

void StateChangeComplete(Time_t time, MachineId_t machine_id) {
    // Called in response to an earlier request to change the state of a machine
    (void)time;
    (void)machine_id;
}