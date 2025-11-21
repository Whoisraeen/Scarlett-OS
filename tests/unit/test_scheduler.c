/**
 * @file test_scheduler.c
 * @brief Scheduler Unit Tests
 */

#include "../test_framework.h"

// Mock scheduler functions
extern void sched_init(void);
extern void* sched_create_task(void (*entry)(void));
extern void sched_destroy_task(void* task);
extern uint32_t sched_get_task_count(void);

TEST(scheduler_init) {
    sched_init();
    ASSERT_EQ(sched_get_task_count(), 0);
}

TEST(scheduler_create_task) {
    void dummy_task(void) {}
    
    void* task = sched_create_task(dummy_task);
    ASSERT_NOT_NULL(task);
    ASSERT_EQ(sched_get_task_count(), 1);
    
    sched_destroy_task(task);
    ASSERT_EQ(sched_get_task_count(), 0);
}

TEST(scheduler_multiple_tasks) {
    void dummy_task(void) {}
    
    void* tasks[5];
    for (int i = 0; i < 5; i++) {
        tasks[i] = sched_create_task(dummy_task);
        ASSERT_NOT_NULL(tasks[i]);
    }
    
    ASSERT_EQ(sched_get_task_count(), 5);
    
    for (int i = 0; i < 5; i++) {
        sched_destroy_task(tasks[i]);
    }
    
    ASSERT_EQ(sched_get_task_count(), 0);
}

int main(void) {
    test_init();
    
    printf("=== Scheduler Tests ===\n");
    RUN_TEST(scheduler_init);
    RUN_TEST(scheduler_create_task);
    RUN_TEST(scheduler_multiple_tasks);
    
    test_print_results();
    return test_get_exit_code();
}
