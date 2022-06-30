#include "SlavePort.hpp"
#include "StorageConfig.hpp"
#include "types.hpp"
#include "Controller.hpp"
#include "FixedLatencyStoragePacket.hpp"
#include <vector>

#include "packetFormat.hpp"         /* ++ */
#include "packetFormat.cpp"         /* ++ */
#include <string>                   /* ++ */


using namespace std;

class c_FixedLatencyStorage : public c_Controller, public c_CacheLogger  {
class ramulator::c_RamulatorPacket;

private:

    c_SlavePort* m_memRdReqPort;
    c_SlavePort* m_memWrReqPort;
    c_SlavePort* m_memWrDataPort;

    c_MasterPort* m_memRdRespPort;
    c_MasterPort* m_memPrbReqPort;

    t_Time m_clockCycle;
    t_Time m_fixedLatency;
    t_Data m_actualDataStore;
    t_Address m_size;
    unsigned m_offset;

    vector<c_FixedLatencyStoragePacket*> m_readQueue;
    vector<c_FixedLatencyStoragePacket*> m_writeQueue;
    vector<c_FixedLatencyStoragePacket*> m_probeQueue;

    t_Credit m_coreRdCredit;
    t_Credit m_coreWrCredit;

    SC_HAS_PROCESS(c_FixedLatencyStorage);
public:
    const std::string configuration = "../../../../ramulator/configs/DDR3-config.cfg";  //Contains the Directory Of the Required CFG File
    c_RamulatorPacket m_DRAMSim* = new c_RamulatorPacket(configuration ,64); //64 bytes is the cacheline as an eg., Allocated on Heap Memory

    c_FixedLatencyStorage(c_TLMModuleName p_name, c_StorageConfig *p_storageConfig);
    ~c_FixedLatencyStorage();

    c_SlavePort* f_getReadReqPort();
    c_SlavePort* f_getWriteReqPort();
    c_MasterPort* f_getReadRespPort();
    c_SlavePort* f_getWriteDataPort();
    c_MasterPort* f_getProbeReqPort();
    
    void f_clockCycle();
    void f_fetch();
	void f_decode();
    void f_execute();
	void f_issue();     
    void f_retire();

    void f_fetchTransaction(t_PortNo p_portNo, c_Transaction *p_transaction);
	c_Transaction* f_retireTransaction(t_PortNo p_portNo,t_RequestType p_reqType, t_ResponseType p_respType);
    t_FixedLatencyRequestType convertToFixedLatencyRequestType(t_RequestType p_reqType);

    void f_checkAddress(t_Address p_address);
    std::string f_toString(std::string p_message);
    bool f_getModuleDone();
    bool f_hasCreditRecvQueue(t_PortNo p_portNo, t_RequestType p_nextReqType, t_ResponseType p_respType);
    void f_watchDogTimer();
};