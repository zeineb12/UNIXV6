/**
 * @file shell.c
 * @brief describes a command interpreter
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unixv6fs.h"
#include "mount.h"
#include "direntv6.h"
#include "sha.h"
#include "inode.h"
#include "error.h"

#define NBR_CMDS 13
#define MAX_SIZE 7*256*SECTOR_SIZE

/*
 * Definition of the shell errors
 */
enum error_shell {
    NO_ERR = 1,
    INVALID_COMMAND, //2
    WRONG_NBR_ARGS, //3
    NOT_MOUNTED, //4
    CAT_DIR, //5
};

/* Array which contains the messages of each shell error */
const char * const ERR_SHELL_MSG[] = {
    "", // no error
    "invalid command",
    "wrong number of arguments",
    "mount the FS before the operation",
    "cat on a directory is not defined",
    "ERROR FS: IO error: No such file or directory"
};

struct unix_filesystem u;

/*
 * Definition of the shell_fct which is a pointer
 * on a function which takes a char** as argument.
 */
typedef int (*shell_fct)(char**);

/*
 * Definition of the shell_map structure.
 */
struct shell_map {
    const char* name; // nom de la commande
    shell_fct fct; // fonction réalisant la commande
    const char* help; // description de la commande
    size_t argc; // nombre d'arguments de la commande
    const char* args; // description des arguments de la commande
};

int do_exit(char** s);
int do_quit(char** s);
int do_help(char** s);
int do_mount(char** s);
int do_lsall(char** s);
int do_psb(char** s);
int do_cat(char** s);
int do_sha(char** s);
int do_inode(char** s);
int do_istat(char** s);
int do_mkfs(char** s);
int do_mkdir(char** s);
int do_add(char** s);

struct shell_map help_cmd = {"help", do_help, "display this help", 0, ""};
struct shell_map exit_cmd = {"exit", do_exit, "exit shell", 0, ""};
struct shell_map quit_cmd = {"quit", do_quit, "exit shell", 0, ""};
struct shell_map mkfs_cmd = {"mkfs", do_mkfs, "create a new filesystem", 3, " <diskname> <#inodes> <#blocks>"};
struct shell_map mount_cmd = {"mount", do_mount, "mount the provided filesystem", 1, " <diskname>"};
struct shell_map mkdir_cmd = {"mkdir", do_mkdir, "create a new directory", 1, " <dirname>"};
struct shell_map lsall_cmd = {"lsall", do_lsall, "list all directories and files contained in the currently mounted filesystem", 0, ""};
struct shell_map add_cmd = {"add", do_add, "add a new file", 2, " <src-fullpath> <dst>"};
struct shell_map cat_cmd = {"cat", do_cat, "display the content of a file", 1, " <pathname>"};
struct shell_map istat_cmd = {"istat", do_istat, "display information about the provided inode", 1, " <inode_nr>"};
struct shell_map ino_cmd = {"inode", do_inode, "display the inode number of a file", 1, " <pathname>"};
struct shell_map sha_cmd = {"sha", do_sha, "display the SHA of a file", 1, " <pathname>"};
struct shell_map psb_cmd = {"psb", do_psb, "Print SuperBlock of the currently mounted filesystem", 0, ""};

struct shell_map shell_cmds[NBR_CMDS];

/**
 * @brief initializes the shell commands
 */
void construct_shell_cmds()
{
    shell_cmds[0] = help_cmd;
    shell_cmds[1] = exit_cmd;
    shell_cmds[2] = quit_cmd;
    shell_cmds[3] = mkfs_cmd;
    shell_cmds[4] = mount_cmd;
    shell_cmds[5] = mkdir_cmd;
    shell_cmds[6] = lsall_cmd;
    shell_cmds[7] = add_cmd;
    shell_cmds[8] = cat_cmd;
    shell_cmds[9] = istat_cmd;
    shell_cmds[10] = ino_cmd;
    shell_cmds[11] = sha_cmd;
    shell_cmds[12] = psb_cmd;
}

/**
 * @brief checks the number of arguments of a command line
 * @param s contains the input (name of the command + args)
 * @return the number of arguments
 */
int args_test(char** s)
{
    int nbr_args =0;
    while(s[nbr_args]!=NULL) {
        nbr_args++;
    }
    nbr_args--;
    return nbr_args;
}

/**
 * @brief terminates the program
 * @param s contains the input (name of the command + args)
 * @return 0 on success; >0 on error
 */
int do_exit(char** s)
{
    int err =0;
    if ((err= args_test(s))!=0) {
        return WRONG_NBR_ARGS;
    }
    if(u.f != NULL) umountv6(&u);
    return 0;
}

/**
 * @brief terminates the program
 * @param s contains the input (name of the command + args)
 * @return 0 on success; >0 on error
 */
int do_quit(char** s)
{
    int err =0;
    if ((err= args_test(s))!=0) {
        return WRONG_NBR_ARGS;
    }
    if(u.f != NULL) umountv6(&u);
    return 0;
}

/**
 * @brief prints the list of all the functions of the shell + a brief description of them
 * @param s contains the input (name of the command + args)
 * @return 0 on success; >0 on error
 */
int do_help(char** s)
{
    int err =0;
    if ((err= args_test(s))!=0) {
        return WRONG_NBR_ARGS;
    }
    for(int i=0; i<NBR_CMDS; ++i) {
        printf("\n- %s%s: %s.", shell_cmds[i].name, shell_cmds[i].args, shell_cmds[i].help);
    }
    printf("\n");
    return 0;
}

/**
 * @brief mounts the current unix filesystem
 * @param s contains the input (name of the command + args)
 * @return 0 on success; >0 for a shell error or <0 on an fs error
 */
int do_mount(char** s)
{
    int err =0;
    if ((err= args_test(s))!=1) {
        return WRONG_NBR_ARGS;
    }
    err=mountv6(s[1],&u);
    return err;
}

/**
 * @brief lists all the files ans directories of the mounted unix filesystem
 * @param s contains the input (name of the command + args)
 * @return 0 on success; >0 for a shell error or <0 on an fs error
 */
int do_lsall(char** s)
{
    int err =0;
    if ((err= args_test(s))!=0) {
        return WRONG_NBR_ARGS;
    }
    if(u.f==NULL) {
        return NOT_MOUNTED;
    }
    err=direntv6_print_tree(&u, ROOT_INUMBER, "");
    return err;
}

/**
 * @brief prints to stdout the content of the superblock
 * @param s contains the input (name of the command + args)
 * @return 0 on success; >0 on error
 */
int do_psb(char** s)
{
    int err =0;
    if ((err= args_test(s))!=0) {
        return WRONG_NBR_ARGS;
    }
    if(u.f==NULL) {
        return NOT_MOUNTED;
    }
    mountv6_print_superblock(&u);
    return err;
}

/**
 * @brief prints the content of a file
 * @param s contains the input (name of the command + args)
 * @return 0 on success; >0 for a shell error or <0 on an fs error
 */
int do_cat(char** s)
{
    int err =0;
    if ((err= args_test(s))!=1) {
        return WRONG_NBR_ARGS;
    }
    if(u.f==NULL) {
        return NOT_MOUNTED;
    }
    int inode_nbr = direntv6_dirlookup(&u, ROOT_INUMBER, s[1]);
    struct filev6 fs;
    memset(&fs, 255, sizeof(fs));
    uint8_t secteur[SECTOR_SIZE];
    secteur[SECTOR_SIZE] = '\0';
    if((err = filev6_open(&u,inode_nbr,&fs))!=0) {
        return err;
    }
    if(fs.i_node.i_mode & IFDIR) {
        return CAT_DIR;
    } else {
		while((err = filev6_readblock(&fs,secteur))>0){
			printf("%s",secteur);
		}
		if(err<0) {
				return err;
		}
		return 0;
    }
}

/**
 * @brief prints the sha256 of the content of a file
 * @param s contains the input (name of the command + args)
 * @return 0 on success; >0 for a shell error or <0 on an fs error
 */
int do_sha(char** s)
{
    int err =0;
    if ((err= args_test(s))!=1) {
        return WRONG_NBR_ARGS;
    }
    if(u.f==NULL) {
        return NOT_MOUNTED;
    }
    int inode_nbr = direntv6_dirlookup(&u, ROOT_INUMBER, s[1]);
    struct filev6 fs;
    memset(&fs, 255, sizeof(fs));
    if((err = filev6_open(&u,inode_nbr,&fs))!=0) {
        return err;
    }
    print_sha_inode(&u, fs.i_node, inode_nbr);
    return 0;
}

/**
 * @brief prints the content of an inode
 * @param s contains the input (name of the command + args)
 * @return 0 on success; >0 for a shell error or <0 on an fs error
 */
int do_istat(char** s)
{
    int err =0;
    if ((err= args_test(s))!=1) {
        return WRONG_NBR_ARGS;
    }
    if(u.f==NULL) {
        return NOT_MOUNTED;
    }
    uint16_t inode_nbr = (uint16_t)atol(s[1]);
    struct inode ino;
    if((err = inode_read(&u, inode_nbr, &ino))!=0) {
        return err;
    }
    inode_print(&ino);
    return 0;
}

/**
 * @brief prints the inode number of a given file
 * @param s contains the input (name of the command + args)
 * @return 0 on success; >0 on error
 */
int do_inode(char** s)
{
    int err =0;
    if ((err= args_test(s))!=1) {
        return WRONG_NBR_ARGS;
    }
    if(u.f==NULL) {
        return NOT_MOUNTED;
    }
    int inode_nbr = direntv6_dirlookup(&u, ROOT_INUMBER, s[1]);
    printf("\ninode: %d \n",inode_nbr);
    return 0;
}

/**
 * @brief creates a new disk
 * @param s contains the input (name of the command + args)
 * @return 0 on success; >0 for a shell error or <0 on an fs error
 */
int do_mkfs(char** s)
{
	int err =0;
    if ((err= args_test(s))!=3) {
        return WRONG_NBR_ARGS;
    }
    uint16_t num_inodes = (uint16_t)atol(s[2]);
    uint16_t num_blocks = (uint16_t)atol(s[3]);
    err = mountv6_mkfs(s[1], num_blocks, num_inodes);
    if(err<0) return err;
	
    return 0;
}

/**
 * @brief creates a directory on the mounted unix filesystem
 * @param s contains the input (name of the command + args)
 * @return 0 on success; >0 for a shell error or <0 on an fs error
 */
int do_mkdir(char** s)
{
	int err =0;
    if ((err= args_test(s))!=1) {
        return WRONG_NBR_ARGS;
    }
    if(u.f==NULL) {
        return NOT_MOUNTED;
    }
    char* entry =  s[1];
    uint16_t dir_mode = IFDIR | IALLOC;
    err = direntv6_create(&u, entry, dir_mode);
    if(err<0) return err;
    return 0;
}

/**
 * @brief adds a local file to the mounted unix filesystem
 * @param s contains the input (name of the command + args)
 * @return 0 on success; >0 for a shell error or <0 on an fs error
 */
int do_add(char** s)
{
    int err =0;
    if ((err= args_test(s))!=2) {
        return WRONG_NBR_ARGS;
    }
    if(u.f==NULL) {
        return NOT_MOUNTED;
    }
    uint16_t file_mode = IALLOC;
    char* child= s[1];
    char* parent =  s[2];
    
    
    int parent_inode = 0;
    if((parent_inode=direntv6_dirlookup(&u,ROOT_INUMBER,parent))<0) return parent_inode;
    
    FILE* entree = fopen(child,"rb");
    if(entree==NULL) return ERR_IO;
    
    /* Get the right name */
    char* last_directory = strrchr(child,'/');
    char* child_name =NULL;
    if(last_directory == NULL){
		child_name=child;
	}else{
		child_name = last_directory+1;
	}
    
    strcat(parent,"/");
    char* new = strcat(parent,child_name);
    
    int child_inode = 0;
    if((child_inode = direntv6_create(&u, new, file_mode))<0) return child_inode;
    
    char data[MAX_SIZE];
    size_t size_file = 0;
    
    if(fseek(entree,0,SEEK_SET)!=0) {
        return ERR_IO;
    }
    int index =0;
    while(!feof(entree) && !ferror(stdin)){
		size_file += fread(&data[index],sizeof(char),1,entree);
		++index;
    }
    
    /* initialisation */
    struct filev6 fv6_child;
    memset(&fv6_child,0,sizeof(fv6_child));
    if((err=filev6_open(&u, child_inode, &fv6_child))<0) return err;
     
    if((err=filev6_writebytes(&u, &fv6_child, data, size_file))<0) return err;
    
    return 0;
   
}


/**
 * @brief splits the input into words and puts them in s
 * @param s will contain the tokenized input (name of the command + args)
 * @param input the command line + the arguments
 */
void tokenize_input(char* input, char** s)
{
    char *tmp = NULL;
    tmp = strtok(input," ");
    int index = 0;
    if(!(strcmp(tmp,"\n"))) {
        index=0;
    } else {
        while (tmp != NULL) {
            if(strcmp(&tmp[strlen(tmp)-1],"\n")==0) {
                tmp[strlen(tmp)-1]='\0';
            }
            s[index]=tmp;
            ++index;
            tmp = strtok(NULL," ");
        }
    }
}

int main(int argc, char *argv[])
{

    construct_shell_cmds();
    int error = 0;


    while(!feof(stdin) && !ferror(stdin) ) {

        char** s = calloc(255,sizeof(char*));
        int len;
        if(s!=NULL){
			char input[255];
			input[254]='\0';
			fgets(input,255,stdin);
			/* l'utilsateur a entré une commande vide */
			if(!(strcmp(input,"\n"))) return INVALID_COMMAND;
			len = strlen(input)-1;
			if((len>=0)&&(input[len]== '\n')) input[len] = '\0';
			tokenize_input(input,s);
			int index = -1;
			for(int i=0; i<NBR_CMDS; i++) {
				if(!(strcmp(s[0],shell_cmds[i].name))) {
					index = i;
				}
			}
			/* L'utilisateur a entré une commande qui n'existe pas */
			if(index<0) {
				error = INVALID_COMMAND;
			}
			/* Sinon on réalise la fonction spécifique a cette commande */
			if(index>=0) {
				if((index==1)&&((args_test(s))==0)) {
					return do_exit(s);
				}
				if((index==2)&&((args_test(s))==0)) {
					return do_quit(s);
				}
				shell_fct ptr = shell_cmds[index].fct;
				error = (*ptr)(s);
			}
			if (error>0) {
				printf("\nERROR SHELL:");
				puts(ERR_SHELL_MSG[error-1]);
			} else {
				if(error<0) {
					printf("\nERROR FS:");
					puts(ERR_MESSAGES[error - ERR_FIRST]);
				}
			}
		}
        free(s);
        s = NULL;
    }
    return error;
}
