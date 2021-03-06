/*
 *  Copyright 2018 Sergey Khabarov, sergeykhbr@gmail.com
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

#include "api_core.h"
#include "rtl_wrapper.h"
#include "iservice.h"
#include "ihap.h"
#include <riscv-isa.h>
#include "coreservices/iserial.h"

namespace debugger {

RtlWrapper::RtlWrapper(IFace *parent, sc_module_name name) : sc_module(name),
    o_clk("clk", 10, SC_NS),
    o_nrst("o_nrst"),
    o_msti("o_msti"),
    i_msto("i_msto"),
    o_interrupt("o_interrupt"),
    o_dport_req_valid("o_dport_req_valid"),
    o_dport_write("o_dport_write"),
    o_dport_addr("o_dport_addr"),
    o_dport_wdata("o_dport_wdata"),
    i_dport_req_ready("i_dport_req_ready"),
    o_dport_resp_ready("o_dport_resp_ready"),
    i_dport_resp_valid("i_dport_resp_valid"),
    i_dport_rdata("i_dport_rdata"),
    i_halted("i_halted") {
    iparent_ = parent;
    clockCycles_ = 1000000; // 1 MHz when default resolution = 1 ps

    v.nrst = 1;//0;
    v.req_addr = 0;
    v.req_len = 0;
    v.req_burst = 0;
    v.b_valid = 0;
    v.b_resp = 0;
    v.interrupt = false;
    async_interrupt = 0;
    request_reset = false;
    w_interrupt = 0;
    v.halted = false;
    v.state = State_Idle;
    r.state = State_Idle;
    v.clk_cnt = 0;
    r.clk_cnt = 0;
    RISCV_event_create(&dport_.valid, "dport_valid");
    dport_.trans_idx_up = 0;
    dport_.trans_idx_down = 0;
    trans.source_idx = 0;//CFG_NASTI_MASTER_CACHED;

    SC_METHOD(comb);
    sensitive << w_interrupt;
    sensitive << i_halted;
    sensitive << i_msto;
    sensitive << i_halted;
    sensitive << r.clk_cnt;
    sensitive << r.req_addr;
    sensitive << r.req_len;
    sensitive << r.req_burst;
    sensitive << r.b_valid;
    sensitive << r.b_resp;
    sensitive << r.nrst;
    sensitive << r.interrupt;
    sensitive << r.state;
    sensitive << r.halted;
    sensitive << r.r_error;
    sensitive << r.w_error;
    sensitive << w_resp_valid;
    sensitive << wb_resp_data;
    sensitive << w_r_error;
    sensitive << w_w_error;
    sensitive << w_dport_req_valid;
    sensitive << w_dport_resp_ready;
    sensitive << w_dport_write;
    sensitive << wb_dport_addr;
    sensitive << wb_dport_wdata;

    SC_METHOD(registers);
    sensitive << o_clk.posedge_event();

    SC_METHOD(sys_bus_proc);
    sensitive << bus_event_;
}

RtlWrapper::~RtlWrapper() {
    RISCV_event_close(&dport_.valid);
}

void RtlWrapper::generateVCD(sc_trace_file *i_vcd, sc_trace_file *o_vcd) {
    if (i_vcd) {
    }
    if (o_vcd) {
        sc_trace(o_vcd, i_msto, i_msto.name());
        sc_trace(o_vcd, o_msti, o_msti.name());

        std::string pn(name());
        sc_trace(o_vcd, r.nrst, pn + ".r_nrst");
        sc_trace(o_vcd, r.state, pn + ".r_state");
        sc_trace(o_vcd, r.req_addr, pn + ".r_req_addr");
        sc_trace(o_vcd, r.req_len, pn + ".r_req_len");
        sc_trace(o_vcd, r.req_burst, pn + ".r_req_burst");
        sc_trace(o_vcd, r.b_resp, pn + ".r_b_resp");
        sc_trace(o_vcd, r.b_valid, pn + ".r_b_valid");
        sc_trace(o_vcd, wb_wdata, pn + ".wb_wdata");
        sc_trace(o_vcd, wb_wstrb, pn + ".wb_wstrb");
    }
}

void RtlWrapper::clk_gen() {
    // todo: instead sc_clock
}

void RtlWrapper::comb() {
    bool w_req_mem_ready;
    sc_uint<CFG_BUS_ADDR_WIDTH> vb_req_addr;
    sc_uint<4> vb_r_resp;
    bool v_r_valid;
    bool v_r_last;
    bool v_w_ready;
    sc_uint<BUS_DATA_WIDTH> vb_wdata;
    sc_uint<BUS_DATA_BYTES> vb_wstrb;
    axi4_master_in_type vmsti;

    w_req_mem_ready = 0;
    vb_r_resp = 0; // OKAY
    v_r_valid = 0;
    v_r_last = 0;
    v_w_ready = 0;
    v.b_valid = 0;
    v.b_resp = 0;

    vb_wdata = 0;
    vb_wstrb = 0;

    v.clk_cnt = r.clk_cnt.read() + 1;
    v.interrupt = w_interrupt;
    v.halted = i_halted.read();

    v.nrst = (r.nrst.read() << 1) | 1;
    switch (r.state.read()) {
    case State_Idle:
        v.r_error = 0;
        v.w_error = 0;
        if (request_reset) {
            v.state = State_Reset;
        } else {
            w_req_mem_ready = 1;
        }
        break;
    case State_Read:
        v_r_valid = 1;
        if (r.req_len.read() == 0) {
            w_req_mem_ready = 1;
            v_r_last = 1;
        } else {
            v.r_error = r.r_error.read() || w_r_error;
            v.req_len = r.req_len.read() - 1;
            if (r.req_burst.read() == 1) {
                vb_req_addr = r.req_addr.read() + 8;
            }
            v.req_addr = vb_req_addr;
        }
        break;
    case State_Write:
        vb_wdata = i_msto.read().w_data;
        vb_wstrb = i_msto.read().w_strb;
        v_w_ready = 1;
        if (r.req_len.read() == 0) {
            v.b_valid = 1;
            v.b_resp = r.w_error.read() || w_w_error ? 0x2 : 0x0;
            w_req_mem_ready = 1;
        } else {
            v.w_error = r.w_error.read() || w_w_error;
            v.req_len = r.req_len.read() - 1;
            if (r.req_burst.read() == 1) {
                vb_req_addr = r.req_addr.read() + 8;
            }
            v.req_addr = vb_req_addr;
        }
        break;
    case State_Reset:
        request_reset = false;
        v.nrst = (r.nrst.read() << 1);
        v.state = State_Idle;
        break;
    default:;
    }

    if (w_req_mem_ready == 1) {
        v.r_error = 0;
        v.w_error = 0;
        if (i_msto.read().ar_valid) {
            v.state = State_Read;
            v.req_addr = i_msto.read().ar_bits.addr;
            v.req_burst = i_msto.read().ar_bits.burst;
            v.req_len = i_msto.read().ar_bits.len;
        } else if (i_msto.read().aw_valid) {
            v.state = State_Write;
            v.req_addr = i_msto.read().aw_bits.addr;
            v.req_burst = i_msto.read().aw_bits.burst;
            v.req_len = i_msto.read().aw_bits.len;
        } else {
            v.state = State_Idle;
        }
    }

    if (r.r_error.read() || w_r_error.read()) {
        vb_r_resp = 0x2;    // SLVERR
    }

    wb_wdata = vb_wdata;
    wb_wstrb = vb_wstrb;

    o_nrst = r.nrst.read()[1].to_bool();

    vmsti.aw_ready = w_req_mem_ready;
    vmsti.w_ready = v_w_ready;
    vmsti.b_valid = r.b_valid;
    vmsti.b_resp = r.b_resp;
    vmsti.b_id = 0;
    vmsti.b_user = 0;

    vmsti.ar_ready = w_req_mem_ready;
    vmsti.r_valid = v_r_valid;
    vmsti.r_resp = vb_r_resp;                    // 0=OKAY;1=EXOKAY;2=SLVERR;3=DECER
    vmsti.r_data = wb_resp_data;
    vmsti.r_last = v_r_last;
    vmsti.r_id = 0;
    vmsti.r_user = 0;
    o_msti = vmsti;     // to trigger event;

    o_interrupt = r.interrupt;

    o_dport_req_valid = w_dport_req_valid;
    o_dport_write = w_dport_write;
    o_dport_resp_ready = w_dport_resp_ready;
    o_dport_write = w_dport_write;
    o_dport_addr = wb_dport_addr;
    o_dport_wdata = wb_dport_wdata;

    if (!r.nrst.read()[1]) {
    }
}

void RtlWrapper::registers() {
    bus_event_.notify(1, SC_NS);
    r = v;
}

void RtlWrapper::sys_bus_proc() {
    /** Simulation events queue */
    IFace *cb;
    uint8_t strob;
    uint64_t offset;

    step_queue_.initProc();
    step_queue_.pushPreQueued();
    uint64_t step_cnt = r.clk_cnt.read();
    while ((cb = step_queue_.getNext(step_cnt)) != 0) {
        static_cast<IClockListener *>(cb)->stepCallback(step_cnt);
    }

    w_interrupt = async_interrupt;

    w_resp_valid = 0;
    wb_resp_data = 0;
    w_r_error = 0;
    w_w_error = 0;

    switch (r.state.read()) {
    case State_Read:
        trans.action = MemAction_Read;
        trans.addr = r.req_addr.read();
        trans.xsize = 8;
        trans.wstrb = 0;
        trans.wpayload.b64[0] = 0;
        resp = ibus_->b_transport(&trans);

        w_resp_valid = 1;
        wb_resp_data = trans.rpayload.b64[0];
        if (resp == TRANS_ERROR) {
            w_r_error = 1;
        }
        break;
    case State_Write:
        trans.action = MemAction_Write;
        strob = static_cast<uint8_t>(wb_wstrb.read());
        offset = mask2offset(strob);
        trans.addr = r.req_addr.read();
        trans.addr += offset;
        trans.xsize = mask2size(strob >> offset);
        trans.wstrb = (1 << trans.xsize) - 1;
        trans.wpayload.b64[0] = wb_wdata.read();
        resp = ibus_->b_transport(&trans);

        w_resp_valid = 1;
        if (resp == TRANS_ERROR) {
            w_w_error = 1;
        }
        break;
    default:;
    }

    // Debug port handling:
    w_dport_req_valid = 0;
    w_dport_resp_ready = 1;
    if (RISCV_event_is_set(&dport_.valid)) {
        RISCV_event_clear(&dport_.valid);
        w_dport_req_valid = 1;
        w_dport_write = dport_.trans->write;
        wb_dport_addr = dport_.trans->addr;
        wb_dport_wdata = dport_.trans->wdata;
    }
    dport_.idx_missmatch = 0;
    if (i_dport_resp_valid.read()) {
        dport_.trans->rdata = i_dport_rdata.read().to_uint64();
        dport_.trans_idx_down++;
        if (dport_.trans_idx_down != dport_.trans_idx_up) {
            dport_.idx_missmatch = 1;
            RISCV_error("error: sync. is lost: up=%d, down=%d",
                         dport_.trans_idx_up, dport_.trans_idx_down);
            dport_.trans_idx_down = dport_.trans_idx_up;
        }
        dport_.cb->nb_response_debug_port(dport_.trans);
    }

}

uint64_t RtlWrapper::mask2offset(uint8_t mask) {
    for (int i = 0; i < BUS_DATA_BYTES; i++) {
        if (mask & 0x1) {
            return static_cast<uint64_t>(i);
        }
        mask >>= 1;
    }
    return 0;
}

uint32_t RtlWrapper::mask2size(uint8_t mask) {
    uint32_t bytes = 0;
    for (int i = 0; i < BUS_DATA_BYTES; i++) {
        if (!(mask & 0x1)) {
            break;
        }
        bytes++;
        mask >>= 1;
    }
    return bytes;
}

void RtlWrapper::setClockHz(double hz) {
    sc_time dt = sc_get_time_resolution();
    clockCycles_ = static_cast<int>((1.0 / hz) / dt.to_seconds() + 0.5);
}
    
void RtlWrapper::registerStepCallback(IClockListener *cb, uint64_t t) {
    if (request_reset) {
        if (r.clk_cnt.read() == t) {
            cb->stepCallback(t);
        }
    } else {
        step_queue_.put(t, cb);
    }
}

bool RtlWrapper::isHalt() {
    return i_halted.read();
}

void RtlWrapper::raiseSignal(int idx) {
    switch (idx) {
    case SIGNAL_HardReset:
        request_reset = true;
        break;
    case INTERRUPT_MExternal:
        async_interrupt = true;
        break;
    default:;
    }
}

void RtlWrapper::lowerSignal(int idx) {
    switch (idx) {
    case INTERRUPT_MExternal:
        async_interrupt = false;
        break;
    default:;
    }
}

void RtlWrapper::nb_transport_debug_port(DebugPortTransactionType *trans,
                                         IDbgNbResponse *cb) {
    dport_.trans = trans;
    dport_.cb = cb;
    dport_.trans_idx_up++;
    RISCV_event_set(&dport_.valid);
}

}  // namespace debugger

