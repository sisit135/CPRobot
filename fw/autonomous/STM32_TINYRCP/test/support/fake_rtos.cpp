// SPDX-License-Identifier: GPL-3.0-only

#include "fake_rtos.hpp"
#include <cassert>
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <vector>
namespace {
struct EventGroup {
  EventBits_t bits = 0;
};
struct Queue {
  unsigned capacity, size;
  std::deque<std::vector<unsigned char>> data;
};
struct Task {
  std::string name;
  TaskFunction_t function;
  void *arg;
};
std::vector<Task> tasks;
std::vector<std::unique_ptr<Queue>> queues;
std::vector<std::unique_ptr<EventGroup>> event_groups;
std::function<void()> on_delay;
struct Finished {};
} // namespace
namespace fake {
int64_t now = 1000000;
void run(const char *name, const std::function<void()> &callback) {
  on_delay = callback;
  for (auto task : tasks)
    if (task.name == name) {
      try {
        task.function(task.arg);
      } catch (const Finished &) {
        return;
      }
      assert(false);
    }
  assert(false);
}
void finish() { throw Finished{}; }
} // namespace fake
QueueHandle_t xQueueCreate(UBaseType_t n, UBaseType_t size) {
  queues.push_back(std::make_unique<Queue>(Queue{n, size, {}}));
  return queues.back().get();
}
BaseType_t xQueueSend(QueueHandle_t h, const void *value, TickType_t) {
  auto &q = *static_cast<Queue *>(h);
  if (q.data.size() >= q.capacity)
    return pdFALSE;
  const auto p = static_cast<const unsigned char *>(value);
  q.data.emplace_back(p, p + q.size);
  return pdTRUE;
}
BaseType_t xQueueOverwrite(QueueHandle_t h, const void *value) {
  auto &q = *static_cast<Queue *>(h);
  assert(q.capacity == 1);
  q.data.clear();
  return xQueueSend(h, value, 0);
}
BaseType_t xQueuePeek(QueueHandle_t h, void *value, TickType_t) {
  auto &q = *static_cast<Queue *>(h);
  if (q.data.empty())
    return pdFALSE;
  std::memcpy(value, q.data.front().data(), q.size);
  return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t h, void *value, TickType_t t) {
  if (!xQueuePeek(h, value, t))
    return pdFALSE;
  static_cast<Queue *>(h)->data.pop_front();
  return pdTRUE;
}
void vQueueDelete(QueueHandle_t) {}
BaseType_t xTaskCreate(TaskFunction_t f, const char *name, uint32_t, void *arg,
                       UBaseType_t, TaskHandle_t *out) {
  tasks.push_back({name, f, arg});
  *out = reinterpret_cast<void *>(tasks.size());
  return pdPASS;
}
TickType_t xTaskGetTickCount() {
  return static_cast<TickType_t>(fake::now / 1000);
}
void vTaskDelayUntil(TickType_t *, TickType_t) {
  assert(on_delay);
  on_delay();
}
void vTaskDelay(TickType_t) {
  assert(on_delay);
  on_delay();
}
void vTaskDelete(TaskHandle_t) { fake::finish(); }
void vTaskStartScheduler() {}
EventGroupHandle_t xEventGroupCreate() {
  event_groups.push_back(std::make_unique<EventGroup>());
  return event_groups.back().get();
}
EventBits_t xEventGroupWaitBits(EventGroupHandle_t handle, EventBits_t bits,
                                BaseType_t, BaseType_t, TickType_t) {
  return static_cast<EventGroup *>(handle)->bits & bits;
}
EventBits_t xEventGroupSetBits(EventGroupHandle_t handle, EventBits_t bits) {
  auto &group = *static_cast<EventGroup *>(handle);
  group.bits |= bits;
  return group.bits;
}
void vEventGroupDelete(EventGroupHandle_t) {}
