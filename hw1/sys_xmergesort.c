#include <linux/linkage.h>
#include <linux/moduleloader.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <asm/unistd.h>
#include <asm/errno.h>
#include "xhw1.h"
#define SORT_ERROR -125
asmlinkage extern long (*sysptr)(void* input_struct);
int read_file(struct file* myfile, char* buf, int size);
void write_file(struct file* myfile, char* buf, int size);
int trim_buffer(char* buf, int size);
int compare(char* str1, char* str2, int flag);
int validate_arg (struct param* input);
asmlinkage long xmergesort(void* input_struct)
{
	struct file *filp1 = NULL, *filp2 = NULL, *filp_w = NULL, *filp_temp = NULL;
	struct param* input = NULL;
	int size = 4096, comp_val = 0 , len = 0, count = 0, comp_prev = -1, errno = 0, i = 0, cp_to_usr = 0;
	char* buf1 = NULL, *buf2 = NULL, *buf_op = NULL, *buf1_orj = NULL, *buf2_orj = NULL, *record_1 = NULL, *record_2 = NULL, *prev_record = NULL, *ip_file1, *ip_file2;
	char* new_line = "\n";
	if(input_struct == NULL){
		printk("ERROR! Input not valid, Please check \n");
		errno = EINVAL;
		return EINVAL;
	}
	else{
		
		input = (struct param*)kmalloc(sizeof(struct param),GFP_KERNEL);
		if(input == NULL){
			printk("Unable to allocate memory! \n");
			errno = -ENOMEM;
			goto out;
		}
		if(copy_from_user(input, input_struct, sizeof(struct param)))
		{
			errno = -EFAULT;
			kfree(input);
			return EFAULT; 
		}
		input->count = (unsigned int*)kmalloc(sizeof(unsigned int),GFP_KERNEL);
		if(input->count == NULL){
                        printk("Unable to allocate memory! \n");
                        errno = -ENOMEM;
                        goto out;
                }
		*(input->count) = 0;
	}
	if (input == NULL|| input->files == NULL || input->files[0] == NULL || input->files[1] == NULL || input->output_file == NULL){
		printk("ERROR! input parameters not proper, Please check \n");
		errno = -EINVAL;
		goto out;

	}
	i = validate_arg(input); // VALIDATES INPUT FILES FOR DIFFERENT FILE NAME AND EXISTANCE OF FILES 
	if(i != 0){
		printk("Problem encountered in file validation, identical files passes or file does not exist");
		errno = i;
		goto out;
	}
	i = 0;

	i = 1;
	while(input->files[i] != NULL){  //WHILE TOTAL NUMBER OF FILES TO BE SORTED ARE FINISHED
		count = 0;
		if(i == 1)
			ip_file1 = input->files[0];
		else{
			// SHUFFEL BETWEEN THE TEMPORARY OUTPUT FILES
			if(i%2 ==  0)
				ip_file1 = "temp_output_remove_hw1_f1.txt_f1";
			else
				ip_file1 = "temp_output_remove_hw1_f2.txt_f2";
		}

		ip_file2 = input->files[i];

		filp1 = filp_open(ip_file1,O_RDONLY,0); 
		filp2 = filp_open(ip_file2, O_RDONLY,0);
		if(i%2 !=  0)
			filp_temp = filp_open("temp_output_remove_hw1_f1.txt_f1", O_WRONLY|O_CREAT, 0644);
		else
			filp_temp = filp_open("temp_output_remove_hw1_f2.txt_f2", O_WRONLY|O_CREAT, 0644);

		if (!filp_temp || IS_ERR(filp_temp)) {
			printk("ERROR! Unable to create the output file %d\n", (int) PTR_ERR(filp_temp));
			errno = -EINVAL;
			filp_temp = NULL;
			goto out;

		}

		if(buf1 == NULL)
			buf1 = (char *)kmalloc(size,GFP_KERNEL);	
		if(buf1 == NULL){
                        printk("Unable to allocate memory! \n");
                        errno = -ENOMEM;
                        goto out;
                }
		buf1[size-1] = '\0';
		buf1[0] = '\0';
		if(buf2 == NULL)
			buf2 = (char *)kmalloc(size,GFP_KERNEL);
		if(buf2 == NULL){
                        printk("Unable to allocate memory! \n");
                        errno = -ENOMEM;
                        goto out;
                }
		buf2[size-1] = '\0';
		buf2[0] = '\0';
		if(buf_op == NULL)
			buf_op = (char *)kmalloc(size,GFP_KERNEL);
		if(buf_op == NULL){
                        printk("Unable to allocate memory! \n");
                        errno = -ENOMEM;
                        goto out;
                }
		buf_op[0] = '\0';
		buf1_orj = buf1;
		buf2_orj = buf2;
		read_file(filp1, buf1,size);
		read_file(filp2, buf2,size);
		record_1 =  strsep(&buf1, "\n");
		record_2 =  strsep(&buf2, "\n");

		printk("RECORD 1:--> %s, RECORD 2---> %s \n",record_1, record_2);

		while(record_1 != NULL && i_size_read(file_inode(filp1)) > 0&& record_2 != NULL &&  i_size_read(file_inode(filp2)) > 0 ){
			//WHILE THERE IS DATA IN EITHER OF THE TWO INPUT FILES
			comp_val = compare(record_1, record_2, input->flag);
			if(strlen(record_1) == 0)
				record_1 = new_line;
			if(strlen(record_2) == 0)
				record_2 = new_line;	

			if(comp_val < 0 ){
				//RECORD 1 IS SMALLER
				if(prev_record != NULL)
					comp_prev =  compare(prev_record, record_1, input->flag);
				if((input->flag & 0x10) != 0 && prev_record != NULL && comp_prev > 0){
					errno = SORT_ERROR;
					goto out;
				}
				if(prev_record == NULL ||  comp_prev < 0  || (comp_prev == 0 && (input->flag & 0x01) == 0)){
					// WE CAN WRITE IN OP BUFFER IN THIS CASE
					if(prev_record == NULL)
						prev_record = (char *)kmalloc(size,GFP_KERNEL);
					if(prev_record == NULL){
                 			 	printk("Unable to allocate memory! \n");
                        			errno = -ENOMEM;
                        			goto out;
                			 }
					
					//else
					//	printk("OUTPUT Record_1 LOOP 1-----> prev_record = %s       record_1 = %s     \n",prev_record, record_1);
					strcpy(prev_record, record_1);
					//printk("OUTPUT Record_1 LOOP 1-----> prev_record = %s       record_1 = %s     \n",prev_record, record_1);
					if(strlen(record_1) + 1 <= size - len){
						strcat(buf_op, record_1);
						if(record_1 != new_line){
							strcat(buf_op, "\n");
							len += strlen(record_1) + 1;
						}
						else
							len += strlen(record_1);
						count++; 
					}
					else{
						write_file(filp_temp, buf_op, len);
						len = 0;
						buf_op[len] = '\0';
						strcat(buf_op, record_1);
						if(record_1 != new_line){
							strcat(buf_op, "\n");
							len += strlen(record_1) + 1;
						}
						else
							len += strlen(record_1);
						count++;
					}
				}
				//IREAD THE NEXT VALUE OF BUFFER
				if(buf1 != NULL && strlen(buf1) > 0)
					record_1 =  strsep(&buf1, "\n");
				else{
					buf1 = buf1_orj;
					if(read_file(filp1, buf1,size) == 0){
						record_1 = NULL;
						break;
					}
					record_1 = strsep(&buf1, "\n");
				}
			}
			else if(comp_val > 0){
				//RECORD 2 IS SMALLER 
				if(prev_record != NULL)
					comp_prev =  compare(prev_record, record_2, input->flag);
				if((input->flag & 0x10) != 0 && prev_record != NULL && comp_prev > 0){
					errno = SORT_ERROR;
					goto out;
				}
				if(prev_record == NULL ||  comp_prev < 0 || (comp_prev == 0 && (input->flag & 0x01) == 0)){
					//CHECK FOR THE FLAGS AND NOW RECORD CAN BE INSERTED INTO BUFFER
					if(prev_record == NULL)
						prev_record = (char *)kmalloc(size,GFP_KERNEL);
					if(prev_record == NULL){
                                                printk("Unable to allocate memory! \n");
                                                errno = -ENOMEM;
                                                goto out;
                                         }
					//else
					//	printk("OUTPUT Record_2:  prev_record = %s     record_2 =  %s     \n",prev_record, record_2);
					strcpy(prev_record, record_2);
					if(strlen(record_2) + 1 <= size - len){
						//OUTPUT BUFFER IS FREE APPEND TO BUFFER
						strcat(buf_op, record_2);
						if(record_2 != new_line){
							strcat(buf_op, "\n");
							len += strlen(record_2) + 1;
						}
						else
							len += strlen(record_2);

						count++;
					}
					else{
						//OUTPUT BUFFER FULL WRITE TO FILE
						write_file(filp_temp, buf_op, len);
						len = 0;
						buf_op[len] = '\0';
						strcat(buf_op, record_2);
						if(record_2 != new_line){
							strcat(buf_op, "\n");
							len += strlen(record_2) + 1;
						}
						else
							len += strlen(record_2);

						count++;
					}			
				}
				// PROCESS NEXT ITEM IN BUFFER 2
				if(buf2 != NULL && strlen(buf2) > 0){
					record_2 =  strsep(&buf2, "\n");
				}
				else{
					buf2 = buf2_orj;
					if(read_file(filp2, buf2, size) == 0){
						record_2 = NULL;
						printk("EOF FILE 2: prev_record: ---> %s   record_2: --->  %s\n", prev_record, record_2);
						break;
					}
					record_2 = strsep(&buf2, "\n");
				}
			}
			else{
				// BOTH RECORDS ARE SAME 
				if(prev_record != NULL)
					comp_prev =  compare(prev_record, record_2, input->flag);
				if((input->flag & 0x10) != 0 && prev_record != NULL && comp_prev > 0){
					errno = SORT_ERROR;
					goto out;
				}


				if(prev_record == NULL ||  comp_prev < 0  || (comp_prev == 0 && (input->flag & 0x01) == 0)){
					// CHECK FOR FLAGS AND NOW THE RECORDS CAN BE WRITTEN
					if(prev_record == NULL)
						prev_record = (char *)kmalloc(size,GFP_KERNEL);
					if(prev_record == NULL){
                                                printk("Unable to allocate memory! \n");
                                                errno = -ENOMEM;
                                                goto out;
                                         }
					strcpy(prev_record, record_1);
					printk("BOTH VALUES EQUAL prev-record: --->  %s record_1: --->  %s  record_2: ---> %s  \n",prev_record, record_1, record_2);
					if(strlen(record_1) + 1 <= size - len){
						strcat(buf_op, record_1);
						if(record_1 != new_line){
							strcat(buf_op, "\n");
							len += strlen(record_1) + 1;
						}
						else
							len += strlen(record_1);

						count++;
					}
					else{
						write_file(filp_temp, buf_op, len);
						len = 0;
						buf_op[len] = '\0';
						strcat(buf_op, record_1);
						if(record_1 != new_line){
							strcat(buf_op, "\n");
							len += strlen(record_1) + 1;
						}
						else
							len += strlen(record_1);

						count++;
					}
					if((input->flag & 0x01) == 0){
						printk("BOTH VALUES EQUAL -U NOT SET prev-record: --->  %s record_1: --->  %s  record_2: ---> %s \n",prev_record, record_1 ,record_2 );
						if(strlen(record_2) + 1 <= size - len){
							strcat(buf_op, record_2);
							if(record_2 != new_line){
								strcat(buf_op, "\n");
								len += strlen(record_2) + 1;
							}
							else
								len += strlen(record_2);

							count++;
						}
						else{
							write_file(filp_temp, buf_op, len);
							len = 0;
							buf_op[len] = '\0';
							strcat(buf_op, record_2);
							if(record_2 != new_line){
								strcat(buf_op, "\n");
								len += strlen(record_1) + 1;
							}
							else
								len += strlen(record_1);

							count++;
						}
					}
				}

				// PROCESS NEXT OF BOTH BUFFERS
				if(buf1 != NULL && strlen(buf1) > 0)
					record_1 =  strsep(&buf1, "\n");
				else{
					buf1 = buf1_orj;
					if(read_file(filp1, buf1,size) == 0)
						record_1 = NULL;
					else
						record_1 = strsep(&buf1, "\n");
				}
				if(buf2 != NULL && strlen(buf2) > 0)
					record_2 =  strsep(&buf2, "\n");
				else{
					buf2 = buf2_orj;
					if(read_file(filp2, buf2,size) == 0)
						record_2 = NULL;
					else
						record_2 = strsep(&buf2, "\n");

				}
			}
		}
		printk("END OF EITHER OF THE FILE:  P: --->  %s R_1: ---> %s  R2: ---> %s\n",prev_record, record_1, record_2);
		while(record_1 != NULL &&  i_size_read(file_inode(filp1)) > 0){
			//PROCESS FILE 1
			comp_prev = -1;
			if(strlen(record_1) == 0){
				record_1 = new_line;
			}

			if(prev_record != NULL)
				comp_prev =  compare(prev_record, record_1, input->flag);
			//printk("P: ---------------> %s R_1: -------------> %s  buff 1 ------> %s\n", prev_record, record_1, buf1);
			if((input->flag & 0x10) != 0 && prev_record != NULL  && comp_prev > 0){
				errno = SORT_ERROR;
				goto out;
			}
			if(prev_record == NULL)
				prev_record = (char *)kmalloc(size,GFP_KERNEL);
			if(prev_record == NULL){
                                printk("Unable to allocate memory! \n");
                                errno = -ENOMEM;
                        	goto out;
                        }
			if(comp_prev < 0 || (comp_prev == 0 && (input->flag & 0x01) == 0)){
				//printk("OUTPUT RECORD 1: P: --->  %s  R_1: ---> %s \n",prev_record, record_1);
				if(prev_record != NULL)
					strcpy(prev_record,record_1);
				if(strlen(record_1) + 1 <= size - len){
					strcat(buf_op, record_1);
					if(record_1 != new_line){
						strcat(buf_op, "\n");
						len += strlen(record_1) + 1;
					}
					else
						len += strlen(record_1);

					count++;
				}
				else{
					write_file(filp_temp, buf_op, len);
					len = 0;
					buf_op[len] = '\0';
					strcat(buf_op, record_1);
					if(record_1 != new_line){
						strcat(buf_op, "\n");
						len += strlen(record_1) + 1;
					}
					else
						len += strlen(record_1);

					count++;

				}
			}
			if(buf1 != NULL && strlen(buf1) > 0)
				record_1 =  strsep(&buf1, "\n");
			else{
				buf1 = buf1_orj;
				if(read_file(filp1, buf1,size) == 0){
					record_1 = NULL;
					break;
				}
				record_1 = strsep(&buf1, "\n");

			}
			//t_p++;
			//printk("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAa R1 ---> %s P1 ----> %s \n", record_1, prev_record);
			//if(t_p > 15)
			//	goto out;
			//return 0;	
		}
		printk("END OF EITHER OF FILES PROCESSING FILE 2 ");

		while(record_2 != NULL &&  i_size_read(file_inode(filp2)) > 0){
			//PROCESS RECORDS OF FILE 2
			comp_prev = -1;
			if(strlen(record_2) == 0)
				record_2 = new_line;
			if(prev_record != NULL)
				comp_prev =  compare(prev_record, record_2, input->flag);
			if((input->flag & 0x10) != 0 && prev_record != NULL && comp_prev > 0){
				errno = SORT_ERROR;
				goto out;
			}
			printk("P: ---> %s R_2: ---> %s \n", prev_record, record_2);
			//printk("EXIT LOOP ------------------------------------------> LOOP 2 %s      %s %d\n",prev_record, record_2, comp_prev);
			if(prev_record == NULL)
				prev_record = (char *)kmalloc(size,GFP_KERNEL);
			if(prev_record == NULL){
                                                printk("Unable to allocate memory! \n");
                                                errno = -ENOMEM;
                                                goto out;
                                         }
			if(comp_prev < 0 || (comp_prev == 0 && (input->flag & 0x01) == 0)){
				//printk("OUTPUT RECORD 2: P: --->  %s  R_2: ---> %s \n",prev_record, record_2);

				if(prev_record != NULL)
					strcpy(prev_record,record_2);

				if(strlen(record_2) + 1 <= size - len){
					strcat(buf_op, record_2);
					if(record_2 != new_line){
						strcat(buf_op, "\n");
						len += strlen(record_2) + 1;
					}
					else
						len += strlen(record_2);

					count++;
				}
				else{
					write_file(filp_temp, buf_op, len);
					len = 0;
					buf_op[len] = '\0';
					strcat(buf_op, record_2);
					if(record_2 != new_line){
						strcat(buf_op, "\n");
						len += strlen(record_2) + 1;
					}
					else
						len += strlen(record_2);

					count++;
				}
			}
			if(buf2 != NULL && strlen(buf2) > 0)
				record_2 =  strsep(&buf2, "\n");
			else{
				buf2 = buf2_orj;
				if(read_file(filp2, buf2, size) == 0){
					record_2 = NULL;
					break;
				}
				record_2 = strsep(&buf2, "\n");
			}
		}
		if(buf_op != NULL && len > 0)
			write_file(filp_temp, buf_op, len);

		if(prev_record == NULL)
			kfree(prev_record);
		prev_record = NULL;
		i++;
		buf1 = buf1_orj;
		buf2 = buf2_orj;
	}
	//SUCCESSFUL EXECURION RENAME FILE
	filp_w = filp_open(input->output_file, O_WRONLY|O_CREAT, filp2->f_path.dentry->d_inode->i_mode);
        if (!filp_w || IS_ERR(filp_w)) {
                printk("ERROR! Unable to create a new output file. %d\n", (int) PTR_ERR(filp_w));
                errno = PTR_ERR(filp_w);
                filp_w = NULL;
                goto out;
        }
 
	vfs_rename(d_inode(file_dentry(filp_temp)->d_parent),file_dentry(filp_temp), d_inode(file_dentry(filp_w)->d_parent),file_dentry(filp_w), NULL, 0);
	vfs_unlink(d_inode(file_dentry(filp_w)->d_parent), file_dentry(filp_w), NULL);
	if(i > 2){
		vfs_unlink(d_inode(file_dentry(filp1)->d_parent), file_dentry(filp1), NULL);
	}
	printk("RENAME TEMP FILE TO THE OUTPUT_FILE\n");

	printk("NUMBER OF RECORDS READ ARE-----------------------------------------------------------------> %d", count);
	*(input->count) = count;
	cp_to_usr = copy_to_user(((struct param*)input_struct)->count, input->count, sizeof(unsigned int));
	if(cp_to_usr < 0){
		printk("Unable to copy from kernel space to user space \n");
                errno = -EFAULT;
                goto out;
	}
	
out:
	if(errno == SORT_ERROR){
		/* DELETE THE TEMPORARY FILE IF FLAG TSET AND ENCOUNTERD UNSORTED ELEMENTS IN EITHER FILE */
		/*if(i_size_read(file_inode(filp_w)) <= 0){
			vfs_unlink(d_inode(file_dentry(filp_w)->d_parent), file_dentry(filp_w), NULL);
		}*/
		//vfs_unlink(d_inode(file_dentry(filp_w)->d_parent), file_dentry(filp_w), NULL);
		vfs_unlink(d_inode(file_dentry(filp_temp)->d_parent), file_dentry(filp_temp), NULL);
		printk("ERROR! Given file is not sorted, Please check");
		if(i >= 2 ){
			vfs_unlink(d_inode(file_dentry(filp1)->d_parent), file_dentry(filp1), NULL);
		}

	}

	//printk("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	if(errno != 0 && i >= 2 && filp1 != NULL){
		vfs_unlink(d_inode(file_dentry(filp1)->d_parent), file_dentry(filp1), NULL);
	}

	//printk("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");

	buf1 = buf1_orj;
	if(buf1 != NULL)
		kfree(buf1);
	buf2 = buf2_orj;
	if(buf2 != NULL)
		kfree(buf2);
	if(buf_op != NULL)
		kfree(buf_op);
	if(prev_record != NULL)
		kfree(prev_record);
	if(input->count != NULL)
		kfree(input->count);
	if(input!= NULL)
		kfree(input);
	/*	if(prev_record_2 != NULL)
		kfree(prev_record_2);*/
	if(filp1 != NULL )
		filp_close(filp1,NULL);
	if(filp2 != NULL )
		filp_close(filp2,NULL);
	if(filp_w != NULL)
		filp_close(filp_w,NULL);
	if(filp_temp != NULL)
		filp_close(filp_temp,NULL);
	return errno;


}

int compare(char* str1, char* str2, int flag){
	//printk("FLAG-----------------------------------------------------------> %d \n",flag);
	if((flag & 0x04) != 0)
		return strcasecmp(str1, str2);
	return strcmp(str1, str2);

}
int read_file(struct file* myfile, char* buf, int size){
	int i = 0, x = 0;
	mm_segment_t oldfs;
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	x =  vfs_read(myfile, buf, size, &myfile->f_pos);
	set_fs(oldfs);
	if(x > 0){
		//i = strlen(buf) -1;
		/*	while( i >= 0 && buf[i] != '\n' && buf[i] != '\0'){
			i--;
			}
			if(buf[i] != '\0')
			buf[++i] =  '\0';*/
		for(i = x-1; i >= 0 && buf[i] != '\n' && buf[i] != '\0'; i--){
			//printk("PADDING \n");
			//buf[i] = '\0';
			myfile->f_pos -= 1;
		}
		if(i >= 0)
			buf[i] = '\0';
		else
			buf[0] = '\0';

	}
	//printk("VFS READ RETURNS -------------------> %d--- len -> %zu    %s \n",x, strlen(buf), buf);

	//	if(i >=0 && buf[i] == '\0')
	//		buf[i] = '\n';
	return x;

}
/*
   void rename_file(struct file *file_temp, struct file *file_w)
   {
//struct inode *parent_inode = d_inode(file_dentry(output_file));
int ret=vfs_rename(d_inode(file_dentry(temp_file)->d_parent),file_dentry(temp_file), d_inode(file_dentry(output_file)->d_parent),file_dentry(output_file), NULL, 0);
vfs_unlink(d_inode(file_dentry(output_file)->d_parent), file_dentry(output_file), NULL);
}*/
int validate_arg (struct param* input){
	int i = 0, j = 0, errno = 0;
	struct filename *getname1 = NULL, *getname2 = NULL, *getname_w = NULL;
	struct file *filp1 = NULL, *filp2 = NULL, *filp_w = NULL;
	char *ip_file1 = NULL, *ip_file2 = NULL;
	struct inode *inode_f1 = NULL, *inode_f2 = NULL, *inode_f_w = NULL;
        umode_t f_mode1,f_mode2,f_mode_w;
	getname_w = getname(input->output_file);
        if(getname_w == NULL){
        	printk("ERROR! Incorrect path for Output file \n");
                errno = -EBADF;
                goto out_arg;
        }                                   
                        
	filp_w = filp_open(input->output_file, O_WRONLY|O_CREAT, 0644);
        if (!filp_w || IS_ERR(filp_w)) {
                printk("ERROR! Unable to create a new output file. \n" );
                errno = -EINVAL;
                filp_w = NULL;
                goto out_arg;
        }
	inode_f_w = file_inode(filp_w);
	f_mode_w = inode_f_w->i_mode;
	for(i = 0;  input->files[i] != NULL && input->files[i+1] != NULL; i++){
		ip_file1 = input->files[i];	
		getname1 = getname(ip_file1);
		if(getname1 == NULL){
			printk("ERROR! Incorrect path for Input file %d \n", i);
			errno = -EBADF;
			goto out_arg;
		}
		filp1 = filp_open(ip_file1,O_RDONLY,0);
		if (!filp1 || IS_ERR(filp1)) {
			printk("ERROR! Unable to open file  %d\n", i);
			errno = -ENOENT;
			filp1 = NULL;
			goto out_arg;
		}
		inode_f1 = file_inode(filp1);
		f_mode1 = inode_f1->i_mode;
		if(!S_ISREG(f_mode1)){
                        printk("ERROR! Some problem \n");
                        errno = -EINVAL;
                        goto out_arg;

                }
		for(j = i+1; input->files[j] != NULL; j++){
			ip_file2 = input->files[j];
			getname2 = getname(ip_file2);
			if(getname2 == NULL){
				printk("ERROR! Incorrect path for Input file %d \n", j);
				errno = -EBADF;
				goto out_arg;
				}
			filp2 = filp_open(ip_file2, O_RDONLY,0);
			if (!filp2 || IS_ERR(filp2)) {
				printk("ERROR! Unable to open file %d \n", j);
				errno = -ENOENT;
				filp2 = NULL;
				goto out_arg;
			}
	                inode_f2 = file_inode(filp2);
        	        if(inode_f1->i_ino == inode_f2->i_ino || inode_f2->i_ino == inode_f_w->i_ino || inode_f1->i_ino == inode_f_w->i_ino){
				if(inode_f1->i_sb->s_dev == inode_f2->i_sb->s_dev || inode_f1->i_sb->s_dev == inode_f_w->i_sb->s_dev || inode_f2->i_sb->s_dev == inode_f_w->i_sb->s_dev){
                                printk("ERROR! Identical files passed. Please check \n");
                                errno = -EINVAL;
				goto out_arg;
                        	}
			}
                	f_mode2 = inode_f2->i_mode;
                	if(!S_ISREG(f_mode2)){
                        	printk("ERROR! Some problem \n");
                        	errno = -EINVAL;
                        	goto out_arg;

                	}
			if(filp2 != NULL)
                        	filp_close(filp2,NULL);
			if(getname2 != NULL)
                        	putname(getname2);
		}
		if(filp1 != NULL)
			filp_close(filp1,NULL);
		if(getname1 != NULL)
                        putname(getname1);
               
	}
	out_arg:
		if(i_size_read(file_inode(filp_w)) <= 0){
                        vfs_unlink(d_inode(file_dentry(filp_w)->d_parent), file_dentry(filp_w), NULL);
                }
		if(getname1 != NULL)
			putname(getname1);
		if(getname2 != NULL)
			putname(getname2);
		 if(getname_w != NULL)
                        putname(getname_w);
		if(filp1 != NULL)
                        filp_close(filp1,NULL);
                if(filp2 != NULL)
                        filp_close(filp2,NULL);
		if(filp_w != NULL)
	                filp_close(filp_w,NULL);
	return errno;	
}

void write_file(struct file* myfile, char* buf, int size){
	int len = strlen(buf);
	mm_segment_t oldfs;
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	vfs_write(myfile, buf, len, &myfile->f_pos);
	set_fs(oldfs);
}
static int __init init_sys_xmergesort(void)
{
	printk("installed new sys_xmergesort module\n");
	if (sysptr == NULL)
		sysptr = xmergesort;
	return 0;
}
static void  __exit exit_sys_xmergesort(void)
{
	if (sysptr != NULL)
		sysptr = NULL;
	printk("removed sys_xmergesort module\n");
}
module_init(init_sys_xmergesort);
module_exit(exit_sys_xmergesort);
MODULE_LICENSE("GPL");
