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

#include "mem/cache/tags/wbar.hh"

#include "debug/CacheRepl.hh"
#include "mem/cache/base.hh"

WBAR::WBAR(const Params *p)
    : BaseSetAssoc(p)
{
}

CacheBlk*
WBAR::accessBlock(Addr addr, bool is_secure, Cycles &lat, int master_id)
{
    CacheBlk *blk = BaseSetAssoc::accessBlock(addr, is_secure, lat, master_id);
    int count;//stx
    //    count = sets[blk->set].getcounter();//stx
//    pos = blk->way + count/2;//stx
    if (blk != nullptr) {
	count = sets[blk->set].counter;//stx
    //    pos = blk->way - count/2;//stx
//	if(pos<0) pos=0 ;        
// move this block to head of the MRU list
	if(blk->type == 1){
        	sets[blk->set].moveToHead(blk);
        }
	else if(blk->type == 0){
//		sets[blk->set].moveToHead(blk);
	        sets[blk->set].promote(blk,count/2);
	}
    //    sets[blk->set].moveToHead(blk);
        DPRINTF(CacheRepl, "set %x: moving blk %x (%s) to MRU\n",
                blk->set, regenerateBlkAddr(blk->tag, blk->set),
                is_secure ? "s" : "ns");
    }

    return blk;
}

CacheBlk*
WBAR::findVictim(Addr addr)
{
    int set = extractSet(addr);
    // grab a replacement candidate
    BlkType *blk = nullptr;
    for (int i = assoc - 1; i >= 0; i--) {
        BlkType *b = sets[set].blks[i];
        if (b->way < allocAssoc) {
            blk = b;
            break;
        }
    }
    assert(!blk || blk->way < allocAssoc);

    if (blk && blk->isValid()) {
        DPRINTF(CacheRepl, "set %x: selecting blk %x for replacement\n",
                set, regenerateBlkAddr(blk->tag, set));
    }

    return blk;
}

void
WBAR::insertBlock(PacketPtr pkt, BlkType *blk)
{
    int pos,count;//stx
    BaseSetAssoc::insertBlock(pkt, blk);

    blk->type = settype(pkt->getAddr()) ? 0:1;//stx-wbar
    int set = extractSet(pkt->getAddr());
    count = sets[set].counter;
    if (pkt->isWriteback()){
    	if (blk->type == 1) pos = count/8;
	else pos = assoc-1-count/2;
//	if (count > 0) sets[set].counter--;
    }
    else{
	if(blk->type == 1) {
	  pos = assoc-1-count/4;
	  if (count > 0) sets[set].counter--;
        }
	else{ 
	  pos = assoc-1-count/8;
	  sets[set].counter++;
	}
    }
    //sets[blk->set].moveToHead(blk);
    sets[set].moveToPos(blk, pos);
   // blk->way = pos;
}

void
WBAR::invalidate(CacheBlk *blk)
{
    BaseSetAssoc::invalidate(blk);

    // should be evicted before valid blocks
    int set = blk->set;
    sets[set].moveToTail(blk);
}

 bool
WBAR::settype(Addr addr){
        unsigned bord = 1024*1024*1024;
        unsigned tag = extractTag(addr);
        if(tag*blkSize*numSets <= bord) return true;
        else return false;

}

WBAR*
WBARParams::create()
{
    return new WBAR(this);
}
