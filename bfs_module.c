#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/stat.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Beyza-Kerem");
MODULE_DESCRIPTION("Process Tree BFS");

static int PID = 0;
module_param(PID, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(PID, "An integer");

// BFS over a process tree
void bfs(struct task_struct *task) {
     struct task_struct *next;
     struct list_head *list;
	
	    printk(KERN_INFO "Name: %s, Process ID: %d\n", task->comm, task->pid);

	   if (&task->sibling == NULL) {
		    printk(KERN_INFO "task has no sibling\n");
		    next = list_entry(&task->children, struct task_struct, sibling);
	 	    bfs(next);
	    } else {
		    printk(KERN_INFO "task has sibling\n");

	    	    list_for_each(list, &task->sibling) {
			    printk(KERN_INFO "inside loop\n");
			    next = list_entry(list, struct task_struct, sibling);
			    printk(KERN_INFO "%s\n", next->comm);
		            bfs(next);
	    	    }
	    }

}

int bfs_traverse_init(void) {
	printk(KERN_INFO "Loading BFS Traverse Module...\n");
	struct task_struct *first = get_pid_task(find_get_pid(PID), PIDTYPE_PID);
	bfs(first);
	return 0;
}

void bfs_traverse_exit(void) {
	printk(KERN_INFO "Removing BFS Traverse Module...\n");
}

module_init(bfs_traverse_init);
module_exit(bfs_traverse_exit);

