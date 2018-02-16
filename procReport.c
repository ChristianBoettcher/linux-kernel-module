#include<linux/module.h>
#include<linux/sched.h>
#include<asm/current.h>
#include<linux/pid.h>
#include<linux/slab.h>
#include<linux/proc_fs.h>
#include<asm/uaccess.h>

#define MAXSTR 255
#define PROCFS_NAME "proc_report"
#define PROCFS_MAX_SIZE		1024

//Patrick Sanchez/Christian Boettcher
//TCSS 422 A
//Assignment2


//-------struct----------
// a global struct for storing the values of a given line (for the report)
typedef struct line_info{
	char pid[MAXSTR];
	char name[MAXSTR];
	char chNum[MAXSTR];
	char chPid[MAXSTR];
	char fstCh[MAXSTR];
}Info;

struct file_operations proc_fops = {
write: procfile_write
};


//-------variables-------
// This structure hold information about the /proc file
static struct proc_dir_entry *Our_Proc_File;

//The buffer used to store character for this module
static char *procfs_buffer;

// The size of the buffer
static unsigned long procfs_buffer_size = 0


//------methods-------
//counts the processes and prints how many are running, unrunnable, or stopped
int process_count(void);

//prints all processes
void process_list(Info *lineStruct);

//counts and prints the number of children a task has, returns -1 if none
int process_children(struct task_struct *task, Info *lineStruct, int curr);

//writes info from *lineStruct to a procFile
void createProcFile(Info *lineStruct);


//-------procFile--------------------------------
// This function is called with the /proc file is written
int procfile_write(struct file *file, const char *buffer, unsigned long count, void *data){
	/* get buffer size */
	procfs_buffer_size = count;
	if (procfs_buffer_size > PROCFS_MAX_SIZE ) {
		procfs_buffer_size = PROCFS_MAX_SIZE;
	}
	
	/* write data to the buffer */
	if ( copy_from_user(procfs_buffer, buffer, procfs_buffer_size) ) {
		return -EFAULT;
	}
	return procfs_buffer_size;
}

// this method kicks off the procFile
void createProcFile(Info *lineStruct, int numProcess){
	
	procfs_buffer = kmalloc(sizeof(char) * MAXSTR * numProcess);
	
	for(int i = 0; i < numProcess; i++){
		if((lineStruct + i)->chNum == 0){
			ptr += sprintf(ptr, "ProcessID=%d Name=%s *No Children\n", (lineStruct + i)->pid, (lineStruct + i)->name);
		}else{
			ptr += sprintf(ptr, "ProcessID=%d Name=%s number_of_children=%d first_child_pid=%d first_child_name=%s\n", (lineStruct + i)->pid, (lineStruct + i)->name, (lineStruct + i)->chNum, (lineStruct + i)->chPid, (lineStruct + i)->fstCh);
		}
	}
		
	Our_Proc_File = create_proc_entry(PROCFS_NAME, 0, NULL, &proc_fops);
	if (Our_Proc_File == NULL) {
		remove_proc_entry(PROCFS_NAME, &proc_root);
		printk(KERN_ALERT "Error: Could not initialize /proc/%s\n",
			PROCFS_NAME);
		return -ENOMEM;
	}
	Our_Proc_File->write_proc = procfile_write;
	Our_Proc_File->owner = THIS_MODULE;
	
	return 0;
}
//------------------------------------------------------------


//proc_init acts as main for our kernel module
//get the parent pid with task->parent->pid
int proc_init (void) {
	int numProcess;
	Info *lineStruct;
  numProcess = process_count(); 
	lineStruct = kmalloc(sizeof(Info) * numProcess, GFP_KERNEL);
			// ---fyi--- (void * kmalloc(size_t size, int flags))
			// ---fyi--- GFP_KERNEL is used for code in process context inside the kernel
			// http://www.makelinux.net/books/lkd2/ch11lev1sec4
  process_list(lineStruct); 
	
	createProcFile(lineStruct,numProcess);

  return 0;
}

//counts the number of children that a task has, returns -1 if none
//*task = pointer to current task
//*lineStruct = pointer to the kmalloc-ed section of memory
//curr = the counter for current task
int process_children(struct task_struct *task, Info *lineStruct, int curr){
  
	//keeps track of the number of children
  int children, cpid;
  //pointer to the head of the child list
  struct list_head *list;
  char * child;
  //initialize
  children = 0;

  //child pid assigned
  cpid = list_first_entry(&task->children, struct task_struct, sibling)->pid;
  //name of the child process
  child = list_first_entry(&task->children, struct task_struct, sibling)->comm;
  

 //count the number of children
  list_for_each(list, &task->children){ 
	children++;
  }

  if(children != 0){
		
    printk(KERN_INFO "number_of_children=%d first_child_pid=%d first_child_name=%s\n", children, cpid, child);
		sprintf((lineStruct + curr)->chNum, "%d", children);
		sprintf((lineStruct + curr)->chPid, "%d", cpid);
		strncpy((lineStruct + curr)->fstCh, child, MAXSTR);
  }  
  else{
    printk("*No Children\n");
		sprintf((lineStruct + curr)->chNum, "%d", 0);
		sprintf((lineStruct + curr)->chPid, "%d", -1);
		strncpy((lineStruct + curr)->fstCh, "*No Children\n", MAXSTR);
  }

  if(children == 0){
    return -1;
  }
  else{
   return children;
  }
}


//prints the pid and name for each process
//*lineStruct = the pointer to the kmalloc-ed section of memory
void process_list(Info *lineStruct){
  
	//pointer to the current task
  struct task_struct *task;
	//helps to locate struct for current task	
	int curr = 0;
	
  //print each pid and process name
  for_each_process(task) {
    printk("ProcessID=%d Name=%s ", task->pid, task->comm);
		
		// with any luck, struct "*lineStruct" at loc "current" ...
		(lineStruct + curr)->pid = task->pid;
		strncpy((lineStruct + curr)->name, task->comm, MAXSTR);
		
		// and the fun continues    
		process_children(task,lineStruct,curr);
		
		curr++;
  } 
}

//this counts the runnable, unrunnable and stopped processes
//you can get the state with task->state /* -1 unrunnable, 0 runnable, >0 stopped */
//now returns an int
int process_count(){
  //used to house the current task
  struct task_struct *task;
  //counters for program types
  long un,run,stp,total;
  //initialize variables
  un = 0;
  run = 0;
  stp = 0;

	total = 0; //Added
	

  //count the number of eacy process type
  for_each_process(task) {      
		if(task->state == -1) {
			un++;
		      }
		else if(task->state == 0){
			run++;
		}
		else{
		  stp++;
		}
  }
	total = (un + run + stp); //Added

  printk(KERN_INFO "PROCESS REPORTER\n");
  printk(KERN_INFO "Unrunnable: %lu\n", un);
  printk(KERN_INFO "Runnable: %lu\n", run);
  printk(KERN_INFO "Stopped: %lu\n\n", stp);
  
	return total; //Added
}

void proc_cleanup(void) {
  printk(KERN_INFO "helloModule: performing cleanup of module\n");
}

MODULE_LICENSE("GPL");
module_init(proc_init);
module_exit(proc_cleanup);


