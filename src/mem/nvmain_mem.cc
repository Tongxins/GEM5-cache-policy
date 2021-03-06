/*
 * Copyright (c) 2012-2013 Pennsylvania State University
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  This file is part of NVMain- A cycle accurate timing, bit-accurate
 *  energy simulator for non-volatile memory. Originally developed by
 *  Matt Poremba at the Pennsylvania State University.
 *
 *  Website: http://www.cse.psu.edu/~poremba/nvmain/
 *  Email: mrp5060@psu.edu
 *
 *  ---------------------------------------------------------------------
 *
 *  If you use this software for publishable research, please include
 *  the original NVMain paper in the citation list and mention the use
 *  of NVMain.
 *
 */

#include "SimInterface/Gem5Interface/Gem5Interface.h"
#include "Utils/HookFactory.h"
#include "base/random.hh"
#include "debug/NVMain.hh"
#include "mem/nvmain_mem.hh"

using namespace NVM;


NVMainMemory::NVMainMemory(const Params *p)
    : AbstractMemory(p), port(name() + ".port", *this),
      lat(p->atomic_latency), lat_var(p->atomic_variance),
      nvmain_atomic(p->atomic_mode), NVMainWarmUp(p->NVMainWarmUp)
{
    char *cfgparams;
    char *cfgvalues;
    char *cparam, *cvalue;

    char *saveptr1, *saveptr2;

    m_nvmainPtr = NULL;
    m_nacked_requests = false;

    m_nvmainConfigPath = p->config;

    m_nvmainConfig = new Config( );

    m_nvmainConfig->Read( m_nvmainConfigPath );
    std::cout << "NVMainControl: Reading NVMain config file: " << m_nvmainConfigPath << "." << std::endl;

    clock = clockPeriod( );

    m_avgAtomicLatency = 100.0f;
    m_numAtomicAccesses = 0;

    retryRead = false;
    retryWrite = false;

    /*
     * Modified by Tao @ 01/22/2013
     * multiple parameters can be manually specified
     * please separate the parameters by comma ","
     * For example,
     *    configparams = tRCD,tCAS,tRP
     *    configvalues = 8,8,8
     */
    cfgparams = (char *)p->configparams.c_str();
    cfgvalues = (char *)p->configvalues.c_str();

    for( cparam = strtok_r( cfgparams, ",", &saveptr1 ), cvalue = strtok_r( cfgvalues, ",", &saveptr2 )
           ; (cparam && cvalue) ; cparam = strtok_r( NULL, ",", &saveptr1 ), cvalue = strtok_r( NULL, ",", &saveptr2) )
    {
        std::cout << "NVMain: Overriding parameter `" << cparam << "' with `" << cvalue << "'" << std::endl;
        m_nvmainConfig->SetValue( cparam, cvalue );
    }

   eventManager = new NVMainMemoryEventManager(&mainEventQueue, *this);

   BusWidth = m_nvmainConfig->GetValue( "BusWidth" );
   tBURST = m_nvmainConfig->GetValue( "tBURST" );
   RATE = m_nvmainConfig->GetValue( "RATE" );

   lastWakeup = curTick();
}


NVMainMemory::~NVMainMemory()
{
    std::cout << "NVMain dtor called" << std::endl;
}


void
NVMainMemory::init()
{
    m_nvmainPtr = new NVMain( );
    m_nvmainSimInterface = new Gem5Interface( );
    m_nvmainEventQueue = new NVM::EventQueue( );

    m_nvmainConfig->SetSimInterface( m_nvmainSimInterface );

    if (port.isConnected()) {
        port.sendRangeChange();
    }

    eventManager->scheduleWakeup( );

    statPrinter.nvmainPtr = m_nvmainPtr;

    registerExitCallback( &statPrinter );

    SetEventQueue( m_nvmainEventQueue );

    /*  Add any specified hooks */
    std::vector<std::string>& hookList = m_nvmainConfig->GetHooks( );

    for( size_t i = 0; i < hookList.size( ); i++ )
    {
        std::cout << "Creating hook " << hookList[i] << std::endl;

        NVMObject *hook = HookFactory::CreateHook( hookList[i] );

        if( hook != NULL )
        {
            AddHook( hook );
            hook->SetParent( this );
            hook->Init( m_nvmainConfig );
        }
        else
        {
            std::cout << "Warning: Could not create a hook named `"
                << hookList[i] << "'." << std::endl;
        }
    }

    /* Setup child and parent modules. */
    AddChild( m_nvmainPtr );
    m_nvmainPtr->SetParent( this );

    m_nvmainPtr->SetConfig( m_nvmainConfig );
}


BaseSlavePort &
NVMainMemory::getSlavePort(const std::string& if_name, PortID idx)
{
    if (if_name != "port") {
        return MemObject::getSlavePort(if_name, idx);
    } else {
        return port;
    }
}


NVMainMemory::MemoryPort::MemoryPort(const std::string& _name, NVMainMemory& _memory)
    : QueuedSlavePort(_name, &_memory, queueImpl),
      queueImpl(_memory, *this), memory(_memory)
{

}


AddrRangeList NVMainMemory::MemoryPort::getAddrRanges() const
{
    AddrRangeList ranges;
    ranges.push_back(memory.getAddrRange());
    return ranges;
}


Tick
NVMainMemory::MemoryPort::recvAtomic(PacketPtr pkt)
{
    if (pkt->memInhibitAsserted())
        return 0;

    /*
     * do the memory access to get the read data and change the response tag
     */
    memory.access(pkt);

    /*
     * calculate the latency. Now it is only random number
     */
    Tick latency = memory.lat;

    if (memory.lat_var != 0)
        latency += random_mt.random<Tick>(0, memory.lat_var);

    /*
     *  if NVMain also needs the packet to warm up the inline cache, create the request
     */
    if( memory.NVMainWarmUp )
    {
        NVMainRequest *request = new NVMainRequest( );
        unsigned int transfer_size;
        uint8_t *hostAddr;

        transfer_size =  memory.BusWidth / 8;
        transfer_size *= memory.tBURST * memory.RATE;

        /* extract the data in the packet */
        if( pkt->isRead() )
        {   // read
            hostAddr = new uint8_t[ pkt->getSize() ];
            memcpy( hostAddr, pkt->getPtr<uint8_t>(), pkt->getSize() );
        }
        else if( pkt->isWrite() )
        {   // write
            hostAddr = new uint8_t[ pkt->getSize() ];
            memcpy( hostAddr, pkt->getPtr<uint8_t>(), pkt->getSize() );
        }
        else
        {
            // if it is neither read nor write, just return
            // well, speed may suffer a little bit...
            return latency;
        }

        /* store the data into the request */
        for(int i = 0; i < transfer_size; i++ )
        {
            request->data.SetByte(i, *(hostAddr + (transfer_size - 1) - i));
        }
        delete [] hostAddr;

        /* initialize the request so that NVMain can correctly serve it */
        request->access = UNKNOWN_ACCESS;
        request->address.SetPhysicalAddress(pkt->req->getPaddr());
        request->status = MEM_REQUEST_INCOMPLETE;
        request->type = (pkt->isRead()) ? READ : WRITE;
        request->owner = (NVMObject *)&memory;
        if(pkt->req->hasPC()) request->programCounter = pkt->req->getPC();
        if(pkt->req->hasContextId()) request->threadId = pkt->req->contextId();

        /*
         * Issue the request to NVMain as an atomic request
         */
        memory.GetChild( )->IssueAtomic(request);

        delete request;
    }

    return latency;
}


void
NVMainMemory::MemoryPort::recvFunctional(PacketPtr pkt)
{
    pkt->pushLabel(memory.name());

    if (!queue.checkFunctional(pkt)) {
        memory.doFunctionalAccess(pkt);
    }

    pkt->popLabel();
}


bool
NVMainMemory::MemoryPort::recvTimingReq(PacketPtr pkt)
{
    /* added by Tao @ 01/24/2013, just copy the code from SimpleMemory */
    /// @todo temporary hack to deal with memory corruption issues until
    /// 4-phase transactions are complete
    for (int x = 0; x < memory.pendingDelete.size(); x++)
        delete memory.pendingDelete[x];
    memory.pendingDelete.clear();

    if (pkt->memInhibitAsserted()) {
        memory.pendingDelete.push_back(pkt);
        return true;
    }


    if (!pkt->isRead() && !pkt->isWrite()) {
        DPRINTF(NVMain, "NVMainMemory: Received a packet that is neither read nor write");

        bool needsResponse = pkt->needsResponse();

        memory.access(pkt);
        if (needsResponse) {
            assert(pkt->isResponse());

            pkt->busFirstWordDelay = pkt->busLastWordDelay = 0;
            queue.schedSendTiming(pkt, curTick() + 1);
        } else {
            memory.pendingDelete.push_back(pkt);
        }

        return true;
    }


    if (memory.retryRead || memory.retryWrite)
    {
        DPRINTF(NVMain, "nvmain_mem.cc: Received request while waiting for retry!\n");
        return false;
    }

    // Bus latency is modeled in NVMain.
    pkt->busFirstWordDelay = pkt->busLastWordDelay = 0;

    NVMainRequest *request = new NVMainRequest( );

    bool enqueued;
    unsigned int transfer_size;
    uint8_t *hostAddr;

    transfer_size =  memory.BusWidth / 8;
    transfer_size *= memory.tBURST * memory.RATE;

    if (pkt->isRead())
    {
        Request *dataReq = new Request(pkt->req->getPaddr(), transfer_size, 0, Request::funcMasterId);
        Packet *dataPkt = new Packet(dataReq, MemCmd::ReadReq);
        dataPkt->allocate();
        memory.doFunctionalAccess(dataPkt);

        hostAddr = new uint8_t[ dataPkt->getSize() ];
        memcpy( hostAddr, dataPkt->getPtr<uint8_t>(), dataPkt->getSize() );

        delete dataPkt;
        delete dataReq;
    }
    else
    {
        hostAddr = new uint8_t[ pkt->getSize() ];

        memcpy( hostAddr, pkt->getPtr<uint8_t>(), pkt->getSize() );
    }

    for(int i = 0; i < transfer_size; i++ )
    {
        request->data.SetByte(i, *(hostAddr + (transfer_size - 1) - i));
    }
    delete [] hostAddr;

    request->access = UNKNOWN_ACCESS;
    request->address.SetPhysicalAddress(pkt->req->getPaddr());
    request->status = MEM_REQUEST_INCOMPLETE;
    request->type = (pkt->isRead()) ? READ : WRITE;
    request->owner = (NVMObject *)&memory;

    if(pkt->req->hasPC()) request->programCounter = pkt->req->getPC();
    if(pkt->req->hasContextId()) request->threadId = pkt->req->contextId();

    /* Call hooks here manually, since there is no one else to do it. */
    std::vector<NVMObject *>& preHooks  = memory.GetHooks( NVMHOOK_PREISSUE );
    std::vector<NVMObject *>& postHooks = memory.GetHooks( NVMHOOK_POSTISSUE );
    std::vector<NVMObject *>::iterator it;

    /* Call pre-issue hooks */
    // TODO: Figure out what to do if it's not enqueued.
    for( it = preHooks.begin(); it != preHooks.end(); it++ )
    {
        (*it)->SetParent( &memory );
        (*it)->IssueCommand( request );
    }

    enqueued = memory.GetChild( )->IssueCommand(request);
    if(enqueued)
    {
        NVMainMemoryRequest *memRequest = new NVMainMemoryRequest;

        memRequest->request = request;
        memRequest->packet = pkt;
        memRequest->issueTick = curTick();
        memRequest->atomic = false;

        DPRINTF(NVMain, "nvmain_mem.cc: Enqueued Mem request for 0x%x of type %s\n", request->address.GetPhysicalAddress( ), ((pkt->isRead()) ? "READ" : "WRITE") );

        memory.m_request_map.insert( std::pair<NVMainRequest *, NVMainMemoryRequest *>( request, memRequest ) );

        /*
         *  It seems gem5 will block until the packet gets a response, so create a copy of the request, so
         *  the memory controller has it, then delete the original copy to respond to the packet.
         */
        if( request->type == WRITE )
          {
            NVMainMemoryRequest *requestCopy = new NVMainMemoryRequest( );

            requestCopy->request = new NVMainRequest( );
            *(requestCopy->request) = *request;
            requestCopy->packet = pkt;
            requestCopy->issueTick = curTick();
            requestCopy->atomic = false;

            memRequest->packet = NULL;

            memory.m_request_map.insert( std::pair<NVMainRequest *, NVMainMemoryRequest *>( requestCopy->request, requestCopy ) );

            memory.RequestComplete( requestCopy->request );
          }
    }
    else
    {
        DPRINTF(NVMain, "nvmain_mem.cc: FAILED enqueue Mem request for 0x%x of type %s\n", request->address.GetPhysicalAddress( ), ((pkt->isRead()) ? "READ" : "WRITE") );

        if (pkt->isRead())
        {
            memory.retryRead = true;
        }
        else
        {
            memory.retryWrite = true;
        }

        delete request;
        request = NULL;
    }

    //memory.Sync();

    /* Call post-issue hooks. */
    if( request != NULL )
    {
        for( it = postHooks.begin(); it != postHooks.end(); it++ )
        {
            (*it)->SetParent( &memory );
            (*it)->IssueCommand( request );
        }
    }

    return enqueued;
}



Tick NVMainMemory::doAtomicAccess(PacketPtr pkt)
{
    access(pkt);
    return static_cast<Tick>(m_avgAtomicLatency);
}



void NVMainMemory::doFunctionalAccess(PacketPtr pkt)
{
    functionalAccess(pkt);
}


void NVMainMemory::NVMainMemoryEvent::process()
{
    // Cycle memory controller
    memory.m_nvmainPtr->Cycle( 1 );

    memory.eventManager->scheduleWakeup( );
}


bool NVMainMemory::RequestComplete(NVM::NVMainRequest *req)
{
    bool isRead = (req->type == READ || req->type == READ_PRECHARGE);
    bool isWrite = (req->type == WRITE || req->type == WRITE_PRECHARGE);

    /* Ignore bus read/write requests generated by the banks. */
    if( req->type == BUS_WRITE || req->type == BUS_READ )
    {
        delete req;
        return true;
    }

    NVMainMemoryRequest *memRequest;
    std::map<NVMainRequest *, NVMainMemoryRequest *>::iterator iter;

    // Find the mem request pointer in the map.
    assert(m_request_map.count(req) != 0);
    iter = m_request_map.find(req);
    memRequest = iter->second;

    //if(memRequest->packet && !memRequest->atomic)
    if(!memRequest->atomic)
    {
        bool respond = false;

        if( memRequest->packet )
        {
            respond = memRequest->packet->needsResponse();
            access(memRequest->packet);
        }

        /*
         *  If we have combined queues (FRFCFS/FCFS) there is a problem.
         *  I assume that gem5 will stall such that only one type of request
         *  will need a retry, however I do not explicitly enfore that only
         *  one sendRetry() be called.
         */
        if( retryRead == true && (isRead || isWrite) )
        {
            retryRead = false;
            port.sendRetry();
        }
        if( retryWrite == true && (isRead || isWrite) )
        {
            retryWrite = false;
            port.sendRetry();
        }

        DPRINTF(NVMain, "Completed Mem request for 0x%x of type %s\n", req->address.GetPhysicalAddress( ), (isRead ? "READ" : "WRITE"));

        if(respond)
        {
            port.queue.schedSendTiming(memRequest->packet, curTick() + 1);

            delete req;
            delete memRequest;
        }
        else
        {
            /* modified by Tao @ 01/24/2013 */
            if( memRequest->packet )
                pendingDelete.push_back(memRequest->packet);
            //delete memRequest->packet;
            delete req;
            delete memRequest;
        }
    }
    else
    {
        delete req;
        delete memRequest;
    }


    m_request_map.erase(iter);

    return true;
}


void NVMainMemory::serialize(std::ostream& os)
{
    std::string nvmain_chkpt_file = "";

    if( m_nvmainConfig->KeyExists( "CheckpointDirectory" ) )
        nvmain_chkpt_file += m_nvmainConfig->GetString( "CheckpointDirectory" ) + "/";
    if( m_nvmainConfig->KeyExists( "CheckpointName" ) )
        nvmain_chkpt_file += m_nvmainConfig->GetString( "CheckpointName" );

    std::cout << "NVMainMemory: Writing to checkpoint file " << nvmain_chkpt_file << std::endl;

    // TODO: Add checkpoint to nvmain itself.
    //m_nvmainPtr->Checkpoint( nvmain_chkpt_file );
}


void NVMainMemory::unserialize(Checkpoint *cp, const std::string& section)
{
    std::string nvmain_chkpt_file = "";

    if( m_nvmainConfig->KeyExists( "CheckpointDirectory" ) )
        nvmain_chkpt_file += m_nvmainConfig->GetString( "CheckpointDirectory" ) + "/";
    if( m_nvmainConfig->KeyExists( "CheckpointName" ) )
        nvmain_chkpt_file += m_nvmainConfig->GetString( "CheckpointName" );

    // TODO: Add checkpoint to nvmain itself
    //m_nvmainPtr->Restore( nvmain_chkpt_file );

    // If we are restoring from a checkpoint, we need to reschedule the wake up
    // so it is not in the past.
    if( eventManager->memEvent->scheduled() && eventManager->memEvent->when() <= curTick() )
    {
        eventManager->deschedule( eventManager->memEvent );
        eventManager->scheduleWakeup();
    }
}


NVMainMemory::NVMainMemoryEventManager::NVMainMemoryEventManager(::EventQueue* eventq, NVMainMemory &_memory)
    : EventManager(eventq), memory(_memory)
{
    memEvent = new NVMainMemoryEvent(_memory);
}


void NVMainMemory::NVMainMemoryEventManager::scheduleWakeup( )
{
    schedule(memEvent, curTick() + memory.clock);
}


NVMainMemory *
NVMainMemoryParams::create()
{
    return new NVMainMemory(this);
}

