#include <linux/linkage.h>
#include <linux/moduleloader.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/string.h>
#include <linux/namei.h>

#include "xmergesort.h"


struct xmerge_args *kargs;
struct file *inputfile1;
struct file *inputfile2;
struct file *outputfile;
struct file *tempfile;

char *outputbufptr;
char *previousstringptr;
char *inputbuffer1;
char *inputbuffer2;
char *tempbuffer;
char *outputbuffer;

int outputfilelen;
int no_of_records;


asmlinkage extern long (*sysptr)(void *arg);

	/* read file will read both input files
	* PAGE_SIZE and write strings to buffer
	*/

	int readfile(struct file *file, char *buf,
					int len, unsigned long long offset)
	{
		mm_segment_t oldfs;
		int bytes;

		oldfs = get_fs();
		set_fs(KERNEL_DS);
		memset(buf, '\0', PAGE_SIZE);
		bytes = vfs_read(file, buf, len, &offset);
		set_fs(oldfs);
		return bytes;
	}

	/* write file will write buffer to temporary file */

	int write_file(unsigned long long offset,
					 unsigned char *data, unsigned int size)
	{
		mm_segment_t oldfs;
		int ret;

		oldfs = get_fs();
		set_fs(KERNEL_DS);
		ret = vfs_write(tempfile, data, size, &offset);
		printk(" writtend bytes to write file %d\n", ret);
		set_fs(oldfs);
		return ret;
	}

	/* It will used for case insensitive flag and convert
	*  all lower case characyers to upper case
	*/

	char touppercase(char ch)
	{
		if ((int)ch >= 97 && (int)ch <= 122)
			ch = (int) ch-32;

		return ch;
	}

	/* copytooutbuffer will copy strings to outputbuffer */

	int copytooutbuffer(char *copybuf)
	{
		int len = 0;
		while (*copybuf != '\n') {
			*outputbufptr++ = *copybuf++;
			len++;
		}
			*outputbufptr++ = '\n';
			return len+1;
	}

	/* copytotemp buffer will copy strings to temp buffer */

	int copytotempbuffer(char *copybuf)
	{
		int len = 0;
		while (*copybuf != '\n') {
			*previousstringptr++ = *copybuf++;
			len++;
		}
			*previousstringptr = '\n';
		return len;
	}

	/* comparetwostring will compare two strings character by
	*  character and return result .
	*/

	int comparetwostring(char *input1ptr, char *input2ptr)
	{
		char char1 = ' ';
		char char2 = ' ';
		int shortstringlen = 0;

	/*Compare string character wise and terminate when find new line or EOF */

		while (*input1ptr != '\n' && *input2ptr != '\n'
				 && *input1ptr != '\0' && *input2ptr != '\0') {
			if (kargs->flags & 0x04) {
				char1 = touppercase(*input1ptr);
				char2 = touppercase(*input2ptr);
			} else {
				char1 = *input1ptr;
				char2 = *input2ptr;
			}
			if (char1 == char2) {
				input1ptr++;
				input2ptr++;
				shortstringlen++;
			} else
				break;
		}

		if (char1 > char2)
			return -1;
		else if (char1 < char2)
			return 1;
		else
			return 0;
	}

	/* This function will be used to merge sort single file, when one of the input file get emptied and one remains */

	int mergesinglefile(char *prevptr, char *curptr,
						 int inputlen, int inputbyte,
							 int outbuflen, int inputfilelen, bool forinp1)
	{
		char *buf = curptr;
		char char1 =  ' ';
		char char2 = ' ';
		int shortstringlen = 0;
		int inputread;
		int prevlen = 0;
		int err = 0;

	/* checking chracters from last inputted character from buffer */

		while (inputlen <= inputbyte && *curptr != '\0') {
			shortstringlen = 0;
			inputread = inputlen;
			while (*previousstringptr != '\n' && *curptr != '\n') {
				if (kargs->flags & 0x04) {
					char1 = touppercase(*prevptr);
					char2 = touppercase(*curptr);
				} else {
					char1 = *previousstringptr;
					char2 = *curptr;
				}
				if (char1 == char2) {
					previousstringptr++;
					curptr++;
					shortstringlen++;
					inputread++;
				} else
					break;
			}

		/* checking characters and outputitng results */

		if (char1 == char2 &&
				 *previousstringptr == '\n' && *curptr == '\n') {
			shortstringlen += 1;


			/* Error check for ua flag */

			if ((kargs->flags & 0x01) && (kargs->flags & 0x02)) {
				err = -EPERM;
				goto out;
			}


			/* Error check for u flag */

			if (kargs->flags & 0x01) {
				previousstringptr = tempbuffer;
				curptr++;
				inputlen += shortstringlen;
				buf = curptr;
			}

			/* Error check for -a flag */

			if (kargs->flags & 0x02) {
				if ((outbuflen - shortstringlen) < 0) {
					outputfilelen = outputfilelen +
								 write_file(outputfilelen, outputbuffer, PAGE_SIZE-outbuflen);
					outbuflen = PAGE_SIZE;
					outputbufptr = outputbuffer;
				}
				previousstringptr = tempbuffer;
				outbuflen = outbuflen - copytooutbuffer(buf);
				no_of_records += 1;
				curptr++;
				buf = curptr;
				inputlen += shortstringlen;
			}
		} else if (char1 > char2 || (*curptr == '\n' && *previousstringptr != '\n')) {
			while (*curptr != '\n' && *curptr != '\0' && inputread < PAGE_SIZE) {
				curptr++;
				shortstringlen++;
				inputread++;
			}
			if (inputread == PAGE_SIZE) {
				shortstringlen = 0;
				if (forinp1) {
					inputbyte = readfile(inputfile1, inputbuffer1, PAGE_SIZE, inputfilelen);
					curptr = inputbuffer1;
					buf = inputbuffer1;
				} else {
					inputbyte = readfile(inputfile2, inputbuffer2, PAGE_SIZE, inputfilelen);
					curptr = inputbuffer2;
					buf = inputbuffer2;
				}
					inputlen = 0;
				while (*curptr != '\n' &&  *curptr != '\0') {
					curptr++;
					shortstringlen++;
				}
			}

			/* Checks for t flag */

			shortstringlen += 1;
			if (kargs->flags & 0x10) {
				err = -EINVAL;
				goto out;
			} else {
				curptr++;
				inputlen += shortstringlen;
				inputfilelen += shortstringlen;
				buf = curptr;
			}
			} else {
				while (*curptr != '\n' &&  *curptr != '\0' && inputread < PAGE_SIZE) {
					curptr++;
					shortstringlen++;
					inputread++;
				}
				if (inputread == PAGE_SIZE) {
					shortstringlen = 0;
					if (forinp1) {
						inputbyte = readfile(inputfile1, inputbuffer1, PAGE_SIZE, inputfilelen);
						curptr = inputbuffer1;
						buf = inputbuffer1;
					} else {
						inputbyte = readfile(inputfile2, inputbuffer2, PAGE_SIZE, inputfilelen);
						curptr = inputbuffer2;
						buf = inputbuffer2;
					}
						inputlen = 0;
					while (*curptr != '\n' &&  *curptr != '\0') {
						curptr++;
						shortstringlen++;
					}
				}

				shortstringlen += 1;
			if ((outbuflen - shortstringlen) < 0) {
				outputfilelen = outputfilelen + write_file(outputfilelen, outputbuffer, PAGE_SIZE-outbuflen);
				outbuflen = PAGE_SIZE;
				outputbufptr = outputbuffer;
			}
				previousstringptr = tempbuffer;
				outbuflen = outbuflen - copytooutbuffer(buf);
				prevlen = copytotempbuffer(buf);
				no_of_records += 1;
				curptr++;
				buf = curptr;
				inputlen += shortstringlen;
				inputfilelen += shortstringlen;
				previousstringptr = tempbuffer;
		}

	}
			/* Check if output buffer is to write to write file */

			write_file(outputfilelen, outputbuffer, PAGE_SIZE-outbuflen);
			goto out;

	out:
		curptr = NULL;
		prevptr = NULL;
		buf = NULL;

	return err;
	}

	/* This function contains complete merge logic for mergesort
	 * Characters from both the files get matched one by one and then processed which ever is shorter
	 * Length of the shorter string get retrieved and string get pushed into temp buffer and output buffer
	 * This process is repeated iteratively till we one file gets EOF
	 * Once the outputbuffer get filled string has to be written to temp file
	 * tempbuffer is maintained to record previous entered string which will help while comparing with next string
	 * If incase outputbuffer gets filled then buffer content get written to tempfile
	 * All flags are handled in this function
	 */

	int mergesort(char *buf1, char *buf2)
	{

		char *input1ptr;
		char *input2ptr;
		char char1 = ' ';
		char char2 = ' ' ;
		int shortstringlen = 0;
		int previousstringlen = 0;
		int input1len = 0;
		int input2len = 0;
		int outputlen = 0;
		int comparestring;
		int outputbuflen = PAGE_SIZE;
		int input1read = 0;
		int input2read = 0;
		int inputfile1len = 0;
		int inputfile2len = 0;
		int err = 0;
		int input1byte = readfile(inputfile1, inputbuffer1, PAGE_SIZE, 0);
		int input2byte = readfile(inputfile2, inputbuffer2, PAGE_SIZE, 0);
		outputbuffer = kmalloc(PAGE_SIZE, GFP_KERNEL);
		tempbuffer = kmalloc(PAGE_SIZE, GFP_KERNEL);
		outputbufptr = outputbuffer;
		input1ptr = buf1;
		input2ptr = buf2;

		/* Loop for whole mergesort operation while atleast one file get emptied */

		while (input1len <= input1byte && input2len <= input2byte &&
				 *input1ptr != '\0' && *input2ptr != '\0') {
			shortstringlen = 0;
			input1read = input1len;
			input2read = input2len;

		/* Check for comparing two string and outputting shorter */

		while (*input1ptr != '\n' && *input2ptr != '\n' && *input1ptr != '\0' &&
				 *input2ptr != '\0' && input1read < PAGE_SIZE && input2read < PAGE_SIZE) {
			if (kargs->flags & 0x04) {
				char1 = touppercase(*input1ptr);
				char2 = touppercase(*input2ptr);
			} else {
				char1 = *input1ptr;
				char2 = *input2ptr;
			}
			if (char1 == char2) {
				input1ptr++;
				input2ptr++;
				shortstringlen++;
				input1read++;
				input2read++;
			} else
				break;
		}

		/* Operation if input1 char is shorter than input2 char */

		if (char1 > char2 || (*input2ptr == '\n' && *input1ptr != '\n')) {
			while (*input2ptr != '\n' && *input2ptr != '\0' && input2read < PAGE_SIZE) {
				input2ptr++;
				shortstringlen++;
				input2read++;
			}
			} else if (char1 < char2 || (*input1ptr == '\n' && *input2ptr != '\n')) {
			while (*input1ptr != '\n' &&  *input1ptr != '\0' && input1read < PAGE_SIZE) {
				input1ptr++;
				shortstringlen++;
				input1read++;
			}
		}

		/* If string length found goes beyond page size, then reading new file */

		if (input1read == PAGE_SIZE) {
			shortstringlen = 0;
			input1byte = readfile(inputfile1, inputbuffer1, PAGE_SIZE, inputfile1len);
			input1ptr = inputbuffer1;
			buf1 = inputbuffer1;
			input1len = 0;
			while (*input1ptr != '\n' &&  *input1ptr != '\0') {
				input1ptr++;
				shortstringlen++;
			}
		}

		if (input2read == PAGE_SIZE) {
			shortstringlen = 0;
			input2byte = readfile(inputfile2, inputbuffer2, PAGE_SIZE, inputfile2len);
			input2ptr = inputbuffer2;
			buf2 = inputbuffer2;
			input2len = 0;
			while (*input2ptr != '\n' &&  *input2ptr != '\0') {
				input2ptr++;
				shortstringlen++;
			}
		}
			shortstringlen += 1;

			/* Checking checking if atleast one string get entered to outputbuffer */

			if (previousstringptr != NULL) {

			/* Conditions based on if both string are same
			*  If input 1 string is shorter then other
			*  or else if input2 string is shorter than first
			*/

			if (*input1ptr == '\n' && *input2ptr == '\n') {
				if ((kargs->flags & 0x01) && (kargs->flags & 0x02)) {
					err = -EPERM;
					goto out;
				}
				previousstringptr =  tempbuffer;

				/* Comparing previous merged string with new one
				*  Based on compare results checking different files
				*  Checking files based on flag given by user
				*  if outputbuffer gets filled then merging outputbuffer to tempfile
				*/

				comparestring = comparetwostring(previousstringptr, buf1);
				if (comparestring == 0) {
					if (kargs->flags & 0x01) {
						previousstringptr = tempbuffer;
						input1ptr++;
						input2ptr++;
						input1len += shortstringlen;
						input2len += shortstringlen;
						inputfile1len += shortstringlen;
						inputfile2len += shortstringlen;
						buf1 = input1ptr;
						buf2 = input2ptr;
				}
					if (kargs->flags & 0x02) {
						if ((outputbuflen - shortstringlen) < 0) {
							outputfilelen = outputfilelen + write_file(outputfilelen, outputbuffer, PAGE_SIZE-outputbuflen);
							outputbuflen = PAGE_SIZE;
							outputbufptr = outputbuffer;
					}
							previousstringptr = tempbuffer;
							outputlen = copytooutbuffer(buf1);
							outputbuflen = outputbuflen - outputlen;
							input1len += outputlen;
							inputfile1len += outputlen;
							previousstringlen = copytotempbuffer(buf1);
							input1ptr++;
							no_of_records += 1;
							buf1 = input1ptr;

					if ((outputbuflen - shortstringlen) < 0) {
						outputfilelen = outputfilelen + write_file(outputfilelen, outputbuffer, PAGE_SIZE-outputbuflen);
						outputbuflen = PAGE_SIZE;
						outputbufptr = outputbuffer;
					}
						previousstringptr = tempbuffer;
						outputbuflen = outputbuflen - copytooutbuffer(buf2);
						previousstringlen = copytotempbuffer(buf2);
						input2len += shortstringlen;
						inputfile2len += shortstringlen;
						input2ptr++;
						no_of_records += 1;
						buf2 = input2ptr;
					}
			} else if (comparestring == -1) {
				if (kargs->flags & 0x10) {
					err = -EINVAL;
					goto out;
					} else {
						input1ptr++;
						input2ptr++;
						input1len += shortstringlen;
						inputfile1len += shortstringlen;
						inputfile2len += shortstringlen;
						input2len += shortstringlen;
						buf1 = input1ptr;
						buf2 = input2ptr;
					}
					} else {
					while (*input1ptr != '\n') {
						input1ptr++;
					}
						if (kargs->flags & 0x01) {
							previousstringptr = tempbuffer;
							if ((outputbuflen - shortstringlen) < 0) {
								outputfilelen = outputfilelen + write_file(outputfilelen, outputbuffer, PAGE_SIZE-outputbuflen);
								outputbuflen = PAGE_SIZE;
								outputbufptr = outputbuffer;
							}

								previousstringptr = tempbuffer;
								outputlen = copytooutbuffer(buf1);
								outputbuflen = outputbuflen - outputlen;
								input1len += outputlen;
								input2len += outputlen;
								no_of_records += 1;
								inputfile1len += outputlen;
								inputfile2len += outputlen;
								previousstringlen = copytotempbuffer(buf1);
								input1ptr++;
								input2ptr++;
								buf1 = input1ptr;
								buf2 = input2ptr;
						}
						if (kargs->flags & 0x02) {
							if ((outputbuflen - shortstringlen) < 0) {
								outputfilelen = outputfilelen + write_file(outputfilelen, outputbuffer, PAGE_SIZE-outputbuflen);
								outputbuflen = PAGE_SIZE;
								outputbufptr = outputbuffer;
							}

								previousstringptr = tempbuffer;
								outputlen = copytooutbuffer(buf1);
								outputbuflen = outputbuflen - outputlen;
								input1len += outputlen;
								inputfile1len += outputlen;
								previousstringlen = copytotempbuffer(buf1);
								input1ptr++;
								no_of_records += 1;
								buf1 = input1ptr;

							if ((outputbuflen - shortstringlen) < 0) {
								outputfilelen = outputfilelen + write_file(outputfilelen, outputbuffer, PAGE_SIZE-outputbuflen);
								outputbuflen = PAGE_SIZE;
								outputbufptr = outputbuffer;
							}

								outputbuflen = outputbuflen - copytooutbuffer(buf2);
								no_of_records += 1;
								input2len += shortstringlen;
								inputfile2len += shortstringlen;
								input2ptr++;
								buf2 = input2ptr;
						}
					}
				} else if (*input1ptr == '\n') {

				/* condition if string of input1 file is shorter */

					previousstringptr = tempbuffer;
					comparestring = comparetwostring(previousstringptr, buf1);
					if (comparestring == 0) {
						if ((kargs->flags & 0x01) && (kargs->flags & 0x02)) {
							err = -EPERM;
							goto out;
						}
						if (kargs->flags & 0x01) {
							previousstringptr = tempbuffer;
							input1ptr++;
							input1len += shortstringlen;
							inputfile1len += shortstringlen;
							input2ptr = buf2;
							buf1 = input1ptr;
						}
						if (kargs->flags & 0x02) {
							if ((outputbuflen - shortstringlen) < 0) {
								outputfilelen = outputfilelen + write_file(outputfilelen, outputbuffer, PAGE_SIZE - outputbuflen);
								outputbuflen = PAGE_SIZE;
								outputbufptr = outputbuffer;
							}
								previousstringptr = tempbuffer;
								outputbuflen = outputbuflen - copytooutbuffer(buf1);
								input1len += shortstringlen;
								no_of_records += 1;
								inputfile1len += shortstringlen;
								input1ptr++;
								buf1 = input1ptr;
								input2ptr = buf2;

						}
						} else if (comparestring == -1) {
							if (kargs->flags & 0x10) {
								err = -EINVAL;
								goto out;
								} else {
									input1len += shortstringlen;
									inputfile1len += shortstringlen;
									input1ptr++;
									buf1 = input1ptr;
									input2ptr = buf2;
						}
					} else {

						/* if input2 file string is found to be shorter */

						while (*input1ptr != '\n') {
							input1ptr++;
						}
						if ((outputbuflen - shortstringlen) < 0) {
							outputfilelen = outputfilelen + write_file(outputfilelen, outputbuffer, PAGE_SIZE - outputbuflen);
							outputbuflen = PAGE_SIZE;
							outputbufptr = outputbuffer;
						}
							previousstringptr = tempbuffer;
							outputbuflen = outputbuflen - copytooutbuffer(buf1);
							previousstringlen = copytotempbuffer(buf1);
							previousstringptr = tempbuffer;
							input1len += shortstringlen;
							inputfile1len += shortstringlen;
							input1ptr++;
							no_of_records += 1;
							buf1 = input1ptr;
							input2ptr = buf2;

					}
				} else {
					previousstringptr = tempbuffer;
					comparestring = comparetwostring(previousstringptr, buf2);

				if (comparestring == 0) {
					if ((kargs->flags & 0x01) && (kargs->flags & 0x02)) {
						err = -EPERM;
						goto out;
					}
					if (kargs->flags & 0x01) {
						previousstringptr = tempbuffer;
						input2ptr++;
						input2len += shortstringlen;
						inputfile2len += shortstringlen;
						input1ptr = buf1;
						buf2 = input2ptr;
					}
					if (kargs->flags & 0x02) {
						if ((outputbuflen - shortstringlen) < 0) {
							outputfilelen = outputfilelen + write_file(outputfilelen, outputbuffer, PAGE_SIZE-outputbuflen);
							outputbuflen = PAGE_SIZE;
							outputbufptr = outputbuffer;
						}
							previousstringptr = tempbuffer;
							outputbuflen = outputbuflen - copytooutbuffer(buf2);
							previousstringlen = copytotempbuffer(buf2);
							input2ptr++;
							no_of_records += 1;
							buf2 = input2ptr;
							input2len += shortstringlen;
							inputfile2len += shortstringlen;
							input1ptr = buf1;
						}
					} else if (comparestring == -1) {
						if (kargs->flags & 0x10) {
							err = -EINVAL;
							goto out;
						} else {
							input2len += shortstringlen;
							inputfile2len += shortstringlen;
							input2ptr++;
							buf2 = input2ptr;
							input1ptr = buf1;
						}
					} else {
						while (*input2ptr != '\n') {
						input2ptr++;
						}
						if ((outputbuflen - shortstringlen) < 0) {
							outputfilelen = outputfilelen + write_file(outputfilelen, outputbuffer, PAGE_SIZE-outputbuflen);
							outputbuflen = PAGE_SIZE;
							outputbufptr = outputbuffer;
							}

							outputbuflen = outputbuflen - copytooutbuffer(buf2);
							previousstringptr = tempbuffer;
							previousstringlen = copytotempbuffer(buf2);
							previousstringptr = tempbuffer;
							input2ptr++;
							no_of_records += 1;
							buf2 = input2ptr;
							input1ptr = buf1;
							input2len = input2len + shortstringlen;
							inputfile2len += shortstringlen;
						}
					}
			} else {

				/* This condition check is for first time Input
				* Both file inputs will be checked and matched
				* File having shorter first string get merged to outputbuffer
				*/

				if (*input1ptr == '\n' && *input2ptr == '\n') {
					if ((kargs->flags & 0x01) && (kargs->flags & 0x02)) {
						err = -EPERM;
						goto out;

					}
				if (kargs->flags & 0x01) {
					if (outputbuflen - shortstringlen >= 0) {
						previousstringptr = tempbuffer;
						outputbuflen = outputbuflen - copytooutbuffer(buf1);
						previousstringlen = copytotempbuffer(buf1);
						previousstringptr = tempbuffer;
						input1len += shortstringlen;
						input2len += shortstringlen;
						inputfile1len += shortstringlen;
						inputfile2len += shortstringlen;
						input1ptr++;
						buf1 = input1ptr;
						input2ptr++;
						no_of_records += 1;
						buf2 = input2ptr;
					}
				}
			if (kargs->flags & 0x02) {
				if ((outputbuflen - shortstringlen) >= 0) {
					previousstringptr = tempbuffer;
					outputbuflen = outputbuflen - copytooutbuffer(buf1);
					previousstringlen = copytotempbuffer(buf1);
					input1ptr++;
					buf1 = input1ptr;
					input1len += shortstringlen;
					no_of_records += 1;
					inputfile1len += shortstringlen;
				}
				if ((outputbuflen - shortstringlen) < 0) {
					outputfilelen = outputfilelen + write_file(outputfilelen, outputbuffer, PAGE_SIZE-outputbuflen);
					outputbuflen = PAGE_SIZE;
					outputbufptr = outputbuffer;
				}

					outputbuflen = outputbuflen - copytooutbuffer(buf2);
					previousstringptr = tempbuffer;
					previousstringlen = copytotempbuffer(buf2);
					input2ptr++;
					buf2 = input2ptr;
					no_of_records += 1;
					input2len += shortstringlen;
					inputfile2len += shortstringlen;
			}
		} else if (*input1ptr == '\n') {
			if (outputbuflen - shortstringlen >= 0) {
				previousstringptr = tempbuffer;
				outputbuflen = outputbuflen - copytooutbuffer(buf1);
				previousstringlen = copytotempbuffer(buf1);
				input1ptr++;
				previousstringptr = tempbuffer;
				buf1 = input1ptr;
				input2ptr = buf2;
				input1len = input1len + shortstringlen;
				inputfile1len += shortstringlen;
				no_of_records += 1;
			} else
				outputfilelen = outputfilelen + write_file(outputfilelen, outputbuffer, PAGE_SIZE-outputbuflen);
		} else {
			if (outputbuflen - shortstringlen >= 0) {
				previousstringptr = tempbuffer;
				outputbuflen = outputbuflen - copytooutbuffer(buf2);
				previousstringlen = copytotempbuffer(buf2);
				input2ptr++;
				previousstringptr = tempbuffer;
				buf2 = input2ptr;
				input1ptr = buf1;
				no_of_records += 1;
				input2len += shortstringlen;
				inputfile2len += shortstringlen;
			} else
				outputfilelen = outputfilelen + write_file(outputfilelen, outputbuffer, PAGE_SIZE-outputbuflen);
			}
		}
	}

		/* mergesinglefile will get called when one file get emptied and other file still have to mergesort */

		if (input1len < input1byte) {
			err = mergesinglefile(previousstringptr, input1ptr, input1len, input1byte, outputbuflen, inputfile1len, true);
		} else if (input2len < input2byte) {
			err = mergesinglefile(previousstringptr, input2ptr, input2len, input2byte, outputbuflen, inputfile2len, false);
		} else {
			write_file(outputfilelen, outputbuffer, PAGE_SIZE-outputbuflen);
		}
			goto out;

	out:
		input1ptr = NULL;
		input2ptr = NULL;
		buf1 = NULL;
		buf2 = NULL;

		return err;
	}

	/* deletetempfile will unlink the file fron userspace */

	int deletetempfile(struct file *temp)
	{
		struct inode  *tempinode = temp->f_path.dentry->d_parent->d_inode;
		struct dentry *parentdentry = NULL;
		int err = 0;

		dget(temp->f_path.dentry);
		parentdentry = dget_parent(temp->f_path.dentry);
		mutex_lock_nested(&(parentdentry->d_inode->i_mutex), I_MUTEX_PARENT);

		err = vfs_unlink(tempinode, temp->f_path.dentry, NULL);
		if (err) {
			printk("vfs_unlink! Error code is %d\n", err);
		}
			mutex_unlock(&(parentdentry->d_inode->i_mutex));
			dput(parentdentry);
			dput(temp->f_path.dentry);

	return err;
	}

	/* This is the entering point of mergesort system call
	 * This function contains all the check required for
	 * input and output files
	 * mergesort logic get called inside this function
	 * After merging temp file gte renamed to outputfile
	 * if in case of bad flag outputfile get deleted
	 */
	asmlinkage long xmergesort(void *arg)
	{
		struct xmerge_args *kuser = NULL;
		int err = 0;
		struct dentry *oldtempdentry = NULL;
		struct dentry *newtempdentry = NULL;
		struct dentry *lockcheck = NULL;
		struct kstat umode;
		int inpflag;
		int outflag;
		mm_segment_t oldfs;
		int rc;

		if (arg == NULL)
			return -EINVAL;

		kuser = (struct xmerge_args *)arg;

		if (access_ok(VERIFY_READ, kuser, sizeof(struct xmerge_args)) == 0) {
			printk("bad user space address.\n");
			err = -EFAULT;
			goto out;
		}

		kargs = (struct xmerge_args *) kmalloc(sizeof(struct xmerge_args), GFP_KERNEL);

		if (unlikely(!kargs)) {
			err = -ENOMEM;
			goto out;
		}


		if	(copy_from_user(&(kargs->flags), &(kuser->flags), sizeof(unsigned int))) {
			err = -EFAULT;
			printk("error in copy from  user %d\n", err);
			goto out;
		}

		kargs->inputfile1 = kmalloc(strlen_user(kuser->inputfile1), GFP_KERNEL);

		if	(!kargs->inputfile1) {
			printk(" inputfile1 fails \n");
			err = -ENOMEM;
		}

		kargs->inputfile2 = kmalloc(strlen_user(kuser->inputfile2), GFP_KERNEL);

		if	(!kargs->inputfile2) {
			printk(" inputfile 2 fails \n");
			err = -ENOMEM;
		}

		kargs->outputfile = kmalloc(strlen_user(kuser->outputfile), GFP_KERNEL);

		if	(!kargs->outputfile) {
			printk(" output file fails \n");
			err = -ENOMEM;
		}

		err = strncpy_from_user(kargs->inputfile1, kuser->inputfile1, strlen_user(kuser->inputfile1));

		if (err < 0) {
		err = -EFAULT;
			printk(" access to user space fails \n");
			goto out;
		}

		err = strncpy_from_user(kargs->inputfile2, kuser->inputfile2, strlen_user(kuser->inputfile2));

		if (err < 0) {
			err = -EFAULT;
			goto out;
		}

		err = strncpy_from_user(kargs->outputfile, kuser->outputfile, strlen_user(kuser->outputfile));

		if (err < 0) {
			err = -EFAULT;
			goto out;
		}

		if (kargs->inputfile1 == NULL || kargs->inputfile2 == NULL || kargs->outputfile == NULL) {
			err = -EINVAL;
			goto out;
		}

		inputfile1 = filp_open(kargs->inputfile1, O_RDONLY, 0);

		if (!inputfile1 || IS_ERR(inputfile1)) {
			err = PTR_ERR(inputfile1);
			goto out;
		}

		if (!(inputfile1->f_mode&FMODE_READ)) {
			printk("Input file 1 not accessible to be read.\n");
			err = -EIO;
			goto out;
		}

		inputfile2 = filp_open(kargs->inputfile2, O_RDONLY, 0);

		if (!inputfile2 || IS_ERR(inputfile2)) {
			err = PTR_ERR(inputfile2);
			goto out;
		}

		if (!(inputfile2->f_mode & FMODE_READ)) {
			printk("Input file 2 not accessible to be read.\n");
			err = -EIO;
			goto out;
		}

		inputbuffer1 = kmalloc(PAGE_SIZE, GFP_KERNEL);

		inputbuffer2 = kmalloc(PAGE_SIZE, GFP_KERNEL);

		if ((!inputbuffer1) || (!inputbuffer2)) {
			err = -ENOMEM;
			goto out;
		}

		oldfs = get_fs();
		set_fs(get_ds());
		outflag = vfs_stat(kargs->outputfile, &umode);
		inpflag = vfs_stat(kargs->inputfile1, &umode);
		set_fs(oldfs);

		outputfile = filp_open(kargs->outputfile, O_WRONLY|O_CREAT, umode.mode);

		if (!outputfile || IS_ERR(outputfile)) {
			printk(" outfile error \n");
			err = PTR_ERR(outputfile);
			goto out;
		}

		if (!(outputfile->f_mode & FMODE_WRITE)) {
			printk("Output File not accessible to be written.\n");
			err = -EIO;
			goto out;
		}

		if (inputfile1->f_path.dentry->d_inode->i_ino == outputfile->f_path.dentry->d_inode->i_ino) {
			printk("Input file 1 and output files are the same\n");
			err = -EINVAL;
			goto out;
		}

		if (inputfile2->f_path.dentry->d_inode->i_ino == outputfile->f_path.dentry->d_inode->i_ino) {
			printk("Input file 2 and output files are the same\n");
			err = -EINVAL;
			goto out;
		}


		if (inputfile1->f_path.dentry->d_inode->i_ino == inputfile2->f_path.dentry->d_inode->i_ino) {
			printk("Input file1 and input fle 2 are the same\n");
			err = -EINVAL;
			goto out;
		}

		tempfile = filp_open((strcat((char *)kargs->outputfile, ".tmp")), O_WRONLY|O_CREAT, inputfile1->f_path.dentry->d_inode->i_mode);
		if (!tempfile || IS_ERR(tempfile)) {
			err = -EINVAL;
			goto out;
		}

		err = mergesort(inputbuffer1, inputbuffer2);
		printk(" err value after merge sort is %d \n", err);
		if (err == 0) {

			oldtempdentry = dget_parent(tempfile->f_path.dentry);

			newtempdentry = dget_parent(outputfile->f_path.dentry);

			lockcheck = lock_rename(oldtempdentry, newtempdentry);

			err = vfs_rename(oldtempdentry->d_inode, tempfile->f_path.dentry, newtempdentry->d_inode, outputfile->f_path.dentry, NULL, 0);
			if (err) {
				printk("vfs_rename failed with ErrNo (%d)\n", err);
			}

			unlock_rename(oldtempdentry, newtempdentry);
			if (kargs->flags & 0x20) {
				printk("returning no of records \n");
				if (copy_to_user(kuser->data, &no_of_records, sizeof(unsigned int))) {
					err = -EFAULT;
					printk("error in copy to  user %d \n", err);
				}

			}

		} else {
			rc = deletetempfile(tempfile);
			if (outflag != 0) {
				printk("deleting out file \n");
			rc = deletetempfile(outputfile);
			}
		}
		printk("returned from mergesort , no of records are %d \n", no_of_records);
		outputbufptr = NULL;
		previousstringptr = NULL;
		outputfilelen = 0;
		no_of_records = 0;
		goto out;

out:

	if (kargs != NULL)
		kfree(kargs);

	if (inputbuffer1 != NULL)
		kfree(inputbuffer1);

	if (inputbuffer2 != NULL)
		kfree(inputbuffer2);

	if (outputbuffer != NULL)
		kfree(outputbuffer);

	if (tempbuffer != NULL)
		kfree(tempbuffer);

	if (tempfile != NULL)
		filp_close(tempfile, NULL);

	if (inputfile1 != NULL)
		filp_close(inputfile1, NULL);

	if (inputfile2 != NULL)
		filp_close(inputfile2, NULL);

	if (outputfile != NULL)
		filp_close(outputfile, NULL);

	if (tempfile != NULL)
		filp_close(tempfile, NULL);

	return err;
}


static int __init init_sys_xmergesort(void)
{
	printk("installed new sys_xmergesort module\n");
	if (sysptr == NULL)
		sysptr = xmergesort;
	return 0;
}
static void __exit exit_sys_xmergesort(void)
{
	if (sysptr != NULL)
		sysptr = NULL;
	printk("removed sys_xmergesort module\n");
}
module_init(init_sys_xmergesort);
module_exit(exit_sys_xmergesort);
MODULE_LICENSE("GPL");
