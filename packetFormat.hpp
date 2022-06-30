/*DIRECTORIES on the Programmer's PC*/
#include <iostream>           // Temporarily included for Return Values for the Callback Function

#include "../ucc/src/cache/types/types.hpp"            //UCC  
#include "../ramulator/src/Config.h"             //Ramulator
#include "../ramulator/src/MemoryFactory.cpp"    //Ramulator   //base statistics dependency issue -_-
#include "../ramulator/src/Request.h"            //Ramulator
#include "../ramulator/src/MemoryFactory.h"      //Ramulator
#include "../ramulator/src/Memory.h"             //Ramulator

// Memory Standards
#include "../ramulator/src/DDR3.h"
#include "../ramulator/src/DDR4.h"
#include "../ramulator/src/LPDDR3.h"
#include "../ramulator/src/LPDDR4.h"
#include "../ramulator/src/GDDR5.h"
#include "../ramulator/src/WideIO.h"
#include "../ramulator/src/WideIO2.h"
#include "../ramulator/src/HBM.h"
#include "../ramulator/src/SALP.h" 


#ifndef PACKETFORMAT_H
#define PACKETFORMAT_H

// class ramulator::Request;
// class ramulator::MemoryBase;

class c_RamulatorPacket 
{

private:
    ramulator::MemoryBase *m_Ramulator_DRAM;
    ramulator::Request::Type m_RequestType{Request::Type::READ};

    //Create a Single Instance of Request Object for Ramulator Requests[Passing in Default Args,Lambda Callback Function]
    // ramulator::Request* m_RequestPacket = new ramulator::Request(0,m_RequestType,f_callbackRamulator);
    long m_address = 0;
    ramulator::Request* m_RequestPacket;

public:
    
    
    //Generate Memory[DRAM] for Simulation[Done Similar to Gem5Wrapper]
    c_RamulatorPacket(const Config& configs,int cacheline);  // Consult on what Cacheline is For

    //Delete Memory[DRAM] after Simulation
    ~c_RamulatorPacket();

    //Make Packets [per Ramulator Standards]
    void f_RamulatorRequest(t_Address m_addr, t_FixedLatencyRequestType p_type);

    //Send a packet and check if it has been sent.
    bool f_sendRamulatorPacket();

    //Tick the DRAM [i.e. Ramulator Simulation Call]
    void f_tickRamulator();
    
    

    //Finish Simulation command needed? Included in Gem5Wrapper, might be Deleted [Used to print statistics]
    void f_FinishRamulatorSimulation();

};

#endif