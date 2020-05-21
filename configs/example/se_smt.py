import spec06_benchmarks
import optparse
import sys
import os

import m5
from m5.defines import buildEnv
from m5.objects import *
from m5.util import addToPath, fatal

addToPath('../')
#addToPath('../ruby')

from ruby import Ruby

from common import Options
from common import Simulation
from common import L3Config
from common import MemConfig
from common import CpuConfig
from common.Caches import *
from common.cpu2000 import *

# Check if KVM support has been enabled, we might need to do VM
# configuration if that's the case.
have_kvm_support = 'BaseKvmCPU' in globals()
def is_kvm_cpu(cpu_class):
    return have_kvm_support and cpu_class != None and \
        issubclass(cpu_class, BaseKvmCPU)

def get_processes(options):
    """Interprets provided options and returns a list of processes"""

    multiprocesses = []
    inputs = []
    outputs = []
    errouts = []
    pargs = []

    workloads = options.cmd.split(';')
    if options.input != "":
        inputs = options.input.split(';')
    if options.output != "":
        outputs = options.output.split(';')
    if options.errout != "":
        errouts = options.errout.split(';')
    if options.options != "":
        pargs = options.options.split(';')

    idx = 0
    for wrkld in workloads:
        process = LiveProcess()
        process.executable = wrkld
        process.cwd = os.getcwd()

        if options.env:
            with open(options.env, 'r') as f:
                process.env = [line.rstrip() for line in f]

        if len(pargs) > idx:
            process.cmd = [wrkld] + pargs[idx].split()
        else:
            process.cmd = [wrkld]

        if len(inputs) > idx:
            process.input = inputs[idx]
        if len(outputs) > idx:
            process.output = outputs[idx]
        if len(errouts) > idx:
            process.errout = errouts[idx]

        multiprocesses.append(process)
        idx += 1

    if options.smt:
        assert(options.cpu_type == "detailed")
        return multiprocesses, idx
    else:
        return multiprocesses, 1


parser = optparse.OptionParser()
Options.addCommonOptions(parser)
Options.addSEOptions(parser)
parser.add_option("-b", "--benchmark", default="",
                 help="The benchmark to be loaded.")

if '--ruby' in sys.argv:
    Ruby.define_options(parser)

(options, args) = parser.parse_args()

if args:
    print "Error: script doesn't take any positional arguments"
    sys.exit(1)

multiprocesses = []
numThreads = 1

if options.benchmark:
    print 'Selected SPEC_CPU2006 benchmark'
    if options.benchmark == 'benchset0':
        print '--> benchset0:'
        pro1 = spec06_benchmarks.leslie3d
        multiprocesses.append(pro1)
        pro2 = spec06_benchmarks.soplex
        multiprocesses.append(pro2)
        pro3 = spec06_benchmarks.milc
        multiprocesses.append(pro3)
        pro4 = spec06_benchmarks.cactusADM
        multiprocesses.append(pro4)
#        pro5 = spec06_benchmarks.zeusmp
#        multiprocesses.append(pro5)
#        pro6 = spec06_benchmarks.bzip2
#        multiprocesses.append(pro6)
#        pro7 = spec06_benchmarks.wrf
#        multiprocesses.append(pro7)
#        pro8 = spec06_benchmarks.gromacs
#        multiprocesses.append(pro8)
    elif options.benchmark == 'benchset1':
        print '--> benchset1:'
        pro1 = spec06_benchmarks.mcf
        multiprocesses.append(pro1)
        pro2 = spec06_benchmarks.bzip2
        multiprocesses.append(pro2)
        pro3 = spec06_benchmarks.libquantum
        multiprocesses.append(pro3)
        pro4 = spec06_benchmarks.lbm
        multiprocesses.append(pro4)
#        pro5 = spec06_benchmarks.xalancbmk
#        multiprocesses.append(pro5)
#        pro6 = spec06_benchmarks.sphinx3
#        multiprocesses.append(pro6)
#        pro7 = spec06_benchmarks.wrf
#        multiprocesses.append(pro7)
#        pro8 = spec06_benchmarks.zeusmp
#        multiprocesses.append(pro8)
    elif options.benchmark == 'benchset2':
        print '--> benchset2:'
        pro1 = spec06_benchmarks.xalancbmk
        multiprocesses.append(pro1)
        pro2 = spec06_benchmarks.sphinx3
        multiprocesses.append(pro2)
        pro3 = spec06_benchmarks.h264ref
        multiprocesses.append(pro3)
        pro4 = spec06_benchmarks.omnetpp
        multiprocesses.append(pro4)
#        pro5 = spec06_benchmarks.leslie3d
#        multiprocesses.append(pro5)
#        pro6 = spec06_benchmarks.gobmk
#        multiprocesses.append(pro6)
#        pro7 = spec06_benchmarks.wrf
#        multiprocesses.append(pro7)
#        pro8 = spec06_benchmarks.sjeng
#        multiprocesses.append(pro8)
    elif options.benchmark == 'benchset3':
        print '--> benchset3:'
        pro1 = spec06_benchmarks.bzip2
        multiprocesses.append(pro1)
        pro2 = spec06_benchmarks.gromacs
        multiprocesses.append(pro2)
        pro3 = spec06_benchmarks.gobmk
        multiprocesses.append(pro3)
        pro4 = spec06_benchmarks.omnetpp
        multiprocesses.append(pro4)
#        pro5 = spec06_benchmarks.leslie3d
#        multiprocesses.append(pro5)
#        pro6 = spec06_benchmarks.dealII
#        multiprocesses.append(pro6)
#        pro7 = spec06_benchmarks.zeusmp
#        multiprocesses.append(pro7)
#        pro8 = spec06_benchmarks.sjeng
#        multiprocesses.append(pro8)
    elif options.benchmark == 'benchset4':
        print '--> benchset4:'
        pro1 = spec06_benchmarks.lbm
        multiprocesses.append(pro1)
        pro2 = spec06_benchmarks.lbm
        multiprocesses.append(pro2)
        pro3 = spec06_benchmarks.lbm
        multiprocesses.append(pro3)
        pro4 = spec06_benchmarks.lbm
        multiprocesses.append(pro4)
        pro5 = spec06_benchmarks.lbm
        multiprocesses.append(pro5)
#        pro6 = spec06_benchmarks.dealII
#        multiprocesses.append(pro6)
#        pro7 = spec06_benchmarks.povray
#        multiprocesses.append(pro7)
#        pro8 = spec06_benchmarks.libquantum
#        multiprocesses.append(pro8)
    elif options.benchmark == 'benchset5':
        print '--> benchset5:'
        pro1 = spec06_benchmarks.sphinx3
        multiprocesses.append(pro1)
        pro2 = spec06_benchmarks.omnetpp
        multiprocesses.append(pro2)
        pro3 = spec06_benchmarks.gcc
        multiprocesses.append(pro3)
        pro4 = spec06_benchmarks.sjeng
        multiprocesses.append(pro4)
        pro5 = spec06_benchmarks.lbm
        multiprocesses.append(pro5)
        pro6 = spec06_benchmarks.perlbench
        multiprocesses.append(pro6)
        pro7 = spec06_benchmarks.sjeng
        multiprocesses.append(pro7)
        pro8 = spec06_benchmarks.GemsFDTD
        multiprocesses.append(pro8)
    elif options.benchmark == 'benchset6':
        print '--> benchset6:'
        pro1 = spec06_benchmarks.calculix
        multiprocesses.append(pro1)
        pro2 = spec06_benchmarks.libquantum
        multiprocesses.append(pro2)
        pro3 = spec06_benchmarks.gcc
        multiprocesses.append(pro3)
        pro4 = spec06_benchmarks.sjeng
        multiprocesses.append(pro4)
        pro5 = spec06_benchmarks.cactusADM
        multiprocesses.append(pro5)
        pro6 = spec06_benchmarks.povray
        multiprocesses.append(pro6)
        pro7 = spec06_benchmarks.xalancbmk
        multiprocesses.append(pro7)
        pro8 = spec06_benchmarks.gcc
        multiprocesses.append(pro8)
    elif options.benchmark == 'benchset7':
        print '--> benchset7:'
        pro1 = spec06_benchmarks.cactusADM
        multiprocesses.append(pro1)
        pro2 = spec06_benchmarks.gobmk
        multiprocesses.append(pro2)
        pro3 = spec06_benchmarks.tonto
        multiprocesses.append(pro3)
        pro4 = spec06_benchmarks.gromacs
        multiprocesses.append(pro4)
        pro5 = spec06_benchmarks.cactusADM
        multiprocesses.append(pro5)
        pro6 = spec06_benchmarks.perlbench
        multiprocesses.append(pro6)
        pro7 = spec06_benchmarks.cactusADM
        multiprocesses.append(pro7)
        pro8 = spec06_benchmarks.h264ref
        multiprocesses.append(pro8)
    elif options.benchmark == 'benchset8':
        print '--> benchset8:'
        pro1 = spec06_benchmarks.wrf
        multiprocesses.append(pro1)
        pro2 = spec06_benchmarks.sphinx3
        multiprocesses.append(pro2)
        pro3 = spec06_benchmarks.gamess
        multiprocesses.append(pro3)
        pro4 = spec06_benchmarks.astar
        multiprocesses.append(pro4)
        pro5 = spec06_benchmarks.dealII
        multiprocesses.append(pro5)
        pro6 = spec06_benchmarks.omnetpp
        multiprocesses.append(pro6)
        pro7 = spec06_benchmarks.bzip2
        multiprocesses.append(pro7)
        pro8 = spec06_benchmarks.bwaves
        multiprocesses.append(pro8)
    elif options.benchmark == 'benchset9':
        print '--> benchset9:'
        pro1 = spec06_benchmarks.GemsFDTD
        multiprocesses.append(pro1)
        pro2 = spec06_benchmarks.namd
        multiprocesses.append(pro2)
        pro3 = spec06_benchmarks.soplex
        multiprocesses.append(pro3)
        pro4 = spec06_benchmarks.astar
        multiprocesses.append(pro4)
        pro5 = spec06_benchmarks.soplex
        multiprocesses.append(pro5)
        pro6 = spec06_benchmarks.omnetpp
        multiprocesses.append(pro6)
        pro7 = spec06_benchmarks.omnetpp
        multiprocesses.append(pro7)
        pro8 = spec06_benchmarks.bwaves
        multiprocesses.append(pro8)
    elif options.benchmark == 'benchset10':
        print '--> benchset10:'
        pro1 = spec06_benchmarks.povray
        multiprocesses.append(pro1)
        pro2 = spec06_benchmarks.libquantum
        multiprocesses.append(pro2)
        pro3 = spec06_benchmarks.h264ref
        multiprocesses.append(pro3)
        pro4 = spec06_benchmarks.omnetpp
        multiprocesses.append(pro4)
        pro5 = spec06_benchmarks.sphinx3
        multiprocesses.append(pro5)
        pro6 = spec06_benchmarks.dealII
        multiprocesses.append(pro6)
        pro7 = spec06_benchmarks.wrf
        multiprocesses.append(pro7)
        pro8 = spec06_benchmarks.zeusmp
        multiprocesses.append(pro8)
    elif options.benchmark == 'benchset11':
        print '--> benchset11:'
        pro1 = spec06_benchmarks.povray
        multiprocesses.append(pro1)
        pro2 = spec06_benchmarks.tonto
        multiprocesses.append(pro2)
        pro3 = spec06_benchmarks.cactusADM
        multiprocesses.append(pro3)
        pro4 = spec06_benchmarks.h264ref
        multiprocesses.append(pro4)
        pro5 = spec06_benchmarks.gcc
        multiprocesses.append(pro5)
        pro6 = spec06_benchmarks.bzip2
        multiprocesses.append(pro6)
        pro7 = spec06_benchmarks.bzip2
        multiprocesses.append(pro7)
        pro8 = spec06_benchmarks.xalancbmk
        multiprocesses.append(pro8)
    elif options.benchmark == 'benchset12':
        print '--> benchset12:'
        pro1 = spec06_benchmarks.libquantum
        multiprocesses.append(pro1)
        pro2 = spec06_benchmarks.GemsFDTD
        multiprocesses.append(pro2)
        pro3 = spec06_benchmarks.calculix
        multiprocesses.append(pro3)
        pro4 = spec06_benchmarks.gamess
        multiprocesses.append(pro4)
        pro5 = spec06_benchmarks.soplex
        multiprocesses.append(pro5)
        pro6 = spec06_benchmarks.hmmer
        multiprocesses.append(pro6)
        pro7 = spec06_benchmarks.gromacs
        multiprocesses.append(pro7)
        pro8 = spec06_benchmarks.astar
        multiprocesses.append(pro8)
    elif options.benchmark == 'benchset13':
        print '--> benchset13:'
        pro1 = spec06_benchmarks.libquantum
        multiprocesses.append(pro1)
        pro2 = spec06_benchmarks.leslie3d
        multiprocesses.append(pro2)
        pro3 = spec06_benchmarks.povray
        multiprocesses.append(pro3)
        pro4 = spec06_benchmarks.cactusADM
        multiprocesses.append(pro4)
        pro5 = spec06_benchmarks.soplex
        multiprocesses.append(pro5)
        pro6 = spec06_benchmarks.xalancbmk
        multiprocesses.append(pro6)
        pro7 = spec06_benchmarks.sjeng
        multiprocesses.append(pro7)
        pro8 = spec06_benchmarks.omnetpp
        multiprocesses.append(pro8)
    elif options.benchmark == 'benchset14':
        print '--> benchset14:'
        pro1 = spec06_benchmarks.namd
        multiprocesses.append(pro1)
        pro2 = spec06_benchmarks.leslie3d
        multiprocesses.append(pro2)
        pro3 = spec06_benchmarks.namd
        multiprocesses.append(pro3)
        pro4 = spec06_benchmarks.cactusADM
        multiprocesses.append(pro4)
        pro5 = spec06_benchmarks.lbm
        multiprocesses.append(pro5)
        pro6 = spec06_benchmarks.perlbench
        multiprocesses.append(pro6)
        pro7 = spec06_benchmarks.gromacs
        multiprocesses.append(pro7)
        pro8 = spec06_benchmarks.cactusADM
        multiprocesses.append(pro8)
    elif options.benchmark == 'benchset15':
        print '--> benchset15:'
        pro1 = spec06_benchmarks.bzip2
        multiprocesses.append(pro1)
        pro2 = spec06_benchmarks.perlbench
        multiprocesses.append(pro2)
        pro3 = spec06_benchmarks.bwaves
        multiprocesses.append(pro3)
        pro4 = spec06_benchmarks.gcc
        multiprocesses.append(pro4)
        pro5 = spec06_benchmarks.tonto
        multiprocesses.append(pro5)
        pro6 = spec06_benchmarks.sjeng
        multiprocesses.append(pro6)
        pro7 = spec06_benchmarks.tonto
        multiprocesses.append(pro7)
        pro8 = spec06_benchmarks.dealII
        multiprocesses.append(pro8)
    elif options.benchmark == 'benchset16':
        print '--> benchset16:'
        pro1 = spec06_benchmarks.cactusADM
        multiprocesses.append(pro1)
        pro2 = spec06_benchmarks.perlbench
        multiprocesses.append(pro2)
        pro3 = spec06_benchmarks.hmmer
        multiprocesses.append(pro3)
        pro4 = spec06_benchmarks.soplex
        multiprocesses.append(pro4)
        pro5 = spec06_benchmarks.wrf
        multiprocesses.append(pro5)
        pro6 = spec06_benchmarks.hmmer
        multiprocesses.append(pro6)
        pro7 = spec06_benchmarks.gcc
        multiprocesses.append(pro7)
        pro8 = spec06_benchmarks.cactusADM
        multiprocesses.append(pro8)
    elif options.benchmark == 'benchset17':
        print '--> benchset17:'
        pro1 = spec06_benchmarks.mcf
        multiprocesses.append(pro1)
        pro2 = spec06_benchmarks.calculix
        multiprocesses.append(pro2)
        pro3 = spec06_benchmarks.namd
        multiprocesses.append(pro3)
        pro4 = spec06_benchmarks.gamess
        multiprocesses.append(pro4)
        pro5 = spec06_benchmarks.mcf
        multiprocesses.append(pro5)
        pro6 = spec06_benchmarks.sphinx3
        multiprocesses.append(pro6)
        pro7 = spec06_benchmarks.gcc
        multiprocesses.append(pro7)
        pro8 = spec06_benchmarks.povray
        multiprocesses.append(pro8)
    elif options.benchmark == 'benchset18':
        print '--> benchset18:'
        pro1 = spec06_benchmarks.gromacs
        multiprocesses.append(pro1)
        pro2 = spec06_benchmarks.bzip2
        multiprocesses.append(pro2)
        pro3 = spec06_benchmarks.calculix
        multiprocesses.append(pro3)
        pro4 = spec06_benchmarks.lbm
        multiprocesses.append(pro4)
        pro5 = spec06_benchmarks.bzip2
        multiprocesses.append(pro5)
        pro6 = spec06_benchmarks.omnetpp
        multiprocesses.append(pro6)
        pro7 = spec06_benchmarks.gcc
        multiprocesses.append(pro7)
        pro8 = spec06_benchmarks.bwaves
        multiprocesses.append(pro8)
    elif options.benchmark == 'benchset19':
        print '--> benchset19:'
        pro1 = spec06_benchmarks.omnetpp
        multiprocesses.append(pro1)
        pro2 = spec06_benchmarks.sphinx3
        multiprocesses.append(pro2)
        pro3 = spec06_benchmarks.wrf
        multiprocesses.append(pro3)
        pro4 = spec06_benchmarks.cactusADM
        multiprocesses.append(pro4)
        pro5 = spec06_benchmarks.gromacs
        multiprocesses.append(pro5)
        pro6 = spec06_benchmarks.namd
        multiprocesses.append(pro6)
        pro7 = spec06_benchmarks.tonto
        multiprocesses.append(pro7)
        pro8 = spec06_benchmarks.gromacs
        multiprocesses.append(pro8)
    else:
        print "No recognized SPEC2006 benchmark selected! Exiting."
        sys.exit(1)
else:
    print >> sys.stderr, "Need --benchmark switch to specify SPEC CPU2006 workload. Exiting!\n"
    sys.exit(1)


#if options.bench:
#    apps = options.bench.split("-")
#    if len(apps) != options.num_cpus:
#        print "number of benchmarks not equal to set num_cpus!"
#        sys.exit(1)
#
 #   for app in apps:
#        try:
#            if buildEnv['TARGET_ISA'] == 'alpha':
#                exec("workload = %s('alpha', 'tru64', '%s')" % (
#                        app, options.spec_input))
#            elif buildEnv['TARGET_ISA'] == 'arm':
#                exec("workload = %s('arm_%s', 'linux', '%s')" % (
#                        app, options.arm_iset, options.spec_input))
#            else:
#                exec("workload = %s(buildEnv['TARGET_ISA', 'linux', '%s')" % (
#                        app, options.spec_input))
#            multiprocesses.append(workload.makeLiveProcess())
#        except:
#            print >>sys.stderr, "Unable to find workload for %s: %s" % (
#                    buildEnv['TARGET_ISA'], app)
#            sys.exit(1)
#elif options.cmd:
#    multiprocesses, numThreads = get_processes(options)
#else:
#    print >> sys.stderr, "No workload specified. Exiting!\n"
#    sys.exit(1)


(CPUClass, test_mem_mode, FutureClass) = Simulation.setCPUClass(options)
CPUClass.numThreads = numThreads

# Check -- do not allow SMT with multiple CPUs
if options.smt and options.num_cpus > 1:
    fatal("You cannot use SMT with multiple CPUs!")

np = options.num_cpus
system = System(cpu = [CPUClass(cpu_id=i) for i in xrange(np)],
                mem_mode = test_mem_mode,
                mem_ranges = [AddrRange(options.mem_size)],
                cache_line_size = options.cacheline_size)

if numThreads > 1:
    system.multi_thread = True
# Create a top-level voltage domain
system.voltage_domain = VoltageDomain(voltage = options.sys_voltage)

# Create a source clock for the system and set the clock period
system.clk_domain = SrcClockDomain(clock =  options.sys_clock,
                                   voltage_domain = system.voltage_domain)

# Create a CPU voltage domain
system.cpu_voltage_domain = VoltageDomain()

# Create a separate clock domain for the CPUs
system.cpu_clk_domain = SrcClockDomain(clock = options.cpu_clock,
                                       voltage_domain =
                                       system.cpu_voltage_domain)

# All cpus belong to a common cpu_clk_domain, therefore running at a common
# frequency.
for cpu in system.cpu:
    cpu.clk_domain = system.cpu_clk_domain

if is_kvm_cpu(CPUClass) or is_kvm_cpu(FutureClass):
    if buildEnv['TARGET_ISA'] == 'x86':
        system.vm = KvmVM()
        for process in multiprocesses:
            process.useArchPT = True
            process.kvmInSE = True
    else:
        fatal("KvmCPU can only be used in SE mode with x86")

# Sanity check
if options.fastmem:
    if CPUClass != AtomicSimpleCPU:
        fatal("Fastmem can only be used with atomic CPU!")
    if (options.caches or options.l2cache):
        fatal("You cannot use fastmem in combination with caches!")

if options.simpoint_profile:
    if not options.fastmem:
        # Atomic CPU checked with fastmem option already
        fatal("SimPoint generation should be done with atomic cpu and fastmem")
    if np > 1:
        fatal("SimPoint generation not supported with more than one CPUs")

#for i in xrange(np):
#    if options.smt:
#        system.cpu[i].workload = multiprocesses
#        system.cpu[i].workload = multiprocesses[0]
#    else:
#
#    if options.fastmem:
#        system.cpu[i].fastmem = True
#
#    if options.simpoint_profile:
#        system.cpu[i].addSimPointProbe(options.simpoint_interval)
#
#    if options.checker:
#        system.cpu[i].addCheckerCpu()
#
#    system.cpu[i].createThreads()

# changed for multiprocesses, wei.
for i in xrange(np): 
#    system.cpu[i].workload = multiprocesses[i]
#    print multiprocesses[i].cmd
    if options.smt:
        system.cpu[i].workload = multiprocesses
        system.cpu[i].workload = multiprocesses[0]
    else:
    if options.fastmem:
        system.cpu[i].fastmem = True

    if options.simpoint_profile:
        system.cpu[i].simpoint_profile = True
        system.cpu[i].simpoint_interval = options.simpoint_interval

if options.ruby:
    if not (options.cpu_type == "detailed" or options.cpu_type == "timing"):
        print >> sys.stderr, "Ruby requires TimingSimpleCPU or O3CPU!!"
        sys.exit(1)

    Ruby.create_system(options, False, system)
    assert(options.num_cpus == len(system.ruby._cpu_ports))

    system.ruby.clk_domain = SrcClockDomain(clock = options.ruby_clock,
                                        voltage_domain = system.voltage_domain)
    for i in xrange(np):
        ruby_port = system.ruby._cpu_ports[i]

        # Create the interrupt controller and connect its ports to Ruby
        # Note that the interrupt controller is always present but only
        # in x86 does it have message ports that need to be connected
        system.cpu[i].createInterruptController()

        # Connect the cpu's cache ports to Ruby
        system.cpu[i].icache_port = ruby_port.slave
        system.cpu[i].dcache_port = ruby_port.slave
        if buildEnv['TARGET_ISA'] == 'x86':
            system.cpu[i].interrupts.pio = ruby_port.master
            system.cpu[i].interrupts.int_master = ruby_port.slave
            system.cpu[i].interrupts.int_slave = ruby_port.master
            system.cpu[i].itb.walker.port = ruby_port.slave
            system.cpu[i].dtb.walker.port = ruby_port.slave
else:
    MemClass = Simulation.setMemClass(options)
    system.membus = SystemXBar()
    system.system_port = system.membus.slave
    L3Config.config_cache(options, system)
    MemConfig.config_mem(options, system)

root = Root(full_system = False, system = system)
Simulation.run(options, root, system, FutureClass)
