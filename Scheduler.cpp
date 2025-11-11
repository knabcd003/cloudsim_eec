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

// convert SLA type to task priority
static Priority_t PriorityFromSLA(SLAType_t s) {
    switch (s) {
        case SLA0: return HIGH_PRIORITY;
        case SLA1: return MID_PRIORITY;
        default:   return LOW_PRIORITY;
    }
}

// choose a default VM type for a machine CPU
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
    SimOutput("Scheduler::Init(): Initializing scheduler (SLA-Partitioned)", 1);

    // Use all machines
    active_machines = total;

    vms.reserve(active_machines);
    machines.reserve(active_machines);

    // Create one VM per machine and attach it.
    // VM CPU type matches machine CPU type.
    // VM type chosen based on CPU type (AIX on POWER, LINUX otherwise).
    for (unsigned i = 0; i < active_machines; i++) {
        MachineId_t mid = MachineId_t(i);
        MachineInfo_t mi = Machine_GetInfo(mid);

        // Make sure machine is on at start
            Machine_SetState(mid, S0);
            mi = Machine_GetInfo(mid);
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
    // The VM now can receive new tasks
    (void)time;
    (void)vm_id;
    migrating = false;
}

static bool CanHost(const VMInfo_t &vminfo,
                    const MachineInfo_t &mi,
                    CPUType_t need_cpu,
                    VMType_t need_vm,
                    bool need_gpu,
                    unsigned need_mem)
{
    if (mi.s_state != S0)                return false;
    if (vminfo.cpu != need_cpu)          return false;
    if (vminfo.vm_type != need_vm)       return false;
    if (need_gpu && !mi.gpus)            return false;
    if (mi.memory_used + need_mem > mi.memory_size)
        return false;
    return true;
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

    CPUType_t  need_cpu   = RequiredCPUType(task_id);
    VMType_t   need_vm    = RequiredVMType(task_id);
    bool       need_gpu   = IsTaskGPUCapable(task_id);
    unsigned   need_mem   = GetTaskMemory(task_id);
    SLAType_t  sla        = RequiredSLA(task_id);
    Priority_t priority   = PriorityFromSLA(sla);

    // Static partition:
    // Use the first half of machines as a "high priority" pool,
    // and the second half as a "best effort" pool.
    unsigned total = vms.size();
    unsigned high_end = total / 2;
    unsigned low_begin = high_end;

    int chosen = -1;

    if (sla == SLA0 || sla == SLA1) {
        // High-SLA tasks:
        //   1) Try FIRST-FIT in the high-priority pool [0, high_end)
        //   2) If none, fallback to FIRST-FIT over all machines
        for (unsigned i = 0; i < high_end; ++i) {
            VMInfo_t      vminfo = VM_GetInfo(vms[i]);
            MachineInfo_t mi     = Machine_GetInfo(vminfo.machine_id);
            if (CanHost(vminfo, mi, need_cpu, need_vm, need_gpu, need_mem)) {
                chosen = static_cast<int>(i);
                break;
            }
        }

        if (chosen < 0) {
            for (unsigned i = 0; i < total; ++i) {
                VMInfo_t      vminfo = VM_GetInfo(vms[i]);
                MachineInfo_t mi     = Machine_GetInfo(vminfo.machine_id);
                if (CanHost(vminfo, mi, need_cpu, need_vm, need_gpu, need_mem)) {
                    chosen = static_cast<int>(i);
                    break;
                }
            }
        }
    } else {
        // Low-SLA tasks (SLA2, SLA3):
        //   1) Try FIRST-FIT in the best-effort pool [low_begin, total)
        //   2) If none, fallback to FIRST-FIT over all machines
        // best-effort pool
        for (unsigned i = low_begin; i < total; ++i) {
            VMInfo_t      vminfo = VM_GetInfo(vms[i]);
            MachineInfo_t mi     = Machine_GetInfo(vminfo.machine_id);
            if (CanHost(vminfo, mi, need_cpu, need_vm, need_gpu, need_mem)) {
                chosen = static_cast<int>(i);
                break;
            }
        }

        if (chosen < 0) {
            for (unsigned i = 0; i < total; ++i) {
                VMInfo_t      vminfo = VM_GetInfo(vms[i]);
                MachineInfo_t mi     = Machine_GetInfo(vminfo.machine_id);
                if (CanHost(vminfo, mi, need_cpu, need_vm, need_gpu, need_mem)) {
                    chosen = static_cast<int>(i);
                    break;
                }
            }
        }
    }

    if (chosen >= 0) {
        VM_AddTask(vms[chosen], task_id, priority);
        SimOutput("Scheduler::NewTask(): Task " + to_string(task_id) +
                  " assigned to VM " + to_string(vms[chosen]) +
                  " on machine " + to_string(machines[chosen]), 4);
        return;
    }

    // No compatible host found:
    // Treat as an SLA violation (unallocated),
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

    // No dynamic power or migrations here.
    // Energy-related behavior is via static partitioning:
    // low-SLA tasks are steered to a subset of machines,
    // leaving others freer for high-SLA workloads.
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

    // All decisions are made in NewTask(); no extra logic here.
}

// Public interface below

static Scheduler SchedulerInstance;

void InitScheduler() {
    SimOutput("InitScheduler(): Initializing scheduler", 4);
    SchedulerInstance.Init();
}

void HandleNewTask(Time_t time, TaskId_t task_id) {
    SimOutput("HandleNewTask(): Received new task " + to_string(task_id) +
              " at time " + to_string(time), 4);
    SchedulerInstance.NewTask(time, task_id);
}

void HandleTaskCompletion(Time_t time, TaskId_t task_id) {
    SimOutput("HandleTaskCompletion(): Task " + to_string(task_id) +
              " completed at time " + to_string(time), 4);
    SchedulerInstance.TaskComplete(time, task_id);
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
    SchedulerInstance.MigrationComplete(time, vm_id);
    migrating = false;
}

void SchedulerCheck(Time_t time) {
    // This function is called periodically by the simulator, no specific event
    SimOutput("SchedulerCheck(): SchedulerCheck() called at " +
              to_string(time), 4);
    SchedulerInstance.PeriodicCheck(time);
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

    SchedulerInstance.Shutdown(time);
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