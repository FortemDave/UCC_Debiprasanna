#include "FixedLatencyStorage.hpp"
#include "FixedLatencyStoragePacket.hpp"
#include "Transaction.hpp"
#include "types.hpp"
#include "TLMTypes.hpp"
#include "MasterPort.hpp"
#include "RequestPool.hpp"
#include <queue>

#include "packetFormat.hpp"
#include "../../../../ramulator/src/Config.h"             //Ramulator


c_FixedLatencyStorage::c_FixedLatencyStorage(c_TLMModuleName p_name, c_StorageConfig *p_storageConfig): c_Controller(p_name) {
    //Ramulator DRAM Object Constructor placed in .hpp file
 
    //Everything initialized from StorageConfig.hpp file
    m_size = p_storageConfig->s_size;   //Address type, can be changed if needed
    m_offset = p_storageConfig->s_offset;   
    m_fixedLatency = p_storageConfig->s_fixedLatency;
    // *** CREDIT = Queue Size ***
    m_coreRdCredit = p_storageConfig->s_readReqQSize;
    m_coreWrCredit = p_storageConfig->s_writeReqQSize;
    // Ports[UCC only]

    // Initializing all with Port No. = 0
    m_memRdReqPort = new c_SlavePort(this,"READREQMEMPortMainMemory",0);
	m_memWrReqPort = new c_SlavePort(this,"WRITEREQMEMPortMainMemory",0);
    m_memRdRespPort = new c_MasterPort(this,"READRESPMEMPortMainMemory",0,t_RequestType::E_NUM_RECVQ_TYPE,t_ResponseType::E_READ_RESP);
    m_memPrbReqPort = new c_MasterPort(this,"PRBREQMEMPortMainMemory",0,t_RequestType::E_PROBE_REQ,t_ResponseType::E_NUM_SENDQ_TYPE);
    m_memWrDataPort = new c_SlavePort(this,"WriteDataMEMPortMainMemory",0);
    
    m_actualDataStore = new unsigned char[m_size];      // No functionality encountered
    m_clockCycle = 0;
}

c_FixedLatencyStorage::~c_FixedLatencyStorage()  {
    m_DRAMSim.f_FinishRamulatorSimulation();    //Used to Print Statistics/
    delete m_DRAMSim;   //Destructor called implicitly by the complier

    delete m_memRdReqPort;
    delete m_memWrReqPort;
    delete m_memRdRespPort;
    delete m_memPrbReqPort;
    delete m_memWrDataPort;
    delete[] m_actualDataStore;
}

void c_FixedLatencyStorage::f_clockCycle() {
    m_DRAMSim.f_tickRamulator();
    f_retire();
    f_issue();
    f_execute();
    f_decode();
    f_fetch();
    m_clockCycle++;
}

void c_FixedLatencyStorage::f_fetchTransaction(t_PortNo p_portNo, c_Transaction *p_transaction)    {

    // (Assuming only limited Packets can be created[Similar to a thread pool])
    c_FixedLatencyStoragePacket *l_packet = c_RequestPool<c_FixedLatencyStoragePacket>::f_allocate();

    //Convert Core read and write request to fixed latency data read and write
    //and a CPU probe (to check the response) is converted to Core Write

    t_FixedLatencyRequestType l_reqType = convertToFixedLatencyRequestType(p_transaction->f_getRequestType());

    //m_clockcycle passes in the current time
    l_packet->f_initPacket(p_transaction,l_reqType,m_clockCycle);

    t_Address l_physAddr = l_packet->f_getStorageAddress();

    //Ensures it's not out of the defined memory range
    // Temporarily implimented as a simple range checker
    f_checkAddress(l_physAddr);    

    //Log the Request type and PUSH_BACK into the appropriate queue
    //Will be processed in the next clock cycle
    
    if(p_transaction->f_getRequestType() == t_RequestType::E_CORE_READ)    {
        p_transaction->f_logDebug("Received Read in Memory\n");
        m_readQueue.push_back(l_packet);
    }
    else if(p_transaction->f_getRequestType() == t_RequestType::E_CORE_WRITE)    {
        p_transaction->f_logDebug("Received Write Request in Memory\n");
        m_probeQueue.push_back(l_packet);
    }
    else if(p_transaction->f_getRequestType() == t_RequestType::E_PROBE_RESP)    {
        p_transaction->f_logDebug("Received Write data in Memory\n");
        m_writeQueue.push_back(l_packet);
    }
    else    {
        // Logs an error and contains a guaranteed assert
        p_transaction->f_logError("Unknown COMMAND while fetching transaction\n");
    }
}

t_FixedLatencyRequestType c_FixedLatencyStorage::convertToFixedLatencyRequestType(t_RequestType p_reqType)  {
    if(p_reqType == t_RequestType::E_CORE_READ) {
        return t_FixedLatencyRequestType::E_FIXED_LATENCY_DATA_READ;
    }
    else if(p_reqType == t_RequestType::E_CORE_WRITE) {
        return t_FixedLatencyRequestType::E_FIXED_LATENCY_DATA_WRITE;        
    }
    // Converts Probe type to Data write as well.
    else if(p_reqType == t_RequestType::E_PROBE_RESP)    {
        return t_FixedLatencyRequestType::E_FIXED_LATENCY_DATA_WRITE;        
    }   
    else    {
        f_logError("Unknown COMMAND while converting packets");
        return t_FixedLatencyRequestType::E_NUM_FIXED_LATENCY_REQTYPE;
    }
}

c_Transaction* c_FixedLatencyStorage::f_retireTransaction(t_PortNo p_portNo, t_RequestType p_reqType, t_ResponseType p_responseType)    {
    //Port Number not used so far

    //MasterPort activities only for retire
    if(p_reqType == t_RequestType::E_PROBE_REQ)    {
        // If the Queue is empty,do nothing (retire not applicable)
        if(!m_probeQueue.empty())   {
        
            //Pick the packet on the HEAD
            c_FixedLatencyStoragePacket *l_packet = m_probeQueue.front();

            //Remove the top of the Queue Before processing
            m_probeQueue.erase(m_probeQueue.begin());

            // Change the Request types, set response to sent.
            //Log the changes, Free the space (DeAllocate not implimented fully yet)
            c_Transaction *l_transaction = l_packet->f_getTransaction();
            l_transaction->f_setRequestType(t_RequestType::E_PROBE_REQ);
            l_transaction->f_setResponseType(t_ResponseType::E_PROBE_RESP_SEND);
            l_transaction->f_logDebug("Sending a probe to get the data and deleting packet");
            c_RequestPool<c_FixedLatencyStoragePacket>::f_deAllocate(l_packet);
            return l_transaction;
        }
    }

    else if(p_responseType == t_ResponseType::E_READ_RESP)    {
        if(!m_readQueue.empty())    {
            c_FixedLatencyStoragePacket *l_packet = (c_FixedLatencyStoragePacket *) m_readQueue.front();
            t_Time l_reqTime = l_packet->f_getReqTime(); 

            // Condition commented out for debug/other reasons:
            // execute if the current cycle(in terms of time) is higher than the request time of the transaction
            // and the fixed latency to respond

            //if(m_clockCycle > (l_reqTime + m_fixedLatency))   {
                c_Transaction* l_transaction = l_packet->f_getTransaction();
                l_transaction->f_setRequestType(t_RequestType::E_FILL);
                l_transaction->f_setResponseType(t_ResponseType::E_READ_RESP);

                l_transaction->f_setResponseCacheState(t_CacheState::E_EXCLUSIVE);
                m_readQueue.erase(m_readQueue.begin());
                l_transaction->f_logDebug("Sending read response and deleting packet");
                c_RequestPool<c_FixedLatencyStoragePacket>::f_deAllocate(l_packet);
                return l_transaction;
            //}            
        }
    }

    else    {
        f_logError("Unknown COMMAND whlle finding transaction to retire");
    }
    return NULL;
}

c_SlavePort* c_FixedLatencyStorage::f_getReadReqPort()  {
    return m_memRdReqPort;
}

c_SlavePort* c_FixedLatencyStorage::f_getWriteReqPort() {
    return m_memWrReqPort;
}

c_SlavePort* c_FixedLatencyStorage::f_getWriteDataPort() {
    return m_memWrDataPort;
}

c_MasterPort* c_FixedLatencyStorage::f_getProbeReqPort() {
    return m_memPrbReqPort;
}

c_MasterPort* c_FixedLatencyStorage::f_getReadRespPort()  {
    return m_memRdRespPort;
}

void c_FixedLatencyStorage::f_checkAddress(t_Address p_address)   {
    // shoudn't it be
    // P_address < m_size or P_address >= m_offset + m_size ?

   /* if (p_address < m_offset || p_address >= m_offset + m_size)   {
        f_debugError("Address out of range. Did you set an appropriate size and offset?");
    }*/
}

void c_FixedLatencyStorage::f_fetch()   {
    m_memRdReqPort->f_clockCycle();
	m_memWrReqPort->f_clockCycle();
    m_memWrDataPort->f_clockCycle();
}

void c_FixedLatencyStorage::f_decode()   {
}

void c_FixedLatencyStorage::f_retire()   {
    m_memRdRespPort->f_clockCycle();
    m_memPrbReqPort->f_clockCycle();
    
}

void c_FixedLatencyStorage::f_execute()   {
    //Can handle N=2 executions in 1 clock cycle
    for(t_Size l_index = 0; l_index < E_NUM_FIXED_LATENCY_REQTYPE; l_index++)   {
        //Only for Data read and non-empty queue
        if(!m_readQueue.empty() && m_readQueue.front()->f_getReqType() == t_FixedLatencyRequestType::E_FIXED_LATENCY_DATA_READ)    {
            c_FixedLatencyStoragePacket *l_packet = m_readQueue.front();
            c_Transaction* l_transaction = l_packet->f_getTransaction();
            
            t_Address l_physAddr = l_packet->f_getStorageAddress();
            //std::memcpy(l_transaction->f_getData(), m_actualDataStore + l_physAddr - m_offset, l_transaction->f_getDataSize());

            /*==================================================Code======================================================*/
            ramulator::Request m_req = c_RamulatorPacket::f_RamulatorRequest(l_physAddr,t_FixedLatencyRequestType::E_FIXED_LATENCY_DATA_READ);
            assert(m_DRAMSim.f_sendRamulatorPacket());
            /*==================================================Code======================================================*/
        }
        //Only for Data Write
        else if(!m_writeQueue.empty() && m_writeQueue.front()->f_getReqType() == t_FixedLatencyRequestType::E_FIXED_LATENCY_DATA_WRITE)    {
            //Copy values and Deallocate memory everywhere
            c_FixedLatencyStoragePacket *l_packet = m_writeQueue.front();
            c_Transaction* l_transaction = l_packet->f_getTransaction();
            t_Address l_physAddr = l_packet->f_getStorageAddress();
            m_writeQueue.erase(m_writeQueue.begin());
            l_transaction->f_logDebug("Done writing in memory deleting packet");
            c_RequestPool<c_Transaction>::f_deAllocate(l_transaction);
            c_RequestPool<c_FixedLatencyStoragePacket>::f_deAllocate(l_packet);

            /*==================================================Code======================================================*/
            ramulator::Request m_req = c_RamulatorPacket::f_RamulatorRequest(l_physAddr,t_FixedLatencyRequestType::E_FIXED_LATENCY_DATA_READ);
            assert(m_DRAMSim.f_sendRamulatorPacket());
            /*==================================================Code======================================================*/
            //std::memcpy(m_actualDataStore + l_physAddr - m_offset, l_transaction->f_getData(), l_transaction->f_getDataSize());
        }
    }
}

void c_FixedLatencyStorage::f_issue()   {
}

std::string c_FixedLatencyStorage::f_toString(std::string p_message)  {
    return p_message;
}

bool c_FixedLatencyStorage::f_getModuleDone(){
    return m_readQueue.empty() && m_probeQueue.empty() && m_writeQueue.empty();
}


// Check if more values can be pushed into the queue?
bool c_FixedLatencyStorage::f_hasCreditRecvQueue(t_PortNo p_portNo, t_RequestType p_reqType, t_ResponseType p_respType) {
    if(p_reqType == t_RequestType::E_CORE_READ) {
        return (m_coreRdCredit - m_readQueue.size() > 0);
    }
    else if(p_reqType == t_RequestType::E_CORE_WRITE) {
        return (m_coreWrCredit - m_probeQueue.size() > 0);
    }
    else if(p_reqType == t_RequestType::E_PROBE_RESP) {
        return (m_coreWrCredit - m_writeQueue.size() > 0);
    }
    else    {
        f_logError("Unknown COMMAND while converting packets");
        return false;
    }
}

void c_FixedLatencyStorage::f_watchDogTimer()  {
}