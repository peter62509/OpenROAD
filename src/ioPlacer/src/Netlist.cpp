/////////////////////////////////////////////////////////////////////////////
//
// BSD 3-Clause License
//
// Copyright (c) 2019, University of California, San Diego.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////////

#include "Netlist.h"

#include "Slots.h"
#include "ioplacer/IOPlacer.h"

namespace ppl {

Netlist::Netlist()
{
  net_pointer_.push_back(0);
}

void Netlist::addIONet(const IOPin& io_pin,
                       const std::vector<InstancePin>& inst_pins)
{
  io_pins_.push_back(io_pin);
  inst_pins_.insert(inst_pins_.end(), inst_pins.begin(), inst_pins.end());
  net_pointer_.push_back(inst_pins_.size());
}

void Netlist::forEachIOPin(std::function<void(int idx, IOPin&)> func)
{
  for (int idx = 0; idx < io_pins_.size(); ++idx) {
    func(idx, io_pins_[idx]);
  }
}

void Netlist::forEachIOPin(
    std::function<void(int idx, const IOPin&)> func) const
{
  for (int idx = 0; idx < io_pins_.size(); ++idx) {
    func(idx, io_pins_[idx]);
  }
}

void Netlist::forEachSinkOfIO(int idx, std::function<void(InstancePin&)> func)
{
  int net_start = net_pointer_[idx];
  int net_end = net_pointer_[idx + 1];
  for (int idx = net_start; idx < net_end; ++idx) {
    func(inst_pins_[idx]);
  }
}

void Netlist::forEachSinkOfIO(
    int idx,
    std::function<void(const InstancePin&)> func) const
{
  int net_start = net_pointer_[idx];
  int net_end = net_pointer_[idx + 1];
  for (int idx = net_start; idx < net_end; ++idx) {
    func(inst_pins_[idx]);
  }
}

int Netlist::numSinksOfIO(int idx)
{
  int net_start = net_pointer_[idx];
  int net_end = net_pointer_[idx + 1];
  return net_end - net_start;
}

int Netlist::numIOPins()
{
  return io_pins_.size();
}

Rect Netlist::getBB(int idx, Point slot_pos)
{
  int net_start = net_pointer_[idx];
  int net_end = net_pointer_[idx + 1];

  int min_x = slot_pos.x();
  int min_y = slot_pos.y();
  int max_x = slot_pos.x();
  int max_y = slot_pos.y();

  for (int idx = net_start; idx < net_end; ++idx) {
    Point pos = inst_pins_[idx].getPos();
    min_x = std::min(min_x, pos.x());
    max_x = std::max(max_x, pos.x());
    min_y = std::min(min_y, pos.y());
    max_y = std::max(max_y, pos.y());
  }

  Point upper_bounds = Point(max_x, max_y);
  Point lower_bounds = Point(min_x, min_y);

  Rect net_b_box(lower_bounds, upper_bounds);
  return net_b_box;
}

int Netlist::computeIONetHPWL(int idx, Point slot_pos)
{
  int net_start = net_pointer_[idx];
  int net_end = net_pointer_[idx + 1];

  int min_x = slot_pos.x();
  int min_y = slot_pos.y();
  int max_x = slot_pos.x();
  int max_y = slot_pos.y();

  for (int idx = net_start; idx < net_end; ++idx) {
    Point pos = inst_pins_[idx].getPos();
    min_x = std::min(min_x, pos.x());
    max_x = std::max(max_x, pos.x());
    min_y = std::min(min_y, pos.y());
    max_y = std::max(max_y, pos.y());
  }

  int x = max_x - min_x;
  int y = max_y - min_y;

  return (x + y);
}

int Netlist::computeIONetHPWL(int idx,
                              Point slot_pos,
                              Edge edge,
                              std::vector<Constraint>& constraints)
{
  int hpwl;

  if (checkSlotForPin(io_pins_[idx], edge, slot_pos, constraints)) {
    hpwl = computeIONetHPWL(idx, slot_pos);
  } else {
    hpwl = std::numeric_limits<int>::max();
  }

  return hpwl;
}

int Netlist::computeDstIOtoPins(int idx, Point slot_pos)
{
  int net_start = net_pointer_[idx];
  int net_end = net_pointer_[idx + 1];

  int total_distance = 0;

  for (int idx = net_start; idx < net_end; ++idx) {
    Point pin_pos = inst_pins_[idx].getPos();
    total_distance += std::abs(pin_pos.x() - slot_pos.x())
                      + std::abs(pin_pos.y() - slot_pos.y());
  }

  return total_distance;
}

bool Netlist::checkSlotForPin(IOPin& pin,
                              Edge edge,
                              odb::Point& point,
                              std::vector<Constraint> constraints)
{
  bool valid_slot = true;

  for (Constraint constraint : constraints) {
    int pos
        = (edge == Edge::top || edge == Edge::bottom) ? point.x() : point.y();

    if (pin.getDirection() == constraint.direction) {
      valid_slot = checkInterval(constraint, edge, pos);
    } else if (pin.getName() == constraint.name) {
      valid_slot = checkInterval(constraint, edge, pos);
    }
  }

  return valid_slot;
}

bool Netlist::checkInterval(Constraint constraint, Edge edge, int pos)
{
  return (constraint.interval.getEdge() == edge
          && pos >= constraint.interval.getBegin()
          && pos <= constraint.interval.getEnd());
}

void Netlist::clear()
{
  inst_pins_.clear();
  net_pointer_.clear();
  io_pins_.clear();
}

}  // namespace ppl
