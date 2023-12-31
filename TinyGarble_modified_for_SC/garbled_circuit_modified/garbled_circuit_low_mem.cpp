/*
 This file is part of JustGarble.

 JustGarble is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 JustGarble is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with JustGarble.  If not, see <http://www.gnu.org/licenses/>.

 */
/*
 This file is part of TinyGarble. It is modified version of JustGarble
 under GNU license.

 TinyGarble is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TinyGarble is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TinyGarble.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "garbled_circuit/garbled_circuit_low_mem.h"

#include "scd/scd.h"
#include "scd/scd_evaluator.h"
#include "util/log.h"
#include "crypto/BN.h"
#include "crypto/OT.h"
#include "crypto/OT_extension.h"
#include "garbled_circuit/garbled_circuit_util.h"
#include "tcpip/tcpip.h"
#include "util/common.h"
#include "util/util.h"
//time calculation
//#include <boost/tokenizer.hpp>
//#include <chrono>
//using namespace std::chrono;
//end

int GarbleBNLowMem(const GarbledCircuit& garbled_circuit, BIGNUM* p_init,
                   BIGNUM* p_input, BIGNUM* g_init, BIGNUM* g_input,
                   uint64_t* clock_cycles, const string& output_mask,
                   int64_t terminate_period, OutputMode output_mode,
                   BIGNUM* output_bn, block R, block global_key,
                   bool disable_OT, int connfd) {

  uint64_t ot_time = 0;
  uint64_t garble_time = 0;
  uint64_t comm_time = 0;

  block* init_labels = nullptr;
  block* input_labels = nullptr;
  BlockPair terminate_label;
  short terminate_val;
  block* output_labels = nullptr;
  short* output_vals = nullptr;

  BlockPair *wires = nullptr;
  CHECK_ALLOC(wires = new BlockPair[garbled_circuit.get_wire_size()]);
  short *wires_val = nullptr;
  CHECK_ALLOC(wires_val = new short[garbled_circuit.get_wire_size()]);
  for (uint64_t i = 0; i < garbled_circuit.get_wire_size(); i++) {
    wires_val[i] = SECRET;  // All wires are initialed with unknown.
  }

  BlockPair *dff_latch = nullptr;
  CHECK_ALLOC(dff_latch = new BlockPair[garbled_circuit.dff_size]);
  short *dff_latch_val = nullptr;
  CHECK_ALLOC(dff_latch_val = new short[garbled_circuit.dff_size]);
  for (uint64_t i = 0; i < garbled_circuit.dff_size; i++) {
    dff_latch_val[i] = SECRET;  // All wires are initialed with secret.
  }

  int *fanout = nullptr;
  CHECK_ALLOC(fanout = new int[garbled_circuit.gate_size]);

  uint64_t num_of_non_xor = NumOfNonXor(garbled_circuit);
  uint64_t garbled_tables_size = num_of_non_xor;
  GarbledTable* garbled_tables_temp = nullptr;
  CHECK_ALLOC(garbled_tables_temp = new GarbledTable[garbled_tables_size]);
  GarbledTable* garbled_tables = nullptr;
  CHECK_ALLOC(garbled_tables = new GarbledTable[garbled_tables_size]);

  CHECK(
      GarbleAllocLabels(garbled_circuit, &init_labels, &input_labels,
                        &output_labels, &output_vals, R));

  CHECK(GarbleGneInitLabels(garbled_circuit, init_labels, R));

  uint64_t ot_start_time = RDTSC;
  {
    CHECK(
        GarbleTransferInitLabels(garbled_circuit, g_init, init_labels,
                                 disable_OT, connfd));
  }
  ot_time += RDTSC - ot_start_time;

  AES_KEY AES_Key;
  AESSetEncryptKey((unsigned char *) &(global_key), 128, &AES_Key);
  DUMP("r_key") << R << endl;
  DUMP("r_key") << global_key << endl;

  uint64_t num_skipped_non_xor_gates = 0;
  for (uint64_t cid = 0; cid < (*clock_cycles); cid++) {
    uint64_t garbled_table_ind;
    CHECK(GarbleGenInputLabels(garbled_circuit, input_labels, R));

    ot_start_time = RDTSC;
    {
      CHECK(
          GarbleTransferInputLabels(garbled_circuit, g_input, input_labels, cid,
                                    disable_OT, connfd));
    }
    ot_time += RDTSC - ot_start_time;

    garble_time += GarbleLowMem(garbled_circuit, p_init, p_input, init_labels,
                                input_labels, garbled_tables_temp,
                                garbled_tables, &garbled_table_ind, R, AES_Key,
                                cid, connfd, wires, wires_val, dff_latch,
                                dff_latch_val, fanout, &terminate_label,
                                &terminate_val, output_labels, output_vals);

    num_skipped_non_xor_gates += num_of_non_xor - garbled_table_ind;

    uint64_t comm_start_time = RDTSC;
    {
      CHECK(SendData(connfd, &garbled_table_ind, sizeof(uint64_t)));
      CHECK(
          SendData(connfd, garbled_tables,
                   garbled_table_ind * sizeof(GarbledTable)));
    }
    comm_time += RDTSC - comm_start_time;

    CHECK(
        GarbleTransferOutputLowMem(garbled_circuit, output_labels, output_vals,
                                   cid, output_mode, output_mask, output_bn,
                                   connfd));

    //if has terminate signal
    if (terminate_period != 0 && garbled_circuit.terminate_id > 0) {
      if (cid % terminate_period == 0) {
        bool is_terminate = false;
        CHECK(
            GarbleTransferTerminate(garbled_circuit, terminate_label,
                                    terminate_val, &is_terminate, connfd));

        if (is_terminate) {
          LOG(INFO) << "Alice Terminated in " << cid + 1 << "cc out of "
              << *clock_cycles << "cc." << endl;
          *clock_cycles = cid + 1;
          break;
        }
      }
      //last clock cycle, not terminated
      if (cid == (*clock_cycles) - 1) {
        LOG(ERROR)
            << "Alice Not enough clock cycles. Circuit is not terminated in "
            << *clock_cycles << "cc." << endl;
      }
    }

  }

  LOG(INFO) << "Alice transfer labels time (cc) = " << ot_time
      << "\t(cc/bit) = "
      << ot_time
          / ((double) (garbled_circuit.e_init_size
              + (*clock_cycles) * garbled_circuit.e_input_size)) << endl;  //TODO:fix clock_cycle

  LOG(INFO) << "Non-secret skipped non-XOR gates = "
      << num_skipped_non_xor_gates << " out of "
      << num_of_non_xor * (*clock_cycles) << "\t ("
      << (100.0 * num_skipped_non_xor_gates)
          / (num_of_non_xor * (*clock_cycles)) << "%)" << endl;

  LOG(INFO) << "Total garbled non-XOR gates = "
      << num_of_non_xor * (*clock_cycles) - num_skipped_non_xor_gates << endl;

  LOG(INFO) << "Alice communication time (cc) = " << comm_time << endl;
  LOG(INFO) << "Alice garbling time (cc) = " << garble_time << endl;

  delete[] init_labels;
  delete[] input_labels;
  delete[] output_labels;
  delete[] output_vals;
  delete[] wires;
  delete[] wires_val;
  delete[] dff_latch;
  delete[] dff_latch_val;
  delete[] fanout;
  delete[] garbled_tables;
  delete[] garbled_tables_temp;
  return SUCCESS;

}

int EvaluateBNLowMem(const GarbledCircuit& garbled_circuit, BIGNUM* p_init,
                     BIGNUM* p_input, BIGNUM* e_init, BIGNUM* e_input,
                     uint64_t* clock_cycles, const string& output_mask,
                     int64_t terminate_period, OutputMode output_mode,
                     BIGNUM* output_bn, block global_key, bool disable_OT,
                     int connfd) {
  uint64_t ot_time = 0;
  uint64_t eval_time = 0;
  uint64_t comm_time = 0;

  block* init_labels = nullptr;
  block* input_labels = nullptr;
  block terminate_label;
  short terminate_val;
  block* output_labels = nullptr;
  short* output_vals = nullptr;

  block *wires = nullptr;
  CHECK_ALLOC(wires = new block[garbled_circuit.get_wire_size()]);
  short *wires_val = nullptr;
  CHECK_ALLOC(wires_val = new short[garbled_circuit.get_wire_size()]);
  for (uint64_t i = 0; i < garbled_circuit.get_wire_size(); i++) {
    wires_val[i] = SECRET;  // All wires are initialed with unknown.
  }

  block *dff_latch = nullptr;
  CHECK_ALLOC(dff_latch = new block[garbled_circuit.dff_size]);
  short *dff_latch_val = nullptr;
  CHECK_ALLOC(dff_latch_val = new short[garbled_circuit.dff_size]);
  for (uint64_t i = 0; i < garbled_circuit.dff_size; i++) {
    dff_latch_val[i] = SECRET;  // All wires are initialed with secret.
  }

  int *fanout = nullptr;
  CHECK_ALLOC(fanout = new int[garbled_circuit.gate_size]);

  uint64_t num_of_non_xor = NumOfNonXor(garbled_circuit);
  GarbledTable* garbled_tables = nullptr;
  CHECK_ALLOC(garbled_tables = new GarbledTable[num_of_non_xor]);

  CHECK(
      EvaluateAllocLabels(garbled_circuit, &init_labels, &input_labels,
                          &output_labels, &output_vals));
  uint64_t ot_start_time = RDTSC;
  {
    CHECK(
        EvaluateTransferInitLabels(garbled_circuit, e_init, init_labels,
                                   disable_OT, connfd));
  }
  ot_time += RDTSC - ot_start_time;

  AES_KEY AES_Key;
  AESSetEncryptKey((unsigned char *) &(global_key), 128, &AES_Key);
  DUMP("r_key") << global_key << endl;

  for (uint64_t cid = 0; cid < (*clock_cycles); cid++) {
    uint64_t garbled_table_ind_rcv;  // #of tables received from garbler
    uint64_t garbled_table_ind;
    ot_start_time = RDTSC;
    {
      CHECK(
          EvaluateTransferInputLabels(garbled_circuit, e_input, input_labels,
                                      cid, disable_OT, connfd));
    }
    ot_time += RDTSC - ot_start_time;

    uint64_t comm_start_time = RDTSC;
    {
      CHECK(RecvData(connfd, &garbled_table_ind_rcv, sizeof(uint64_t)));
      CHECK(
          RecvData(connfd, garbled_tables,
                   garbled_table_ind_rcv * sizeof(GarbledTable)));
    }
    comm_time += RDTSC - comm_start_time;

    eval_time += EvaluateLowMem(garbled_circuit, p_init, p_input, init_labels,
                                input_labels, garbled_tables,
                                &garbled_table_ind, AES_Key, cid, connfd, wires,
                                wires_val, dff_latch, dff_latch_val, fanout,
                                &terminate_label, &terminate_val, output_labels,
                                output_vals);

    CHECK_EXPR_MSG(garbled_table_ind == garbled_table_ind_rcv,
                   "Number of garbled tables generated "
                   "by Alice and received by Bob are not equal.");

    for (uint64_t j = 0; j < garbled_table_ind; j++) {  // clear tables
      garbled_tables[j].gid = (uint32_t)(-1);
    }

    CHECK(
        EvaluateTransferOutputLowMem(garbled_circuit, output_labels,
                                     output_vals, cid, output_mode, output_mask,
                                     output_bn, connfd));

    //if has terminate signal
    if (terminate_period != 0 && garbled_circuit.terminate_id > 0) {
      if (cid % terminate_period == 0) {
        bool is_terminate = false;
        CHECK(
            EvaluateTransferTerminate(garbled_circuit, terminate_label,
                                      terminate_val, &is_terminate, connfd));

        if (is_terminate) {
          LOG(INFO) << "Bob Terminated in " << cid + 1 << "cc out of "
              << *clock_cycles << "cc." << endl;
          *clock_cycles = cid + 1;
          break;
        }
      }
      //last clock cycle, not terminated
      if (cid == (*clock_cycles) - 1) {
        LOG(ERROR)
            << "Bob Not enough clock cycles. Circuit is not terminated in "
            << *clock_cycles << "cc." << endl;
      }
    }
  }

  LOG(INFO) << "Bob transfer labels time (cc) = " << ot_time << "\t(cc/bit) = "
      << ot_time
          / ((double) (garbled_circuit.e_init_size
              + (*clock_cycles) * garbled_circuit.e_input_size)) << endl;  //TODO:fix clock_cycle

  LOG(INFO) << "Bob communication time (cc) = " << comm_time << endl;
  LOG(INFO) << "Bob evaluation time (cc) = " << eval_time << endl;

  delete[] init_labels;
  delete[] input_labels;
  delete[] output_labels;
  delete[] output_vals;
  delete[] wires;
  delete[] wires_val;
  delete[] dff_latch;
  delete[] dff_latch_val;
  delete[] fanout;
  delete[] garbled_tables;

  return SUCCESS;
}

uint64_t GarbleLowMem(const GarbledCircuit& garbled_circuit, BIGNUM* p_init,
                      BIGNUM* p_input, block* init_labels, block* input_labels,
                      GarbledTable* garbled_tables_temp,
                      GarbledTable* garbled_tables, uint64_t *garbled_table_ind,
                      block R, AES_KEY& AES_Key, uint64_t cid, int connfd,
                      BlockPair* wires, short* wires_val, BlockPair* dff_latch,
                      short* dff_latch_val, int* fanout,
                      BlockPair* terminate_label, short* terminate_val,
                      block* output_labels, short* output_vals) {
                      //added by mohammad for time

  uint64_t start_time = RDTSC;

  // init
  int64_t gate_bias = (int64_t) garbled_circuit.get_gate_lo_index();
  uint64_t dff_bias = garbled_circuit.get_dff_lo_index();

  if (cid == 0) {
    for (uint64_t i = 0; i < garbled_circuit.dff_size; i++) {
      int64_t wire_index = garbled_circuit.I[i];
      if (wire_index == CONST_ZERO) {
        wires_val[dff_bias + i] = 0;
      } else if (wire_index == CONST_ONE) {
        wires_val[dff_bias + i] = 1;
      } else if (wire_index >= 0
          && wire_index < (int64_t) garbled_circuit.get_init_size()) {
        if (wire_index < (int64_t) garbled_circuit.get_p_init_hi_index()) {
          wires_val[dff_bias + i] = BN_is_bit_set(
              p_init,
              wire_index - (int64_t) garbled_circuit.get_p_init_lo_index());
        } else {
          int64_t label_index = wire_index
              - (int64_t) garbled_circuit.get_p_init_hi_index();
          wires[dff_bias + i].label0 = init_labels[label_index * 2 + 0];
          wires[dff_bias + i].label1 = init_labels[label_index * 2 + 1];
          wires_val[dff_bias + i] = SECRET;
        }
      } else {
        LOG(ERROR) << "Invalid I: " << wire_index << endl;
        wires_val[dff_bias + i] = 0;
      }
      DUMP("dff") << wires[dff_bias + i].label0 << endl;
    }
  } else {  //copy latched labels
    for (uint64_t i = 0; i < garbled_circuit.dff_size; i++) {
      int64_t wire_index = garbled_circuit.D[i];
      if (wire_index == CONST_ZERO) {
        dff_latch_val[i] = 0;
      } else if (wire_index == CONST_ONE) {
        dff_latch_val[i] = 1;
      } else if (wire_index >= 0
          && wire_index < (int64_t) garbled_circuit.get_wire_size()) {
        dff_latch[i].label0 = wires[wire_index].label0;
        dff_latch[i].label1 = wires[wire_index].label1;
        dff_latch_val[i] = wires_val[wire_index];
      } else {
        LOG(ERROR) << "Invalid D: " << wire_index << endl;
        dff_latch_val[i] = 0;  // Wire with invalid D values become 0.
      }
    }
    for (uint64_t i = 0; i < garbled_circuit.dff_size; i++) {
      wires[dff_bias + i].label0 = dff_latch[i].label0;
      wires[dff_bias + i].label1 = dff_latch[i].label1;
      wires_val[dff_bias + i] = dff_latch_val[i];
    }
  }

  // inputs
  uint64_t p_input_bias = garbled_circuit.get_p_input_lo_index();
  for (uint64_t i = 0; i < garbled_circuit.p_input_size; i++) {
    wires_val[p_input_bias + i] = BN_is_bit_set(
        p_input, cid * garbled_circuit.p_input_size + i);
  }

  uint64_t input_bias = garbled_circuit.get_g_input_lo_index();
  for (uint64_t i = 0; i < garbled_circuit.get_secret_input_size(); i++) {
        //added by  Mohammad for garble inputs start time
    uint64_t start = RDTSC;
    wires[input_bias + i].label0 = input_labels[i * 2 + 0];
    wires[input_bias + i].label1 = input_labels[i * 2 + 1];
    wires_val[input_bias + i] = SECRET;

    DUMP("input") << input_labels[i * 2 + 0] << endl;
            //added by  Mohammad for garble inputs end time
         uint64_t stop = RDTSC;
     //LOG(INFO) << "Garbled Input " << i << "time = " << stop - start << endl;
 cout<<"Garbled Input " << i << "time = " << stop - start << endl;
  }

  for (uint64_t i = 0; i < garbled_circuit.gate_size; i++) {  // known values
      //added by  Mohammad for garble inputs start time
    //uint64_t start = RDTSC;
    fanout[i] = garbled_circuit.garbledGates[i].fanout;  // init fanout

    GarbledGate& garbledGate = garbled_circuit.garbledGates[i];
    int64_t input0 = garbledGate.input0;
    int64_t input1 = garbledGate.input1;
    int64_t output = garbledGate.output;
    int type = garbledGate.type;

    short input0_value = SECRET;
    if (input0 == CONST_ZERO) {
      input0_value = 0;
    } else if (input0 == CONST_ONE) {
      input0_value = 1;
    } else if (input0 >= 0
        && input0 < (int64_t) garbled_circuit.get_wire_size()) {
      input0_value = wires_val[input0];
    } else {
      LOG(ERROR) << "Invalid input0 index: " << input0 << endl;
      input0_value = 0;
    }

    short input1_value = SECRET;
    if (input1 == CONST_ZERO) {
      input1_value = 0;
    } else if (input1 == CONST_ONE) {
      input1_value = 1;
    } else if (input1 >= 0
        && input1 < (int64_t) garbled_circuit.get_wire_size()) {
      input1_value = wires_val[input1];
    } else if (type != NOTGATE) {
      LOG(ERROR) << "Invalid input1 index: " << input1 << endl;
      input1_value = 0;
    }

    GarbleEvalGateKnownValue(input0_value, input1_value, type,
                             &wires_val[output]);
    if (!IsSecret(wires_val[output])) {
      fanout[i] = 0;
      if (IsSecret(input0_value)) {
        ReduceFanout(garbled_circuit, fanout, input0, gate_bias);
      }
      if (IsSecret(input1_value)) {
        ReduceFanout(garbled_circuit, fanout, input1, gate_bias);
      }
    }
    //added by  Mohammad for garble inputs end time
     //     uint64_t stop = RDTSC;
 //LOG(INFO) << "Garbled Input " << i << "time = " << stop - start << endl;
 //cout<<"Garbled Input " << i << "time = " << stop - start << endl;
  }

  uint64_t garbled_table_ind_temp = 0;
  
  for (uint64_t i = 0; i < garbled_circuit.gate_size; i++) {  // secret values
   //added by  Mohammad for garble table start time
   // uint64_t start = RDTSC;
    GarbledGate& garbledGate = garbled_circuit.garbledGates[i];

    int64_t input0 = garbledGate.input0;
    int64_t input1 = garbledGate.input1;
    int64_t output = garbledGate.output;
    int type = garbledGate.type;

    if (fanout[i] > 0 && IsSecret(wires_val[output])) {
      BlockPair input0_labels = { ZeroBlock(), ZeroBlock() };
      short input0_value = SECRET;
      if (input0 == CONST_ZERO) {
        input0_value = 0;
      } else if (input0 == CONST_ONE) {
        input0_value = 1;
      } else if (input0 >= 0
          && input0 < (int64_t) garbled_circuit.get_wire_size()) {
        input0_labels = wires[input0];
        input0_value = wires_val[input0];
      } else {
        LOG(ERROR) << "Invalid input0 index: " << input0 << endl;
        input0_value = 0;
      }

      BlockPair input1_labels = { ZeroBlock(), ZeroBlock() };
      short input1_value = SECRET;
      if (input1 == CONST_ZERO) {
        input1_value = 0;
      } else if (input1 == CONST_ONE) {
        input1_value = 1;
      } else if (input1 >= 0
          && input1 < (int64_t) garbled_circuit.get_wire_size()) {
        input1_labels = wires[input1];
        input1_value = wires_val[input1];
      } else if (type != NOTGATE) {
        LOG(ERROR) << "Invalid input1 index: " << input1 << endl;
        input1_value = 0;
      }

      GarbleGate(input0_labels, input0_value, input1_labels, input1_value, type,
                 cid, i, garbled_tables_temp, &garbled_table_ind_temp, R,
                 AES_Key, &wires[output], &wires_val[output]);
      if (!IsSecret(wires_val[output])) {
        fanout[i] = 0;
        if (IsSecret(input0_value)) {
          ReduceFanout(garbled_circuit, fanout, input0, gate_bias);
        }
        if (IsSecret(input1_value)) {
          ReduceFanout(garbled_circuit, fanout, input1, gate_bias);
        }
      }
    }
     //added by  Mohammad for garble table end time
   // uint64_t stop = RDTSC;
 //LOG(INFO) << "Garbled Table " << i << "time = " << stop - start << endl;
  //cout<<"Garbled Table " << i << "time = " << stop - start << endl;
  }

  *garbled_table_ind = 0;  // fill the tables array from 0

// table select
  for (uint64_t i = 0; i < garbled_table_ind_temp; i++) {
    GarbledTable &table = garbled_tables_temp[i];
    if (fanout[table.gid] > 0) {
      garbled_tables[(*garbled_table_ind)++] = table;
      DUMP("table") << table.row[0] << endl;
      DUMP("table") << table.row[1] << endl;
    }
  }

  for (uint64_t i = 0; i < garbled_circuit.output_size; i++) {
    output_labels[(i) * 2 + 0] = wires[garbled_circuit.outputs[i]].label0;
    output_labels[(i) * 2 + 1] = wires[garbled_circuit.outputs[i]].label1;
    output_vals[i] = wires_val[garbled_circuit.outputs[i]];
  }
  if (garbled_circuit.terminate_id > 0) {
    (*terminate_label) = wires[garbled_circuit.terminate_id];
    (*terminate_val) = wires_val[garbled_circuit.terminate_id];
  }

  uint64_t end_time = RDTSC;

  if (*garbled_table_ind != 0) {
    LOG(INFO) << "@" << cid << "\tgarbled_table_ind = " << *garbled_table_ind
        << endl;
  }
  //LOG(INFO) << "Garbled Input "  << "time = " << end_time - start_time  << endl;
 //cout<<"Garbled Input "  << "time = " << end_time - start_time<< endl;
  return (end_time - start_time);

}

uint64_t EvaluateLowMem(const GarbledCircuit& garbled_circuit, BIGNUM* p_init,
                        BIGNUM* p_input, block* init_labels,
                        block* input_labels, GarbledTable* garbled_tables,
                        uint64_t *garbled_table_ind, AES_KEY& AES_Key,
                        uint64_t cid, int connfd, block *wires,
                        short* wires_val, block *dff_latch,
                        short* dff_latch_val, int* fanout,
                        block* terminate_label, short* terminate_val,
                        block* output_labels, short* output_vals) {
  *garbled_table_ind = 0;
  uint64_t start_time = RDTSC;

  // init
  uint64_t dff_bias = garbled_circuit.get_dff_lo_index();
  int64_t gate_bias = (int64_t) garbled_circuit.get_gate_lo_index();
  if (cid == 0) {
    for (uint64_t i = 0; i < garbled_circuit.dff_size; i++) {
      int64_t wire_index = garbled_circuit.I[i];
      if (wire_index == CONST_ZERO) {
        wires_val[dff_bias + i] = 0;
      } else if (wire_index == CONST_ONE) {
        wires_val[dff_bias + i] = 1;
      } else if (wire_index >= 0
          && wire_index < (int64_t) garbled_circuit.get_init_size()) {
        if (wire_index < (int64_t) garbled_circuit.get_p_init_hi_index()) {
          wires_val[dff_bias + i] = BN_is_bit_set(
              p_init,
              wire_index - (int64_t) garbled_circuit.get_p_init_lo_index());
        } else {
          int64_t label_index = wire_index
              - (int64_t) garbled_circuit.get_p_init_hi_index();

          wires[dff_bias + i] = init_labels[label_index];
          wires_val[dff_bias + i] = SECRET;
        }
      } else {
        LOG(ERROR) << "Invalid I: " << wire_index << endl;
        wires_val[dff_bias + i] = 0;
      }
      DUMP("dff") << wires[dff_bias + i] << endl;
    }
  } else {  //copy latched labels
    for (uint64_t i = 0; i < garbled_circuit.dff_size; i++) {
      int64_t wire_index = garbled_circuit.D[i];
      if (wire_index == CONST_ZERO) {
        dff_latch_val[i] = 0;
      } else if (wire_index == CONST_ONE) {
        dff_latch_val[i] = 1;
      } else if (wire_index >= 0
          && wire_index < (int64_t) garbled_circuit.get_wire_size()) {
        dff_latch[i] = wires[wire_index];
        dff_latch_val[i] = wires_val[wire_index];
      } else {
        LOG(ERROR) << "Invalid D: " << wire_index << endl;
        dff_latch_val[i] = 0;
      }
    }
    for (uint64_t i = 0; i < garbled_circuit.dff_size; i++) {
      wires[dff_bias + i] = dff_latch[i];
      wires_val[dff_bias + i] = dff_latch_val[i];
    }
  }

  // inputs
  uint64_t p_input_bias = garbled_circuit.get_p_input_lo_index();
  for (uint64_t i = 0; i < garbled_circuit.p_input_size; i++) {
    wires_val[p_input_bias + i] = BN_is_bit_set(
        p_input, cid * garbled_circuit.p_input_size + i);
  }
  uint64_t input_bias = garbled_circuit.get_g_input_lo_index();
  for (uint64_t i = 0; i < garbled_circuit.get_secret_input_size(); i++) {
    wires[input_bias + i] = input_labels[i];
    DUMP("input") << input_labels[i] << endl;
  }

  for (uint64_t i = 0; i < garbled_circuit.gate_size; i++) {  // known values
    fanout[i] = garbled_circuit.garbledGates[i].fanout;  // init fanout

    GarbledGate& garbledGate = garbled_circuit.garbledGates[i];
    int64_t input0 = garbledGate.input0;
    int64_t input1 = garbledGate.input1;
    int64_t output = garbledGate.output;
    int type = garbledGate.type;

    short input0_value = SECRET;
    if (input0 == CONST_ZERO) {
      input0_value = 0;
    } else if (input0 == CONST_ONE) {
      input0_value = 1;
    } else if (input0 >= 0
        && input0 < (int64_t) garbled_circuit.get_wire_size()) {
      input0_value = wires_val[input0];
    } else {
      LOG(ERROR) << "Invalid input0 index: " << input0 << endl;
      input0_value = 0;
    }

    short input1_value = SECRET;
    if (input1 == CONST_ZERO) {
      input1_value = 0;
    } else if (input1 == CONST_ONE) {
      input1_value = 1;
    } else if (input1 >= 0
        && input1 < (int64_t) garbled_circuit.get_wire_size()) {
      input1_value = wires_val[input1];
    } else if (type != NOTGATE) {
      LOG(ERROR) << "Invalid input1 index: " << input1 << endl;
      input1_value = 0;
    }

    GarbleEvalGateKnownValue(input0_value, input1_value, type,
                             &wires_val[output]);
    if (!IsSecret(wires_val[output])) {
      fanout[i] = 0;
      if (IsSecret(input0_value)) {
        ReduceFanout(garbled_circuit, fanout, input0, gate_bias);
      }
      if (IsSecret(input1_value)) {
        ReduceFanout(garbled_circuit, fanout, input1, gate_bias);
      }
    }
  }

  for (uint64_t i = 0; i < garbled_circuit.gate_size; i++) {  // secret values
    GarbledGate& garbledGate = garbled_circuit.garbledGates[i];
    int64_t input0 = garbledGate.input0;
    int64_t input1 = garbledGate.input1;
    int64_t output = garbledGate.output;
    int type = garbledGate.type;

    if (fanout[i] > 0 && IsSecret(wires_val[output])) {
      block input0_labels = ZeroBlock();
      short input0_value = SECRET;
      if (input0 == CONST_ZERO) {
        input0_value = 0;
      } else if (input0 == CONST_ONE) {
        input0_value = 1;
      } else if (input0 >= 0
          && input0 < (int64_t) garbled_circuit.get_wire_size()) {
        input0_labels = wires[input0];
        input0_value = wires_val[input0];
      } else {
        LOG(ERROR) << "Invalid input0 index: " << input0 << endl;
        input0_value = 0;
      }

      block input1_labels = ZeroBlock();
      short input1_value = SECRET;
      if (input1 == CONST_ZERO) {
        input1_value = 0;
      } else if (input1 == CONST_ONE) {
        input1_value = 1;
      } else if (input1 >= 0
          && input1 < (int64_t) garbled_circuit.get_wire_size()) {
        input1_labels = wires[input1];
        input1_value = wires_val[input1];
      } else if (type != NOTGATE) {
        LOG(ERROR) << "Invalid input1 index: " << input1 << endl;
        input1_value = 0;
      }
      EvalGate(input0_labels, input0_value, input1_labels, input1_value, type,
               cid, i, garbled_tables, garbled_table_ind, AES_Key,
               &wires[output], &wires_val[output]);
      if (!IsSecret(wires_val[output])) {
        fanout[i] = 0;
        if (IsSecret(input0_value)) {
          ReduceFanout(garbled_circuit, fanout, input0, gate_bias);
        }
        if (IsSecret(input1_value)) {
          ReduceFanout(garbled_circuit, fanout, input1, gate_bias);
        }
      }
    }
  }

  for (uint64_t i = 0; i < garbled_circuit.output_size; i++) {
    output_labels[i] = wires[garbled_circuit.outputs[i]];
    output_vals[i] = wires_val[garbled_circuit.outputs[i]];
    DUMP("output") << wires[garbled_circuit.outputs[i]] << endl;
  }
  if (garbled_circuit.terminate_id > 0) {
    (*terminate_label) = wires[garbled_circuit.terminate_id];
    (*terminate_val) = wires_val[garbled_circuit.terminate_id];
  }

  uint64_t end_time = RDTSC;
  return (end_time - start_time);
}

int GarbleAllocLabels(const GarbledCircuit& garbled_circuit,
                      block** init_labels, block** input_labels,
                      block** output_labels, short** output_vals, block R) {

// allocate and generate random init and inputs label pairs
  (*init_labels) = nullptr;
  if (garbled_circuit.get_init_size() > 0) {
    CHECK_ALLOC(
        (*init_labels) = new block[garbled_circuit.get_secret_init_size() * 2]);
    for (uint i = 0; i < garbled_circuit.get_secret_init_size(); i++) {
      (*init_labels)[i * 2 + 0] = RandomBlock();
      (*init_labels)[i * 2 + 1] = XorBlock(R, (*init_labels)[i * 2 + 0]);
    }
  }

  (*input_labels) = nullptr;
  if (garbled_circuit.get_secret_input_size() > 0) {
    CHECK_ALLOC(
        (*input_labels) =
            new block[garbled_circuit.get_secret_input_size() * 2]);
  }

  (*output_labels) = nullptr;
  if (garbled_circuit.output_size > 0) {
    CHECK_ALLOC((*output_labels) = new block[garbled_circuit.output_size * 2]);
  }

  (*output_vals) = nullptr;
  if (garbled_circuit.output_size > 0) {
    CHECK_ALLOC((*output_vals) = new short[garbled_circuit.output_size]);
  }

  return SUCCESS;
}

int GarbleGneInitLabels(const GarbledCircuit& garbled_circuit,
                        block* init_labels, block R) {

  for (uint i = 0; i < garbled_circuit.get_secret_init_size(); i++) {
    init_labels[i * 2 + 0] = RandomBlock();
    init_labels[i * 2 + 1] = XorBlock(R, init_labels[i * 2 + 0]);
  }

  return SUCCESS;
}

int GarbleGenInputLabels(const GarbledCircuit& garbled_circuit,
                         block* input_labels, block R) {
  if (garbled_circuit.get_secret_input_size() > 0) {
    for (uint i = 0; i < garbled_circuit.get_secret_input_size(); i++) {
      input_labels[i * 2 + 0] = RandomBlock();
      input_labels[i * 2 + 1] = XorBlock(R, input_labels[i * 2 + 0]);
    }
  }
  return SUCCESS;
}

int EvaluateAllocLabels(const GarbledCircuit& garbled_circuit,
                        block** init_labels, block** input_labels,
                        block** output_labels, short** output_vals) {

  (*init_labels) = nullptr;
  if (garbled_circuit.get_secret_init_size() > 0) {
    CHECK_ALLOC((*init_labels) =
        new block[garbled_circuit.get_secret_init_size()]);
  }

  (*input_labels) = nullptr;
  if (garbled_circuit.get_secret_input_size() > 0) {
    CHECK_ALLOC(
        (*input_labels) = new block[garbled_circuit.get_secret_input_size()]);
  }

  (*output_labels) = nullptr;
  if (garbled_circuit.output_size > 0) {
    CHECK_ALLOC((*output_labels) = new block[garbled_circuit.output_size]);
  }

  (*output_vals) = nullptr;
  if (garbled_circuit.output_size > 0) {
    CHECK_ALLOC((*output_vals) = new short[garbled_circuit.output_size]);
  }

  return SUCCESS;
}

int GarbleOTInitLowMem(const GarbledCircuit& garbled_circuit,
                       block* init_labels, int connfd) {

  uint32_t message_len = garbled_circuit.e_init_size;
  if (message_len == 0) {
    return SUCCESS;
  }
  block **message = nullptr;
  CHECK_ALLOC(message = new block*[message_len]);

  for (uint i = 0; i < garbled_circuit.e_init_size; i++) {
    CHECK_ALLOC(message[i] = new block[2]);
    for (uint j = 0; j < 2; j++) {
      message[i][j] = init_labels[(i + garbled_circuit.g_init_size) * 2 + j];
    }
  }

  if (message_len > OT_EXT_LEN) {
    CHECK(OTExtSend(message, message_len, connfd));
  } else {
    CHECK(OTSend(message, message_len, connfd));
  }

  if (message != nullptr) {
    for (uint i = 0; i < message_len; i++) {
      delete[] message[i];
    }
    delete[] message;
  }

  return SUCCESS;
}

int GarbleOTInputLowMem(const GarbledCircuit& garbled_circuit,
                        block* input_labels, uint64_t cid, int connfd) {

  uint32_t message_len = garbled_circuit.e_input_size;
  if (message_len == 0) {
    return SUCCESS;
  }
  block **message = nullptr;
  CHECK_ALLOC(message = new block*[message_len]);

  for (uint i = 0; i < garbled_circuit.e_input_size; i++) {
    CHECK_ALLOC(message[i] = new block[2]);
    for (uint j = 0; j < 2; j++) {
      message[i][j] = input_labels[(i + garbled_circuit.g_input_size) * 2 + j];
    }
  }

  if (message_len > OT_EXT_LEN) {
    CHECK(OTExtSend(message, message_len, connfd));
  } else {
    CHECK(OTSend(message, message_len, connfd));
  }

  if (message != nullptr) {
    for (uint i = 0; i < message_len; i++) {
      delete[] message[i];
    }
    delete[] message;
  }

  return SUCCESS;
}

int EvalauteOTInitLowMem(const GarbledCircuit& garbled_circuit, BIGNUM* e_init,
                         block* init_labels, int connfd) {
  uint32_t message_len = garbled_circuit.e_init_size;
  if (message_len == 0) {
    return SUCCESS;
  }
  bool *select = nullptr;
  CHECK_ALLOC(select = new bool[message_len]);
  for (uint i = 0; i < garbled_circuit.e_init_size; i++) {
    select[i] = BN_is_bit_set(e_init, i);
  }

  block* message = nullptr;
  CHECK_ALLOC(message = new block[message_len]);

  if (message_len > OT_EXT_LEN) {
    CHECK(OTExtRecv(select, message_len, connfd, message));
  } else {
    CHECK(OTRecv(select, message_len, connfd, message));
  }

  for (uint i = 0; i < garbled_circuit.e_init_size; i++) {
    init_labels[i + garbled_circuit.g_init_size] = message[i];
  }

  delete[] select;
  delete[] message;

  return SUCCESS;
}

int EvalauteOTInputLowMem(const GarbledCircuit& garbled_circuit,
                          BIGNUM* e_input, block* input_labels, uint64_t cid,
                          int connfd) {
  uint32_t message_len = garbled_circuit.e_input_size;
  if (message_len == 0) {
    return SUCCESS;
  }
  bool *select = nullptr;
  CHECK_ALLOC(select = new bool[message_len]);
  for (uint i = 0; i < garbled_circuit.e_input_size; i++) {
    select[i] = BN_is_bit_set(e_input, cid * garbled_circuit.e_input_size + i);
  }

  block* message = nullptr;
  CHECK_ALLOC(message = new block[message_len]);

  if (message_len > OT_EXT_LEN) {
    CHECK(OTExtRecv(select, message_len, connfd, message));
  } else {
    CHECK(OTRecv(select, message_len, connfd, message));
  }

  for (uint i = 0; i < garbled_circuit.e_input_size; i++) {
    input_labels[i + garbled_circuit.g_input_size] = message[i];
  }

  delete[] select;

  return SUCCESS;
}

int GarbleTransferInitLabels(const GarbledCircuit& garbled_circuit,
                             BIGNUM* g_init, block* init_labels,
                             bool disable_OT, int connfd) {

// g_init
  for (uint i = 0; i < garbled_circuit.g_init_size; i++) {
    if (i >= (uint) BN_num_bits(g_init) || BN_is_bit_set(g_init, i) == 0) {
      CHECK(SendData(connfd, &init_labels[i * 2 + 0], sizeof(block)));
    } else {
      CHECK(SendData(connfd, &init_labels[i * 2 + 1], sizeof(block)));
    }
  }

  if (disable_OT) {
// e_init
    BIGNUM* e_init = BN_new();
    CHECK(RecvBN(connfd, e_init));
    for (uint i = 0; i < garbled_circuit.e_init_size; i++) {
      if (i >= (uint) BN_num_bits(e_init) || BN_is_bit_set(e_init, i) == 0) {
        CHECK(
            SendData(connfd,
                     &init_labels[(i + garbled_circuit.g_init_size) * 2 + 0],
                     sizeof(block)));
      } else {
        CHECK(
            SendData(connfd,
                     &init_labels[(i + garbled_circuit.g_init_size) * 2 + 1],
                     sizeof(block)));
      }
    }
    BN_free(e_init);

  } else {
    CHECK(GarbleOTInitLowMem(garbled_circuit, init_labels, connfd));
  }
  return SUCCESS;
}

int GarbleTransferInputLabels(const GarbledCircuit& garbled_circuit,
                              BIGNUM* g_input, block* input_labels,
                              uint64_t cid, bool disable_OT, int connfd) {

// g_input
  for (uint i = 0; i < garbled_circuit.g_input_size; i++) {
    if (cid * garbled_circuit.g_input_size + i >= (uint) BN_num_bits(g_input)
        || BN_is_bit_set(g_input, cid * garbled_circuit.g_input_size + i)
            == 0) {
      CHECK(SendData(connfd, &input_labels[(i) * 2 + 0], sizeof(block)));
    } else {
      CHECK(SendData(connfd, &input_labels[(i) * 2 + 1], sizeof(block)));
    }
  }

  if (disable_OT) {
// e_input
    BIGNUM* e_input = BN_new();
    CHECK(RecvBN(connfd, e_input));
    for (uint i = 0; i < garbled_circuit.e_input_size; i++) {
      if (cid * garbled_circuit.e_input_size + i >= (uint) BN_num_bits(e_input)
          || BN_is_bit_set(e_input, cid * garbled_circuit.e_input_size + i)
              == 0) {
        CHECK(
            SendData(connfd,
                     &input_labels[(i + garbled_circuit.g_input_size) * 2 + 0],
                     sizeof(block)));
      } else {
        CHECK(
            SendData(connfd,
                     &input_labels[(i + garbled_circuit.g_input_size) * 2 + 1],
                     sizeof(block)));
      }
    }

    BN_free(e_input);

  } else {
    CHECK(GarbleOTInputLowMem(garbled_circuit, input_labels, cid, connfd));
  }
  return SUCCESS;
}

int EvaluateTransferInitLabels(const GarbledCircuit& garbled_circuit,
                               BIGNUM* e_init, block* init_labels,
                               bool disable_OT, int connfd) {
// g_init
  for (uint i = 0; i < garbled_circuit.g_init_size; i++) {
    CHECK(RecvData(connfd, &init_labels[i], sizeof(block)));
  }

  if (disable_OT) {
// e_init
    CHECK(SendBN(connfd, e_init));
    for (uint i = 0; i < garbled_circuit.e_init_size; i++) {
      CHECK(
          RecvData(connfd, &init_labels[i + garbled_circuit.g_init_size],
                   sizeof(block)));
    }

  } else {
    CHECK(EvalauteOTInitLowMem(garbled_circuit, e_init, init_labels, connfd));
  }
  return SUCCESS;
}

int EvaluateTransferInputLabels(const GarbledCircuit& garbled_circuit,
                                BIGNUM* e_input, block* input_labels,
                                uint64_t cid, bool disable_OT, int connfd) {
// g_input
  for (uint i = 0; i < garbled_circuit.g_input_size; i++) {
    CHECK(RecvData(connfd, &input_labels[i], sizeof(block)));
  }

  if (disable_OT) {
// e_input
    CHECK(SendBN(connfd, e_input));
    for (uint i = 0; i < garbled_circuit.e_input_size; i++) {
      CHECK(
          RecvData(connfd, &input_labels[i + garbled_circuit.g_input_size],
                   sizeof(block)));
    }
  } else {
    CHECK(
        EvalauteOTInputLowMem(garbled_circuit, e_input, input_labels, cid,
                              connfd));
  }
  return SUCCESS;
}

int GarbleTransferOutputLowMem(const GarbledCircuit& garbled_circuit,
                               block* output_labels, short* output_vals,
                               uint64_t cid, OutputMode output_mode,
                               const string& output_mask, BIGNUM* output_bn,
                               int connfd) {
  BIGNUM* output_mask_bn = BN_new();
  BN_hex2bn(&output_mask_bn, output_mask.c_str());

  uint64_t output_bit_offset = 0;
  if (output_mode == OutputMode::consecutive) {  // normal mode, keep all the bits.
    output_bit_offset = cid * garbled_circuit.output_size;
  } else if (output_mode == OutputMode::separated_clock) {  // Separated by clock mode, keep all the bits.
    output_bit_offset = cid * garbled_circuit.output_size;
  } else if (output_mode == OutputMode::last_clock) {  // keep the last cycle, overwrite the bits.
    output_bit_offset = 0;
  }

  for (uint64_t i = 0; i < garbled_circuit.output_size; i++) {
    if (output_vals[i] == 0) {
      BN_clear_bit(output_bn, output_bit_offset + i);
    } else if (output_vals[i] == 1) {
      if (i < (uint64_t) BN_num_bits(output_mask_bn)
          && BN_is_bit_set(output_mask_bn, i) == 1) {
        BN_set_bit(output_bn, output_bit_offset + i);
      }
    } else {
      short garble_output_type = get_LSB(output_labels[(i) * 2 + 0]);
      short eval_output_type;
      if (i >= (uint64_t) BN_num_bits(output_mask_bn)
          || BN_is_bit_set(output_mask_bn, i) == 0) {
        CHECK(SendData(connfd, &garble_output_type, sizeof(short)));
        BN_clear_bit(output_bn, output_bit_offset + i);
      } else {
        CHECK(RecvData(connfd, &eval_output_type, sizeof(short)));
        if (eval_output_type != garble_output_type) {
          BN_set_bit(output_bn, output_bit_offset + i);
        } else {
          BN_clear_bit(output_bn, output_bit_offset + i);
        }
      }
    }
  }

  BN_free(output_mask_bn);
  return SUCCESS;
}

int EvaluateTransferOutputLowMem(const GarbledCircuit& garbled_circuit,
                                 block* output_labels, short* output_vals,
                                 uint64_t cid, OutputMode output_mode,
                                 const string& output_mask, BIGNUM* output_bn,
                                 int connfd) {
  BIGNUM* output_mask_bn = BN_new();
  BN_hex2bn(&output_mask_bn, output_mask.c_str());

  uint64_t output_bit_offset = 0;
  if (output_mode == OutputMode::consecutive) {  // normal mode, keep all the bits.
    output_bit_offset = cid * garbled_circuit.output_size;
  } else if (output_mode == OutputMode::separated_clock) {  // Separated by clock mode, keep all the bits.
    output_bit_offset = cid * garbled_circuit.output_size;
  } else if (output_mode == OutputMode::last_clock) {  // keep the last cycle, overwrite the bits.
    output_bit_offset = 0;
  }

  for (uint64_t i = 0; i < garbled_circuit.output_size; i++) {
    if (output_vals[i] == 0) {
      BN_clear_bit(output_bn, output_bit_offset + i);
    } else if (output_vals[i] == 1) {
      if (i >= (uint64_t) BN_num_bits(output_mask_bn)
          && BN_is_bit_set(output_mask_bn, i) == 0) {
        BN_set_bit(output_bn, output_bit_offset + i);
      }
    } else {
      short garble_output_type;
      short eval_output_type = get_LSB(output_labels[i]);
      if (i >= (uint64_t) BN_num_bits(output_mask_bn)
          || BN_is_bit_set(output_mask_bn, i) == 0) {
        CHECK(RecvData(connfd, &garble_output_type, sizeof(short)));
        if (eval_output_type != garble_output_type) {
          BN_set_bit(output_bn, output_bit_offset + i);
        } else {
          BN_clear_bit(output_bn, output_bit_offset + i);
        }
      } else {
        CHECK(SendData(connfd, &eval_output_type, sizeof(short)));
        BN_clear_bit(output_bn, output_bit_offset + i);
      }
    }
  }

  BN_free(output_mask_bn);
  return SUCCESS;
}
