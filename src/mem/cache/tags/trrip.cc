/*
 * Copyright (c) 2012-2013 ARM Limited
 * All rights reserved.
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2003-2005,2014 The Regents of The University of Michigan
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
 *
 * Authors: Erik Hallnor
 */

/**
 * @file
 * Definitions of a LRU tag store.
 */

#include "mem/cache/tags/trrip.hh"

#include "debug/CacheRepl.hh"
#include "mem/cache/base.hh"
#define RRPVMAX 7
#define BIMODAL 80  
TRRIP::TRRIP(const Params *p)
    : BaseSetAssoc(p)
{
}

CacheBlk*
TRRIP::accessBlock(Addr addr, bool is_secure, Cycles &lat, int master_id)
{
    CacheBlk *blk = BaseSetAssoc::accessBlock(addr, is_secure, lat, master_id);
    int set = extractSet(addr);
    if (sets[set].bip_interval == BIMODAL) sets[set].bip_interval=0;
    else sets[set].bip_interval++;
    if (blk != nullptr) {
        // move this block to head of the MRU list
	if ( blk->type == 1) blk->rrpv = 0;
	else {
	    blk->rrpv-=3;
	    if (blk->rrpv < 0) blk->rrpv = 0;
	}
  //      sets[blk->set].moveToHead(blk);
        DPRINTF(CacheRepl, "set %x: moving blk %x (%s) to MRU\n",
                blk->set, regenerateBlkAddr(blk->tag, blk->set),
                is_secure ? "s" : "ns");
    }

    return blk;
}

CacheBlk*
TRRIP::findVictim(Addr addr)
{
    CacheBlk *blk = BaseSetAssoc::findVictim(addr);
    int set = extractSet(addr);
    // grab a replacement candidate
    if (blk && blk->isValid()){
	blk = search(addr);
	while (blk->rrpv < RRPVMAX){
	    for (int j=0;j<assoc;j++){
		BlkType *b = sets[set].blks[j];
		b->rrpv++;
	    }
	}         
        assert(blk->way < allocAssoc);
        DPRINTF(CacheRepl, "set %x: selecting blk %x for replacement\n",
               set, regenerateBlkAddr(blk->tag, set));
    }

    return blk;
}

CacheBlk*
TRRIP::search(Addr addr)
{
    int set = extractSet(addr);
    // grab a replacement candidate
    BlkType *blk = sets[set].blks[0];
    for (int i = assoc-1; i < 0; i--) {
	BlkType *b = sets[set].blks[i];
	if (b->rrpv > blk->rrpv) blk = b;
//	if (b->rrpv == blk->rrpv && !b->isDirty()) blk = b;  
    }
    return blk;
}
/*
    assert(!blk || blk->way < allocAssoc);

    if (blk && blk->isValid()) {
        DPRINTF(CacheRepl, "set %x: selecting blk %x for replacement\n",
                set, regenerateBlkAddr(blk->tag, set));
    }

    return blk;
}*/

void
TRRIP::insertBlock(PacketPtr pkt, BlkType *blk)
{
    BaseSetAssoc::insertBlock(pkt, blk);
    int set = extractSet(pkt->getAddr());
    
    if (sets[set].bip_interval == BIMODAL) {
	blk->rrpv = RRPVMAX - 2;
	//sets[set].moveToHead(blk);
    }
    else {
	if (blk->type == 0) blk->rrpv = RRPVMAX;
        else blk->rrpv = RRPVMAX - 1;
    }
}

void
TRRIP::invalidate(CacheBlk *blk)
{
    BaseSetAssoc::invalidate(blk);

    // should be evicted before valid blocks
    int set = blk->set;
    sets[set].moveToTail(blk);
}

TRRIP*
TRRIPParams::create()
{
    return new TRRIP(this);
}
