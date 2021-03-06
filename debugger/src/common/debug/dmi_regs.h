/**
 *  Copyright 2019 Sergey Khabarov, sergeykhbr@gmail.com
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#pragma once

#include <iservice.h>
#include "generic/mapreg.h"

namespace debugger {

class DMCONTROL_TYPE : public MappedReg64Type {
 public:
    DMCONTROL_TYPE(IService *parent, const char *name, uint64_t addr) :
                MappedReg64Type(parent, name, addr) {}

    union ValueType {
        uint64_t val;
        uint8_t u8[8];
        struct {
            uint64_t dmactive : 1;          // [0] 1=module functioning normally
            uint64_t ndmreset : 1;          // [1] 1=system reset
            uint64_t rsrv5_2 : 4;           // [5:2]
            uint64_t hartselhi : 10;        // [15:6]
            uint64_t hartsello : 10;        // [25:16]
            uint64_t rsrv29_26  : 4;        // [29:26]
            uint64_t resumereq : 1;         // [30]
            uint64_t haltreq : 1;           // [31]
            uint64_t rsv      : 32;
        } bits;
    };

 protected:
    virtual uint64_t aboutToWrite(uint64_t new_val) override;
    virtual uint64_t aboutToRead(uint64_t cur_val) override;
};

class DMSTATUS_TYPE : public MappedReg64Type {
 public:
    DMSTATUS_TYPE(IService *parent, const char *name, uint64_t addr) :
                MappedReg64Type(parent, name, addr) {}

    union ValueType {
        uint64_t val;
        uint8_t u8[8];
        struct {
            uint64_t version : 4;           // [3:0] 2=version 0.13
            uint64_t rsrv6_4 : 3;           // [6:4]
            uint64_t authenticated : 1;     // [7]
            uint64_t anyhalted : 1;         // [8]
            uint64_t allhalted : 1;         // [9]
            uint64_t anyrunning : 1;        // [10]
            uint64_t allrunning : 1;        // [11]
            uint64_t anyunavail : 1;        // [12]
            uint64_t allunavail : 1;        // [13]
            uint64_t anynonexistent: 1;     // [14]
            uint64_t allnonexistent: 1;     // [15]
            uint64_t anyresumeack: 1;       // [16]
            uint64_t allresumeack: 1;       // [17]
            uint64_t rsrv21_18  : 4;        // [21:18]
            uint64_t impebreak : 1;         // [22]
            uint64_t rsv      : 41;         // [63:23]
        } bits;
    };

 protected:
    virtual uint64_t aboutToRead(uint64_t cur_val) override;
};

class HALTSUM_TYPE : public MappedReg64Type {
 public:
    HALTSUM_TYPE(IService *parent, const char *name, uint64_t addr) :
                MappedReg64Type(parent, name, addr) {}
 protected:
    virtual uint64_t aboutToRead(uint64_t cur_val) override;
};


}  // namespace debugger

