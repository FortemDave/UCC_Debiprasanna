
/* Include Types.hpp from UCC and request.h from Ramulator*/

#include <limits.h>
#include "../ramulator/src/Request.h"  /* #include "Request.h" */ 
#include "packetFormat.hpp"


using namespace ramulator;

    /*Used to create an instance of Memory saved as 'm_Ramulator_DRAM'*/
static map<string, function<MemoryBase *(const Config&, int) >> f_memoryGenerator = {
    {"DDR3", &MemoryFactory<DDR3>::create}, {"DDR4", &MemoryFactory<DDR4>::create},
    {"LPDDR3", &MemoryFactory<LPDDR3>::create}, {"LPDDR4", &MemoryFactory<LPDDR4>::create},
    {"GDDR5", &MemoryFactory<GDDR5>::create},
    {"WideIO", &MemoryFactory<WideIO>::create}, {"WideIO2", &MemoryFactory<WideIO2>::create},
    {"HBM", &MemoryFactory<HBM>::create},
    {"SALP-1", &MemoryFactory<SALP>::create}, {"SALP-2", &MemoryFactory<SALP>::create}, {"SALP-MASA", &MemoryFactory<SALP>::create},
};

c_RamulatorPacket::c_RamulatorPacket(const Config& configs,int cacheline){
    const string& std_name = configs["standard"];
    assert(f_memoryGenerator.find(std_name) != f_memoryGenerator.end() && "unrecognized standard name");
    m_Ramulator_DRAM = f_memoryGenerator[std_name](configs,cacheline);

    //Request Object is created only Once and then Reused.
    m_RequestPacket = new ramulator::Request(m_address,m_RequestType,f_callbackRamulator);
}

c_RamulatorPacket::~c_RamulatorPacket(){
    delete m_RequestPacket;
    delete m_Ramulator_DRAM;
}

//Changes to be made
//2, callback...impliment it, but how?
void c_RamulatorPacket::f_RamulatorRequest(t_Address m_addr, t_FixedLatencyRequestType p_type/*,int core_id = 0*/){
    /* Convert UCC Request ENUM type to Ramulator ENUM type */

    switch(p_type){
        case t_FixedLatencyRequestType::E_FIXED_LATENCY_DATA_READ:
            m_RequestType = Request::Type::READ;
            break;
        case t_FixedLatencyRequestType::E_FIXED_LATENCY_DATA_WRITE:
            m_RequestType = Request::Type::WRITE;
            break;
        case t_FixedLatencyRequestType::E_NUM_FIXED_LATENCY_REQTYPE:
            m_RequestType = Request::Type::MAX;                      //Other Request Types in Between are handled by Ramulator
            break;
        default:
            //Unexpected Request Type Encountered
            assert(false);
    }

    //Ramulator Works with Signed Long Int and Not Unsigned Long Long Int [Ramulator Limitation]
    assert(m_addr <= LONG_MAX && m_addr >= 0);      
    long m_address = static_cast<long>(m_addr);

    /* Ramulator also provides specification for mentioning which core is being used.
        In case that has to be specified, add an index integer (i.e. from 0), specifying
        the core index and update the packetFormat.hpp and FixedLatencyStorage.cpp files in UCC */
    
    m_RequestPacket->addr = m_address;
    m_RequestPacket->type = m_RequestType;
    // m_RequestPacket->coreid = 0; // This is the default Value on all Calls

    // return Request(m_address,m_RequestType,/*core_id*/);
}

bool c_RamulatorPacket::f_sendRamulatorPacket(){
    return m_Ramulator_DRAM->send(*m_RequestPacket);
}

void c_RamulatorPacket::f_tickRamulator(){
    m_Ramulator_DRAM->tick();
}

void c_RamulatorPacket::f_FinishRamulatorSimulation(){
    m_Ramulator_DRAM->finish();
    //Stats::statlist.printall(); //Used to generate statistics list
}

// Callback Function of The Request Class to Execute on every served Request
// Note: Callback on WRITE has been disabled -> "Hassan -> Incorrect Callback Behaviour when using --mode = DRAM"
// auto read_complete = [&](Request& r){latencies[r.depart - r.arrive]++;};

auto f_callbackRamulator = [](ramulator::Request& m_servedRequest)
{
    if(m_servedRequest.type == ramulator::Request::Type::READ){
        std::cout << "\033[1;32mREAD \033[0;m" << " Request: " << std::hex << m_servedRequest.addr << " served.\n";
    }
    else if(m_servedRequest.type == ramulator::Request::Type::WRITE){
        std::cout << "\033[1;36mWRITE\033[0;m" << " Request: " << std::hex << m_servedRequest.addr << " served.\n";
    }
    else{
        std::cout << static_cast<int>(m_servedRequest.type) << "Type at Addr: " << std::hex << m_servedRequest.addr << " Request served.\n";
    }
};