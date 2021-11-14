#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/module.h>

// BFS over a process tree
void bfs(struct task_struct *task) {
     struct task_struct *next;
     struct list_head *list;
	
	    printk(KERN_INFO "Name: %s, Process ID: %d\n", task->comm, task->pid);
	
	    list_for_each(list, &task->sibling) {
		        next = list_entry(list, struct task_struct, children);
		        bfs(next);
	    }
}

int bfs_traverse_init(void) {
	printk(KERN_INFO "Loading BFS Traverse Module...\n");
	bfs(&init_task);
	return 0;
}

void bfs_traverse_exit(void) {
	printk(KERN_INFO "Removing BFS Traverse Module...\n");
}

module_init(bfs_traverse_init);
module_exit(bfs_traverse_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Beyza-Kerem");
MODULE_DESCRIPTION("Process Tree BFS");

