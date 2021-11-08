#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/module.h>

// DFS over a process tree
void dfs(struct task_struct *task) {
     struct task_struct *next;
     struct list_head *list;
	
	    printk(KERN_INFO "Name: %s, Process ID: %d\n", task->comm, task->pid);
	
	    list_for_each(list, &task->children) {
		        next = list_entry(list, struct task_struct, sibling);
		        dfs(next);
	    }
}

int dfs_traverse_init(void) {
	printk(KERN_INFO "Loading DFS Traverse Module...\n");
	dfs(&init_task);
	return 0;
}

void dfs_traverse_exit(void) {
	printk(KERN_INFO "Removing DFS Traverse Module...\n");
}

module_init(dfs_traverse_init);
module_exit(dfs_traverse_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Beyza-Kerem");
MODULE_DESCRIPTION("Process Tree DFS");
