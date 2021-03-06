#include "order.h"
#include "unit.h"
#include "offsets.h"

DummyListHead<Order, Order::offset_of_allocated> first_allocated_order;

Order::Order()
{
    allocated.Add(first_allocated_order);
}

Order *Order::Allocate(int order_id, uint32_t position_xy, Unit *target, int fow_unit_id)
{
    Order *order;
    order = new Order;
    order->list.prev = nullptr;
    order->list.next = nullptr;
    order->order_id = order_id;
    order->position = Point(position_xy & 0xffff, position_xy >> 16);
    order->target = target;
    order->fow_unit = fow_unit_id;

    return order;
}

void Order::SingleDelete()
{
    allocated.Remove();
    delete this;
}

void Order::DeleteAll()
{
    auto it = first_allocated_order.begin();
    while (it != first_allocated_order.end())
    {
        Order *order = *it;
        ++it;
        delete order;
    }
    first_allocated_order.Reset();
}

bool IsTargetableOrder(int order)
{
    switch (order)
    {
        case Order::Die: case Order::BeingInfested: case Order::SpiderMine: case Order::DroneStartBuild: case Order::InfestMine4: case Order::BuildTerran:
        case Order::BuildProtoss1: case Order::BuildProtoss2: case Order::ConstructingBuilding: case Order::PlaceAddon: case Order::BuildNydusExit:
        case Order::Land: case Order::LiftOff: case Order::DroneLiftOff: case Order::HarvestObscured: case Order::MoveToGas:
        case Order::WaitForGas: case Order::HarvestGas: case Order::MoveToMinerals: case Order::WaitForMinerals: case Order::MiningMinerals:
        case Order::Harvest3: case Order::StopHarvest: case Order::CtfCop2:
            return false;
        default:
            return true;
    }
}

void Unit::PrependOrder(int order, const Unit *target, const Point &pos)
{
    if (order_queue_begin)
    {
        InsertOrder(this, order, target, pos.AsDword(), None, order_queue_begin.AsRawPointer());
    }
    else
    {
        AddOrder(this, order, nullptr, None, pos.AsDword(), target);
    }
}
