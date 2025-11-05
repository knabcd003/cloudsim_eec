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

void Scheduler::Init() {
    // Find the parameters of the clusters
    // Get the total number of machines
    // For each machine:
    //      Get the type of the machine
    //      Get the memory of the machine
    //      Get the number of CPUs
    //      Get if there is a GPU or not
    //
    SimOutput("Scheduler::Init(): Total number of machines is " + to_string(Machine_GetTotal()), 3);
    SimOutput("Scheduler::Init(): Initializing scheduler (Load Balancing)", 1);

    // Adjust active_machines if total is smaller
    if (active_machines > Machine_GetTotal())
        active_machines = Machine_GetTotal();

    // Create one VM per active machine
    for (unsigned i = 0; i < active_machines; i++)
        vms.push_back(VM_Create(LINUX, X86));
    for (unsigned i = 0; i < active_machines; i++)
        machines.push_back(MachineId_t(i));
    for (unsigned i = 0; i < active_machines; i++)
        VM_Attach(vms[i], machines[i]);

    // Optional: change CPU P-states if dynamic flag is enabled
    bool dynamic = false;
    if (dynamic)
        for (unsigned i = 0; i < 4; i++)
            for (unsigned j = 0; j < 8; j++)
                Machine_SetCorePerformance(MachineId_t(0), j, P3);

    // Turn off ARM machines (if any exist beyond index 24)
    for (unsigned i = 24; i < Machine_GetTotal(); i++)
        Machine_SetState(MachineId_t(i), S5);

    SimOutput("Scheduler::Init(): VM ids are " + to_string(vms[0]) + " and " + to_string(vms[1]), 3);
}

void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    // Update your data structure. The VM now can receive new tasks
    (void)time; (void)vm_id;
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

    CPUType_t need_cpu = RequiredCPUType(task_id);
    unsigned need_mem  = GetTaskMemory(task_id);
    Priority_t priority = PriorityFromSLA(RequiredSLA(task_id));

    // Find least-loaded compatible machine with enough memory
    int best_idx = -1;
    unsigned best_load = UINT_MAX;

    for (unsigned i = 0; i < machines.size(); ++i) {
        MachineInfo_t mi = Machine_GetInfo(machines[i]);
        if (mi.s_state != S0) continue;
        if (mi.cpu != need_cpu) continue;
        if (mi.memory_size < mi.memory_used + need_mem) continue;

        if (mi.active_tasks < best_load) {
            best_load = mi.active_tasks;
            best_idx = static_cast<int>(i);
        }
    }

    // Assign to best machine or fallback
    if (best_idx >= 0) {
        VM_AddTask(vms[best_idx], task_id, priority);
    } else {
        SimOutput("Scheduler::NewTask(): No feasible host found, using fallback placement", 0);
        VM_AddTask(vms[task_id % active_machines], task_id, priority);
    }
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
    // Removed demo migration block (caused segfaults and is unnecessary)
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
    SetTaskPriority(task_id, HIGH_PRIORITY);
}

void StateChangeComplete(Time_t time, MachineId_t machine_id) {
    // Called in response to an earlier request to change the state of a machine
    (void)time; (void)machine_id;
}
