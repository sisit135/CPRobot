// SPDX-License-Identifier: GPL-3.0-only
// Linux cooperative fibers: real task bodies retain their stacks across delays.
// This models periodic scheduling and mailboxes, not MCU preemption or ISRs.
#include "scheduler.hpp"
#include "platform/rtos.hpp"
#include <algorithm>
#include <cstring>
#include <deque>
#include <memory>
#include <stdexcept>
#include <ucontext.h>
#include <vector>
namespace {
struct Task {
  ucontext_t context{};
  std::vector<char> stack = std::vector<char>(256 * 1024);
  std::string name;
  TaskFunction_t entry{};
  void *arg{};
  unsigned priority{};
  int64_t wake = 0;
  bool dead = false;
};
struct Queue {
  unsigned capacity, size;
  std::deque<std::vector<unsigned char>> data;
};
std::vector<std::unique_ptr<Task>> tasks;
std::vector<std::unique_ptr<Queue>> queues;
std::vector<std::unique_ptr<EventBits_t>> groups;
ucontext_t host;
Task *current = nullptr;
int64_t clock_us = 1000000;
void yield(int64_t wake) {
  if (!current)
    throw std::runtime_error("RTOS wait outside task");
  current->wake = wake;
  swapcontext(&current->context, &host);
}
void entry() {
  current->entry(current->arg);
  current->dead = true;
  yield(INT64_MAX);
}
} // namespace
namespace lifecycle {
int64_t now_us() { return clock_us; }
void advance_clock(int64_t dt) { clock_us += dt; }
void run_tasks() {
  unsigned budget = 10000;
  for (;;) {
    Task *ready = nullptr;
    for (auto &task : tasks)
      if (!task->dead && task->wake <= clock_us &&
          (!ready || task->priority > ready->priority))
        ready = task.get();
    if (!ready)
      break;
    if (!--budget)
      throw std::runtime_error("task failed to yield");
    current = ready;
    swapcontext(&host, &ready->context);
    current = nullptr;
  }
}
std::vector<std::string> task_states() {
  std::vector<std::string> out;
  for (const auto &t : tasks)
    out.push_back(t->name + (t->dead ? ": stopped" : ": waiting"));
  return out;
}
} // namespace lifecycle
QueueHandle_t xQueueCreate(UBaseType_t count, UBaseType_t size) {
  queues.push_back(std::make_unique<Queue>(Queue{count, size, {}}));
  return queues.back().get();
}
BaseType_t xQueueSend(QueueHandle_t h, const void *value, TickType_t) {
  auto &q = *static_cast<Queue *>(h);
  if (q.data.size() == q.capacity)
    return pdFALSE;
  auto p = static_cast<const unsigned char *>(value);
  q.data.emplace_back(p, p + q.size);
  return pdTRUE;
}
BaseType_t xQueueOverwrite(QueueHandle_t h, const void *value) {
  auto &q = *static_cast<Queue *>(h);
  if (q.capacity != 1)
    throw std::runtime_error("overwrite requires mailbox");
  q.data.clear();
  return xQueueSend(h, value, 0);
}
BaseType_t xQueuePeek(QueueHandle_t h, void *value, TickType_t timeout) {
  auto &q = *static_cast<Queue *>(h);
  const int64_t until = clock_us + int64_t(timeout) * 1000;
  while (q.data.empty()) {
    if (!timeout || clock_us >= until)
      return pdFALSE;
    yield(clock_us + 1000);
  }
  std::memcpy(value, q.data.front().data(), q.size);
  return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t h, void *value, TickType_t timeout) {
  if (!xQueuePeek(h, value, timeout))
    return pdFALSE;
  static_cast<Queue *>(h)->data.pop_front();
  return pdTRUE;
}
void vQueueDelete(QueueHandle_t) {}
BaseType_t xTaskCreate(TaskFunction_t f, const char *name, uint32_t, void *arg,
                       UBaseType_t priority, TaskHandle_t *out) {
  auto t = std::make_unique<Task>();
  t->entry = f;
  t->name = name;
  t->arg = arg;
  t->priority = priority;
  getcontext(&t->context);
  t->context.uc_stack.ss_sp = t->stack.data();
  t->context.uc_stack.ss_size = t->stack.size();
  t->context.uc_link = &host;
  makecontext(&t->context, entry, 0);
  if (out)
    *out = t.get();
  tasks.push_back(std::move(t));
  return pdPASS;
}
TickType_t xTaskGetTickCount() { return clock_us / 1000; }
void vTaskDelayUntil(TickType_t *wake, TickType_t period) {
  *wake += period;
  yield(std::max(clock_us + 1, int64_t(*wake) * 1000));
}
void vTaskDelay(TickType_t delay) {
  yield(clock_us + std::max(1u, delay) * 1000LL);
}
void vTaskDelete(TaskHandle_t h) {
  auto t = h ? static_cast<Task *>(h) : current;
  t->dead = true;
  if (t == current)
    yield(INT64_MAX);
}
void vTaskStartScheduler() { lifecycle::run_tasks(); }
EventGroupHandle_t xEventGroupCreate() {
  groups.push_back(std::make_unique<EventBits_t>(0));
  return groups.back().get();
}
EventBits_t xEventGroupSetBits(EventGroupHandle_t h, EventBits_t bits) {
  return *static_cast<EventBits_t *>(h) |= bits;
}
EventBits_t xEventGroupWaitBits(EventGroupHandle_t h, EventBits_t bits,
                                BaseType_t clear, BaseType_t all,
                                TickType_t timeout) {
  auto &value = *static_cast<EventBits_t *>(h);
  const int64_t until = clock_us + int64_t(timeout) * 1000;
  while (all ? (value & bits) != bits : !(value & bits)) {
    if (clock_us >= until)
      return value;
    yield(clock_us + 1000);
  }
  auto result = value;
  if (clear)
    value &= ~bits;
  return result;
}
void vEventGroupDelete(EventGroupHandle_t) {}
