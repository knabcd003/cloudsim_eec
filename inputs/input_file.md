machine class:
{
# Heterogeneous X86 machines for standard workloads
        Number of machines: 8
        CPU type: X86
        Number of cores: 8
        Memory: 32768
        S-States: [120, 100, 80, 60, 40, 20, 0]
        P-States: [12, 8, 6, 4]
        C-States: [12, 3, 1, 0]
        MIPS: [2000, 1600, 1200, 800]
        GPUs: no
}

machine class:
{
# POWER machines with GPUs for AI and HPC workloads
        Number of machines: 4
        CPU type: POWER
        Number of cores: 16
        Memory: 65536
        S-States: [220, 180, 150, 120, 80, 30, 0]
        P-States: [22, 16, 11, 7]
        C-States: [22, 5, 2, 0]
        MIPS: [5000, 3800, 2700, 1800]
        GPUs: yes
}

machine class:
{
# ARM machines for lightweight streaming and background tasks
        Number of machines: 6
        CPU type: ARM
        Number of cores: 8
        Memory: 16384
        S-States: [80, 65, 50, 35, 22, 8, 0]
        P-States: [8, 6, 4, 2]
        C-States: [8, 2, 1, 0]
        MIPS: [1200, 900, 600, 300]
        GPUs: no
}

task class:
{
# Web workload: frequent, short tasks with strict SLA
        Start time: 60000
        End time : 1000000
        Inter arrival: 6000
        Expected runtime: 800000
        Memory: 8
        VM type: LINUX
        GPU enabled: no
        SLA type: SLA1
        CPU type: X86
        Task type: WEB
        Seed: 530001
}

task class:
{
# Streaming workload: moderate runtime and real-time sensitive
        Start time: 120000
        End time : 1600000
        Inter arrival: 20000
        Expected runtime: 4000000
        Memory: 10
        VM type: LINUX_RT
        GPU enabled: no
        SLA type: SLA0
        CPU type: ARM
        Task type: STREAM
        Seed: 530002
}

task class:
{
# AI workload: long-running GPU-intensive tasks
        Start time: 300000
        End time : 3000000
        Inter arrival: 40000
        Expected runtime: 20000000
        Memory: 32
        VM type: LINUX
        GPU enabled: yes
        SLA type: SLA1
        CPU type: POWER
        Task type: AI
        Seed: 530003
}

task class:
{
# Crypto workload: CPU-heavy and medium duration
        Start time: 150000
        End time : 2200000
        Inter arrival: 15000
        Expected runtime: 6000000
        Memory: 12
        VM type: LINUX
        GPU enabled: no
        SLA type: SLA2
        CPU type: X86
        Task type: CRYPTO
        Seed: 530004
}

task class:
{
# Background HPC workload: best-effort tasks to fill idle machines
        Start time: 1000000
        End time : 4000000
        Inter arrival: 60000
        Expected runtime: 30000000
        Memory: 48
        VM type: LINUX
        GPU enabled: yes
        SLA type: SLA3
        CPU type: POWER
        Task type: HPC
        Seed: 530005
}
